#include "wifi_manager.h"
#include "app_config.h"
#include "log.h"
#include <stdio.h>
#include <string.h>

#if NESSO_ENABLE_HW
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_coexist.h>
#include <esp_wifi.h>
#endif

static bool g_ap = false;
static bool g_sta_started = false;
static bool g_mdns = false;
static bool g_coex_on = false;
static bool g_hold_off = false;
static uint32_t g_last_ensure_ms = 0;

bool wifi_sta_begin() {
#if NESSO_ENABLE_HW
  if (!g_coex_on) {
    /* Must be early for C6 Wi-Fi + 802.15.4 time-sharing. */
    const esp_err_t e = esp_coex_wifi_i154_enable();
    g_coex_on = (e == ESP_OK);
    LOG_I("wifi", "esp_coex_wifi_i154_enable → %d", (int)e);
    /* Keep balance — switching prefer at runtime coincided with device reboot during join. */
    esp_coex_preference_set(ESP_COEX_PREFER_BALANCE);
  }
  if (!g_sta_started) {
    WiFi.persistent(true);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false); /* coexistence is less painful with sleep off */
    WiFi.begin();
    g_sta_started = true;
  }
  LOG_I("wifi", "STA begin (never log password)");
  return true;
#else
  LOG_W("wifi", "scaffold STA");
  return false;
#endif
}

void wifi_sta_set_hold_off(bool hold_off) {
  g_hold_off = hold_off;
  /* Alleen reconnect onderdrukken. Geen esp_coex_preference_set hier:
   * PREFER_BT tijdens join leidde tot reboot (video: Join open → Even geduld). */
  LOG_I("wifi", "hold-off %s (Zigbee join)", hold_off ? "ON" : "OFF");
}

void wifi_sta_set_join_tx_low(bool low) {
#if NESSO_ENABLE_HW
  if (!g_sta_started) {
    return;
  }
  /* Default ~19.5 dBm; drop to 8.5 dBm while Zigbee permit-join is loud. */
  const wifi_power_t p = low ? WIFI_POWER_8_5dBm : WIFI_POWER_19_5dBm;
  if (WiFi.setTxPower(p)) {
    LOG_I("wifi", "TX power %s", low ? "8.5 dBm (join)" : "19.5 dBm");
  } else {
    LOG_W("wifi", "setTxPower failed");
  }
#else
  (void)low;
#endif
}

void wifi_sta_poll() {
#if NESSO_ENABLE_HW
  if (wifi_sta_connected()) {
    if (!g_mdns) {
      if (MDNS.begin(NESSO_MDNS_HOSTNAME)) {
        MDNS.setInstanceName("Qvantum installatie");
        MDNS.addService("http", "tcp", 80);
        g_mdns = true;
        LOG_I("wifi", "mDNS http://%s.local/", NESSO_MDNS_HOSTNAME);
      } else {
        LOG_W("wifi", "mDNS start mislukt");
      }
    }
  } else {
    if (g_mdns) {
      MDNS.end();
      g_mdns = false;
    }
    /* Soft ensure — never during Zigbee join (RF thrash kills pairing). */
    if (!g_hold_off && g_sta_started) {
      const uint32_t now = millis();
      if (now - g_last_ensure_ms > 8000UL) {
        g_last_ensure_ms = now;
        wifi_sta_ensure();
      }
    }
  }
#endif
}

bool wifi_sta_connected() {
#if NESSO_ENABLE_HW
  return WiFi.status() == WL_CONNECTED;
#else
  return false;
#endif
}

bool wifi_sta_ensure() {
#if NESSO_ENABLE_HW
  if (!g_sta_started) {
    return wifi_sta_begin();
  }
  if (wifi_sta_connected() || g_hold_off) {
    return wifi_sta_connected();
  }
  LOG_W("wifi", "ensure: esp_wifi_connect (no disconnect)");
  esp_wifi_connect();
  return false;
#else
  return false;
#endif
}

bool wifi_sta_reconnect() {
#if NESSO_ENABLE_HW
  if (g_hold_off) {
    LOG_W("wifi", "reconnect skipped (hold-off / Zigbee join)");
    return false;
  }
  if (!g_sta_started) {
    return wifi_sta_begin();
  }
  if (wifi_sta_connected()) {
    return true;
  }
  /* Prefer soft ensure; only fall back to begin() without erase. */
  LOG_W("wifi", "reconnect soft");
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin();
  return true;
#else
  return false;
#endif
}

bool wifi_sta_ip(char *out, size_t out_len) {
  if (out == nullptr || out_len == 0) {
    return false;
  }
  out[0] = '\0';
#if NESSO_ENABLE_HW
  if (!wifi_sta_connected()) {
    return false;
  }
  IPAddress ip = WiFi.localIP();
  snprintf(out, out_len, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  return true;
#else
  return false;
#endif
}

bool wifi_has_saved_network() {
#if NESSO_ENABLE_HW
  wifi_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  if (!g_sta_started) {
    WiFi.mode(WIFI_STA);
  }
  if (esp_wifi_get_config(WIFI_IF_STA, &cfg) != ESP_OK) {
    return false;
  }
  return cfg.sta.ssid[0] != 0;
#else
  return false;
#endif
}

void wifi_erase_credentials() {
#if NESSO_ENABLE_HW
  if (g_mdns) {
    MDNS.end();
    g_mdns = false;
  }
  WiFi.disconnect(true /* wifioff */, true /* eraseap */);
  WiFi.eraseAP();
  g_sta_started = false;
  LOG_I("wifi", "credentials erased");
#endif
}

bool wifi_mdns_ready() {
  return g_mdns;
}

bool wifi_mdns_hostname(char *out, size_t out_len) {
  if (out == nullptr || out_len == 0) {
    return false;
  }
  snprintf(out, out_len, "%s", NESSO_MDNS_HOSTNAME);
  return true;
}

bool wifi_start_service_ap_fallback() {
  LOG_W("wifi", "SoftAP fallback only; Zigbee must be paused");
  g_ap = false;
  return false;
}

void wifi_stop_service_ap() {
  g_ap = false;
}

bool wifi_service_ap_active() {
  return g_ap;
}
