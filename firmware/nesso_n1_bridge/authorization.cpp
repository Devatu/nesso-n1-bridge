#include "authorization.h"

AuthResult authorize_button_event(const DeviceRegistry *reg, const ButtonEvent *event) {
  if (reg == nullptr || event == nullptr) {
    return AUTH_UNKNOWN_DEVICE;
  }
  if (!event->source_ieee_resolved || eui64_is_zero(&event->source)) {
    return AUTH_NO_IEEE;
  }
  if (registry_is_router(reg, &event->source)) {
    return AUTH_ROUTER_FORBIDDEN;
  }
  if (!registry_is_authorized_shelly(reg, &event->source)) {
    return AUTH_UNKNOWN_DEVICE;
  }
  if (event->kind != BUTTON_KIND_SINGLE) {
    return AUTH_EVENT_IGNORED;
  }
  if (event->button < 1 || event->button > 4) {
    return AUTH_EVENT_IGNORED;
  }
  return AUTH_OK;
}
