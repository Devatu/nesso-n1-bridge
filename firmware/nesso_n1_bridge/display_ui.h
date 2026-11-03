#ifndef NESSO_DISPLAY_UI_H
#define NESSO_DISPLAY_UI_H

#include "heatpump_config.h"
#include "ota_manager.h"
#include "ui_menu.h"
#include <stdbool.h>
#include <stdint.h>

bool display_begin();
void display_bind_heatpump(HeatpumpConfig *hp);
void display_show_status(const char *line);
void display_show_service_qr(const char *payload);
void display_show_ble_prov(const char *service_name, const char *pop);
void display_show_home();
void display_show_ota(const OtaStatus *ota);
void display_show_menu(uint8_t selected);
void display_show_info();
void display_show_factory_confirm();
void display_show_wifi_confirm();
/* index 0..3 = modes, 4 = terug; status mag nullptr zijn */
void display_show_modbus_test(uint8_t selected, const char *status);

#endif
