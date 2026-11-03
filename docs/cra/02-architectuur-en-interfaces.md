# 02 – Architectuur en interfaces

| Veld | Waarde |
|---|---|
| Document-ID | CRA-02 |
| Versie | 0.1-draft |
| Datum | 2026-11-01 |
| Status | Concept |
| Bronnen | `docs/01-ARCHITECTUUR.md`, `docs/06-PROVISIONING-PORTAL-OTA.md` |

## Datastroom (runtime)

```
Shelly BLU RC Button 4 ZB (≤5, sleepy ED)
        │ Zigbee (evt. via routers)
        ▼
Nesso N1 Zigbee coordinator
        │ authenticate EUI-64 allowlist
        │ decode button → ACTION_1..4
        │ dedup / rate limit
        ▼
Modbus TCP client ──► 1 warmtepomp (IP/port/unit uit NVS; registers compile-time)
```

## Trust boundaries

| Zone | Inhoud | Vertrouwen |
|---|---|---|
| Zigbee mesh | Shelly + optionele routers | Alleen allowlisted EUI-64 mag acties; short address alleen onvoldoende |
| LAN | STA, lokale HTTP-portal, Modbus | Geen inbound WAN; portal auth + CSRF |
| BLE (tijdelijk) | Wi-Fi provisioning | Alleen service/fysiek; Sec≠0 + PoP; daarna BLE uit |
| Internet (outbound) | HTTPS update-check | Signed manifest; geen device GitHub-token |
| Device NVS | Wi-Fi, heatpump, registry | Atomische writes; geen secrets in logs |

## Externe interfaces

| Interface | Richting | Protocol | Doel |
|---|---|---|---|
| Zigbee 802.15.4 | in/out | ZCL On/Off, Level, Scenes | Knop-events |
| Wi-Fi STA | out/in LAN | IP | Modbus + portal + OTA check |
| BLE | tijdelijk | Espressif wifi_prov | Wi-Fi credentials |
| Modbus TCP | out | FC allowlist | Warmtepomp-acties |
| HTTPS | out | TLS | OTA manifest/assets |
| USB | service | CDC | Debug/flash (productie: beperkt) |
| Display/KEY1/KEY2 | lokaal | UI | PoP, status, service |

## Wat bewust ontbreekt (attack surface)

- Cloud API / remote dashboard  
- Inbound port forwarding-aanname  
- SoftAP als primaire provision (Espressif SoftAP+Zigbee-router = X)  
- Portal-registereditor  

## Secure defaults (ontwerp)

- Zigbee join standaard dicht; open alleen ≤120 s in service.  
- BLE provision alleen unprovisioned of na fysieke KEY.  
- OTA-security-check niet permanent uitschakelbaar.  
- Onbekende EUI / router → geen Modbus.
