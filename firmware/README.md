# Firmware

Fase 0-scaffold + provisioning/OTA-ontwerp. Geen hardwarebinary, geen echte Modbus-registers, geen secrets.

Zie `docs/06-PROVISIONING-PORTAL-OTA.md`.

## Wat werkt nu (host)

Pure logica in `nesso_n1_bridge/`:

- Shelly-decoder, EUI-64 registry, autorisatie, dedup
- Compile-time action table met **placeholders**
- Heatpump IP/poort/unit-ID validatie
- Lokale portal API-classificatie (register-editor **verboden**)
- OTA-manifest parse + policy (HTTPS, board, no security downgrade)
- OTA A/B state helpers

## Wat stub/TODO is (hardware)

`ble_provision` (WiFiProv BLE), `wifi_manager`, `installer_portal` HTTP-server, `zigbee_coordinator`, `display_ui`, volledige OTA download/sign verify, NVS. `NESSO_ENABLE_HW` staat op 0.

## Productflow

1. **BLE** eenmalig Wi-Fi (Zigbee gepauzeerd). SoftAP niet primair.
2. **Lokale LAN-portal** voor warmtepomp IP / poort / unit ID (geen cloud, geen vrije registers).
3. **OTA A/B** naar inactive slot + rollback.

## Hosttests

```powershell
powershell -ExecutionPolicy Bypass -File firmware/test/run_host_tests.ps1
```

## Eerste flash + OTA A/B

Volledige checklist: **`docs/08-EERSTE-FLASH-EN-OTA.md`**.

### Arduino IDE

1. Boards Manager: `esp32` by Espressif **≥ 3.3.5** (doel 3.3.12).
2. Library Manager: `Arduino_Nesso_N1` 1.0.0.
3. Board: **Arduino Nesso N1**.
4. Zigbee Mode → **Zigbee ZCZR (coordinator/router)**.
5. Partition Scheme → **Zigbee ZCZR 4MB with spiffs** (dit is OTA A/B).
6. Open `firmware/nesso_n1_bridge/nesso_n1_bridge.ino`.
7. Hardware: `#define NESSO_ENABLE_HW 1` in `app_config.h`.

### Arduino CLI

```powershell
powershell -ExecutionPolicy Bypass -File scripts\setup_arduino_cli.ps1
powershell -ExecutionPolicy Bypass -File scripts\build_firmware.ps1
powershell -ExecutionPolicy Bypass -File scripts\flash_firmware.ps1 -Port COMx
```

FQBN: `esp32:esp32:arduino_nesso_n1:ZigbeeMode=zczr,PartitionScheme=zigbee_zczr`

### GitHub OTA

1. Init/push repo (project was nog geen git-repo).
2. CI: `.github/workflows/ci.yml` (hosttests).
3. Release draft: `.github/workflows/release.yml` op tag `v*`.
4. Productie: signed manifest + `.bin` in Release assets; device HTTPS OTA naar inactive slot.
