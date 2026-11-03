#ifndef NESSO_BLE_PROVISION_H
#define NESSO_BLE_PROVISION_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Wi-Fi one-time setup via Espressif unified provisioning over BLE.
 * Installer client: official "ESP BLE Provisioning" app (Play Store / App Store).
 * CRA: Security 0 forbidden; PoP required; see docs/06 and docs/07-CRA-CONTROLS.md.
 */

enum BleProvState {
  BLE_PROV_IDLE = 0,
  BLE_PROV_WAITING_ZIGBEE_PAUSE,
  BLE_PROV_ACTIVE,
  BLE_PROV_VALIDATING_STA,
  BLE_PROV_SUCCESS,
  BLE_PROV_FAILED,
  BLE_PROV_TIMEOUT
};

enum BleProvSecurity {
  BLE_PROV_SEC_FORBIDDEN_0 = 0,
  BLE_PROV_SEC_1_POP = 1,
  BLE_PROV_SEC_2_SRP = 2
};

struct BleProvStatus {
  BleProvState state;
  BleProvSecurity security;
  char service_name[24];
  char pop[16];      /* proof-of-possession; never log in production builds */
  char sec2_user[16]; /* Security 2 username when used; often "wifiprov" */
  uint32_t deadline_ms;
};

bool ble_prov_begin(BleProvStatus *status, uint32_t now_ms);
void ble_prov_poll(BleProvStatus *status, uint32_t now_ms);
void ble_prov_abort(BleProvStatus *status);
bool ble_prov_is_active(const BleProvStatus *status);
bool ble_prov_wifi_already_provisioned();
BleProvSecurity ble_prov_selected_security();
void ble_prov_set_reset_on_next_begin(bool reset); /* KEY1 re-provision: wipe NVS Wi-Fi */

#endif
