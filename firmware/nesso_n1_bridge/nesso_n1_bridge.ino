#include "action_dispatcher.h"
#include "app_config.h"
#include "ble_provision.h"
#include "display_ui.h"
#include "installer_portal.h"
#include "log.h"
#include "modbus_client.h"
#include "ota_manager.h"
#include "storage.h"
#include "ui_menu.h"
#include "wifi_manager.h"
#include "zigbee_coordinator.h"

#if NESSO_ENABLE_HW
#include <Arduino_Nesso_N1.h>
#endif

enum UiScreen {
  UI_HOME = 0,
  UI_MENU,
  UI_OTA,
  UI_INFO,
  UI_WIFI_CONFIRM,
  UI_FACTORY_CONFIRM,
  UI_MODBUS_TEST,
  UI_ZIGBEE,
  UI_BLE
};

/* Lijstvolgorde op testscherm: Uit, Gereduceerd, Normaal, Extra, Terug */
static const uint16_t k_vent_test_modes[] = {VENT_OFF, VENT_REDUCED, VENT_NORMAL, VENT_EXTRA};

static AppConfigStore g_store;
static EventDedup g_dedup;
static BleProvStatus g_ble;
static OtaStatus g_ota;
static bool g_service_mode = false;
static bool g_post_prov_done = false;
static uint32_t g_last_home_ms = 0;
static UiScreen g_screen = UI_HOME;
static uint8_t g_menu_index = 0;
static uint8_t g_modbus_test_index = 0;
static bool g_modbus_job_pending = false;

#if NESSO_ENABLE_HW
static bool g_key1_latched = false;
static bool g_key2_latched = false;
#endif

static void start_ble_wifi_provision(bool reset_creds) {
  LOG_I("app", "Wi-Fi provision via BLE");
  zigbee_pause();
  portal_stop();
  ble_prov_set_reset_on_next_begin(reset_creds);
  if (ble_prov_begin(&g_ble, millis())) {
    g_service_mode = true;
    g_post_prov_done = false;
    g_screen = UI_BLE;
    display_show_ble_prov(g_ble.service_name, g_ble.pop);
  } else {
    g_screen = UI_HOME;
    display_show_status("Wi-Fi koppelen mislukt");
    zigbee_resume();
  }
}

static void start_normal_services() {
  /* Wi-Fi first; Zigbee starts later in poll() after STA is up + coex enable. */
  wifi_sta_begin();
  if (heatpump_config_is_ready(&g_store.heatpump)) {
    modbus_set_target(&g_store.heatpump);
  }
  portal_begin_with_store(&g_store.heatpump, &g_ota, &g_store);
  zigbee_coordinator_begin(&g_store.registry, &g_dedup, &g_store);
  g_screen = UI_HOME;
  display_show_home();
}

static void do_factory_reset() {
  LOG_W("app", "factory reset requested");
  display_show_status("Bezig met reset…");
  portal_stop();
  wifi_erase_credentials();
  storage_factory_erase(); /* wis warmtepomp/Modbus + flags in NVS */
  storage_init_defaults(&g_store);
  delay(400);
#if NESSO_ENABLE_HW
  ESP.restart();
#endif
}

static void open_menu() {
  g_screen = UI_MENU;
  display_show_menu(g_menu_index);
}

static void activate_menu_item() {
  switch (ui_menu_id_at(g_menu_index)) {
  case UI_MENU_HOME:
  case UI_MENU_CLOSE:
    g_screen = UI_HOME;
    display_show_home();
    break;
  case UI_MENU_WIFI:
    g_screen = UI_WIFI_CONFIRM;
    display_show_wifi_confirm();
    break;
  case UI_MENU_UPDATE:
    g_ota.availability = OTA_AVAIL_CHECKING;
    g_screen = UI_OTA;
    display_show_ota(&g_ota);
    ota_check_for_ui(&g_ota);
    display_show_ota(&g_ota);
    break;
  case UI_MENU_MODBUS_TEST:
    g_modbus_test_index = 0;
    g_screen = UI_MODBUS_TEST;
    if (heatpump_config_is_ready(&g_store.heatpump)) {
      modbus_set_target(&g_store.heatpump);
      display_show_modbus_test(g_modbus_test_index, nullptr);
    } else {
      display_show_modbus_test(4, "Eerst warmtepomp-IP");
    }
    break;
  case UI_MENU_ZIGBEE:
    g_screen = UI_ZIGBEE;
    display_show_zigbee();
    break;
  case UI_MENU_INFO:
    g_screen = UI_INFO;
    display_show_info();
    break;
  case UI_MENU_FACTORY:
    g_screen = UI_FACTORY_CONFIRM;
    display_show_factory_confirm();
    break;
  default:
    break;
  }
}

static void activate_modbus_test() {
  if (g_modbus_test_index >= 4) {
    g_screen = UI_MENU;
    display_show_menu(g_menu_index);
    return;
  }
  if (!heatpump_config_is_ready(&g_store.heatpump)) {
    display_show_modbus_test(g_modbus_test_index, "Eerst warmtepomp-IP");
    return;
  }
  if (modbus_async_busy()) {
    display_show_modbus_test(g_modbus_test_index, "Nog bezig…");
    return;
  }
  modbus_set_target(&g_store.heatpump);
  const uint16_t mode = k_vent_test_modes[g_modbus_test_index];
  if (!modbus_async_start_write(mode)) {
    display_show_modbus_test(g_modbus_test_index, "Kon niet starten");
    return;
  }
  g_modbus_job_pending = true;
  display_show_modbus_test(g_modbus_test_index, "Bezig…");
}

