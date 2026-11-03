#include "installer_portal.h"
#include "app_config.h"
#include "log.h"
#include "portal_api.h"

static bool g_running = false;
static HeatpumpConfig *g_hp = nullptr;

bool portal_begin(HeatpumpConfig *heatpump) {
  g_hp = heatpump;
#if NESSO_ENABLE_HW
  LOG_I("portal", "TODO: bind WebServer to STA LAN IP only; session+CSRF auth");
#else
  LOG_I("portal", "scaffold: local LAN portal ready (no cloud). Routes: /api/heatpump /api/status");
#endif
  g_running = true;
  return true;
}

void portal_poll() {
  if (!g_running) {
    return;
  }
  /* HW: accept HTTP; classify via portal_classify_path; reject register editors. */
}

void portal_stop() {
  g_running = false;
}

bool portal_is_running() {
  return g_running;
}

HeatpumpConfig *portal_heatpump() {
  return g_hp;
}
