#include "zigbee_coordinator.h"
#include "action_dispatcher.h"
#include "action_table.h"
#include "app_config.h"
#include "log.h"
#include "modbus_client.h"
#include "shelly_decoder.h"
#include <stdio.h>
#include <string.h>

#if NESSO_ENABLE_HW
#ifndef ZIGBEE_MODE_ZCZR
#error "Zigbee ZCZR (coordinator) mode required"
#endif
#include "Zigbee.h"
#endif

static DeviceRegistry *g_reg = nullptr;
static EventDedup *g_dedup = nullptr;
static AppConfigStore *g_store = nullptr;
static bool g_running = false;
static bool g_paused = false;
static uint32_t g_join_until_ms = 0;
static char g_last_event[48] = "";

#if NESSO_ENABLE_HW
/* Light EP so Shelly remotes can bind and send On/Off / Level / Scenes. */
#define NESSO_ZB_LIGHT_EP 10
static ZigbeeDimmableLight g_zb_light = ZigbeeDimmableLight(NESSO_ZB_LIGHT_EP);

static void persist_registry() {
  if (g_store != nullptr) {
    storage_save(g_store);
  }
}

static void set_last_event(const char *msg) {
  if (msg == nullptr) {
    g_last_event[0] = '\0';
    return;
  }
  snprintf(g_last_event, sizeof(g_last_event), "%s", msg);
}

static bool join_open_now() {
  if (g_join_until_ms == 0) {
    return false;
  }
  const uint32_t now = millis();
  if ((int32_t)(now - g_join_until_ms) >= 0) {
    g_join_until_ms = 0;
    return false;
  }
  return true;
}

static bool eui_from_zcl_addr(const esp_zb_zcl_addr_t &addr, uint16_t *short_out, Eui64 *eui,
                              bool *resolved) {
  if (short_out) {
    *short_out = 0;
  }
  if (resolved) {
    *resolved = false;
  }
  if (eui == nullptr) {
    return false;
  }
  eui64_clear(eui);

  if (addr.addr_type == ESP_ZB_ZCL_ADDR_TYPE_IEEE) {
    memcpy(eui->bytes, addr.u.ieee_addr, 8);
    const bool ok = !eui64_is_zero(eui);
    if (resolved) {
      *resolved = ok;
    }
    return ok;
  }

  const uint16_t short_addr = addr.u.short_addr;
  if (short_out) {
    *short_out = short_addr;
  }
  esp_zb_ieee_addr_t ieee = {0};
  if (esp_zb_ieee_address_by_short(short_addr, ieee) == ESP_OK) {
    memcpy(eui->bytes, ieee, 8);
    const bool ok = !eui64_is_zero(eui);
    if (resolved) {
      *resolved = ok;
    }
    return ok;
  }
  return false;
}

static bool try_enroll_during_join(const Eui64 *eui) {
  if (g_reg == nullptr || eui == nullptr || eui64_is_zero(eui)) {
    return false;
  }
  if (!join_open_now()) {
    return false;
  }
  if (registry_is_authorized_shelly(g_reg, eui) || registry_is_router(g_reg, eui)) {
    return false;
  }
  char label[16];
  snprintf(label, sizeof(label), "Knop %d", registry_shelly_count(g_reg) + 1);
  if (!registry_enroll_shelly(g_reg, eui, label)) {
    set_last_event("join: registry vol");
    LOG_W("zigbee", "enroll failed (full or conflict)");
    return false;
  }
  persist_registry();
  set_last_event("join: nieuwe knop");
  LOG_I("zigbee", "enrolled Shelly %s count=%d", label, registry_shelly_count(g_reg));
  return true;
}

static void enroll_from_bound_devices() {
  if (!join_open_now() || g_reg == nullptr) {
    return;
  }
  std::list<zb_device_params_t *> devices = g_zb_light.getBoundDevices();
  for (zb_device_params_t *dev : devices) {
    if (dev == nullptr) {
      continue;
    }
    Eui64 eui;
    memcpy(eui.bytes, dev->ieee_addr, 8);
    try_enroll_during_join(&eui);
  }
}

static void handle_button_pipeline(ButtonEvent *ev) {
  if (ev == nullptr || g_reg == nullptr || g_dedup == nullptr) {
    return;
  }
  if (join_open_now() && ev->source_ieee_resolved) {
    try_enroll_during_join(&ev->source);
  }

  DispatchOutcome out = dispatcher_handle(g_reg, g_dedup, ev);
  char line[48];
  if (out.result == DISPATCH_QUEUED) {
    const ActionSpec *spec = action_for_button(ev->button);
    const bool ok = modbus_enqueue(spec);
    snprintf(line, sizeof(line), "btn%u → Modbus %s", ev->button, ok ? "OK" : "fail");
    LOG_I("zigbee", "%s", line);
  } else if (out.result == DISPATCH_AUTH_FAIL) {
    snprintf(line, sizeof(line), "btn%u geweigerd (niet gekoppeld)", ev->button);
    LOG_W("zigbee", "%s auth=%d", line, (int)out.auth);
  } else if (out.result == DISPATCH_DEDUPED) {
    snprintf(line, sizeof(line), "btn%u dedup", ev->button);
  } else {
    snprintf(line, sizeof(line), "btn%u result=%d", ev->button, (int)out.result);
  }
  set_last_event(line);
}

