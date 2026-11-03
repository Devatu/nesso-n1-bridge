#include "installer_portal.h"
#include "app_config.h"
#include "display_ui.h"
#include "heatpump_config.h"
#include "log.h"
#include "log_ring.h"
#include "modbus_client.h"
#include "portal_api.h"
#include "storage.h"
#include "wifi_manager.h"
#include <stdio.h>
#include <string.h>

#if NESSO_ENABLE_HW
#include <WebServer.h>
static WebServer g_server(80);
#endif

static bool g_running = false;
static HeatpumpConfig *g_hp = nullptr;
static OtaStatus *g_ota = nullptr;
static AppConfigStore *g_store = nullptr;

#if NESSO_ENABLE_HW
static const char *PAGE_CSS =
    "body{font-family:system-ui,sans-serif;max-width:28rem;margin:1.5rem auto;padding:0 1rem;"
    "background:#0f1419;color:#e7ecf1}"
    "h1{font-size:1.35rem;margin:0 0 .25rem}"
    "p.lead{color:#9aa7b5;margin:0 0 1.25rem}"
    "section{background:#1a222c;border-radius:12px;padding:1rem 1.1rem;margin:0 0 1rem}"
    "label{display:block;font-size:.85rem;color:#9aa7b5;margin:.65rem 0 .25rem}"
    "input{width:100%;box-sizing:border-box;padding:.55rem .65rem;border-radius:8px;"
    "border:1px solid #2c3845;background:#0f1419;color:#e7ecf1}"
    "button{margin-top:1rem;width:100%;padding:.7rem;border:0;border-radius:8px;"
    "background:#2dd4bf;color:#042f2e;font-weight:600}"
    "button.secondary{background:#334155;color:#e7ecf1}"
    ".ok{color:#4ade80}.warn{color:#fbbf24}.muted{color:#9aa7b5;font-size:.85rem}"
    "ol{margin:.5rem 0 0 1.1rem;padding:0;color:#c5d0db}"
    "li{margin:.35rem 0}";

