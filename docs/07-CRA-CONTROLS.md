# 07 – CRA-controles (technische mapping)

Status: **ontwerp / dossier-input**. Geen juridische goedkeuring. Fabrikantrol, supportperiode en conformiteitsprocedure moeten juridisch bevestigd worden (`docs/04-CRA-SHAREPOINT.md`).

Referentiekader: EU Cyber Resilience Act (Verordening (EU) 2024/2847), Annex I essential requirements + vulnerability handling. Dossiermap: SharePoint `NESSO-N1/`.

## Productprofiel (aanname tot juridische bevestiging)

- Product met digitale elementen: Nesso N1 heatpump bridge (firmware + hardware).
- Gebruik: professionele/installatie-omgeving (warmtepomp LAN + Zigbee).
- Geen cloud-besturing; lokale installatie; outbound HTTPS alleen voor signed updates.
- Ondersteuningsperiode: **OPEN / juridisch** (minimaal beleid vastleggen vóór marktintroductie).

## Annex I Part I → productcontroles

| CRA-eis (samengevat) | Productcontrole | Bewijs / status |
|---|---|---|
| Passend cybersecurityniveau o.b.v. risico | Threat model + risk assessment in SharePoint `04_` | TODO risicoanalyse |
| Geen bekende exploiteerbare vulns bij release | CI, dependency review, release gate, SBOM | TODO CI |
| Secure by default + reset naar veilige staat | Geen open Zigbee-join; BLE alleen na fysieke trigger; factory/service reset procedure | Deels ontwerp |
| Security updates (secure distributie; waar van toepassing automatisch) | Verplichte OTA-check boot+24u; signed manifest; A/B + rollback; offline Zigbee/Modbus blijft werken | Ontwerp + scaffold |
| Bescherming tegen onbevoegde toegang | BLE Sec≠0 + PoP; lokale portal auth/CSRF; EUI-64 allowlist; geen free registers | Ontwerp |
| Confidentialiteit / integriteit data | Geen secrets in logs; NVS atomisch; TLS naar updatehost; PoP niet gelogd | Ontwerp |
| Minimaliseer attack surface | Geen inbound WAN; geen cloud; SoftAP niet primair; BLE na provision uit | Ontwerp |
| Beperk impact van incidenten | Fail-safe Modbus; dedup; OTA rollback; knoppad offline | Ontwerp |
| Logging / monitoring (passend) | Lokale diagnostiek; geen extern dashboard met PII | Ontwerp |
| Secure deletion / reset | Service factory reset wis Wi-Fi + heatpump + registry volgens procedure | TODO |

### Industriële auto-update nuance (Recital 56)

Voor professionele/industriële context kan “blind consumer auto-update” storend zijn. Dit product:

- **wel** automatische *controle* en installatie van **expliciet goedgekeurde security-releases**;
- **niet** blind “latest GitHub”;
- update buiten actieve Modbus-transactie;
- Zigbee→Modbus blijft werken zonder internet.

Dat blijft CRA-gericht (secure update-pad) zonder operationele chaos. Permanente uitschakeling van security-updatechecks blijft **verboden**.

## Annex I Part II → vulnerability handling

| Eis | Implementatie |
|---|---|
| SBOM (machine-readable, ≥ top-level deps) | CI genereert SBOM per release; SharePoint `03_` + `07_` |
| Vulns remediatie zonder onnodige vertraging | Security contact + triage SLA; signed OTA patch |
| Security tests/reviews | Hosttests, later hardware + static analysis in CI |
| Publicatie fixed vulns | Release notes + advisory (proces) |
| Coordinated vulnerability disclosure | Policy-document in SharePoint `08_` |
| Contactpunt | security@… (eigenaar levert) |
| Secure update distributie | HTTPS + signature + A/B; gratis security updates |
| Scheiding security vs feature updates waar haalbaar | `security_version` in manifest |

## Provisioning onder CRA (onbevoegde toegang / default security)

| Regel | Besluit |
|---|---|
| Officiële client | **ESP BLE Provisioning** (Espressif) – Play Store + App Store |
| Security 0 (plaintext) | **Verboden** in productie |
| Minimum | **Security 1 + niet-lege PoP** (per apparaat/sessie) |
| Doel productie | **Security 2** (SRP6a + AES-GCM) zodra Arduino `WiFiProv`/core dit stabiel biedt en beide store-apps dit op Nesso-C6 bewezen hebben |
| PoP | Alleen op display/QR tijdens service; nooit in logs/repo; timeout |
| Na succes | BLE-stack/geheugen vrijgeven; Zigbee hervatten |
| Herprovision | Alleen fysieke KEY1/KEY2; niet remote |

## OTA onder CRA

Zie `docs/02-SECURITY-OTA.md` + `docs/06-PROVISIONING-PORTAL-OTA.md`.

Kort: signed manifest, board/product/channel checks, monotone `security_version`, inactive slot, bootvalidate, rollback, geen eFuse anti-rollback zonder herstelplan, geen device-side GitHub token.

## Dossier (SharePoint) – minimale inhoud

1. Productbeschrijving + intended use  
2. Architectuur / trust boundaries (BLE, LAN portal, Zigbee, Modbus, OTA)  
3. Componenten + SBOM + leveranciers (Arduino, Espressif, Shelly, app)  
4. Cybersecurity risk assessment + Annex I mapping (dit document als start)  
5. Secure development / source traceability (GitHub)  
6. Testbewijzen (host + hardware)  
7. Release/updatebeleid + signing/keyrotatie  
8. Vulnerability handling + incidenten  
9. Gebruikersinformatie (installatiehandleiding BLE-app, PoP, supportperiode-einde)  
10. Change control / approvals  

## Open juridisch / product

- Exacte fabrikantrol (manufacturer vs. andere economic operator).  
- Supportperiode ≥ productlevensduur-beleid.  
- Of product “important/critical” productcategorie raakt (conformiteitsmodule).  
- Security contact e-mail en CVD-publicatiekanaal.  
