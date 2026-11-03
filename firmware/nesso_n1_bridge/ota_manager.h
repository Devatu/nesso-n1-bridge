#ifndef NESSO_OTA_MANAGER_H
#define NESSO_OTA_MANAGER_H

#include "ota_manifest.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum OtaState {
  OTA_IDLE = 0,
  OTA_CHECK_DUE,
  OTA_FETCH_MANIFEST,
  OTA_VALIDATE_MANIFEST,
  OTA_UPDATE_AVAILABLE, /* check done; waiting for apply (UI or auto) */
  OTA_DOWNLOAD,
  OTA_VERIFY_IMAGE,
  OTA_REBOOT_PENDING,
  OTA_PENDING_VERIFY,
  OTA_COMMITTED,
  OTA_ROLLED_BACK,
  OTA_ERROR,
  OTA_OFFLINE_SKIP
};

enum OtaAvailability {
  OTA_AVAIL_UNKNOWN = 0,
  OTA_AVAIL_CHECKING,
  OTA_AVAIL_UP_TO_DATE,
  OTA_AVAIL_UPDATE_AVAILABLE,
  OTA_AVAIL_OFFLINE,
  OTA_AVAIL_ERROR
};

struct OtaStatus {
  OtaState state;
  OtaAvailability availability;
  uint32_t last_check_ms;
  uint32_t next_check_ms;
  char current_version[24];
  char remote_version[24];
  char pending_version[24];
  char pending_asset_url[192];
  char pending_sha256_hex[65];
  uint32_t pending_size_bytes;
  char last_error[48];
  bool auto_apply; /* true for scheduled checks; false for UI "alleen controleren" */
};

void ota_init(OtaStatus *st, uint32_t now_ms);

/* Semver-achtig: major.minor.patch; suffix na '-' negeren voor numeric cmp.
 * <0 als a<b, 0 gelijk (numeric), >0 als a>b. */
int ota_version_cmp(const char *a, const char *b);

/* True als remote nieuwer is (version of security_version). */
bool ota_remote_is_newer(const OtaManifest *remote, const char *running_version,
                         uint32_t running_security_version);

/* Parse+policy+vergelijk; zet availability. Download alleen als auto_apply. */
bool ota_ingest_manifest_json(OtaStatus *st, const char *json);

/* Handmatige/periodieke check: fetch (HW) of scaffold. */
bool ota_check_now(OtaStatus *st);

/* UI/portal: check zonder direct downloaden. */
bool ota_check_for_ui(OtaStatus *st);

/* Start download naar inactive slot als UPDATE_AVAILABLE. */
bool ota_apply_available(OtaStatus *st);

void ota_poll(OtaStatus *st, uint32_t now_ms);
bool ota_boot_validate_or_rollback(OtaStatus *st, bool selftest_ok);

size_t ota_format_status_json(const OtaStatus *st, char *out, size_t out_len);

/* Legacy helper: policy-fout → ERROR; OK alleen betekent "mag evalueren". */
OtaState ota_after_manifest_policy(OtaStatus *st, OtaManifestError policy);

#endif
