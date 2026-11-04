#include "zigbee_coordinator.h"
#include "action_dispatcher.h"
#include "action_table.h"
#include "app_config.h"
#include "log.h"
#include "modbus_client.h"
#include "shelly_decoder.h"
#include "wifi_manager.h"
#include <stdio.h>
#include <string.h>

#if NESSO_ENABLE_HW
#ifndef ZIGBEE_MODE_ZCZR
#error "Zigbee ZCZR (coordinator) mode required"
#endif
#include "Zigbee.h"
#include <esp_coexist.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include "hal/brownout_ll.h"
#include "nwk/esp_zigbee_nwk.h"
/* BOD HW-reset is disabled; keep TX usable for battery remotes during join. */
#ifndef NESSO_ZB_TX_DBM
#define NESSO_ZB_TX_DBM 8 /* was 15: USB-brownout/hang bij Shelly-TX + bind */
#endif
#endif

static DeviceRegistry *g_reg = nullptr;
static EventDedup *g_dedup = nullptr;
static AppConfigStore *g_store = nullptr;
static bool g_running = false;
static bool g_paused = false;
static bool g_start_pending = false;
static bool g_start_failed = false;
static bool g_start_task_launched = false;
static bool g_join_ready = false; /* true pas ná privilege-install */
static uint32_t g_settle_until_ms = 0; /* na klaar: geen enroll/diag/bind-start */
static uint32_t g_join_until_ms = 0;
static char g_last_event[48] = "";
#if NESSO_ENABLE_HW
static volatile uint8_t g_pending_join_s = 0;
static volatile uint8_t g_deferred_join_s = 0; /* join gevraagd vóór ready */
static volatile bool g_pending_close_join = false;
static volatile bool g_join_op_busy = false;
static void zigbee_join_open_task(void *arg);
static void zigbee_join_close_task(void *arg);
#endif

/* Button-path counters — visible via /api/zigbee/diag + periodieke dump. */
static uint32_t g_cnt_attr = 0;
static uint32_t g_cnt_priv = 0;
static uint32_t g_cnt_priv_drop = 0;
static uint32_t g_cnt_decode_ok = 0;
static uint32_t g_cnt_decode_fail = 0;
static uint32_t g_cnt_dispatch_ok = 0;
static uint32_t g_cnt_auth_fail = 0;
static uint32_t g_cnt_bind_ok = 0;
static uint32_t g_cnt_bind_fail = 0;
static uint32_t g_last_attr_ms = 0;
static uint32_t g_last_priv_ms = 0;
static uint32_t g_last_btn_ms = 0;
static char g_last_raw[64] = "";

static void fmt_eui(char *out, size_t n, const uint8_t *b) {
  if (out == nullptr || n < 24 || b == nullptr) {
    return;
  }
  snprintf(out, n, "%02X%02X%02X%02X%02X%02X%02X%02X", b[7], b[6], b[5], b[4], b[3], b[2], b[1],
           b[0]);
}

#if NESSO_ENABLE_HW
/* C5-style: 4 gateway EPs 10-13 ↔ Shelly button EPs 1-4. */
#define NESSO_ZB_GW_EP0 10
#define NESSO_ZB_GW_COUNT 4
#define NESSO_ZB_GW_EP(shelly_ep) ((uint8_t)(NESSO_ZB_GW_EP0 + (shelly_ep) - 1))
#define NESSO_ZB_LIGHT_EP NESSO_ZB_GW_EP0 /* primary / diag */

#define BIND_ON_OFF_DELAY_MS 600u
#define BIND_CB_TIMEOUT_MS 4000u /* te kort → false timeout + late CB = chaos/freeze */
#define BIND_MAX_RETRIES 4u
#define BIND_START_DELAY_MS 800u
#define BIND_WAKE_WAIT_MS 1500u
#define BIND_WAKE_GIVEUP_MS 30000u
#define BIND_ON_OFF_STEPS 2u /* factory On/Off mode: alleen Shelly EP1+EP2 */
#define BIND_ZB_LOCK_MS 300u

static bool try_enroll_during_join(const Eui64 *eui);
static void set_last_event(const char *msg);
static bool eui_from_zcl_addr(const esp_zb_zcl_addr_t &addr, uint16_t *short_out, Eui64 *eui,
                              bool *resolved);
static void ingest_zcl_button(const ZclIncoming *in);
static void maybe_wake_bind_all(uint16_t short_addr, const Eui64 *eui);
static void handle_button_pipeline(ButtonEvent *ev);
static void try_attr_as_button(uint8_t dst_ep, uint8_t onoff);
static bool neighbor_is_awake(uint16_t short_addr);
static void bind_wifi_quiet(bool on);
static bool join_open_now();

struct ShellyBindStep {
  uint8_t src_ep;
  uint16_t cluster;
};

/*
 * Alleen On/Off EP1+EP2 — genoeg voor single-press (productbeleid).
 * Scenes overgeslagen: verlengde sleepy-bind → UI/join-freeze.
 */
static const ShellyBindStep k_bind_steps[] = {
    {1, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF},
    {2, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF},
};
static constexpr uint8_t k_bind_step_count =
    (uint8_t)(sizeof(k_bind_steps) / sizeof(k_bind_steps[0]));
static uint16_t g_onoff_bound_short = 0xFFFFu;
static uint8_t g_onoff_bound_count = 0;
/* Match-desc CB draait op Zigbee-task — alleen flag, bind op main. */
static volatile uint16_t g_pending_match_short = 0xFFFFu;
static esp_zb_ieee_addr_t g_pending_match_ieee = {0};
static volatile bool g_pending_close_join_after_bind = false;

/* PRIV/ATTR callbacks draaien in Zigbee-task — zwaar werk daar = freeze. Queue → poll. */
#define ZCL_Q_LEN 8
static ZclIncoming g_zcl_q[ZCL_Q_LEN];
static volatile uint8_t g_zcl_q_w = 0;
static volatile uint8_t g_zcl_q_r = 0;
static volatile uint8_t g_pending_attr_ep = 0;
static volatile uint8_t g_pending_attr_onoff = 0xFF;
static portMUX_TYPE g_zcl_q_mux = portMUX_INITIALIZER_UNLOCKED;

static bool zcl_q_push(const ZclIncoming *in) {
  if (in == nullptr) {
    return false;
  }
  portENTER_CRITICAL(&g_zcl_q_mux);
  const uint8_t next = (uint8_t)((g_zcl_q_w + 1u) % ZCL_Q_LEN);
  if (next == g_zcl_q_r) {
    portEXIT_CRITICAL(&g_zcl_q_mux);
    return false;
  }
  g_zcl_q[g_zcl_q_w] = *in;
  g_zcl_q_w = next;
  portEXIT_CRITICAL(&g_zcl_q_mux);
  return true;
}

static bool zcl_q_pop(ZclIncoming *out) {
  if (out == nullptr) {
    return false;
  }
  portENTER_CRITICAL(&g_zcl_q_mux);
  if (g_zcl_q_r == g_zcl_q_w) {
    portEXIT_CRITICAL(&g_zcl_q_mux);
    return false;
  }
  *out = g_zcl_q[g_zcl_q_r];
  g_zcl_q_r = (uint8_t)((g_zcl_q_r + 1u) % ZCL_Q_LEN);
  portEXIT_CRITICAL(&g_zcl_q_mux);
  return true;
}

class NessoShellyTargetLight : public ZigbeeDimmableLight {
 public:
  explicit NessoShellyTargetLight(uint8_t endpoint) : ZigbeeDimmableLight(endpoint) {}

