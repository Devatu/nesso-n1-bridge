#include "portal_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool eq_ci_prefix(const char *s, const char *prefix) {
  while (*prefix) {
    char a = *s++;
    char b = *prefix++;
    if (a >= 'A' && a <= 'Z') {
      a = (char)(a - 'A' + 'a');
    }
    if (b >= 'A' && b <= 'Z') {
      b = (char)(b - 'A' + 'a');
    }
    if (a != b) {
      return false;
    }
  }
  return true;
}

bool portal_path_is_register_editor(const char *path) {
  if (path == nullptr) {
    return false;
  }
  return strstr(path, "/api/registers") != nullptr || strstr(path, "/api/modbus/write") != nullptr ||
         strstr(path, "/register") != nullptr;
}

PortalRequestKind portal_classify_path(const char *method, const char *path) {
  if (method == nullptr || path == nullptr) {
    return PORTAL_REQ_UNKNOWN;
  }
  if (portal_path_is_register_editor(path)) {
    return PORTAL_REQ_FORBIDDEN_REGISTERS;
  }
  if (eq_ci_prefix(method, "GET") && strcmp(path, "/api/status") == 0) {
    return PORTAL_REQ_GET_STATUS;
  }
  if (eq_ci_prefix(method, "GET") && strcmp(path, "/api/heatpump") == 0) {
    return PORTAL_REQ_GET_HEATPUMP;
  }
  if (eq_ci_prefix(method, "PUT") && strcmp(path, "/api/heatpump") == 0) {
    return PORTAL_REQ_PUT_HEATPUMP;
  }
  if (eq_ci_prefix(method, "POST") && strcmp(path, "/api/heatpump") == 0) {
    return PORTAL_REQ_PUT_HEATPUMP;
  }
  return PORTAL_REQ_UNKNOWN;
}

static bool form_get(const char *body, const char *key, char *out, size_t out_len) {
  if (body == nullptr || key == nullptr || out == nullptr || out_len == 0) {
    return false;
  }
  out[0] = '\0';
  const char *p = strstr(body, key);
  if (p == nullptr) {
    return false;
  }
  p += strlen(key);
  if (*p != '=') {
    return false;
  }
  ++p;
  size_t i = 0;
  while (*p && *p != '&' && i + 1 < out_len) {
    out[i++] = *p++;
  }
  out[i] = '\0';
  return i > 0;
}

bool portal_parse_heatpump_form(const char *body, PortalHeatpumpPut *out) {
  if (out == nullptr) {
    return false;
  }
  memset(out, 0, sizeof(*out));
  char port_s[8];
  char unit_s[8];
  if (!form_get(body, "ipv4", out->ipv4, sizeof(out->ipv4))) {
    return false;
  }
  if (!form_get(body, "port", port_s, sizeof(port_s))) {
    return false;
  }
  if (!form_get(body, "unit_id", unit_s, sizeof(unit_s))) {
    return false;
  }
  form_get(body, "label", out->label, sizeof(out->label));
  unsigned long port = strtoul(port_s, nullptr, 10);
  unsigned long unit = strtoul(unit_s, nullptr, 10);
  if (port == 0 || port > 65535 || unit > 247) {
    return false;
  }
  out->tcp_port = (uint16_t)port;
  out->unit_id = (uint8_t)unit;
  return true;
}

int portal_apply_heatpump_put(HeatpumpConfig *cfg, const PortalHeatpumpPut *put) {
  if (cfg == nullptr || put == nullptr) {
    return (int)HP_ERR_NULL;
  }
  return (int)heatpump_config_set(cfg, put->ipv4, put->tcp_port, put->unit_id, put->label);
}

size_t portal_format_heatpump_json(const HeatpumpConfig *cfg, char *out, size_t out_len) {
  if (cfg == nullptr || out == nullptr || out_len == 0) {
    return 0;
  }
  return (size_t)snprintf(out, out_len,
                          "{\"configured\":%s,\"ipv4\":\"%s\",\"tcp_port\":%u,\"unit_id\":%u,\"label\":\"%s\"}",
                          cfg->configured ? "true" : "false", cfg->ipv4, cfg->tcp_port, cfg->unit_id,
                          cfg->label);
}
