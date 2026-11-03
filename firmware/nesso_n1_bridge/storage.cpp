#include "storage.h"
#include "log.h"
#include <string.h>

void storage_init_defaults(AppConfigStore *store) {
  if (store == nullptr) {
    return;
  }
  memset(store, 0, sizeof(*store));
  store->schema_version = NESSO_CONFIG_SCHEMA_VERSION;
  store->wifi_provisioned = false;
  heatpump_config_clear(&store->heatpump);
  registry_init(&store->registry);
}

bool storage_load(AppConfigStore *store) {
  storage_init_defaults(store);
  LOG_W("storage", "TODO: versioned NVS load/migration; RAM defaults until hardware");
  return true;
}

bool storage_save(const AppConfigStore *store) {
  if (store == nullptr) {
    return false;
  }
  LOG_W("storage", "TODO: atomic NVS write schema=%u wifi=%d hp=%d", store->schema_version,
        store->wifi_provisioned ? 1 : 0, store->heatpump.configured ? 1 : 0);
  return false;
}

bool storage_save_heatpump(AppConfigStore *store, const HeatpumpConfig *hp) {
  if (store == nullptr || hp == nullptr || !heatpump_config_is_ready(hp)) {
    return false;
  }
  store->heatpump = *hp;
  return storage_save(store);
}
