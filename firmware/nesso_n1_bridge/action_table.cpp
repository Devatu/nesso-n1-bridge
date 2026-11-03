#include "action_table.h"

/*
 * Compile-time allowlist — Qvantum D100041 holding register 68 (Ventilation State).
 * Zigbee-knoppen: 1→Uit(0) 2→Gereduceerd(3) 3→Normaal(1) 4→Extra(2).
 */
static const ActionSpec k_actions[] = {
    {ACTION_1, 1, MB_FN_WRITE_SINGLE_REGISTER, 68, 0, true, "Ventilatie uit"},
    {ACTION_2, 2, MB_FN_WRITE_SINGLE_REGISTER, 68, 3, true, "Ventilatie gereduceerd"},
    {ACTION_3, 3, MB_FN_WRITE_SINGLE_REGISTER, 68, 1, true, "Ventilatie normaal"},
    {ACTION_4, 4, MB_FN_WRITE_SINGLE_REGISTER, 68, 2, true, "Ventilatie extra"},
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
