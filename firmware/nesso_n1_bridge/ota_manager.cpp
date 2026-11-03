#include "ota_manager.h"
#include "app_config.h"
#include "log.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if NESSO_ENABLE_HW
#include "wifi_manager.h"
#include <HTTPClient.h>
#include <NetworkClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>
#endif

static void copy_version(char *dst, size_t dst_len, const char *src) {
  if (dst == nullptr || dst_len == 0) {
    return;
  }
  if (src == nullptr) {
    dst[0] = '\0';
    return;
  }
  snprintf(dst, dst_len, "%s", src);
}

static bool parse_semver_core(const char *s, unsigned *maj, unsigned *min, unsigned *pat) {
  if (s == nullptr || maj == nullptr || min == nullptr || pat == nullptr) {
    return false;
  }
  *maj = *min = *pat = 0;
  if (!isdigit((unsigned char)*s)) {
    return false;
  }
  char *end = nullptr;
  unsigned long a = strtoul(s, &end, 10);
  if (end == s || *end != '.') {
    return false;
  }
  s = end + 1;
  unsigned long b = strtoul(s, &end, 10);
  if (end == s || *end != '.') {
    return false;
  }
  s = end + 1;
  unsigned long c = strtoul(s, &end, 10);
  if (end == s) {
    return false;
  }
  *maj = (unsigned)a;
  *min = (unsigned)b;
  *pat = (unsigned)c;
  return true;
}

int ota_version_cmp(const char *a, const char *b) {
  unsigned am = 0, an = 0, ap = 0, bm = 0, bn = 0, bp = 0;
  bool ok_a = parse_semver_core(a, &am, &an, &ap);
  bool ok_b = parse_semver_core(b, &bm, &bn, &bp);
  if (ok_a && ok_b) {
    if (am != bm) {
      return (am < bm) ? -1 : 1;
    }
    if (an != bn) {
      return (an < bn) ? -1 : 1;
    }
    if (ap != bp) {
      return (ap < bp) ? -1 : 1;
    }
    return 0;
  }
  if (a == nullptr && b == nullptr) {
    return 0;
  }
  if (a == nullptr) {
    return -1;
  }
  if (b == nullptr) {
    return 1;
  }
  return strcmp(a, b);
}

bool ota_remote_is_newer(const OtaManifest *remote, const char *running_version,
                         uint32_t running_security_version) {
  if (remote == nullptr || running_version == nullptr) {
    return false;
  }
  if (remote->security_version > running_security_version) {
    return true;
  }
  return ota_version_cmp(remote->version, running_version) > 0;
}

void ota_init(OtaStatus *st, uint32_t now_ms) {
  if (st == nullptr) {
    return;
  }
  memset(st, 0, sizeof(*st));
  st->state = OTA_IDLE;
  st->availability = OTA_AVAIL_UNKNOWN;
  st->last_check_ms = 0;
  /* Niet meteen na boot: HTTPS-check blokkeert anders UI/portal/knoppen. */
  st->next_check_ms = now_ms + 6UL * 3600UL * 1000UL;
  st->auto_apply = true;
  copy_version(st->current_version, sizeof(st->current_version), NESSO_FW_VERSION);
}

OtaState ota_after_manifest_policy(OtaStatus *st, OtaManifestError policy) {
  if (st == nullptr) {
    return OTA_ERROR;
  }
  if (policy == OTA_MANIFEST_OK) {
    st->state = OTA_VALIDATE_MANIFEST;
    return st->state;
  }
  snprintf(st->last_error, sizeof(st->last_error), "manifest_policy=%d", (int)policy);
  st->state = OTA_ERROR;
  st->availability = OTA_AVAIL_ERROR;
  return st->state;
}

