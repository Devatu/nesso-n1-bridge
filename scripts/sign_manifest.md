# Manifest signing (procedure)

**Geen private keys in deze repo.**

## Eenmalig (productie later)

```powershell
# Example ECDSA P-256 keypair (OpenSSL). Private key → GitHub Environment secret only.
openssl ecparam -name prime256v1 -genkey -noout -out ota_private.pem
openssl ec -in ota_private.pem -pubout -out ota_public.pem
```

- Embed `ota_public.pem` contents in firmware (compile-time).  
- Store `ota_private.pem` in GitHub Actions Environment `production-signing` (required reviewers).  
- Rotate via dual-key window (document in SharePoint `07_`).

## Canonical bytes to sign

Sign the UTF-8 JSON **without** the `signature` field, keys sorted, no insignificant whitespace — exact canonicalization must be identical on CI and device. Document the chosen canonical form in the first signed release notes before production.

## CI step (concept)

1. Build `.bin`  
2. SHA-256 of `.bin` → manifest `sha256` / `size_bytes` / `asset_url`  
3. Sign canonical manifest → `signature`  
4. Upload immutable Release assets  
5. Human approval on protected environment  

Device never stores a GitHub token.
