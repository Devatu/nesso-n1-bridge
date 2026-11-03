#include "ble_provision.h"
#include "app_config.h"
#include "log.h"
#include "zigbee_coordinator.h"
#include <stdio.h>
#include <string.h>

#if NESSO_ENABLE_HW
#include <WiFi.h>
#include <WiFiProv.h>
#endif

static bool g_provisioned_flag = false;

BleProvSecurity ble_prov_selected_security() {
  /* Target Sec2 for CRA production posture when Arduino WiFiProv path is proven on Nesso.
   * Store apps negotiate sec_ver from device; until HW proof, advertise Sec1+PoP minimum. */
#if defined(NESSO_BLE_FORCE_SEC2) && NESSO_BLE_FORCE_SEC2
  return BLE_PROV_SEC_2_SRP;
#else
  return BLE_PROV_SEC_1_POP;
#endif
}

bool ble_prov_wifi_already_provisioned() {
  return g_provisioned_flag;
}

bool ble_prov_begin(BleProvStatus *status, uint32_t now_ms) {
  if (status == nullptr) {
    return false;
  }
  memset(status, 0, sizeof(*status));

  status->security = ble_prov_selected_security();
  if (status->security == BLE_PROV_SEC_FORBIDDEN_0) {
    LOG_E("ble_prov", "CRA refuse: Security 0 is forbidden");
    status->state = BLE_PROV_FAILED;
    return false;
  }

  if (zigbee_is_running()) {
    status->state = BLE_PROV_WAITING_ZIGBEE_PAUSE;
    LOG_W("ble_prov", "Zigbee still running; pause required before BLE (RF share)");
    zigbee_pause();
  }

  snprintf(status->service_name, sizeof(status->service_name), "%s", NESSO_BLE_PROV_SERVICE_PREFIX);
  snprintf(status->sec2_user, sizeof(status->sec2_user), "wifiprov");
  /* PoP filled by HW RNG on device; placeholder until NESSO_ENABLE_HW path. Never log real PoP. */
  snprintf(status->pop, sizeof(status->pop), "POP_PENDING");
  status->deadline_ms = now_ms + (uint32_t)NESSO_BLE_PROV_TIMEOUT_S * 1000U;
  status->state = BLE_PROV_ACTIVE;

#if NESSO_ENABLE_HW
  LOG_I("ble_prov", "TODO: WiFiProv BLE + Sec%u + PoP; client=ESP BLE Provisioning app",
        (unsigned)status->security);
  /* Never beginProvision with WIFI_PROV_SECURITY_0.
   * Sec1: WIFI_PROV_SECURITY_1 + pop
   * Sec2: when API available — SRP username + pop; app reads sec_ver */
#else
  LOG_W("ble_prov", "scaffold: BLE session for ESP BLE Provisioning app (sec=%u)",
        (unsigned)status->security);
#endif
  return true;
}

void ble_prov_poll(BleProvStatus *status, uint32_t now_ms) {
  if (status == nullptr || status->state == BLE_PROV_IDLE) {
    return;
  }
  if (status->state == BLE_PROV_ACTIVE || status->state == BLE_PROV_VALIDATING_STA) {
    if (now_ms >= status->deadline_ms) {
      status->state = BLE_PROV_TIMEOUT;
      LOG_W("ble_prov", "timeout; wipe PoP from RAM; Zigbee may resume after abort");
      memset(status->pop, 0, sizeof(status->pop));
    }
  }
}

void ble_prov_abort(BleProvStatus *status) {
  if (status == nullptr) {
    return;
  }
  status->state = BLE_PROV_IDLE;
  memset(status->pop, 0, sizeof(status->pop));
  LOG_I("ble_prov", "aborted; caller should Zigbee resume");
}

bool ble_prov_is_active(const BleProvStatus *status) {
  if (status == nullptr) {
    return false;
  }
  return status->state == BLE_PROV_ACTIVE || status->state == BLE_PROV_VALIDATING_STA ||
         status->state == BLE_PROV_WAITING_ZIGBEE_PAUSE;
}
