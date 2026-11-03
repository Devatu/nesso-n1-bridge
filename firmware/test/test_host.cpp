#include "../nesso_n1_bridge/action_dispatcher.h"
#include "../nesso_n1_bridge/heatpump_config.h"
#include "../nesso_n1_bridge/ota_manager.h"
#include "../nesso_n1_bridge/ota_manifest.h"
#include "../nesso_n1_bridge/portal_api.h"
#include "../nesso_n1_bridge/shelly_decoder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_failed = 0;

#define EXPECT(cond, msg)                                                                          \
  do {                                                                                             \
    if (!(cond)) {                                                                                 \
      printf("FAIL %s:%d %s\n", __FILE__, __LINE__, msg);                                          \
      ++g_failed;                                                                                  \
    }                                                                                              \
  } while (0)

static Eui64 make_eui(uint8_t last) {
  Eui64 eui;
  eui64_clear(&eui);
  eui.bytes[7] = last;
  eui.bytes[0] = 0x28;
  return eui;
}

static ZclIncoming make_cmd(const Eui64 *eui, uint8_t ep, uint16_t cluster, uint8_t command) {
  ZclIncoming cmd;
  eui64_copy(&cmd.src_ieee, eui);
  cmd.src_short = 0x1234;
  cmd.ieee_resolved = true;
  cmd.src_endpoint = ep;
  cmd.cluster = cluster;
  cmd.command = command;
  cmd.scene_id = 0;
  cmd.step_mode = 0;
  return cmd;
}

static void test_decoder_onoff_mode() {
  const Eui64 eui = make_eui(1);
  ButtonEvent ev;
  ZclIncoming c1 = make_cmd(&eui, 1, ZCL_CLUSTER_ONOFF, ZCL_CMD_ON);
  EXPECT(shelly_decode(&c1, &ev) && ev.button == 1 && ev.kind == BUTTON_KIND_SINGLE, "btn1 on");
  ZclIncoming c2 = make_cmd(&eui, 1, ZCL_CLUSTER_ONOFF, ZCL_CMD_OFF);
  EXPECT(shelly_decode(&c2, &ev) && ev.button == 2, "btn2 off");
  ZclIncoming c3 = make_cmd(&eui, 2, ZCL_CLUSTER_ONOFF, ZCL_CMD_ON);
  EXPECT(shelly_decode(&c3, &ev) && ev.button == 3, "btn3 on");
  ZclIncoming c4 = make_cmd(&eui, 2, ZCL_CLUSTER_ONOFF, ZCL_CMD_OFF);
  EXPECT(shelly_decode(&c4, &ev) && ev.button == 4, "btn4 off");
}

static void test_decoder_toggle_and_scenes() {
  const Eui64 eui = make_eui(2);
  ButtonEvent ev;
  ZclIncoming t3 = make_cmd(&eui, 3, ZCL_CLUSTER_ONOFF, ZCL_CMD_TOGGLE);
  EXPECT(shelly_decode(&t3, &ev) && ev.button == 3 && ev.kind == BUTTON_KIND_SINGLE, "toggle ep3");
  ZclIncoming sc = make_cmd(&eui, 4, ZCL_CLUSTER_SCENES, ZCL_CMD_RECALL_SCENE);
  sc.scene_id = 1;
  EXPECT(shelly_decode(&sc, &ev) && ev.button == 4 && ev.kind == BUTTON_KIND_DOUBLE, "double");
  sc.scene_id = 11;
  EXPECT(shelly_decode(&sc, &ev) && ev.kind == BUTTON_KIND_LONG, "long");
  ZclIncoming hold = make_cmd(&eui, 1, ZCL_CLUSTER_LEVEL, ZCL_CMD_STEP);
  hold.step_mode = ZCL_STEP_DOWN;
  EXPECT(shelly_decode(&hold, &ev) && ev.button == 2 && ev.kind == BUTTON_KIND_HOLD, "hold btn2");
  ZclIncoming bad = make_cmd(&eui, 5, ZCL_CLUSTER_ONOFF, ZCL_CMD_ON);
  EXPECT(!shelly_decode(&bad, &ev), "reject ep5");
}