static void send_home() {
  char ip[20] = "";
  wifi_sta_ip(ip, sizeof(ip));
  const bool hp_ok = g_hp && heatpump_config_is_ready(g_hp);

  String html;
  html.reserve(3500);
  html += "<!doctype html><html lang=nl><meta charset=utf-8>";
  html += "<meta name=viewport content=\"width=device-width,initial-scale=1\">";
  html += "<title>Nesso installatie</title><style>";
  html += PAGE_CSS;
  html += "</style><body>";
  html += "<h1>Nesso installatie</h1>";
  html += "<p class=lead>Open <strong>http://";
  html += NESSO_MDNS_HOSTNAME;
  html += ".local</strong> op dit Wi-Fi-netwerk. Geen cloud.</p>";

  html += "<section><strong>Stap 1 — Wi-Fi</strong><br>";
  if (wifi_sta_connected()) {
    html += "<span class=ok>Verbonden</span><br>";
    html += "<span class=muted>http://";
    html += NESSO_MDNS_HOSTNAME;
    html += ".local";
    if (ip[0]) {
      html += " · IP ";
      html += ip;
    }
    html += "</span>";
  } else {
    html += "<span class=warn>Niet verbonden</span>";
  }
  html += "</section>";

  html += "<section><strong>Stap 2 — Warmtepomp</strong>";
  if (hp_ok) {
    html += "<p class=ok>Ingesteld: ";
    html += g_hp->ipv4;
    html += " · poort ";
    html += String(g_hp->tcp_port);
    html += " · unit ";
    html += String(g_hp->unit_id);
    html += "</p>";
  } else {
    html += "<p class=warn>Nog niet ingesteld — vul hieronder in.</p>";
  }
  html += "<form method=POST action=/api/heatpump>";
  html += "<label>IP-adres warmtepomp</label>";
  html += "<input name=ipv4 required placeholder=192.168.1.50 value=\"";
  if (g_hp && g_hp->ipv4[0]) {
    html += g_hp->ipv4;
  }
  html += "\">";
  html += "<label>Poort (meestal 502)</label>";
  html += "<input name=port type=number min=1 max=65535 value=";
  html += String(g_hp && g_hp->tcp_port ? g_hp->tcp_port : NESSO_HEATPUMP_PORT_DEFAULT);
  html += ">";
  html += "<label>Unit ID (meestal 1)</label>";
  html += "<input name=unit_id type=number min=0 max=247 value=";
  html += String(g_hp && g_hp->configured ? g_hp->unit_id : NESSO_HEATPUMP_UNIT_DEFAULT);
  html += ">";
  html += "<label>Naam (optioneel)</label>";
  html += "<input name=label maxlength=23 placeholder=Warmtepomp value=\"";
  if (g_hp && g_hp->label[0]) {
    html += g_hp->label;
  }
  html += "\">";
  html += "<button type=submit>Opslaan</button></form>";
  html += "<p class=muted>Geen vrije register-editor — alleen vaste ventilatie-acties.</p>";
  html += "<button class=secondary type=button id=mbbtn onclick=\"testMb()\">Test Modbus-verbinding</button>";
  html += "<p class=muted id=mbstatus></p>";
  html += "<script>async function testMb(){const s=document.getElementById('mbstatus');"
          "const b=document.getElementById('mbbtn');b.disabled=true;s.textContent='Bezig…';s.className='muted';"
          "try{let r=await fetch('/api/modbus/probe',{method:'POST'});let j=await r.json();"
          "while(j.state==='busy'){await new Promise(x=>setTimeout(x,400));"
          "r=await fetch('/api/modbus/status');j=await r.json()}"
          "s.className=j.ok?'ok':'warn';s.textContent=j.msg||JSON.stringify(j)}"
          "catch(e){s.className='warn';s.textContent='Portal/timeout — probeer opnieuw'}"
          "b.disabled=false}</script>";
  html += "</section>";

  html += "<section><strong>Stap 3 — Knoppen</strong>";
  html += "<ol><li>Zigbee-knoppen in de buurt houden</li>";
  html += "<li>Later: knoppen koppelen in de setup</li></ol></section>";

  html += "<section><strong>Software</strong><p class=muted>Huidige versie: ";
  html += NESSO_FW_VERSION;
  html += "</p>";
  html += "<button class=secondary type=button onclick=\"otaCheck()\">Controleren op update</button>";
  html += "<button class=secondary type=button onclick=\"otaApply()\">Installeren</button>";
  html += "<p class=muted id=otastatus></p>";
  html += "<script>async function otaCheck(){const s=document.getElementById('otastatus');"
          "s.textContent='Controleren…';try{const r=await fetch('/api/ota/check',{method:'POST'});"
          "const j=await r.json();s.textContent=j.availability+' remote='+(j.remote||'')+"
          "' err='+(j.last_error||'')}catch(e){s.textContent='OTA-check mislukt'}}"
          "async function otaApply(){const s=document.getElementById('otastatus');"
          "s.textContent='Installeren… (apparaat herstart bij succes)';"
          "try{const r=await fetch('/api/ota/apply',{method:'POST'});"
          "const j=await r.json();s.textContent=j.availability+' err='+(j.last_error||'')}"
          "catch(e){s.textContent='Apply gestart of verbinding verbroken (herstart?)'}}</script>";
  html += "</section>";

  html += "<section><strong>Log</strong>";
  html += "<p class=muted>Laatste regels van het apparaat (ook zonder USB).</p>";
  html += "<pre id=logbox style=\"white-space:pre-wrap;font-size:.75rem;max-height:14rem;"
          "overflow:auto;background:#0f1419;padding:.6rem;border-radius:8px\">Laden…</pre>";
  html += "<button class=secondary type=button onclick=\"refreshLog()\">Log vernieuwen</button>";
  html += "<script>async function refreshLog(){try{const r=await fetch('/api/logs');"
          "document.getElementById('logbox').textContent=await r.text()}catch(e)"
          "{document.getElementById('logbox').textContent='Log ophalen mislukt'}}"
          "refreshLog();setInterval(refreshLog,3000);</script></section>";

  html += "<p class=muted>Wi-Fi opnieuw? Houd knop 1 op het apparaat in.</p>";
  html += "</body></html>";
  g_server.send(200, "text/html; charset=utf-8", html);
}

static void handle_root() {
  send_home();
}

static void handle_heatpump_post() {
  PortalHeatpumpPut put;
  memset(&put, 0, sizeof(put));
  String body = "ipv4=" + g_server.arg("ipv4") + "&port=" + g_server.arg("port") +
                "&unit_id=" + g_server.arg("unit_id") + "&label=" + g_server.arg("label");
  if (!portal_parse_heatpump_form(body.c_str(), &put) || g_hp == nullptr) {
    g_server.send(400, "text/plain; charset=utf-8", "Ongeldige gegevens. Controleer IP, poort en unit.");
    return;
  }
  if (portal_apply_heatpump_put(g_hp, &put) != (int)HP_OK) {
    g_server.send(400, "text/plain; charset=utf-8", "Kon warmtepomp niet opslaan.");
    return;
  }
  if (g_store != nullptr) {
    storage_save_heatpump(g_store, g_hp);
  }
  display_show_home();
  g_server.sendHeader("Location", "/");
  g_server.send(303, "text/plain", "OK");
}

static void handle_logs() {
  /* Genoeg voor ~48 regels */
  static char buf[8192];
  log_ring_copy(buf, sizeof(buf));
  g_server.send(200, "text/plain; charset=utf-8", buf);
}