static void poll_modbus_ui_result() {
  if (!g_modbus_job_pending || g_screen != UI_MODBUS_TEST) {
    return;
  }
  ModbusResult r = MB_OK;
  uint16_t mode = 0;
  if (!modbus_async_take_result(&r, &mode)) {
    return;
  }
  g_modbus_job_pending = false;
  char line[44];
  if (r == MB_OK) {
    snprintf(line, sizeof(line), "OK → %s", ventilation_mode_label(mode));
  } else {
    snprintf(line, sizeof(line), "%s", modbus_result_text(r));
  }
  display_show_modbus_test(g_modbus_test_index, line);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("=== Nesso serial ready ===");
  LOG_I("app", "boot %s schema=%u hw=%d", NESSO_FW_VERSION, NESSO_CONFIG_SCHEMA_VERSION,
        NESSO_ENABLE_HW);

  storage_load(&g_store);
  dispatcher_init(&g_dedup);
  ota_init(&g_ota, millis());
  ota_boot_validate_or_rollback(&g_ota, true);
  modbus_worker_begin();

  display_begin();
  display_bind_heatpump(&g_store.heatpump);

  wifi_sta_begin();
  if (wifi_has_saved_network() || g_store.wifi_provisioned || ble_prov_wifi_already_provisioned()) {
    g_store.wifi_provisioned = true;
    start_normal_services();
  } else {
    start_ble_wifi_provision(false);
  }
}

void loop() {
  uint32_t now = millis();

#if NESSO_ENABLE_HW
  const bool key1 = (digitalRead(KEY1) == LOW);
  const bool key2 = (digitalRead(KEY2) == LOW);

  if (key2 && !g_key2_latched) {
    g_key2_latched = true;
    if (g_screen == UI_BLE) {
      /* tijdens setup: negeren */
    } else if (g_screen == UI_MENU) {
      ui_menu_next(&g_menu_index);
      display_show_menu(g_menu_index);
    } else if (g_screen == UI_MODBUS_TEST) {
      g_modbus_test_index = (uint8_t)((g_modbus_test_index + 1) % 5);
      display_show_modbus_test(g_modbus_test_index, nullptr);
    } else if (g_screen == UI_ZIGBEE) {
      g_screen = UI_MENU;
      display_show_menu(g_menu_index);
    } else if (g_screen == UI_WIFI_CONFIRM || g_screen == UI_FACTORY_CONFIRM) {
      g_screen = UI_MENU;
      display_show_menu(g_menu_index);
    } else {
      open_menu();
    }
  } else if (!key2) {
    g_key2_latched = false;
  }

  if (key1 && !g_key1_latched) {
    g_key1_latched = true;
    if (g_screen == UI_MENU) {
      activate_menu_item();
    } else if (g_screen == UI_MODBUS_TEST) {
      activate_modbus_test();
    } else if (g_screen == UI_ZIGBEE) {
      if (zigbee_join_is_open()) {
        zigbee_close_join();
      } else {
        zigbee_open_join((uint8_t)NESSO_ZIGBEE_JOIN_WINDOW_S);
      }
      display_show_zigbee();
    } else if (g_screen == UI_WIFI_CONFIRM) {
      start_ble_wifi_provision(true);
    } else if (g_screen == UI_FACTORY_CONFIRM) {
      do_factory_reset();
    } else if (g_screen == UI_OTA) {
      if (g_ota.availability == OTA_AVAIL_UPDATE_AVAILABLE) {
        g_ota.state = OTA_DOWNLOAD;
        display_show_ota(&g_ota);
        if (!ota_apply_available(&g_ota)) {
          display_show_ota(&g_ota);
        }
      } else {
        open_menu();
      }
    } else if (g_screen == UI_HOME || g_screen == UI_INFO) {
      open_menu();
    }
  } else if (!key1) {
    g_key1_latched = false;
  }
#else
  (void)g_service_mode;
#endif

  ble_prov_poll(&g_ble, now);

  if (g_ble.state == BLE_PROV_SUCCESS && !g_post_prov_done) {
    g_post_prov_done = true;
    g_service_mode = false;
    g_store.wifi_provisioned = true;
    storage_save(&g_store);
    start_normal_services();
  } else if (g_ble.state == BLE_PROV_FAILED || g_ble.state == BLE_PROV_TIMEOUT) {
    g_screen = UI_HOME;
    display_show_status(g_ble.state == BLE_PROV_TIMEOUT ? "Tijd verlopen — menu > Wi-Fi"
                                                       : "Mislukt — menu > Wi-Fi");
    zigbee_resume();
    g_ble.state = BLE_PROV_IDLE;
  }

  if (!ble_prov_is_active(&g_ble)) {
    zigbee_coordinator_poll();
    if (g_screen == UI_HOME && !g_service_mode && (now - g_last_home_ms) > 5000) {
      g_last_home_ms = now;
      if (wifi_sta_connected()) {
        display_show_home();
      }
    }
    if (g_screen == UI_ZIGBEE && (now - g_last_home_ms) > 1000) {
      g_last_home_ms = now;
      display_show_zigbee();
    }
  }
  wifi_sta_poll();
  portal_poll();
  modbus_poll();
  poll_modbus_ui_result();
  ota_poll(&g_ota, now);
  delay(40);
}
