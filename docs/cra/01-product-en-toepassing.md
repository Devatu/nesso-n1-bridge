# 01 – Product en toepassing

| Veld | Waarde |
|---|---|
| Document-ID | CRA-01 |
| Versie | 0.2-draft |
| Datum | 2026-11-01 |
| Status | Concept – systeemgrenzen bijgewerkt |

## Productnaam (werknaam)

**Nesso N1 Heatpump Bridge** (`nesso-n1-heatpump-bridge`)

## Korte beschrijving

Lokale bridge-firmware op Arduino **Nesso N1** (ESP32-C6) die tot vijf Shelly **BLU RC Button 4 ZB** (Zigbee) koppelt aan **één** warmtepomp via **Modbus TCP**. Installatie en beheer gebeuren lokaal (BLE Wi-Fi-provisioning + LAN-portal). Firmware-updates via outbound HTTPS met cryptografische verificatie en A/B-rollback.

## Beoogd gebruik (intended purpose)

- Professionele installatie in gebouw-/HVAC-context.  
- Zigbee-knoppen → vaste, allowlisted Modbus-acties op één LAN-warmtepomp.  
- Installateur configureert Wi-Fi (BLE) en warmtepomp IP/poort/unit ID (lokale portal).  

## Systeemgrenzen (geen fysieke samenbouw)

Drie **aparte** producten, alleen digitaal gekoppeld (zie `13-systeemgrenzen-fabrikantrollen.md`):

| Product | Fabrikant (uitgangspunt) | Conformiteit |
|---|---|---|
| Warmtepomp | **Wij** | Eigen CE/DoC |
| Shelly BLU RC Button 4 ZB | Shelly | Eigen CE/DoC |
| Arduino Nesso N1 (hardware) | Arduino S.r.l. | Eigen CE/RED/DoC |

Interfaces: Shelly↔Nesso = Zigbee; Nesso↔warmtepomp = Modbus TCP/LAN. Geen gedeelde behuizing of bedrade radio-assemblage.

## Geen beoogd gebruik / misuse-grenzen

- Geen cloud-dashboard, geen remote besturing, geen inbound WAN-poorten.  
- Geen vrij bewerkbare Modbus-registers via portal of Zigbee.  
- Geen belofte van multi-warmtepomp of willekeurige Zigbee-apparaten buiten allowlist.  
- Routers/repeaters mogen het mesh versterken maar geen Modbus-acties starten.

## Gebruikersgroepen

| Rol | Taken |
|---|---|
| Installateur | BLE Wi-Fi, portal heatpump-config, Zigbee pairing, service-reset |
| Gebouwgebruiker | Fysieke Shelly-knoppen (1–4) |
| Fabrikant/maintainer | Signed releases, vulnerability handling (**OPEN:** contact) |

## Hardware-/softwareversies die dit document raken

| Onderdeel | Vastgelegd |
|---|---|
| Board | Arduino Nesso N1, SKU TPX00227, ESP32-C6, 16 MB flash |
| Firmware product-ID | `nesso-n1-heatpump-bridge` |
| Board-ID (OTA) | `arduino_nesso_n1` |
| Shelly | SBBT-104CUS / modelId `BLU RC Button 4 ZB` (max 5) |

## Supportperiode

**OPEN – juridisch / producteigenaar.** Vastleggen vóór marktintroductie; gebruiker informeren bij naderen einde support.

## Classificatie CRA

Bridge: Nesso **ingekocht, fysiek ongewijzigd**, **eigen FW**, door ons op de markt gebracht → CRA-spoor op bridge-FW is **van ons** (`CRA-13`).  
**OPEN – juridisch:** exacte DoC-formulering bridge; PDE-classificatie warmtepomp vs. bridge; important/critical; conformity module.
