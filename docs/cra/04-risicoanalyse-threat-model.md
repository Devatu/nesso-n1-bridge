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