static void on_privilege_command(const esp_zb_zcl_privilege_command_message_t *message) {
  if (message == nullptr || g_paused || !g_running) {
    return;
  }

  ZclIncoming in;
  memset(&in, 0, sizeof(in));
  in.cluster = message->info.cluster;
  in.command = message->info.command.id;
  in.src_endpoint = message->info.src_endpoint;
  eui_from_zcl_addr(message->info.src_address, &in.src_short, &in.src_ieee, &in.ieee_resolved);

  if (message->data != nullptr && message->size > 0) {
    const uint8_t *p = (const uint8_t *)message->data;
    if (in.cluster == ZCL_CLUSTER_LEVEL && in.command == ZCL_CMD_STEP && message->size >= 1) {
      in.step_mode = p[0];
    } else if (in.cluster == ZCL_CLUSTER_SCENES && in.command == ZCL_CMD_RECALL_SCENE) {
      /* group_id (2) + scene_id (1) */
      if (message->size >= 3) {
        in.scene_id = p[2];
      } else if (message->size >= 1) {
        in.scene_id = p[0];
      }
    }
  }

  LOG_I("zigbee", "ZCL cl=0x%04x cmd=0x%02x ep=%u short=0x%04x ieee=%d", in.cluster, in.command,
        in.src_endpoint, in.src_short, in.ieee_resolved ? 1 : 0);

  ButtonEvent ev;
  memset(&ev, 0, sizeof(ev));
  if (!shelly_decode(&in, &ev)) {
    return;
  }
  ev.timestamp_ms = millis();
  handle_button_pipeline(&ev);
}

static void register_privilege_commands() {
  /* On/Off */
  g_zb_light.addPrivilegeCommand(ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ZCL_CMD_OFF);
  g_zb_light.addPrivilegeCommand(ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ZCL_CMD_ON);
  g_zb_light.addPrivilegeCommand(ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ZCL_CMD_TOGGLE);
  /* Level step (hold) */
  g_zb_light.addPrivilegeCommand(ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL, ZCL_CMD_STEP);
  /* Scenes recall (multi-press) */
  g_zb_light.addPrivilegeCommand(ESP_ZB_ZCL_CLUSTER_ID_SCENES, ZCL_CMD_RECALL_SCENE);
  g_zb_light.onPrivilegeCommand(on_privilege_command);
}
#endif /* NESSO_ENABLE_HW */

bool zigbee_coordinator_begin(DeviceRegistry *reg, EventDedup *dedup, AppConfigStore *store) {
  g_reg = reg;
  g_dedup = dedup;
  g_store = store;
  g_paused = false;
  g_join_until_ms = 0;
  g_last_event[0] = '\0';

#if NESSO_ENABLE_HW
  if (Zigbee.started()) {
    g_running = true;
    LOG_I("zigbee", "already started");
    return true;
  }

  g_zb_light.setManufacturerAndModel("Qvantum", "NessoN1Bridge");
  g_zb_light.allowMultipleBinding(true);
  register_privilege_commands();

  if (!Zigbee.addEndpoint(&g_zb_light)) {
    LOG_E("zigbee", "addEndpoint failed");
    g_running = false;
    return false;
  }

  /* Closed by default — never setRebootOpenNetwork for production. */
  Zigbee.setTimeout(45000);
  if (!Zigbee.begin(ZIGBEE_COORDINATOR, false)) {
    LOG_E("zigbee", "Zigbee.begin(COORDINATOR) failed");
    g_running = false;
    return false;
  }

  g_running = true;
  LOG_I("zigbee", "coordinator up; join closed; shellys=%d",
        g_reg ? registry_shelly_count(g_reg) : 0);
  return true;
#else
  LOG_W("zigbee", "scaffold (HOST) — no radio");
  g_running = false;
  return false;
#endif
}

void zigbee_coordinator_poll() {
#if NESSO_ENABLE_HW
  if (!g_running || g_paused) {
    return;
  }
  if (g_join_until_ms != 0 && !join_open_now()) {
    LOG_I("zigbee", "join window ended");
    set_last_event("join gesloten");
  }
  static uint32_t last_bind_scan_ms = 0;
  const uint32_t now = millis();
  if (join_open_now() && (now - last_bind_scan_ms) > 2000) {
    last_bind_scan_ms = now;
    enroll_from_bound_devices();
  }
#else
  (void)0;
#endif
}

