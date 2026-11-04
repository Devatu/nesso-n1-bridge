#include "display_ui.h"
#include "app_config.h"
#include "log.h"
#include "modbus_client.h"
#include "wifi_manager.h"
#include "zigbee_coordinator.h"
#include <stdio.h>
#include <string.h>

#if NESSO_ENABLE_HW
#include <Arduino_Nesso_N1.h>
#include <lgfx/utility/lgfx_qrcode.h>

static NessoDisplay g_display;
static bool g_ok = false;
static HeatpumpConfig *g_hp = nullptr;
static bool g_home_drawn = false;
static bool g_zb_drawn = false;
static char g_home_sig[96];
static char g_zb_sig[128];

/* Palette (RGB565) — teal accent, rustige contrasten op 240×135. */
static const uint16_t kBg = 0x0841;       /* diep grafiet */
static const uint16_t kAccent = 0x2BB7;   /* teal */
static const uint16_t kText = 0xEF7D;     /* soft white */
static const uint16_t kMuted = 0x8410;    /* grijs */
static const uint16_t kOk = 0x2E0A;       /* groen */
static const uint16_t kWarn = 0xFDC0;     /* amber */
static const uint16_t kDanger = 0xF28A;   /* soft red-orange */
static const uint16_t kSelBg = 0x196A;    /* selectievlak */
static const uint16_t kLine = 0x2104;     /* hairline */
static const uint16_t kQrLight = 0x4A69;
static const uint16_t kQrDark = kBg;

static void clear_screen() {
  g_home_drawn = false;
  g_zb_drawn = false;
  g_display.fillScreen(kBg);
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
  g_display.drawFastHLine(6, 112, 210, kLine);
  g_display.setTextColor(kMuted);
  g_display.setTextSize(1);
  g_display.setCursor(6, 118);
  g_display.print(text ? text : "");
}

static void draw_status_dot(int x, int y, uint16_t color) {
  g_display.fillCircle(x, y, 3, color);
}

