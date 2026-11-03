#ifndef NESSO_OTA_MANIFEST_H
#define NESSO_OTA_MANIFEST_H

#include "app_config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

struct OtaManifest {
  uint32_t schema;
  char product[48];
  char board[32];
  char channel[16];
  char version[24];
  uint32_t security_version;
  char asset_url[192];
  char sha256_hex[65];
  uint32_t size_bytes;
  char signature_algorithm[16]; /* "ecdsa-p256" or "rsa" */
  char signature_b64[512];
};

enum OtaManifestError {
  OTA_MANIFEST_OK = 0,
  OTA_MANIFEST_PARSE,
  OTA_MANIFEST_SCHEMA,
  OTA_MANIFEST_PRODUCT,
  OTA_MANIFEST_BOARD,
  OTA_MANIFEST_CHANNEL,
  OTA_MANIFEST_SECURITY_DOWNGRADE,
  OTA_MANIFEST_URL,
  OTA_MANIFEST_HASH,
  OTA_MANIFEST_SIZE,
  OTA_MANIFEST_SIG_ALG,
  OTA_MANIFEST_SIGNATURE
};

/* Minimal field extractor for host tests; not a full JSON library. */
bool ota_manifest_extract_string(const char *json, const char *key, char *out, size_t out_len);
bool ota_manifest_extract_u32(const char *json, const char *key, uint32_t *out);

OtaManifestError ota_manifest_parse(const char *json, OtaManifest *out);
OtaManifestError ota_manifest_validate_policy(const OtaManifest *m, uint32_t running_security_version);
bool ota_sha256_hex_valid(const char *hex);

#endif
