#include "zigbee_coordinator.h"
#include "action_dispatcher.h"
#include "action_table.h"
#include "app_config.h"
#include "log.h"
#include "modbus_client.h"
#include "shelly_decoder.h"
#include "wifi_manager.h"
#include <stdio.h>
#include <string.h>

#if NESSO_ENABLE_HW
#ifndef ZIGBEE_MODE_ZCZR
#error "Zigbee ZCZR (coordinator) mode required"
#endif
#include "Zigbee.h"
#include <esp_coexist.h>
#include <WiFi.h>
#include "hal/brownout_ll.h"
#include "bdb/esp_zigbee_bdb_commissioning.h"
#include "nwk/esp_zigbee_nwk.h"
/* BOD HW-reset is disabled; keep TX usable for battery remotes during join. */
#ifndef NESSO_ZB_TX_DBM
#define NESSO_ZB_TX_DBM 15
#endif
#endif

static DeviceRegistry *g_reg = nullptr;
static EventDedup *g_dedup = nullptr;
static AppConfigStore *g_store = nullptr;
static bool g_running = false;
static bool g_paused = false;
static bool g_start_pending = false;
static bool g_start_failed = false;
static bool g_start_task_launched = false;
static uint32_t g_join_until_ms = 0;
static char g_last_event[48] = "";

#if NESSO_ENABLE_HW
/* Light EP so Shelly remotes can bind and send On/Off / Level / Scenes. */
#define NESSO_ZB_LIGHT_EP 10

static bool try_enroll_during_join(const Eui64 *eui);
static void set_last_event(const char *msg);

/* Bind Shelly client clusters → our light EP (ZigbeeDimmableLight has empty findEndpoint). */
class NessoShellyTargetLight : public ZigbeeDimmableLight {
 public:
  explicit NessoShellyTargetLight(uint8_t endpoint) : ZigbeeDimmableLight(endpoint) {}

  void findEndpoint(esp_zb_zdo_match_desc_req_param_t *cmd_req) override {
    if (cmd_req == nullptr) {
      return;
    }
    /* Shelly BLU RC Button 4 ZB: On/Off is CLIENT (out-cluster). */
    uint16_t cluster_list[] = {ESP_ZB_ZCL_CLUSTER_ID_ON_OFF};
    esp_zb_zdo_match_desc_req_param_t req = {
        .dst_nwk_addr = cmd_req->dst_nwk_addr,
        .addr_of_interest = cmd_req->addr_of_interest,
        .profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .num_in_clusters = 0,
        .num_out_clusters = 1,
        .cluster_list = cluster_list,
    };
    LOG_I("zigbee", "match On/Off-client @ short=0x%04x", cmd_req->dst_nwk_addr);
    esp_zb_zdo_match_cluster(&req, &NessoShellyTargetLight::findCbWrapper, this);
  }

  void bindRemoteClusters(uint16_t short_addr, const esp_zb_ieee_addr_t ieee) {
    Eui64 eui;
    memcpy(eui.bytes, ieee, 8);
    if (try_enroll_during_join(&eui)) {
      set_last_event("join: knop gezien");
    }
    static const uint16_t clusters[] = {
        ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
        ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,
        ESP_ZB_ZCL_CLUSTER_ID_SCENES,
    };
    for (uint8_t ep = 1; ep <= 4; ++ep) {
      for (uint16_t cl : clusters) {
        esp_zb_zdo_bind_req_param_t bind_req;
        memset(&bind_req, 0, sizeof(bind_req));
        memcpy(bind_req.src_address, ieee, sizeof(esp_zb_ieee_addr_t));
        bind_req.src_endp = ep;
        bind_req.cluster_id = cl;
        bind_req.dst_addr_mode = ESP_ZB_ZDO_BIND_DST_ADDR_MODE_64_BIT_EXTENDED;
        esp_zb_get_long_address(bind_req.dst_address_u.addr_long);
        bind_req.dst_endp = (uint8_t)getEndpoint();
        bind_req.req_dst_addr = short_addr;
        esp_zb_zdo_device_bind_req(&bind_req, &NessoShellyTargetLight::bindCbWrapper, this);
      }
    }
    /* Track as bound so enroll_from_bound_devices / UI see it. */
    zb_device_params_t *dev = (zb_device_params_t *)malloc(sizeof(zb_device_params_t));
    if (dev != nullptr) {
      memset(dev, 0, sizeof(*dev));
      dev->endpoint = 1;
      dev->short_addr = short_addr;
      memcpy(dev->ieee_addr, ieee, 8);
      addBoundDevice(dev);
    }
    LOG_I("zigbee", "bind reqs sent short=0x%04x (EP1-4 OnOff/Level/Scenes)", short_addr);
  }

 private:
  static void findCbWrapper(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint,
                            void *user_ctx) {
    auto *self = static_cast<NessoShellyTargetLight *>(user_ctx);
    if (self != nullptr) {
      self->findCb(zdo_status, addr, endpoint);
    }
  }

