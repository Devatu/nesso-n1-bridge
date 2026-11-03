---
title: "Nesso N1 Heatpump Bridge — CRA technisch dossier"
subtitle: "Annex I / VII gerichte conceptdocumentatie (draft)"
author: "Nesso N1 project"
date: "2026-11-01"
---

# Disclaimer

Dit document is **conceptuele technische documentatie** ter ondersteuning van een CRA-dossier.

Het is **geen** EU-verklaring van overeenstemming en vormt **geen** juridische claim dat het product “CRA-conform” is.

Arduino CE/RED-documenten voor TPX00227 zijn **leveranciersbewijs** voor het ongewijzigde bord. Warmtepomp en Shelly hebben eigen DoC’s. De bridge-firmware is ons CRA-spoor.

\newpage


\newpage


# CRA-dossier (werkmap in Git)

**Status:** conceptuele technische documentatie voor Annex VII / Annex I.  
**Geen** EU-conformiteitsverklaring, **geen** juridische “CRA-conform”-claim.

Deze map is de Git-bron voor het SharePoint-dossier (`docs/04-CRA-SHAREPOINT.md`). Na review kopiëren/linken naar SharePoint met versie + goedkeurder.

## Leesbare export

| Bestand | Pad |
|---|---|
| Word | `docs/cra/export/CRA-dossier-Nesso-N1.docx` |
| PDF | `docs/cra/export/CRA-dossier-Nesso-N1.pdf` |

Opnieuw genereren:

```powershell
pandoc docs/cra/export/CRA-dossier-Nesso-N1.md -o docs/cra/export/CRA-dossier-Nesso-N1.docx --toc --toc-depth=2
python scripts/export_cra_pdf.py
```

| # | Document | Annex VII / I | Nu vulbaar? |
|---|---|---|---|
| 01 | [Product en toepassing](01-product-en-toepassing.md) | VII productbeschrijving | Ja (concept) |
| 02 | [Architectuur en interfaces](02-architectuur-en-interfaces.md) | VII ontwerp/werking | Ja |
| 03 | [Componenteninventaris](03-componenteninventaris.md) | VII + SBOM-voorloper | Deels (top-level) |
| 04 | [Risicoanalyse en threat model](04-risicoanalyse-threat-model.md) | Art. 13 / Annex I basis | Ja (eerste iteratie) |
| 05 | [Annex I controlematrix](05-annex-I-controlematrix.md) | Annex I Part I+II | Ja (mapping) |
| 06 | [Secure development](06-secure-development.md) | VII secure development | Ja (procesontwerp) |
| 07 | [Update- en releasebeleid](07-update-en-releasebeleid.md) | Annex I updates | Ja |
| 08 | [Vulnerability handling (CVD)](08-vulnerability-handling.md) | Annex I Part II | Ja (beleidstemplate) |
| 09 | [Gebruikers- en installatie-informatie](09-gebruikersinformatie.md) | VII gebruikersinfo | Ja (concept) |
| 10 | [Change control](10-change-control.md) | Traceerbaarheid | Ja (template) |
| 11 | [Technische documentatie-checklist](11-technische-documentatie-checklist.md) | Annex VII checklist | Ja |
| 12 | [Hardwarecertificaten Arduino TPX00227](12-hardware-certificaten-arduino.md) | Leveranciers CE/RED/FCC/… | Ja (gecatalogiseerd) |
| 13 | [Systeemgrenzen en fabrikantrollen](13-systeemgrenzen-fabrikantrollen.md) | Wie DoC/CRA voor welk product | Ja (eigenaarinput) |

## Nog niet bouwen als “af”

- EU-verklaring van overeenstemming  
- Definitieve fabrikantrol / supportperiode / conformity assessment module  
- Volledige machine-readable SBOM uit CI-build  
- Hardware-testbewijzen en pen-test rapporten  
- Incidentlog met echte cases  

Die volgen na juridische input en hardwarevalidatie.


\newpage


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


\newpage


# 02 – Architectuur en interfaces

| Veld | Waarde |
|---|---|
| Document-ID | CRA-02 |
| Versie | 0.1-draft |
| Datum | 2026-11-01 |
| Status | Concept |
| Bronnen | `docs/01-ARCHITECTUUR.md`, `docs/06-PROVISIONING-PORTAL-OTA.md` |

## Datastroom (runtime)

```
Shelly BLU RC Button 4 ZB (≤5, sleepy ED)
        │ Zigbee (evt. via routers)
        ▼
Nesso N1 Zigbee coordinator
        │ authenticate EUI-64 allowlist
        │ decode button → ACTION_1..4
        │ dedup / rate limit
        ▼
Modbus TCP client ──► 1 warmtepomp (IP/port/unit uit NVS; registers compile-time)
```

