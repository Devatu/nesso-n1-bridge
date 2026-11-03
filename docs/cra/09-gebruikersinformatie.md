# 09 – Gebruikers- en installatie-informatie (concept)

| Veld | Waarde |
|---|---|
| Document-ID | CRA-09 |
| Versie | 0.1-draft |
| Datum | 2026-11-01 |
| Status | Concept handleiding – hardware UX nog te valideren |

## Veiligheids- en privacykenmerken (kort)

- Lokale installatie; geen cloud-bediening.  
- Wi-Fi via officiële **ESP BLE Provisioning**-app + code (PoP) op het scherm.  
- Automatische controle op goedgekeurde beveiligingsupdates; knoppen werken ook zonder internet.  
- Alleen gekoppelde Shelly-knoppen bedienen de warmtepomp.

## Benodigdheden

- Arduino Nesso N1 met deze firmware  
- Tot 5× Shelly BLU RC Button 4 ZB  
- Wi-Fi 2.4 GHz  
- Telefoon met [ESP BLE Provisioning (Android)](https://play.google.com/store/apps/details?id=com.espressif.provble) of [iOS](https://apps.apple.com/us/app/esp-ble-provisioning/id1473590141)  
- Warmtepomp bereikbaar via Modbus TCP op LAN  

## Wi-Fi instellen (eenmalig)

1. Zet Nesso aan. Bij eerste start (of na service-knop) verschijnt BLE-setup.  
2. Noteer **apparaatnaam** en **PoP** op het display (of QR).  
3. Open ESP BLE Provisioning → kies het apparaat → voer PoP in.  
4. Selecteer Wi-Fi-netwerk en wachtwoord.  
5. Bij succes verdwijnt BLE-setup; Nesso verbindt als station.  
6. Zigbee-knoppen zijn tijdens BLE-setup tijdelijk niet beschikbaar.

## Warmtepomp koppelen (lokale portal)

1. Zoek het IP van de Nesso op je LAN (router/DHCP).  
2. Open `http://<nesso-ip>/` op een LAN-PC.  
3. Log in (lokale credentials – **TODO** productiewaarden).  
4. Vul warmtepomp **IPv4**, **TCP-poort**, **unit ID** in.  
5. Modbus-registers zijn vast in firmware; niet via portal wijzigbaar.

## Shelly koppelen

1. Activeer service/pairing (portal of KEY – **TODO** exacte UI).  
2. Op Shelly: 5× knop voor Zigbee join (zie Shelly-docs).  
3. Bevestig EUI in allowlist (max 5).  
4. Sluit pairingvenster (≤120 s).

## Updates

Het apparaat zoekt periodiek naar goedgekeurde updates. Bij installatie kan een korte herstart volgen; knopbediening hervat na boot. Zonder internet blijven bestaande knopfuncties werken.

## Support einde

Bij naderen einde van de supportperiode informeert de fabrikant de gebruiker (**OPEN:** kanalen en periode).
