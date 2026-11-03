#ifndef NESSO_WIFI_MANAGER_H
#define NESSO_WIFI_MANAGER_H

#include <stdbool.h>
#include <stddef.h>

bool wifi_sta_begin();
void wifi_sta_poll();
bool wifi_sta_connected();
bool wifi_sta_ip(char *out, size_t out_len);
bool wifi_has_saved_network();
void wifi_erase_credentials();
bool wifi_mdns_ready();
bool wifi_mdns_hostname(char *out, size_t out_len);
bool wifi_start_service_ap_fallback();
void wifi_stop_service_ap();
bool wifi_service_ap_active();

#endif
