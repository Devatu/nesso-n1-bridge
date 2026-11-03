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
