#include "modbus_client.h"
#include "log.h"

static HeatpumpConfig g_target;
static bool g_has_target = false;

bool modbus_set_target(const HeatpumpConfig *cfg) {
  if (!heatpump_config_is_ready(cfg)) {
    g_has_target = false;
    LOG_W("modbus", "target rejected: heatpump config incomplete");
    return false;
  }
  g_target = *cfg;
  g_has_target = true;
  LOG_I("modbus", "target set %s:%u unit=%u (registers still compile-time)", g_target.ipv4,
        g_target.tcp_port, g_target.unit_id);
  return true;
}

bool modbus_enqueue(const ActionSpec *spec) {
  if (!g_has_target) {
    LOG_W("modbus", "enqueue refused: no heatpump IP/unit configured");
    return false;
  }
  if (!action_is_ready(spec)) {
    LOG_W("modbus", "enqueue refused: action table not owner-confirmed");
    return false;
  }
  LOG_I("modbus", "TODO: write %s unit=%u FC=%u addr=%u value=%u", g_target.ipv4, g_target.unit_id,
        (unsigned)spec->function, spec->address, spec->value);
  return false;
}

void modbus_poll() {}
