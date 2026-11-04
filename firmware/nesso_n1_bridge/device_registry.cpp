#include "device_registry.h"
#include <string.h>

static bool enroll(RegistrySlot *slots, int max_slots, const Eui64 *eui, const char *label) {
  if (eui == nullptr || eui64_is_zero(eui)) {
    return false;
  }
  for (int i = 0; i < max_slots; ++i) {
    if (slots[i].occupied && eui64_equal(&slots[i].eui, eui)) {
      return false;
    }
  }
  for (int i = 0; i < max_slots; ++i) {
    if (!slots[i].occupied) {
      eui64_copy(&slots[i].eui, eui);
      slots[i].occupied = true;
      slots[i].enabled = true;
      memset(slots[i].label, 0, sizeof(slots[i].label));
      if (label != nullptr) {
        strncpy(slots[i].label, label, sizeof(slots[i].label) - 1);
      }
      return true;
    }
  }
  return false;
}

static bool remove_slot(RegistrySlot *slots, int max_slots, const Eui64 *eui) {
  for (int i = 0; i < max_slots; ++i) {
    if (slots[i].occupied && eui64_equal(&slots[i].eui, eui)) {
      memset(&slots[i], 0, sizeof(slots[i]));
      return true;
    }
  }
  return false;
}

static bool contains(const RegistrySlot *slots, int max_slots, const Eui64 *eui, bool require_enabled) {
  if (eui == nullptr || eui64_is_zero(eui)) {
    return false;
  }
  for (int i = 0; i < max_slots; ++i) {
    if (slots[i].occupied && eui64_equal(&slots[i].eui, eui)) {
      return !require_enabled || slots[i].enabled;
    }
  }
  return false;
}

void registry_init(DeviceRegistry *reg) {
  memset(reg, 0, sizeof(*reg));
  reg->schema_version = NESSO_CONFIG_SCHEMA_VERSION;
}

bool registry_enroll_shelly(DeviceRegistry *reg, const Eui64 *eui, const char *label) {
  if (registry_is_router(reg, eui)) {
    return false;
  }
  return enroll(reg->shelly, NESSO_MAX_SHELLY_SLOTS, eui, label);
}

bool registry_remove_shelly(DeviceRegistry *reg, const Eui64 *eui) {
  return remove_slot(reg->shelly, NESSO_MAX_SHELLY_SLOTS, eui);
}

bool registry_enroll_router(DeviceRegistry *reg, const Eui64 *eui) {
  if (registry_is_authorized_shelly(reg, eui)) {
    return false;
  }
  return enroll(reg->routers, NESSO_MAX_ROUTER_SLOTS, eui, "router");
}

bool registry_remove_router(DeviceRegistry *reg, const Eui64 *eui) {
  return remove_slot(reg->routers, NESSO_MAX_ROUTER_SLOTS, eui);
}

bool registry_is_authorized_shelly(const DeviceRegistry *reg, const Eui64 *eui) {
  return contains(reg->shelly, NESSO_MAX_SHELLY_SLOTS, eui, true);
}

bool registry_is_router(const DeviceRegistry *reg, const Eui64 *eui) {
  return contains(reg->routers, NESSO_MAX_ROUTER_SLOTS, eui, false);
}

int registry_shelly_count(const DeviceRegistry *reg) {
  int n = 0;
  for (int i = 0; i < NESSO_MAX_SHELLY_SLOTS; ++i) {
    if (reg->shelly[i].occupied) {
      ++n;
    }
  }
  return n;
}

int registry_shelly_index(const DeviceRegistry *reg, const Eui64 *eui) {
  if (reg == nullptr || eui == nullptr || eui64_is_zero(eui)) {
    return -1;
  }
  for (int i = 0; i < NESSO_MAX_SHELLY_SLOTS; ++i) {
    if (reg->shelly[i].occupied && eui64_equal(&reg->shelly[i].eui, eui)) {
      return i;
    }
  }
  return -1;
}
