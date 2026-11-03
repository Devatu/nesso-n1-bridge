# 04 – CRA/SharePoint dossierstructuur

SharePoint is het gecontroleerde CRA-dossier; GitHub is leidend voor broncode en release-assets.

**Werkmap in deze repo (nu gevuld als draft):** `docs/cra/`  
Technische controlestart (samenvatting): `docs/07-CRA-CONTROLS.md`.  
Provisioning-client: Espressif **ESP BLE Provisioning** (`docs/06-PROVISIONING-PORTAL-OTA.md`).

```
SharePoint/NESSO-N1/          ↔  docs/cra/
- 01_Product-en-toepassing/      ← 01-product-en-toepassing.md
- 02_Architectuur-en-interfaces/ ← 02-architectuur-en-interfaces.md
- 03_Componenten-en-leveranciers/← 03 + 12 + 13 + `Arduino_TPX00227-*` + Shelly DoC + warmtepomp DoC (+ later SBOM)
- 04_Risicoanalyse-en-threat-model/ ← 04-risicoanalyse-threat-model.md
- 05_Secure-development-.../     ← 06-secure-development.md
- 06_Test-en-verificatiebewijzen/← (host logs + later HW; docs/03-TESTPLAN)
- 07_Firmware-releases-.../      ← 07-update-en-releasebeleid.md
- 08_Vulnerability-handling-.../ ← 08-vulnerability-handling.md
- 09_Conformiteit-en-gebruikers/ ← 09-gebruikersinformatie.md + 11-checklist
- 10_Change-control-en-approvals/← 10-change-control.md
```

Annex I-matrix: `docs/cra/05-annex-I-controlematrix.md`.

Documenteer eigenaarschap, versie, goedkeuring, retentie en toegangsrechten bij elke SharePoint-upload.  
Laat fabrikantrol, conformiteitsprocedure, supportperiode en bewaartermijnen **juridisch** bevestigen vóór marktclaim “CRA-conform”.
