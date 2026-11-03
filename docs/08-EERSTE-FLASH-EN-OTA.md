# 08 – Eerste flash, documentatie en OTA A/B via GitHub

Voor als de **Nesso N1 fysiek binnen is**. Bouwt voort op Fase 0 (`docs/VERIFICATION_RESULTS.md`) en CRA-rollen (`docs/cra/13-…`).

## Doelbeeld

1. Nesso **fysiek ongewijzigd** flashen met jullie firmware.  
2. Build reproduceerbaar (Arduino IDE of CLI).  
3. Partities met **OTA A/B** (`ota_0` / `ota_1` + `otadata`).  
4. GitHub repo: CI compileert/test; release publiceert `.bin` + signed manifest.  
5. Device haalt updates via HTTPS (GitHub Release assets), verifieert, schrijft inactive slot, reboot, commit/rollback.

## Rolverdeling (kort)

| Onderdeel | Wie |
|---|---|
| Hardware CE/RED | Arduino TPX00227 (ongewijzigd) |
| Firmware + OTA + CRA bridge | Jullie |
| Warmtepomp / Shelly CE | Eigen DoC’s |

---

## Fase A – Eenmalige PC-setup (vandaag)

### A1. Drivers / poort

1. USB-C kabel (data, geen charge-only).  
2. Nesso aan; Windows Apparatenbeheer → poorten. Verwacht iets als **USB Serial Device (COMx)** (ESP32-C6 USB-CDC).  
3. Noteer `COMx`.

### A2. Arduino toolchain

**Optie IDE (eenvoudig voor eerste flash):**

1. Arduino IDE 2.x.  
2. Boards Manager → `esp32` by Espressif → **≥ 3.3.5** (doel 3.3.12; lokaal kan 3.3.11 al staan).  
3. Library Manager:
   - `Arduino_Nesso_N1` 1.0.0  
   - dependencies (`M5GFX`, …) volgen automatisch.  
4. Board: **Arduino Nesso N1**.  
5. Tools:
   - **Zigbee Mode** → `Zigbee ZCZR (coordinator/router)`  
   - **Partition Scheme** → `Zigbee ZCZR 4MB with spiffs`  
   - Port → jouw `COMx`  
   - Upload Speed → 921600 (of lager bij fouten)

**Optie CLI (reproduceerbaar / CI):**

```powershell
powershell -ExecutionPolicy Bypass -File scripts\setup_arduino_cli.ps1
powershell -ExecutionPolicy Bypass -File scripts\build_firmware.ps1
powershell -ExecutionPolicy Bypass -File scripts\flash_firmware.ps1 -Port COM8
```

FQBN (vastgelegd):

```text
esp32:esp32:arduino_nesso_n1:ZigbeeMode=zczr,PartitionScheme=zigbee_zczr
```

Deze ZCZR-tabel heeft **officieel A/B** (`app0`/`app1` = ota_0/ota_1, elk ~1.25 MB) + Zigbee-storage. Zie `firmware/partitions/README.md`.

### A3. Eerste sketch (sanity, vóór product-FW)

Flash eerst een officieel Nesso-voorbeeld (display of blink KEY1) om USB + board package te bewijzen. Pas daarna productfirmware.

### A4. Productfirmware eerste keer

1. Open `firmware/nesso_n1_bridge/nesso_n1_bridge.ino`.  
2. Voor hardware: in `app_config.h` → `#define NESSO_ENABLE_HW 1`.  
3. Compile + Upload.  
4. Serial Monitor 115200: verwacht bootlog `Fase0+prov` / module TODOs.  
5. **Nog geen** productie-Zigbee/Modbus tot die modules HW-getest zijn.

Documenteer in SharePoint `06_Test-…`: datum, core-versie, FQBN, COM-poort, git-commit/hash van binary.

---

## Fase B – Git-repo op GitHub (vereiste voor OTA)

Lokaal is dit project nu **nog geen git-repo**. Voor OTA via GitHub:

1. Maak private (of public binary + private source) GitHub-repo.  
2. Init + push (alleen als jij dat wilt):

```powershell
cd "C:\Users\Devin\Nesso N1"
git init
git add .
git commit -m "Initial Nesso bridge scaffold and CRA dossier"
git branch -M main
git remote add origin https://github.com/ORG/REPO.git
git push -u origin main
```

3. **Nooit** committen: signing private keys, `.env`, Wi-Fi secrets, `Arduino15\inventory.yaml` secrets.

Workflows staan in `.github/workflows/`:

| File | Rol |
|---|---|
| `ci.yml` | PR: hosttests (+ later compile) |
| `release.yml` | Tag `v*`: build `.bin`, checksums, SBOM-placeholder, manifest; signing in protected environment |

---

## Fase C – OTA A/B constructie

### C1. Partities (nu)

