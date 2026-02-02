// =======================================================
// File: src/v022/W10_WebConfig_022.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : W10_WebConfig_022.h
 * 모듈약어 : W10
 * 모듈명 : Web Config Server (P0/P1/P2 UI: Export/Import/Rollback/OTA, Status+Keycodes, STA+mDNS)
 * ------------------------------------------------------
 * 기능 요약
 *  - 정적 리소스(.gz 자동) + 캐시 정책(immutable / no-store)
 *  - /api/keycodes: HID usage id 기반 + mods(mask) 제공
 *  - /api/status: 현장 판단용(health/score/noiseRMS/i2c recover/err hist)
 *  - P0/P1: /api/config export/import + /api/config/rollback
 *  - P1: OTA(Web) 업로드 /api/ota (progress + 결과)
 *  - STA 모드 + mDNS (예: elite-airmouse.local)
 *  - 웹에서 PPT 모드 / DPI / Precision 모드 제어 (/api/control)
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
 *
 * [W10 mods 정책 (짧게)]
 *  - /api/keycodes mods[].mask 는 HID KeyboardInputReport.modifiers 바이트와 1:1 매칭
 *    LCtrl=0x01, LShift=0x02, LAlt=0x04, LMeta=0x08, RCtrl=0x10, RShift=0x20, RAlt=0x40, RMeta=0x80
 *  - E10은 이 mask를 modifierKeyPress/Release(mask)로 전송 -> OS 호환 안정(정책 확정)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>

#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include <ESPAsyncWebServer.h>
#include <Update.h>

