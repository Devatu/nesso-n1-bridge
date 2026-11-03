#include "modbus_client.h"
#include "app_config.h"
#include "log.h"
#include "wifi_manager.h"
#include <stdio.h>
#include <string.h>

#if NESSO_ENABLE_HW
#include <WiFi.h>
#include <WiFiClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#endif

static HeatpumpConfig g_target;
static bool g_has_target = false;
static uint16_t g_txn_id = 1;

#if NESSO_ENABLE_HW
/* Qvantum Modbus TCP lijkt 1 client tegelijk te accepteren; hergebruik + nette close. */
static WiFiClient g_mb;
static uint32_t g_mb_last_ms = 0;
static const uint32_t kMbIdleCloseMs = 5000;
/* Connect blokkeert UI; kort houden. RX iets ruimer — Qvantum kan traag antwoorden. */
static const int32_t kMbConnectTimeoutMs = 2500;
static const uint32_t kMbIoTimeoutMs = 3000;
#endif

const char *ventilation_mode_label(uint16_t mode) {
  switch (mode) {
  case VENT_OFF:
    return "Uit";
  case VENT_NORMAL:
    return "Normaal";
  case VENT_EXTRA:
    return "Extra";
  case VENT_REDUCED:
    return "Gereduceerd";
  default:
    return "?";
  }
}

const char *modbus_result_text(ModbusResult r) {
  switch (r) {
  case MB_OK:
    return "OK";
  case MB_ERR_NO_TARGET:
    return "Geen warmtepomp-IP";
  case MB_ERR_NO_WIFI:
    return "Geen Wi-Fi";
  case MB_ERR_CONNECT:
    return "Poort 502 dicht";
  case MB_ERR_TIMEOUT:
    return "Geen antwoord";
  case MB_ERR_PROTOCOL:
    return "Protocolfout";
  case MB_ERR_DENIED:
    return "Niet toegestaan";
  case MB_ERR_EXCEPTION:
    return "Modbus-fout";
  case MB_ERR_BUSY:
    return "Bezig";
  default:
    return "Fout";
  }
}

bool modbus_ventilation_allowed(uint16_t value) {
  return value <= (uint16_t)VENT_REDUCED;
}

bool modbus_set_target(const HeatpumpConfig *cfg) {
  if (!heatpump_config_is_ready(cfg)) {
    g_has_target = false;
    LOG_W("modbus", "target rejected: heatpump config incomplete");
    return false;
  }
  const bool changed =
      g_has_target && (strcmp(g_target.ipv4, cfg->ipv4) != 0 || g_target.tcp_port != cfg->tcp_port ||
                       g_target.unit_id != cfg->unit_id);
  g_target = *cfg;
  g_has_target = true;
  LOG_I("modbus", "target set %s:%u unit=%u", g_target.ipv4, g_target.tcp_port, g_target.unit_id);
#if NESSO_ENABLE_HW
  /* Niet midden in een job de socket killen — dat maakt probes corrupt. */
  if (changed && !modbus_async_busy()) {
    LOG_I("modbus", "target changed → close idle socket");
    g_mb.stop();
  }
#endif
  return true;
}

bool modbus_has_target() {
  return g_has_target;
}

#if NESSO_ENABLE_HW
static void log_hex(const char *what, const uint8_t *buf, size_t n) {
  char line[96];
  size_t pos = 0;
  pos += (size_t)snprintf(line + pos, sizeof(line) - pos, "%s %uB:", what, (unsigned)n);
  for (size_t i = 0; i < n && pos + 4 < sizeof(line); ++i) {
    pos += (size_t)snprintf(line + pos, sizeof(line) - pos, " %02X", buf[i]);
  }
  LOG_I("modbus", "%s", line);
}

static void log_net_context(const char *phase) {
  char lip[20] = "?";
  wifi_sta_ip(lip, sizeof(lip));
  const bool sta = wifi_sta_connected();
  const int rssi = sta ? WiFi.RSSI() : 0;
  LOG_I("modbus", "%s wifi=%d ip=%s rssi=%d sock=%d idle_ms=%lu target=%s:%u unit=%u", phase,
        sta ? 1 : 0, lip, rssi, g_mb.connected() ? 1 : 0,
        (unsigned long)(g_mb.connected() ? (millis() - g_mb_last_ms) : 0), g_target.ipv4,
        g_target.tcp_port, g_target.unit_id);
}