## Trust boundaries

| Zone | Inhoud | Vertrouwen |
|---|---|---|
| Zigbee mesh | Shelly + optionele routers | Alleen allowlisted EUI-64 mag acties; short address alleen onvoldoende |
| LAN | STA, lokale HTTP-portal, Modbus | Geen inbound WAN; portal auth + CSRF |
| BLE (tijdelijk) | Wi-Fi provisioning | Alleen service/fysiek; Sec≠0 + PoP; daarna BLE uit |
| Internet (outbound) | HTTPS update-check | Signed manifest; geen device GitHub-token |
| Device NVS | Wi-Fi, heatpump, registry | Atomische writes; geen secrets in logs |

## Externe interfaces

| Interface | Richting | Protocol | Doel |
|---|---|---|---|
| Zigbee 802.15.4 | in/out | ZCL On/Off, Level, Scenes | Knop-events |
| Wi-Fi STA | out/in LAN | IP | Modbus + portal + OTA check |
| BLE | tijdelijk | Espressif wifi_prov | Wi-Fi credentials |
| Modbus TCP | out | FC allowlist | Warmtepomp-acties |
| HTTPS | out | TLS | OTA manifest/assets |
| USB | service | CDC | Debug/flash (productie: beperkt) |
| Display/KEY1/KEY2 | lokaal | UI | PoP, status, service |

## Wat bewust ontbreekt (attack surface)

- Cloud API / remote dashboard  
- Inbound port forwarding-aanname  
- SoftAP als primaire provision (Espressif SoftAP+Zigbee-router = X)  
- Portal-registereditor  

## Secure defaults (ontwerp)

- Zigbee join standaard dicht; open alleen ≤120 s in service.  
- BLE provision alleen unprovisioned of na fysieke KEY.  
- OTA-security-check niet permanent uitschakelbaar.  
- Onbekende EUI / router → geen Modbus.


\newpage


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


\newpage


# 04 – Risicoanalyse en threat model (eerste iteratie)

| Veld | Waarde |
|---|---|
| Document-ID | CRA-04 |
| Versie | 0.1-draft |
| Datum | 2026-11-01 |
| Status | Eerste technische iteratie – geen formele risk acceptance |

Methode: assets → dreigingen → impact → bestaande/mitigaties → restantisico.  
Formele goedkeuring door producteigenaar/security officer: **OPEN**.

## Assets

| ID | Asset | Vertrouwelijkheid | Integriteit | Beschikbaarheid |
|---|---|---|---|---|
| A1 | Wi-Fi credentials | H | H | M |
| A2 | Heatpump IP/port/unit + Modbus pad | M | H | H |
| A3 | Shelly EUI allowlist / Zigbee netwerkkey | H | H | H |
| A4 | Firmware image / bootketen | M | H | H |
| A5 | OTA signing private key (CI) | H | H | M |
| A6 | PoP tijdens provision | H | H | L |

## Dreigingen en mitigaties

| ID | Dreiging | Asset | Mitigatie (ontwerp) | Restantisico |
|---|---|---|---|---|
| T1 | Wi-Fi credentials snifferen tijdens BLE | A1 | Sec≠0, PoP, timeout, fysieke start | M tot Sec2 bewezen |
| T2 | Vreemde Zigbee knop stuurt Modbus | A2/A3 | EUI-64 allowlist; max 5; routers geen actie | L–M (HW test open) |
| T3 | Short-address spoof / rejoin | A3 | Weiger zonder IEEE-resolutie | L–M |
| T4 | LAN portal zonder auth / CSRF | A2 | Sessies, CSRF, LAN-only, timeout | M tot HW portal live |
| T5 | Registermanipulatie via portal | A2 | Geen register-API | L |
| T6 | Malicious OTA / downgrade | A4 | HTTPS, signature, security_version, A/B rollback | M tot signing live |
| T7 | SoftAP+Zigbee RF-storing / misconfig | A3 | SoftAP niet primair; Zigbee pause bij BLE | L |
| T8 | Secrets in logs | A1/A6 | Verbod in code/policy | L |
| T9 | Supply-chain dependency vuln | A4 | SBOM, updates, CI scanning **TODO** | M |
| T10 | Fysieke diefstal apparaat | A1–A3 | Lokale scope; factory reset; **geen** Secure Boot nog | M (bewust) |
| T11 | Update-loop / brick | A4 | Rollback, markeer mislukte versie | L–M |
| T12 | Internet down blokkeert knoppen | A2 | Offline Zigbee→Modbus verplicht | L |

