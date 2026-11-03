# 00 – Te verifiëren vóór productiecode

Desktoponderzoek: `docs/VERIFICATION_RESULTS.md` (2026-11-01). Hardware nog niet uitgevoerd.

| Vraag | Vereiste uitkomst | Status |
|---|---|---|
| Exacte Arduino Nesso N1 board variant / ESP32-C6 pinout | Board package, versie, Arduino IDE buildprocedure | DESKTOP OK — FQBN `esp32:esp32:arduino_nesso_n1`, core ≥ 3.3.5, library `Arduino_Nesso_N1` 1.0.0. Hardwarecompile open. |
| Flashcapaciteit en actuele partities | OTA slots, NVS, recovery; daadwerkelijke partitietabel | DEELS — 16 MB flash. Nesso-menu heeft alleen ZCZR 4 MB-layout (`zigbee_zczr.csv` met A/B 1.25 MB). 16 MB-ZCZR-menu ontbreekt. |
| ESP32-C6 Arduino Zigbee coordinator ondersteuning | Werkende stack/API, versie, max devices/routers | CONDITIONAL — ZCZR in boards.txt + `Zigbee.begin(ZIGBEE_COORDINATOR)`. Officiële Nesso-docs tonen alleen ED. Max routers ongemeten. |
| Arduino-ESP32 rollback en bootloader configuratie | Werkelijk werkende esp_ota_* state transitions | OPEN op hardware — core claimt rollback-bootloader; niet op Nesso bewezen. |
| Nesso display en fysieke input | Bibliotheken, pinnen, QR-rendering, serviceknop | DESKTOP OK — ST7789 135×240, KEY1/KEY2 via expander. QR via M5GFX, niet officieel gedocumenteerd. Powerknop is geen service-GPIO. |
| Wi-Fi STA + SoftAP + Zigbee coexistence | Hardwaretest incl. bereik, latency, stabiliteit | RISK — Espressif: STA+router C1; SoftAP+router X. Mitigatie: Zigbee pauzeren tijdens AP. Hardwaretest verplicht. |
| Shelly BLU RC Button 4 ZB Zigbee mode | Exacte endpoints, clusters, event encoding, pairingprocedure, firmwareversie | DESKTOP OK (encoding/pairing) — SBBT-104CUS, modelId `BLU RC Button 4 ZB`. Firmwareversie testhardware onbekend. |
| Shelly batterijgedrag en sleepy end-device | Parent/router failover, berichtverlies, debounce | OPEN — geen hardware. |
| Modbus registers | Adres, function code, datatype, write value, veiligheid, idempotentie | WACHT OP EIGENAAR |
| Warmtepomp IP/poort/unit ID | Installatiebeleid, statisch/DHCP, Modbus server, authenticatie/netwerksegmentatie | WACHT OP EIGENAAR |
| Firmware signing | Ed25519/ECDSA library en verifier op target, protected key management | CONDITIONAL — Arduino `Update` + RSA/ECDSA. Ed25519 via mbedTLS niet beschikbaar. Geen productiekey. |
| Product/releasebeleid | Supportperiode, updatevensters, staged rollout, herstelprocedure | OPEN |

Geen van deze open punten mag als feit worden ingevuld op basis van aannames. Bij een ongeschikte Arduino Zigbee API: documenteer alternatieven en trade-offs, vraag akkoord voor frameworkwijziging.

**Fase 0-besluit:** CONDITIONAL GO voor scaffold + hosttests. Zie `docs/VERIFICATION_RESULTS.md`.