  void findCb(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint) {
    if (zdo_status != ESP_ZB_ZDP_STATUS_SUCCESS) {
      LOG_W("zigbee", "no On/Off-client match (status=%d)", (int)zdo_status);
      return;
    }
    esp_zb_ieee_addr_t ieee = {0};
    if (esp_zb_ieee_address_by_short(addr, ieee) != ESP_OK) {
      LOG_W("zigbee", "IEEE lookup failed for 0x%04x", addr);
      return;
    }
    LOG_I("zigbee", "Shelly-achtig device short=0x%04x ep=%u — enroll+bind", addr, endpoint);
    bindRemoteClusters(addr, ieee);
  }

  static void bindCbWrapper(esp_zb_zdp_status_t zdo_status, void *user_ctx) {
    (void)user_ctx;
    if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
      LOG_I("zigbee", "bind OK");
    } else {
      LOG_W("zigbee", "bind status=%d (remote mogelijk al slapen — F&B opnieuw)", (int)zdo_status);
    }
  }
};

static NessoShellyTargetLight g_zb_light = NessoShellyTargetLight(NESSO_ZB_LIGHT_EP);
static bool g_ep_ready = false;

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

#if NESSO_ENABLE_HW
/* Soften BOD so a brief RF current dip does not hard-reset the chip. */
static void soften_brownout_for_rf() {
  brownout_ll_ana_reset_enable(false);
  brownout_ll_reset_config(false, 0x3ff, BROWNOUT_RESET_LEVEL_SYSTEM);
  brownout_ll_intr_enable(false);
  brownout_ll_bod_enable(false);
  LOG_I("zigbee", "brownout HW-reset disabled (USB/RF spike mitigation)");
}

static void apply_zb_tx_power() {
  esp_zb_set_tx_power((int8_t)NESSO_ZB_TX_DBM);
  int8_t got = 0;
  esp_zb_get_tx_power(&got);
  LOG_I("zigbee", "TX power set %d dBm (readback %d)", (int)NESSO_ZB_TX_DBM, (int)got);
}
#endif

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

