# Fase 0 – Verificatieresultaten

Datum: 2026-11-01  
Scope: desktoponderzoek van officiële bronnen. Geen hardware in deze sessie.  
Besluit: **CONDITIONAL GO** voor Fase 1-scaffold en host-unit tests. **NO-GO** voor productie-flashbeveiliging, eFuse anti-rollback, productie-signing en hardwaregevaarlijke handelingen.

## Samenvatting GO / NO-GO

| Onderwerp | Besluit | Voorwaarde |
|---|---|---|
| Arduino Nesso N1 + IDE/CLI | **GO** | `esp32` core **≥ 3.3.5** (getest-doel: **3.3.12**), library `Arduino_Nesso_N1` **1.0.0** |
| Zigbee coordinator API (Arduino) | **CONDITIONAL GO** | Tools → Zigbee Mode = `Zigbee ZCZR`; officiële Nesso-docs tonen alleen End Device |
| IEEE EUI-64 bronauthenticatie | **CONDITIONAL GO** | `esp_zb_ieee_address_by_short()` + Device Announce; weigeren als lookup faalt |
| Display, KEY1/KEY2, QR | **GO** (software) | 135×240 is krap; QR-weergave is haalbaar, niet officieel gedocumenteerd op Nesso |
| Flash / OTA A/B | **CONDITIONAL GO** | Officiële ZCZR-tabel is 4 MB-layout; 16 MB-ZCZR-menu ontbreekt; rollback alleen met rollback-bootloader + 2 OTA-slots |
| Wi-Fi STA + Zigbee coordinator | **RISK / HARDWARETEST** | Espressif: STA + Zigbee router = **C1** (onstabiel) |
| SoftAP + Zigbee coordinator | **MITIGATIE VERPLICHT** | Espressif: SoftAP + Zigbee router = **X** (niet ondersteund). AP alleen met Zigbee gepauzeerd |
| Shelly BLU RC Button 4 ZB signature | **GO** (decoder-ontwerp) | Officiële Shelly-docs + Z2M; pairing/firmware nog niet op hardware |
| Modbus-registers / warmtepomp-IP | **BLOCKED** | Wacht op eigenaar |
| Hardware beschikbaar | **BLOCKED** | Wacht op eigenaar |
| Firmware signing op target | **CONDITIONAL GO** | Arduino `Update` + `UPDATE_SIGN`: RSA/ECDSA. **Niet Ed25519** via mbedTLS |
| Productie Secure Boot / flash encrypt / eFuse | **NO-GO** | Geen herstelplan en geen manufacturing-flow |

**Eindoordeel Fase 0:** doorgaan met compileerbare Arduino-scaffold en hosttests. Zigbee/Wi-Fi/OTA blijven stubs tot hardwaretests slagen.

---

## 1. Board, core, pinout

