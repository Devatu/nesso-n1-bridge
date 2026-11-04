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
#include "zigbee_coordinator.h"
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
    ":root{--bg:#0c1218;--panel:#151d27;--line:#243041;--text:#e8eef4;--muted:#8b9aab;"
    "--teal:#2dd4bf;--teal-dim:#115e59;--ok:#34d399;--warn:#fbbf24;--danger:#f87171}"
    "*{box-sizing:border-box}"
    "body{margin:0;font-family:'Segoe UI',ui-sans-serif,system-ui,sans-serif;color:var(--text);"
    "background:radial-gradient(1200px 500px at 10% -10%,#123030 0%,transparent 55%),"
    "radial-gradient(900px 400px at 100% 0%,#1a2433 0%,transparent 50%),var(--bg);"
    "line-height:1.45}"
    ".wrap{max-width:34rem;margin:0 auto;padding:1.5rem 1.1rem 2.5rem}"
    ".brand{font-size:1.75rem;font-weight:700;letter-spacing:-.03em;color:var(--teal);margin:0}"
    ".sub{margin:.2rem 0 0;font-size:1.05rem;font-weight:600;color:var(--text)}"
    ".lead{color:var(--muted);margin:.55rem 0 1.4rem;font-size:.95rem}"
    ".lead code{color:var(--teal);font-family:ui-monospace,Consolas,monospace;font-size:.9em}"
    "section{background:var(--panel);border:1px solid var(--line);border-radius:14px;"
    "padding:1.05rem 1.15rem;margin:0 0 .9rem}"
    ".sec-h{display:flex;align-items:center;gap:.55rem;margin:0 0 .65rem}"
    ".step{flex:0 0 auto;width:1.55rem;height:1.55rem;border-radius:999px;background:var(--teal-dim);"
    "color:var(--teal);font-size:.75rem;font-weight:700;display:grid;place-items:center}"
    ".sec-h h2{margin:0;font-size:1rem;font-weight:650}"
    "label{display:block;font-size:.8rem;color:var(--muted);margin:.7rem 0 .28rem}"
    "input{width:100%;padding:.6rem .7rem;border-radius:9px;border:1px solid var(--line);"
    "background:#0c1218;color:var(--text);font-size:1rem}"
    "input:focus{outline:2px solid rgba(45,212,191,.35);border-color:var(--teal)}"
    ".row{display:flex;gap:.5rem;flex-wrap:wrap;margin-top:.85rem}"
    "button{appearance:none;border:0;border-radius:9px;padding:.65rem .95rem;font-weight:650;"
    "font-size:.92rem;cursor:pointer;background:var(--teal);color:#042f2e}"
    "button:disabled{opacity:.55;cursor:wait}"
    "button.secondary{background:#243041;color:var(--text)}"
    "button.ghost{background:transparent;color:var(--muted);border:1px solid var(--line)}"
    "button.tiny{padding:.3rem .55rem;font-size:.78rem;font-weight:600}"
    ".ok{color:var(--ok)}.warn{color:var(--warn)}.muted{color:var(--muted);font-size:.85rem}"
    ".pill{display:inline-flex;align-items:center;gap:.4rem;padding:.2rem .55rem;border-radius:999px;"
    "background:#0c1218;border:1px solid var(--line);font-size:.8rem}"
    ".dot{width:.45rem;height:.45rem;border-radius:50%;background:var(--muted)}"
    ".dot.on{background:var(--ok)}.dot.warn{background:var(--warn)}"
    "ol{margin:.35rem 0 .7rem 1.15rem;padding:0;color:#c2cedb;font-size:.9rem}"
    "li{margin:.28rem 0}"
    ".device{display:flex;align-items:center;justify-content:space-between;gap:.6rem;"
    "padding:.55rem .65rem;margin:.4rem 0;border:1px solid var(--line);border-radius:10px;"
    "background:#101821}"
    ".device b{font-weight:650}"
    "pre{white-space:pre-wrap;word-break:break-word;font-size:.72rem;max-height:16rem;overflow:auto;"
    "background:#0c1218;border:1px solid var(--line);border-radius:10px;padding:.7rem;margin:.55rem 0 0;"
    "color:#b8c6d4;font-family:ui-monospace,Consolas,monospace}"
    ".foot{margin-top:1.1rem;color:var(--muted);font-size:.82rem;text-align:center}";