## Risicoacceptatie

Nog niet getekend. Aanbevolen na: hardware Zigbee/BLE-tests, portal-hardening review, eerste signed OTA dry-run.

## Koppeling Annex I

Zie `05-annex-I-controlematrix.md`.


\newpage


# 05 – Annex I controlematrix

| Veld | Waarde |
|---|---|
| Document-ID | CRA-05 |
| Versie | 0.1-draft |
| Datum | 2026-11-01 |
| Status | Mapping – implementatie deels scaffold |

Legenda status: **D** = ontwerp vastgelegd · **S** = scaffold/code · **T** = getest (host/HW) · **O** = open

## Part I – Essential cybersecurity requirements

| Eis (samenvatting) | Controle | Bewijs | Status |
|---|---|---|---|
| Passend niveau o.b.v. risico | CRA-04 threat model | dit dossier | D |
| Geen bekende exploiteerbare vulns bij release | Release gate + SBOM + scan | CI **TODO** | O |
| Secure by default | Join dicht; BLE fysiek; geen Sec0 | docs/06, code constants | D/S |
| Reset naar oorspronkelijke veilige staat | Factory/service reset procedure | **TODO** procedure + code | O |
| Security updates / secure distributie | Signed OTA A/B, check boot+24u | docs/02, ota_* | D/S |
| Opt-out / postpone nuance | Geen permanente OTA-disable; postpone tijdens Modbus | docs/07-CRA-CONTROLS | D |
| Onbevoegde toegang | PoP, portal auth, EUI allowlist | docs/06, authorization_* | D/S/T(host) |
| Vertrouwelijkheid | Geen secrets in logs; TLS OTA | policy + code comments | D |
| Integriteit | Dedup, allowlist registers, image hash/sign | action_*, ota_manifest | D/S/T(host) |
| Minimaliseer aanvalsoppervlak | Geen cloud/inbound WAN | architectuur | D |
| Beperk incidentimpact | Fail-safe Modbus, rollback, offline knoppen | architectuur | D |
| Logging passend | Lokale logs, geen extern PII-dashboard | telemetry ontwerp | D |
| Secure deletion | Wis NVS credentials/registry | **TODO** | O |

## Part II – Vulnerability handling

| Eis | Controle | Status |
|---|---|---|
| SBOM machine-readable | CI CycloneDX/SPDX | O (voorloper CRA-03) |
| Remediëren zonder onnodige vertraging | CVD + OTA patch SLA | D (template CRA-08) |
| Security tests | Hosttests nu; HW/CI later | S/T(host) |
| Publicatie fixed vulns | Release notes / advisory | O |
| CVD-beleid | CRA-08 | D |
| Security contact | **OPEN** eigenaar | O |
| Secure update distributie | HTTPS + sign + A/B | D/S |
| Security updates gratis | Beleid: ja voor security channel | D |


\newpage


# 06 – Secure development en brontracering

| Veld | Waarde |
|---|---|
| Document-ID | CRA-06 |
| Versie | 0.1-draft |
| Datum | 2026-11-01 |
| Status | Procesontwerp |

## Bron van waarheid

| Artefact | Locatie |
|---|---|
| Source | GitHub (of lokale Git tot remote) |
| Releases / binaries | GitHub Release assets (immutable) |
| CRA-dossier kopieën | SharePoint (`docs/04-CRA-SHAREPOINT.md`) |
| Signing keys | CI/KMS – **nooit** in repo of device |

## Ontwikkelprincipes

1. Geen geheimen in repo (`.gitignore`: `.env`, keys, pem).  
2. Geen gefingeerde hardware-/registerfeiten; BLOCKED/TODO markeren.  
3. Host-unit tests voor pure logica vóór hardware.  
4. PR-review vóór `main`.  
5. Release alleen via protected environment + menselijke approval.  
6. Security updates bij voorkeur scheidbaar via `security_version`.

## Geplande CI-gates (nog niet live)

- Compile (Arduino FQBN Nesso + ZCZR)  
- Host unit tests  
- Secret scanning  
- Static analysis (basis)  
- SBOM generatie  
- Manifest sign (protected)

## Traceerbaarheid

Elke release: git tag ↔ binary SHA-256 ↔ signed manifest ↔ SBOM ↔ release notes ↔ hardware compatibility.


\newpage


# 07 – Update- en releasebeleid