static void test_registry_limits() {
  DeviceRegistry reg;
  registry_init(&reg);
  for (uint8_t i = 1; i <= 5; ++i) {
    Eui64 eui = make_eui(i);
    EXPECT(registry_enroll_shelly(&reg, &eui, "s"), "enroll");
  }
  Eui64 sixth = make_eui(6);
  EXPECT(!registry_enroll_shelly(&reg, &sixth, "s6"), "reject sixth");
  EXPECT(registry_shelly_count(&reg) == 5, "count 5");
  Eui64 dup = make_eui(1);
  EXPECT(!registry_enroll_shelly(&reg, &dup, "dup"), "reject duplicate");
  Eui64 router = make_eui(9);
  EXPECT(registry_enroll_router(&reg, &router), "router enroll");
  EXPECT(!registry_enroll_shelly(&reg, &router, "no"), "router cannot be shelly");
  EXPECT(registry_is_router(&reg, &router), "router lookup");
  Eui64 zero;
  eui64_clear(&zero);
  EXPECT(!registry_enroll_shelly(&reg, &zero, "z"), "reject zero EUI");
}

static void test_dispatch_auth_and_dedup() {
  DeviceRegistry reg;
  EventDedup dedup;
  registry_init(&reg);
  dispatcher_init(&dedup);
  Eui64 shelly = make_eui(0xA1);
  Eui64 router = make_eui(0xB2);
  Eui64 stranger = make_eui(0xC3);
  registry_enroll_shelly(&reg, &shelly, "s1");
  registry_enroll_router(&reg, &router);

  ButtonEvent ev;
  ZclIncoming cmd = make_cmd(&shelly, 1, ZCL_CLUSTER_ONOFF, ZCL_CMD_ON);
  EXPECT(shelly_decode(&cmd, &ev), "decode");
  ev.timestamp_ms = 1000;
  DispatchOutcome o = dispatcher_handle(&reg, &dedup, &ev);
  EXPECT(o.auth == AUTH_OK && o.result == DISPATCH_ACTION_NOT_READY && o.action == ACTION_1,
         "authorized but registers blocked");

  ev.timestamp_ms = 1100;
  o = dispatcher_handle(&reg, &dedup, &ev);
  EXPECT(o.result == DISPATCH_DEDUPED, "dedup within 400ms");

  ev.timestamp_ms = 1500;
  o = dispatcher_handle(&reg, &dedup, &ev);
  EXPECT(o.result == DISPATCH_ACTION_NOT_READY, "accepted after window");

  cmd = make_cmd(&router, 1, ZCL_CLUSTER_ONOFF, ZCL_CMD_ON);
  EXPECT(shelly_decode(&cmd, &ev), "router decode");
  ev.timestamp_ms = 2000;
  o = dispatcher_handle(&reg, &dedup, &ev);
  EXPECT(o.auth == AUTH_ROUTER_FORBIDDEN && o.result == DISPATCH_AUTH_FAIL, "router forbidden");

  cmd = make_cmd(&stranger, 1, ZCL_CLUSTER_ONOFF, ZCL_CMD_ON);
  EXPECT(shelly_decode(&cmd, &ev), "stranger decode");
  ev.timestamp_ms = 2100;
  o = dispatcher_handle(&reg, &dedup, &ev);
  EXPECT(o.auth == AUTH_UNKNOWN_DEVICE, "unknown EUI");

  ev.source_ieee_resolved = false;
  ev.timestamp_ms = 2200;
  o = dispatcher_handle(&reg, &dedup, &ev);
  EXPECT(o.auth == AUTH_NO_IEEE, "short address only rejected");

  cmd = make_cmd(&shelly, 1, ZCL_CLUSTER_SCENES, ZCL_CMD_RECALL_SCENE);
  cmd.scene_id = 1;
  EXPECT(shelly_decode(&cmd, &ev), "double decode");
  ev.timestamp_ms = 3000;
  o = dispatcher_handle(&reg, &dedup, &ev);
  EXPECT(o.auth == AUTH_EVENT_IGNORED, "double ignored by policy");
}

static void test_action_table_placeholders() {
  const ActionSpec *a1 = action_for_button(1);
  EXPECT(a1 != nullptr && a1->action == ACTION_1, "map btn1");
  EXPECT(!action_is_ready(a1), "placeholder not ready");
  EXPECT(action_for_button(0) == nullptr, "invalid button");
}