static void send_home() {
  char ip[20] = "";
  wifi_sta_ip(ip, sizeof(ip));
  const bool hp_ok = g_hp && heatpump_config_is_ready(g_hp);
  const bool wifi_ok = wifi_sta_connected();

  String html;
  html.reserve(7800);
  html += "<!doctype html><html lang=nl><meta charset=utf-8>";
  html += "<meta name=viewport content=\"width=device-width,initial-scale=1\">";
  html += "<title>Qvantum · Nesso</title><style>";
  html += PAGE_CSS;
  html += "</style><body><div class=wrap>";
  html += "<p class=brand>Qvantum</p>";
  html += "<p class=sub>Nesso installatie</p>";
  html += "<p class=lead>Open <code>http://";
  html += NESSO_MDNS_HOSTNAME;
  html += ".local</code> op dit Wi-Fi. Lokaal — geen cloud.</p>";

  html += "<section><div class=sec-h><span class=step>1</span><h2>Wi-Fi</h2></div>";
  if (wifi_ok) {
    html += "<span class=pill><span class=\"dot on\"></span><span class=ok>Verbonden</span></span>";
    html += "<p class=muted style=\"margin:.55rem 0 0\">";
    html += NESSO_MDNS_HOSTNAME;
    html += ".local";
    if (ip[0]) {
      html += " · ";
      html += ip;
    }
    html += "</p>";
  } else {
    html += "<span class=pill><span class=\"dot warn\"></span><span class=warn>Niet verbonden</span></span>";
  }
  html += "</section>";

  html += "<section><div class=sec-h><span class=step>2</span><h2>Warmtepomp</h2></div>";
  if (hp_ok) {
    html += "<p class=ok style=\"margin:0 0 .6rem\">Ingesteld · ";
    html += g_hp->ipv4;
    html += " · poort ";
    html += String(g_hp->tcp_port);
    html += " · unit ";
    html += String(g_hp->unit_id);
    html += "</p>";
  } else {
    html += "<p class=warn style=\"margin:0 0 .6rem\">Nog niet ingesteld</p>";
  }
  html += "<form method=POST action=/api/heatpump>";
  html += "<label>IP-adres</label>";
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
  html += "<div class=row><button type=submit>Opslaan</button>";
  html += "<button class=secondary type=button id=mbbtn onclick=\"testMb()\">Test Modbus</button></div>";
  html += "</form>";
  html += "<p class=muted id=mbstatus style=\"margin:.55rem 0 0\"></p>";
  html += "<script>async function testMb(){const s=document.getElementById('mbstatus');"
          "const b=document.getElementById('mbbtn');b.disabled=true;s.textContent='Bezig…';s.className='muted';"
          "try{let r=await fetch('/api/modbus/probe',{method:'POST'});let j=await r.json();"
          "while(j.state==='busy'){await new Promise(x=>setTimeout(x,400));"
          "r=await fetch('/api/modbus/status');j=await r.json()}"
          "s.className=j.ok?'ok':'warn';s.textContent=j.msg||JSON.stringify(j)}"
          "catch(e){s.className='warn';s.textContent='Portal/timeout — opnieuw proberen'}"
          "b.disabled=false}</script>";
  html += "</section>";

  html += "<section><div class=sec-h><span class=step>3</span><h2>Zigbee-knoppen</h2></div>";
  html += "<p class=muted id=zbstatus style=\"margin:0 0 .5rem\">Laden…</p>";
  html += "<ol><li>Join openen</li>";
  html += "<li>Shelly dichtbij: 5× drukken (paars = pair)</li>";
  html += "<li>Daarna 1× drukken — Nesso bindt alle 4 knoppen</li></ol>";
  html += "<p class=muted>Laatste: <strong id=zblast style=\"color:var(--text)\">—</strong></p>";
  html += "<div class=row><button type=button onclick=\"zbJoin()\">Join openen</button>";
  html += "<button class=secondary type=button onclick=\"zbRefresh()\">Vernieuwen</button></div>";
  html += "<div id=zblist></div>";
  html += "<script>async function zbRefresh(){try{const r=await fetch('/api/zigbee');"
          "const j=await r.json();const s=document.getElementById('zbstatus');"
          "s.className=j.join_open?'warn':'muted';"
          "s.textContent=(j.running?'Zigbee aan':'Zigbee uit')+' · '+j.shelly_count+'/5 knoppen'"
          "+(j.join_open?(' · join '+j.join_remaining_s+'s'):'');"
          "document.getElementById('zblast').textContent=j.last_event||'—';"
          "let h='';(j.shellys||[]).forEach(x=>{h+='<div class=device><div><b>'+x.label+"
          "'</b><div class=muted>'+x.eui+'</div></div>"
          "+'<button class=\"secondary tiny\" type=button onclick=\"zbRemove('+x.index+')\">"
          "Verwijder</button></div>'});"
          "document.getElementById('zblist').innerHTML=h||'<p class=muted>Nog geen knoppen</p>'}"
          "catch(e){document.getElementById('zbstatus').textContent='Zigbee-status mislukt'}}"
          "async function zbJoin(){await fetch('/api/zigbee/join',{method:'POST'});zbRefresh()}"
          "async function zbRemove(i){await fetch('/api/zigbee/remove?index='+i,{method:'POST'});"
          "zbRefresh()}zbRefresh();setInterval(zbRefresh,3000)</script></section>";

  html += "<section><div class=sec-h><span class=step>·</span><h2>Software</h2></div>";
  html += "<p class=muted style=\"margin:0 0 .55rem\">Firmware <strong style=\"color:var(--text)\">";
  html += NESSO_FW_VERSION;
  html += "</strong></p>";
  html += "<div class=row><button class=secondary type=button onclick=\"otaCheck()\">Check update</button>";
  html += "<button class=secondary type=button onclick=\"otaApply()\">Installeren</button></div>";
  html += "<p class=muted id=otastatus style=\"margin:.55rem 0 0\"></p>";
  html += "<script>async function otaCheck(){const s=document.getElementById('otastatus');"
          "s.textContent='Controleren…';try{const r=await fetch('/api/ota/check',{method:'POST'});"
          "const j=await r.json();s.textContent=j.availability+' remote='+(j.remote||'')+"
          "' err='+(j.last_error||'')}catch(e){s.textContent='OTA-check mislukt'}}"
          "async function otaApply(){const s=document.getElementById('otastatus');"
          "s.textContent='Installeren… (herstart bij succes)';"
          "try{const r=await fetch('/api/ota/apply',{method:'POST'});"
          "const j=await r.json();s.textContent=j.availability+' err='+(j.last_error||'')}"
          "catch(e){s.textContent='Apply gestart of verbinding verbroken'}}</script>";
  html += "</section>";

  html += "<section><div class=sec-h><span class=step>·</span><h2>Debug</h2></div>";
  html += "<p class=muted style=\"margin:0 0 .45rem\">Diag + log (tag zbtrace).</p>";
  html += "<div class=row><button class=secondary type=button onclick=\"zbDiag()\">Diag</button>";
  html += "<button class=secondary type=button onclick=\"zbRebind()\">Rebind (druk knop)</button>";
  html += "<button class=secondary type=button onclick=\"refreshLog()\">Log</button>";
  html += "<button class=ghost type=button id=logcopybtn onclick=\"copyLog()\">Kopiëren</button></div>";
  html += "<pre id=zbdiag>—</pre>";
  html += "<pre id=logbox>Laden…</pre>";
  html += "<script>async function zbDiag(){try{const r=await fetch('/api/zigbee/diag');"
          "document.getElementById('zbdiag').textContent=JSON.stringify(await r.json(),null,2)}"
          "catch(e){document.getElementById('zbdiag').textContent='diag mislukt'}}"
          "async function zbRebind(){await fetch('/api/zigbee/rebind',{method:'POST'});zbDiag()}"
          "async function refreshLog(){try{const r=await fetch('/api/logs');"
          "document.getElementById('logbox').textContent=await r.text()}catch(e)"
          "{document.getElementById('logbox').textContent='Log ophalen mislukt'}}"
          "async function copyLog(){const t=document.getElementById('logbox').textContent||'';"
          "const b=document.getElementById('logcopybtn');"
          "try{if(navigator.clipboard&&window.isSecureContext){await navigator.clipboard.writeText(t)}"
          "else{const a=document.createElement('textarea');a.value=t;"
          "a.style.cssText='position:fixed;left:-9999px';document.body.appendChild(a);"
          "a.select();document.execCommand('copy');a.remove()}"
          "b.textContent='Gekopieerd';setTimeout(()=>b.textContent='Kopiëren',1500)}"
          "catch(e){b.textContent='Mislukt';setTimeout(()=>b.textContent='Kopiëren',2000)}}"
          "refreshLog();setInterval(refreshLog,4000);</script></section>";

  html += "<p class=foot>Wi-Fi opnieuw? Houd knop 1 op het apparaat in.</p>";
  html += "</div></body></html>";
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
  /* Niet display_show_home() forceren — steelt Zigbee-pairingscherm weg. */
  g_server.sendHeader("Location", "/");
  g_server.send(303, "text/plain", "OK");
}

