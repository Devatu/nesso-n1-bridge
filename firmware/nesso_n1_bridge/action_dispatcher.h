#ifndef NESSO_ACTION_DISPATCHER_H
#define NESSO_ACTION_DISPATCHER_H

#include "action_table.h"
#include "authorization.h"
#include "event_dedup.h"

enum DispatchResult {
  DISPATCH_QUEUED = 0,
  DISPATCH_AUTH_FAIL,
  DISPATCH_DEDUPED,
  DISPATCH_ACTION_NOT_READY,
  DISPATCH_BAD_EVENT
};

struct DispatchOutcome {
  DispatchResult result;
  AuthResult auth;
  ActionId action;
};

void dispatcher_init(EventDedup *dedup);
DispatchOutcome dispatcher_handle(const DeviceRegistry *reg, EventDedup *dedup, ButtonEvent *event);

#endif
