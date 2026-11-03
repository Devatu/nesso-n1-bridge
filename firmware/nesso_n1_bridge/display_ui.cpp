#include "display_ui.h"
#include "app_config.h"
#include "log.h"
#include "modbus_client.h"
#include "wifi_manager.h"
#include <stdio.h>
#include <string.h>

#if NESSO_ENABLE_HW
#include <Arduino_Nesso_N1.h>
#include <lgfx/utility/lgfx_qrcode.h>

static NessoDisplay g_display;
static bool g_ok = false;
static HeatpumpConfig *g_hp = nullptr;

/* Gedimde QR: backlight is alleen aan/uit; minder wit = minder camera-glare. */
static const uint16_t kQrLight = 0x5ACB;
static const uint16_t kQrDark = TFT_BLACK;

static void clear_screen() {
  g_display.fillScreen(TFT_BLACK);
  g_display.setTextWrap(false);
}

static void draw_qr_dimmed(const char *payload, int32_t x, int32_t y, int32_t w) {
  if (payload == nullptr || w <= 0) {
    return;
  }
  for (uint8_t version = 1; version <= 40; ++version) {
    QRCode qrcode;
    auto *qrcodeData = (uint8_t *)alloca(lgfx_qrcode_getBufferSize(version));
    if (0 != lgfx_qrcode_initText(&qrcode, qrcodeData, version, 0, payload)) {
      continue;
    }
    const int_fast16_t thickness = w / qrcode.size;
    if (thickness <= 0) {
      break;
    }
    const int_fast16_t lineLength = qrcode.size * thickness;
    const int_fast16_t offset = (w - lineLength) >> 1;

    g_display.fillRect(x, y, w, w, kQrLight);
    int_fast16_t dy = y + offset;
    for (int_fast16_t iy = 0; iy < qrcode.size; ++iy) {
      int_fast16_t dx = x + offset;
      for (int_fast16_t ix = 0; ix < qrcode.size; ++ix) {
        if (lgfx_qrcode_getModule(&qrcode, ix, iy)) {
          g_display.fillRect(dx, dy, thickness, thickness, kQrDark);
        }
        dx += thickness;
      }
      dy += thickness;
    }
    break;
  }
}

static void footer(const char *text) {
  g_display.setTextColor(TFT_DARKGREY);
  g_display.setTextSize(1);
  g_display.setCursor(6, 118);
  g_display.print(text ? text : "");
}

static void draw_header(const char *title) {
  g_display.setTextColor(TFT_CYAN);
  g_display.setTextSize(2);
  g_display.setCursor(6, 6);
  g_display.print(title ? title : "Qvantum");
}

/* Verticale label rechts naast de groene knop (1 char per regel). */
static void draw_vertical_label(int x, int y0, const char *text, uint16_t color) {
  if (text == nullptr || text[0] == '\0') {
    return;
  }
  g_display.setTextColor(color);
  g_display.setTextSize(1);
  int y = y0;
  for (const char *p = text; *p; ++p) {
    char ch[2] = {*p, '\0'};
    g_display.setCursor(x, y);
    g_display.print(ch);
    y += 12;
  }
}

/* Knop2 boven → hint rechtsboven; knop1 rechts → verticale hint. */
static void draw_nav_cues(const char *top_cue, const char *side_cue) {
  g_display.setTextSize(1);
  if (top_cue != nullptr && top_cue[0] != '\0') {
    const int w = (int)strlen(top_cue) * 6;
    const int x = 240 - 4 - w; /* zo ver mogelijk naar rechts */
    g_display.setTextColor(TFT_DARKGREY);
    g_display.setCursor(x < 0 ? 0 : x, 10);
    g_display.print(top_cue);
  }
  if (side_cue != nullptr && side_cue[0] != '\0') {
    const int n = (int)strlen(side_cue);
    const int y0 = (135 - n * 12) / 2;
    draw_vertical_label(228, y0 < 28 ? 28 : y0, side_cue, TFT_GREEN);
  }
}
#endif

void display_bind_heatpump(HeatpumpConfig *hp) {
#if NESSO_ENABLE_HW
  g_hp = hp;
#else
  (void)hp;
#endif
}

