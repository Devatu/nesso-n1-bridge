#ifndef NESSO_ZIGBEE_COORDINATOR_H
#define NESSO_ZIGBEE_COORDINATOR_H

#include <stdbool.h>
#include <stdint.h>

/* BLOCKED until ZCZR hardware test. SoftAP must call pause() first. */
bool zigbee_coordinator_begin();
void zigbee_coordinator_poll();
bool zigbee_open_join(uint8_t seconds);
void zigbee_close_join();
void zigbee_pause();
void zigbee_resume();
bool zigbee_is_running();

#endif
