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
