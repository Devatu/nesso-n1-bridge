#ifndef NESSO_WIFI_MANAGER_H
#define NESSO_WIFI_MANAGER_H

#include <stdbool.h>

bool wifi_sta_begin();
void wifi_sta_poll();
bool wifi_sta_connected();
bool wifi_start_service_ap_fallback(); /* only with Zigbee paused; not primary */
void wifi_stop_service_ap();
bool wifi_service_ap_active();

#endif
