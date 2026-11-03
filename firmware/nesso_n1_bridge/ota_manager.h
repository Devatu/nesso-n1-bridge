#ifndef NESSO_OTA_MANAGER_H
#define NESSO_OTA_MANAGER_H

#include "ota_manifest.h"
#include <stdbool.h>
#include <stdint.h>

enum OtaState {
  OTA_IDLE = 0,
  OTA_CHECK_DUE,
  OTA_FETCH_MANIFEST,
  OTA_VALIDATE_MANIFEST,
  OTA_DOWNLOAD,
  OTA_VERIFY_IMAGE,
  OTA_REBOOT_PENDING,
  OTA_PENDING_VERIFY,
  OTA_COMMITTED,
  OTA_ROLLED_BACK,
  OTA_ERROR,
  OTA_OFFLINE_SKIP
};

struct OtaStatus {
  OtaState state;
  uint32_t last_check_ms;
  uint32_t next_check_ms;
  char last_error[48];
  char pending_version[24];
};

void ota_init(OtaStatus *st, uint32_t now_ms);
bool ota_check_now(OtaStatus *st);
void ota_poll(OtaStatus *st, uint32_t now_ms);
bool ota_boot_validate_or_rollback(OtaStatus *st, bool selftest_ok);

/* Host-testable transition helper after a parsed manifest. */
OtaState ota_after_manifest_policy(OtaStatus *st, OtaManifestError policy);

#endif
