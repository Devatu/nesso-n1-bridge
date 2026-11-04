#ifndef NESSO_WIFI_MANAGER_H
#define NESSO_WIFI_MANAGER_H

#include <stdbool.h>
#include <stddef.h>

bool wifi_sta_begin();
void wifi_sta_poll();
bool wifi_sta_connected();
/* Soft reconnect — never disconnect/erase; safe beside Zigbee. */
bool wifi_sta_ensure();
/* Harder reconnect (disconnect+begin). Avoid during Zigbee join. */
bool wifi_sta_reconnect();
void wifi_sta_set_hold_off(bool hold_off); /* true = don't auto-reconnect (e.g. Zigbee join) */
/* Lower STA TX during Zigbee join to cut dual-radio current spikes (C6 single RF). */
void wifi_sta_set_join_tx_low(bool low);
bool wifi_sta_ip(char *out, size_t out_len);
bool wifi_has_saved_network();
void wifi_erase_credentials();
bool wifi_mdns_ready();
bool wifi_mdns_hostname(char *out, size_t out_len);
bool wifi_start_service_ap_fallback();
void wifi_stop_service_ap();
bool wifi_service_ap_active();

#endif
