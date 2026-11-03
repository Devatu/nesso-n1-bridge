# 02 – Security, automatische OTA en GitHub

## OTA A/B (besloten)
Partition schema met `otadata` + `ota_0`/`ota_1` (officiële Zigbee ZCZR-tabel). Download altijd naar inactive slot. Na reboot: `ESP_OTA_IMG_PENDING_VERIFY` → selftest → `esp_ota_mark_app_valid_cancel_rollback()` of `esp_ota_mark_app_invalid_rollback_and_reboot()`. NVS (Wi-Fi, heatpump IP/port/unit ID, Shelly registry) blijft behouden. Geen eFuse anti-rollback. Zie `docs/06-PROVISIONING-PORTAL-OTA.md`.

## Updatebeleid
- Device controleert na boot zodra netwerk beschikbaar is en vervolgens elke 24 uur ± jitter.
- Geen user toggle om beveiligingsupdates permanent uit te schakelen; geen verplichte internetverbinding voor Zigbee/Modbus-bediening.
- Release wordt pas aangeboden na interne goedkeuring, canary/staged rollout en expliciete compatibiliteitscontrole.
- Niet direct 'latest GitHub release' blind vertrouwen: gebruik expliciet signed manifest met release channel, target hardware en minimum bootloader.
- Update buiten actieve Modbus-transactie; beperk rebootimpact, plan wachttijd, veilige reset van command queue.
- Bij download-/signaturefout: huidige firmware behouden; retry met exponentiële backoff.
- Bij nieuwe firmware die boot/selftest faalt: rollback naar bekende werkende image, markeer mislukte versie en voorkom update-loop.
- Offline: oude werkende firmware blijft functioneren en update-status vermeldt 'laatst gecontroleerd'.

## Trust chain
1. HTTPS met servercertificaatvalidatie en correcte tijd/vertrouwensstrategie.
2. Embedded vertrouwde publieke signing key; private signing key uitsluitend in beschermde CI/KMS, nooit in repo/device.
3. Signed canonical manifest: schema, product, board, hardware revision, channel, version, security version, asset URL, size, SHA-256, signature, release date, rollout policy.
4. Verifieer signature vóór vertrouwen in manifestinhoud; verifieer image size/hash en waar mogelijk image signature vóór boot.
5. Bescherm tegen downgrade/replay via monotone security version of expliciete signed policy, met rollback-excepties. Geen eFuse anti-rollback activeren zolang veilige rollback niet bewezen is.
6. Secure Boot / flash encryption alleen na hardware-/manufacturing-plan, key backup en recoverytest.

## Voorbeeldmanifest (SCHEMA, geen geldige release)
```json
{
  "schema": 1,
  "product": "nesso-n1-heatpump-bridge",
  "board": "VERIFY_EXACT_BOARD_ID",
  "channel": "stable",
  "version": "0.1.0",
  "security_version": 1,
  "asset_url": "https://github.com/ORG/REPO/releases/download/v0.1.0/firmware.bin",
  "sha256": "REPLACE_WITH_REAL_64_HEX",
  "size_bytes": 0,
  "signature_algorithm": "VERIFY_SUPPORTED_ALGORITHM",
  "signature": "BASE64_SIGNATURE_PLACEHOLDER"
}
```
Signed canonicalization and URL trust boundaries must be specified before implementation. Manifest should be immutable per version; latest-channel pointer can change but must resolve to a signed manifest. Define behavior for redirect hosts, redirects, TLS failure, time failure, malformed data, rollback, key rotation and revocation.

## GitHub repo / CI
- `main`: reviewed code; PR checks compile + unit tests + static analysis.
- `release/*` or annotated tags: approved release candidate; generate binaries and SBOM.
- GitHub Actions with least-privilege permissions; pin action versions; production signing in protected environment with human approval.
- Publish `.bin`, signed manifest, checksums, SBOM, release notes, hardware compatibility and rollback instructions.
- Public binary-release repository can be separate from private source repository; confirm GitHub access/asset URL availability on device.
- OTA client must not require a GitHub token stored on devices.

## Vulnerability process / CRA
Security contact, intake, triage, severity, patch deadlines, coordinated disclosure, support-period policy, incident log, SBOM, reproducible build provenance.  
Technische Annex I-mapping: `docs/07-CRA-CONTROLS.md`. Dossier: `docs/04-CRA-SHAREPOINT.md`.  
CRA reporting obligations, fabrikantrol en supportperiode: juridische review vóór marktclaim.
