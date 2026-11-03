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