| Veld | Waarde |
|---|---|
| Document-ID | CRA-07 |
| Versie | 0.1-draft |
| Datum | 2026-11-01 |
| Status | Concept – aligned met producteis OTA |
| Bronnen | `docs/02-SECURITY-OTA.md`, `docs/06-PROVISIONING-PORTAL-OTA.md` |

## Doelen (CRA)

- Security updates veilig, tijdig, gratis distributie.  
- Integriteit en authenticiteit van updates.  
- Beschikbaarheid: offline knoppen blijven werken.  
- Herstel bij mislukte update (A/B rollback).

## Devicegedrag

| Moment | Actie |
|---|---|
| Boot (als STA up) | OTA-check |
| Elke 24 u ± jitter | OTA-check |
| Offline / TLS-fout | Geen brick; status “laatst gecontroleerd”; Zigbee/Modbus door |
| Actieve Modbus-transactie | Update uitstellen tot queue veilig |
| Permanente disable security-check | **Niet toegestaan** |

## Trust chain

1. HTTPS + servercertificaatvalidatie  
2. Signed manifest (ECDSA-P256 of RSA; niet Ed25519/mbedTLS)  
3. Checks: product, board `arduino_nesso_n1`, channel, monotone `security_version`, size, SHA-256  
4. Download naar **inactive** OTA-slot  
5. Image-verificatie  
6. Reboot → PENDING_VERIFY selftest → commit of rollback  

## Releasekanalen

| Channel | Gebruik |
|---|---|
| `stable` | Productie (device default) |
| staged/canary | Intern vóór stable (**proces**) |

Geen blind vertrouwen op “latest” zonder signed manifest.

## Key management

| Key | Opslag | Rotatie |
|---|---|---|
| OTA signing private | CI/KMS protected env | Procedure **TODO** |
| OTA public | Embedded in firmware | Bij rotatie: dual-key window **TODO** |
| Revocation | Manifest/policy + non-install van mislukte versies | **TODO** detail |

## eFuse / Secure Boot

Niet activeren zonder manufacturing-herstelplan en backup keys (producteis).


\newpage


# 08 – Vulnerability handling (CVD-beleid)

| Veld | Waarde |
|---|---|
| Document-ID | CRA-08 |
| Versie | 0.1-draft |
| Datum | 2026-11-01 |
| Status | Beleidstemplate – contact nog OPEN |

## Security contact

| Veld | Waarde |
|---|---|
| E-mail | **OPEN – producteigenaar** (bijv. security@…) |
| PGP/key | Optioneel **TODO** |
| Response-doel (intake) | 5 werkdagen bevestiging (**concept-SLA**) |

## Coordinated Vulnerability Disclosure

1. Melder stuurt beschrijving, getroffen versie, reproduceerstappen (geen publieke 0-day dump zonder coördinatie).  
2. Triage: severity (CVSS of interne schaal), exploitability, impact op A1–A6.  
3. Remediatie: patch → signed security release → advisory.  
4. Publicatie: na patch-beschikbaarheid; uitstel alleen als publicatierisico > voordeel (CRA-uitzondering).  
5. Credit aan melder indien gewenst.

## Interne intake

| Stap | Owner | Artefact |
|---|---|---|
| Registratie | Security contact | Incident/vuln log (SharePoint 08_) |
| Triage | Engineering + security | Severity, affected SBOM components |
| Fix | Engineering | PR + tests |
| Release | Release manager | Signed assets + notes |
| Notify | Product | Installateurs / klanten naar beleid |

## Scope

- Eigen firmware `nesso_n1_bridge`  
- Integratiekeuzes (BLE provision, portal, Zigbee allowlist, OTA)  
- Bekende issues in third-party components → upstream + mitigatie/update

## Wat dit document nog nodig heeft

- Vast security@-adres  
- Goedgekeurde SLA’s  
- Publieke CVD-pagina of SECURITY.md in repo  
- Koppeling aan CRA-rapportageplichten (juridisch, vanaf toepasselijke data)


\newpage


# 09 – Gebruikers- en installatie-informatie (concept)

| Veld | Waarde |
|---|---|
| Document-ID | CRA-09 |
| Versie | 0.1-draft |
| Datum | 2026-11-01 |
| Status | Concept handleiding – hardware UX nog te valideren |

## Veiligheids- en privacykenmerken (kort)

- Lokale installatie; geen cloud-bediening.  
- Wi-Fi via officiële **ESP BLE Provisioning**-app + code (PoP) op het scherm.  
- Automatische controle op goedgekeurde beveiligingsupdates; knoppen werken ook zonder internet.  
- Alleen gekoppelde Shelly-knoppen bedienen de warmtepomp.

