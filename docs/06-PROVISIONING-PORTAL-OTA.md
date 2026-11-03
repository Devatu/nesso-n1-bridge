# 06 – BLE-provisioning, lokale portal, OTA A/B

Besluitdatum: 2026-11-01. Wijzigt SoftAP-eerste approach vanwege Espressif SoftAP+Zigbee-router = **X**.  
CRA-mapping: `docs/07-CRA-CONTROLS.md`.

## Officiële BLE-client (onderzocht)

Installateurs gebruiken de **Espressif ESP BLE Provisioning**-app:

| Platform | App | Links |
|---|---|---|
| Android | ESP BLE Provisioning | [Google Play `com.espressif.provble`](https://play.google.com/store/apps/details?id=com.espressif.provble) |
| iOS | ESP BLE Provisioning | [App Store id 1473590141](https://apps.apple.com/us/app/esp-ble-provisioning/id1473590141) |
| Bron | Officiële Espressif repos | [esp-idf-provisioning-android](https://github.com/espressif/esp-idf-provisioning-android), [esp-idf-provisioning-ios](https://github.com/espressif/esp-idf-provisioning-ios) |
| Docs | Unified / Wi-Fi provisioning | [ESP-IDF provisioning](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/provisioning/provisioning.html) |

Play Store (geraadpleegd 2026-11-01): BLE Wi-Fi provisioning voor ESP-IDF ≥ 3.2; **Security Level 1**; **Proof of Possession (PoP)**; Wi-Fi scan list; *No data collected / No data shared with third parties* (developer declaration).  
Android library + iOS updates ondersteunen ook **Security 2** (SRP6a + AES-GCM); app detecteert `sec_ver` van het device. Espressif noemt Security 2 aanbevolen voor productie.

CLI-debug (ontwikkelaar): `esp_prov` — niet voor eindinstallateur.

### Installateur-stappen (UX)

1. Fysieke service-actie op Nesso (KEY1/KEY2) of first-boot unprovisioned.  
2. Display toont BLE-servicenaam (bijv. `PROV_NESSO_xx`) + **PoP** (en/of QR met die waarden).  
3. Open **ESP BLE Provisioning** → scan/selecteer device → voer PoP in.  
4. Kies Wi-Fi uit scanlijst → app stuurt credentials over beveiligde sessie.  
5. Device valideert STA; bij succes: BLE uit, Zigbee aan, PoP verdwijnt van display.  
6. Daarna lokale LAN-portal voor warmtepomp IP/poort/unit ID.

## Wat wel / niet

| Vraag | Besluit |
|---|---|
| Wi-Fi eenmalig | **Ja**, BLE + officiële Espressif-app |
| SoftAP primair | **Nee** (coexistence) |
| SoftAP fallback | Alleen Zigbee gepauzeerd + akkoord |
| Security 0 | **Verboden** (CRA) |
| Security minimum | **Sec 1 + niet-lege PoP** |
| Security doel | **Sec 2** wanneer core/app-pad op Nesso bewezen |
| Cloud portal | **Nee** |
| Lokale LAN-portal | **Ja** — IP/poort/unit ID; geen registers |
| OTA A/B | **Ja** — signed, rollback |

## Flow

```
[KEY1/KEY2 of factory-unprovisioned]
        │
        ▼
  Zigbee pause ──► BLE (WiFiProv) + PoP op display
        │              ▲
        │              └── ESP BLE Provisioning app (PoP + Wi-Fi keuze)
        ▼
  STA validate ──► NVS credentials atomisch ──► BLE free ──► Zigbee resume
        │
        ▼
  Lokale portal http://<device-ip>/  (LAN auth)
        ├── Heatpump IP / port / unit ID
        ├── Zigbee join 120s (service)
        └── OTA: status + knop Controleren + knop Installeren (geen OTA-disable)

  Display: KEY2 = OTA check (huidige vs remote); bij update KEY2-lang of portal Installeren

  OTA check: HTTPS GET signed channel-manifest → vergelijk version/security_version met NESSO_FW_VERSION
  Auto (boot+24u): check + auto_apply → inactive slot → verify → reboot → commit/rollback
  UI: check zonder download; apply pas na knop
```

### OTA API (lokaal)

| Method | Pad | Gedrag |
|---|---|---|
| `GET` | `/api/ota` | JSON: `current`, `remote`, `availability`, `update_available` |
| `POST` | `/api/ota/check` | Manifest ophalen + vergelijken (`ota_check_for_ui`) |
| `POST` | `/api/ota/apply` | Download naar inactive slot als update beschikbaar |

Channel-URL: `NESSO_OTA_MANIFEST_URL` (publieke Release-asset; geen GitHub-token op device).

## BLE-eisen (CRA + product)

- Transport: `WIFI_PROV_SCHEME_BLE` (Arduino `WiFiProv` / ESP-IDF wifi_prov_mgr).  
- **Nooit** `WIFI_PROV_SECURITY_0`.  
- PoP: cryptografisch sterk, per apparaat of per sessie; timeout ≤ 10 min; niet loggen.  
- Credentials pas persistent na geslaagde STA; anders oude config behouden (secure-by-default / geen brick).  
- Na succes: `FREE_BTDM` / BLE-geheugen vrij.  
- Her-provision: alleen fysiek, niet via internet.  
- Tijdens sessie: Zigbee gepauzeerd; UI toont “knoppen tijdelijk uit”.

## Lokale portal

LAN-only. Auth + CSRF + sessietimeout.  
Toegestaan: heatpump IPv4, TCP-poort, unit ID 0–247, status, Zigbee service-acties.  
Verboden: Modbus-registereditor, cloud-URL besturing, permanente OTA-uitschakeling.

## OTA A/B (CRA updatepad)

`otadata` + `ota_0`/`ota_1`. Signed manifest (ECDSA/RSA, niet Ed25519/mbedTLS). Monotone `security_version`. Rollback bij selftest-fail. NVS behouden. Geen eFuse anti-rollback zonder herstelplan. Geen GitHub-token op device.

## Open / BLOCKED

- Hardwarebewijs: Sec2 vs Sec1 met store-app op Nesso C6 + Arduino core.  
- Default Modbus-poort/unit-conventie (eigenaar).  
- SoftAP-fallback alleen na akkoord.  
- Rollback-bootloader op Nesso bewijzen.  
