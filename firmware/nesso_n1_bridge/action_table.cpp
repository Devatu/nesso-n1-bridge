#include "action_table.h"

/*
 * Compile-time allowlist. Registers/values remain placeholders until the
 * owner confirms the heat-pump map. Do not invent production addresses.
 */
static const ActionSpec k_actions[] = {
    {ACTION_1, 1, MB_FN_UNSET, 0xFFFF, 0, false, "BLOCKED: owner must confirm ACTION_1"},
    {ACTION_2, 2, MB_FN_UNSET, 0xFFFF, 0, false, "BLOCKED: owner must confirm ACTION_2"},
    {ACTION_3, 3, MB_FN_UNSET, 0xFFFF, 0, false, "BLOCKED: owner must confirm ACTION_3"},
    {ACTION_4, 4, MB_FN_UNSET, 0xFFFF, 0, false, "BLOCKED: owner must confirm ACTION_4"},
};

const ActionSpec *action_for_button(uint8_t button) {
  for (unsigned i = 0; i < sizeof(k_actions) / sizeof(k_actions[0]); ++i) {
    if (k_actions[i].button == button) {
      return &k_actions[i];
    }
  }
  return nullptr;
}

bool action_is_ready(const ActionSpec *spec) {
  return spec != nullptr && spec->confirmed_by_owner && spec->function != MB_FN_UNSET &&
         spec->address != 0xFFFF;
}