  void zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) override {
    if (message == nullptr) {
      return;
    }
    ++g_cnt_attr;
    g_last_attr_ms = millis();
    uint8_t onoff = 0xFF;
    if (message->attribute.data.value != nullptr &&
        message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
      onoff = *((uint8_t *)message->attribute.data.value);
    }
    snprintf(g_last_raw, sizeof(g_last_raw), "ATTR dst=%u v=%u", message->info.dst_endpoint, onoff);
    /* Geen LOG/pipeline hier — alleen markeren voor poll. */
    if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
      g_pending_attr_ep = message->info.dst_endpoint;
      g_pending_attr_onoff = onoff;
    }
  }

  void findEndpoint(esp_zb_zdo_match_desc_req_param_t *cmd_req) override {
    /* Alleen primary EP (10) zoekt — voorkomt 4× match flood. */
    if (cmd_req == nullptr || getEndpoint() != NESSO_ZB_GW_EP0) {
      return;
    }
    uint16_t cluster_list[] = {ESP_ZB_ZCL_CLUSTER_ID_ON_OFF};
    esp_zb_zdo_match_desc_req_param_t req = {
        .dst_nwk_addr = cmd_req->dst_nwk_addr,
        .addr_of_interest = cmd_req->addr_of_interest,
        .profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .num_in_clusters = 0,
        .num_out_clusters = 1,
        .cluster_list = cluster_list,
    };
    LOG_I("zbtrace", "match On/Off-client @ short=0x%04x", cmd_req->dst_nwk_addr);
    esp_zb_zdo_match_cluster(&req, &NessoShellyTargetLight::findCbWrapper, this);
  }

  void bindRemoteClusters(uint16_t short_addr, const esp_zb_ieee_addr_t ieee, bool force = false) {
    Eui64 eui;
    memcpy(eui.bytes, ieee, 8);
    if (try_enroll_during_join(&eui)) {
      set_last_event("join: knop gezien");
    }
    queueBindJob(short_addr, ieee, force);
    bool already = false;
    std::list<zb_device_params_t *> devices = getBoundDevices();
    for (zb_device_params_t *d : devices) {
      if (d != nullptr && memcmp(d->ieee_addr, ieee, 8) == 0) {
        already = true;
        break;
      }
    }
    if (!already) {
      zb_device_params_t *dev = (zb_device_params_t *)malloc(sizeof(zb_device_params_t));
      if (dev != nullptr) {
        memset(dev, 0, sizeof(*dev));
        dev->endpoint = 1;
        dev->short_addr = short_addr;
        memcpy(dev->ieee_addr, ieee, 8);
        addBoundDevice(dev);
      }
    }
  }

  void pollBindQueue() {
    /* Match-CB (Zigbee-task) → bind starten op main (geen Wi-Fi vanuit ZB-task). */
    if (g_pending_match_short != 0xFFFFu) {
      const uint16_t addr = g_pending_match_short;
      esp_zb_ieee_addr_t ieee;
      memcpy(ieee, g_pending_match_ieee, 8);
      g_pending_match_short = 0xFFFFu;
      LOG_I("zbtrace", "match→bind short=0x%04x (main)", addr);
      bindRemoteClusters(addr, ieee, false);
    }

    if (!bind_job_active_) {
      return;
    }
    const uint32_t now = millis();

    /* CB-resultaat alleen op main verwerken (finishBind mag Wi-Fi aanraken). */
    if (pending_cb_ != 0) {
      const int8_t r = pending_cb_;
      const uint8_t gen = pending_cb_gen_;
      pending_cb_ = 0;
      if (bind_awaiting_cb_ && gen == bind_tx_gen_) {
        bind_awaiting_cb_ = false;
        if (r > 0) {
          ++g_cnt_bind_ok;
          LOG_I("zbtrace", "C5-bind CB OK step=%u", bind_step_);
          onBindResult(true, false);
        } else {
          ++g_cnt_bind_fail;
          LOG_W("zbtrace", "C5-bind CB FAIL step=%u short=0x%04x", bind_step_, bind_short_);
          onBindResult(false, false);
        }
        return;
      }
      LOG_I("zbtrace", "C5-bind late/stale CB ignored (gen %u/%u await=%u)", gen, bind_tx_gen_,
            bind_awaiting_cb_ ? 1 : 0);
    }

    if (bind_awaiting_cb_) {
      if ((int32_t)(now - bind_sent_ms_) < (int32_t)BIND_CB_TIMEOUT_MS) {
        return;
      }
      ++g_cnt_bind_fail;
      LOG_W("zbtrace", "bind timeout step=%u short=0x%04x (sleepy?) — wacht op wake", bind_step_,
            bind_short_);
      bind_awaiting_cb_ = false;
      ++bind_tx_gen_; /* late CB voor deze TX negeren */
      onBindResult(false, true /* was_timeout */);
      return;
    }
    if ((int32_t)(now - bind_next_ms_) < 0) {
      return;
    }
    sendCurrentStep();
  }

  bool bindJobActive() const { return bind_job_active_; }
  uint8_t bindStep() const { return bind_step_; }
  uint16_t bindShort() const { return bind_short_; }
  bool onOffBoundOk() const {
    return bind_onoff_ok_ >= BIND_ON_OFF_STEPS ||
           (g_onoff_bound_short == bind_short_ && g_onoff_bound_count >= BIND_ON_OFF_STEPS);
  }

  /*
   * Knopverkeer = device wakker — ZDO-bind NIET afbreken.
   * Vroeger stopten we hier → vaak alleen EP1 gebonden → knop 3/4 dood,
   * en g_onoff_bound_short blokkeerde daarna elke rebind.
   */
  void markBoundFromTraffic(uint16_t short_addr) {
    if (short_addr == 0 || short_addr >= 0xFFF8u) {
      return;
    }
    if (!bind_job_active_ || bind_short_ != short_addr) {
      return;
    }
    /* Versnel volgende ZDO-stap: device is net wakker geworden. */
    if (!bind_awaiting_cb_ && (int32_t)(millis() - bind_next_ms_) < 0) {
      bind_next_ms_ = millis() + 50;
    }
    LOG_I("zbtrace", "knopverkeer short=0x%04x — bind door (step=%u onoff_ok=%u)", short_addr,
          bind_step_, bind_onoff_ok_);
  }

 private:
  bool bind_job_active_ = false;
  bool bind_awaiting_cb_ = false;
  uint8_t bind_step_ = 0;
  uint8_t bind_retries_ = 0;
  uint8_t bind_onoff_ok_ = 0;
  uint8_t bind_tx_gen_ = 1;
  volatile int8_t pending_cb_ = 0; /* 0=none, >0 OK, <0 fail — alleen ZB-task schrijft */
  volatile uint8_t pending_cb_gen_ = 0;
  uint16_t bind_short_ = 0;
  esp_zb_ieee_addr_t bind_ieee_ = {0};
  uint32_t bind_sent_ms_ = 0;
  uint32_t bind_next_ms_ = 0;
  uint32_t bind_wait_start_ms_ = 0;
  uint32_t bind_wait_log_ms_ = 0;

  static uint32_t delayForStep(uint8_t /*step_idx*/) { return BIND_ON_OFF_DELAY_MS; }

  void queueBindJob(uint16_t short_addr, const esp_zb_ieee_addr_t ieee, bool force = false) {
    if (bind_job_active_ && bind_short_ == short_addr && memcmp(bind_ieee_, ieee, 8) == 0 &&
        !force) {
      return;
    }
    if (!force && !bind_job_active_ && g_onoff_bound_short == short_addr &&
        g_onoff_bound_count >= BIND_ON_OFF_STEPS) {
      return; /* EP1+EP2 On/Off al gelukt */
    }
    if (force && bind_job_active_) {
      LOG_I("zbtrace", "force rebind — stop huidige job short=0x%04x", bind_short_);
      bind_job_active_ = false;
      bind_awaiting_cb_ = false;
      pending_cb_ = 0;
    }
    if (force) {
      g_onoff_bound_short = 0xFFFFu;
      g_onoff_bound_count = 0;
    }
    bind_job_active_ = true;
    bind_awaiting_cb_ = false;
    pending_cb_ = 0;
    bind_step_ = 0;
    bind_retries_ = 0;
    bind_onoff_ok_ = 0;
    bind_short_ = short_addr;
    memcpy(bind_ieee_, ieee, 8);
    bind_next_ms_ = millis() + (force ? 200u : BIND_START_DELAY_MS);
    bind_wait_start_ms_ = millis();
    bind_wait_log_ms_ = 0;
    bind_wifi_quiet(true);
    char eui_s[24];
    fmt_eui(eui_s, sizeof(eui_s), ieee);
    LOG_I("zbtrace", "C5-bind START%s short=0x%04x eui=%s — houd Shelly wakker (blijven drukken)",
          force ? " (FORCE)" : "", short_addr, eui_s);
    set_last_event(force ? "rebind: druk Shelly wakker" : "bind: druk Shelly wakker");
  }

  void finishBind() {
    bind_job_active_ = false;
    bind_awaiting_cb_ = false;
    pending_cb_ = 0;
    /* Altijd Wi-Fi herstellen — join-hold-off is apart. Voorkomt stilstaande portal/UI. */
    bind_wifi_quiet(false);
    const bool ok = bind_onoff_ok_ >= BIND_ON_OFF_STEPS;
    if (ok) {
      g_onoff_bound_short = bind_short_;
      g_onoff_bound_count = bind_onoff_ok_;
      g_pending_close_join_after_bind = true; /* join mag dicht; knoppen werken */
    } else if (bind_onoff_ok_ > 0) {
      LOG_W("zbtrace", "On/Off bind onvolledig %u/%u — rebind nodig", bind_onoff_ok_,
            BIND_ON_OFF_STEPS);
      g_onoff_bound_short = 0xFFFFu;
      g_onoff_bound_count = 0;
    }
    LOG_I("zbtrace", "C5-bind DONE short=0x%04x onoff_ok=%u/%u bind_ok=%lu fail=%lu", bind_short_,
          bind_onoff_ok_, BIND_ON_OFF_STEPS, (unsigned long)g_cnt_bind_ok,
          (unsigned long)g_cnt_bind_fail);
    set_last_event(ok ? "bind OK — test alle 4 knoppen" : "bind onvolledig — Rebind + drukken");
  }

  void onBindResult(bool ok, bool was_timeout) {
    if (ok) {
      bind_retries_ = 0;
      if (bind_step_ < BIND_ON_OFF_STEPS) {
        ++bind_onoff_ok_;
      }
      ++bind_step_;
    } else if (bind_retries_ + 1 < BIND_MAX_RETRIES) {
      ++bind_retries_;
      bind_next_ms_ = millis() + (was_timeout ? BIND_WAKE_WAIT_MS : delayForStep(bind_step_));
      set_last_event("bind timeout — druk knop");
      return;
    } else {
      bind_retries_ = 0;
      ++bind_step_;
    }

    if (bind_step_ >= k_bind_step_count) {
      finishBind();
      return;
    }
    bind_next_ms_ = millis() + delayForStep(bind_step_);
  }

  void sendCurrentStep() {
    if (!bind_job_active_ || bind_awaiting_cb_) {
      return;
    }
    if (bind_step_ >= k_bind_step_count) {
      finishBind();
      return;
    }
    /* Alleen TX als sleepy remote in neighbor-tabel staat (RX aan). */
    if (!neighbor_is_awake(bind_short_)) {
      const uint32_t now = millis();
      if (bind_wait_start_ms_ == 0) {
        bind_wait_start_ms_ = now;
      }
      if ((now - bind_wait_start_ms_) >= BIND_WAKE_GIVEUP_MS) {
        if (bind_onoff_ok_ >= BIND_ON_OFF_STEPS ||
            (g_onoff_bound_short == bind_short_ && g_onoff_bound_count >= BIND_ON_OFF_STEPS)) {
          LOG_I("zbtrace", "bind give-up wake maar On/Off al OK — DONE short=0x%04x",
                bind_short_);
          if (bind_onoff_ok_ < BIND_ON_OFF_STEPS) {
            bind_onoff_ok_ = BIND_ON_OFF_STEPS;
          }
          g_onoff_bound_short = bind_short_;
          g_onoff_bound_count = BIND_ON_OFF_STEPS;
          finishBind();
          return;
        }
        LOG_W("zbtrace", "geen neighbor voor 0x%04x na %lus — Shelly niet op RF. "
                         "Open join + 5× Shelly (paars) om opnieuw te joinen.",
              bind_short_, (unsigned long)(BIND_WAKE_GIVEUP_MS / 1000u));
        set_last_event("Shelly offline — 5x paars");
        finishBind();
        return;
      }
      if (bind_wait_log_ms_ == 0 || (now - bind_wait_log_ms_) >= 5000u) {
        bind_wait_log_ms_ = now;
        LOG_I("zbtrace", "wacht wake short=0x%04x step=%u neighbors leeg — druk of 5× paars",
              bind_short_, bind_step_);
        set_last_event("wacht: Shelly op RF?");
      }
      bind_next_ms_ = now + BIND_WAKE_WAIT_MS;
      return;
    }
    bind_wait_start_ms_ = millis(); /* opnieuw tellen na succesvolle wake */
    const ShellyBindStep &st = k_bind_steps[bind_step_];
    const uint8_t dst_ep = NESSO_ZB_GW_EP(st.src_ep);
    esp_zb_zdo_bind_req_param_t bind_req;
    memset(&bind_req, 0, sizeof(bind_req));
    memcpy(bind_req.src_address, bind_ieee_, sizeof(esp_zb_ieee_addr_t));
    bind_req.src_endp = st.src_ep;
    bind_req.cluster_id = st.cluster;
    bind_req.dst_addr_mode = ESP_ZB_ZDO_BIND_DST_ADDR_MODE_64_BIT_EXTENDED;
    esp_zb_get_long_address(bind_req.dst_address_u.addr_long);
    bind_req.dst_endp = dst_ep;
    bind_req.req_dst_addr = bind_short_;
    ++bind_tx_gen_;
    if (bind_tx_gen_ == 0) {
      bind_tx_gen_ = 1;
    }
    pending_cb_ = 0;
    bind_awaiting_cb_ = true;
    bind_sent_ms_ = millis();
    LOG_I("zbtrace", "C5-bind TX %u/%u src_ep=%u cl=0x%04x → gw_ep=%u short=0x%04x retry=%u awake=1",
          (unsigned)bind_step_ + 1, (unsigned)k_bind_step_count, st.src_ep, st.cluster, dst_ep,
          bind_short_, bind_retries_);
    if (!esp_zb_lock_acquire(pdMS_TO_TICKS(BIND_ZB_LOCK_MS))) {
      LOG_W("zbtrace", "bind: zb lock timeout — later opnieuw");
      bind_awaiting_cb_ = false;
      bind_next_ms_ = millis() + BIND_WAKE_WAIT_MS;
      return;
    }
    esp_zb_zdo_device_bind_req(&bind_req, &NessoShellyTargetLight::bindCbWrapper, this);
    esp_zb_lock_release();
  }

  static void findCbWrapper(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint,
                            void *user_ctx) {
    auto *self = static_cast<NessoShellyTargetLight *>(user_ctx);
    if (self != nullptr) {
      self->findCb(zdo_status, addr, endpoint);
    }
  }

  /* Zigbee-task: geen LOG/Wi-Fi/bind — alleen pending flag. */
  void findCb(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint) {
    (void)endpoint;
    if (zdo_status != ESP_ZB_ZDP_STATUS_SUCCESS) {
      return;
    }
    esp_zb_ieee_addr_t ieee = {0};
    if (esp_zb_ieee_address_by_short(addr, ieee) != ESP_OK) {
      return;
    }
    memcpy(g_pending_match_ieee, ieee, 8);
    g_pending_match_short = addr;
  }

  /* Zigbee-task: alleen resultaat flaggen — verwerking in pollBindQueue (main). */
  static void bindCbWrapper(esp_zb_zdp_status_t zdo_status, void *user_ctx) {
    auto *self = static_cast<NessoShellyTargetLight *>(user_ctx);
    if (self == nullptr) {
      return;
    }
    self->pending_cb_gen_ = self->bind_tx_gen_;
    self->pending_cb_ = (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) ? 1 : -1;
  }
};

