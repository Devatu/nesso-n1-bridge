#include "ble_provision.h"
#include "app_config.h"
#include "log.h"
#include "wifi_manager.h"
#include "zigbee_coordinator.h"
#include <stdio.h>
#include <string.h>

#if NESSO_ENABLE_HW
#include <WiFi.h>
#include <WiFiProv.h>
#include <esp_random.h>
#include <network_provisioning/manager.h>
#endif

static bool g_provisioned_flag = false;
static bool g_reset_provisioned = false;
static bool g_event_hooked = false;
static BleProvStatus *g_active = nullptr;
static bool g_saw_cred_success = false;
static bool g_saw_got_ip = false;

#if NESSO_ENABLE_HW
/* Fresh instance each session — global WiFiProv keeps provInitDone and skips re-init. */
static WiFiProvClass *g_prov = nullptr;

static WiFiProvClass &prov() {
  if (g_prov == nullptr) {
    g_prov = new WiFiProvClass();
  }
  return *g_prov;
}

static void prov_recycle() {
  if (g_prov != nullptr) {
    g_prov->endProvision();
  }
  network_prov_mgr_deinit();
  delete g_prov;
  g_prov = new WiFiProvClass();
}
#endif

BleProvSecurity ble_prov_selected_security() {
#if defined(NESSO_BLE_FORCE_SEC2) && NESSO_BLE_FORCE_SEC2
  return BLE_PROV_SEC_2_SRP;
#else
  return BLE_PROV_SEC_1_POP;
#endif
}

bool ble_prov_wifi_already_provisioned() {
  return g_provisioned_flag;
}

void ble_prov_set_reset_on_next_begin(bool reset) {
  g_reset_provisioned = reset;
}

#if NESSO_ENABLE_HW
static void fill_pop(char *out, size_t out_len) {
  if (out == nullptr || out_len < 9) {
    return;
  }
  /* 8 hex chars from HW RNG — shown on display, entered in ESP BLE Provisioning app. */
  uint32_t r0 = esp_random();
  uint32_t r1 = esp_random();
  snprintf(out, out_len, "%08X", (unsigned)(r0 ^ (r1 << 1)));
}

static void mark_success_if_ready() {
  if (g_active == nullptr) {
    return;
  }
  /* App needs BLE ACK (CRED_SUCCESS → PROV_END). Do not resume Zigbee before that. */
  if (g_saw_cred_success && g_saw_got_ip) {
    g_active->state = BLE_PROV_SUCCESS;
    g_provisioned_flag = true;
    memset(g_active->pop, 0, sizeof(g_active->pop));
    LOG_I("ble_prov", "provision OK (creds+IP) — app ACK done");
  }
}

static void on_sys_event(arduino_event_t *ev) {
  if (g_active == nullptr || ev == nullptr) {
    return;
  }
  switch (ev->event_id) {
  case ARDUINO_EVENT_PROV_START:
    g_active->state = BLE_PROV_ACTIVE;
    LOG_I("ble_prov", "provisioning started (BLE Sec1)");
    break;
  case ARDUINO_EVENT_PROV_CRED_RECV:
    /* Do not log password. SSID length only. */
    LOG_I("ble_prov", "credentials received (ssid_len=%u)",
          (unsigned)strlen((const char *)ev->event_info.prov_cred_recv.ssid));
    g_active->state = BLE_PROV_VALIDATING_STA;
    break;
  case ARDUINO_EVENT_PROV_CRED_FAIL:
    LOG_E("ble_prov", "credential fail");
    g_active->state = BLE_PROV_FAILED;
    memset(g_active->pop, 0, sizeof(g_active->pop));
    break;
  case ARDUINO_EVENT_PROV_CRED_SUCCESS:
    LOG_I("ble_prov", "credentials accepted (notify app)");
    g_saw_cred_success = true;
    mark_success_if_ready();
    break;
  case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    LOG_I("ble_prov", "STA got IP");
    g_saw_got_ip = true;
    /* Stay in VALIDATING until CRED_SUCCESS/PROV_END so Zigbee does not kill BLE ACK. */
    if (g_active->state == BLE_PROV_ACTIVE || g_active->state == BLE_PROV_VALIDATING_STA) {
      g_active->state = BLE_PROV_VALIDATING_STA;
    }
    mark_success_if_ready();
    break;
  case ARDUINO_EVENT_PROV_END:
    LOG_I("ble_prov", "provisioning end (BLE freed)");
    if (g_saw_got_ip || g_saw_cred_success) {
      g_saw_cred_success = true;
      g_saw_got_ip = g_saw_got_ip || (WiFi.status() == WL_CONNECTED);
      mark_success_if_ready();
    }
    break;
  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
    if (g_active->state == BLE_PROV_VALIDATING_STA) {
      LOG_W("ble_prov", "STA disconnected while validating");
    }
    break;
  default:
    break;
  }
}
#endif

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
  status->deadline_ms = now_ms + (uint32_t)NESSO_BLE_PROV_TIMEOUT_S * 1000U;
  status->state = BLE_PROV_ACTIVE;
  g_active = status;
  g_saw_cred_success = false;
  g_saw_got_ip = false;

