#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : W10_WebConfig_019.h
 * 모듈약어 : W10
 * 모듈명 : Web Config Server (Gzip Assets, Keycodes(Search), Status, STA+mDNS) v0.1.9
 * ------------------------------------------------------
 * 기능 요약
 *  - 정적 리소스(.gz 자동) + 캐시 정책(immutable/ no-store)
 *  - /api/keycodes: HID Usage ID 기반(Keyboard 0x07 + Consumer 0x0C) 대량 제공
 *  - /api/status: BLE/IP/heap/uptime/DPI/PPT + gyro bias/temp/dt/에러 카운터
 *  - STA 모드 + mDNS (예: elite-airmouse.local)
 *  - 웹에서 PPT 모드 토글 / DPI 즉시 변경
 *  - [P0] /api/config 저장 응답에 reboot 필요 플래그/메시지 정리
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

#include "C10_Config_017.h"
#include "E10_EliteAirMouse_019.h"

class CL_W10_WebConfig {
  private:
    AsyncWebServer _svr;

    CL_C10_Config* _cfg = nullptr;
    T_E10_ApplyFn  _applyFn = nullptr;
    void*          _applyCtx = nullptr;

    ST_C10_WiFiConfig_t _wifi;
    ST_C10_E10Config_t  _e10;

    // ---------- Assets ----------
    struct ST_W10_Asset_t {
        const char* uri;
        const char* fs_path_plain;
        const char* fs_path_gz;
        const char* content_type;
        bool        cache_immutable;
    };

    static constexpr ST_W10_Asset_t s_assets[] = {
        { "/",                   "/www/index_019.html", "/www/index_019.html.gz", "text/html", false },
        { "/www/",               "/www/index_019.html", "/www/index_019.html.gz", "text/html", false },
        { "/www/style_019.css",  "/www/style_019.css",  "/www/style_019.css.gz",  "text/css", true  },
        { "/www/app_019.js",     "/www/app_019.js",     "/www/app_019.js.gz",     "application/javascript", true  },
    };

    static constexpr const char* G_W10_LEGACY_STYLE_URI = "/www/style.css";
    static constexpr const char* G_W10_LEGACY_APP_URI   = "/www/app.js";
    static constexpr const char* G_W10_STYLE_URI        = "/www/style_019.css";
    static constexpr const char* G_W10_APP_URI          = "/www/app_019.js";

    // ---------- Key tables ----------
    // W10 mods 정책(짧게):
    // - mods[].mask는 HID "modifier byte(bitfield)" 의미(LCtrl=0x01, LShift=0x02 ...)
    // - E10은 KeyboardDevice::modifierKeyPress/Release로 modifier byte에 직접 반영(가장 호환 안정)
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

    // keys: page + usage
    struct ST_W10_Key_t { const char* name; uint8_t page; uint16_t usage; const char* group; };

    static constexpr uint8_t G_W10_PAGE_KB = 0x07;
    static constexpr uint8_t G_W10_PAGE_CONSUMER = 0x0C;

