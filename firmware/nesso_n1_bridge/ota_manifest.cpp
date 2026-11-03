#include "ota_manifest.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *find_key(const char *json, const char *key) {
  if (json == nullptr || key == nullptr) {
    return nullptr;
  }
  char pattern[64];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  const char *p = strstr(json, pattern);
  if (p == nullptr) {
    return nullptr;
  }
  p += strlen(pattern);
  while (*p && (isspace((unsigned char)*p) || *p == ':')) {
    ++p;
  }
  return p;
}

bool ota_manifest_extract_string(const char *json, const char *key, char *out, size_t out_len) {
  if (out == nullptr || out_len == 0) {
    return false;
  }
  out[0] = '\0';
  const char *p = find_key(json, key);
  if (p == nullptr || *p != '"') {
    return false;
  }
  ++p;
  size_t i = 0;
  while (*p && *p != '"' && i + 1 < out_len) {
    if (*p == '\\' && p[1] != '\0') {
      ++p;
    }
    out[i++] = *p++;
  }
  if (*p != '"') {
    out[0] = '\0';
    return false;
  }
  out[i] = '\0';
  return true;
}

bool ota_manifest_extract_u32(const char *json, const char *key, uint32_t *out) {
  if (out == nullptr) {
    return false;
  }
  const char *p = find_key(json, key);
  if (p == nullptr || !isdigit((unsigned char)*p)) {
    return false;
  }
  char *end = nullptr;
  unsigned long v = strtoul(p, &end, 10);
  if (end == p) {
    return false;
  }
  *out = (uint32_t)v;
  return true;
}

bool ota_sha256_hex_valid(const char *hex) {
  if (hex == nullptr || strlen(hex) != 64) {
    return false;
  }
  for (int i = 0; i < 64; ++i) {
    if (!isxdigit((unsigned char)hex[i])) {
      return false;
    }
  }
  return true;
}

OtaManifestError ota_manifest_parse(const char *json, OtaManifest *out) {
  if (json == nullptr || out == nullptr) {
    return OTA_MANIFEST_PARSE;
  }
  memset(out, 0, sizeof(*out));
  if (!ota_manifest_extract_u32(json, "schema", &out->schema) ||
      !ota_manifest_extract_string(json, "product", out->product, sizeof(out->product)) ||
      !ota_manifest_extract_string(json, "board", out->board, sizeof(out->board)) ||
      !ota_manifest_extract_string(json, "channel", out->channel, sizeof(out->channel)) ||
      !ota_manifest_extract_string(json, "version", out->version, sizeof(out->version)) ||
      !ota_manifest_extract_u32(json, "security_version", &out->security_version) ||
      !ota_manifest_extract_string(json, "asset_url", out->asset_url, sizeof(out->asset_url)) ||
      !ota_manifest_extract_string(json, "sha256", out->sha256_hex, sizeof(out->sha256_hex)) ||
      !ota_manifest_extract_u32(json, "size_bytes", &out->size_bytes) ||
      !ota_manifest_extract_string(json, "signature_algorithm", out->signature_algorithm,
                                   sizeof(out->signature_algorithm)) ||
      !ota_manifest_extract_string(json, "signature", out->signature_b64, sizeof(out->signature_b64))) {
    return OTA_MANIFEST_PARSE;
  }
  return OTA_MANIFEST_OK;
}

OtaManifestError ota_manifest_validate_policy(const OtaManifest *m, uint32_t running_security_version) {
  if (m == nullptr) {
    return OTA_MANIFEST_PARSE;
  }
  if (m->schema != 1) {
    return OTA_MANIFEST_SCHEMA;
  }
  if (strcmp(m->product, NESSO_PRODUCT_ID) != 0) {
    return OTA_MANIFEST_PRODUCT;
  }
  if (strcmp(m->board, NESSO_BOARD_ID) != 0) {
    return OTA_MANIFEST_BOARD;
  }
  if (strcmp(m->channel, NESSO_OTA_CHANNEL) != 0) {
    return OTA_MANIFEST_CHANNEL;
  }
  if (m->security_version < running_security_version) {
    return OTA_MANIFEST_SECURITY_DOWNGRADE;
  }
  if (strncmp(m->asset_url, "https://", 8) != 0) {
    return OTA_MANIFEST_URL;
  }
  if (!ota_sha256_hex_valid(m->sha256_hex)) {
    return OTA_MANIFEST_HASH;
  }
  if (m->size_bytes == 0) {
    return OTA_MANIFEST_SIZE;
  }
  if (strcmp(m->signature_algorithm, "ecdsa-p256") != 0 &&
      strcmp(m->signature_algorithm, "rsa") != 0) {
    return OTA_MANIFEST_SIG_ALG;
  }
  if (m->signature_b64[0] == '\0') {
    return OTA_MANIFEST_SIGNATURE;
  }
  return OTA_MANIFEST_OK;
}