static bool neighbor_is_awake(uint16_t short_addr) {
  if (short_addr == 0 || short_addr >= 0xFFF8u) {
    return false;
  }
  esp_zb_nwk_info_iterator_t it = ESP_ZB_NWK_INFO_ITERATOR_INIT;
  esp_zb_nwk_neighbor_info_t nbr;
  while (esp_zb_nwk_get_next_neighbor(&it, &nbr) == ESP_OK) {
    if (nbr.short_addr != short_addr) {
      continue;
    }
    /* rel=5 UNAUTH: join nog niet klaar — ZDO bind → 133. Wacht op CHILD. */
    if (nbr.relationship == ESP_ZB_NWK_RELATIONSHIP_UNAUTHENTICATED_CHILD) {
      static uint16_t s_unauth_short = 0xFFFFu;
      static uint32_t s_unauth_log_ms = 0;
      const uint32_t now = millis();
      if (s_unauth_short != short_addr || (now - s_unauth_log_ms) > 5000u) {
        s_unauth_short = short_addr;
        s_unauth_log_ms = now;
        LOG_I("zbtrace", "short=0x%04x UNAUTH(rel=5) — wacht CHILD (5x Shelly + open join)",
              short_addr);
      }
      return false;
    }
    if (nbr.relationship != ESP_ZB_NWK_RELATIONSHIP_CHILD &&
        nbr.relationship != ESP_ZB_NWK_RELATIONSHIP_NONE_OF_THE_ABOVE) {
      return false;
    }
    /* age≥3 ≈ bijna asleep — ZDO 133; wacht op verse RX (age 0–1). */
    return nbr.lqi > 0 && nbr.age <= 1;
  }
  return false;
}

static void bind_wifi_quiet(bool on) {
  wifi_sta_set_join_tx_low(on);
  WiFi.setSleep(on);
}

static NessoShellyTargetLight g_zb_lights[NESSO_ZB_GW_COUNT] = {
    NessoShellyTargetLight(NESSO_ZB_GW_EP(1)),
    NessoShellyTargetLight(NESSO_ZB_GW_EP(2)),
    NessoShellyTargetLight(NESSO_ZB_GW_EP(3)),
    NessoShellyTargetLight(NESSO_ZB_GW_EP(4)),
};
#define g_zb_light (g_zb_lights[0]) /* bind queue + match op primary */
static bool g_ep_ready = false;

