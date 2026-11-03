#ifndef NESSO_AUTHORIZATION_H
#define NESSO_AUTHORIZATION_H

#include "device_registry.h"

enum AuthResult {
  AUTH_OK = 0,
  AUTH_NO_IEEE,
  AUTH_UNKNOWN_DEVICE,
  AUTH_ROUTER_FORBIDDEN,
  AUTH_EVENT_IGNORED
};

AuthResult authorize_button_event(const DeviceRegistry *reg, const ButtonEvent *event);

#endif
