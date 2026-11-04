#include "shelly_decoder.h"

static bool set_event(ButtonEvent *out, const ZclIncoming *cmd, uint8_t button, ButtonKind kind) {
  if (button < 1 || button > 4) {
    return false;
  }
  eui64_copy(&out->source, &cmd->src_ieee);
  out->button = button;
  out->kind = kind;
  out->timestamp_ms = 0;
  out->source_ieee_resolved = cmd->ieee_resolved;
  return true;
}

/*
 * Shelly BLU RC Button 4 ZB — default On/Off mode (factory):
 *   EP1 ON  → knop 1,  EP1 OFF → knop 2
 *   EP2 ON  → knop 3,  EP2 OFF → knop 4
 * Toggle mode: TOGGLE op EP k → knop k
 * Scenes: RecallScene op EP k → knop k (double/triple/long)
 */
static uint8_t button_from_onoff(uint8_t src_ep, uint8_t command) {
  if (command == ZCL_CMD_TOGGLE) {
    return src_ep;
  }
  if (src_ep >= 1 && src_ep <= 2 &&
      (command == ZCL_CMD_ON || command == ZCL_CMD_OFF)) {
    return (uint8_t)((src_ep - 1) * 2 + (command == ZCL_CMD_ON ? 1 : 2));
  }
  return src_ep;
}

static uint8_t button_from_hold(uint8_t src_ep, uint8_t step_mode) {
  if (src_ep >= 1 && src_ep <= 2) {
    return (uint8_t)((src_ep - 1) * 2 + (step_mode == ZCL_STEP_DOWN ? 2 : 1));
  }
  return src_ep;
}

bool shelly_decode(const ZclIncoming *cmd, ButtonEvent *out) {
  if (cmd == nullptr || out == nullptr) {
    return false;
  }
  if (cmd->src_endpoint < 1 || cmd->src_endpoint > 4) {
    return false;
  }

  if (cmd->cluster == ZCL_CLUSTER_ONOFF) {
    if (cmd->command == ZCL_CMD_TOGGLE || cmd->command == ZCL_CMD_ON ||
        cmd->command == ZCL_CMD_OFF) {
      return set_event(out, cmd, button_from_onoff(cmd->src_endpoint, cmd->command),
                       BUTTON_KIND_SINGLE);
    }
    return false;
  }

  if (cmd->cluster == ZCL_CLUSTER_LEVEL && cmd->command == ZCL_CMD_STEP) {
    return set_event(out, cmd, button_from_hold(cmd->src_endpoint, cmd->step_mode),
                     BUTTON_KIND_HOLD);
  }

  if (cmd->cluster == ZCL_CLUSTER_SCENES && cmd->command == ZCL_CMD_RECALL_SCENE) {
    ButtonKind kind;
    switch (cmd->scene_id) {
      case 1:
        kind = BUTTON_KIND_DOUBLE;
        break;
      case 2:
        kind = BUTTON_KIND_TRIPLE;
        break;
      case 11:
        kind = BUTTON_KIND_LONG;
        break;
      case 12:
        kind = BUTTON_KIND_DOUBLE_LONG;
        break;
      case 13:
        kind = BUTTON_KIND_TRIPLE_LONG;
        break;
      default:
        return false;
    }
    return set_event(out, cmd, cmd->src_endpoint, kind);
  }

  return false;
}