static void handle_logs() {
  /* ~120 regels zbtrace */
  static char buf[24576];
  log_ring_copy(buf, sizeof(buf));
  g_server.send(200, "text/plain; charset=utf-8", buf);
}

static void handle_zigbee_diag() {
  static char json[1536];
  zigbee_format_diag_json(json, sizeof(json));
  /* dump_now alleen als klaar — tijdens start freezet neighbor-walk/USB */
  if (zigbee_join_ready()) {
    zigbee_diag_dump_now();
  }
  g_server.send(200, "application/json", json);
}

static void handle_zigbee_rebind() {
  zigbee_diag_rebind_all();
  handle_zigbee_diag();
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

static void handle_zigbee_get() {
  static char json[768];
  zigbee_format_status_json(json, sizeof(json));
  g_server.send(200, "application/json", json);
}

static void handle_zigbee_join() {
  const bool ok = zigbee_open_join((uint8_t)NESSO_ZIGBEE_JOIN_WINDOW_S);
  if (!ok) {
    g_server.send(503, "application/json", "{\"ok\":false,\"msg\":\"Zigbee niet actief\"}");
    return;
  }
  handle_zigbee_get();
}

static void handle_zigbee_remove() {
  const int idx = g_server.hasArg("index") ? g_server.arg("index").toInt() : -1;
  if (idx < 0 || idx >= NESSO_MAX_SHELLY_SLOTS || !zigbee_remove_shelly_index((uint8_t)idx)) {
    g_server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  handle_zigbee_get();
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
  g_server.on("/api/zigbee", HTTP_GET, handle_zigbee_get);
  g_server.on("/api/zigbee/join", HTTP_POST, handle_zigbee_join);
  g_server.on("/api/zigbee/remove", HTTP_POST, handle_zigbee_remove);
  g_server.on("/api/zigbee/diag", HTTP_GET, handle_zigbee_diag);
  g_server.on("/api/zigbee/rebind", HTTP_POST, handle_zigbee_rebind);
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
