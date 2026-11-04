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
/* false tijdens Zigbee.begin/privilege — UI mag nog geen join starten. */
bool zigbee_join_ready();
int zigbee_shelly_count();
bool zigbee_remove_shelly_index(uint8_t index);
/* Last join/press line for UI (never null; may be empty). */
const char *zigbee_last_event();
/* JSON status for portal; buf >= 512 recommended. */
void zigbee_format_status_json(char *buf, size_t buflen);
/* Debug snapshot: counters, neighbors, enrolled, bind state. buf >= 1536. */
void zigbee_format_diag_json(char *buf, size_t buflen);
/* Force ZDO rebind of all enrolled Shellies (while awake). */
void zigbee_diag_rebind_all();
/* Dump neighbor/registry lines into the log ring now. */
void zigbee_diag_dump_now();

#endif