#include "C10_Config_022.h"
#include "E10_EliteAirMouse_022.h"

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
        { "/",                  "/www/index_022.html", "/www/index_022.html.gz", "text/html", false },
        { "/www/",              "/www/index_022.html", "/www/index_022.html.gz", "text/html", false },

        { "/www/style_022.css", "/www/style_022.css",  "/www/style_022.css.gz",  "text/css", true  },
        { "/www/app_022.js",    "/www/app_022.js",     "/www/app_022.js.gz",     "application/javascript", true  },
    };

    static constexpr const char* G_W10_STYLE_URI = "/www/style_022.css";
    static constexpr const char* G_W10_APP_URI   = "/www/app_022.js";

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

    // (예시) 필요한 만큼 계속 확장 가능
    static constexpr ST_W10_Key_t s_keys[] = {
        { "None", 0 },

        { "A", 0x04 },{ "B", 0x05 },{ "C", 0x06 },{ "D", 0x07 },{ "E", 0x08 },{ "F", 0x09 },
        { "G", 0x0A },{ "H", 0x0B },{ "I", 0x0C },{ "J", 0x0D },{ "K", 0x0E },{ "L", 0x0F },
        { "M", 0x10 },{ "N", 0x11 },{ "O", 0x12 },{ "P", 0x13 },{ "Q", 0x14 },{ "R", 0x15 },
        { "S", 0x16 },{ "T", 0x17 },{ "U", 0x18 },{ "V", 0x19 },{ "W", 0x1A },{ "X", 0x1B },
        { "Y", 0x1C },{ "Z", 0x1D },

        { "1", 0x1E },{ "2", 0x1F },{ "3", 0x20 },{ "4", 0x21 },{ "5", 0x22 },
        { "6", 0x23 },{ "7", 0x24 },{ "8", 0x25 },{ "9", 0x26 },{ "0", 0x27 },

        { "Enter", 0x28 },{ "Esc", 0x29 },{ "Backspace", 0x2A },{ "Tab", 0x2B },{ "Space", 0x2C },

        { "-", 0x2D },{ "=", 0x2E },{ "[", 0x2F },{ "]", 0x30 },{ "\\", 0x31 },
        { ";", 0x33 },{ "'", 0x34 },{ "`", 0x35 },{ ",", 0x36 },{ ".", 0x37 },{ "/", 0x38 },

        { "F1", 0x3A },{ "F2", 0x3B },{ "F3", 0x3C },{ "F4", 0x3D },{ "F5", 0x3E },{ "F6", 0x3F },
        { "F7", 0x40 },{ "F8", 0x41 },{ "F9", 0x42 },{ "F10",0x43 },{ "F11",0x44 },{ "F12",0x45 },

        { "Insert", 0x49 },{ "Home", 0x4A },{ "PageUp", 0x4B },{ "Delete", 0x4C },{ "End", 0x4D },{ "PageDown", 0x4E },
        { "Right", 0x4F },{ "Left", 0x50 },{ "Down", 0x51 },{ "Up", 0x52 },
    };

    bool _mdnsStarted = false;

    static void s_handleWifiEvent(WiFiEvent_t p_event, WiFiEventInfo_t p_info);
    void _handleWifi(WiFiEvent_t p_event);
    static CL_W10_WebConfig* s_instance;

    // OTA runtime
    volatile bool _otaInProgress = false;
    volatile uint32_t _otaTotal = 0;
    volatile uint32_t _otaWritten = 0;
    volatile bool _otaOk = false;
    char _otaErr[64];

  public:
    CL_W10_WebConfig() : _svr(80) {
        s_instance = this;
        memset(&_wifi, 0, sizeof(_wifi));
        memset(&_e10,  0, sizeof(_e10));
        memset(_otaErr, 0, sizeof(_otaErr));
        strlcpy(_otaErr, "none", sizeof(_otaErr));
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

        // basic redirects
        _svr.on("/www/style.css", HTTP_GET, [](AsyncWebServerRequest* p_req){ p_req->redirect(G_W10_STYLE_URI); });
        _svr.on("/www/app.js", HTTP_GET, [](AsyncWebServerRequest* p_req){ p_req->redirect(G_W10_APP_URI); });

        // APIs
        _svr.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* p_req){ this->apiGetConfig(p_req); });

        _svr.on("/api/config", HTTP_POST,
            [this](AsyncWebServerRequest* p_req){ (void)p_req; },
            nullptr,
            [this](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total){
                this->apiPostConfig(p_req, p_data, p_len, p_index, p_total);
            }
        );

        // export/import/rollback
        _svr.on("/api/config/export", HTTP_GET, [this](AsyncWebServerRequest* p_req){ this->apiExportConfig(p_req); });

        _svr.on("/api/config/import", HTTP_POST,
            [this](AsyncWebServerRequest* p_req){ (void)p_req; },
            nullptr,
            [this](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total){
                this->apiImportConfig(p_req, p_data, p_len, p_index, p_total);
            }
        );

        _svr.on("/api/config/rollback", HTTP_POST,
            [this](AsyncWebServerRequest* p_req){ (void)p_req; },
            nullptr,
            [this](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total){
                (void)p_data; (void)p_len; (void)p_index; (void)p_total;
                this->apiRollbackConfig(p_req);
            }
        );

        _svr.on("/api/keycodes", HTTP_GET, [this](AsyncWebServerRequest* p_req){ this->apiKeycodes(p_req); });
        _svr.on("/api/status",   HTTP_GET, [this](AsyncWebServerRequest* p_req){ this->apiStatus(p_req); });

        // control: ppt_mode / dpi_level / precision_mode
        _svr.on("/api/control", HTTP_POST,
            [this](AsyncWebServerRequest* p_req){ (void)p_req; },
            nullptr,
            [this](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total){
                this->apiControl(p_req, p_data, p_len, p_index, p_total);
            }
        );

        // OTA
        _svr.on("/api/ota", HTTP_POST,
            [this](AsyncWebServerRequest* p_req){
                // finish callback
                JsonDocument v_doc;
                v_doc["ok"] = _otaOk;
                v_doc["err"] = _otaErr;
                v_doc["written"] = (uint32_t)_otaWritten;
                v_doc["total"] = (uint32_t)_otaTotal;
                String v_out; serializeJson(v_doc, v_out);

                p_req->send(_otaOk ? 200 : 500, "application/json", v_out);

                // ✅ 성공이면 리부트 (브릭 방지: 실패 시 리부트 금지)
                if (_otaOk) {
                    delay(200);
                    ESP.restart();
                }
            },
            [this](AsyncWebServerRequest* p_req, const String& p_filename, size_t p_index, uint8_t* p_data, size_t p_len, bool p_final){
                this->apiOtaUpload(p_req, p_filename, p_index, p_data, p_len, p_final);
            }
        );

        _svr.on("/api/ota/status", HTTP_GET, [this](AsyncWebServerRequest* p_req){ this->apiOtaStatus(p_req); });

        _svr.on("/api/reboot", HTTP_POST,
            [this](AsyncWebServerRequest* p_req){ (void)p_req; },
            nullptr,
            [this](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total){
                (void)p_data; (void)p_len; (void)p_index; (void)p_total;
                JsonDocument v_doc; v_doc["ok"]=true; sendJson(p_req, v_doc);
                delay(50); ESP.restart();
            }
        );

        _svr.onNotFound([](AsyncWebServerRequest* p_req){
            p_req->send(404, "text/plain", "not found");
        });

        _svr.begin();

        Serial.printf("[W10] WebConfig started. mode=%s, ip=%s\n",
                      WiFi.getMode() == WIFI_AP ? "AP" : "STA",
                      (WiFi.getMode() == WIFI_AP ? WiFi.softAPIP() : WiFi.localIP()).toString().c_str());
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
    // JSON helpers
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
    // /api/config (GET/POST)
    // -------------------------
    void apiGetConfig(AsyncWebServerRequest* p_req) {
        (void)_cfg->loadAll(_wifi, _e10);

        JsonDocument v_doc;
        v_doc["ver"] = (uint16_t)G_C10_CFG_VER;

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

        JsonObject v_p = v_e["precision"].to<JsonObject>();
        v_p["enable"] = _e10.precision_enable;
        v_p["deadzone"] = _e10.precision_deadzone;
        v_p["gain"] = _e10.precision_gain;
        v_p["accel"] = _e10.precision_accel;
        v_p["max_step"] = _e10.precision_max_step;
        v_p["smooth"] = _e10.precision_smooth;

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
    // Export/Import/Rollback (P1 + P0 safe)
    // -------------------------
    void apiExportConfig(AsyncWebServerRequest* p_req) {
        String v_json;
        bool v_ok = _cfg->exportJson(v_json);

        if (!v_ok) {
            p_req->send(500, "application/json", "{\"ok\":false}");
            return;
        }

        AsyncWebServerResponse* v_res = p_req->beginResponse(200, "application/json", v_json);
        v_res->addHeader("Content-Disposition", "attachment; filename=\"config.json\"");
        v_res->addHeader("Cache-Control", "no-store");
        p_req->send(v_res);
    }

    void apiImportConfig(AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total) {
        String* v_body = getOrCreateReqBody(p_req, p_index);
        if (v_body == nullptr) { p_req->send(500, "application/json", "{\"ok\":false}"); return; }

        for (size_t i=0; i<p_len; i++) (*v_body) += (char)p_data[i];
        if (p_index + p_len < p_total) return;

        bool v_saved = false;
        bool v_applied = false;
        bool v_ok = _cfg->importJson(*v_body, v_saved, v_applied);

        finalizeReqBody(p_req);

        if (v_ok && v_saved && _applyFn != nullptr) v_applied = _applyFn(_applyCtx);

        JsonDocument v_doc;
        v_doc["ok"] = v_ok;
        v_doc["saved"] = v_saved;
        v_doc["applied"] = v_applied;
        v_doc["note"] = "WiFi changes require reboot to apply.";
        sendJson(p_req, v_doc);
    }

    void apiRollbackConfig(AsyncWebServerRequest* p_req) {
        bool v_ok = _cfg->rollbackFromBak();
        bool v_applied = false;
        if (v_ok && _applyFn != nullptr) v_applied = _applyFn(_applyCtx);

        JsonDocument v_doc;
        v_doc["ok"] = v_ok;
        v_doc["applied"] = v_applied;
        v_doc["note"] = "Rollback applied. WiFi changes require reboot.";
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
    // /api/status (P1)
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
            v_e["precision_mode"] = v_s.precision_mode;

            JsonObject v_h = v_e["health"].to<JsonObject>();
            v_h["state"] = v_s.health;         // 0/1/2
            v_h["score"] = v_s.health_score;   // 0~1000

            JsonObject v_gyro = v_e["gyro"].to<JsonObject>();
            v_gyro["bias_x"] = v_s.gyro_bias_x;
            v_gyro["bias_y"] = v_s.gyro_bias_y;
            v_gyro["bias_z"] = v_s.gyro_bias_z;
            v_gyro["rms"] = v_s.gyro_rms;

            v_e["cursor_rms"] = v_s.cursor_rms;
            v_e["temp_c"] = v_s.temp_c;

            JsonObject v_t = v_e["timing"].to<JsonObject>();
            v_t["sampling_ms_target"] = v_s.sampling_ms_target;
            v_t["sampling_ms_avg"] = v_s.sampling_ms_avg;

            JsonObject v_i2c = v_e["i2c"].to<JsonObject>();
            v_i2c["recover_count"] = v_s.i2c_recover_count;
            v_i2c["recover_last_ok"] = v_s.i2c_recover_last_ok;

            JsonObject v_err = v_e["err"].to<JsonObject>();
            v_err["mpu_nan"] = v_s.err_mpu_nan;
            v_err["mutex_miss"] = v_s.err_mutex_miss;
            v_err["task_overrun"] = v_s.err_task_overrun;

            JsonArray v_hist = v_e["err_hist"].to<JsonArray>();
            for (uint8_t i=0; i<v_s.err_hist_n; i++) {
                JsonObject o = v_hist.add<JsonObject>();
                o["ts_ms"] = v_s.err_hist[i].ts_ms;
                o["code"] = v_s.err_hist[i].code;
                o["value"] = v_s.err_hist[i].value;
            }
        }

        // OTA status
        JsonObject v_ota = v_doc["ota"].to<JsonObject>();
        v_ota["in_progress"] = _otaInProgress;
        v_ota["total"] = (uint32_t)_otaTotal;
        v_ota["written"] = (uint32_t)_otaWritten;
        v_ota["ok"] = _otaOk;
        v_ota["err"] = _otaErr;

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

        if (v_err) {
            p_req->send(400, "application/json", "{\"ok\":false,\"err\":\"bad_json\"}");
            return;
        }

        CL_E10_EliteAirMouse* v_e10 = (CL_E10_EliteAirMouse*)_applyCtx;
        bool v_ok = true;

        if (v_e10 != nullptr) {
            if (!v_doc["ppt_mode"].isNull()) v_ok = v_ok && v_e10->setPptMode((bool)v_doc["ppt_mode"]);
            if (!v_doc["dpi_level"].isNull()) v_ok = v_ok && v_e10->setDpiLevel((uint8_t)v_doc["dpi_level"]);
            if (!v_doc["precision_mode"].isNull()) v_ok = v_ok && v_e10->setPrecisionMode((bool)v_doc["precision_mode"]);
        } else v_ok = false;

        JsonDocument v_out;
        v_out["ok"] = v_ok;
        sendJson(p_req, v_out);
    }

    // -------------------------
    // OTA upload (P0)
    // -------------------------
    void apiOtaUpload(AsyncWebServerRequest* p_req, const String& p_filename, size_t p_index, uint8_t* p_data, size_t p_len, bool p_final) {
        (void)p_req;

        if (p_index == 0) {
            _otaInProgress = true;
            _otaWritten = 0;
            _otaTotal = (uint32_t)p_req->contentLength();
            _otaOk = false;
            memset(_otaErr, 0, sizeof(_otaErr));
            strlcpy(_otaErr, "in_progress", sizeof(_otaErr));

            // Begin OTA
            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                strlcpy(_otaErr, "Update.begin failed", sizeof(_otaErr));
                _otaInProgress = false;
                return;
            }
        }

        if (p_len) {
            size_t v_w = Update.write(p_data, p_len);
            _otaWritten += (uint32_t)v_w;
            if (v_w != p_len) {
                strlcpy(_otaErr, "Update.write mismatch", sizeof(_otaErr));
            }
        }

        if (p_final) {
            if (!Update.end(true)) {
                strlcpy(_otaErr, "Update.end failed", sizeof(_otaErr));
                _otaOk = false;
            } else if (Update.hasError()) {
                strlcpy(_otaErr, "Update.hasError", sizeof(_otaErr));
                _otaOk = false;
            } else {
                _otaOk = true;
                strlcpy(_otaErr, "ok", sizeof(_otaErr));
            }
            _otaInProgress = false;
        }
    }

    void apiOtaStatus(AsyncWebServerRequest* p_req) {
        JsonDocument v_doc;
        v_doc["in_progress"] = _otaInProgress;
        v_doc["total"] = (uint32_t)_otaTotal;
        v_doc["written"] = (uint32_t)_otaWritten;
        v_doc["ok"] = _otaOk;
        v_doc["err"] = _otaErr;
        sendJson(p_req, v_doc);
    }
};

// WiFi event bridge
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
