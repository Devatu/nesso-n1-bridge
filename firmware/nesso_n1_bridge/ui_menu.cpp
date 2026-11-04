#include "ui_menu.h"

const char *ui_menu_label(UiMenuId id) {
  switch (id) {
  case UI_MENU_HOME:
    return "Overzicht";
  case UI_MENU_WIFI:
    return "Wi-Fi opnieuw";
  case UI_MENU_UPDATE:
    return "Software-update";
  case UI_MENU_MODBUS_TEST:
    return "Ventilatie-test";
  case UI_MENU_ZIGBEE:
    return "Zigbee-knoppen";
  case UI_MENU_INFO:
    return "Info / versie";
  case UI_MENU_FACTORY:
    return "Fabrieksinstellingen";
  case UI_MENU_CLOSE:
    return "Sluiten";
  default:
    return "?";
  }
}

void ui_menu_next(uint8_t *index) {
  if (index == nullptr) {
    return;
  }
  *index = (uint8_t)((*index + 1) % (uint8_t)UI_MENU_COUNT);
}

UiMenuId ui_menu_id_at(uint8_t index) {
  if (index >= (uint8_t)UI_MENU_COUNT) {
    return UI_MENU_HOME;
  }
  return (UiMenuId)index;
}
