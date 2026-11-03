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
