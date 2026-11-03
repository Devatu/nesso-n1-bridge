# 06 – Secure development en brontracering

| Veld | Waarde |
|---|---|
| Document-ID | CRA-06 |
| Versie | 0.1-draft |
| Datum | 2026-11-01 |
| Status | Procesontwerp |

## Bron van waarheid

| Artefact | Locatie |
|---|---|
| Source | GitHub (of lokale Git tot remote) |
| Releases / binaries | GitHub Release assets (immutable) |
| CRA-dossier kopieën | SharePoint (`docs/04-CRA-SHAREPOINT.md`) |
| Signing keys | CI/KMS – **nooit** in repo of device |

## Ontwikkelprincipes

1. Geen geheimen in repo (`.gitignore`: `.env`, keys, pem).  
2. Geen gefingeerde hardware-/registerfeiten; BLOCKED/TODO markeren.  
3. Host-unit tests voor pure logica vóór hardware.  
4. PR-review vóór `main`.  
5. Release alleen via protected environment + menselijke approval.  
6. Security updates bij voorkeur scheidbaar via `security_version`.

## Geplande CI-gates (nog niet live)

- Compile (Arduino FQBN Nesso + ZCZR)  
- Host unit tests  
- Secret scanning  
- Static analysis (basis)  
- SBOM generatie  
- Manifest sign (protected)

## Traceerbaarheid

Elke release: git tag ↔ binary SHA-256 ↔ signed manifest ↔ SBOM ↔ release notes ↔ hardware compatibility.