static void mb_force_close() {
  LOG_I("modbus", "socket stop connected=%d", g_mb.connected() ? 1 : 0);
  g_mb.stop();
  delay(30);
}

static ModbusResult mb_ensure_connected() {
  if (!wifi_sta_connected()) {
    LOG_W("modbus", "ensure: no wifi (status)");
    return MB_ERR_NO_WIFI;
  }
  if (g_mb.connected()) {
    LOG_I("modbus", "ensure: reuse open socket");
    g_mb_last_ms = millis();
    return MB_OK;
  }

  IPAddress ip;
  if (!ip.fromString(g_target.ipv4)) {
    LOG_W("modbus", "ensure: bad ipv4 '%s'", g_target.ipv4);
    return MB_ERR_CONNECT;
  }

  mb_force_close();
  /* setTimeout = ms voor read/write; connect() krijgt eigen timeout_ms. */
  g_mb.setTimeout(kMbIoTimeoutMs);
  g_mb.setNoDelay(true);
  const uint32_t t0 = millis();
  LOG_I("modbus", "TCP connect → %u.%u.%u.%u:%u (max %dms)", ip[0], ip[1], ip[2], ip[3],
        g_target.tcp_port, (int)kMbConnectTimeoutMs);
  const bool ok = g_mb.connect(ip, g_target.tcp_port, kMbConnectTimeoutMs);
  const uint32_t dt = millis() - t0;
  if (ok) {
    g_mb_last_ms = millis();
    g_mb.setTimeout(kMbIoTimeoutMs);
    g_mb.setNoDelay(true);
    /* Laat de server de sessie afmaken voordat we de eerste PDU sturen. */
    delay(80);
    LOG_I("modbus", "TCP connected in %lums", (unsigned long)dt);
    return MB_OK;
  }
  LOG_W("modbus", "TCP connect FAIL after %lums (poort dicht/bezet)", (unsigned long)dt);
  mb_force_close();
  return MB_ERR_CONNECT;
}

static ModbusResult mb_transact(const uint8_t *req, size_t req_len, uint8_t *rsp, size_t rsp_len,
                                size_t *got) {
  if (got) {
    *got = 0;
  }

  log_net_context("transact-begin");
  log_hex("TX", req, req_len);

  ModbusResult cr = mb_ensure_connected();
  if (cr != MB_OK) {
    LOG_W("modbus", "transact abort: %s", modbus_result_text(cr));
    return cr;
  }

  int flushed = 0;
  while (g_mb.available() > 0) {
    (void)g_mb.read();
    flushed++;
  }
  if (flushed > 0) {
    LOG_W("modbus", "flushed %d stale RX bytes before TX", flushed);
  }

  const uint32_t t_tx = millis();
  const int written = g_mb.write(req, req_len);
  if (written != (int)req_len) {
    LOG_W("modbus", "socket write %d/%u — reconnect", written, (unsigned)req_len);
    mb_force_close();
    cr = mb_ensure_connected();
    if (cr != MB_OK) {
      return cr;
    }
    if (g_mb.write(req, req_len) != (int)req_len) {
      LOG_E("modbus", "socket write failed after reconnect");
      mb_force_close();
      return MB_ERR_CONNECT;
    }
  } else {
    LOG_I("modbus", "TX written %d B in %lums", written, (unsigned long)(millis() - t_tx));
  }
  g_mb.flush();

  size_t n = 0;
  const uint32_t deadline = millis() + kMbIoTimeoutMs;
  const uint32_t t_rx = millis();
  while (n < rsp_len && (int32_t)(deadline - millis()) > 0) {
    while (g_mb.available() && n < rsp_len) {
      int c = g_mb.read();
      if (c < 0) {
        break;
      }
      rsp[n++] = (uint8_t)c;
    }
    if (n >= 8) {
      const uint16_t len = ((uint16_t)rsp[4] << 8) | rsp[5];
      if (n >= (size_t)(6 + len)) {
        break;
      }
    }
    if (!g_mb.connected() && n == 0) {
      LOG_W("modbus", "socket dropped while waiting for RX");
      break;
    }
    delay(5);
  }

  g_mb_last_ms = millis();
  if (got) {
    *got = n;
  }
  log_hex("RX", rsp, n);
  LOG_I("modbus", "RX done %u B in %lums sock=%d", (unsigned)n, (unsigned long)(millis() - t_rx),
        g_mb.connected() ? 1 : 0);

  if (n < 8) {
    LOG_W("modbus", "short/no response — closing");
    LOG_W("modbus", "hint: TCP open maar geen PDU — unit-ID of Modbus TCP niet actief (nu unit=%u)",
          g_target.unit_id);
    mb_force_close();
    return MB_ERR_TIMEOUT;
  }
  return MB_OK;
}

