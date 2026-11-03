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
