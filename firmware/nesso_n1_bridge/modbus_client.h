#ifndef NESSO_MODBUS_CLIENT_H
#define NESSO_MODBUS_CLIENT_H

#include "action_table.h"
#include "heatpump_config.h"

/* Target host from NVS heatpump config; registers stay compile-time allowlist. */
bool modbus_set_target(const HeatpumpConfig *cfg);
bool modbus_enqueue(const ActionSpec *spec);
void modbus_poll();

#endif
