# Assets (logo)

- `qvantum-logo.png` — bronlogo (Qvantum)
- `convert_logo.py` — maakt `../nesso_n1_bridge/qvantum_logo.h` (24×24 RGB565)
- Op het scherm: `pushImage` van die bitmap

Logo vernieuwen:

```powershell
python firmware\assets\convert_logo.py
```

Daarna firmware opnieuw bouwen/flashen.
