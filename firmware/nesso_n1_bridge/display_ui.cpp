#include "display_ui.h"
#include "log.h"

bool display_begin() {
  LOG_W("display", "TODO: NessoDisplay/M5GFX when NESSO_ENABLE_HW=1");
  return false;
}

void display_show_status(const char *line) {
  LOG_I("display", "%s", line);
}

void display_show_service_qr(const char *payload) {
  LOG_I("display", "TODO QR payload len=%u (not an auth substitute)", payload ? 0u : 0u);
  (void)payload;
}
