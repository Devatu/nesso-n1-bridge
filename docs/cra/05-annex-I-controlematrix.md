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
