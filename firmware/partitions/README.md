# Partities – OTA A/B voor Nesso N1

## Keuze voor eerste builds (aanbevolen)

Arduino IDE / CLI:

- **Partition Scheme:** `Zigbee ZCZR 4MB with spiffs`  
- Bestand in core: `zigbee_zczr.csv`

Inhoud (Espressif officieel):

| Partition | Rol |
|---|---|
| `nvs` | Config |
| `otadata` | Welke OTA-slot actief is |
| `app0` (`ota_0`) | Slot A |
| `app1` (`ota_1`) | Slot B |
| `spiffs` | Bestanden |
| `zb_storage` / `zb_fct` | Zigbee NVS/FAT |
| `coredump` | Crash dumps |

Slotgrootte ~**1.25 MB** per app. Board heeft **16 MB** flash; rest blijft ongebruikt.

Bron:  
https://github.com/espressif/arduino-esp32/blob/master/tools/partitions/zigbee_zczr.csv

## Als de app te groot wordt

Kopieer **ongewijzigd** uit dezelfde Espressif-repo:

- `zigbee_zczr_8MB.csv` → sketchmap als `partitions.csv`  
- Of wacht tot Nesso-boardmenu een 8/16 MB ZCZR-optie krijgt  

Geen zelfverzonnen layout zonder meting van app-grootte + `zb_storage`.

## Rollback

Officiële Arduino-ESP32-bootloader ondersteunt doorgaans `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`.  
Na OTA: selftest → `esp_ota_mark_app_valid_cancel_rollback()` of rollback.  
**Geen** eFuse anti-rollback zonder herstelplan.