    // Keyboard/Keypad page(0x07) 확장(대표군)
    static constexpr ST_W10_Key_t s_keys[] = {
        { "None", G_W10_PAGE_KB, 0x00, "misc" },

        // Letters
        { "A", G_W10_PAGE_KB, 0x04, "letters" },{ "B", G_W10_PAGE_KB, 0x05, "letters" },
        { "C", G_W10_PAGE_KB, 0x06, "letters" },{ "D", G_W10_PAGE_KB, 0x07, "letters" },
        { "E", G_W10_PAGE_KB, 0x08, "letters" },{ "F", G_W10_PAGE_KB, 0x09, "letters" },
        { "G", G_W10_PAGE_KB, 0x0A, "letters" },{ "H", G_W10_PAGE_KB, 0x0B, "letters" },
        { "I", G_W10_PAGE_KB, 0x0C, "letters" },{ "J", G_W10_PAGE_KB, 0x0D, "letters" },
        { "K", G_W10_PAGE_KB, 0x0E, "letters" },{ "L", G_W10_PAGE_KB, 0x0F, "letters" },
        { "M", G_W10_PAGE_KB, 0x10, "letters" },{ "N", G_W10_PAGE_KB, 0x11, "letters" },
        { "O", G_W10_PAGE_KB, 0x12, "letters" },{ "P", G_W10_PAGE_KB, 0x13, "letters" },
        { "Q", G_W10_PAGE_KB, 0x14, "letters" },{ "R", G_W10_PAGE_KB, 0x15, "letters" },
        { "S", G_W10_PAGE_KB, 0x16, "letters" },{ "T", G_W10_PAGE_KB, 0x17, "letters" },
        { "U", G_W10_PAGE_KB, 0x18, "letters" },{ "V", G_W10_PAGE_KB, 0x19, "letters" },
        { "W", G_W10_PAGE_KB, 0x1A, "letters" },{ "X", G_W10_PAGE_KB, 0x1B, "letters" },
        { "Y", G_W10_PAGE_KB, 0x1C, "letters" },{ "Z", G_W10_PAGE_KB, 0x1D, "letters" },

        // Numbers
        { "1", G_W10_PAGE_KB, 0x1E, "numbers" },{ "2", G_W10_PAGE_KB, 0x1F, "numbers" },
        { "3", G_W10_PAGE_KB, 0x20, "numbers" },{ "4", G_W10_PAGE_KB, 0x21, "numbers" },
        { "5", G_W10_PAGE_KB, 0x22, "numbers" },{ "6", G_W10_PAGE_KB, 0x23, "numbers" },
        { "7", G_W10_PAGE_KB, 0x24, "numbers" },{ "8", G_W10_PAGE_KB, 0x25, "numbers" },
        { "9", G_W10_PAGE_KB, 0x26, "numbers" },{ "0", G_W10_PAGE_KB, 0x27, "numbers" },

        // Control
        { "Enter", G_W10_PAGE_KB, 0x28, "control" },{ "Esc", G_W10_PAGE_KB, 0x29, "control" },
        { "Backspace", G_W10_PAGE_KB, 0x2A, "control" },{ "Tab", G_W10_PAGE_KB, 0x2B, "control" },
        { "Space", G_W10_PAGE_KB, 0x2C, "control" },

        // Symbols
        { "-", G_W10_PAGE_KB, 0x2D, "symbols" },{ "=", G_W10_PAGE_KB, 0x2E, "symbols" },
        { "[", G_W10_PAGE_KB, 0x2F, "symbols" },{ "]", G_W10_PAGE_KB, 0x30, "symbols" },
        { "\\", G_W10_PAGE_KB, 0x31, "symbols" },{ ";", G_W10_PAGE_KB, 0x33, "symbols" },
        { "'", G_W10_PAGE_KB, 0x34, "symbols" },{ "`", G_W10_PAGE_KB, 0x35, "symbols" },
        { ",", G_W10_PAGE_KB, 0x36, "symbols" },{ ".", G_W10_PAGE_KB, 0x37, "symbols" },
        { "/", G_W10_PAGE_KB, 0x38, "symbols" },

        // Function keys
        { "F1", G_W10_PAGE_KB, 0x3A, "function" },{ "F2", G_W10_PAGE_KB, 0x3B, "function" },
        { "F3", G_W10_PAGE_KB, 0x3C, "function" },{ "F4", G_W10_PAGE_KB, 0x3D, "function" },
        { "F5", G_W10_PAGE_KB, 0x3E, "function" },{ "F6", G_W10_PAGE_KB, 0x3F, "function" },
        { "F7", G_W10_PAGE_KB, 0x40, "function" },{ "F8", G_W10_PAGE_KB, 0x41, "function" },
        { "F9", G_W10_PAGE_KB, 0x42, "function" },{ "F10",G_W10_PAGE_KB, 0x43, "function" },
        { "F11",G_W10_PAGE_KB, 0x44, "function" },{ "F12",G_W10_PAGE_KB, 0x45, "function" },

        // Navigation
        { "Insert", G_W10_PAGE_KB, 0x49, "nav" },{ "Home", G_W10_PAGE_KB, 0x4A, "nav" },
        { "PageUp", G_W10_PAGE_KB, 0x4B, "nav" },{ "Delete", G_W10_PAGE_KB, 0x4C, "nav" },
        { "End", G_W10_PAGE_KB, 0x4D, "nav" },{ "PageDown", G_W10_PAGE_KB, 0x4E, "nav" },
        { "Right", G_W10_PAGE_KB, 0x4F, "nav" },{ "Left", G_W10_PAGE_KB, 0x50, "nav" },
        { "Down", G_W10_PAGE_KB, 0x51, "nav" },{ "Up", G_W10_PAGE_KB, 0x52, "nav" },

        // Keypad
        { "KP_Enter", G_W10_PAGE_KB, 0x58, "keypad" },
        { "KP_1", G_W10_PAGE_KB, 0x59, "keypad" },{ "KP_2", G_W10_PAGE_KB, 0x5A, "keypad" },
        { "KP_3", G_W10_PAGE_KB, 0x5B, "keypad" },{ "KP_4", G_W10_PAGE_KB, 0x5C, "keypad" },
        { "KP_5", G_W10_PAGE_KB, 0x5D, "keypad" },{ "KP_6", G_W10_PAGE_KB, 0x5E, "keypad" },
        { "KP_7", G_W10_PAGE_KB, 0x5F, "keypad" },{ "KP_8", G_W10_PAGE_KB, 0x60, "keypad" },
        { "KP_9", G_W10_PAGE_KB, 0x61, "keypad" },{ "KP_0", G_W10_PAGE_KB, 0x62, "keypad" },
        { "KP_.", G_W10_PAGE_KB, 0x63, "keypad" },

        // Consumer page(0x0C) - UI에서 “미디어키”로 표시만(추후 기능 매핑 확장 가능)
        { "VolUp",   G_W10_PAGE_CONSUMER, 0x00E9, "consumer" },
        { "VolDown", G_W10_PAGE_CONSUMER, 0x00EA, "consumer" },
        { "Mute",    G_W10_PAGE_CONSUMER, 0x00E2, "consumer" },
        { "PlayPause", G_W10_PAGE_CONSUMER, 0x00CD, "consumer" },
        { "NextTrack", G_W10_PAGE_CONSUMER, 0x00B5, "consumer" },
        { "PrevTrack", G_W10_PAGE_CONSUMER, 0x00B6, "consumer" },
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

        for (size_t i = 0; i < (sizeof(s_assets) / sizeof(s_assets[0])); i++) {
            const ST_W10_Asset_t& v_a = s_assets[i];
            _svr.on(v_a.uri, HTTP_GET, [this, v_a](AsyncWebServerRequest* p_req){
                this->serveAsset(p_req, v_a);
            });
        }

        _svr.on(G_W10_LEGACY_STYLE_URI, HTTP_GET, [](AsyncWebServerRequest* p_req){ p_req->redirect(G_W10_STYLE_URI); });
        _svr.on(G_W10_LEGACY_APP_URI,   HTTP_GET, [](AsyncWebServerRequest* p_req){ p_req->redirect(G_W10_APP_URI); });

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

        _svr.onNotFound([](AsyncWebServerRequest* p_req){ p_req->send(404, "text/plain", "not found"); });

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

        if (v_forceAp || (!v_hasSta && (v_auto || v_forceSta == false))) {
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
    // API helpers
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
    // /api/config (GET)
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
        JsonObject v_mdns = v_w["mdns"].to<JsonObject>();
        v_mdns["host"] = _wifi.mdns_host;

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
    // /api/config (POST)
    // [P0] 저장 결과에 reboot 필요 플래그 정리
    // -------------------------
    void apiPostConfig(AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total) {
        String* v_body = getOrCreateReqBody(p_req, p_index);
        if (v_body == nullptr) { p_req->send(500, "application/json", "{\"ok\":false}"); return; }

        for (size_t i = 0; i < p_len; i++) (*v_body) += (char)p_data[i];
        if (p_index + p_len < p_total) return;

        // 이전값 보관(재부팅 필요 판단)
        ST_C10_WiFiConfig_t v_oldWifi;
        ST_C10_E10Config_t  v_oldE10;
        memset(&v_oldWifi, 0, sizeof(v_oldWifi));
        memset(&v_oldE10,  0, sizeof(v_oldE10));
        (void)_cfg->loadAll(v_oldWifi, v_oldE10);

        _cfg->makeDefaultsWiFi(_wifi);
        _cfg->makeDefaultsE10(_e10);

        (void)_cfg->patchFromJsonWiFi(*v_body, _wifi);
        (void)_cfg->patchFromJsonE10(*v_body, _e10);

        finalizeReqBody(p_req);

        bool v_ok = _cfg->saveAll(_wifi, _e10);

        // WiFi 변경은 reboot 필요(정책)
        bool v_wifiChanged = false;
        if (strcmp(v_oldWifi.sta_ssid, _wifi.sta_ssid) != 0) v_wifiChanged = true;
        if (strcmp(v_oldWifi.sta_pass, _wifi.sta_pass) != 0) v_wifiChanged = true;
        if (strcmp(v_oldWifi.ap_ssid,  _wifi.ap_ssid)  != 0) v_wifiChanged = true;
        if (strcmp(v_oldWifi.ap_pass,  _wifi.ap_pass)  != 0) v_wifiChanged = true;
        if (strcmp(v_oldWifi.mdns_host,_wifi.mdns_host)!= 0) v_wifiChanged = true;
        if (v_oldWifi.mode != _wifi.mode) v_wifiChanged = true;

        bool v_applied = false;
        if (v_ok && _applyFn != nullptr) v_applied = _applyFn(_applyCtx);

        JsonDocument v_doc;
        v_doc["ok"] = v_ok;
        v_doc["applied"] = v_applied;
        v_doc["wifi_changed"] = v_wifiChanged;
        v_doc["requires_reboot"] = v_wifiChanged; // WiFi는 reboot로만 적용
        v_doc["message"] = v_wifiChanged ? "WiFi setting changed. Press REBOOT to apply WiFi." : "Saved.";
        sendJson(p_req, v_doc);
    }

    // -------------------------
    // /api/keycodes (P1: 확장 + 검색/그룹 UI용 메타 제공)
    // -------------------------
    void apiKeycodes(AsyncWebServerRequest* p_req) {
        JsonDocument v_doc;

        JsonObject v_meta = v_doc["meta"].to<JsonObject>();
        v_meta["kb_page"] = (uint8_t)G_W10_PAGE_KB;
        v_meta["consumer_page"] = (uint8_t)G_W10_PAGE_CONSUMER;

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
            o["page"] = s_keys[i].page;
            o["usage"] = (uint16_t)s_keys[i].usage;
            o["group"] = s_keys[i].group;
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

            JsonObject v_gyro = v_e["gyro"].to<JsonObject>();
            v_gyro["bias_x"] = v_s.gyro_bias_x;
            v_gyro["bias_y"] = v_s.gyro_bias_y;
            v_gyro["bias_z"] = v_s.gyro_bias_z;

            v_e["temp_c"] = v_s.temp_c;

            JsonObject v_t = v_e["timing"].to<JsonObject>();
            v_t["sampling_ms_target"] = v_s.sampling_ms_target;
            v_t["sampling_ms_avg"] = v_s.sampling_ms_avg;

            JsonObject v_err = v_e["err"].to<JsonObject>();
            v_err["mpu_read"] = v_s.err_mpu_read;
            v_err["mutex_miss"] = v_s.err_mutex_miss;
            v_err["task_overrun"] = v_s.err_task_overrun;
        }

        sendJson(p_req, v_doc);
    }

    // -------------------------
    // /api/control
    // -------------------------
    void apiControl(AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total) {
        String* v_body = getOrCreateReqBody(p_req, p_index);
        if (v_body == nullptr) { p_req->send(500, "application/json", "{\"ok\":false}"); return; }

        for (size_t i=0; i<p_len; i++) (*v_body) += (char)p_data[i];
        if (p_index + p_len < p_total) return;

        JsonDocument v_doc;
        DeserializationError v_err = deserializeJson(v_doc, *v_body);
        finalizeReqBody(p_req);

        if (v_err) { p_req->send(400, "application/json", "{\"ok\":false,\"err\":\"bad_json\"}"); return; }

        CL_E10_EliteAirMouse* v_e10 = (CL_E10_EliteAirMouse*)_applyCtx;
        bool v_ok = true;

        if (v_e10 != nullptr) {
            if (!v_doc["ppt_mode"].isNull()) v_ok = v_ok && v_e10->setPptMode((bool)v_doc["ppt_mode"]);
            if (!v_doc["dpi_level"].isNull()) v_ok = v_ok && v_e10->setDpiLevel((uint8_t)v_doc["dpi_level"]);
        } else v_ok = false;

        JsonDocument v_out;
        v_out["ok"] = v_ok;
        sendJson(p_req, v_out);
    }

    void apiReset(AsyncWebServerRequest* p_req) {
        _cfg->makeDefaultsWiFi(_wifi);
        _cfg->makeDefaultsE10(_e10);

        bool v_ok = _cfg->saveAll(_wifi, _e10);
        bool v_applied = false;
        if (v_ok && _applyFn != nullptr) v_applied = _applyFn(_applyCtx);

        JsonDocument v_doc;
        v_doc["ok"] = v_ok;
        v_doc["applied"] = v_applied;
        v_doc["requires_reboot"] = true; // reset은 WiFi도 같이 초기화될 수 있어 reboot 권장
        v_doc["message"] = "Defaults saved. Press REBOOT to apply WiFi.";
        sendJson(p_req, v_doc);
    }

    void apiReboot(AsyncWebServerRequest* p_req) {
        JsonDocument v_doc;
        v_doc["ok"] = true;
        sendJson(p_req, v_doc);
        delay(50);
        ESP.restart();
    }
};

// WiFi 이벤트 브리지
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