static void try_attr_as_button(uint8_t dst_ep, uint8_t onoff) {
  if (dst_ep < NESSO_ZB_GW_EP0 || dst_ep >= NESSO_ZB_GW_EP0 + NESSO_ZB_GW_COUNT) {
    return;
  }
  if (g_reg == nullptr || g_paused || !g_running) {
    return;
  }
  /* GW EP10↔Shelly EP1, EP11↔EP2, … — On/Off mode: ON/OFF op EP1/2 → knop 1..4. */
  const uint8_t shelly_ep = (uint8_t)(dst_ep - NESSO_ZB_GW_EP0 + 1);
  uint8_t button = shelly_ep;
  if (shelly_ep <= 2 && onoff <= 1) {
    button = (uint8_t)((shelly_ep - 1) * 2 + (onoff ? 1 : 2));
  }
  /* ATTR heeft geen IEEE — bij 1 enrolled Shelly toch knop afvuren (C5-gedrag). */
  int occupied = -1;
  int count = 0;
  for (int i = 0; i < NESSO_MAX_SHELLY_SLOTS; ++i) {
    if (g_reg->shelly[i].occupied) {
      occupied = i;
      ++count;
    }
  }
  if (count != 1 || occupied < 0) {
    LOG_I("zbtrace", "ATTR btn≈%u — wacht PRIV (shelly_count=%d)", button, count);
    set_last_event("ATTR — druk opnieuw / PRIV");
    return;
  }
  ButtonEvent ev;
  memset(&ev, 0, sizeof(ev));
  eui64_copy(&ev.source, &g_reg->shelly[occupied].eui);
  ev.source_ieee_resolved = true;
  ev.button = button;
  ev.kind = BUTTON_KIND_SINGLE;
  ev.timestamp_ms = millis();
  LOG_I("zbtrace", "ATTR→btn %u (dst_ep=%u onoff=%u) sole Shelly", button, dst_ep, onoff);
  handle_button_pipeline(&ev);
}

static void persist_registry() {
  if (g_store != nullptr) {
    storage_save(g_store);
  }
}

static void set_last_event(const char *msg) {
  if (msg == nullptr) {
    g_last_event[0] = '\0';
    return;
  }
  snprintf(g_last_event, sizeof(g_last_event), "%s", msg);
}

#if NESSO_ENABLE_HW
/* Soften BOD so a brief RF current dip does not hard-reset the chip. */
static void soften_brownout_for_rf() {
  brownout_ll_ana_reset_enable(false);
  brownout_ll_reset_config(false, 0x3ff, BROWNOUT_RESET_LEVEL_SYSTEM);
  brownout_ll_intr_enable(false);
  brownout_ll_bod_enable(false);
  LOG_I("zigbee", "brownout HW-reset disabled (USB/RF spike mitigation)");
}

static void apply_zb_tx_power() {
  esp_zb_set_tx_power((int8_t)NESSO_ZB_TX_DBM);
  int8_t got = 0;
  esp_zb_get_tx_power(&got);
  LOG_I("zigbee", "TX power set %d dBm (readback %d)", (int)NESSO_ZB_TX_DBM, (int)got);
}
#endif

static bool join_open_now() {
  if (g_join_until_ms == 0) {
    return false;
  }
  const uint32_t now = millis();
  if ((int32_t)(now - g_join_until_ms) >= 0) {
    g_join_until_ms = 0;
    return false;
  }
  return true;
}

static bool eui_from_zcl_addr(const esp_zb_zcl_addr_t &addr, uint16_t *short_out, Eui64 *eui,
                              bool *resolved) {
  if (short_out) {
    *short_out = 0;
  }
  if (resolved) {
    *resolved = false;
  }
  if (eui == nullptr) {
    return false;
  }
  eui64_clear(eui);

  if (addr.addr_type == ESP_ZB_ZCL_ADDR_TYPE_IEEE) {
    memcpy(eui->bytes, addr.u.ieee_addr, 8);
    const bool ok = !eui64_is_zero(eui);
    if (resolved) {
      *resolved = ok;
    }
    return ok;
  }

  const uint16_t short_addr = addr.u.short_addr;
  if (short_out) {
    *short_out = short_addr;
  }
  esp_zb_ieee_addr_t ieee = {0};
  if (esp_zb_ieee_address_by_short(short_addr, ieee) == ESP_OK) {
    memcpy(eui->bytes, ieee, 8);
    const bool ok = !eui64_is_zero(eui);
    if (resolved) {
      *resolved = ok;
    }
    return ok;
  }
  return false;
}

static bool try_enroll_during_join(const Eui64 *eui) {
  if (g_reg == nullptr || eui == nullptr || eui64_is_zero(eui)) {
    return false;
  }
  if (!join_open_now()) {
    return false;
  }
  if (registry_is_authorized_shelly(g_reg, eui) || registry_is_router(g_reg, eui)) {
    return false;
  }
  char label[16];
  snprintf(label, sizeof(label), "Knop %d", registry_shelly_count(g_reg) + 1);
  if (!registry_enroll_shelly(g_reg, eui, label)) {
    set_last_event("join: registry vol");
    LOG_W("zigbee", "enroll failed (full or conflict)");
    return false;
  }
  persist_registry();
  set_last_event("join: nieuwe knop");
  LOG_I("zigbee", "enrolled Shelly %s count=%d", label, registry_shelly_count(g_reg));
  return true;
}

static void enroll_from_bound_devices() {
  if (!join_open_now() || g_reg == nullptr) {
    return;
  }
  std::list<zb_device_params_t *> devices = g_zb_light.getBoundDevices();
  for (zb_device_params_t *dev : devices) {
    if (dev == nullptr) {
      continue;
    }
    Eui64 eui;
    memcpy(eui.bytes, dev->ieee_addr, 8);
    try_enroll_during_join(&eui);
  }
}

/* Catch children; bind pas als authenticated CHILD (niet UNAUTH=5). */
static void enroll_from_neighbors() {
  if (g_reg == nullptr) {
    return;
  }
  if (g_zb_light.bindJobActive()) {
    return; /* één bind tegelijk; poll stuurt wake-gated TX */
  }
  esp_zb_nwk_info_iterator_t it = ESP_ZB_NWK_INFO_ITERATOR_INIT;
  esp_zb_nwk_neighbor_info_t nbr;
  while (esp_zb_nwk_get_next_neighbor(&it, &nbr) == ESP_OK) {
    if (nbr.relationship != ESP_ZB_NWK_RELATIONSHIP_CHILD &&
        nbr.relationship != ESP_ZB_NWK_RELATIONSHIP_UNAUTHENTICATED_CHILD) {
      continue;
    }
    Eui64 eui;
    memcpy(eui.bytes, nbr.ieee_addr, 8);
    if (eui64_is_zero(&eui)) {
      continue;
    }
    if (join_open_now() && try_enroll_during_join(&eui)) {
      LOG_I("zbtrace", "enrolled short=0x%04x lqi=%u rel=%u", nbr.short_addr, nbr.lqi,
            (unsigned)nbr.relationship);
    }
    if (!registry_is_authorized_shelly(g_reg, &eui)) {
      continue;
    }
    if (nbr.lqi == 0 || nbr.age > 1) {
      continue; /* alleen echt wakker starten */
    }
    if (nbr.relationship == ESP_ZB_NWK_RELATIONSHIP_UNAUTHENTICATED_CHILD) {
      static uint32_t s_enroll_unauth_ms = 0;
      if ((millis() - s_enroll_unauth_ms) > 5000u) {
        s_enroll_unauth_ms = millis();
        LOG_I("zbtrace", "short=0x%04x UNAUTH — enrolled, bind wacht op CHILD", nbr.short_addr);
      }
      continue;
    }
    if (g_onoff_bound_short == nbr.short_addr && g_onoff_bound_count >= BIND_ON_OFF_STEPS) {
      continue;
    }
    LOG_I("zbtrace", "CHILD awake short=0x%04x — start C5-bind", nbr.short_addr);
    g_zb_light.bindRemoteClusters(nbr.short_addr, nbr.ieee_addr, false);
  }
}

static const char *button_kind_nl(ButtonKind kind) {
  switch (kind) {
    case BUTTON_KIND_SINGLE:
      return "enkel";
    case BUTTON_KIND_DOUBLE:
      return "dubbel";
    case BUTTON_KIND_TRIPLE:
      return "tripel";
    case BUTTON_KIND_HOLD:
      return "vasthouden";
    case BUTTON_KIND_LONG:
      return "lang";
    case BUTTON_KIND_DOUBLE_LONG:
      return "2x lang";
    case BUTTON_KIND_TRIPLE_LONG:
      return "3x lang";
    default:
      return "?";
  }
}

