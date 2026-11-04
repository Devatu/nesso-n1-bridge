#ifndef NESSO_UI_MENU_H
#define NESSO_UI_MENU_H

#include <stdint.h>

enum UiMenuId {
  UI_MENU_HOME = 0,
  UI_MENU_WIFI,
  UI_MENU_UPDATE,
  UI_MENU_MODBUS_TEST,
  UI_MENU_ZIGBEE,
  UI_MENU_INFO,
  UI_MENU_FACTORY,
  UI_MENU_CLOSE,
  UI_MENU_COUNT
};

const char *ui_menu_label(UiMenuId id);
void ui_menu_next(uint8_t *index);
UiMenuId ui_menu_id_at(uint8_t index);

#endif