static void handle_modbus_status() {
  char json[160];
  if (modbus_async_busy()) {
    g_server.send(200, "application/json", "{\"state\":\"busy\",\"ok\":false,\"msg\":\"Bezig…\"}");
    return;
  }
  ModbusResult r = MB_OK;
  uint16_t mode = 0;
  if (!modbus_async_take_result(&r, &mode)) {
    g_server.send(200, "application/json", "{\"state\":\"idle\",\"ok\":false,\"msg\":\"Idle\"}");
    return;
  }
  if (r == MB_OK) {
    snprintf(json, sizeof(json),
             "{\"state\":\"done\",\"ok\":true,\"msg\":\"Modbus OK → %s\"}",
             ventilation_mode_label(mode));
  } else if (r == MB_ERR_CONNECT) {
    snprintf(json, sizeof(json),
             "{\"state\":\"done\",\"ok\":false,\"msg\":\"Poort 502 dicht\"}");
  } else {
    snprintf(json, sizeof(json),
             "{\"state\":\"done\",\"ok\":false,\"msg\":\"%s — unit-ID / Enable writing\"}",
             modbus_result_text(r));
  }
  g_server.send(200, "application/json", json);
}

static void handle_modbus_probe() {
  if (g_hp == nullptr || !heatpump_config_is_ready(g_hp)) {
    g_server.send(200, "application/json",
                  "{\"state\":\"done\",\"ok\":false,\"msg\":\"Eerst warmtepomp-IP\"}");
    return;
  }
  modbus_set_target(g_hp);
  if (modbus_async_busy() || !modbus_async_start_probe()) {
    g_server.send(200, "application/json", "{\"state\":\"busy\",\"ok\":false,\"msg\":\"Bezig…\"}");
    return;
  }
  g_server.send(200, "application/json", "{\"state\":\"busy\",\"ok\":false,\"msg\":\"Bezig…\"}");
}

static void handle_ota_get() {
  char json[256];
  portal_format_ota_json(g_ota, json, sizeof(json));
  g_server.send(200, "application/json", json);
}

static void handle_ota_check() {
  if (g_ota != nullptr) {
    g_ota->availability = OTA_AVAIL_CHECKING;
    display_show_ota(g_ota);
    ota_check_for_ui(g_ota);
    display_show_ota(g_ota);
  }
  handle_ota_get();
}

static void handle_ota_apply() {
  if (g_ota != nullptr) {
    ota_apply_available(g_ota);
  }
  handle_ota_get();
}

static void handle_forbidden() {
  g_server.send(403, "text/plain; charset=utf-8", "Niet toegestaan.");
}

static void handle_not_found() {
  if (portal_path_is_register_editor(g_server.uri().c_str())) {
    handle_forbidden();
    return;
  }
  g_server.send(404, "text/plain; charset=utf-8", "Niet gevonden");
}
#endif

bool portal_begin(HeatpumpConfig *heatpump, OtaStatus *ota) {
  return portal_begin_with_store(heatpump, ota, nullptr);
}

bool portal_begin_with_store(HeatpumpConfig *heatpump, OtaStatus *ota, AppConfigStore *store) {
  g_hp = heatpump;
  g_ota = ota;
  g_store = store;
#if NESSO_ENABLE_HW
  g_server.on("/", HTTP_GET, handle_root);
  g_server.on("/api/heatpump", HTTP_POST, handle_heatpump_post);
  g_server.on("/api/modbus/probe", HTTP_POST, handle_modbus_probe);
  g_server.on("/api/modbus/status", HTTP_GET, handle_modbus_status);
  g_server.on("/api/logs", HTTP_GET, handle_logs);
  g_server.on("/api/ota", HTTP_GET, handle_ota_get);
  g_server.on("/api/ota/check", HTTP_POST, handle_ota_check);
  g_server.on("/api/ota/apply", HTTP_POST, handle_ota_apply);
  g_server.on("/api/registers", HTTP_ANY, handle_forbidden);
  g_server.onNotFound(handle_not_found);
  g_server.begin();
  char ip[20] = "";
  wifi_sta_ip(ip, sizeof(ip));
  LOG_I("portal", "installatiepagina http://%s/ (LAN only)", ip[0] ? ip : "?.?.?.?");
#else
  LOG_I("portal", "scaffold portal");
#endif
  g_running = true;
  return true;
}

void portal_poll() {
  if (!g_running) {
    return;
  }
#if NESSO_ENABLE_HW
  g_server.handleClient();
#endif
}

void portal_stop() {
#if NESSO_ENABLE_HW
  if (g_running) {
    g_server.stop();
  }
#endif
  g_running = false;
}

bool portal_is_running() {
  return g_running;
}

HeatpumpConfig *portal_heatpump() {
  return g_hp;
}

OtaStatus *portal_ota() {
  return g_ota;
}