Gebruik **Zigbee ZCZR 4MB with spiffs** (`zigbee_zczr.csv`):

- `otadata`  
- `app0` / `ota_0`  
- `app1` / `ota_1`  
- `zb_storage`, `spiffs`, …

Dat **is** A/B. Flash is 16 MB; bovenste ruimte blijft ongebruikt tot jullie later een grotere officiële ZCZR-tabel kopiëren (`zigbee_zczr_8MB.csv` als sketch-`partitions.csv` als app > 1.25 MB).

### C2. Devicegedrag (productcode)

Reeds ontworpen in `ota_manager` / `ota_manifest`:

1. Boot + elke 24u ± jitter: HTTPS GET signed manifest.  
2. Checks: product, board `arduino_nesso_n1`, channel, `security_version`, SHA-256, signature.  
3. Download naar **inactive** slot (`Update` / `esp_ota_*`).  
4. Reboot → `PENDING_VERIFY` selftest → commit of rollback.  
5. Offline: knoppad blijft werken.

Manifest-voorbeeldschema: `docs/02-SECURITY-OTA.md` en `release/manifest.schema.json`.

### C3. GitHub Release assets (per versie)

Immutabele assets per tag, bv. `v0.1.0`:

| Asset | Inhoud |
|---|---|
| `nesso-n1-bridge-v0.1.0.bin` | Firmware image |
| `nesso-n1-bridge-v0.1.0.bin.sha256` | Checksum |
| `manifest-v0.1.0.json` | Canonical manifest |
| `manifest-v0.1.0.json.sig` of signature in JSON | ECDSA-P256/RSA |
| `sbom-v0.1.0.cdx.json` | SBOM (CRA) |

Device-URL in manifest = **HTTPS** Release download-URL (geen GitHub-token op device).

### C4. Signing (CRA / producteis)

| Item | Regel |
|---|---|
| Algoritme | ECDSA P-256 of RSA (`UPDATE_SIGN`); **niet** Ed25519/mbedTLS |
| Private key | Alleen GitHub Actions **Environment** + approval; nooit in repo |
| Public key | In firmware (compile-time) |
| Eerste releases | Mag “staging channel” + testkey; productiekey pas na akkoord |

Zie `scripts/sign_manifest.md` (procedure). Scripts met echte keys volgen ná keygeneratie.

---

## Fase D – Aanbevolen volgorde (hardware week 1)

| Dag | Actie | Klaar als |
|---|---|---|
| 1 | USB + blink/display voorbeeld | Serial + scherm werken |
| 1 | CLI/IDE build product-FW met ZCZR | Compile OK |
| 1 | USB flash product-FW | Bootlog OK |
| 2 | GitHub repo + CI hosttests | Groene CI |
| 2–3 | Wi-Fi STA smoke (nog zonder Zigbee of Zigbee pause) | STA connected |
| 3 | BLE provision smoke met ESP BLE Provisioning-app | Credentials in NVS |
| 4 | Zigbee ZCZR form + openNetwork | Network up |
| 5 | OTA dry-run: handmatig tweede image naar inactive slot | A/B reboot/rollback |
| later | Signed GitHub release → device check | Echte OTA |

---

## Wat je nu níet moet doen

- eFuse Secure Boot / flash encryption / anti-rollback  
- SoftAP terwijl Zigbee coordinator draait  
- Productie-signing keys in de repo  
- Vrije Modbus-registers in de portal  
- Claimen “CRA-conform” zonder DoC-tekst bridge + SBOM-uit-CI  

---

## Verificatie (deze machine, 2026-11-03)

| Check | Resultaat |
|---|---|
| `arduino-cli` in `tools/arduino-cli` | OK |
| esp32 core | **3.3.11** geïnstalleerd (3.3.12 beschikbaar) |
| Libs `Arduino_Nesso_N1` + `M5GFX` | OK |
| Compile FQBN ZCZR + `zigbee_zczr` | **OK** — ~255 KB / 1.25 MB slot (19%) |
| USB COM Nesso | Nog niet stabiel in `board list` — kabel/driver controleren bij flash |
| Git-repo | Nog niet geïnitialiseerd — nodig voor GitHub OTA |

---

## Snelle checklist eerste middag

- [ ] COM-poort gezien (Nesso aangesloten)  
- [x] esp32 core ≥ 3.3.5  
- [x] `Arduino_Nesso_N1` geïnstalleerd  
- [x] Board + ZCZR + Zigbee ZCZR partitie (compile bewezen)  
- [ ] Voorbeeldsketch geflasht  
- [ ] Product-FW geflasht (`NESSO_ENABLE_HW 1`)  
- [ ] Bootlog bewaard  
- [ ] GitHub-repo aangemaakt + push  
- [x] OTA A/B partitie via FQBN `PartitionScheme=zigbee_zczr`  
