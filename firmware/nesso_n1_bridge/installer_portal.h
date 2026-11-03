#ifndef NESSO_INSTALLER_PORTAL_H
#define NESSO_INSTALLER_PORTAL_H

#include "heatpump_config.h"
#include <stdbool.h>

/*
 * Local LAN HTTP portal after Wi-Fi STA is up.
 * Not a cloud dashboard. No inbound WAN assumption.
 * Can set heatpump IP/port/unit ID. Cannot edit Modbus registers.
 */
bool portal_begin(HeatpumpConfig *heatpump);
void portal_poll();
void portal_stop();
bool portal_is_running();
HeatpumpConfig *portal_heatpump();

#endif
