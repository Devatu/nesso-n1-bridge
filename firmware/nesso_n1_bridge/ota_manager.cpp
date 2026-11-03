#include "ota_manager.h"
#include "app_config.h"
#include "log.h"
#include <stdio.h>
#include <string.h>

#if NESSO_ENABLE_HW
#include <esp_ota_ops.h>
#include <Update.h>
#endif

void ota_init(OtaStatus *st, uint32_t now_ms) {
  if (st == nullptr) {
    return;
  }
  memset(st, 0, sizeof(*st));
  st->state = OTA_IDLE;
  st->last_check_ms = 0;
  st->next_check_ms = now_ms; /* check soon after boot once STA is up */
}

OtaState ota_after_manifest_policy(OtaStatus *st, OtaManifestError policy) {
  if (st == nullptr) {
    return OTA_ERROR;
  }
  if (policy == OTA_MANIFEST_OK) {
    st->state = OTA_DOWNLOAD;
    return st->state;
  }
  snprintf(st->last_error, sizeof(st->last_error), "manifest_policy=%d", (int)policy);
  st->state = OTA_ERROR;
  return st->state;
}

bool ota_check_now(OtaStatus *st) {
  if (st == nullptr) {
    return false;
  }
  st->state = OTA_FETCH_MANIFEST;
  LOG_I("ota", "check start (HTTPS signed manifest; A/B inactive slot)");
#if NESSO_ENABLE_HW
  LOG_W("ota", "TODO: HTTPS GET manifest, ECDSA verify, Update to inactive partition");
#else
  /* Host/scaffold: no network. Mark offline skip so Zigbee/Modbus never blocks. */
  st->state = OTA_OFFLINE_SKIP;
  snprintf(st->last_error, sizeof(st->last_error), "offline_or_scaffold");
#endif
  return false;
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
    /* 24h + deterministic jitter placeholder (real RNG on HW). */
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
