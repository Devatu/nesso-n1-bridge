# 03 – Componenteninventaris (SBOM-voorloper)

| Veld | Waarde |
|---|---|
| Document-ID | CRA-03 |
| Versie | 0.1-draft |
| Datum | 2026-11-01 |
| Status | Concept – geen volledige machine-readable SBOM |

CRA vraagt een machine-readable SBOM (≥ top-level dependencies) in de technische documentatie. Dit document is de **handmatige voorloper** tot CI SBOM (CycloneDX/SPDX) per release.

## Productcomponenten (top-level)

| Component | Leverancier | Rol | Versie/ref | Opmerking |
|---|---|---|---|---|
| Arduino Nesso N1 (TPX00227) | Arduino S.r.l. | Hardware | CE/RED DoC + Eurofins type exam 2025; zie CRA-12 | ESP32-C6, 16 MB; leverancierscertificaten in `docs/Arduino_TPX00227-*` |
| ESP32-C6 SoC + RF | Espressif | MCU, Wi-Fi, BLE, 802.15.4 | via Arduino-ESP32 core | |
| Arduino-ESP32 core | Espressif | Framework | ≥ 3.3.5 (doel 3.3.12) | Incl. Zigbee, WiFiProv, Update |
| Arduino_Nesso_N1 | Arduino | Board support | 1.0.0 | Display, expander, KEY1/2 |
| M5GFX | M5Stack | Display | via library dep | |
| Firmware `nesso_n1_bridge` | Dit project | Applicatie | 0.0.2-fase0-prov | Eigen code |
| Shelly BLU RC Button 4 ZB | Shelly | Zigbee ED | SBBT-104CUS; FW **OPEN** | Max 5 |
| ESP BLE Provisioning app | Espressif | Provisioning client | Store-app | Geen eigen cloud |
| Warmtepomp Modbus TCP | Derde partij | Actuator | **OPEN** | 1 target |

## Software-dependencies (verwacht in SBOM)

Te genereren uit build, minimaal:

- Arduino-ESP32 precompiled libs (Wi-Fi, BLE/NimBLE, Zigbee/ZBOSS, mbedTLS, …)
- Application modules in `firmware/nesso_n1_bridge/`
- Host-test toolchain hoort niet in device-SBOM

## Leveranciersdocumentatie (SharePoint 03_)

- Arduino Nesso datasheet / user manual links
- **Arduino TPX00227 certificaten & DoC’s** — catalogus: `docs/cra/12-hardware-certificaten-arduino.md` (bestanden in `docs/Arduino_TPX00227-*`)
- Shelly BLU RC Button 4 ZB technical docs
- Espressif provisioning + OTA docs
- Verklaringen / license notices **TODO**

**Belangrijk:** Arduino CE/RED dekt het bord als Arduino-product. Dat is leveranciersbewijs, geen automatische CE/RED/CRA van het heatpump-bridge eindproduct.

## Volgende stap

CI: SBOM artifact per signed release → SharePoint `03_` + `07_`.
