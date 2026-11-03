# Partities - OTA A/B voor Nesso N1

## Productbuild (huidig)

Sketch gebruikt **`firmware/nesso_n1_bridge/partitions.csv`** = kopie van Espressif **`zigbee_zczr_8MB.csv`**.

| Partition | Rol |
|---|---|
| `otadata` | Actieve OTA-slot |
| `app0` / `ota_0` | Slot A (~3.3 MB) |
| `app1` / `ota_1` | Slot B (~3.3 MB) |
| `zb_storage` / `zb_fct` | Zigbee |
| `spiffs` / `coredump` | FS / dumps |

Nodig omdat **WiFiProv + Zigbee ZCZR + display** > 1.25 MB (te groot voor `zigbee_zczr` 4MB-tabel).

Board heeft **16 MB** flash; deze 8 MB-layout laat bovenste helft ongebruikt.

Bron:  
https://github.com/espressif/arduino-esp32/blob/master/tools/partitions/zigbee_zczr_8MB.csv

FQBN blijft `ZigbeeMode=zczr`; upload.maximum_size in build/flash scripts = `3407872`.

## Rollback

Na OTA: selftest → `esp_ota_mark_app_valid_cancel_rollback()` of rollback.  
**Geen** eFuse anti-rollback zonder herstelplan.