| Gegeven | Waarde | Bron |
|---|---|---|
| Product | Arduino Nesso N1, SKU **TPX00227** | [docs.arduino.cc/hardware/nesso-n1](https://docs.arduino.cc/hardware/nesso-n1) |
| SoC | ESP32-C6, RISC-V single-core @ 160 MHz, 512 kB SRAM | [User Manual](https://docs.arduino.cc/tutorials/nesso-n1/user-manual/) |
| Externe flash | **16 MB** (GD25Q128 / W25Q128) | [Datasheet](https://docs.arduino.cc/resources/datasheets/TPX00227-datasheet.pdf), [datasheet.md](https://github.com/arduino/docs-content/blob/main/content/hardware/09.kits/maker/nesso-n1/datasheet/datasheet.md) |
| Radio | Wi-Fi 6 2.4 GHz, BLE 5.3, 802.15.4 Zigbee/Thread, LoRa SX1262 | idem |
| Board package | `esp32` by Espressif Systems **≥ 3.3.5** (Nesso sinds 3.3.3) | User Manual; [arduino-esp32#11985](https://github.com/espressif/arduino-esp32/pull/11985) |
| Laatste stabiele core (2026-09-18) | **3.3.12**, ESP-IDF v5.5.5 | [arduino-esp32 releases](https://github.com/espressif/arduino-esp32/releases) |
| FQBN | `esp32:esp32:arduino_nesso_n1` | `boards.txt` (core master) |
| `build.board` | `ARDUINO_NESSO_N1` | idem |
| `build.mcu` | `esp32c6` | idem |
| Support library | `Arduino_Nesso_N1` **1.0.0**, depends M5GFX + Arduino_BMI270_BMM150 | [library.properties](https://github.com/arduino-libraries/Arduino_Nesso_N1) |
| GPIO | 3.3 V only, niet 5 V-tolerant | User Manual |

### Directe ESP32-C6 pinnen (User Manual)

| Naam | GPIO | Functie |
|---|---|---|
| SDA | 10 | I2C |
| SCL | 8 | I2C |
| MOSI / MISO / SCK | 21 / 22 / 20 | SPI (gedeeld LCD / LoRa / ext. flash-pad) |
| LCD_CS / LCD_RS | 17 / 16 | Display |
| SYS_IRQ | 3 | IMU + expander IRQ |
| KEY1 / KEY2 | expander E0.P0 / E0.P1 | Vereist `#include <Arduino_Nesso_N1.h>` |
| LCD_RESET / LCD_BACKLIGHT | E1.P1 / E1.P6 | idem |
| LED_BUILTIN | E1.P7 | Groen |
| POWEROFF | E1.P0 | Power-off sturing |

Powerknop is **geen** vrije GPIO: klik = aan/reset, dubbelklik = uit, lang = bootloader. Installer-servicemodus moet dus **KEY1 en/of KEY2** gebruiken, niet de powerknop.

---

## 2. Display en QR

| Gegeven | Waarde | Bron |
|---|---|---|
| LCD | 1.14" IPS, **135×240**, ST7789(P3), SPI | User Manual, pinout PDF |
| Touch | FT6336U, I2C 0x38 | Pinout PDF |
| API | `NessoDisplay` / `NessoTouch` (M5GFX) | User Manual, Arduino_Nesso_N1 |

QR: geen officiële Nesso-QR-API. M5GFX kan pixels tekenen; een QR van ~21×21 modules past op 135×240 in landscape als AP-SSID/wachtwoord kort gehouden worden. **Hardwareverificatie nog open.** QR is weergave, geen authenticatie.

---

## 3. Flash, partities, OTA, rollback

### Feiten

- `boards.txt` zet `build.flash_size=16MB`.
- Nesso-menu ZCZR: alleen **`Zigbee ZCZR 4MB with spiffs`** → `zigbee_zczr.csv`, `upload.maximum_size=1310720` (1.25 MB app).
- Geen `zigbee_zczr_8MB`, geen `zigbee_zczr_16MB`, geen `Custom` in het Nesso-menu.
- Officiële `zigbee_zczr.csv` (Espressif) bevat wél A/B:

```
nvs, otadata, app0=ota_0 0x140000, app1=ota_1 0x140000,
spiffs, zb_storage, zb_fct, rcp_fw, coredump
```

Bron: [zigbee_zczr.csv](https://github.com/espressif/arduino-esp32/blob/master/tools/partitions/zigbee_zczr.csv)

- Officiële `zigbee_zczr_8MB.csv` heeft dezelfde Zigbee-FAT-partities en **3.25 MB** per OTA-slot. Die tabel staat **niet** in het Nesso-menu.
- Arduino-ESP32 bootloader: discussie #12350 stelt dat de officiële core `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` heeft. Dat is **niet** door ons op deze Nesso-bootloader geflasht/getest.
- eFuse anti-rollback / Secure Boot / flash encryption: **niet activeren** (producteis 6).

### Keuze (niet als feit geflasht)

1. Eerste coordinator-builds: officiële `zigbee_zczr` (4 MB-layout). Werkt op 16 MB-flash; bovenste 12 MB ongebruikt.
2. Als de app > 1.25 MB wordt: sketch-`partitions.csv` **kopiëren** van officiële `zigbee_zczr_8MB.csv` (geen eigen layout verzinnen).
3. Eigen 16 MB-ZCZR-tabel: **BLOCKED** tot app-grootte en `zb_storage`-behoefte gemeten zijn.

---

## 4. Zigbee coordinator API

### Wat de Nesso-docs zeggen

Officieel voorbeeld is **End Device** (light bulb), met:

- Tools → Zigbee Mode → End device
- Partition Scheme → Zigbee SPIFF 4MB

Bron: [User Manual – Zigbee](https://docs.arduino.cc/tutorials/nesso-n1/user-manual/)

### Wat de Arduino-ESP32-stack biedt

Nesso `boards.txt` bevat:

```
ZigbeeMode.zczr=Zigbee ZCZR (coordinator/router)
build.zigbee_mode=-DZIGBEE_MODE_ZCZR
build.zigbee_libs=-lesp_zb_api.zczr -lzboss_stack.zczr -lzboss_port.native
```

API (documentatie 3.3.x):

- `Zigbee.begin(ZIGBEE_COORDINATOR)`
- `Zigbee.openNetwork(seconds)` / `closeNetwork()` / `setRebootOpenNetwork()`
- `Zigbee.stop()` / `start()` — nodig om SoftAP-mitigatie te implementeren
- `Zigbee.formatIEEEAddress()`
- Device Announce op coordinator (`ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE`)

Bronnen:

- [ZigbeeCore](https://docs.espressif.com/projects/arduino-esp32/en/latest/zigbee/zigbee_core.html)
- [ZigbeeCore.cpp](https://github.com/espressif/arduino-esp32/blob/master/libraries/Zigbee/src/ZigbeeCore.cpp)

### EUI-64 (producteis 3)

| Mechanisme | Status |
|---|---|
| `esp_zb_ieee_address_by_short(short, ieee)` | Officieel; lokale address-map; `ESP_ERR_NOT_FOUND` mogelijk | [NWK API](https://docs.espressif.com/projects/esp-zigbee-sdk/en/release-v1.0/esp32c5/api-reference/nwk/esp_zigbee_nwk.html) |
| Device Announce IEEE | Aanwezig in coordinator-signal handler | ZigbeeCore.cpp |
| Alleen short address | **Onvoldoende** — actie weigeren |

Arduino high-level `ZigbeeLight`/`ZigbeeSwitch` geeft geen kant-en-klare “incoming remote button + IEEE”-callback. Ruwe ZCL-interceptie: `addPrivilegeCommand` + `onPrivilegeCommand` ([PR #12479](https://github.com/espressif/arduino-esp32/pull/12479), aanwezig in recente 3.3.x). **Fase 3 moet dit op hardware bewijzen.**

### Limieten (routers / max devices)

`esp_zb_nwk_set_max_children()` bestaat. Werkelijke neighbor/routing-capaciteit van de precompiled ZBOSS-lib op Nesso: **niet gemeten**. Router-limiet blijft **TODO + hardwaremeting**.

---

## 5. Wi-Fi / Zigbee coexistence (kritiek)

Espressif, ESP32-C6, één 2.4 GHz RF-pad, TDM:

Bron: [RF Coexistence ESP32-C6](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-guides/coexist.html)

| Combinatie | Officieel |
|---|---|
| Wi-Fi STA connected + Zigbee End Device | Y (stabiel) |
| Wi-Fi STA + Zigbee Router/Coordinator | **C1** (ondersteund, onstabiel) |
| SoftAP beacon + Zigbee Router | **X** |
| SoftAP connecting/connected + Zigbee Router | **X** |

Coordinator gedraagt zich RF-technisch als router (RX-on-when-idle). Espressif adviseert dual-SoC voor een Wi-Fi Zigbee-gateway. Dat is een **productwijziging**, geen Nesso-alleen-oplossing.

Bevestigd door:

- [esp-zigbee-sdk#273](https://github.com/espressif/esp-zigbee-sdk/issues/273) — “WiFi AP + Zigbee coexistence is not yet supported”
- [esp-idf#18356](https://github.com/espressif/esp-idf/issues/18356) — SoftAP-clients falen met Zigbee actief

### Verplichte productmitigatie (nog te testen)

1. Normaal bedrijf: STA + Zigbee coordinator. SoftAP **uit**.
2. Installer-modus: `Zigbee.stop()` → SoftAP + portal → credentials valideren → AP sluiten → `Zigbee.start()`.
3. Tijdens AP-venster (~10 min) werken knoppen **niet**. Dat moet in de UI staan.
4. 72-uurs STA+Zigbee+Modbus-test is **exit criterium**, geen aanname.

---

## 6. Shelly BLU RC Button 4 ZB

| Veld | Waarde | Bron |
|---|---|---|
| Korte naam | **SBBT-104CUS** | [Shelly docs](https://shelly-api-docs.shelly.cloud/docs-ble/Devices/BLU_ZB/button4_ZB/) |
| BLE device id | 0x16 | idem |
| Zigbee modelId | `BLU RC Button 4 ZB` | [Z2M issue #30640](https://github.com/Koenkk/zigbee2mqtt/issues/30640) (`manufName=Shelly`, `manufId=5264`) |
| Manufacturer | `Shelly` | idem |
| Profile | 0x0104 HA | Shelly docs |
| Device Id | 0x0000 On/Off Switch | Shelly docs |
| Type | Sleepy end device, battery | Z2M database dump |
| Endpoints | 1–4 | Shelly docs, Z2M |

### Pairing (Shelly)

- Zigbee steer: **5× willekeurige knop**, magenta, 1 minuut.
- Find & Bind: 2 kort + hold op die knop.
- Factory reset: 30 s hold binnen 3 min na batterij.

### Event encoding (officieel + Z2M-converter)

Default **On/Off mode** (factory):

| Fysieke knop | Cluster | Endpoint | Command |
|---|---|---|---|
| 1 single | On/Off 0x0006 | 1 | ON 0x01 |
| 2 single | On/Off 0x0006 | 1 | OFF 0x00 |
| 3 single | On/Off 0x0006 | 2 | ON 0x01 |
| 4 single | On/Off 0x0006 | 2 | OFF 0x00 |
| 1 hold | Level 0x0008 | 1 | Step Up (mode 0) |
| 2 hold | Level 0x0008 | 1 | Step Down (mode 1) |
| 3 hold | Level 0x0008 | 2 | Step Up |
| 4 hold | Level 0x0008 | 2 | Step Down |

**Toggle mode** (Switch Actions 0x0010): TOGGLE op EP k = knop k.

Beide modi, scenes (RecallScene op EP k):

| Scene | Event |
|---|---|
| 1 | double |
| 2 | triple |
| 11 | single long |
| 12 | double long |
| 13 | triple long |

Bronnen: Shelly technical docs; [zigbee-herdsman-converters `four_buttons_*`](https://github.com/Koenkk/zigbee-herdsman-converters/blob/master/src/devices/shelly.ts).

Productbeleid (docs/01): **alleen single press** → ACTION_1..4. Overige events decoderen en **negeren**, tot de eigenaar anders beslist.

Identiteit: modelstring is **onvoldoende**. Alleen geauthenticeerde EUI-64 in de allowlist mag een actie triggeren. Routers nooit.

Firmwareversie van testhardware: **onbekend / BLOCKED**.

---

## 7. Signing / OTA-client

| Optie | Bruikbaarheid |
|---|---|
| Arduino `Update` + `-DUPDATE_SIGN` + RSA/ECDSA | **Aanbevolen pad** — [Update library](https://github.com/espressif/arduino-esp32/tree/master/libraries/Update) |
| Ed25519 via mbedTLS | **Niet** — mbedTLS ondersteunt geen EdDSA |
| HTTPS + cert bundle | Arduino-ESP32 heeft `useBuiltinCACertBundle` (3.3.12) |
| GitHub token op device | Verboden |

Keuze tot nader order: **ECDSA P-256** voor manifest + image, SHA-256, monotone `security_version`. Geen productiekey in repo.

---

## 8. Nog BLOCKED / eigenaar

1. Exacte vier Modbus-acties (FC, adres, type, waarde, betekenis, readback, idempotentie).
2. Warmtepomp IP-beleid, TCP-poort, unit ID, netwerksegmentatie.
3. Beschikbare Nesso-hardware revisie en Shelly-firmwareversie.
4. Of knopuitval tijdens SoftAP-venster acceptabel is.
5. Double/hold/long: bevestig “alleen single press”.
6. Updatevenster / max bedieningsonderbreking bij OTA-reboot.

---

## 9. Verificatiestappen op hardware (niet uitgevoerd)

1. Arduino IDE 2 + core 3.3.12: board `Arduino Nesso N1`, ZCZR, `zigbee_zczr`, blink KEY1 + display.
2. Coordinator-example compileren; `Zigbee.begin(ZIGBEE_COORDINATOR)` + `openNetwork(120)`.
3. Shelly 5×-join; log IEEE uit Device Announce; vergelijk met `esp_zb_ieee_address_by_short`.
4. Knop 1–4: bewijs On/Off EP/command-mapping; test dat short-only pad geweigerd wordt.
5. Router joinen; bewijs dat router-EUI geen Modbus triggert.
6. STA + coordinator 1 uur, daarna SoftAP mét en zonder `Zigbee.stop()`.
7. OTA A/B: schrijf inactive slot, pending-verify, rollback bij geforceerde selftest-fail.
8. Meet RAM, app-grootte, neighbor-tabel, sleepy parent-failover.

---

## 10. Ontwerpkeuzes Fase 0 → scaffold

- Arduino-sketchmap `firmware/nesso_n1_bridge/` (IDE-compatibel).
- Pure logica (decoder, registry, auth, dedup, action table) host-compileerbaar.
- Hardwaremodules zijn stubs met `BLOCKED`/`TODO`.
- Geen echte Modbus-registers, geen secrets, geen CI-release, geen custom 16 MB-partitie als feit.

## 11. Provisioning / portal / OTA (eigenaarvraag 2026-11-01)

Vastgelegd in `docs/06-PROVISIONING-PORTAL-OTA.md` + CRA `docs/07-CRA-CONTROLS.md`:

| Onderdeel | Besluit | Implementatiestatus |
|---|---|---|
| BLE Wi-Fi provision | Primair; SoftAP niet primair (coexistence) | Scaffold + state machine; WiFiProv HW TODO |
| Officiële app | Espressif **ESP BLE Provisioning** (Play/App Store) | Gedocumenteerd; hardware UX-test open |
| Security | Sec0 verboden; min Sec1+PoP; doel Sec2 | Constants + code weigert Sec0 |
| Lokale portal IP/poort/unit ID | Ja, LAN-only; geen cloud | Host-testbare API + validatie; HTTP-server HW TODO |
| Vrije Modbus-registers in portal | Nee | Classificatie weigert `/api/registers` e.d. |
| OTA A/B | Ja, signed manifest, rollback | Manifest policy + state helpers host-tested; flash pad HW TODO |
| CRA dossier | SharePoint structuur + Annex I mapping | `docs/04` + `docs/07`; juridische rol open |
