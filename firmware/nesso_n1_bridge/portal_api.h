#ifndef NESSO_PORTAL_API_H
#define NESSO_PORTAL_API_H

#include "heatpump_config.h"
#include "ota_manager.h"
#include <stdbool.h>
#include <stddef.h>

/*
 * Local LAN portal API helpers (no cloud).
 * Allowed: heatpump IP / port / unit ID, status, OTA check/apply.
 * Forbidden: free Modbus register/value edits; permanent OTA disable.
 */

enum PortalRequestKind {
  PORTAL_REQ_UNKNOWN = 0,
  PORTAL_REQ_GET_STATUS,
  PORTAL_REQ_GET_HEATPUMP,
  PORTAL_REQ_PUT_HEATPUMP,
  PORTAL_REQ_GET_OTA,
  PORTAL_REQ_POST_OTA_CHECK,
  PORTAL_REQ_POST_OTA_APPLY,
  PORTAL_REQ_FORBIDDEN_REGISTERS
};

struct PortalHeatpumpPut {
  char ipv4[16];
  uint16_t tcp_port;
  uint8_t unit_id;
  char label[24];
};

PortalRequestKind portal_classify_path(const char *method, const char *path);
bool portal_parse_heatpump_form(const char *body, PortalHeatpumpPut *out);
int portal_apply_heatpump_put(HeatpumpConfig *cfg, const PortalHeatpumpPut *put);
size_t portal_format_heatpump_json(const HeatpumpConfig *cfg, char *out, size_t out_len);
size_t portal_format_ota_json(const OtaStatus *ota, char *out, size_t out_len);
bool portal_path_is_register_editor(const char *path);

#endif
