# 03 – Testplan en acceptatiecriteria

## Functionaliteit
- Alle 5 gekoppelde Shellys: knop 1..4 leveren identieke Modbus-acties op hetzelfde target.
- Zesde Shelly geweigerd; ongekoppelde EUI-64 geweigerd; router-join geeft nooit een Modbus write.
- Test duplicate EUI, short-address wijziging, leave/rejoin, router reboot, coordinator reboot, sleepy device, mesh-routeverandering.
- Dubbele Zigbee deliveries en ingedrukte knop tijdens Modbus timeout mogen geen ongewenste herhaalde actie veroorzaken.
- Modbus target niet bereikbaar: bounded retries, veilige errorstatus, geen onverwachte writes bij herstel.

## Provisioning
- Service AP is standaard uit; opent alleen na fysieke actie, sluit bij timeout/succes.
- QR toont juiste tijdelijke credentials; portal vereist authenticatie; CSRF en session expiry testen.
- Ongeldige Wi-Fi-instellingen vernietigen bestaande werkende configuratie niet.
- Zigbee permit-join sluit op tijd; onverwachte joins detecteren/verwijderen volgens stackmogelijkheden.

## OTA
- Correct gesigneerde compatibele update werkt; corrupte binary, gewijzigde manifestvelden, ongeldige signature, verkeerd board, downgrade en redirect worden geweigerd.
- Stroomuitval tijdens download/flash/boot; failed selftest; NVS schema migratie en rollback.
- Geen internet, DNS-fout, TLS-certificaatfout, GitHub rate limit en manifest 404 laten knoppen functioneren.
- Release rollout stop/hold: devices installeren geen geblokkeerde versie.
- Test 24-uurs check + jitter/backoff met gesimuleerde klok; voorkom update storm.

## Non-functioneel
- Zigbee/Wi-Fi coexistence en Modbus latentie in realistische opstelling met repeaters.
- RAM/flash usage, watchdog, boot time, 72-uurs duurtest, power cycling.
- Vulnerability scans, SBOM, secret scanning, signed release verification, secure storage review.

## Exit criteria
Geen release zonder hardwaretests, gevalideerde registertabel, ondertekende artifacts, bewezen recovery, update/rollback test en vrijgegeven dossier.