bool display_begin() {
#if NESSO_ENABLE_HW
  g_ok = g_display.begin();
  if (!g_ok) {
    LOG_E("display", "NessoDisplay begin failed");
    return false;
  }
  g_display.setRotation(1);
  clear_screen();
  g_display.setTextColor(TFT_CYAN);
  g_display.setTextSize(2);
  g_display.setCursor(6, 48);
  g_display.print("Qvantum");
  g_display.setTextColor(TFT_DARKGREY);
  g_display.setTextSize(1);
  g_display.setCursor(6, 78);
  g_display.print("Even geduld…");
  LOG_I("display", "OK 240x135");
  return true;
#else
  LOG_W("display", "scaffold (no panel)");
  return false;
#endif
}

void display_show_status(const char *line) {
  LOG_I("display", "%s", line ? line : "");
#if NESSO_ENABLE_HW
  if (!g_ok) {
    return;
  }
  display_show_home();
  if (line && line[0]) {
    g_display.setTextColor(TFT_YELLOW);
    g_display.setTextSize(1);
    g_display.setCursor(6, 118);
    g_display.print(line);
  }
#else
  (void)line;
#endif
}

void display_show_service_qr(const char *payload) {
  display_show_ble_prov(payload, nullptr);
}

void display_show_ble_prov(const char *service_name, const char *pop) {
  LOG_I("display", "BLE setup screen + QR");
#if NESSO_ENABLE_HW
  if (!g_ok) {
    return;
  }
  clear_screen();

  const char *name = service_name && service_name[0] ? service_name : "PROV_NESSO";
  const char *code = pop && pop[0] ? pop : "--------";

  /* Espressif BLE Prov app QR payload (zie WiFiProv::printQR). */
  char payload[160];
  snprintf(payload, sizeof(payload),
           "{\"ver\":\"v1\",\"name\":\"%s\",\"pop\":\"%s\",\"transport\":\"ble\"}", name, code);

  /* Links: korte uitleg + code. Rechts: gedimde QR (minder glare). */
  g_display.setTextColor(0x5ACB);
  g_display.setTextSize(1);
  g_display.setCursor(4, 4);
  g_display.print("Stap 1: Wi-Fi");

  g_display.setTextColor(0x8410);
  g_display.setCursor(4, 22);
  g_display.print("1. Open ESP BLE");
  g_display.setCursor(4, 34);
  g_display.print("   Provisioning");
  g_display.setCursor(4, 50);
  g_display.print("2. Scan QR-code");
  g_display.setCursor(4, 66);
  g_display.print("   of vul code in:");

  g_display.setTextColor(0xC600); /* gedempt geel */
  g_display.setTextSize(2);
  g_display.setCursor(4, 84);
  g_display.print(code);

  g_display.setTextColor(TFT_DARKGREY);
  g_display.setTextSize(1);
  g_display.setCursor(4, 118);
  g_display.printf("%s", name);

  draw_qr_dimmed(payload, 122, 6, 112);
#else
  (void)service_name;
  (void)pop;
#endif
}

void display_show_home() {
#if NESSO_ENABLE_HW
  if (!g_ok) {
    return;
  }
  clear_screen();
  draw_header("Qvantum");
  draw_nav_cues("^Menu", "Kies");

  char ip[20];
  const bool wifi_ok = wifi_sta_ip(ip, sizeof(ip));
  const bool hp_ok = g_hp != nullptr && heatpump_config_is_ready(g_hp);

  g_display.setTextSize(1);
  if (wifi_ok) {
    g_display.setTextColor(TFT_GREEN);
    g_display.setCursor(6, 30);
    g_display.print("Wi-Fi OK");
    if (!hp_ok) {
      g_display.setTextColor(TFT_YELLOW);
      g_display.setCursor(6, 46);
      g_display.print("Stap 2: open in browser");
      g_display.setTextColor(TFT_CYAN);
      g_display.setTextSize(2);
      g_display.setCursor(6, 66);
      g_display.printf("%s.local", NESSO_MDNS_HOSTNAME);
      g_display.setTextColor(TFT_DARKGREY);
      g_display.setTextSize(1);
      g_display.setCursor(6, 96);
      g_display.printf("of http://%s", ip);
    } else {
      g_display.setTextColor(TFT_GREEN);
      g_display.setCursor(6, 46);
      g_display.print("Warmtepomp: ingesteld");
      g_display.setTextColor(TFT_WHITE);
      g_display.setCursor(6, 64);
      g_display.printf("%s  unit %u", g_hp->ipv4, (unsigned)g_hp->unit_id);
      g_display.setTextColor(TFT_CYAN);
      g_display.setCursor(6, 82);
      g_display.printf("%s.local", NESSO_MDNS_HOSTNAME);
      g_display.setTextColor(TFT_DARKGREY);
      g_display.setCursor(6, 98);
      g_display.printf("IP %s", ip);
    }
  } else {
    g_display.setTextColor(TFT_ORANGE);
    g_display.setCursor(6, 42);
    g_display.print("Geen Wi-Fi");
    g_display.setTextColor(TFT_WHITE);
    g_display.setCursor(6, 62);
    g_display.print("Menu > Wi-Fi opnieuw");
  }
#endif
}

