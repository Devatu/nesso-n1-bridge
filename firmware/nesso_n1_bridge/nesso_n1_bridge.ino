#include "action_dispatcher.h"
#include "app_config.h"
#include "ble_provision.h"
#include "display_ui.h"
#include "installer_portal.h"
#include "log.h"
#include "modbus_client.h"
#include "ota_manager.h"
#include "storage.h"
#include "wifi_manager.h"
#include "zigbee_coordinator.h"

#if NESSO_ENABLE_HW
#include <Arduino_Nesso_N1.h>
#endif

static AppConfigStore g_store;
static EventDedup g_dedup;
static BleProvStatus g_ble;
static OtaStatus g_ota;
static bool g_service_mode = false;

static void start_ble_wifi_provision() {
  LOG_I("app", "Wi-Fi provision via BLE (not SoftAP primary)");
  zigbee_pause();
  if (ble_prov_begin(&g_ble, millis())) {
    g_service_mode = true;
    display_show_status("BLE Wi-Fi setup");
    display_show_service_qr(g_ble.service_name);
  } else {
    display_show_status("BLE prov fail");
    zigbee_resume();
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  LOG_I("app", "boot %s schema=%u hw=%d", NESSO_FW_VERSION, NESSO_CONFIG_SCHEMA_VERSION,
        NESSO_ENABLE_HW);

  storage_load(&g_store);
  dispatcher_init(&g_dedup);
  ota_init(&g_ota, millis());
  ota_boot_validate_or_rollback(&g_ota, true);

  display_begin();
  display_show_status("Fase0+prov");

  if (!g_store.wifi_provisioned && !ble_prov_wifi_already_provisioned()) {
    start_ble_wifi_provision();
  } else {
    wifi_sta_begin();
    zigbee_coordinator_begin();
    if (heatpump_config_is_ready(&g_store.heatpump)) {
      modbus_set_target(&g_store.heatpump);
    }
    portal_begin(&g_store.heatpump);
  }

  LOG_I("app", "KEY1/KEY2: service/BLE re-provision; portal sets heatpump IP/port/unit only");
}

void loop() {
  uint32_t now = millis();

#if NESSO_ENABLE_HW
  if (digitalRead(KEY1) == LOW) {
    start_ble_wifi_provision();
    delay(300);
  }
#else
  (void)g_service_mode;
#endif

  ble_prov_poll(&g_ble, now);
  if (!ble_prov_is_active(&g_ble)) {
    zigbee_coordinator_poll();
  }
  wifi_sta_poll();
  portal_poll();
  modbus_poll();
  ota_poll(&g_ota, now);
  delay(50);
}