static void handle_button_pipeline(ButtonEvent *ev) {
  if (ev == nullptr || g_reg == nullptr || g_dedup == nullptr) {
    LOG_W("zbtrace", "pipeline drop: null ev/reg/dedup");
    return;
  }
  if (join_open_now() && ev->source_ieee_resolved) {
    try_enroll_during_join(&ev->source);
  }

  char eui_s[24];
  fmt_eui(eui_s, sizeof(eui_s), ev->source.bytes);
  const int slot = registry_shelly_index(g_reg, &ev->source);
  const int shelly_nr = (slot >= 0) ? (slot + 1) : 0;
  LOG_I("zbtrace", "pipeline btn=%u kind=%s eui=%s resolved=%d slot=%d join=%d", ev->button,
        button_kind_nl(ev->kind), eui_s, ev->source_ieee_resolved ? 1 : 0, slot,
        join_open_now() ? 1 : 0);

  DispatchOutcome out = dispatcher_handle(g_reg, g_dedup, ev);
  char line[48];
  if (out.result == DISPATCH_QUEUED) {
    ++g_cnt_dispatch_ok;
    g_last_btn_ms = millis();
    const ActionSpec *spec = action_for_button(ev->button);
    const bool ok = modbus_enqueue(spec);
    if (shelly_nr > 0) {
      snprintf(line, sizeof(line), "Shelly %d knop %u %s%s", shelly_nr, ev->button,
               button_kind_nl(ev->kind), ok ? "" : " (Modbus fail)");
    } else {
      snprintf(line, sizeof(line), "knop %u %s%s", ev->button, button_kind_nl(ev->kind),
               ok ? "" : " (Modbus fail)");
    }
    LOG_I("zbtrace", "DISPATCH OK #%lu %s mb=%d", (unsigned long)g_cnt_dispatch_ok, line,
          ok ? 1 : 0);
#if NESSO_ENABLE_HW
    {
      esp_zb_ieee_addr_t ieee = {0};
      memcpy(ieee, ev->source.bytes, 8);
      const uint16_t sa = esp_zb_address_short_by_ieee(ieee);
      g_zb_light.markBoundFromTraffic(sa);
    }
#endif
  } else if (out.result == DISPATCH_AUTH_FAIL) {
    ++g_cnt_auth_fail;
    snprintf(line, sizeof(line), "knop %u geweigerd (niet gekoppeld)", ev->button);
    LOG_W("zbtrace", "AUTH FAIL #%lu %s auth=%d eui=%s — enroll via join of druk tijdens join",
          (unsigned long)g_cnt_auth_fail, line, (int)out.auth, eui_s);
  } else if (out.result == DISPATCH_DEDUPED) {
    if (shelly_nr > 0) {
      snprintf(line, sizeof(line), "Shelly %d knop %u (dubbel)", shelly_nr, ev->button);
    } else {
      snprintf(line, sizeof(line), "knop %u (dubbel)", ev->button);
    }
    LOG_I("zbtrace", "DEDUP %s", line);
  } else {
    snprintf(line, sizeof(line), "knop %u result=%d", ev->button, (int)out.result);
    LOG_W("zbtrace", "DISPATCH other %s", line);
  }
  set_last_event(line);
}

static void ingest_zcl_button(const ZclIncoming *in) {
  if (in == nullptr) {
    return;
  }
  if (g_paused || !g_running) {
    LOG_W("zbtrace", "ingest DROP paused=%d running=%d cl=0x%04x cmd=0x%02x", g_paused ? 1 : 0,
          g_running ? 1 : 0, in->cluster, in->command);
    return;
  }
  char eui_s[24];
  fmt_eui(eui_s, sizeof(eui_s), in->src_ieee.bytes);
  LOG_I("zbtrace", "ZCL-IN cl=0x%04x cmd=0x%02x src_ep=%u short=0x%04x eui=%s ok=%d step=%u scene=%u",
        in->cluster, in->command, in->src_endpoint, in->src_short, eui_s,
        in->ieee_resolved ? 1 : 0, in->step_mode, in->scene_id);

  ButtonEvent ev;
  memset(&ev, 0, sizeof(ev));
  if (!shelly_decode(in, &ev)) {
    ++g_cnt_decode_fail;
    LOG_W("zbtrace", "DECODE FAIL #%lu cl=0x%04x cmd=0x%02x ep=%u step=%u scene=%u "
                     "(mapping kent dit frame niet)",
          (unsigned long)g_cnt_decode_fail, in->cluster, in->command, in->src_endpoint,
          in->step_mode, in->scene_id);
    set_last_event("ZCL onbekend — zie log");
    return;
  }
  ++g_cnt_decode_ok;
  LOG_I("zbtrace", "DECODE OK #%lu → btn=%u kind=%s", (unsigned long)g_cnt_decode_ok, ev.button,
        button_kind_nl(ev.kind));
  ev.timestamp_ms = millis();
  handle_button_pipeline(&ev);
}

static void on_privilege_command(const esp_zb_zcl_privilege_command_message_t *message) {
  /* Runs on Zigbee stack task — keep minimal: copy + queue. No LOG/bind/Modbus. */
  if (message == nullptr) {
    return;
  }
  if (g_paused || !g_running) {
    ++g_cnt_priv_drop;
    return;
  }

  ++g_cnt_priv;
  g_last_priv_ms = millis();

  ZclIncoming in;
  memset(&in, 0, sizeof(in));
  in.cluster = message->info.cluster;
  in.command = message->info.command.id;
  in.src_endpoint = message->info.src_endpoint;
  eui_from_zcl_addr(message->info.src_address, &in.src_short, &in.src_ieee, &in.ieee_resolved);

  if (message->data != nullptr && message->size > 0) {
    const uint8_t *p = (const uint8_t *)message->data;
    if (in.cluster == ZCL_CLUSTER_LEVEL && in.command == ZCL_CMD_STEP && message->size >= 1) {
      in.step_mode = p[0];
    } else if (in.cluster == ZCL_CLUSTER_SCENES && in.command == ZCL_CMD_RECALL_SCENE) {
      if (message->size >= 3) {
        in.scene_id = p[2];
      } else if (message->size >= 1) {
        in.scene_id = p[0];
      }
    }
  }

  snprintf(g_last_raw, sizeof(g_last_raw), "PRIV cl=%04x cmd=%02x ep=%u", in.cluster, in.command,
           in.src_endpoint);
  if (!zcl_q_push(&in)) {
    ++g_cnt_priv_drop;
  }
}

/* Callbacks may be set before begin(); addPrivilegeCommand MUST run after begin(). */
static void register_privilege_callbacks() {
  for (uint8_t i = 0; i < NESSO_ZB_GW_COUNT; ++i) {
    g_zb_lights[i].onPrivilegeCommand(on_privilege_command);
  }
}

static void install_privilege_commands() {
  /* Privilege gives src IEEE/EP (attr callback does not). Op alle 4 gateway-EPs. */
  static const struct {
    uint16_t cluster;
    uint16_t command;
  } k_cmds[] = {
      {ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ZCL_CMD_OFF},
      {ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ZCL_CMD_ON},
      {ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ZCL_CMD_TOGGLE},
      {ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL, ZCL_CMD_STEP},
      {ESP_ZB_ZCL_CLUSTER_ID_SCENES, ZCL_CMD_RECALL_SCENE},
  };
  for (uint8_t ep_i = 0; ep_i < NESSO_ZB_GW_COUNT; ++ep_i) {
    const uint8_t ep = NESSO_ZB_GW_EP((uint8_t)(ep_i + 1));
    for (const auto &c : k_cmds) {
      esp_err_t e = ESP_ERR_TIMEOUT;
      if (esp_zb_lock_acquire(pdMS_TO_TICKS(500))) {
        e = esp_zb_zcl_add_privilege_command(ep, c.cluster, c.command);
        esp_zb_lock_release();
      }
      if (e != ESP_OK) {
        LOG_W("zbtrace", "privilege ep=%u cl=0x%04x cmd=0x%02x → %s", ep, c.cluster, c.command,
              esp_err_to_name(e));
      }
      vTaskDelay(pdMS_TO_TICKS(15));
    }
  }
  LOG_I("zbtrace", "privilege cmds geinstalleerd (%u eps × %u cmds)", (unsigned)NESSO_ZB_GW_COUNT,
        (unsigned)(sizeof(k_cmds) / sizeof(k_cmds[0])));
}

/* Rate-limited: sleepy remotes only accept ZDO bind while awake after a press. */
static void maybe_wake_bind_all(uint16_t short_addr, const Eui64 *eui) {
  if (eui == nullptr) {
    return;
  }
  static uint32_t last_ms = 0;
  static uint8_t last_eui[8] = {0};
  const uint32_t now = millis();
  if (memcmp(last_eui, eui->bytes, 8) == 0 && (now - last_ms) < 15000u) {
    return;
  }
  memcpy(last_eui, eui->bytes, 8);
  last_ms = now;
  esp_zb_ieee_addr_t ieee = {0};
  memcpy(ieee, eui->bytes, 8);
  LOG_I("zigbee", "wake-bind all EPs short=0x%04x", short_addr);
  g_zb_light.bindRemoteClusters(short_addr, ieee);
}

static void rebind_enrolled_shellies() {
  if (g_reg == nullptr) {
    return;
  }
  /* Force: wis "al gebonden"-vlag anders is Rebind een no-op. */
  g_onoff_bound_short = 0xFFFFu;
  g_onoff_bound_count = 0;
  int n = 0;
  int waiting = 0;
  for (int i = 0; i < NESSO_MAX_SHELLY_SLOTS; ++i) {
    if (!g_reg->shelly[i].occupied) {
      continue;
    }
    esp_zb_ieee_addr_t ieee = {0};
    memcpy(ieee, g_reg->shelly[i].eui.bytes, 8);
    const uint16_t short_addr = esp_zb_address_short_by_ieee(ieee);
    if (short_addr == 0xFFFFu || short_addr == 0x0000u) {
      LOG_W("zigbee", "Shelly %d niet in netwerk — druk knop, daarna opnieuw Rebind", i + 1);
      ++waiting;
      continue;
    }
    LOG_I("zigbee", "FORCE rebind Shelly %d short=0x%04x — blijf drukken", i + 1, short_addr);
    g_zb_light.bindRemoteClusters(short_addr, ieee, true);
    ++n;
    break; /* één tegelijk; volgende na DONE via wake/enroll */
  }
  if (n > 0) {
    set_last_event("rebind bezig — blijf knoppen drukken");
  } else if (waiting > 0) {
    set_last_event("rebind: eerst knop drukken (Shelly slapen)");
  } else {
    set_last_event("geen Shelly om te rebinden");
  }
}
#endif /* NESSO_ENABLE_HW */

