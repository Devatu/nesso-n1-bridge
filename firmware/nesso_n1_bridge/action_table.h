#ifndef NESSO_ACTION_TABLE_H
#define NESSO_ACTION_TABLE_H

#include "app_config.h"
#include <stdbool.h>
#include <stdint.h>

enum ModbusFunction {
  MB_FN_UNSET = 0,
  MB_FN_WRITE_SINGLE_COIL = 5,
  MB_FN_WRITE_SINGLE_REGISTER = 6,
  MB_FN_WRITE_MULTIPLE_REGISTERS = 16
};

struct ActionSpec {
  ActionId action;
  uint8_t button;
  ModbusFunction function;
  uint16_t address;
  uint16_t value;
  bool confirmed_by_owner;
  const char *meaning;
};

const ActionSpec *action_for_button(uint8_t button);
bool action_is_ready(const ActionSpec *spec);

#endif
