#include "wifi_manager.h"
#include "log.h"

static bool g_ap = false;
static bool g_sta = false;

bool wifi_sta_begin() {
  LOG_W("wifi", "TODO: STA from BLE-provisioned credentials; never log password");
  g_sta = false;
  return false;
}

void wifi_sta_poll() {}

bool wifi_sta_connected() {
  return g_sta;
}

bool wifi_start_service_ap_fallback() {
  LOG_W("wifi", "SoftAP fallback only; Zigbee must be paused (Espressif SoftAP+ZR=X)");
  g_ap = false;
  return false;
}

void wifi_stop_service_ap() {
  g_ap = false;
}

bool wifi_service_ap_active() {
  return g_ap;
}