#if NESSO_ENABLE_HW
static void zigbee_start_task(void *arg) {
  (void)arg;
  LOG_I("zigbee", "background start (Wi-Fi already up)");
  set_last_event("Zigbee starten…");

  if (!g_ep_ready) {
    register_privilege_callbacks();
    for (uint8_t i = 0; i < NESSO_ZB_GW_COUNT; ++i) {
      g_zb_lights[i].allowMultipleBinding(true);
      if (i == 0) {
        g_zb_lights[i].setManufacturerAndModel("Qvantum", "NessoN1Bridge");
      }
      if (!Zigbee.addEndpoint(&g_zb_lights[i])) {
        LOG_E("zbtrace", "addEndpoint EP%u failed", NESSO_ZB_GW_EP((uint8_t)(i + 1)));
        g_start_failed = true;
        g_start_pending = false;
        g_start_task_launched = false;
        set_last_event("endpoint fout");
        vTaskDelete(nullptr);
        return;
      }
      LOG_I("zbtrace", "gateway EP%u added (Shelly knop %u)", NESSO_ZB_GW_EP((uint8_t)(i + 1)),
            (unsigned)(i + 1));
    }
    g_ep_ready = true;
  }

  /* Reinforce coex (also enabled in wifi_sta_begin). */
  const esp_err_t coex = esp_coex_wifi_i154_enable();
  LOG_I("zigbee", "esp_coex_wifi_i154_enable → %d", (int)coex);

  Zigbee.setTimeout(45000);
  if (!Zigbee.begin(ZIGBEE_COORDINATOR, false)) {
    LOG_E("zigbee", "Zigbee.begin(COORDINATOR) failed");
    g_running = false;
    g_start_failed = true;
    g_start_pending = false;
    g_start_task_launched = false;
    set_last_event("Zigbee start mislukt");
    wifi_sta_reconnect();
    vTaskDelete(nullptr);
    return;
  }

  /* Nog NIET g_running: privilege-install + poll/diag tegelijk = freeze. */
  g_join_ready = false;
  g_start_pending = false;
  g_start_failed = false;
  soften_brownout_for_rf();
  apply_zb_tx_power();
  install_privilege_commands(); /* AFTER begin — otherwise lock fails and presses are invisible */
  Zigbee.setDebugMode(false); /* true floodt USB Serial → freeze bij knopdruk */
  LOG_I("zbtrace", "coordinator up fw=%s shellys=%d wifi=%d gw_ep=%u-%u C5-bind debug=1",
        NESSO_FW_VERSION, g_reg ? registry_shelly_count(g_reg) : 0, wifi_sta_connected() ? 1 : 0,
        (unsigned)NESSO_ZB_GW_EP0, (unsigned)(NESSO_ZB_GW_EP0 + NESSO_ZB_GW_COUNT - 1));

  /* Formation often drops STA on single-RF C6 — reconnect and wait a bit. */
  for (int i = 0; i < 20 && !wifi_sta_connected(); ++i) {
    if (i == 0 || i == 5 || i == 10) {
      wifi_sta_reconnect();
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  LOG_I("zigbee", "post-start wifi=%d — settle…", wifi_sta_connected() ? 1 : 0);

  /* Join-tijdens-start negeren; poll nog NIET starten (g_running blijft false). */
  g_deferred_join_s = 0;
  g_pending_join_s = 0;
  vTaskDelay(pdMS_TO_TICKS(1500)); /* coex/USB laten zakken vóór poll/bind */

  g_settle_until_ms = millis() + 10000UL; /* 10s geen auto-enroll/diag */
  g_running = true;
  g_join_ready = true;
  g_start_task_launched = false;
  set_last_event(registry_shelly_count(g_reg) > 0 ? "klaar — druk Join, 5× paars"
                                                  : "coordinator aan");
  LOG_I("zbtrace", "KLAAR fw=%s — wacht 10s of druk Join", NESSO_FW_VERSION);
  vTaskDelete(nullptr);
}
#endif

bool zigbee_coordinator_begin(DeviceRegistry *reg, EventDedup *dedup, AppConfigStore *store) {
  g_reg = reg;
  g_dedup = dedup;
  g_store = store;
  g_paused = false;
  g_join_until_ms = 0;
  g_join_ready = false;
  g_last_event[0] = '\0';
  g_start_failed = false;
  g_start_task_launched = false;

#if NESSO_ENABLE_HW
  g_deferred_join_s = 0;
  if (Zigbee.started()) {
    g_running = true;
    g_join_ready = true;
    g_start_pending = false;
    LOG_I("zigbee", "already started");
    return true;
  }

  /* Only start after Wi-Fi STA is connected. Never block the UI loop. */
  g_start_pending = true;
  g_running = false;
  set_last_event("wacht op Wi-Fi");
  LOG_I("zigbee", "waiting for Wi-Fi before Zigbee (ESP32-C6 coex=C1)");
  return true;
#else
  LOG_W("zigbee", "scaffold (HOST) — no radio");
  g_running = false;
  return false;
#endif
}

void zigbee_coordinator_poll() {
#if NESSO_ENABLE_HW
  if (g_paused) {
    return;
  }

  if (g_start_pending && !g_running && !g_start_failed && !g_start_task_launched) {
    if (wifi_sta_connected()) {
      g_start_task_launched = true;
      set_last_event("Zigbee starten…");
      LOG_I("zigbee", "Wi-Fi up — launching Zigbee task");
      /* Separate task: Zigbee.begin() can block ~tens of seconds. */
      xTaskCreate(zigbee_start_task, "zb_start", 8192, nullptr, 1, nullptr);
    }
  }

  if (!g_running || !g_join_ready) {
    return;
  }
  /* Bind klaar → join dicht (timer bleef anders hangen / Wi-Fi hold-off). */
  if (g_pending_close_join_after_bind) {
    g_pending_close_join_after_bind = false;
    if (join_open_now() || g_join_until_ms != 0) {
      LOG_I("zbtrace", "bind OK → join sluiten");
      zigbee_close_join();
    }
  }
  if (g_join_until_ms != 0 && !join_open_now()) {
    /* Niet meer verlengen tijdens bind — dat zette join vast + Wi-Fi hold-off. */
    LOG_I("zigbee", "join window ended");
    g_join_until_ms = 0;
#if NESSO_ENABLE_HW
    wifi_sta_set_join_tx_low(false);
    wifi_sta_set_hold_off(false);
    wifi_sta_ensure();
#endif
    if (!g_zb_light.bindJobActive()) {
      set_last_event("join gesloten");
    }
  }
  /* Join open/close op achtergrond — openNetwork/TX mag UI-thread niet blokkeren. */
  if (!g_join_op_busy) {
    if (g_pending_close_join) {
      g_pending_close_join = false;
      g_join_op_busy = true;
      xTaskCreate(zigbee_join_close_task, "zb_jclose", 4096, nullptr, 2, nullptr);
    } else if (g_pending_join_s != 0) {
      const uint8_t s = g_pending_join_s;
      g_pending_join_s = 0;
      g_join_op_busy = true;
      xTaskCreate(zigbee_join_open_task, "zb_jopen", 4096, (void *)(uintptr_t)s, 2, nullptr);
    }
  }

  static uint32_t last_bind_scan_ms = 0;
  const uint32_t now = millis();

  /* Drain PRIV queue op UI/main-thread (veilig voor Modbus/log/bind). */
  {
    ZclIncoming in;
    uint8_t n = 0;
    while (n < ZCL_Q_LEN && zcl_q_pop(&in)) {
      char eui_s[24];
      fmt_eui(eui_s, sizeof(eui_s), in.src_ieee.bytes);
      LOG_I("zbtrace", "PRIV-Q cl=0x%04x cmd=0x%02x src_ep=%u short=0x%04x eui=%s", in.cluster,
            in.command, in.src_endpoint, in.src_short, eui_s);
      if (in.ieee_resolved && in.src_short != 0 && in.src_short != 0xFFFFu &&
          !(g_onoff_bound_short == in.src_short &&
            g_onoff_bound_count >= BIND_ON_OFF_STEPS)) {
        maybe_wake_bind_all(in.src_short, &in.src_ieee);
      }
      ingest_zcl_button(&in);
      ++n;
    }
    if (g_pending_attr_ep != 0) {
      const uint8_t ep = g_pending_attr_ep;
      const uint8_t onoff = g_pending_attr_onoff;
      g_pending_attr_ep = 0;
      g_pending_attr_onoff = 0xFF;
      LOG_I("zbtrace", "ATTR-Q dst_ep=%u onoff=%u", ep, onoff);
      try_attr_as_button(ep, onoff);
    }
  }

  g_zb_light.pollBindQueue();
  const bool settling = (g_settle_until_ms != 0 && (int32_t)(now - g_settle_until_ms) < 0);
  if (!settling && (now - last_bind_scan_ms) > 2000) {
    last_bind_scan_ms = now;
    if (join_open_now()) {
      enroll_from_bound_devices();
    }
    enroll_from_neighbors();
  }
  /* Geen periodieke diag (USB/RF load). Alleen handmatig via portal-knop. */
#else
  (void)0;
#endif
}

#if NESSO_ENABLE_HW
static void zigbee_join_open_task(void *arg) {
  const uint8_t seconds = (uint8_t)(uintptr_t)arg;
  set_last_event("join starten…");
  wifi_sta_set_hold_off(true); /* stop Wi-Fi reconnect thrash during pairing */
  soften_brownout_for_rf();
  wifi_sta_set_join_tx_low(true);
  WiFi.setSleep(true);
  vTaskDelay(pdMS_TO_TICKS(400)); /* PA/coex settle — korte delay freezet sneller */
  esp_err_t err = ESP_FAIL;
  if (esp_zb_lock_acquire(pdMS_TO_TICKS(1500))) {
    err = esp_zb_bdb_open_network(seconds);
    esp_zb_lock_release();
  } else {
    LOG_W("zigbee", "open_join: zb lock timeout — geen fallback (voorkom hang)");
    err = ESP_ERR_TIMEOUT;
  }
  if (err != ESP_OK) {
    LOG_W("zigbee", "open_network → %s", esp_err_to_name(err));
    g_join_until_ms = 0;
    wifi_sta_set_join_tx_low(false);
    wifi_sta_set_hold_off(false);
    WiFi.setSleep(false);
    set_last_event("join mislukt — opnieuw");
  } else {
    g_join_until_ms = millis() + (uint32_t)seconds * 1000UL;
    set_last_event("join open — 5× knop");
    LOG_I("zigbee", "join open %u s — Shelly: 5× knop; daarna 1× = bind alle 4",
          (unsigned)seconds);
  }
  g_join_op_busy = false;
  vTaskDelete(nullptr);
}

static void zigbee_join_close_task(void *arg) {
  (void)arg;
  if (Zigbee.started()) {
    if (esp_zb_lock_acquire(pdMS_TO_TICKS(800))) {
      esp_zb_bdb_close_network();
      esp_zb_lock_release();
    } else {
      Zigbee.closeNetwork();
    }
  }
  wifi_sta_set_join_tx_low(false);
  wifi_sta_set_hold_off(false);
  wifi_sta_ensure();
  set_last_event("join gesloten");
  LOG_I("zigbee", "join closed");
  g_join_op_busy = false;
  vTaskDelete(nullptr);
}
#endif

bool zigbee_open_join(uint8_t seconds) {
  if (seconds == 0) {
    seconds = (uint8_t)NESSO_ZIGBEE_JOIN_WINDOW_S;
  }
  if (seconds > NESSO_ZIGBEE_JOIN_WINDOW_S) {
    seconds = (uint8_t)NESSO_ZIGBEE_JOIN_WINDOW_S;
  }
#if NESSO_ENABLE_HW
  if (g_paused) {
    LOG_W("zigbee", "open_join refused (paused)");
    return false;
  }
  /* Join tijdens Zigbee.begin/privilege → short=0xffff + netwerk-chaos. */
  if (!g_join_ready || !g_running || !Zigbee.started()) {
    g_deferred_join_s = seconds;
    set_last_event("wacht: Zigbee start…");
    LOG_I("zigbee", "open_join deferred %u s (not ready yet)", (unsigned)seconds);
    return true;
  }
  g_pending_close_join = false;
  g_pending_join_s = seconds;
  g_join_until_ms = millis() + (uint32_t)seconds * 1000UL;
  set_last_event("join starten…");
  LOG_I("zigbee", "open_join queued %u s", (unsigned)seconds);
  return true;
#else
  (void)seconds;
  return false;
#endif
}

void zigbee_close_join() {
  g_join_until_ms = 0;
#if NESSO_ENABLE_HW
  g_pending_join_s = 0;
  g_pending_close_join = true;
  set_last_event("join sluiten…");
#else
  set_last_event("join gesloten");
#endif
  LOG_I("zigbee", "close_join queued");
}

bool zigbee_join_is_open() {
#if NESSO_ENABLE_HW
  return join_open_now();
#else
  return false;
#endif
}

uint32_t zigbee_join_remaining_s() {
#if NESSO_ENABLE_HW
  if (!join_open_now()) {
    return 0;
  }
  const uint32_t now = millis();
  return (g_join_until_ms - now + 999UL) / 1000UL;
#else
  return 0;
#endif
}

void zigbee_pause() {
  g_paused = true;
  g_join_until_ms = 0;
#if NESSO_ENABLE_HW
  wifi_sta_set_hold_off(false);
  if (Zigbee.started()) {
    Zigbee.closeNetwork();
    Zigbee.stop();
  }
#endif
  g_running = false;
  LOG_I("zigbee", "paused (required before SoftAP/BLE)");
}

void zigbee_resume() {
#if NESSO_ENABLE_HW
  if (Zigbee.initialized() && !Zigbee.started()) {
    Zigbee.start();
  }
  g_running = Zigbee.started();
#else
  g_running = false;
#endif
  g_paused = false;
  LOG_I("zigbee", "resume running=%d", g_running ? 1 : 0);
}

bool zigbee_is_running() {
  return g_running && !g_paused;
}

bool zigbee_join_ready() {
  return g_join_ready && g_running && !g_paused;
}

int zigbee_shelly_count() {
  return g_reg ? registry_shelly_count(g_reg) : 0;
}

const char *zigbee_last_event() {
  return g_last_event;
}

bool zigbee_remove_shelly_index(uint8_t index) {
  if (g_reg == nullptr || index >= NESSO_MAX_SHELLY_SLOTS) {
    return false;
  }
  if (!g_reg->shelly[index].occupied) {
    return false;
  }
  Eui64 eui = g_reg->shelly[index].eui;
  if (!registry_remove_shelly(g_reg, &eui)) {
    return false;
  }
#if NESSO_ENABLE_HW
  persist_registry();
#endif
  set_last_event("knop verwijderd");
  return true;
}

void zigbee_diag_dump_now() {
#if NESSO_ENABLE_HW
  if (!g_running || !g_join_ready) {
    /* Stil tijdens start — log-spam + neighbor-walk freezet USB. */
    return;
  }
  const uint32_t now = millis();
  LOG_I("zbtrace",
        "DIAG attr=%lu priv=%lu priv_drop=%lu dec_ok=%lu dec_fail=%lu disp=%lu auth_fail=%lu "
        "bind_ok=%lu bind_fail=%lu | age_attr=%ld age_priv=%ld age_btn=%ld | last=%s",
        (unsigned long)g_cnt_attr, (unsigned long)g_cnt_priv, (unsigned long)g_cnt_priv_drop,
        (unsigned long)g_cnt_decode_ok, (unsigned long)g_cnt_decode_fail,
        (unsigned long)g_cnt_dispatch_ok, (unsigned long)g_cnt_auth_fail,
        (unsigned long)g_cnt_bind_ok, (unsigned long)g_cnt_bind_fail,
        g_last_attr_ms ? (long)(now - g_last_attr_ms) : -1L,
        g_last_priv_ms ? (long)(now - g_last_priv_ms) : -1L,
        g_last_btn_ms ? (long)(now - g_last_btn_ms) : -1L, g_last_raw[0] ? g_last_raw : "-");

  if (g_reg != nullptr) {
    for (int i = 0; i < NESSO_MAX_SHELLY_SLOTS; ++i) {
      if (!g_reg->shelly[i].occupied) {
        continue;
      }
      char eui_s[24];
      fmt_eui(eui_s, sizeof(eui_s), g_reg->shelly[i].eui.bytes);
      esp_zb_ieee_addr_t ieee = {0};
      memcpy(ieee, g_reg->shelly[i].eui.bytes, 8);
      const uint16_t short_addr = esp_zb_address_short_by_ieee(ieee);
      LOG_I("zbtrace", "  enrolled[%d] %s eui=%s short=0x%04x en=%d", i, g_reg->shelly[i].label,
            eui_s, short_addr, g_reg->shelly[i].enabled ? 1 : 0);
    }
  }

  esp_zb_nwk_info_iterator_t it = ESP_ZB_NWK_INFO_ITERATOR_INIT;
  esp_zb_nwk_neighbor_info_t nbr;
  int n_nbr = 0;
  bool any_unauth = false;
  bool any_child = false;
  while (esp_zb_nwk_get_next_neighbor(&it, &nbr) == ESP_OK) {
    char eui_s[24];
    fmt_eui(eui_s, sizeof(eui_s), nbr.ieee_addr);
    LOG_I("zbtrace", "  nbr short=0x%04x eui=%s rel=%u depth=%u lqi=%u age=%u", nbr.short_addr,
          eui_s, (unsigned)nbr.relationship, (unsigned)nbr.depth, (unsigned)nbr.lqi,
          (unsigned)nbr.age);
    if (nbr.relationship == ESP_ZB_NWK_RELATIONSHIP_UNAUTHENTICATED_CHILD) {
      any_unauth = true;
    }
    if (nbr.relationship == ESP_ZB_NWK_RELATIONSHIP_CHILD) {
      any_child = true;
    }
    ++n_nbr;
  }
  LOG_I("zbtrace", "  neighbors=%d bind_job=%d step=%u join=%d light_ep=%u", n_nbr,
        g_zb_light.bindJobActive() ? 1 : 0, (unsigned)g_zb_light.bindStep(),
        join_open_now() ? 1 : 0, (unsigned)NESSO_ZB_LIGHT_EP);

  bool enrolled_with_short = false;
  if (g_reg != nullptr) {
    for (int i = 0; i < NESSO_MAX_SHELLY_SLOTS; ++i) {
      if (!g_reg->shelly[i].occupied) {
        continue;
      }
      esp_zb_ieee_addr_t ieee = {0};
      memcpy(ieee, g_reg->shelly[i].eui.bytes, 8);
      const uint16_t sa = esp_zb_address_short_by_ieee(ieee);
      if (sa != 0xFFFFu && sa != 0) {
        enrolled_with_short = true;
        break;
      }
    }
  }
  bool any_ffff = false;
  if (g_reg != nullptr) {
    for (int i = 0; i < NESSO_MAX_SHELLY_SLOTS; ++i) {
      if (!g_reg->shelly[i].occupied) {
        continue;
      }
      esp_zb_ieee_addr_t ieee = {0};
      memcpy(ieee, g_reg->shelly[i].eui.bytes, 8);
      if (esp_zb_address_short_by_ieee(ieee) == 0xFFFFu) {
        any_ffff = true;
        break;
      }
    }
  }
  if (g_cnt_dispatch_ok > 0) {
    LOG_I("zbtrace", "HINT: knoppad OK (dispatch=%lu) — test alle 4 knoppen / hold / dubbel.",
          (unsigned long)g_cnt_dispatch_ok);
  } else if (g_cnt_decode_ok > 0 && g_cnt_auth_fail > 0) {
    LOG_W("zbtrace", "HINT: decode OK maar auth fail — EUI niet in registry (join opnieuw).");
  } else if (g_cnt_priv > 0 && g_cnt_decode_fail > 0 && g_cnt_decode_ok == 0) {
    LOG_W("zbtrace", "HINT: PRIV wel, decode faalt — ZCL mapping/ep/cmd klopt niet.");
  } else if (g_cnt_priv > 0 && g_cnt_decode_ok == 0) {
    LOG_I("zbtrace", "HINT: PRIV binnen — druk nog eens; verwacht DECODE OK + DISPATCH OK.");
  } else if (g_cnt_bind_ok >= 4 && g_cnt_attr == 0 && g_cnt_priv == 0) {
    LOG_I("zbtrace", "HINT: bind OK (%lu) — druk nu een knop; verwacht PRIV/ATTR.",
          (unsigned long)g_cnt_bind_ok);
  } else if (g_cnt_attr == 0 && g_cnt_priv == 0 && any_ffff) {
    LOG_W("zbtrace", "HINT: short=0xFFFF — knop niet in Zigbee-tabel. "
                     "Wacht tot Zigbee klaar, open join, 5× Shelly (paars).");
  } else if (g_cnt_attr == 0 && g_cnt_priv == 0 && n_nbr == 0 && enrolled_with_short) {
    LOG_W("zbtrace", "HINT: enrolled maar neighbors=0 — knop niet op RF. "
                     "Open join + 5× Shelly (paars) opnieuw joinen (alleen drukken helpt niet).");
  } else if (g_cnt_attr == 0 && g_cnt_priv == 0 && any_unauth && !any_child) {
    LOG_W("zbtrace", "HINT: knop is UNAUTH (rel=5) — join niet af. Open join, 5x Shelly "
                     "tot DIAG rel=1 (CHILD), dán bind.");
  } else if (g_cnt_attr == 0 && g_cnt_priv == 0) {
    LOG_W("zbtrace", "HINT: nog geen ATTR/PRIV — binding ontbreekt of knop stuurt niet naar ons. "
                     "Open join, 5x Shelly, daarna 1x drukken terwijl wakker.");
  } else if (g_cnt_attr > 0 && g_cnt_priv == 0) {
    LOG_W("zbtrace", "HINT: ATTR wel, PRIV niet — privilege-cmds of src-adres probleem.");
  }
#else
  LOG_I("zbtrace", "diag scaffold");
#endif
}

void zigbee_diag_rebind_all() {
#if NESSO_ENABLE_HW
  LOG_I("zbtrace", "diag: force rebind all enrolled");
  rebind_enrolled_shellies();
#else
  (void)0;
#endif
}

void zigbee_format_diag_json(char *buf, size_t buflen) {
  if (buf == nullptr || buflen < 64) {
    return;
  }
  const uint32_t now = millis();
  snprintf(buf, buflen,
           "{\"fw\":\"%s\",\"running\":%s,\"paused\":%s,\"join_open\":%s,"
           "\"attr\":%lu,\"priv\":%lu,\"priv_drop\":%lu,\"decode_ok\":%lu,\"decode_fail\":%lu,"
           "\"dispatch_ok\":%lu,\"auth_fail\":%lu,\"bind_ok\":%lu,\"bind_fail\":%lu,"
           "\"ms_since_attr\":%ld,\"ms_since_priv\":%ld,\"ms_since_btn\":%ld,"
           "\"last_raw\":\"%s\",\"last_event\":\"%s\",\"shelly_count\":%d,"
           "\"bind_job\":%s,\"bind_step\":%u,\"light_ep\":%u}",
           NESSO_FW_VERSION, zigbee_is_running() ? "true" : "false", g_paused ? "true" : "false",
           zigbee_join_is_open() ? "true" : "false", (unsigned long)g_cnt_attr,
           (unsigned long)g_cnt_priv, (unsigned long)g_cnt_priv_drop, (unsigned long)g_cnt_decode_ok,
           (unsigned long)g_cnt_decode_fail, (unsigned long)g_cnt_dispatch_ok,
           (unsigned long)g_cnt_auth_fail, (unsigned long)g_cnt_bind_ok,
           (unsigned long)g_cnt_bind_fail, g_last_attr_ms ? (long)(now - g_last_attr_ms) : -1L,
           g_last_priv_ms ? (long)(now - g_last_priv_ms) : -1L,
           g_last_btn_ms ? (long)(now - g_last_btn_ms) : -1L, g_last_raw[0] ? g_last_raw : "",
           g_last_event[0] ? g_last_event : "", zigbee_shelly_count(),
#if NESSO_ENABLE_HW
           g_zb_light.bindJobActive() ? "true" : "false", (unsigned)g_zb_light.bindStep(),
           (unsigned)NESSO_ZB_LIGHT_EP
#else
           "false", 0u, 0u
#endif
  );
}

void zigbee_format_status_json(char *buf, size_t buflen) {
  if (buf == nullptr || buflen < 32) {
    return;
  }
  const int count = zigbee_shelly_count();
  int n = snprintf(buf, buflen,
                   "{\"running\":%s,\"join_open\":%s,\"join_remaining_s\":%u,\"shelly_count\":%d,"
                   "\"last_event\":\"%s\",\"shellys\":[",
                   zigbee_is_running() ? "true" : "false", zigbee_join_is_open() ? "true" : "false",
                   (unsigned)zigbee_join_remaining_s(), count, g_last_event[0] ? g_last_event : "");
  if (n < 0 || (size_t)n >= buflen) {
    buf[0] = '\0';
    return;
  }
  size_t used = (size_t)n;
  if (g_reg != nullptr) {
    for (int i = 0; i < NESSO_MAX_SHELLY_SLOTS; ++i) {
      if (!g_reg->shelly[i].occupied) {
        continue;
      }
      const Eui64 *e = &g_reg->shelly[i].eui;
      char eui_str[28];
      snprintf(eui_str, sizeof(eui_str), "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", e->bytes[7],
               e->bytes[6], e->bytes[5], e->bytes[4], e->bytes[3], e->bytes[2], e->bytes[1],
               e->bytes[0]);
      const char *comma = (used > 0 && buf[used - 1] == '[') ? "" : ",";
      int m = snprintf(buf + used, buflen - used,
                       "%s{\"index\":%d,\"label\":\"%s\",\"eui\":\"%s\",\"enabled\":%s}", comma, i,
                       g_reg->shelly[i].label[0] ? g_reg->shelly[i].label : "Knop", eui_str,
                       g_reg->shelly[i].enabled ? "true" : "false");
      if (m < 0 || (size_t)m >= buflen - used) {
        break;
      }
      used += (size_t)m;
    }
  }
  if (used + 2 < buflen) {
    buf[used++] = ']';
    buf[used++] = '}';
    buf[used] = '\0';
  }
}
