#ifndef NESSO_DISPLAY_UI_H
#define NESSO_DISPLAY_UI_H

#include <stdbool.h>

bool display_begin();
void display_show_status(const char *line);
void display_show_service_qr(const char *payload);

#endif