#if NESSO_ENABLE_HW
  fill_pop(status->pop, sizeof(status->pop));

  if (!g_event_hooked) {
    WiFi.onEvent(on_sys_event);
    g_event_hooked = true;
  }

  wifi_sta_set_hold_off(true); /* no reconnect thrash during BLE session */

  const bool reset = g_reset_provisioned;
  g_reset_provisioned = false;
  if (reset) {
    wifi_erase_credentials();
  }

  /* New WiFiProvClass each session — required after FREE_BLE deinit. */
  prov_recycle();

  uint8_t uuid[16] = {0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf,
                      0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02};

  LOG_I("ble_prov", "WiFiProv BLE service=%s sec=1 reset=%d (PoP on display only)",
        status->service_name, (int)reset);

  /* Keep BLE up long enough for the phone app to receive success. */
  prov().disableAutoStop(5000);

  WiFi.begin(); /* no SSID — from app or NVS after reset */
  prov().beginProvision(NETWORK_PROV_SCHEME_BLE, NETWORK_PROV_SCHEME_HANDLER_FREE_BLE,
                        NETWORK_PROV_SECURITY_1, status->pop, status->service_name, nullptr, uuid,
                        true /* always reset mgr state for a clean BLE session */);
#else
  snprintf(status->pop, sizeof(status->pop), "POP_PENDING");
  LOG_W("ble_prov", "scaffold: BLE session for ESP BLE Provisioning app (sec=%u)",
        (unsigned)status->security);
#endif
  return true;
}

void ble_prov_poll(BleProvStatus *status, uint32_t now_ms) {
  if (status == nullptr || status->state == BLE_PROV_IDLE || status->state == BLE_PROV_SUCCESS) {
    return;
  }
  if (status->state == BLE_PROV_ACTIVE || status->state == BLE_PROV_VALIDATING_STA) {
    if (now_ms >= status->deadline_ms) {
      status->state = BLE_PROV_TIMEOUT;
      LOG_W("ble_prov", "timeout; wipe PoP from RAM");
      memset(status->pop, 0, sizeof(status->pop));
#if NESSO_ENABLE_HW
      if (g_prov != nullptr) {
        g_prov->endProvision();
      }
#endif
    }
  }
}

void ble_prov_abort(BleProvStatus *status) {
  if (status == nullptr) {
    return;
  }
  status->state = BLE_PROV_IDLE;
  memset(status->pop, 0, sizeof(status->pop));
#if NESSO_ENABLE_HW
  if (g_prov != nullptr) {
    g_prov->endProvision();
  }
#endif
  LOG_I("ble_prov", "aborted; caller should Zigbee resume");
}

bool ble_prov_is_active(const BleProvStatus *status) {
  if (status == nullptr) {
    return false;
  }
  return status->state == BLE_PROV_ACTIVE || status->state == BLE_PROV_VALIDATING_STA ||
         status->state == BLE_PROV_WAITING_ZIGBEE_PAUSE;
}
