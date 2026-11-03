#ifndef NESSO_HEATPUMP_CONFIG_H
#define NESSO_HEATPUMP_CONFIG_H

#include "app_config.h"
#include <stdbool.h>
#include <stdint.h>

struct HeatpumpConfig {
  char ipv4[16]; /* "255.255.255.255" + NUL */
  uint16_t tcp_port;
  uint8_t unit_id; /* Modbus unit / slave 0..247 */
  char label[24];
  bool configured;
};

enum HeatpumpConfigError {
  HP_OK = 0,
  HP_ERR_NULL,
  HP_ERR_IPV4,
  HP_ERR_PORT,
  HP_ERR_UNIT_ID,
  HP_ERR_LABEL
};

void heatpump_config_clear(HeatpumpConfig *cfg);
HeatpumpConfigError heatpump_config_set(HeatpumpConfig *cfg, const char *ipv4, uint16_t tcp_port,
                                        uint8_t unit_id, const char *label);
bool heatpump_config_is_ready(const HeatpumpConfig *cfg);
bool heatpump_parse_ipv4(const char *text, uint8_t out_octets[4]);

#endif
