#include "action_dispatcher.h"

void dispatcher_init(EventDedup *dedup) {
  dedup_init(dedup, NESSO_DEDUP_WINDOW_MS);
}

DispatchOutcome dispatcher_handle(const DeviceRegistry *reg, EventDedup *dedup, ButtonEvent *event) {
  DispatchOutcome out;
  out.result = DISPATCH_BAD_EVENT;
  out.auth = AUTH_UNKNOWN_DEVICE;
  out.action = ACTION_NONE;
  if (event == nullptr) {
    return out;
  }
  out.auth = authorize_button_event(reg, event);
  if (out.auth != AUTH_OK) {
    out.result = DISPATCH_AUTH_FAIL;
    return out;
  }
  if (!dedup_accept(dedup, event)) {
    out.result = DISPATCH_DEDUPED;
    return out;
  }
  const ActionSpec *spec = action_for_button(event->button);
  if (!action_is_ready(spec)) {
    out.action = spec != nullptr ? spec->action : ACTION_NONE;
    out.result = DISPATCH_ACTION_NOT_READY;
    return out;
  }
  out.action = spec->action;
  out.result = DISPATCH_QUEUED;
  return out;
}