void display_show_menu(uint8_t selected) {
#if NESSO_ENABLE_HW
  if (!g_ok) {
    return;
  }
  if (selected >= (uint8_t)UI_MENU_COUNT) {
    selected = 0;
  }
  clear_screen();
  draw_header("Menu");
  draw_nav_cues("^Volgende", "Kies");

  /* 4 regels; rechtsruimte vrijhouden voor verticaal "Kies". */
  const int window = 4;
  const int row_w = 200;
  int start = (int)selected - 1;
  if (start < 0) {
    start = 0;
  }
  if (start > (int)UI_MENU_COUNT - window) {
    start = (int)UI_MENU_COUNT - window;
  }
  if (start < 0) {
    start = 0;
  }

  g_display.setTextSize(1);
  for (int i = 0; i < window && (start + i) < (int)UI_MENU_COUNT; i++) {
    const uint8_t idx = (uint8_t)(start + i);
    const int y = 32 + i * 22;
    if (idx == selected) {
      g_display.fillRect(4, y - 2, row_w, 18, TFT_NAVY);
      g_display.setTextColor(TFT_YELLOW);
      g_display.setCursor(8, y);
      g_display.printf("> %s", ui_menu_label(ui_menu_id_at(idx)));
    } else {
      g_display.setTextColor(TFT_WHITE);
      g_display.setCursor(8, y);
      g_display.printf("  %s", ui_menu_label(ui_menu_id_at(idx)));
    }
  }
#else
  (void)selected;
#endif
}

void display_show_info() {
#if NESSO_ENABLE_HW
  if (!g_ok) {
    return;
  }
  clear_screen();
  g_display.setTextColor(TFT_CYAN);
  g_display.setTextSize(2);
  g_display.setCursor(6, 4);
  g_display.print("Info");
  draw_nav_cues("^Menu", "Kies");
  g_display.setTextColor(TFT_WHITE);
  g_display.setTextSize(1);
  g_display.setCursor(6, 36);
  g_display.printf("FW %s", NESSO_FW_VERSION);
  g_display.setCursor(6, 52);
  g_display.printf("Product %s", NESSO_PRODUCT_ID);
  g_display.setCursor(6, 68);
  g_display.printf("Host %s.local", NESSO_MDNS_HOSTNAME);
  char ip[20];
  if (wifi_sta_ip(ip, sizeof(ip))) {
    g_display.setCursor(6, 84);
    g_display.printf("IP %s", ip);
  }
#endif
}

void display_show_factory_confirm() {
#if NESSO_ENABLE_HW
  if (!g_ok) {
    return;
  }
  clear_screen();
  g_display.setTextColor(TFT_ORANGE);
  g_display.setTextSize(2);
  g_display.setCursor(6, 8);
  g_display.print("Reset?");
  draw_nav_cues("^Nee", "Ja");
  g_display.setTextColor(TFT_WHITE);
  g_display.setTextSize(1);
  g_display.setCursor(6, 44);
  g_display.print("Wi-Fi en warmtepomp-");
  g_display.setCursor(6, 60);
  g_display.print("instellingen wissen.");
#endif
}

void display_show_wifi_confirm() {
#if NESSO_ENABLE_HW
  if (!g_ok) {
    return;
  }
  clear_screen();
  g_display.setTextColor(TFT_ORANGE);
  g_display.setTextSize(2);
  g_display.setCursor(6, 8);
  g_display.print("Wi-Fi opnieuw?");
  draw_nav_cues("^Nee", "Ja");
  g_display.setTextColor(TFT_WHITE);
  g_display.setTextSize(1);
  g_display.setCursor(6, 44);
  g_display.print("Huidig netwerk wissen");
  g_display.setCursor(6, 60);
  g_display.print("en opnieuw koppelen");
  g_display.setCursor(6, 76);
  g_display.print("via de telefoon-app.");
#endif
}

