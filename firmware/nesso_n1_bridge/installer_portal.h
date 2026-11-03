#ifndef NESSO_INSTALLER_PORTAL_H
#define NESSO_INSTALLER_PORTAL_H

#include "heatpump_config.h"
#include "ota_manager.h"
#include "storage.h"
#include <stdbool.h>

/*
 * Local LAN installatiepagina (geen cloud).
 * Warmtepomp IP/poort/unit + OTA-check. Geen Modbus-registereditor.
 */
bool portal_begin(HeatpumpConfig *heatpump, OtaStatus *ota);
bool portal_begin_with_store(HeatpumpConfig *heatpump, OtaStatus *ota, AppConfigStore *store);
void portal_poll();
void portal_stop();
bool portal_is_running();
HeatpumpConfig *portal_heatpump();
OtaStatus *portal_ota();

#endif
