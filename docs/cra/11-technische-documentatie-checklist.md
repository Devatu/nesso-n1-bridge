# 11 – Technische documentatie-checklist (Annex VII-achtig)

| Veld | Waarde |
|---|---|
| Document-ID | CRA-11 |
| Versie | 0.1-draft |
| Datum | 2026-11-01 |
| Status | Checklist – geen afvink-claim van volledige CRA-conformiteit |

Gebaseerd op typische Annex VII-elementen (productbeschrijving, ontwerp, risicoanalyse, SBOM/vuln-handling, tests, DoC). Juridische volledigheid: **OPEN**.

| # | Vereiste (samengevat) | Onze artefact | Status |
|---|---|---|---|
| 1 | Algemene productbeschrijving, intended purpose, versies | CRA-01 | Draft |
| 2 | Begrijpen van ontwerp/ontwikkeling/werking | CRA-02, docs/01, docs/06 | Draft |
| 3 | Foto’s/illustraties HW (indien van toepassing) | SharePoint **TODO** | Open |
| 4 | Cybersecurity risk assessment | CRA-04 | Eerste iteratie |
| 5 | Annex I mapping / oplossingen | CRA-05, docs/07-CRA-CONTROLS | Draft |
| 6 | Supportperiode-onderbouwing | CRA-01 veld | **Juridisch open** |
| 7 | Toegepaste standaarden/specs | **TODO** (bijv. later EN-normen) | Open |
| 8 | Test- en verificatiebewijzen | Hosttests; HW **TODO**; docs/03-TESTPLAN | Deels |
| 9 | SBOM machine-readable | CRA-03 voorloper; CI **TODO** | Deels |
| 10 | Vulnerability handling info + contact | CRA-08 | Template (contact open) |
| 11 | Secure update distributie beschreven | CRA-07 | Draft |
| 12 | Gebruikersinformatie | CRA-09 | Concept |
| 13 | EU-verklaring van overeenstemming | — | **Niet nu** |
| 14 | Change control / goedkeuringen | CRA-10 | Template |

## Wat je nu wél kunt claimen intern

“We hebben een **CRA-gerichte technische dossierbasis** in Git (`docs/cra/`), gekoppeld aan productarchitectuur, updatepad, provisioning en vulnerability-proces.”

## Wat je níet kunt claimen

“Dit product is CRA-conform / CE-CRA gereed” — zolang fabrikantrol, supportperiode, SBOM-uit-CI, HW-tests, DoC en eventuele conformity assessment open staan.
