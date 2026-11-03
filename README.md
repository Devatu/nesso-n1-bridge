# Nesso N1 – Cursor ontwikkelpakket v2

Dit pakket is een **uitvoerbare ontwikkelspecificatie**, geen reeds gevalideerde firmware. Open deze map in Cursor en plak `CURSOR_STARTPROMPT.md` in Agent. Cursor moet gefaseerd implementeren, compileren en hardware-aannames expliciet verifiëren.

## Besloten productgedrag
- Arduino Nesso N1 / ESP32-C6; eigen C++-firmware, bij voorkeur bouwbaar met Arduino IDE.
- Zigbee-coördinator met maximaal **5** Shelly BLU RC Button 4 ZB eindapparaten.
- Alle 5 bedienen **dezelfde 4 acties**, via **dezelfde vaste Modbus TCP-registers** van **één warmtepomp**.
- IEEE EUI-64-identiteiten in persistente allowlist; netwerk-authenticatie is óók vereist.
- Zigbee-routers/repeaters zijn toegestaan, maar uitsluitend via tijdelijk geopende commissioning. Routers krijgen nooit bedieningsrechten. Geen vaste merk-whitelist; alleen geteste modellen officieel ondersteunen.
- Tijdelijke lokale Wi-Fi AP/portal voor installateur, met QR-code indien display dit ondersteunt; AP sluit automatisch.
- Wi-Fi STA voor Modbus LAN en uitgaande HTTPS-updatecontrole; geen cloudportaal, inbound internetpoorten of externe Modbus-bediening.
- Automatische, niet uitschakelbare controles op goedgekeurde updates bij start en elke 24 uur; veilige installatie, ondertekening, staged rollout, herstel/rollback en offline blijven functioneren.
- GitHub voor code en release-assets; SharePoint voor beheerde CRA-technische documentatie.

## Volgorde
1. Lees `docs/00-VERIFICATIE.md` en `docs/VERIFICATION_RESULTS.md`.
2. Lees `docs/01-ARCHITECTUUR.md`, `docs/02-SECURITY-OTA.md`, `docs/03-TESTPLAN.md`.
3. Start Cursor met `CURSOR_STARTPROMPT.md`.
4. Vul **geen** echte Modbus-registers, wachtwoorden, signing keys of hardwarepinnen in zonder verificatie.

Fase 0 (2026-11-01): **CONDITIONAL GO** voor scaffold + hosttests. Hardware, Modbus-registers en productie-signing blijven open.

Provisioning: **BLE Wi-Fi** via Espressif **ESP BLE Provisioning**-app (App Store / Play Store), **lokale LAN-portal** voor warmtepomp IP/poort/unit ID, **OTA A/B**.  
CRA-dossier drafts: `docs/cra/` (Annex-gerichte technische docs). Samenvatting: `docs/07-CRA-CONTROLS.md`. Geen cloudportaal; geen Security 0; geen vrije Modbus-registers. Geen juridische CRA-conformiteitsclaim.

**Nesso binnengekomen?** Start hier: [`docs/08-EERSTE-FLASH-EN-OTA.md`](docs/08-EERSTE-FLASH-EN-OTA.md) (flash, ZCZR A/B, GitHub OTA).

## Repo-opzet
De meegeleverde `firmware/`-map is een scaffold. Cursor vult de bestanden in nadat de technische haalbaarheid is vastgesteld. `docs/` beschrijft het contract. `.github/workflows/` bevat een workflow-specificatie; daadwerkelijke CI wordt pas aangezet nadat de board/buildconfiguratie werkt.
