#ifndef NESSO_DEVICE_REGISTRY_H
#define NESSO_DEVICE_REGISTRY_H

#include "types.h"

struct RegistrySlot {
  Eui64 eui;
  bool enabled;
  bool occupied;
  char label[16];
};

struct DeviceRegistry {
  uint16_t schema_version;
  RegistrySlot shelly[NESSO_MAX_SHELLY_SLOTS];
  RegistrySlot routers[NESSO_MAX_ROUTER_SLOTS];
};

void registry_init(DeviceRegistry *reg);
bool registry_enroll_shelly(DeviceRegistry *reg, const Eui64 *eui, const char *label);
bool registry_remove_shelly(DeviceRegistry *reg, const Eui64 *eui);
bool registry_enroll_router(DeviceRegistry *reg, const Eui64 *eui);
bool registry_remove_router(DeviceRegistry *reg, const Eui64 *eui);
bool registry_is_authorized_shelly(const DeviceRegistry *reg, const Eui64 *eui);
bool registry_is_router(const DeviceRegistry *reg, const Eui64 *eui);
int registry_shelly_count(const DeviceRegistry *reg);

#endif
