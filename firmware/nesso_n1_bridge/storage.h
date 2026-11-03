#ifndef NESSO_STORAGE_H
#define NESSO_STORAGE_H

#include "device_registry.h"
#include "heatpump_config.h"

struct AppConfigStore {
  uint16_t schema_version;
  bool wifi_provisioned;
  HeatpumpConfig heatpump;
  DeviceRegistry registry;
};

void storage_init_defaults(AppConfigStore *store);
bool storage_load(AppConfigStore *store);
bool storage_save(const AppConfigStore *store);
bool storage_save_heatpump(AppConfigStore *store, const HeatpumpConfig *hp);

#endif