static void draw_header(const char *title) {
  g_display.fillRect(0, 0, 3, 135, kAccent);
  g_display.setTextColor(kAccent);
  g_display.setTextSize(2);
  g_display.setCursor(10, 6);
  g_display.print(title ? title : "Qvantum");
  g_display.drawFastHLine(10, 26, 200, kLine);
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
    const int x = 240 - 6 - w;
    g_display.setTextColor(kMuted);
    g_display.setCursor(x < 10 ? 10 : x, 8);
    g_display.print(top_cue);
  }
  if (side_cue != nullptr && side_cue[0] != '\0') {
    const int n = (int)strlen(side_cue);
    const int y0 = (135 - n * 12) / 2;
    draw_vertical_label(228, y0 < 30 ? 30 : y0, side_cue, kAccent);
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
  g_display.fillRect(0, 0, 3, 135, kAccent);
  g_display.setTextColor(kAccent);
  g_display.setTextSize(2);
  g_display.setCursor(14, 44);
  g_display.print("Qvantum");
  g_display.setTextColor(kMuted);
  g_display.setTextSize(1);
  g_display.setCursor(14, 72);
  g_display.print("Nesso start…");
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
    g_display.setTextColor(kWarn);
    g_display.setTextSize(1);
    g_display.setCursor(10, 118);
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

  /* Links tekstkolom (~118px), rechts QR — tekst mag QR niet overlappen. */
  const int qr_x = 130;
  const int qr_w = 104;
  const int text_max_x = qr_x - 6;

  g_display.fillRect(0, 0, 3, 135, kAccent);
  g_display.setTextColor(kAccent);
  g_display.setTextSize(1);
  g_display.setCursor(10, 6);
  g_display.print("Stap 1 · Wi-Fi");

  g_display.setTextColor(kMuted);
  g_display.setCursor(10, 24);
  g_display.print("1. Open BLE Prov");
  g_display.setCursor(10, 38);
  g_display.print("2. Scan QR / code");

  g_display.setTextColor(kWarn);
  g_display.setTextSize(2);
  g_display.setCursor(10, 58);
  /* PoP max ~8 chars @ size 2 blijft links van QR. */
  g_display.print(code);

  g_display.setTextColor(kMuted);
  g_display.setTextSize(1);
  g_display.setCursor(10, 118);
  /* Naam inkorten zodat die niet onder de QR doorloopt. */
  int max_chars = (text_max_x - 10) / 6;
  if (max_chars > 16) {
    max_chars = 16;
  }
  if (max_chars > 2 && (int)strlen(name) > max_chars) {
    char short_name[20];
    snprintf(short_name, sizeof(short_name), "%.*s..", max_chars - 2, name);
    g_display.print(short_name);
  } else {
    g_display.print(name);
  }

  draw_qr_dimmed(payload, qr_x, 10, qr_w);
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
  char ip[20];
  const bool wifi_ok = wifi_sta_ip(ip, sizeof(ip));
  const bool hp_ok = g_hp != nullptr && heatpump_config_is_ready(g_hp);
  char sig[96];
  snprintf(sig, sizeof(sig), "%d|%s|%d|%s|%u", wifi_ok ? 1 : 0, wifi_ok ? ip : "-", hp_ok ? 1 : 0,
           (hp_ok && g_hp) ? g_hp->ipv4 : "-", (hp_ok && g_hp) ? (unsigned)g_hp->unit_id : 0u);
  if (g_home_drawn && strncmp(sig, g_home_sig, sizeof(g_home_sig)) == 0) {
    return; /* geen fillScreen-flicker als er niets veranderde */
  }
  strncpy(g_home_sig, sig, sizeof(g_home_sig) - 1);
  g_home_sig[sizeof(g_home_sig) - 1] = '\0';

  clear_screen();
  g_home_drawn = true;
  draw_header("Qvantum");
  draw_nav_cues("^Menu", "Kies");

  g_display.setTextSize(1);
  if (wifi_ok) {
    draw_status_dot(14, 38, kOk);
    g_display.setTextColor(kOk);
    g_display.setCursor(22, 34);
    g_display.print("Wi-Fi verbonden");
    if (!hp_ok) {
      g_display.setTextColor(kWarn);
      g_display.setCursor(10, 52);
      g_display.print("Stap 2 · open in browser");
      g_display.setTextColor(kAccent);
      g_display.setTextSize(2);
      g_display.setCursor(10, 70);
      g_display.printf("%s.local", NESSO_MDNS_HOSTNAME);
      g_display.setTextColor(kMuted);
      g_display.setTextSize(1);
      g_display.setCursor(10, 98);
      g_display.printf("of http://%s", ip);
    } else {
      draw_status_dot(14, 56, kOk);
      g_display.setTextColor(kOk);
      g_display.setCursor(22, 52);
      g_display.print("Warmtepomp OK");
      g_display.setTextColor(kText);
      g_display.setCursor(10, 70);
      g_display.printf("%s  · unit %u", g_hp->ipv4, (unsigned)g_hp->unit_id);
      g_display.setTextColor(kAccent);
      g_display.setCursor(10, 86);
      g_display.printf("%s.local", NESSO_MDNS_HOSTNAME);
      g_display.setTextColor(kMuted);
      g_display.setCursor(10, 102);
      g_display.printf("IP %s", ip);
    }
  } else {
    draw_status_dot(14, 48, kWarn);
    g_display.setTextColor(kWarn);
    g_display.setCursor(22, 44);
    g_display.print("Geen Wi-Fi");
    g_display.setTextColor(kText);
    g_display.setCursor(10, 66);
    g_display.print("Menu → Wi-Fi opnieuw");
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
    const int y = 34 + i * 20;
    if (idx == selected) {
      g_display.fillRoundRect(8, y - 3, row_w, 18, 3, kSelBg);
      g_display.fillRect(8, y - 3, 3, 18, kAccent);
      g_display.setTextColor(kAccent);
      g_display.setCursor(16, y);
      g_display.printf("%s", ui_menu_label(ui_menu_id_at(idx)));
    } else {
      g_display.setTextColor(kText);
      g_display.setCursor(16, y);
      g_display.printf("%s", ui_menu_label(ui_menu_id_at(idx)));
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
  draw_header("Info");
  draw_nav_cues("^Menu", "Kies");
  g_display.setTextColor(kText);
  g_display.setTextSize(1);
  g_display.setCursor(10, 36);
  g_display.printf("FW  %s", NESSO_FW_VERSION);
  g_display.setTextColor(kMuted);
  g_display.setCursor(10, 52);
  g_display.printf("%s", NESSO_PRODUCT_ID);
  g_display.setTextColor(kAccent);
  g_display.setCursor(10, 72);
  g_display.printf("%s.local", NESSO_MDNS_HOSTNAME);
  char ip[20];
  if (wifi_sta_ip(ip, sizeof(ip))) {
    g_display.setTextColor(kMuted);
    g_display.setCursor(10, 88);
    g_display.printf("IP  %s", ip);
  }
#endif
}

void display_show_factory_confirm() {
#if NESSO_ENABLE_HW
  if (!g_ok) {
    return;
  }
  clear_screen();
  draw_header("Reset?");
  draw_nav_cues("^Nee", "Ja");
  g_display.setTextColor(kWarn);
  g_display.setTextSize(1);
  g_display.setCursor(10, 40);
  g_display.print("Wi-Fi + warmtepomp");
  g_display.setCursor(10, 56);
  g_display.print("instellingen wissen.");
  g_display.setTextColor(kMuted);
  g_display.setCursor(10, 80);
  g_display.print("Kan niet ongedaan.");
#endif
}

void display_show_wifi_confirm() {
#if NESSO_ENABLE_HW
  if (!g_ok) {
    return;
  }
  clear_screen();
  draw_header("Wi-Fi?");
  draw_nav_cues("^Nee", "Ja");
  g_display.setTextColor(kText);
  g_display.setTextSize(1);
  g_display.setCursor(10, 40);
  g_display.print("Huidig netwerk wissen");
  g_display.setCursor(10, 56);
  g_display.print("en opnieuw koppelen");
  g_display.setTextColor(kMuted);
  g_display.setCursor(10, 80);
  g_display.print("via de telefoon-app.");
#endif
}

void display_show_zigbee() {
#if NESSO_ENABLE_HW
  if (!g_ok) {
    return;
  }
  const bool joining = zigbee_join_is_open();
  const bool wifi_ok = wifi_sta_connected();
  const bool zb_run = zigbee_is_running();
  const bool join_ready = zigbee_join_ready();
  const int shelly_n = zigbee_shelly_count();
  const unsigned join_s = joining ? (unsigned)zigbee_join_remaining_s() : 0u;
  const unsigned join_bucket = (join_s / 5u) * 5u; /* minder fillScreen tijdens join */
  const char *ev = zigbee_last_event();
  char sig[128];
  snprintf(sig, sizeof(sig), "%d|%d|%d|%d|%d|%u|%s", joining ? 1 : 0, wifi_ok ? 1 : 0,
           zb_run ? 1 : 0, join_ready ? 1 : 0, shelly_n, join_bucket, (ev && ev[0]) ? ev : "-");
  if (g_zb_drawn && strncmp(sig, g_zb_sig, sizeof(g_zb_sig)) == 0) {
    return;
  }
  strncpy(g_zb_sig, sig, sizeof(g_zb_sig) - 1);
  g_zb_sig[sizeof(g_zb_sig) - 1] = '\0';

  clear_screen();
  g_zb_drawn = true;
  draw_header("Zigbee");

  if (!join_ready) {
    draw_nav_cues("^Menu", "Wacht");
  } else {
    draw_nav_cues(joining ? "^Wacht" : "^Menu", joining ? "Stop" : "Join");
  }

  g_display.setTextSize(1);
  g_display.setTextColor(kText);
  g_display.setCursor(10, 34);
  g_display.printf("Knoppen  %d / %d", shelly_n, NESSO_MAX_SHELLY_SLOTS);

  draw_status_dot(14, 54, wifi_ok ? kOk : kWarn);
  g_display.setTextColor(wifi_ok ? kOk : kWarn);
  g_display.setCursor(22, 50);
  g_display.print(wifi_ok ? "Wi-Fi OK" : (joining ? "Wi-Fi even weg" : "Geen Wi-Fi"));

  if (!join_ready) {
    g_display.setTextColor(kWarn);
    g_display.setCursor(10, 70);
    g_display.print("Radio start…");
    g_display.setTextColor(kMuted);
    g_display.setCursor(10, 86);
    g_display.print("Join daarna · 5× paars");
  } else if (joining) {
    g_display.setTextColor(kWarn);
    g_display.setCursor(10, 70);
    g_display.printf("Join open · %us", join_s);
    g_display.setTextColor(kMuted);
    g_display.setCursor(10, 86);
    g_display.print("5× pair, dan 1× bind");
  } else {
    g_display.setTextColor(kOk);
    g_display.setCursor(10, 70);
    g_display.print("Netwerk dicht");
    g_display.setTextColor(kMuted);
    g_display.setCursor(10, 86);
    g_display.printf("Join = %ds open", NESSO_ZIGBEE_JOIN_WINDOW_S);
  }

  if (ev != nullptr && ev[0] != '\0') {
    footer(ev);
  }
#else
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
  draw_header("Ventilatie");
  draw_nav_cues("^Volgende", "Kies");

  g_display.setTextSize(1);
  for (uint8_t i = 0; i < 5; i++) {
    const int y = 32 + (int)i * 14;
    if (i == selected) {
      g_display.fillRoundRect(8, y - 2, 200, 13, 2, kSelBg);
      g_display.fillRect(8, y - 2, 3, 13, kAccent);
      g_display.setTextColor(kAccent);
      g_display.setCursor(16, y);
    } else {
      g_display.setTextColor(kText);
      g_display.setCursor(16, y);
    }
    if (i < 4) {
      g_display.printf("%s", ventilation_mode_label(k_modes[i]));
    } else {
      g_display.print("Terug");
    }
  }
  if (status && status[0]) {
    g_display.setTextColor(kWarn);
    g_display.setCursor(10, 108);
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
  draw_header("Software");

  const bool update_ready =
      ota != nullptr && ota->availability == OTA_AVAIL_UPDATE_AVAILABLE;
  const bool downloading = ota != nullptr && ota->state == OTA_DOWNLOAD;

  if (downloading) {
    g_display.setTextColor(kWarn);
    g_display.setTextSize(1);
    g_display.setCursor(10, 48);
    g_display.print("Bezig met installeren…");
    g_display.setTextColor(kMuted);
    g_display.setCursor(10, 68);
    g_display.print("Herstart volgt.");
    footer("Niet uitschakelen");
    return;
  }

  draw_nav_cues("^Menu", update_ready ? "Ja" : "Kies");

  g_display.setTextColor(kMuted);
  g_display.setTextSize(1);
  g_display.setCursor(10, 34);
  g_display.printf("Nu  %s", ota ? ota->current_version : "?");

  const char *msg = "Nog niet gecontroleerd";
  uint16_t color = kText;
  if (ota != nullptr) {
    switch (ota->availability) {
    case OTA_AVAIL_CHECKING:
      msg = "Bezig met controleren…";
      color = kWarn;
      break;
    case OTA_AVAIL_UP_TO_DATE:
      msg = "Je hebt de nieuwste";
      color = kOk;
      break;
    case OTA_AVAIL_UPDATE_AVAILABLE:
      msg = "Update beschikbaar";
      color = kWarn;
      break;
    case OTA_AVAIL_OFFLINE:
      msg = "Geen Wi-Fi voor check";
      color = kWarn;
      break;
    case OTA_AVAIL_ERROR:
      msg = ota->last_error[0] ? ota->last_error : "Geen updateinfo";
      color = kDanger;
      break;
    default:
      break;
    }
    if (ota->remote_version[0] && update_ready) {
      g_display.setTextColor(kAccent);
      g_display.setCursor(10, 50);
      g_display.printf("Nieuw  %s", ota->remote_version);
    }
  }
  g_display.setTextColor(color);
  g_display.setCursor(10, update_ready ? 68 : 56);
  g_display.print(msg);
  if (update_ready) {
    g_display.setTextColor(kMuted);
    g_display.setCursor(10, 90);
    g_display.print("Groene knop = installeren");
  }
#else
  (void)ota;
#endif
}
