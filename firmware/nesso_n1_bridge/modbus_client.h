#ifndef NESSO_MODBUS_CLIENT_H
#define NESSO_MODBUS_CLIENT_H

#include "action_table.h"
#include "heatpump_config.h"
#include <stdbool.h>
#include <stdint.h>

/* Qvantum D100041: Holding register 68 = Ventilation State */
#define NESSO_MB_HOLD_VENTILATION 68u

enum VentilationMode {
  VENT_OFF = 0,
  VENT_NORMAL = 1,
  VENT_EXTRA = 2,
  VENT_REDUCED = 3
};

enum ModbusResult {
  MB_OK = 0,
  MB_ERR_NO_TARGET,
  MB_ERR_NO_WIFI,
  MB_ERR_CONNECT,
  MB_ERR_TIMEOUT,
  MB_ERR_PROTOCOL,
  MB_ERR_DENIED,
  MB_ERR_EXCEPTION,
  MB_ERR_BUSY
};

/* Target host from NVS heatpump config; registers stay compile-time allowlist. */
bool modbus_set_target(const HeatpumpConfig *cfg);
bool modbus_has_target();
void modbus_worker_begin(); /* FreeRTOS worker — houdt UI/portal responsive */

bool modbus_ventilation_allowed(uint16_t value);

/* Sync (host-tests / worker-intern). Op device liever async_* gebruiken. */
ModbusResult modbus_probe(uint16_t *out_mode_or_null);
ModbusResult modbus_write_ventilation(uint16_t mode);
ModbusResult modbus_read_ventilation(uint16_t *out_mode);

/* Non-blocking: start job op worker-task. false = al bezig / geen target. */
bool modbus_async_start_probe();
bool modbus_async_start_write(uint16_t mode);
bool modbus_async_busy();
/* true als er een nieuw resultaat klaarstaat (eenmalig consumeren). */
bool modbus_async_take_result(ModbusResult *result, uint16_t *mode_out);

bool modbus_enqueue(const ActionSpec *spec);
void modbus_poll();

const char *modbus_result_text(ModbusResult r);
const char *ventilation_mode_label(uint16_t mode);

#endif