/* Fallback: catch children that joined even if match-desc missed them. */
static void enroll_from_neighbors() {
  if (!join_open_now() || g_reg == nullptr) {
    return;
  }
  esp_zb_nwk_info_iterator_t it = ESP_ZB_NWK_INFO_ITERATOR_INIT;
  esp_zb_nwk_neighbor_info_t nbr;
  while (esp_zb_nwk_get_next_neighbor(&it, &nbr) == ESP_OK) {
    if (nbr.relationship != ESP_ZB_NWK_RELATIONSHIP_CHILD &&
        nbr.relationship != ESP_ZB_NWK_RELATIONSHIP_UNAUTHENTICATED_CHILD) {
      continue;
    }
    Eui64 eui;
    memcpy(eui.bytes, nbr.ieee_addr, 8);
    if (eui64_is_zero(&eui)) {
      continue;
    }
    if (try_enroll_during_join(&eui)) {
      LOG_I("zigbee", "neighbor child enrolled short=0x%04x lqi=%u", nbr.short_addr, nbr.lqi);
      g_zb_light.bindRemoteClusters(nbr.short_addr, nbr.ieee_addr);
    }
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

#if NESSO_ENABLE_HW
static void zigbee_start_task(void *arg) {
  (void)arg;
  LOG_I("zigbee", "background start (Wi-Fi already up)");
  set_last_event("Zigbee starten…");

  if (!g_ep_ready) {
    g_zb_light.setManufacturerAndModel("Qvantum", "NessoN1Bridge");
    g_zb_light.allowMultipleBinding(true);
    register_privilege_commands();
    if (!Zigbee.addEndpoint(&g_zb_light)) {
      LOG_E("zigbee", "addEndpoint failed");
      g_start_failed = true;
      g_start_pending = false;
      g_start_task_launched = false;
      set_last_event("endpoint fout");
      vTaskDelete(nullptr);
      return;
    }
    g_ep_ready = true;
  }

  /* Reinforce coex (also enabled in wifi_sta_begin). */
  const esp_err_t coex = esp_coex_wifi_i154_enable();
  LOG_I("zigbee", "esp_coex_wifi_i154_enable → %d", (int)coex);

  Zigbee.setTimeout(45000);
  if (!Zigbee.begin(ZIGBEE_COORDINATOR, false)) {
    LOG_E("zigbee", "Zigbee.begin(COORDINATOR) failed");
    g_running = false;
    g_start_failed = true;
    g_start_pending = false;
    g_start_task_launched = false;
    set_last_event("Zigbee start mislukt");
    wifi_sta_reconnect();
    vTaskDelete(nullptr);
    return;
  }

  g_running = true;
  g_start_pending = false;
  g_start_failed = false;
  g_start_task_launched = false;
  soften_brownout_for_rf();
  apply_zb_tx_power();
  set_last_event("coordinator aan");
  LOG_I("zigbee", "coordinator up; shellys=%d wifi=%d",
        g_reg ? registry_shelly_count(g_reg) : 0, wifi_sta_connected() ? 1 : 0);

  /* Formation often drops STA on single-RF C6 — reconnect and wait a bit. */
  for (int i = 0; i < 20 && !wifi_sta_connected(); ++i) {
    if (i == 0 || i == 5 || i == 10) {
      wifi_sta_reconnect();
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  LOG_I("zigbee", "post-start wifi=%d", wifi_sta_connected() ? 1 : 0);
  vTaskDelete(nullptr);
}
#endif

bool zigbee_coordinator_begin(DeviceRegistry *reg, EventDedup *dedup, AppConfigStore *store) {
  g_reg = reg;
  g_dedup = dedup;
  g_store = store;
  g_paused = false;
  g_join_until_ms = 0;
  g_last_event[0] = '\0';
  g_start_failed = false;
  g_start_task_launched = false;

#if NESSO_ENABLE_HW
  if (Zigbee.started()) {
    g_running = true;
    g_start_pending = false;
    LOG_I("zigbee", "already started");
    return true;
  }

  /* Only start after Wi-Fi STA is connected. Never block the UI loop. */
  g_start_pending = true;
  g_running = false;
  set_last_event("wacht op Wi-Fi");
  LOG_I("zigbee", "waiting for Wi-Fi before Zigbee (ESP32-C6 coex=C1)");
  return true;
#else
  LOG_W("zigbee", "scaffold (HOST) — no radio");
  g_running = false;
  return false;
#endif
}

void zigbee_coordinator_poll() {
#if NESSO_ENABLE_HW
  if (g_paused) {
    return;
  }

  if (g_start_pending && !g_running && !g_start_failed && !g_start_task_launched) {
    if (wifi_sta_connected()) {
      g_start_task_launched = true;
      set_last_event("Zigbee starten…");
      LOG_I("zigbee", "Wi-Fi up — launching Zigbee task");
      /* Separate task: Zigbee.begin() can block ~tens of seconds. */
      xTaskCreate(zigbee_start_task, "zb_start", 8192, nullptr, 1, nullptr);
    }
  }

  if (!g_running) {
    return;
  }
  if (g_join_until_ms != 0 && !join_open_now()) {
    LOG_I("zigbee", "join window ended");
    g_join_until_ms = 0;
#if NESSO_ENABLE_HW
    esp_zb_bdb_finding_binding_cancel_target(NESSO_ZB_LIGHT_EP);
    wifi_sta_set_join_tx_low(false);
    wifi_sta_set_hold_off(false);
    wifi_sta_ensure();
#endif
    set_last_event("join gesloten");
  }
  static uint32_t last_bind_scan_ms = 0;
  const uint32_t now = millis();
  if (join_open_now() && (now - last_bind_scan_ms) > 2000) {
    last_bind_scan_ms = now;
    enroll_from_bound_devices();
    enroll_from_neighbors();
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
  wifi_sta_set_hold_off(true); /* stop Wi-Fi reconnect thrash during pairing */
  soften_brownout_for_rf();
  wifi_sta_set_join_tx_low(true);
  apply_zb_tx_power();
  delay(30); /* let PA settle before permit-join beacon burst */
  Zigbee.openNetwork(seconds);
  /* Shelly needs F&B target (or our auto-bind) after join; BDB min duration is 180s. */
  const uint16_t fb_s =
      (seconds < (uint8_t)ESP_ZB_BDB_MIN_COMMISSIONING_TIME) ? (uint16_t)ESP_ZB_BDB_MIN_COMMISSIONING_TIME
                                                             : (uint16_t)seconds;
  const esp_err_t fb = esp_zb_bdb_finding_binding_start_target(NESSO_ZB_LIGHT_EP, fb_s);
  LOG_I("zigbee", "F&B target ep=%u %us → %d", NESSO_ZB_LIGHT_EP, (unsigned)fb_s, (int)fb);
  g_join_until_ms = millis() + (uint32_t)seconds * 1000UL;
  set_last_event("join open");
  LOG_I("zigbee", "join open %u s — Shelly: 5× knop (paars), dichtbij houden", (unsigned)seconds);
  return true;
#else
  (void)seconds;
  return false;
#endif
}

void zigbee_close_join() {
  g_join_until_ms = 0;
#if NESSO_ENABLE_HW
  esp_zb_bdb_finding_binding_cancel_target(NESSO_ZB_LIGHT_EP);
  if (Zigbee.started()) {
    Zigbee.closeNetwork();
  }
  wifi_sta_set_join_tx_low(false);
  wifi_sta_set_hold_off(false);
  wifi_sta_ensure(); /* restore STA gently after pairing window */
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
  wifi_sta_set_hold_off(false);
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