static void fill_txn_header(uint8_t *req, uint8_t unit, uint8_t fc) {
  const uint16_t tid = g_txn_id++;
  if (g_txn_id == 0) {
    g_txn_id = 1;
  }
  req[0] = (uint8_t)(tid >> 8);
  req[1] = (uint8_t)tid;
  req[2] = 0;
  req[3] = 0;
  req[4] = 0;
  req[5] = 6;
  req[6] = unit;
  req[7] = fc;
}

static ModbusResult write_holding_u16(uint16_t address, uint16_t value) {
  LOG_I("modbus", "WRITE HR%u := %u (%s)", address, value, ventilation_mode_label(value));
  if (!g_has_target) {
    LOG_W("modbus", "write: no target");
    return MB_ERR_NO_TARGET;
  }
  if (!wifi_sta_connected()) {
    LOG_W("modbus", "write: no wifi");
    return MB_ERR_NO_WIFI;
  }
  if (address != NESSO_MB_HOLD_VENTILATION || !modbus_ventilation_allowed(value)) {
    LOG_W("modbus", "write: denied addr=%u val=%u", address, value);
    return MB_ERR_DENIED;
  }

  uint8_t req[12];
  fill_txn_header(req, g_target.unit_id, 6);
  req[8] = (uint8_t)(address >> 8);
  req[9] = (uint8_t)address;
  req[10] = (uint8_t)(value >> 8);
  req[11] = (uint8_t)value;

  uint8_t rsp[16];
  size_t got = 0;
  const ModbusResult tr = mb_transact(req, sizeof(req), rsp, sizeof(rsp), &got);
  if (tr != MB_OK) {
    LOG_W("modbus", "write result: %s", modbus_result_text(tr));
    return tr;
  }
  if (got < 9) {
    LOG_W("modbus", "write: RX too short %u", (unsigned)got);
    return MB_ERR_TIMEOUT;
  }
  if (rsp[7] & 0x80) {
    LOG_W("modbus", "write exception FC=0x%02X code=%u", rsp[7], got > 8 ? rsp[8] : 0);
    return MB_ERR_EXCEPTION;
  }
  if (got < 12 || rsp[7] != 6) {
    LOG_W("modbus", "write protocol: fc=%u got=%u", rsp[7], (unsigned)got);
    return MB_ERR_PROTOCOL;
  }
  const uint16_t echo_addr = ((uint16_t)rsp[8] << 8) | rsp[9];
  const uint16_t echo_val = ((uint16_t)rsp[10] << 8) | rsp[11];
  if (echo_addr != address || echo_val != value) {
    LOG_W("modbus", "write echo mismatch addr=%u/%u val=%u/%u", echo_addr, address, echo_val,
          value);
    return MB_ERR_PROTOCOL;
  }
  LOG_I("modbus", "WRITE OK HR%u=%u", address, value);
  return MB_OK;
}

static ModbusResult read_holding_u16_unit(uint16_t address, uint8_t unit, uint16_t *out) {
  LOG_I("modbus", "READ HR%u unit=%u", address, unit);
  if (out == nullptr) {
    return MB_ERR_PROTOCOL;
  }
  if (!g_has_target) {
    return MB_ERR_NO_TARGET;
  }
  if (!wifi_sta_connected()) {
    return MB_ERR_NO_WIFI;
  }
  if (address != NESSO_MB_HOLD_VENTILATION) {
    return MB_ERR_DENIED;
  }

  uint8_t req[12];
  fill_txn_header(req, unit, 3);
  req[8] = (uint8_t)(address >> 8);
  req[9] = (uint8_t)address;
  req[10] = 0;
  req[11] = 1;

  uint8_t rsp[16];
  size_t got = 0;
  const ModbusResult tr = mb_transact(req, sizeof(req), rsp, sizeof(rsp), &got);
  if (tr != MB_OK) {
    LOG_W("modbus", "read unit=%u: %s", unit, modbus_result_text(tr));
    return tr;
  }
  if (got < 9) {
    return MB_ERR_TIMEOUT;
  }
  if (rsp[7] & 0x80) {
    LOG_W("modbus", "read exception unit=%u FC=0x%02X code=%u", unit, rsp[7],
          got > 8 ? rsp[8] : 0);
    return MB_ERR_EXCEPTION;
  }
  if (got < 11 || rsp[7] != 3 || rsp[8] != 2) {
    LOG_W("modbus", "read protocol unit=%u fc=%u bc=%u got=%u", unit, rsp[7], rsp[8],
          (unsigned)got);
    return MB_ERR_PROTOCOL;
  }
  *out = ((uint16_t)rsp[9] << 8) | rsp[10];
  LOG_I("modbus", "READ OK unit=%u HR%u=%u (%s)", unit, address, *out,
        ventilation_mode_label(*out));
  return MB_OK;
}