void display_show_modbus_test(uint8_t selected, const char *status) {
#if NESSO_ENABLE_HW
  if (!g_ok) {
    return;
  }
  /* 0 Uit, 1 Gereduceerd, 2 Normaal, 3 Extra, 4 Terug — zelfde volgorde als knoppen later */
  static const uint16_t k_modes[] = {VENT_OFF, VENT_REDUCED, VENT_NORMAL, VENT_EXTRA};
  if (selected > 4) {
    selected = 4;
  }

  clear_screen();
  g_display.setTextColor(TFT_CYAN);
  g_display.setTextSize(2);
  g_display.setCursor(6, 4);
  g_display.print("Ventilatie");
  draw_nav_cues("^Volgende", "Kies");

  g_display.setTextSize(1);
  for (uint8_t i = 0; i < 5; i++) {
    const int y = 28 + (int)i * 14;
    if (i == selected) {
      g_display.fillRect(4, y - 1, 200, 13, TFT_NAVY);
      g_display.setTextColor(TFT_YELLOW);
      g_display.setCursor(8, y);
      g_display.print("> ");
    } else {
      g_display.setTextColor(TFT_WHITE);
      g_display.setCursor(8, y);
      g_display.print("  ");
    }
    if (i < 4) {
      g_display.printf("%s (%u)", ventilation_mode_label(k_modes[i]), (unsigned)k_modes[i]);
    } else {
      g_display.print("Terug");
    }
  }
  if (status && status[0]) {
    g_display.setTextColor(TFT_ORANGE);
    g_display.setCursor(6, 102);
    g_display.print(status);
  }
#else
  (void)selected;
  (void)status;
#endif
}

void display_show_ota(const OtaStatus *ota) {
#if NESSO_ENABLE_HW
  if (!g_ok) {
    return;
  }
  clear_screen();
  g_display.setTextColor(TFT_CYAN);
  g_display.setTextSize(2);
  g_display.setCursor(6, 4);
  g_display.print("Software");

  const bool update_ready =
      ota != nullptr && ota->availability == OTA_AVAIL_UPDATE_AVAILABLE;
  const bool downloading = ota != nullptr && ota->state == OTA_DOWNLOAD;

  if (downloading) {
    g_display.setTextColor(TFT_YELLOW);
    g_display.setTextSize(1);
    g_display.setCursor(6, 52);
    g_display.print("Bezig met installeren…");
    g_display.setTextColor(TFT_DARKGREY);
    g_display.setCursor(6, 72);
    g_display.print("Even geduld, herstart volgt.");
    footer("Niet uitschakelen");
    return;
  }

  draw_nav_cues("^Menu", update_ready ? "Ja" : "Kies");

  g_display.setTextColor(TFT_WHITE);
  g_display.setTextSize(1);
  g_display.setCursor(6, 30);
  g_display.printf("Nu: %s", ota ? ota->current_version : "?");

  const char *msg = "Nog niet gecontroleerd";
  uint16_t color = TFT_WHITE;
  if (ota != nullptr) {
    switch (ota->availability) {
    case OTA_AVAIL_CHECKING:
      msg = "Bezig met controleren…";
      color = TFT_YELLOW;
      break;
    case OTA_AVAIL_UP_TO_DATE:
      msg = "Je hebt de nieuwste versie";
      color = TFT_GREEN;
      break;
    case OTA_AVAIL_UPDATE_AVAILABLE:
      msg = "Nieuwe firmware beschikbaar";
      color = TFT_YELLOW;
      break;
    case OTA_AVAIL_OFFLINE:
      msg = "Geen Wi-Fi voor check";
      color = TFT_ORANGE;
      break;
    case OTA_AVAIL_ERROR:
      msg = ota->last_error[0] ? ota->last_error : "Geen updateinfo (nog)";
      color = TFT_ORANGE;
      break;
    default:
      break;
    }
    if (ota->remote_version[0] && update_ready) {
      g_display.setCursor(6, 46);
      g_display.printf("Nieuw: %s", ota->remote_version);
    }
  }
  g_display.setTextColor(color);
  g_display.setCursor(6, update_ready ? 62 : 52);
  g_display.print(msg);
  if (update_ready) {
    g_display.setTextColor(TFT_DARKGREY);
    g_display.setCursor(6, 86);
    g_display.print("Groene knop = installeren");
  }
#else
  (void)ota;
#endif
}
