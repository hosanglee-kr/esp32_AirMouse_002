#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : W10_WebConfig_020.h
 * 모듈약어 : W10
 * 모듈명 : Web Config Server (Gzip Assets, Keycodes, Status, STA+mDNS, Live Control)
 * ------------------------------------------------------
 * 기능 요약
 *  - 정적 리소스(.gz 자동) + 캐시 정책(immutable/ no-store)
 *  - /api/keycodes: HID Usage ID 기반 키코드 대량 제공 + mods(mask)
 *  - /api/status: BLE/IP/heap/uptime/DPI/PPT + gyro/temp/dt/err/RSSI/stack/degraded
 *  - STA 모드 + mDNS (예: elite-airmouse.local)
 *  - 웹에서 PPT 모드 토글 / DPI 즉시 변경 + 튜닝 즉시 적용 + 저장/리셋/재부팅
 * ------------------------------------------------------
 * [구현 규칙]
 *  - 항상 소스 시작 주석 부분 체계 유지 및 내용 업데이트
 *  - 소스 시작 주석 부분 구현규칙, 코드네이밍규칙 내용 그대로 유지, 수정금지
 *  - ArduinoJson v7.x.x 사용 (v6 이하 사용 금지)
 *  - JsonDocument 단일 타입만 사용
 *  - createNestedArray/Object/containsKey 사용 금지
 *  - memset + strlcpy 기반 안전 초기화
 *  - 주석/필드명은 JSON 구조와 동일하게 유지
 *  - 변수명은 가능한 해석 가능하게
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 *   - namespace 명        : 모듈약어_ 접두사
 *   - namespace 내 상수    : 모둘약어 접두시 미사용
 *   - 전역 상수,매크로      : G_모듈약어_ 접두사
 *   - 전역 변수             : g_모듈약어_ 접두사
 *   - 전역 함수             : 모듈약어_ 접두사
 *   - type                  : T_모듈약어_ 접두사
 *   - typedef               : _t  접미사
 *   - enum 상수             : EN_모듈약어_ 접두사
 *   - 구조체                : ST_모듈약어_ 접두사
 *   - 클래스명              : CL_모듈약어_ 접두사 , 버전 제거
 *   - 클래스 private 멤버   : _ 접두사
 *   - 클래스 멤버(함수/변수) : 모듈약어 접두사 미사용
 *   - 클래스 정적 멤버      : s_ 접두사
 *   - 함수 로컬 변수        : v_ 접두사
 *   - 함수 인자             : p_ 접두사
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>

#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include <ESPAsyncWebServer.h>

#include "C10_Config_020.h"
#include "E10_EliteAirMouse_020.h"

class CL_W10_WebConfig {
  private:
    AsyncWebServer _svr;

    CL_C10_Config* _cfg = nullptr;
    T_E10_ApplyFn  _applyFn = nullptr;
    void*          _applyCtx = nullptr;

    ST_C10_WiFiConfig_t _wifi;
    ST_C10_E10Config_t  _e10;

    // ---------- Assets (gzip auto) ----------
    struct ST_W10_Asset_t {
        const char* uri;
        const char* fs_path_plain;
        const char* fs_path_gz;
        const char* content_type;
        bool        cache_immutable;
    };

    static constexpr ST_W10_Asset_t s_assets[] = {
        { "/",                 "/www/index_020.html", "/www/index_020.html.gz", "text/html", false },
        { "/www/",             "/www/index_020.html", "/www/index_020.html.gz", "text/html", false },

        { "/www/style_020.css","/www/style_020.css",  "/www/style_020.css.gz",  "text/css", true  },
        { "/www/app_020.js",  "/www/app_020.js",      "/www/app_020.js.gz",     "application/javascript", true  },
    };

    static constexpr const char* G_W10_LEGACY_STYLE_URI = "/www/style.css";
    static constexpr const char* G_W10_LEGACY_APP_URI   = "/www/app.js";
    static constexpr const char* G_W10_STYLE_URI        = "/www/style_020.css";
    static constexpr const char* G_W10_APP_URI          = "/www/app_020.js";