static ModbusResult read_holding_u16(uint16_t address, uint16_t *out) {
  return read_holding_u16_unit(address, g_target.unit_id, out);
}
#endif

ModbusResult modbus_probe(uint16_t *out_mode_or_null) {
  LOG_I("modbus", "=== PROBE start ===");
  if (!g_has_target) {
    LOG_W("modbus", "probe: no target");
    return MB_ERR_NO_TARGET;
  }
#if NESSO_ENABLE_HW
  log_net_context("probe");
  /* Scan op worker-task (UI blijft responsive). Stilte = meestal verkeerde unit. */
  const uint8_t units[] = {g_target.unit_id, 1, 5, 255, 0, 2};
  uint16_t mode = 0;
  ModbusResult last = MB_ERR_TIMEOUT;
  for (size_t i = 0; i < sizeof(units); ++i) {
    bool dup = false;
    for (size_t j = 0; j < i; ++j) {
      if (units[j] == units[i]) {
        dup = true;
        break;
      }
    }
    if (dup) {
      continue;
    }
    last = read_holding_u16_unit(NESSO_MB_HOLD_VENTILATION, units[i], &mode);
    if (last == MB_OK) {
      if (units[i] != g_target.unit_id) {
        LOG_W("modbus", "antwoord op unit=%u (portal=%u) — zet unit-ID in portal", units[i],
              g_target.unit_id);
      }
      if (out_mode_or_null) {
        *out_mode_or_null = mode;
      }
      LOG_I("modbus", "=== PROBE end: OK unit=%u mode=%u ===", units[i], mode);
      return MB_OK;
    }
    mb_force_close();
  }
  LOG_W("modbus", "=== PROBE end: geen PDU op units — Modbus TCP echt aan in app? ===");
  return last;
#else
  if (out_mode_or_null) {
    *out_mode_or_null = VENT_NORMAL;
  }
  return MB_OK;
#endif
}

ModbusResult modbus_write_ventilation(uint16_t mode) {
  LOG_I("modbus", "=== WRITE vent mode=%u (%s) ===", mode, ventilation_mode_label(mode));
#if NESSO_ENABLE_HW
  const ModbusResult r = write_holding_u16(NESSO_MB_HOLD_VENTILATION, mode);
  LOG_I("modbus", "=== WRITE end: %s ===", modbus_result_text(r));
  return r;
#else
  if (!g_has_target) {
    return MB_ERR_NO_TARGET;
  }
  if (!modbus_ventilation_allowed(mode)) {
    return MB_ERR_DENIED;
  }
  LOG_I("modbus", "scaffold write HR%u=%u", NESSO_MB_HOLD_VENTILATION, mode);
  return MB_OK;
#endif
}

ModbusResult modbus_read_ventilation(uint16_t *out_mode) {
  LOG_I("modbus", "=== READ vent ===");
#if NESSO_ENABLE_HW
  const ModbusResult r = read_holding_u16(NESSO_MB_HOLD_VENTILATION, out_mode);
  LOG_I("modbus", "=== READ end: %s ===", modbus_result_text(r));
  return r;
#else
  if (out_mode == nullptr) {
    return MB_ERR_PROTOCOL;
  }
  if (!g_has_target) {
    return MB_ERR_NO_TARGET;
  }
  *out_mode = VENT_NORMAL;
  return MB_OK;
#endif
}

