# CI/CD

| Workflow | Status | Doel |
|---|---|---|
| `ci.yml` | Actief (hosttests) | PR/main: unit tests |
| `release.yml` | Draft-release op tag `v*` | Schema/voorbeeld + notes; **geen** productie-signing tot Environment `production-signing` + keys |

FQBN productbuild:

```text
esp32:esp32:arduino_nesso_n1:ZigbeeMode=zczr,PartitionScheme=zigbee_zczr
```

Zie `docs/08-EERSTE-FLASH-EN-OTA.md` en `scripts/sign_manifest.md`.

Zet geen live productie-signing workflow aan zonder protected environment en menselijke approval.