bool zigbee_open_join(uint8_t seconds) {
  if (seconds == 0) {
    seconds = (uint8_t)NESSO_ZIGBEE_JOIN_WINDOW_S;
  }
  if (seconds > NESSO_ZIGBEE_JOIN_WINDOW_S) {
    seconds = (uint8_t)NESSO_ZIGBEE_JOIN_WINDOW_S;
  }
#if NESSO_ENABLE_HW
  if (!g_running || g_paused || !Zigbee.started()) {
    LOG_W("zigbee", "open_join refused (running=%d paused=%d)", g_running ? 1 : 0, g_paused ? 1 : 0);
    return false;
  }
  Zigbee.openNetwork(seconds);
  g_join_until_ms = millis() + (uint32_t)seconds * 1000UL;
  set_last_event("join open");
  LOG_I("zigbee", "join open %u s — zet Shelly in pair (5× knop)", (unsigned)seconds);
  return true;
#else
  (void)seconds;
  return false;
#endif
}

void zigbee_close_join() {
  g_join_until_ms = 0;
#if NESSO_ENABLE_HW
  if (Zigbee.started()) {
    Zigbee.closeNetwork();
  }
#endif
  set_last_event("join gesloten");
  LOG_I("zigbee", "join closed");
}

bool zigbee_join_is_open() {
#if NESSO_ENABLE_HW
  return join_open_now();
#else
  return false;
#endif
}

uint32_t zigbee_join_remaining_s() {
#if NESSO_ENABLE_HW
  if (!join_open_now()) {
    return 0;
  }
  const uint32_t now = millis();
  return (g_join_until_ms - now + 999UL) / 1000UL;
#else
  return 0;
#endif
}

void zigbee_pause() {
  g_paused = true;
  g_join_until_ms = 0;
#if NESSO_ENABLE_HW
  if (Zigbee.started()) {
    Zigbee.closeNetwork();
    Zigbee.stop();
  }
#endif
  g_running = false;
  LOG_I("zigbee", "paused (required before SoftAP/BLE)");
}

void zigbee_resume() {
#if NESSO_ENABLE_HW
  if (Zigbee.initialized() && !Zigbee.started()) {
    Zigbee.start();
  }
  g_running = Zigbee.started();
#else
  g_running = false;
#endif
  g_paused = false;
  LOG_I("zigbee", "resume running=%d", g_running ? 1 : 0);
}

bool zigbee_is_running() {
  return g_running && !g_paused;
}

int zigbee_shelly_count() {
  return g_reg ? registry_shelly_count(g_reg) : 0;
}

bool zigbee_remove_shelly_index(uint8_t index) {
  if (g_reg == nullptr || index >= NESSO_MAX_SHELLY_SLOTS) {
    return false;
  }
  if (!g_reg->shelly[index].occupied) {
    return false;
  }
  Eui64 eui = g_reg->shelly[index].eui;
  if (!registry_remove_shelly(g_reg, &eui)) {
    return false;
  }
#if NESSO_ENABLE_HW
  persist_registry();
#endif
  set_last_event("knop verwijderd");
  return true;
}

void zigbee_format_status_json(char *buf, size_t buflen) {
  if (buf == nullptr || buflen < 32) {
    return;
  }
  const int count = zigbee_shelly_count();
  int n = snprintf(buf, buflen,
                   "{\"running\":%s,\"join_open\":%s,\"join_remaining_s\":%u,\"shelly_count\":%d,"
                   "\"last_event\":\"%s\",\"shellys\":[",
                   zigbee_is_running() ? "true" : "false", zigbee_join_is_open() ? "true" : "false",
                   (unsigned)zigbee_join_remaining_s(), count, g_last_event[0] ? g_last_event : "");
  if (n < 0 || (size_t)n >= buflen) {
    buf[0] = '\0';
    return;
  }
  size_t used = (size_t)n;
  if (g_reg != nullptr) {
    for (int i = 0; i < NESSO_MAX_SHELLY_SLOTS; ++i) {
      if (!g_reg->shelly[i].occupied) {
        continue;
      }
      const Eui64 *e = &g_reg->shelly[i].eui;
      char eui_str[28];
      snprintf(eui_str, sizeof(eui_str), "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", e->bytes[7],
               e->bytes[6], e->bytes[5], e->bytes[4], e->bytes[3], e->bytes[2], e->bytes[1],
               e->bytes[0]);
      const char *comma = (used > 0 && buf[used - 1] == '[') ? "" : ",";
      int m = snprintf(buf + used, buflen - used,
                       "%s{\"index\":%d,\"label\":\"%s\",\"eui\":\"%s\",\"enabled\":%s}", comma, i,
                       g_reg->shelly[i].label[0] ? g_reg->shelly[i].label : "Knop", eui_str,
                       g_reg->shelly[i].enabled ? "true" : "false");
      if (m < 0 || (size_t)m >= buflen - used) {
        break;
      }
      used += (size_t)m;
    }
  }
  if (used + 2 < buflen) {
    buf[used++] = ']';
    buf[used++] = '}';
    buf[used] = '\0';
  }
}