bool modbus_enqueue(const ActionSpec *spec) {
  if (!g_has_target) {
    LOG_W("modbus", "enqueue refused: no heatpump IP/unit configured");
    return false;
  }
  if (!action_is_ready(spec)) {
    LOG_W("modbus", "enqueue refused: action table not ready");
    return false;
  }
  if (spec->function != MB_FN_WRITE_SINGLE_REGISTER) {
    LOG_W("modbus", "enqueue refused: unsupported FC %u", (unsigned)spec->function);
    return false;
  }
  LOG_I("modbus", "enqueue btn=%u → HR%u=%u (async)", spec->button, spec->address, spec->value);
  return modbus_async_start_write(spec->value);
}

#if NESSO_ENABLE_HW
enum MbJobType { MB_JOB_PROBE = 1, MB_JOB_WRITE = 2 };

struct MbJob {
  MbJobType type;
  uint16_t mode;
};

static QueueHandle_t g_job_q = nullptr;
static portMUX_TYPE g_res_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool g_busy = false;
static volatile bool g_result_ready = false;
static ModbusResult g_async_result = MB_OK;
static uint16_t g_async_mode = 0;

static void mb_worker(void *arg) {
  (void)arg;
  MbJob job;
  for (;;) {
    if (xQueueReceive(g_job_q, &job, portMAX_DELAY) != pdTRUE) {
      continue;
    }
    ModbusResult r = MB_ERR_NO_TARGET;
    uint16_t mode = 0;
    if (job.type == MB_JOB_PROBE) {
      r = modbus_probe(&mode);
    } else if (job.type == MB_JOB_WRITE) {
      r = modbus_write_ventilation(job.mode);
      mode = job.mode;
    }
    portENTER_CRITICAL(&g_res_mux);
    g_async_result = r;
    g_async_mode = mode;
    g_result_ready = true;
    g_busy = false;
    portEXIT_CRITICAL(&g_res_mux);
  }
}

void modbus_worker_begin() {
  if (g_job_q != nullptr) {
    return;
  }
  g_job_q = xQueueCreate(2, sizeof(MbJob));
  /* ESP32-C6 is single-core; plain xTaskCreate. */
  xTaskCreate(mb_worker, "mb_tcp", 6144, nullptr, 1, nullptr);
  LOG_I("modbus", "worker task started");
}

bool modbus_async_busy() {
  return g_busy;
}

static bool mb_async_start(MbJobType type, uint16_t mode) {
  if (!g_has_target) {
    return false;
  }
  if (g_job_q == nullptr) {
    modbus_worker_begin();
  }
  portENTER_CRITICAL(&g_res_mux);
  if (g_busy) {
    portEXIT_CRITICAL(&g_res_mux);
    return false;
  }
  g_busy = true;
  g_result_ready = false;
  portEXIT_CRITICAL(&g_res_mux);

  MbJob job = {type, mode};
  if (xQueueSend(g_job_q, &job, 0) != pdTRUE) {
    portENTER_CRITICAL(&g_res_mux);
    g_busy = false;
    portEXIT_CRITICAL(&g_res_mux);
    return false;
  }
  return true;
}

bool modbus_async_start_probe() {
  LOG_I("modbus", "async probe queued");
  return mb_async_start(MB_JOB_PROBE, 0);
}

bool modbus_async_start_write(uint16_t mode) {
  LOG_I("modbus", "async write queued mode=%u", mode);
  return mb_async_start(MB_JOB_WRITE, mode);
}

bool modbus_async_take_result(ModbusResult *result, uint16_t *mode_out) {
  bool ready = false;
  portENTER_CRITICAL(&g_res_mux);
  if (g_result_ready) {
    if (result) {
      *result = g_async_result;
    }
    if (mode_out) {
      *mode_out = g_async_mode;
    }
    g_result_ready = false;
    ready = true;
  }
  portEXIT_CRITICAL(&g_res_mux);
  return ready;
}

void modbus_poll() {
  if (g_mb.connected() && !g_busy && (millis() - g_mb_last_ms) > kMbIdleCloseMs) {
    LOG_I("modbus", "idle close after %ums", (unsigned)kMbIdleCloseMs);
    g_mb.stop();
  }
}
#else
void modbus_worker_begin() {}
bool modbus_async_busy() {
  return false;
}
bool modbus_async_start_probe() {
  return false;
}
bool modbus_async_start_write(uint16_t mode) {
  (void)mode;
  return false;
}
bool modbus_async_take_result(ModbusResult *result, uint16_t *mode_out) {
  (void)result;
  (void)mode_out;
  return false;
}
void modbus_poll() {}
#endif
