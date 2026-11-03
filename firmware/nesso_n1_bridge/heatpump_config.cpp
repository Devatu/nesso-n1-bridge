#include "heatpump_config.h"
#include <stdio.h>
#include <string.h>

void heatpump_config_clear(HeatpumpConfig *cfg) {
  if (cfg == nullptr) {
    return;
  }
  memset(cfg, 0, sizeof(*cfg));
  cfg->tcp_port = NESSO_HEATPUMP_PORT_DEFAULT;
  cfg->unit_id = NESSO_HEATPUMP_UNIT_DEFAULT;
}

bool heatpump_parse_ipv4(const char *text, uint8_t out_octets[4]) {
  if (text == nullptr || out_octets == nullptr) {
    return false;
  }
  unsigned a = 0, b = 0, c = 0, d = 0;
  char tail = 0;
  if (sscanf(text, "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail) != 4) {
    return false;
  }
  if (a > 255 || b > 255 || c > 255 || d > 255) {
    return false;
  }
  /* Reject unspecified / broadcast-ish portal mistakes; loopback allowed for dry-run labs. */
  if (a == 0 || a == 255) {
    return false;
  }
  out_octets[0] = (uint8_t)a;
  out_octets[1] = (uint8_t)b;
  out_octets[2] = (uint8_t)c;
  out_octets[3] = (uint8_t)d;
  return true;
}

HeatpumpConfigError heatpump_config_set(HeatpumpConfig *cfg, const char *ipv4, uint16_t tcp_port,
                                        uint8_t unit_id, const char *label) {
  if (cfg == nullptr) {
    return HP_ERR_NULL;
  }
  uint8_t octets[4];
  if (!heatpump_parse_ipv4(ipv4, octets)) {
    return HP_ERR_IPV4;
  }
  if (tcp_port == 0) {
    return HP_ERR_PORT;
  }
  if (unit_id > 247) {
    return HP_ERR_UNIT_ID;
  }
  if (label != nullptr && strlen(label) >= sizeof(cfg->label)) {
    return HP_ERR_LABEL;
  }

  snprintf(cfg->ipv4, sizeof(cfg->ipv4), "%u.%u.%u.%u", octets[0], octets[1], octets[2], octets[3]);
  cfg->tcp_port = tcp_port;
  cfg->unit_id = unit_id;
  memset(cfg->label, 0, sizeof(cfg->label));
  if (label != nullptr && label[0] != '\0') {
    strncpy(cfg->label, label, sizeof(cfg->label) - 1);
  }
  cfg->configured = true;
  return HP_OK;
}

bool heatpump_config_is_ready(const HeatpumpConfig *cfg) {
  return cfg != nullptr && cfg->configured && cfg->ipv4[0] != '\0' && cfg->tcp_port != 0 &&
         cfg->unit_id <= 247;
}
