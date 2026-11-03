#include "zigbee_coordinator.h"
#include "log.h"

static bool g_running = false;

bool zigbee_coordinator_begin() {
  LOG_W("zigbee", "BLOCKED: coordinator start requires ZCZR hardware test");
  g_running = false;
  return false;
}

void zigbee_coordinator_poll() {}

bool zigbee_open_join(uint8_t seconds) {
  (void)seconds;
  LOG_W("zigbee", "TODO: Zigbee.openNetwork() only in installer service mode");
  return false;
}

void zigbee_close_join() {}

void zigbee_pause() {
  g_running = false;
  LOG_I("zigbee", "paused (required before SoftAP)");
}

void zigbee_resume() {
  LOG_W("zigbee", "TODO: Zigbee.start() after SoftAP close");
}

bool zigbee_is_running() {
  return g_running;
}
