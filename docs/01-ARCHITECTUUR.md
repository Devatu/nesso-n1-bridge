# 01 – Firmwarearchitectuur

## Datastroom
Shelly #1..#5 (Zigbee sleepy end devices) → Zigbee coordinator Nesso N1 (evt. via Zigbee routers) → **authenticate source** → normalize button event → deduplicate/rate limit → compile-time `ActionTable` → Modbus TCP client → één warmtepomp.

Geen direct Modbus-pad vanuit HTTP portal, Zigbee router, update server of ongeautoriseerde Zigbee nodes.

## Modules
- `app`: lifecycle, event loop, watchdog, startup checks.
- `zigbee_coordinator`: network commissioning, device join/leave, route handling.
- `device_registry`: 5 Shelly EUI-64 slots, Zigbee routers apart, persistent versioned NVS, migration.
- `shelly_decoder`: bewezen cluster/endpoint/event naar `ButtonEvent`.
- `authorization`: source identity, network association, allowed event type.
- `action_dispatcher`: one global mapping for all switches, debounce, queue, anti-repeat.
- `modbus_client`: only fixed target host from NVS + compile-time register allowlist; timeout, bounded retry, no unsafe repeat.
- `wifi_manager`: STA credentials, connection/reconnect; SoftAP is fallback only with Zigbee paused.
- `ble_provision`: one-time Wi-Fi setup via Espressif WiFiProv BLE + PoP; Zigbee paused during session; BLE memory freed after.
- `heatpump_config`: validated IPv4, TCP port, unit ID (0–247); no free registers.
- `installer_portal`: authenticated **local LAN** HTTP after STA; heatpump IP/port/unit ID, pairing, status, OTA status; no cloud; no register editor.
- `display_ui`: status, PoP/QR for BLE provision, service mode; hardware-abstracted.
- `ota_manager`: A/B inactive slot, signed manifest, download, boot validation, rollback.
- `storage`: config schema (Wi-Fi meta, heatpump, registry), atomic writes, no secrets in logs.
- `telemetry`: local diagnostic log, no external dashboard.

## Registratie
- Shelly slots 0..4: EUI-64, label, enabled, commissioning metadata.
- Router registry: separately stored, bounded by measured network resources, EUI-64, type, join time.
- A device cannot be authorized as Shelly solely by self-reported model string; require authenticated association and approved enrollment.
- Reject sixth Shelly, duplicates, unknown device, invalid button event.

## Actiemapping
Button 1→ACTION_1; 2→ACTION_2; 3→ACTION_3; 4→ACTION_4, regardless of which of the five Shellys sends it. Map ACTION_n to **owner-confirmed hardcoded** Modbus function/address/type/value. Specify whether click/hold/double click are ignored; default **single press only**, pending Shelly verification.

## Provisioning en installerportal
Zie `docs/06-PROVISIONING-PORTAL-OTA.md`.

- **Wi-Fi eenmalig:** BLE provisioning (primair), niet SoftAP. SoftAP alleen als fallback met Zigbee gepauzeerd.
- **Lokale portal:** na STA, alleen LAN. Configureert warmtepomp IP/poort/unit ID; geen Modbus-registereditor; geen cloud.
- Service/re-provision: fysieke KEY1/KEY2. Zigbee joining alleen expliciet 120s. QR/PoP op display is weergave, geen auth-vervanging.

## Repeater policy
No hardcoded model allowlist needed; routers must join the same secure Zigbee network during commissioning. Persist identity; allow removal and network-key renewal procedure. Official compatibility list limited to tested models. Router messages cannot trigger Modbus; only source EUI-64 in Shelly registry may trigger actions.