## Benodigdheden

- Arduino Nesso N1 met deze firmware  
- Tot 5× Shelly BLU RC Button 4 ZB  
- Wi-Fi 2.4 GHz  
- Telefoon met [ESP BLE Provisioning (Android)](https://play.google.com/store/apps/details?id=com.espressif.provble) of [iOS](https://apps.apple.com/us/app/esp-ble-provisioning/id1473590141)  
- Warmtepomp bereikbaar via Modbus TCP op LAN  

## Wi-Fi instellen (eenmalig)

1. Zet Nesso aan. Bij eerste start (of na service-knop) verschijnt BLE-setup.  
2. Noteer **apparaatnaam** en **PoP** op het display (of QR).  
3. Open ESP BLE Provisioning → kies het apparaat → voer PoP in.  
4. Selecteer Wi-Fi-netwerk en wachtwoord.  
5. Bij succes verdwijnt BLE-setup; Nesso verbindt als station.  
6. Zigbee-knoppen zijn tijdens BLE-setup tijdelijk niet beschikbaar.

## Warmtepomp koppelen (lokale portal)

1. Zoek het IP van de Nesso op je LAN (router/DHCP).  
2. Open `http://<nesso-ip>/` op een LAN-PC.  
3. Log in (lokale credentials – **TODO** productiewaarden).  
4. Vul warmtepomp **IPv4**, **TCP-poort**, **unit ID** in.  
5. Modbus-registers zijn vast in firmware; niet via portal wijzigbaar.

## Shelly koppelen

1. Activeer service/pairing (portal of KEY – **TODO** exacte UI).  
2. Op Shelly: 5× knop voor Zigbee join (zie Shelly-docs).  
3. Bevestig EUI in allowlist (max 5).  
4. Sluit pairingvenster (≤120 s).

## Updates

Het apparaat zoekt periodiek naar goedgekeurde updates. Bij installatie kan een korte herstart volgen; knopbediening hervat na boot. Zonder internet blijven bestaande knopfuncties werken.

## Support einde

Bij naderen einde van de supportperiode informeert de fabrikant de gebruiker (**OPEN:** kanalen en periode).


\newpage


# 10 – Change control (template)

| Veld | Waarde |
|---|---|
| Document-ID | CRA-10 |
| Versie | 0.1-draft |
| Datum | 2026-11-01 |
| Status | Template |

## Wanneer change control

- Firmware releases (feature of security)  
- Wijziging trust boundaries (BLE/portal/OTA/Zigbee/Modbus)  
- Dependency upgrades met security-impact  
- Signing key rotatie  
- Product intended-use wijziging  

## Change record (kopieer per wijziging)

| Veld | Invullen |
|---|---|
| Change-ID | CR-YYYY-NNN |
| Datum | |
| Indiener | |
| Samenvatting | |
| Risico-impact (CRA-04) | nieuw / ongewijzigd / herzien |
| Annex I-impact | |
| Testbewijs | host / HW / OTA dry-run |
| SBOM bijgewerkt | ja/nee |
| Goedkeurder | |
| Release-tag | |

## Goedkeuringsmatrix (voorstel)

| Type | Review code | Security | Producteigenaar |
|---|---|---|---|
| Security patch | verplicht | verplicht | geïnformeerd |
| Feature release | verplicht | bij interface-wijziging | verplicht |
| Key rotatie | verplicht | verplicht | verplicht |


\newpage


# 11 – Technische documentatie-checklist (Annex VII-achtig)

| Veld | Waarde |
|---|---|
| Document-ID | CRA-11 |
| Versie | 0.1-draft |
| Datum | 2026-11-01 |
| Status | Checklist – geen afvink-claim van volledige CRA-conformiteit |

Gebaseerd op typische Annex VII-elementen (productbeschrijving, ontwerp, risicoanalyse, SBOM/vuln-handling, tests, DoC). Juridische volledigheid: **OPEN**.

| # | Vereiste (samengevat) | Onze artefact | Status |
|---|---|---|---|
| 1 | Algemene productbeschrijving, intended purpose, versies | CRA-01 | Draft |
| 2 | Begrijpen van ontwerp/ontwikkeling/werking | CRA-02, docs/01, docs/06 | Draft |
| 3 | Foto’s/illustraties HW (indien van toepassing) | SharePoint **TODO** | Open |
| 4 | Cybersecurity risk assessment | CRA-04 | Eerste iteratie |
| 5 | Annex I mapping / oplossingen | CRA-05, docs/07-CRA-CONTROLS | Draft |
| 6 | Supportperiode-onderbouwing | CRA-01 veld | **Juridisch open** |
| 7 | Toegepaste standaarden/specs | **TODO** (bijv. later EN-normen) | Open |
| 8 | Test- en verificatiebewijzen | Hosttests; HW **TODO**; docs/03-TESTPLAN | Deels |
| 9 | SBOM machine-readable | CRA-03 voorloper; CI **TODO** | Deels |
| 10 | Vulnerability handling info + contact | CRA-08 | Template (contact open) |
| 11 | Secure update distributie beschreven | CRA-07 | Draft |
| 12 | Gebruikersinformatie | CRA-09 | Concept |
| 13 | EU-verklaring van overeenstemming | — | **Niet nu** |
| 14 | Change control / goedkeuringen | CRA-10 | Template |

## Wat je nu wél kunt claimen intern

“We hebben een **CRA-gerichte technische dossierbasis** in Git (`docs/cra/`), gekoppeld aan productarchitectuur, updatepad, provisioning en vulnerability-proces.”

## Wat je níet kunt claimen

“Dit product is CRA-conform / CE-CRA gereed” — zolang fabrikantrol, supportperiode, SBOM-uit-CI, HW-tests, DoC en eventuele conformity assessment open staan.


\newpage


# 12 – Hardwarecertificaten Arduino Nesso N1 (TPX00227)

| Veld | Waarde |
|---|---|
| Document-ID | CRA-12 |
| Versie | 0.2-draft |
| Datum | 2026-11-01 |
| Status | Leveranciersbewijs; Zigbee = beoogd productgebruik Arduino |

## Bronbestanden (in `docs/`)

| Bestand | Type | Wat het is |
|---|---|---|
| `Arduino_TPX00227-DoC_CE.pdf` | EU DoC | Arduino S.r.l. verklaart Nesso N1 TPX00227 conform RED 2014/53/EU + RoHS (gedateerd 11/11/2025, Fabio Violante) |
| `Arduino_TPX00227-CERT_CE_RED.pdf` | EU-type examination | Eurofins NB 0980, reg. **4243-09-2025-250586**, Module B RED, 29/09/2025 – Safety/Health/EMC/Radio = Conform |
| `Arduino_TPX00227-DoC_UKCA.pdf` | UKCA DoC | Radio Equipment Regulations 2017 + UK RoHS |
| `Arduino_TPX00227-CERT_UKCA.pdf` | UKCA cert | Certificaat UKCA |
| `Arduino_TPX00227-DoC_FCC.pdf` | FCC DoC | FCC-verklaring |
| `Arduino_TPX00227-CERT_FCC_DTS.pdf` | FCC grant | Digital Transmission System |
| `Arduino_TPX00227-CERT_FCC_DSS.pdf` | FCC grant | Spread Spectrum |
| `Arduino_TPX00227-CERT_CE_ISED.pdf` | ISED Canada | Grant, RSS-247/Gen/102, cert aan Arduino S.r.l., 30/09/2025 – “Arduino Nesso N1 with WIFI and Lora” |
| `Arduino_TPX00227-CERT_MIC.pdf` | Japan MIC | Radio certificaat |

**Certificate holder / DoC-ondertekenaar:** Arduino S.r.l., Via Andrea Appiani 25, 20900 Monza (Italy) — **niet** de maker van de heatpump-bridge firmware/eindtoepassing.

## Wat de RED-annex expliciet noemt (Eurofins)

| Item | Waarde uit certificaat |
|---|---|
| Model | TPX00227 / Arduino Nesso N1 |
| HW/SW version in cert | N/A |
| Frequenties | 2.4G Wi‑Fi 2412–2472 MHz; BLE 2402–2480 MHz; LoRa 868–868.6 MHz |
| Antennes | 2.4G Wi‑Fi/BT: FPC; LoRa: externe antenne |
| Normen (DoC) | o.a. EN IEC 62368-1, EN 301 489-x, EN 300 328, EN 300 220-x, RoHS IEC 62321-… |

### Zigbee is beoogd gebruik van dit CE/RED-product

Arduino verkoopt TPX00227 in de EU (store.arduino.cc) en promoot expliciet Zigbee:

- Store tech specs: **Zigbee 3.0** naast Wi‑Fi 6, BLE 5.3, Thread 1.4, LoRa  
- Productdocs/datasheet: **802.15.4 (Thread/Zigbee®)** op ESP32-C6 + FPC-antenne  
- Conformities op store: CE, FCC, IC, UKCA, RoHS, …  

Dat Zigbee niet als apart woord in de Eurofins-annexregel staat (daar: Wi‑Fi / BLE / LoRa) betekent **niet** dat Zigbee “buiten certificaat” valt. De 2.4 GHz-radio (zelfde FPC-pad) valt onder **EN 300 328** (wideband transmission); Zigbee/802.15.4 deelt die band met Wi‑Fi/BLE. Voor het **bord als Arduino-product** is Zigbee dus onderdeel van het gepromote, in de EU verkochte intended use.

Voor **onze** firmware blijft relevant: antenne niet wijzigen, radiovermogen/kanalen binnen platform, LoRa alleen gebruiken zoals gecertificeerd (of uit laten). Gelijktijdige radio’s (Zigbee + Wi‑Fi) zijn op C6 coexistence-technisch beperkt — dat is vooral een **productontwerp-/stabiliteitspunt**, geen argument dat Arduino Zigbee niet zou mogen verkopen.

## Correcte lezing in *dit* systeem (eigenaar 2026-11-01)

Zie ook `13-systeemgrenzen-fabrikantrollen.md`.

- Warmtepomp, Shelly en Nesso-bord zijn **drie CE-producten**, **niet fysiek** samengevoegd; alleen Zigbee + Modbus TCP.  
- **Wij** = fabrikant warmtepomp. **Shelly** = knoppen. **Arduino** = Nesso-hardware (incl. Zigbee intended use).  
- Arduino-DoC dekt het **bord**, niet de warmtepomp en niet Shelly — en hoeft dat ook niet: die hebben eigen DoC’s.

### Wat wél geldt

1. Arduino CE/RED = leveranciersbewijs voor TPX00227 (radio/safety, Zigbee bedoeld).  
2. Digitale-only koppeling ≠ één nieuw samengesteld radioapparaat vanzelf.  
3. Eigen FW op Nesso is Arduino-ecosysteem-conform; antenne/RF binnen boardontwerp houden.

### Wat níet automatisch geldt

| Claim | Beoordeling |
|---|---|
| “Drie DoC’s samen = klaar voor alles” | CE/RED per product ja als uitgangspunt; **CRA** en eventuele rol als fabrikant van een *geprogrammeerde bridge* nog apart. |
| “Zigbee gebruiken mag niet onder RED” | **Onjuist** (Arduino EU-positionering). |
| “CE/RED ⇒ CRA geregeld” | **Nee.** |

## Aanbevolen dossierpositie (technisch)

1. Archiveer Arduino-PDF’s + Shelly DoC/CE + warmtepomp DoC/CE in SharePoint `03_`.  
2. Architectuur: “no physical interconnection”.  
3. Jurist: scenario A/B/C voor bridge-FW (`CRA-13`).  
4. CRA-spoor voor PDE’s die **wij** op de markt brengen blijft actief.

## Checksums (lokaal, 2026-11-01)

Gegenereerd bij catalogisering; opnieuw berekenen bij file-wijziging.

Zie script of handmatig: `Get-FileHash docs\Arduino_TPX00227-*.pdf`.

## OPEN

- Exacte FCC ID / ISED certification number uit grants.  
- Jurist: DoC-formulering “unmodified TPX00227 + our firmware” (`CRA-13` vastgelegd).  
- Shelly DoC/CE-bestanden in repo/SharePoint toevoegen.  
- Of LoRa unused/powered down in deze toepassing.


\newpage


# 13 – Systeemgrenzen en fabrikantrollen

| Veld | Waarde |
|---|---|
| Document-ID | CRA-13 |
| Versie | 0.2-draft |
| Datum | 2026-11-01 |
| Status | Scenario bridge vastgelegd – juridische DoC-tekst nog reviewen |

## Feiten (eigenaar)

1. **Wij** zijn de fabrikant van de **warmtepomp** (eigen CE/DoC voor dat product).  
2. **Shelly** is fabrikant van de **BLU RC Button 4 ZB** (eigen CE/DoC).  
3. **Arduino** is fabrikant van het **Nesso N1-bord TPX00227** (eigen CE/RED/DoC; Zigbee is gepromoot intended use).  
4. De drie apparaten zijn **niet fysiek** tot één toestel samengesteld. Koppeling is **louter digitaal**:
   - Shelly ↔ Nesso: **Zigbee** (draadloos)
   - Nesso ↔ warmtepomp: **Modbus TCP** over **LAN/Wi‑Fi** (IP)

```
[Shelly knoppen] --Zigbee--> [Nesso N1 + bridge-FW] --Modbus TCP/LAN--> [Warmtepomp]
   Shelly CE/DoC              Arduino bord CE/RED          Warmtepomp CE/DoC
                              (+ FW: zie rollen hieronder)
```

Dit is een **systeem van interoperabele, afzonderlijk op de markt gebrachte producten**, geen mechanisch geïntegreerde radio-assemblage.

## Gevolg voor CE/RED-denken

| Product | Wie tekent DoC (uitgangspunt) | Wat dekt het |
|---|---|---|
| Warmtepomp | **Wij** | Warmtepomp als product |
| Shelly BLU RC Button 4 ZB | **Shelly** | Knoppen als product |
| Arduino Nesso N1 hardware | **Arduino S.r.l.** | Bord TPX00227 incl. radio’s (Wi‑Fi/BLE/802.15.4/LoRa volgens hun dossier) |

Digitale koppeling alleen maakt **niet** automatisch één nieuw “samengesteld radioapparaat” dat alle drie DoC’s vervangt. Elke partij blijft verantwoordelijk voor **zijn** product zoals het op de markt wordt gebracht.

## Firmware op de Nesso – vastgelegd (eigenaar)

| Keuze | Vastgelegd |
|---|---|
| Hardware | Nesso N1 **ingekocht** bij Arduino; **fysiek ongewijzigd** (geen antenne-/RF-/behuizingwijziging) |
| Software | **Eigen** firmware `nesso-n1-heatpump-bridge` geflasht |
| Marktplaatsing | Wij brengen de geprogrammeerde Nesso als onderdeel van ons aanbod / systeem (bij warmtepomp) op de markt |

### Leeswijzer CE/RED vs CRA

| Laag | Wie / wat |
|---|---|
| Radio-/veiligheidshardware TPX00227 | Arduino CE/RED/DoC als **component-/basisproduct**; fysiek origineel houden ondersteunt hergebruik van dat bewijs |
| Product met digitale elementen “bridge” (Nesso + onze FW) | **Wij** zijn in de praktijk de partij die dit PDE op de markt brengt → **eigen** verantwoordelijkheid voor FW, updates, CRA Annex I/II, en doorgaans een **eigen DoC** (of juridisch goedgekeurde integratietekst die Arduino-DoC + onze FW-verklaring combineert) |
| Warmtepomp | Onze bestaande CE/DoC |
| Shelly | Shelly CE/DoC |

Fysiek ongewijzigd ≠ “geen fabrikantrol meer”. Het betekent: **geen nieuwe RF-hardwarekeuring verwacht** zolang radio/antenne/vermogen binnen Arduino-ontwerp blijven; wél verantwoordelijkheid voor **softwaregedrag** (Zigbee coordinator, Wi‑Fi, BLE provision, OTA, Modbus).

### Wat we wél / niet claimen

| Claim | Status |
|---|---|
| Arduino-bord blijft CE/RED-gedekt als hardwarebasis | Ondersteund (ongewijzigd + Zigbee intended use) |
| Shelly / warmtepomp houden eigen DoC | Ja |
| Onze FW + marktplaatsing bridge valt volledig onder alleen Arduino-DoC | **Nee** – te ruim; jurist voor DoC-formulering |
| CRA voor bridge-FW is ons spoor | **Ja** (`docs/cra/`) |

## CRA (los van CE/RED)

CRA geldt per **product with digital elements** dat wij als fabrikant op de markt brengen.

- Warmtepomp (als PDE / connected product): ons CRA/CE-spoor voor de warmtepomp.  
- Shelly: Shelly’s verantwoordelijkheid.  
- Bridge-FW / geprogrammeerde Nesso: hangt af van scenario A/B/C hierboven. Het CRA-dossier in `docs/cra/` is vooral gericht op de **bridge-firmwarefunctionaliteit**.

Digitale-only interface ontslaat niet van cybersecurity-eisen voor *ons* product; het voorkomt wél de misvatting dat Shelly+Nesso+warmtepomp één fysiek CE-toestel moeten worden.

## Dossieracties

1. SharePoint `03_`: DoC/CE warmtepomp + Shelly DoC/CE + Arduino TPX00227-set.  
2. Architectuur: “no physical interconnection; Zigbee + Modbus TCP only”; “Nesso hardware unmodified”.  
3. Jurist: DoC-tekst voor “programmed Nesso bridge” (Arduino DoC als annex + onze FW/CRA-verklaring).  
4. Geen claim “Arduino-DoC alleen dekt onze volledige bridge inclusief CRA”.