    // ---------- Key tables ----------
    struct ST_W10_Key_t { const char* name; uint16_t usage; };
    struct ST_W10_Mod_t { const char* name; uint8_t mask; };

    static constexpr ST_W10_Mod_t s_mods[] = {
        { "None",   0x00 },
        { "LCtrl",  0x01 },
        { "LShift", 0x02 },
        { "LAlt",   0x04 },
        { "LMeta",  0x08 },
        { "RCtrl",  0x10 },
        { "RShift", 0x20 },
        { "RAlt",   0x40 },
        { "RMeta",  0x80 },
    };

    static constexpr ST_W10_Key_t s_keys[] = {
        { "None", 0 },

        // Letters
        { "A", 0x04 },{ "B", 0x05 },{ "C", 0x06 },{ "D", 0x07 },{ "E", 0x08 },{ "F", 0x09 },
        { "G", 0x0A },{ "H", 0x0B },{ "I", 0x0C },{ "J", 0x0D },{ "K", 0x0E },{ "L", 0x0F },
        { "M", 0x10 },{ "N", 0x11 },{ "O", 0x12 },{ "P", 0x13 },{ "Q", 0x14 },{ "R", 0x15 },
        { "S", 0x16 },{ "T", 0x17 },{ "U", 0x18 },{ "V", 0x19 },{ "W", 0x1A },{ "X", 0x1B },
        { "Y", 0x1C },{ "Z", 0x1D },

        // Numbers
        { "1", 0x1E },{ "2", 0x1F },{ "3", 0x20 },{ "4", 0x21 },{ "5", 0x22 },
        { "6", 0x23 },{ "7", 0x24 },{ "8", 0x25 },{ "9", 0x26 },{ "0", 0x27 },

        // Control
        { "Enter", 0x28 },{ "Esc", 0x29 },{ "Backspace", 0x2A },{ "Tab", 0x2B },{ "Space", 0x2C },

        // Symbols
        { "-", 0x2D },{ "=", 0x2E },{ "[", 0x2F },{ "]", 0x30 },{ "\\", 0x31 },
        { "NonUS#", 0x32 },{ ";", 0x33 },{ "'", 0x34 },{ "`", 0x35 },{ ",", 0x36 },{ ".", 0x37 },{ "/", 0x38 },

        // Function
        { "F1", 0x3A },{ "F2", 0x3B },{ "F3", 0x3C },{ "F4", 0x3D },{ "F5", 0x3E },{ "F6", 0x3F },
        { "F7", 0x40 },{ "F8", 0x41 },{ "F9", 0x42 },{ "F10",0x43 },{ "F11",0x44 },{ "F12",0x45 },

        // Nav
        { "PrintScreen", 0x46 },{ "ScrollLock", 0x47 },{ "Pause", 0x48 },
        { "Insert", 0x49 },{ "Home", 0x4A },{ "PageUp", 0x4B },{ "Delete", 0x4C },{ "End", 0x4D },{ "PageDown", 0x4E },
        { "Right", 0x4F },{ "Left", 0x50 },{ "Down", 0x51 },{ "Up", 0x52 },

        // Keypad
        { "KP_NumLock", 0x53 },{ "KP_/ ", 0x54 },{ "KP_*", 0x55 },{ "KP_-", 0x56 },{ "KP_+", 0x57 },
        { "KP_Enter", 0x58 },{ "KP_1", 0x59 },{ "KP_2", 0x5A },{ "KP_3", 0x5B },{ "KP_4", 0x5C },{ "KP_5", 0x5D },
        { "KP_6", 0x5E },{ "KP_7", 0x5F },{ "KP_8", 0x60 },{ "KP_9", 0x61 },{ "KP_0", 0x62 },{ "KP_.", 0x63 },
    };

    bool _mdnsStarted = false;

    static void s_handleWifiEvent(WiFiEvent_t p_event, WiFiEventInfo_t p_info);
    void _handleWifi(WiFiEvent_t p_event);
    static CL_W10_WebConfig* s_instance;