static void test_heatpump_config() {
  HeatpumpConfig cfg;
  heatpump_config_clear(&cfg);
  EXPECT(!heatpump_config_is_ready(&cfg), "empty not ready");
  EXPECT(heatpump_config_set(&cfg, "192.168.1.50", 502, 1, "wp") == HP_OK, "set ok");
  EXPECT(heatpump_config_is_ready(&cfg), "ready");
  EXPECT(strcmp(cfg.ipv4, "192.168.1.50") == 0, "ipv4 stored");
  EXPECT(heatpump_config_set(&cfg, "0.0.0.0", 502, 1, nullptr) == HP_ERR_IPV4, "reject 0.0.0.0");
  EXPECT(heatpump_config_set(&cfg, "10.0.0.1", 0, 1, nullptr) == HP_ERR_PORT, "reject port 0");
  EXPECT(heatpump_config_set(&cfg, "10.0.0.1", 502, 248, nullptr) == HP_ERR_UNIT_ID, "reject unit 248");
  EXPECT(heatpump_config_set(&cfg, "not-an-ip", 502, 1, nullptr) == HP_ERR_IPV4, "reject junk");
}

static void test_portal_api() {
  EXPECT(portal_classify_path("GET", "/api/heatpump") == PORTAL_REQ_GET_HEATPUMP, "get hp");
  EXPECT(portal_classify_path("PUT", "/api/heatpump") == PORTAL_REQ_PUT_HEATPUMP, "put hp");
  EXPECT(portal_classify_path("POST", "/api/registers") == PORTAL_REQ_FORBIDDEN_REGISTERS,
         "forbid registers");
  EXPECT(portal_classify_path("POST", "/api/modbus/write") == PORTAL_REQ_FORBIDDEN_REGISTERS,
         "forbid write");

  PortalHeatpumpPut put;
  EXPECT(portal_parse_heatpump_form("ipv4=10.0.0.8&port=502&unit_id=2&label=hp1", &put), "parse");
  EXPECT(put.tcp_port == 502 && put.unit_id == 2, "fields");
  HeatpumpConfig cfg;
  heatpump_config_clear(&cfg);
  EXPECT(portal_apply_heatpump_put(&cfg, &put) == (int)HP_OK, "apply");
  EXPECT(heatpump_config_is_ready(&cfg), "portal ready");

  char json[160];
  size_t n = portal_format_heatpump_json(&cfg, json, sizeof(json));
  EXPECT(n > 0 && strstr(json, "10.0.0.8") != nullptr, "json");
}

static void test_ota_manifest_policy() {
  const char *good =
      "{"
      "\"schema\":1,"
      "\"product\":\"nesso-n1-heatpump-bridge\","
      "\"board\":\"arduino_nesso_n1\","
      "\"channel\":\"stable\","
      "\"version\":\"0.2.0\","
      "\"security_version\":1,"
      "\"asset_url\":\"https://example.com/fw.bin\","
      "\"sha256\":\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\","
      "\"size_bytes\":123456,"
      "\"signature_algorithm\":\"ecdsa-p256\","
      "\"signature\":\"AAAA\""
      "}";
  OtaManifest m;
  EXPECT(ota_manifest_parse(good, &m) == OTA_MANIFEST_OK, "parse good");
  EXPECT(ota_manifest_validate_policy(&m, 1) == OTA_MANIFEST_OK, "policy ok");
  EXPECT(ota_manifest_validate_policy(&m, 2) == OTA_MANIFEST_SECURITY_DOWNGRADE, "no downgrade");

  const char *http =
      "{"
      "\"schema\":1,"
      "\"product\":\"nesso-n1-heatpump-bridge\","
      "\"board\":\"arduino_nesso_n1\","
      "\"channel\":\"stable\","
      "\"version\":\"0.2.0\","
      "\"security_version\":1,"
      "\"asset_url\":\"http://insecure.example/fw.bin\","
      "\"sha256\":\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\","
      "\"size_bytes\":123456,"
      "\"signature_algorithm\":\"ecdsa-p256\","
      "\"signature\":\"AAAA\""
      "}";
  EXPECT(ota_manifest_parse(http, &m) == OTA_MANIFEST_OK, "parse http");
  EXPECT(ota_manifest_validate_policy(&m, 1) == OTA_MANIFEST_URL, "https only");

  OtaStatus st;
  ota_init(&st, 0);
  EXPECT(ota_after_manifest_policy(&st, OTA_MANIFEST_OK) == OTA_DOWNLOAD, "to download");
  EXPECT(ota_after_manifest_policy(&st, OTA_MANIFEST_BOARD) == OTA_ERROR, "bad board");
}

int main() {
  test_decoder_onoff_mode();
  test_decoder_toggle_and_scenes();
  test_registry_limits();
  test_dispatch_auth_and_dedup();
  test_action_table_placeholders();
  test_heatpump_config();
  test_portal_api();
  test_ota_manifest_policy();
  if (g_failed != 0) {
    printf("%d test(s) failed\n", g_failed);
    return 1;
  }
  printf("all host tests passed\n");
  return 0;
}
