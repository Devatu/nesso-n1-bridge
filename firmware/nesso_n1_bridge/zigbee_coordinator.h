#ifndef NESSO_ZIGBEE_COORDINATOR_H
#define NESSO_ZIGBEE_COORDINATOR_H

#include "device_registry.h"
#include "event_dedup.h"
#include "storage.h"
#include <stdbool.h>
#include <stdint.h>

/* SoftAP/BLE must call zigbee_pause() first (Espressif SoftAP+Zigbee = X). */
bool zigbee_coordinator_begin(DeviceRegistry *reg, EventDedup *dedup, AppConfigStore *store);
void zigbee_coordinator_poll();
bool zigbee_open_join(uint8_t seconds);
void zigbee_close_join();
bool zigbee_join_is_open();
uint32_t zigbee_join_remaining_s();
void zigbee_pause();
void zigbee_resume();
bool zigbee_is_running();
int zigbee_shelly_count();
bool zigbee_remove_shelly_index(uint8_t index);
/* JSON status for portal; buf >= 512 recommended. */
void zigbee_format_status_json(char *buf, size_t buflen);

#endif
