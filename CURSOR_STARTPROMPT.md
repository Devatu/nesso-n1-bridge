# OPDRACHT AAN CURSOR AGENT – NESSO N1

Je bent senior embedded engineer (ESP32-C6, Arduino, Zigbee, Modbus TCP, secure OTA) en security engineer. Bouw dit project in kleine, verifieerbare fases. Lees eerst **alle** bestanden in `docs/` en `README.md`.

## Niet onderhandelbare producteisen
1. Arduino Nesso N1 met eigen firmware; Arduino IDE compatibiliteit is vereist tenzij een gedocumenteerde technische beperking een expliciete keuze noodzakelijk maakt.
2. ESP32-C6 Zigbee coordinator; maximaal vijf Shelly BLU RC Button 4 ZB. Alle schakelaars hebben identieke knop 1–4 → actie 1–4 mapping. Slechts één vaste Modbus TCP-target en één allowlisted register/action tabel in firmware. Geen vrij invoerbare registers/waarden in de portal.
3. Ontvangende Zigbee-stack koppelt bronberichten aan geauthenticeerde IEEE EUI-64. Een short address alleen is onvoldoende. Koppel- en verwijderacties zijn alleen mogelijk in lokale installer-servicemodus.
4. Zigbee routers kunnen deelnemen tijdens gecontroleerde tijdelijke commissioning, maar mogen nooit Modbus acties initiëren. Ondersteuning en limieten hangen af van werkelijk beschikbare Zigbee-stack resources.
5. Wi-Fi configuratie via tijdelijke lokale AP met sterk per-apparaat/per-sessie geheim, beperkte duur, lokale authenticatie, QR op display alleen indien ondersteund. Na setup AP uit; STA blijft aan.
6. Automatische updatecheck bij boot en elke 24 uur met jitter/backoff, via outbound HTTPS. Alleen expliciet goedgekeurde, cryptografisch ondertekende, hardwarecompatibele firmware. OTA A/B met bootvalidatie en rollback als werkelijk ondersteund door bootloader/partitietabel. Nooit productie-flashbeveiliging of anti-rollback activeren zonder herstelplan.
7. Geen cloud dashboard, geen externe besturing, geen inbound WAN. Offline Zigbee→Modbus blijft werken. Geen blokkade van knoppen wegens ontbrekend internet.
8. GitHub Actions build/test/release; GitHub Release assets + ondertekend manifest, productie/publicatie uitsluitend via gecontroleerde approval. SharePoint CRA-dossierstructuur documenteren, niet rechtstreeks verbinden zonder expliciete opdracht.
9. Fail-safe Modbus: allowlisted acties, strikte inputvalidatie, timeouts, retries met begrenzing, geen onbedoelde herhalingen of state flips bij dubbel events. Geen log van secrets.
10. Geen gefingeerde hardwarepinnen, Shelly-clusters/endpoints, Modbus-registers, partitie-layout of API-compatibiliteit. Markeer onbekenden als BLOCKED/TODO met verificatiestappen.

## Werkwijze / fases
**Fase 0 – Feasibility:** inspecteer Nesso N1 documentatie, Arduino board package en beschikbare Zigbee coordinator API; bepaal display, inputknop, flash, partitions, bootloader rollback, Wi-Fi/Zigbee coexistence. Maak `docs/VERIFICATION_RESULTS.md` met links/versies en expliciete GO/NO-GO. Zoek Shelly Zigbee device signature en werkelijk ondersteunde button events; leg bron vast. Stel vragen alleen voor onmisbare externe gegevens (Modbus registertabel, warmtepomp-IP beleid, hardware beschikbaarheid).

**Fase 1 – Buildbare basis:** configureer reproduceerbare Arduino build (Arduino CLI of aantoonbaar Arduino IDE), `main`, modules, structured logging, config versioning, host-unit tests voor pure logica. Bouw een binary; commit geen secrets.

**Fase 2 – Modbus:** hardcoded registermapping als compile-time tabel met placeholders totdat eigenaar echte waarden bevestigt; dry-run/mock server tests; veilige commando-queue en event-deduplicatie.

**Fase 3 – Zigbee:** coordinator, veilige commissioning, EUI-64 allowlist (max 5), button-event parsing, repeater-router management, persistent storage, herstel na reboot. Test expliciet dat routers of niet-geautoriseerde end devices geen actie uitvoeren.

**Fase 4 – Provisioning:** fysieke lokale servicetrigger, tijdelijke AP, beveiligde portal, Wi-Fi credential validation en atomische opslag, QR, pairing/repeaterbeheer, sessietimeout, sluiting AP.

**Fase 5 – OTA:** signed release manifest, immutable versioned assets, staged channel, version/hardware checks, download naar inactive partition, cryptografische verificatie, reboot/selftest/commit of rollback; offline recovery. Simuleer onderbroken downloads en corrupte assets. Behoud instellingen over upgrades en rollback.

**Fase 6 – GitHub CI/CD en documentatie:** CI compileert en test. Releaseworkflow bouwt reproduceerbaar, genereert SBOM/checksums, signeert manifest met afgeschermde key, vereist approval en publiceert immutable assets. Beschrijf keyrotatie, revocation, vulnerability handling, CRA dossier.

## Output bij elke fase
- Wat is daadwerkelijk geïmplementeerd versus mock/TODO.
- Exacte build- en testcommando's, plus resultaten.
- Gewijzigde bestanden en ontwerpkeuzes.
- Risico's, open punten en volgende stap.
- Ga niet automatisch verder naar hardwaregevaarlijke handelingen of productie-signing zonder goedkeuring.

**Start nu uitsluitend met Fase 0 en de projectscaffold.**