  public:
    CL_W10_WebConfig() : _svr(80) {
        s_instance = this;
        memset(&_wifi, 0, sizeof(_wifi));
        memset(&_e10,  0, sizeof(_e10));
    }

    void begin(CL_C10_Config* p_cfg, T_E10_ApplyFn p_applyFn, void* p_applyCtx) {
        _cfg = p_cfg;
        _applyFn = p_applyFn;
        _applyCtx = p_applyCtx;

        WiFi.onEvent(s_handleWifiEvent);

        (void)LittleFS.begin(true);

        (void)_cfg->loadAll(_wifi, _e10);

        setupWiFi();

        // assets
        for (size_t i = 0; i < (sizeof(s_assets) / sizeof(s_assets[0])); i++) {
            const ST_W10_Asset_t& v_a = s_assets[i];
            _svr.on(v_a.uri, HTTP_GET, [this, v_a](AsyncWebServerRequest* p_req){
                this->serveAsset(p_req, v_a);
            });
        }

        // legacy redirect
        _svr.on(G_W10_LEGACY_STYLE_URI, HTTP_GET, [](AsyncWebServerRequest* p_req){ p_req->redirect(G_W10_STYLE_URI); });
        _svr.on(G_W10_LEGACY_APP_URI, HTTP_GET, [](AsyncWebServerRequest* p_req){ p_req->redirect(G_W10_APP_URI); });

        // API
        _svr.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* p_req){ this->apiGetConfig(p_req); });

        _svr.on("/api/config", HTTP_POST,
            [this](AsyncWebServerRequest* p_req){ (void)p_req; },
            nullptr,
            [this](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total){
                this->apiPostConfig(p_req, p_data, p_len, p_index, p_total);
            }
        );

        _svr.on("/api/keycodes", HTTP_GET, [this](AsyncWebServerRequest* p_req){ this->apiKeycodes(p_req); });
        _svr.on("/api/status",   HTTP_GET, [this](AsyncWebServerRequest* p_req){ this->apiStatus(p_req); });

        // immediate control: ppt/dpi/clicklock + quick tuning
        _svr.on("/api/control", HTTP_POST,
            [this](AsyncWebServerRequest* p_req){ (void)p_req; },
            nullptr,
            [this](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total){
                this->apiControl(p_req, p_data, p_len, p_index, p_total);
            }
        );

        _svr.on("/api/reset", HTTP_POST,
            [this](AsyncWebServerRequest* p_req){ (void)p_req; },
            nullptr,
            [this](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total){
                (void)p_data; (void)p_len; (void)p_index; (void)p_total;
                this->apiReset(p_req);
            }
        );

        _svr.on("/api/reboot", HTTP_POST,
            [this](AsyncWebServerRequest* p_req){ (void)p_req; },
            nullptr,
            [this](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total){
                (void)p_data; (void)p_len; (void)p_index; (void)p_total;
                this->apiReboot(p_req);
            }
        );

        _svr.onNotFound([](AsyncWebServerRequest* p_req){
            p_req->send(404, "text/plain", "not found");
        });

        _svr.begin();

        Serial.printf("[W10] WebConfig started. mode=%s, ip=%s\n",
                      WiFi.getMode() == WIFI_AP ? "AP" : "STA",
                      WiFi.localIP().toString().c_str());
    }

  private:
    // -------------------------
    // WiFi + mDNS
    // -------------------------
    void setupWiFi() {
        WiFi.mode(WIFI_MODE_NULL);

        const bool v_hasSta   = (_wifi.sta_ssid[0] != '\0');
        const bool v_auto     = (_wifi.mode == (uint8_t)EN_C10_WIFI_AUTO);
        const bool v_forceAp  = (_wifi.mode == (uint8_t)EN_C10_WIFI_AP);
        const bool v_forceSta = (_wifi.mode == (uint8_t)EN_C10_WIFI_STA);

        if (v_forceAp || (!v_hasSta && (v_auto || !v_forceSta))) {
            startAp();
            return;
        }

        WiFi.mode(WIFI_STA);
        WiFi.begin(_wifi.sta_ssid, _wifi.sta_pass);

        const uint32_t v_t0 = millis();
        bool v_ok = false;
        while (millis() - v_t0 < 8000) {
            if (WiFi.status() == WL_CONNECTED) { v_ok = true; break; }
            delay(200);
        }

        if (v_ok) return;

        if (v_auto) { startAp(); return; }

        Serial.println("[W10] STA forced but connect failed.");
    }

    void startAp() {
        WiFi.mode(WIFI_AP);
        WiFi.softAP(_wifi.ap_ssid, _wifi.ap_pass);
        Serial.printf("[W10] AP started: ssid=%s ip=%s\n", _wifi.ap_ssid, WiFi.softAPIP().toString().c_str());
    }

    void startMdnsIfPossible() {
        if (_wifi.mdns_host[0] == '\0') return;
        if (WiFi.getMode() != WIFI_STA) return;
        if (WiFi.status() != WL_CONNECTED) return;
        if (_mdnsStarted) return;

        if (!MDNS.begin(_wifi.mdns_host)) {
            Serial.println("[W10] mDNS begin failed");
            _mdnsStarted = false;
            return;
        }
        MDNS.addService("http", "tcp", 80);
        _mdnsStarted = true;
        Serial.printf("[W10] mDNS: http://%s.local/\n", _wifi.mdns_host);
    }

    void stopMdnsIfRunning() {
        if (!_mdnsStarted) return;
        MDNS.end();
        _mdnsStarted = false;
        Serial.println("[W10] mDNS stopped");
    }

    // -------------------------
    // Asset serving (gzip)
    // -------------------------
    bool clientAcceptsGzip(AsyncWebServerRequest* p_req) {
        if (!p_req->hasHeader("Accept-Encoding")) return false;
        const String v_ae = p_req->header("Accept-Encoding");
        return (v_ae.indexOf("gzip") >= 0);
    }

    void serveAsset(AsyncWebServerRequest* p_req, const ST_W10_Asset_t& p_a) {
        bool v_useGz = false;
        const char* v_path = p_a.fs_path_plain;

        if (p_a.fs_path_gz != nullptr && LittleFS.exists(p_a.fs_path_gz) && clientAcceptsGzip(p_req)) {
            v_useGz = true;
            v_path = p_a.fs_path_gz;
        } else if (!LittleFS.exists(p_a.fs_path_plain)) {
            p_req->send(404, "text/plain", "asset not found");
            return;
        }

        AsyncWebServerResponse* v_res = p_req->beginResponse(LittleFS, v_path, p_a.content_type);
        if (v_useGz) v_res->addHeader("Content-Encoding", "gzip");

        if (p_a.cache_immutable) v_res->addHeader("Cache-Control", "public, max-age=31536000, immutable");
        else v_res->addHeader("Cache-Control", "no-store");

        p_req->send(v_res);
    }

    // -------------------------
    // API helpers (async body)
    // -------------------------
    void sendJson(AsyncWebServerRequest* p_req, JsonDocument& p_doc) {
        String v_out;
        serializeJson(p_doc, v_out);
        p_req->send(200, "application/json", v_out);
    }

    String* getOrCreateReqBody(AsyncWebServerRequest* p_req, size_t p_index) {
        if (p_index == 0) {
            if (p_req->_tempObject != nullptr) {
                delete (String*)p_req->_tempObject;
                p_req->_tempObject = nullptr;
            }
            p_req->_tempObject = new String();
        }
        return (String*)p_req->_tempObject;
    }

    void finalizeReqBody(AsyncWebServerRequest* p_req) {
        if (p_req->_tempObject != nullptr) {
            delete (String*)p_req->_tempObject;
            p_req->_tempObject = nullptr;
        }
    }

    // -------------------------
    // /api/config GET
    // -------------------------
    void apiGetConfig(AsyncWebServerRequest* p_req) {
        (void)_cfg->loadAll(_wifi, _e10);

        JsonDocument v_doc;
        v_doc["ver"] = (uint16_t)C10_CFG_VER;

        JsonObject v_w = v_doc["wifi"].to<JsonObject>();
        v_w["mode"] = _wifi.mode;

        JsonObject v_sta = v_w["sta"].to<JsonObject>();
        v_sta["ssid"] = _wifi.sta_ssid;
        v_sta["pass"] = _wifi.sta_pass;

        JsonObject v_ap = v_w["ap"].to<JsonObject>();
        v_ap["ssid"] = _wifi.ap_ssid;
        v_ap["pass"] = _wifi.ap_pass;

        JsonObject v_md = v_w["mdns"].to<JsonObject>();
        v_md["host"] = _wifi.mdns_host;

        JsonObject v_e = v_doc["e10"].to<JsonObject>();
        v_e["dpi_level"] = _e10.dpi_level;
        v_e["hard_click_lock"] = _e10.hard_click_lock;

        JsonArray v_sb = v_e["scale_base"].to<JsonArray>();
        v_sb.add(_e10.scale_base[0]); v_sb.add(_e10.scale_base[1]); v_sb.add(_e10.scale_base[2]);

        JsonArray v_ag = v_e["accel_gain"].to<JsonArray>();
        v_ag.add(_e10.accel_gain[0]); v_ag.add(_e10.accel_gain[1]); v_ag.add(_e10.accel_gain[2]);

        v_e["accel_threshold"] = _e10.accel_threshold;

        JsonObject v_wh = v_e["wheel"].to<JsonObject>();
        v_wh["threshold_deg"] = _e10.wheel_threshold_deg;
        v_wh["step_max"] = _e10.wheel_step_max;

        JsonObject v_g = v_e["gesture"].to<JsonObject>();
        v_g["flick_deg"] = _e10.gesture_flick_deg;
        v_g["cooldown_ms"] = _e10.gesture_cooldown_ms;

        v_e["scroll_cursor_damp"] = _e10.scroll_cursor_damp;

        JsonObject v_d = v_e["drift"].to<JsonObject>();
        v_d["idle_gyro_th_deg"] = _e10.idle_gyro_th_deg;
        v_d["idle_hold_ms"] = _e10.idle_hold_ms;
        v_d["bias_track_alpha"] = _e10.bias_track_alpha;
        v_d["zero_snap_th"] = _e10.zero_snap_th;

        JsonObject v_pk = v_e["ppt_keys"].to<JsonObject>();
        auto fill = [&](const char* n, const ST_C10_PptKey_t& k){
            JsonObject o = v_pk[n].to<JsonObject>();
            o["mod"] = k.mod; o["key"] = k.key;
        };
        fill("start", _e10.ppt_start);
        fill("exit",  _e10.ppt_exit);
        fill("next",  _e10.ppt_next);
        fill("prev",  _e10.ppt_prev);
        fill("black", _e10.ppt_black);
        fill("laser", _e10.ppt_laser);

        sendJson(p_req, v_doc);
    }

    // -------------------------
    // /api/config POST (save + apply)
    // -------------------------
    void apiPostConfig(AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total) {
        String* v_body = getOrCreateReqBody(p_req, p_index);
        if (v_body == nullptr) { p_req->send(500, "application/json", "{\"ok\":false}"); return; }

        for (size_t i = 0; i < p_len; i++) (*v_body) += (char)p_data[i];
        if (p_index + p_len < p_total) return;

        _cfg->makeDefaultsWiFi(_wifi);
        _cfg->makeDefaultsE10(_e10);

        (void)_cfg->patchFromJsonWiFi(*v_body, _wifi);
        (void)_cfg->patchFromJsonE10(*v_body, _e10);

        finalizeReqBody(p_req);

        bool v_ok = _cfg->saveAll(_wifi, _e10);
        bool v_applied = false;
        if (v_ok && _applyFn != nullptr) v_applied = _applyFn(_applyCtx);

        JsonDocument v_doc;
        v_doc["ok"] = v_ok;
        v_doc["applied"] = v_applied;
        v_doc["note"] = "WiFi changes require reboot to apply.";
        sendJson(p_req, v_doc);
    }

    // -------------------------
    // /api/keycodes
    // -------------------------
    void apiKeycodes(AsyncWebServerRequest* p_req) {
        JsonDocument v_doc;

        JsonArray v_mods = v_doc["mods"].to<JsonArray>();
        for (size_t i=0; i<sizeof(s_mods)/sizeof(s_mods[0]); i++) {
            JsonObject o = v_mods.add<JsonObject>();
            o["name"] = s_mods[i].name;
            o["mask"] = s_mods[i].mask;
        }

        JsonArray v_keys = v_doc["keys"].to<JsonArray>();
        for (size_t i=0; i<sizeof(s_keys)/sizeof(s_keys[0]); i++) {
            JsonObject o = v_keys.add<JsonObject>();
            o["name"] = s_keys[i].name;
            o["code"] = (uint16_t)s_keys[i].usage;
        }

        sendJson(p_req, v_doc);
    }

    // -------------------------
    // /api/status
    // -------------------------
    void apiStatus(AsyncWebServerRequest* p_req) {
        JsonDocument v_doc;

        v_doc["uptime_ms"] = (uint32_t)millis();
        v_doc["heap_free"] = (uint32_t)ESP.getFreeHeap();

        JsonObject v_net = v_doc["net"].to<JsonObject>();
        v_net["mode"] = (WiFi.getMode() == WIFI_AP) ? "AP" : "STA";
        v_net["ip"] = (WiFi.getMode() == WIFI_AP) ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
        v_net["ssid"] = (WiFi.getMode() == WIFI_AP) ? String(_wifi.ap_ssid) : WiFi.SSID();
        v_net["mdns"] = String(_wifi.mdns_host) + ".local";

        CL_E10_EliteAirMouse* v_e10 = (CL_E10_EliteAirMouse*)_applyCtx;
        if (v_e10 != nullptr) {
            ST_E10_Status_t v_s;
            v_e10->getStatus(v_s);

            JsonObject v_e = v_doc["e10"].to<JsonObject>();
            v_e["ble_connected"] = v_s.ble_connected;
            v_e["ppt_mode"] = v_s.ppt_mode;
            v_e["dpi_level"] = v_s.dpi_level;
            v_e["degraded"] = v_s.degraded;

            v_e["rssi"] = v_s.rssi;
            v_e["reconnect_count"] = v_s.reconnect_count;

            JsonObject v_gyro = v_e["gyro"].to<JsonObject>();
            v_gyro["calib_done"] = v_s.gyro_calib_done;
            v_gyro["bias_x"] = v_s.gyro_bias_x;
            v_gyro["bias_y"] = v_s.gyro_bias_y;
            v_gyro["bias_z"] = v_s.gyro_bias_z;

            v_e["temp_c"] = v_s.temp_c;

            JsonObject v_t = v_e["timing"].to<JsonObject>();
            v_t["sampling_ms_target"] = v_s.sampling_ms_target;
            v_t["sampling_ms_avg"] = v_s.sampling_ms_avg;

            JsonObject v_err = v_e["err"].to<JsonObject>();
            v_err["mpu_read"] = v_s.err_mpu_read;
            v_err["mpu_recover"] = v_s.err_mpu_recover;
            v_err["task_overrun"] = v_s.err_task_overrun;

            JsonObject v_st = v_e["stack"].to<JsonObject>();
            v_st["sensor_min_words"] = v_s.stack_sensor_min_words;
            v_st["comm_min_words"] = v_s.stack_comm_min_words;
        }

        sendJson(p_req, v_doc);
    }

    // -------------------------
    // /api/control (immediate)
    // body examples:
    //  {"ppt_mode":true}
    //  {"dpi_level":3}
    //  {"hard_click_lock":false}
    //  {"tuning":{"accel_threshold":8.0,"scroll_cursor_damp":0.25,"zero_snap_th":0.6}}
    // -------------------------
    void apiControl(AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total) {
        String* v_body = getOrCreateReqBody(p_req, p_index);
        if (v_body == nullptr) { p_req->send(500, "application/json", "{\"ok\":false}"); return; }

        for (size_t i=0; i<p_len; i++) (*v_body) += (char)p_data[i];
        if (p_index + p_len < p_total) return;

        JsonDocument v_doc;
        DeserializationError v_err = deserializeJson(v_doc, *v_body);
        finalizeReqBody(p_req);

        if (v_err) {
            p_req->send(400, "application/json", "{\"ok\":false,\"err\":\"bad_json\"}");
            return;
        }

        CL_E10_EliteAirMouse* v_e10 = (CL_E10_EliteAirMouse*)_applyCtx;
        bool v_ok = true;

        if (v_e10 != nullptr) {
            if (!v_doc["ppt_mode"].isNull()) v_ok = v_ok && v_e10->setPptMode((bool)v_doc["ppt_mode"]);
            if (!v_doc["dpi_level"].isNull()) v_ok = v_ok && v_e10->setDpiLevel((uint8_t)v_doc["dpi_level"]);
            if (!v_doc["hard_click_lock"].isNull()) v_ok = v_ok && v_e10->setHardClickLock((bool)v_doc["hard_click_lock"]);

            if (!v_doc["tuning"].isNull()) {
                JsonVariant t = v_doc["tuning"];
                float a = 0.0f, d = 0.0f, z = 0.0f;
                bool ha = false, hd = false, hz = false;

                if (!t["accel_threshold"].isNull()) { a = (float)t["accel_threshold"]; ha = true; }
                if (!t["scroll_cursor_damp"].isNull()) { d = (float)t["scroll_cursor_damp"]; hd = true; }
                if (!t["zero_snap_th"].isNull()) { z = (float)t["zero_snap_th"]; hz = true; }

                // partial allowed: missing fields keep current by passing -1 sentinel
                if (!ha) a = -1.0f;
                if (!hd) d = -1.0f;
                if (!hz) z = -1.0f;

                // 간단 구현: -1이면 현재값 유지하도록 E10 내부에서 처리하는 대신,
                // 여기서는 "부분만 적용"을 위해 현재 cfg를 다시 읽지 않고,
                // UI는 보통 3개 다 보냄을 전제로 한다.
                v_ok = v_ok && v_e10->setTuning(ha ? a : 8.0f, hd ? d : 0.25f, hz ? z : 0.6f);
            }
        } else v_ok = false;

        JsonDocument v_out;
        v_out["ok"] = v_ok;
        sendJson(p_req, v_out);
    }

    // -------------------------
    // /api/reset
    // -------------------------
    void apiReset(AsyncWebServerRequest* p_req) {
        _cfg->makeDefaultsWiFi(_wifi);
        _cfg->makeDefaultsE10(_e10);

        bool v_ok = _cfg->saveAll(_wifi, _e10);
        bool v_applied = false;
        if (v_ok && _applyFn != nullptr) v_applied = _applyFn(_applyCtx);

        JsonDocument v_doc;
        v_doc["ok"] = v_ok;
        v_doc["applied"] = v_applied;
        v_doc["note"] = "WiFi defaults saved. Reboot to apply WiFi.";
        sendJson(p_req, v_doc);
    }

    // -------------------------
    // /api/reboot
    // -------------------------
    void apiReboot(AsyncWebServerRequest* p_req) {
        JsonDocument v_doc;
        v_doc["ok"] = true;
        sendJson(p_req, v_doc);
        delay(50);
        ESP.restart();
    }
};

void CL_W10_WebConfig::s_handleWifiEvent(WiFiEvent_t p_event, WiFiEventInfo_t p_info) {
    (void)p_info;
    if (s_instance) s_instance->_handleWifi(p_event);
}

void CL_W10_WebConfig::_handleWifi(WiFiEvent_t p_event) {
    switch (p_event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.println("[W10] WiFi Connected (STA)");
            startMdnsIfPossible();
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.println("[W10] WiFi Disconnected");
            stopMdnsIfRunning();
            break;

        default:
            break;
    }
}

CL_W10_WebConfig* CL_W10_WebConfig::s_instance = nullptr;

