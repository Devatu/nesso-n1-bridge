#include "storage.h"
#include "app_config.h"
#include "log.h"
#include <string.h>

#if NESSO_ENABLE_HW
#include <Preferences.h>
static const char *kNs = "nesso_cfg";
#endif

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
  if (store == nullptr) {
    return false;
  }

#if NESSO_ENABLE_HW
  Preferences prefs;
  if (!prefs.begin(kNs, true /* read-only */)) {
    LOG_W("storage", "NVS open failed; using defaults");
    return true;
  }

  const uint16_t schema = prefs.getUShort("schema", 0);
  if (schema == 0) {
    prefs.end();
    LOG_I("storage", "NVS empty");
    return true;
  }

  store->schema_version = schema;
  store->wifi_provisioned = prefs.getBool("wifi_ok", false);

  const bool hp_ok = prefs.getBool("hp_ok", false);
  if (hp_ok) {
    char ip[16];
    char label[24];
    memset(ip, 0, sizeof(ip));
    memset(label, 0, sizeof(label));
    prefs.getString("hp_ip", ip, sizeof(ip));
    prefs.getString("hp_label", label, sizeof(label));
    const uint16_t port = prefs.getUShort("hp_port", NESSO_HEATPUMP_PORT_DEFAULT);
    const uint8_t unit = prefs.getUChar("hp_unit", NESSO_HEATPUMP_UNIT_DEFAULT);
    if (heatpump_config_set(&store->heatpump, ip, port, unit, label) != HP_OK) {
      LOG_W("storage", "NVS heatpump invalid; cleared");
      heatpump_config_clear(&store->heatpump);
    } else {
      LOG_I("storage", "heatpump restored %s:%u unit=%u", store->heatpump.ipv4,
            (unsigned)store->heatpump.tcp_port, (unsigned)store->heatpump.unit_id);
    }
  }

  prefs.end();
  LOG_I("storage", "loaded schema=%u wifi=%d hp=%d", store->schema_version,
        store->wifi_provisioned ? 1 : 0, store->heatpump.configured ? 1 : 0);
  return true;
#else
  LOG_W("storage", "scaffold: no NVS (HOST)");
  return true;
#endif
}

bool storage_save(const AppConfigStore *store) {
  if (store == nullptr) {
    return false;
  }

#if NESSO_ENABLE_HW
  Preferences prefs;
  if (!prefs.begin(kNs, false)) {
    LOG_E("storage", "NVS open for write failed");
    return false;
  }

  prefs.putUShort("schema", store->schema_version);
  prefs.putBool("wifi_ok", store->wifi_provisioned);

  if (store->heatpump.configured && heatpump_config_is_ready(&store->heatpump)) {
    prefs.putBool("hp_ok", true);
    prefs.putString("hp_ip", store->heatpump.ipv4);
    prefs.putUShort("hp_port", store->heatpump.tcp_port);
    prefs.putUChar("hp_unit", store->heatpump.unit_id);
    prefs.putString("hp_label", store->heatpump.label);
  } else {
    prefs.putBool("hp_ok", false);
    prefs.remove("hp_ip");
    prefs.remove("hp_port");
    prefs.remove("hp_unit");
    prefs.remove("hp_label");
  }

  prefs.end();
  LOG_I("storage", "saved schema=%u wifi=%d hp=%d", store->schema_version,
        store->wifi_provisioned ? 1 : 0, store->heatpump.configured ? 1 : 0);
  return true;
#else
  LOG_W("storage", "scaffold save ignored");
  return false;
#endif
}

bool storage_save_heatpump(AppConfigStore *store, const HeatpumpConfig *hp) {
  if (store == nullptr || hp == nullptr || !heatpump_config_is_ready(hp)) {
    return false;
  }
  store->heatpump = *hp;
  store->schema_version = NESSO_CONFIG_SCHEMA_VERSION;
  return storage_save(store);
}

bool storage_factory_erase() {
#if NESSO_ENABLE_HW
  Preferences prefs;
  if (!prefs.begin(kNs, false)) {
    LOG_E("storage", "NVS erase: open failed");
    return false;
  }
  const bool ok = prefs.clear();
  prefs.end();
  LOG_W("storage", "factory erase NVS nesso_cfg ok=%d", ok ? 1 : 0);
  return ok;
#else
  return true;
#endif
}