bool ota_ingest_manifest_json(OtaStatus *st, const char *json) {
  if (st == nullptr) {
    return false;
  }
  OtaManifest m;
  OtaManifestError pe = ota_manifest_parse(json, &m);
  if (pe != OTA_MANIFEST_OK) {
    snprintf(st->last_error, sizeof(st->last_error), "parse=%d", (int)pe);
    st->state = OTA_ERROR;
    st->availability = OTA_AVAIL_ERROR;
    return false;
  }
  OtaManifestError pol = ota_manifest_validate_policy(&m, NESSO_SECURITY_VERSION);
  if (ota_after_manifest_policy(st, pol) == OTA_ERROR) {
    return false;
  }

  copy_version(st->remote_version, sizeof(st->remote_version), m.version);
  copy_version(st->pending_version, sizeof(st->pending_version), m.version);
  snprintf(st->pending_asset_url, sizeof(st->pending_asset_url), "%s", m.asset_url);
  snprintf(st->pending_sha256_hex, sizeof(st->pending_sha256_hex), "%s", m.sha256_hex);
  st->pending_size_bytes = m.size_bytes;

  if (!ota_remote_is_newer(&m, st->current_version, NESSO_SECURITY_VERSION)) {
    st->availability = OTA_AVAIL_UP_TO_DATE;
    st->state = OTA_IDLE;
    st->last_error[0] = '\0';
    LOG_I("ota", "up to date running=%s remote=%s", st->current_version, st->remote_version);
    return true;
  }

  st->availability = OTA_AVAIL_UPDATE_AVAILABLE;
  st->state = OTA_UPDATE_AVAILABLE;
  LOG_I("ota", "update available running=%s remote=%s", st->current_version, st->remote_version);

  if (st->auto_apply) {
    return ota_apply_available(st);
  }
  return true;
}

#if NESSO_ENABLE_HW
static bool hex_nibble(char c, uint8_t *out) {
  if (c >= '0' && c <= '9') {
    *out = (uint8_t)(c - '0');
    return true;
  }
  if (c >= 'a' && c <= 'f') {
    *out = (uint8_t)(c - 'a' + 10);
    return true;
  }
  if (c >= 'A' && c <= 'F') {
    *out = (uint8_t)(c - 'A' + 10);
    return true;
  }
  return false;
}

static bool sha256_hex_equals(const uint8_t digest[32], const char *hex) {
  if (hex == nullptr || strlen(hex) != 64) {
    return false;
  }
  for (int i = 0; i < 32; ++i) {
    uint8_t hi = 0, lo = 0;
    if (!hex_nibble(hex[i * 2], &hi) || !hex_nibble(hex[i * 2 + 1], &lo)) {
      return false;
    }
    if (digest[i] != (uint8_t)((hi << 4) | lo)) {
      return false;
    }
  }
  return true;
}

static bool download_and_flash_https(OtaStatus *st) {
  if (st->pending_asset_url[0] == '\0' || st->pending_size_bytes == 0) {
    snprintf(st->last_error, sizeof(st->last_error), "no_asset");
    return false;
  }
  if (!wifi_sta_connected()) {
    snprintf(st->last_error, sizeof(st->last_error), "geen_wifi");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure(); /* tijdelijk; later CA-pinning */
  client.setTimeout(30);
  client.setHandshakeTimeout(20);
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(60000);
  http.setConnectTimeout(15000);
  LOG_I("ota", "GET asset %s (%u B)", st->pending_asset_url, (unsigned)st->pending_size_bytes);
  if (!http.begin(client, st->pending_asset_url)) {
    snprintf(st->last_error, sizeof(st->last_error), "asset_begin");
    return false;
  }
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    snprintf(st->last_error, sizeof(st->last_error), "asset_http_%d", code);
    http.end();
    return false;
  }

  const int content_len = http.getSize();
  const size_t expect = st->pending_size_bytes;
  if (content_len > 0 && (size_t)content_len != expect) {
    LOG_W("ota", "Content-Length %d != manifest %u (doorgaan op manifest)", content_len,
          (unsigned)expect);
  }

  if (!Update.begin(expect, U_FLASH)) {
    snprintf(st->last_error, sizeof(st->last_error), "update_begin");
    http.end();
    return false;
  }

  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);

  NetworkClient *stream = http.getStreamPtr();
  if (stream == nullptr) {
    snprintf(st->last_error, sizeof(st->last_error), "no_stream");
    Update.abort();
    mbedtls_sha256_free(&sha);
    http.end();
    return false;
  }
  uint8_t buf[1024];
  size_t written = 0;
  const uint32_t t0 = millis();
  while (written < expect && http.connected() && (millis() - t0) < 180000U) {
    const size_t avail = stream->available();
    if (avail == 0) {
      delay(10);
      continue;
    }
    size_t n = avail;
    if (n > sizeof(buf)) {
      n = sizeof(buf);
    }
    if (n > expect - written) {
      n = expect - written;
    }
    const int rd = stream->readBytes(buf, n);
    if (rd <= 0) {
      break;
    }
    mbedtls_sha256_update(&sha, buf, (size_t)rd);
    if (Update.write(buf, (size_t)rd) != (size_t)rd) {
      snprintf(st->last_error, sizeof(st->last_error), "update_write");
      Update.abort();
      mbedtls_sha256_free(&sha);
      http.end();
      return false;
    }
    written += (size_t)rd;
  }
  http.end();

  uint8_t digest[32];
  mbedtls_sha256_finish(&sha, digest);
  mbedtls_sha256_free(&sha);

  if (written != expect) {
    snprintf(st->last_error, sizeof(st->last_error), "size_%u/%u", (unsigned)written, (unsigned)expect);
    Update.abort();
    return false;
  }
  if (!sha256_hex_equals(digest, st->pending_sha256_hex)) {
    snprintf(st->last_error, sizeof(st->last_error), "sha256_mismatch");
    Update.abort();
    return false;
  }
  /* ECDSA nog niet op device; signature-veld is wel verplicht in manifest-policy. */
  LOG_W("ota", "SHA-256 OK; ECDSA verify nog scaffold");

  if (!Update.end(true)) {
    snprintf(st->last_error, sizeof(st->last_error), "update_end");
    return false;
  }
  st->state = OTA_REBOOT_PENDING;
  LOG_I("ota", "flash OK → reboot to %s", st->pending_version);
  delay(500);
  ESP.restart();
  return true;
}
#endif

