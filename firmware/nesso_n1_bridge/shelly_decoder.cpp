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

bool shelly_decode(const ZclIncoming *cmd, ButtonEvent *out) {
  if (cmd == nullptr || out == nullptr) {
    return false;
  }
  if (cmd->src_endpoint < 1 || cmd->src_endpoint > 4) {
    return false;
  }

  if (cmd->cluster == ZCL_CLUSTER_ONOFF) {
    if (cmd->command == ZCL_CMD_TOGGLE) {
      return set_event(out, cmd, cmd->src_endpoint, BUTTON_KIND_SINGLE);
    }
    if (cmd->command == ZCL_CMD_ON) {
      if (cmd->src_endpoint == 1) {
        return set_event(out, cmd, 1, BUTTON_KIND_SINGLE);
      }
      if (cmd->src_endpoint == 2) {
        return set_event(out, cmd, 3, BUTTON_KIND_SINGLE);
      }
      return false;
    }
    if (cmd->command == ZCL_CMD_OFF) {
      if (cmd->src_endpoint == 1) {
        return set_event(out, cmd, 2, BUTTON_KIND_SINGLE);
      }
      if (cmd->src_endpoint == 2) {
        return set_event(out, cmd, 4, BUTTON_KIND_SINGLE);
      }
      return false;
    }
    return false;
  }

  if (cmd->cluster == ZCL_CLUSTER_LEVEL && cmd->command == ZCL_CMD_STEP) {
    if (cmd->src_endpoint == 1 && cmd->step_mode == ZCL_STEP_UP) {
      return set_event(out, cmd, 1, BUTTON_KIND_HOLD);
    }
    if (cmd->src_endpoint == 1 && cmd->step_mode == ZCL_STEP_DOWN) {
      return set_event(out, cmd, 2, BUTTON_KIND_HOLD);
    }
    if (cmd->src_endpoint == 2 && cmd->step_mode == ZCL_STEP_UP) {
      return set_event(out, cmd, 3, BUTTON_KIND_HOLD);
    }
    if (cmd->src_endpoint == 2 && cmd->step_mode == ZCL_STEP_DOWN) {
      return set_event(out, cmd, 4, BUTTON_KIND_HOLD);
    }
    return false;
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
