# 05 – Vaststaande keuzes en open vragen

## Vaststaand
- 5 Shelly BLU RC Button 4 ZB maximaal; allemaal dezelfde 4 functies.
- 1 warmtepomp; **vaste Modbus-registertabel** in firmware; IP/poort/unit ID via lokale portal.
- Repeaters toegestaan via commissioning; geen verplichte merken-whitelist; geen Modbusrechten.
- Installateur configureert **lokaal**; geen cloud dashboard; geen inbound WAN.
- Wi-Fi: **BLE provisioning** eenmalig (SoftAP niet primair i.v.m. Zigbee coexistence).
- Verplichte automatische updatecontrole; **OTA A/B** met signed manifest en rollback; offline bediening blijft werken.

## Besluit 2026-11-01 (eigenaarvraag)
- BLE-provision voor Wi-Fi: **ja** (primair), client = **ESP BLE Provisioning** (App Store / Play Store).
- Security 0 verboden; minimum Sec1+PoP; doel Sec2 (CRA). Zie `docs/06` + `docs/07-CRA-CONTROLS.md`.
- Portal voor warmtepomp IP + unit ID e.d.: **ja**, als **lokale LAN-portal**, niet cloud.
- OTA A/B: **ja**. Details: `docs/06-PROVISIONING-PORTAL-OTA.md`.

## Hardware CE/RED & systeemgrenzen — 2026-11-01
- Arduino TPX00227-certificaten in `docs/` / `docs/cra/12-…`; Zigbee = Arduino intended use in EU.
- **Wij** = fabrikant warmtepomp (eigen CE/DoC). Shelly = knoppen (eigen CE/DoC). Arduino = Nesso-hardware.
- Nesso: **ingekocht, fysiek ongewijzigd**, **eigen firmware**, door ons geleverd/op de markt gebracht. Zie `docs/cra/13-…`.
- Drie boxes alleen digitaal gekoppeld (Zigbee + Modbus TCP/LAN).
- OPEN (jurist): DoC-tekst geprogrammeerde bridge (Arduino-DoC als hardwarebijlage + onze FW/CRA).

## Nog aan te leveren door producteigenaar
- Exacte vier Modbus-acties (function code, adres, datatype, waarde, betekenis, readback, idempotentie).
- Gewenste knop-eventsemantiek: vooralsnog alleen single press; hoe omgaan met double/hold?
- Default/typical TCP-poort en unit-ID-range van de warmtepomp (portal valideert 1–65535 / 0–247).
- Gewenste updatevensters en maximale toegestane korte bedieningsonderbreking (BLE-sessie + OTA-reboot).
- Welke Nesso-hardwareversie en Shelly firmwareversie voor tests beschikbaar zijn.
- Of internet altijd voorzien is of alleen normaal gesproken beschikbaar.