bool ota_apply_available(OtaStatus *st) {
  if (st == nullptr) {
    return false;
  }
  if (st->availability != OTA_AVAIL_UPDATE_AVAILABLE && st->state != OTA_UPDATE_AVAILABLE) {
    snprintf(st->last_error, sizeof(st->last_error), "nothing_to_apply");
    return false;
  }
  st->state = OTA_DOWNLOAD;
  LOG_I("ota", "apply start version=%s (A/B inactive slot)", st->pending_version);
#if NESSO_ENABLE_HW
  if (!download_and_flash_https(st)) {
    st->state = OTA_ERROR;
    st->availability = OTA_AVAIL_ERROR;
    LOG_W("ota", "apply failed: %s", st->last_error);
    return false;
  }
  return true;
#else
  snprintf(st->last_error, sizeof(st->last_error), "apply_scaffold_only");
  st->state = OTA_UPDATE_AVAILABLE;
  return true;
#endif
}

#if NESSO_ENABLE_HW
static bool fetch_manifest_https(char *buf, size_t buflen, char *err, size_t errlen) {
  if (buf == nullptr || buflen < 32) {
    return false;
  }
  buf[0] = '\0';
  if (!wifi_sta_connected()) {
    snprintf(err, errlen, "geen_wifi");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure(); /* tijdelijk; later CA-pinning */
  /* Zonder dit kan TLS-handshake oneindig blijven hangen → UI "Bezig…". */
  client.setTimeout(12);
  client.setHandshakeTimeout(12);
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(12000);
  http.setConnectTimeout(12000);
  http.setReuse(false);
  if (!http.begin(client, NESSO_OTA_MANIFEST_URL)) {
    snprintf(err, errlen, "http_begin");
    return false;
  }
  http.addHeader("Cache-Control", "no-cache");
  http.addHeader("Pragma", "no-cache");

  const int code = http.GET();
  if (code <= 0) {
    snprintf(err, errlen, "http_timeout");
    http.end();
    return false;
  }
  if (code != HTTP_CODE_OK) {
    snprintf(err, errlen, "http_%d", code);
    http.end();
    return false;
  }

  const String body = http.getString();
  http.end();
  if (body.length() == 0 || (size_t)body.length() + 1 > buflen) {
    snprintf(err, errlen, "body_bad");
    return false;
  }
  memcpy(buf, body.c_str(), body.length() + 1);
  return true;
}
#endif

static bool run_check(OtaStatus *st, bool auto_apply) {
  if (st == nullptr) {
    return false;
  }
  st->auto_apply = auto_apply;
  st->availability = OTA_AVAIL_CHECKING;
  st->state = OTA_FETCH_MANIFEST;
  st->last_error[0] = '\0';
  LOG_I("ota", "check start channel=%s auto_apply=%d url=%s", NESSO_OTA_CHANNEL, (int)auto_apply,
        NESSO_OTA_MANIFEST_URL);

#if NESSO_ENABLE_HW
  char json[1536];
  char err[32];
  err[0] = '\0';
  bool ok = false;
  if (!fetch_manifest_https(json, sizeof(json), err, sizeof(err))) {
    const bool offline = (strcmp(err, "geen_wifi") == 0);
    st->state = offline ? OTA_OFFLINE_SKIP : OTA_ERROR;
    st->availability = offline ? OTA_AVAIL_OFFLINE : OTA_AVAIL_ERROR;
    snprintf(st->last_error, sizeof(st->last_error), "%s", err[0] ? err : "fetch_fail");
    LOG_W("ota", "check failed: %s", st->last_error);
    ok = false;
  } else {
    ok = ota_ingest_manifest_json(st, json);
  }
  /* Nooit CHECKING achterlaten — UI mag niet eeuwig "Bezig…" tonen. */
  if (st->availability == OTA_AVAIL_CHECKING) {
    st->state = OTA_ERROR;
    st->availability = OTA_AVAIL_ERROR;
    if (st->last_error[0] == '\0') {
      snprintf(st->last_error, sizeof(st->last_error), "check_stuck");
    }
  }
  return ok;
#else
  st->state = OTA_OFFLINE_SKIP;
  st->availability = OTA_AVAIL_OFFLINE;
  snprintf(st->last_error, sizeof(st->last_error), "offline_or_scaffold");
  return false;
#endif
}

bool ota_check_now(OtaStatus *st) {
  return run_check(st, true);
}

bool ota_check_for_ui(OtaStatus *st) {
  return run_check(st, false);
}

void ota_poll(OtaStatus *st, uint32_t now_ms) {
  if (st == nullptr) {
    return;
  }
  if (st->state == OTA_IDLE && now_ms >= st->next_check_ms) {
    st->state = OTA_CHECK_DUE;
  }
  if (st->state == OTA_CHECK_DUE) {
    ota_check_now(st);
    st->last_check_ms = now_ms;
    uint32_t jitter_ms = (now_ms % (NESSO_OTA_JITTER_MAX_S + 1U)) * 1000U;
    st->next_check_ms = now_ms + (uint32_t)NESSO_OTA_CHECK_PERIOD_S * 1000U + jitter_ms;
    if (st->state == OTA_OFFLINE_SKIP || st->state == OTA_ERROR) {
      st->state = OTA_IDLE;
    }
  }
}

bool ota_boot_validate_or_rollback(OtaStatus *st, bool selftest_ok) {
  if (st == nullptr) {
    return false;
  }
#if NESSO_ENABLE_HW
  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;
  if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK &&
      ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
    st->state = OTA_PENDING_VERIFY;
    if (selftest_ok) {
      esp_ota_mark_app_valid_cancel_rollback();
      st->state = OTA_COMMITTED;
      LOG_I("ota", "A/B commit OK");
      return true;
    }
    LOG_E("ota", "selftest failed; rolling back");
    st->state = OTA_ROLLED_BACK;
    esp_ota_mark_app_invalid_rollback_and_reboot();
    return false;
  }
#else
  (void)selftest_ok;
  st->state = OTA_COMMITTED;
#endif
  return true;
}

static const char *avail_str(OtaAvailability a) {
  switch (a) {
  case OTA_AVAIL_CHECKING:
    return "checking";
  case OTA_AVAIL_UP_TO_DATE:
    return "up_to_date";
  case OTA_AVAIL_UPDATE_AVAILABLE:
    return "update_available";
  case OTA_AVAIL_OFFLINE:
    return "offline";
  case OTA_AVAIL_ERROR:
    return "error";
  default:
    return "unknown";
  }
}

size_t ota_format_status_json(const OtaStatus *st, char *out, size_t out_len) {
  if (st == nullptr || out == nullptr || out_len == 0) {
    return 0;
  }
  return (size_t)snprintf(out, out_len,
                          "{\"current\":\"%s\",\"remote\":\"%s\",\"availability\":\"%s\","
                          "\"update_available\":%s,\"channel\":\"%s\",\"last_error\":\"%s\"}",
                          st->current_version, st->remote_version, avail_str(st->availability),
                          (st->availability == OTA_AVAIL_UPDATE_AVAILABLE) ? "true" : "false",
                          NESSO_OTA_CHANNEL, st->last_error);
}
