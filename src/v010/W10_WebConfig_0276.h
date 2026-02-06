// =======================================================
// File: src/v010/W10_WebConfig_0276.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : W10_WebConfig_0276.h
 * 모듈약어 : W10
 * 모듈명 : Web Config/Status/UI/OTA Server (Field Dashboard + PPT + WiFi/E10 Editor)
 * ------------------------------------------------------
 * 기능 요약
 *  - 자산(HTML/CSS/JS + svg/png/webp 원본) 서빙 + gzip(있으면) 서빙
 *  - Cache-Control 자동 분류(immutable vs no-store)
 *  - /api/status : 현장용 대시보드 데이터(health/anomaly/i2c/err/ota/boot)
 *  - /api/keycodes : kb(0x00~0xE7) + consumer presets + mods
 *  - /api/ppt, /api/ppt/test : PPT Keymap(v2) 편집/즉시 테스트
 *  - /api/config : config json read
 *  - /api/config/save : 저장+apply
 *  - /api/config/apply : apply-only(저장 없음)
 *  - /api/config/export/import/rollback : 백업/복구
 *  - /api/ota : Web OTA 업로드 + /api/ota/status
 *  - /api/safeboot : 조회/해제
 *  - /api/factory_reset : 초기화 + 재부팅
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
 *   - 클래스 private 멤버 함수/변수   : _ 접두사
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
#include <Update.h>
#include <string.h>
#include <strings.h>

#include "C10_Config_0274.h"
#include "E10_EliteAirMouse_0272.h"
#include "W10_Const_0276.h"

class CL_W10_WebConfig {
  private:
    AsyncWebServer _svr;

    CL_C10_Config* _cfg = nullptr;
    bool (*_applyFn)(void*) = nullptr;
    void* _applyCtx = nullptr;

    ST_C10_WiFiConfig_t _wifi;
    ST_C10_E10Config_t  _e10;

    bool _mdnsStarted = false;

    // (요구) inline static
    inline static CL_W10_WebConfig* s_instance = nullptr;

    // OTA state (ISR-like context에서 바뀔 수 있으므로 volatile)
    volatile bool _otaInProgress = false;
    volatile uint32_t _otaTotal = 0;
    volatile uint32_t _otaWritten = 0;
    volatile bool _otaOk = false;
    char _otaErr[64];

  public:
    CL_W10_WebConfig() : _svr(80) {
        s_instance = this;
        memset(&_wifi, 0, sizeof(_wifi));
        memset(&_e10, 0, sizeof(_e10));
        memset(_otaErr, 0, sizeof(_otaErr));
        strlcpy(_otaErr, "none", sizeof(_otaErr));
    }

    void begin(CL_C10_Config* p_cfg, bool (*p_applyFn)(void*), void* p_applyCtx) {
        _cfg = p_cfg;
        _applyFn = p_applyFn;
        _applyCtx = p_applyCtx;

        WiFi.onEvent(CL_W10_WebConfig::_WiFiEventThunk);
        (void)LittleFS.begin(true);

        (void)_cfg->loadAll(_wifi, _e10);

        _setupWiFi();

        // ---- Static assets ----
        for (size_t v_i = 0; v_i < (sizeof(G_W10_ASSETS) / sizeof(G_W10_ASSETS[0])); v_i++) {
            const ST_W10_Asset_t& v_a = G_W10_ASSETS[v_i];
            _svr.on(v_a.uri, HTTP_GET, [this, v_a](AsyncWebServerRequest* p_req) {
                this->_serveAsset(p_req, v_a);
            });
        }

        // ---- APIs ----
        _svr.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* p_req) { this->_apiStatus(p_req); });
        _svr.on("/api/keycodes", HTTP_GET, [this](AsyncWebServerRequest* p_req) { this->_apiKeycodes(p_req); });

        _svr.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* p_req) { this->_apiGetConfig(p_req); });

        _svr.on("/api/config/save", HTTP_POST,
            [this](AsyncWebServerRequest* p_req) { (void)p_req; }, nullptr,
            [this](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total) {
                this->_apiConfigSaveBody(p_req, p_data, p_len, p_index, p_total);
            });

        _svr.on("/api/config/apply", HTTP_POST,
            [this](AsyncWebServerRequest* p_req) { (void)p_req; }, nullptr,
            [this](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total) {
                this->_apiConfigApplyBody(p_req, p_data, p_len, p_index, p_total);
            });

        _svr.on("/api/config/export", HTTP_GET, [this](AsyncWebServerRequest* p_req) { this->_apiExport(p_req); });

        _svr.on("/api/config/import", HTTP_POST,
            [this](AsyncWebServerRequest* p_req) { (void)p_req; }, nullptr,
            [this](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total) {
                this->_apiImportBody(p_req, p_data, p_len, p_index, p_total);
            });

        _svr.on("/api/config/rollback", HTTP_POST,
            [this](AsyncWebServerRequest* p_req) { (void)p_req; }, nullptr,
            [this](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total) {
                (void)p_data; (void)p_len; (void)p_index; (void)p_total;
                this->_apiRollback(p_req);
            });

        _svr.on("/api/control", HTTP_POST,
            [this](AsyncWebServerRequest* p_req) { (void)p_req; }, nullptr,
            [this](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total) {
                this->_apiControlBody(p_req, p_data, p_len, p_index, p_total);
            });

        _svr.on("/api/ppt", HTTP_GET, [this](AsyncWebServerRequest* p_req) { this->_apiGetPpt(p_req); });

        _svr.on("/api/ppt", HTTP_POST,
            [this](AsyncWebServerRequest* p_req) { (void)p_req; }, nullptr,
            [this](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total) {
                this->_apiPostPptBody(p_req, p_data, p_len, p_index, p_total);
            });

        _svr.on("/api/ppt/test", HTTP_POST,
            [this](AsyncWebServerRequest* p_req) { (void)p_req; }, nullptr,
            [this](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total) {
                this->_apiPptTestBody(p_req, p_data, p_len, p_index, p_total);
            });

        _svr.on("/api/ota/status", HTTP_GET, [this](AsyncWebServerRequest* p_req) { this->_apiOtaStatus(p_req); });

        _svr.on("/api/ota", HTTP_POST,
            [this](AsyncWebServerRequest* p_req) {
                JsonDocument v_out;
                v_out["ok"] = _otaOk;
                v_out["err"] = _otaErr;
                v_out["written"] = (uint32_t)_otaWritten;
                v_out["total"] = (uint32_t)_otaTotal;

                String v_json;
                serializeJson(v_out, v_json);
                p_req->send(_otaOk ? 200 : 500, "application/json", v_json);
                if (_otaOk) { delay(200); ESP.restart(); }
            },
            [this](AsyncWebServerRequest* p_req, const String& p_filename, size_t p_index, uint8_t* p_data, size_t p_len, bool p_final) {
                this->_apiOtaUpload(p_req, p_filename, p_index, p_data, p_len, p_final);
            });

        _svr.on("/api/safeboot", HTTP_GET, [this](AsyncWebServerRequest* p_req) { this->_apiSafeBootGet(p_req); });

        _svr.on("/api/safeboot", HTTP_POST,
            [this](AsyncWebServerRequest* p_req) { (void)p_req; }, nullptr,
            [this](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total) {
                this->_apiSafeBootPostBody(p_req, p_data, p_len, p_index, p_total);
            });

        _svr.on("/api/factory_reset", HTTP_POST,
            [this](AsyncWebServerRequest* p_req) { (void)p_req; }, nullptr,
            [this](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total) {
                (void)p_data; (void)p_len; (void)p_index; (void)p_total;
                this->_apiFactoryReset(p_req);
            });

        _svr.on("/api/reboot", HTTP_POST, [this](AsyncWebServerRequest* p_req) {
            p_req->send(200, "application/json", "{\"ok\":true}");
            delay(50);
            ESP.restart();
        });

        _svr.onNotFound([](AsyncWebServerRequest* p_req) { p_req->send(404, "text/plain", "not found"); });

        _svr.begin();
    }

  private:
    // ======================================================
    // WiFi Event Thunk
    // ======================================================
    static void _WiFiEventThunk(WiFiEvent_t p_event, WiFiEventInfo_t p_info) {
        (void)p_info;
        if (s_instance) s_instance->_onWiFiEvent(p_event);
    }

    // ======================================================
    // WiFi Setup
    // ======================================================
    void _setupWiFi() {
        WiFi.mode(WIFI_MODE_NULL);

        const bool v_hasSta = (_wifi.sta_ssid[0] != '\0');
        const bool v_autoM = (_wifi.mode == (uint8_t)EN_C10_WIFI_AUTO);
        const bool v_forceAp = (_wifi.mode == (uint8_t)EN_C10_WIFI_AP);
        const bool v_forceSta = (_wifi.mode == (uint8_t)EN_C10_WIFI_STA);

        // (0273~) SafeBoot: 무조건 AP 진입
        if (_cfg && _cfg->isSafeMode()) {
            char v_ssid[33];
            memset(v_ssid, 0, sizeof(v_ssid));
            strlcpy(v_ssid, _wifi.ap_ssid, sizeof(v_ssid));
            if (strlen(v_ssid) <= 28) strlcat(v_ssid, "-SAFE", sizeof(v_ssid));

            WiFi.mode(WIFI_AP);
            if (_wifi.ap_pass[0] != '\0') WiFi.softAP(v_ssid, _wifi.ap_pass);
            else WiFi.softAP(v_ssid);
            return;
        }

        // AP 강제 또는 STA 정보 없음이면 AP
        if (v_forceAp || (!v_hasSta && (v_autoM || !v_forceSta))) {
            _startAp();
            return;
        }

        // STA 시도
        WiFi.mode(WIFI_STA);
        WiFi.begin(_wifi.sta_ssid, _wifi.sta_pass);

        const uint32_t v_t0 = millis();
        bool v_ok = false;
        while (millis() - v_t0 < 8000) {
            if (WiFi.status() == WL_CONNECTED) { v_ok = true; break; }
            delay(200);
        }

        if (v_ok) {
            _startMdns();
            return;
        }

        // STA 실패 → AUTO면 AP fallback
        if (v_autoM) {
            _startAp();
            return;
        }

        // forceSta인데 실패: 여기서는 그대로 둠(원하면 fallback 옵션 추가 가능)
    }

    void _startAp() {
        WiFi.mode(WIFI_AP);
        // pass가 비어있으면 open AP 허용
        if (_wifi.ap_pass[0] != '\0') WiFi.softAP(_wifi.ap_ssid, _wifi.ap_pass);
        else WiFi.softAP(_wifi.ap_ssid);
    }

    void _onWiFiEvent(WiFiEvent_t p_event) {
        if (p_event == ARDUINO_EVENT_WIFI_STA_GOT_IP) _startMdns();
        if (p_event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) _stopMdns();
    }

    void _startMdns() {
        if (_mdnsStarted) return;
        if (_wifi.mdns_host[0] == '\0') return;
        if (WiFi.getMode() != WIFI_STA || WiFi.status() != WL_CONNECTED) return;

        if (!MDNS.begin(_wifi.mdns_host)) return;
        MDNS.addService("http", "tcp", 80);
        _mdnsStarted = true;
    }

    void _stopMdns() {
        if (!_mdnsStarted) return;
        MDNS.end();
        _mdnsStarted = false;
    }

    // ======================================================
    // Asset helpers (gzip + cache-control auto classify)
    // ======================================================
    bool _acceptsGzip(AsyncWebServerRequest* p_req) {
        if (!p_req->hasHeader("Accept-Encoding")) return false;
        const String v_ae = p_req->header("Accept-Encoding");
        return (v_ae.indexOf("gzip") >= 0);
    }

    bool _endsWithI(const char* p_s, const char* p_suffix) {
        if (!p_s || !p_suffix) return false;
        const size_t v_ls = strlen(p_s);
        const size_t v_lf = strlen(p_suffix);
        if (v_lf > v_ls) return false;
        return (strcasecmp(p_s + (v_ls - v_lf), p_suffix) == 0);
    }

    bool _isHtmlPath(const char* p_path) {
        if (!p_path) return false;
        return _endsWithI(p_path, ".html") || _endsWithI(p_path, ".htm");
    }

    bool _isStaticAssetPath(const char* p_path) {
        // gzip 결과물(.gz)은 “원본이 뭐든” 정적 자산 취급(immutable 후보)
        if (!p_path) return false;
        if (_endsWithI(p_path, ".gz")) return true;

        // 이미지/벡터는 gzip 제외(툴에서 복사만) → 캐시는 immutable가 유리
        if (_endsWithI(p_path, ".css")) return true;
        if (_endsWithI(p_path, ".js")) return true;
        if (_endsWithI(p_path, ".svg")) return true;
        if (_endsWithI(p_path, ".png")) return true;
        if (_endsWithI(p_path, ".webp")) return true;
        if (_endsWithI(p_path, ".ico")) return true;
        return false;
    }

    bool _hasVersionToken(const char* p_path) {
        // 0275 자산 파일명 규칙(예: style_0275.css / app_0275.js / index_0275.html)
        // immutable을 안전하게 적용하기 위해 “버전 토큰 포함 시” immutable 확정
        if (!p_path) return false;
        const char* v = p_path;

        // _0275 / .0275. / _v0275 등 다양한 토큰 허용
        if (strstr(v, "_0275") != nullptr) return true;
        if (strstr(v, ".0275") != nullptr) return true;
        if (strstr(v, "v0275") != nullptr) return true;

        // gz 파일은 원본이 버전파일일 가능성이 높지만, 안전하게 원본명 기준을 같이 체크
        // (필요하면 여기서 더 강화 가능)
        return false;
    }

    const char* _cacheControlFor(const char* p_uri, const char* p_path, const char* p_type) {
        (void)p_type;

        // 1) HTML은 기본 no-store (특히 index/루트)
        if (p_uri && (strcmp(p_uri, "/") == 0 || strcmp(p_uri, "/www/") == 0)) return G_W10_CC_NOSTORE;
        if (_isHtmlPath(p_path)) return G_W10_CC_NOSTORE;

        // 2) 정적 자산은 immutable 우선
        if (_isStaticAssetPath(p_path)) {
            // 버전 토큰이 있으면 immutable 확정
            if (_hasVersionToken(p_path)) return G_W10_CC_IMMUTABLE;
            // 버전 토큰이 없으면 너무 공격적 immutable을 피하고 1시간 캐시
            return G_W10_CC_SHORT;
        }

        // 3) 그 외는 안전하게 no-store
        return G_W10_CC_NOSTORE;
    }

    void _serveAsset(AsyncWebServerRequest* p_req, const ST_W10_Asset_t& p_a) {
        bool v_useGz = false;
        const char* v_path = p_a.plain;

        // gzip 우선(있고, 클라이언트가 gzip 허용 시)
        if (p_a.gz && LittleFS.exists(p_a.gz) && _acceptsGzip(p_req)) {
            v_useGz = true;
            v_path = p_a.gz;
        } else {
            if (!LittleFS.exists(p_a.plain)) {
                p_req->send(404, "text/plain", "asset not found");
                return;
            }
            v_path = p_a.plain;
        }

        AsyncWebServerResponse* v_res = p_req->beginResponse(LittleFS, v_path, p_a.type);
        if (v_useGz) v_res->addHeader("Content-Encoding", "gzip");

        // 자동 Cache-Control 분류
        const char* v_cc = _cacheControlFor(p_a.uri, v_path, p_a.type);
        v_res->addHeader("Cache-Control", v_cc);

        p_req->send(v_res);
    }

    // ======================================================
    // JSON helpers (단일 JsonDocument 규칙 준수)
    // ======================================================
    void _sendJson(AsyncWebServerRequest* p_req, JsonDocument& p_doc, int p_code = 200) {
        String v_out;
        serializeJson(p_doc, v_out);
        p_req->send(p_code, "application/json", v_out);
    }

    String* _reqBody(AsyncWebServerRequest* p_req, size_t p_index) {
        // AsyncWebServerRequest::_tempObject를 String*으로 사용
        if (p_index == 0) {
            if (p_req->_tempObject) {
                delete (String*)p_req->_tempObject;
                p_req->_tempObject = nullptr;
            }
            p_req->_tempObject = new String();
        }
        return (String*)p_req->_tempObject;
    }

    void _reqBodyFree(AsyncWebServerRequest* p_req) {
        if (p_req->_tempObject) {
            delete (String*)p_req->_tempObject;
            p_req->_tempObject = nullptr;
        }
    }

    // ======================================================
    // /api/status
    // ======================================================
    void _apiStatus(AsyncWebServerRequest* p_req) {
        JsonDocument v_doc;

        v_doc["uptime_ms"] = (uint32_t)millis();
        v_doc["heap_free"] = (uint32_t)ESP.getFreeHeap();

        JsonObject v_net = v_doc["net"].to<JsonObject>();
        v_net["mode"] = (WiFi.getMode() == WIFI_AP) ? "AP" : "STA";
        v_net["ip"] = (WiFi.getMode() == WIFI_AP) ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
        v_net["ssid"] = (WiFi.getMode() == WIFI_AP) ? String(_wifi.ap_ssid) : WiFi.SSID();
        v_net["mdns"] = String(_wifi.mdns_host) + ".local";

        CL_E10_EliteAirMouse* v_e10 = (CL_E10_EliteAirMouse*)_applyCtx;
        if (v_e10) {
            ST_E10_Status_t v_s;
            v_e10->getStatus(v_s);

            JsonObject v_e = v_doc["e10"].to<JsonObject>();
            v_e["ble_connected"] = v_s.ble_connected;
            v_e["ppt_mode"] = v_s.ppt_mode;
            v_e["dpi_level"] = v_s.dpi_level;
            v_e["precision_enable"] = v_s.precision_enable;
            v_e["precision_mode"] = v_s.precision_mode;
            v_e["fsm_state"] = v_s.fsm_state;
            v_e["fsm_sub"] = v_s.fsm_sub;

            JsonObject v_h = v_e["health"].to<JsonObject>();
            v_h["state"] = v_s.health;
            v_h["score"] = v_s.health_score;

            JsonObject v_gyro = v_e["gyro"].to<JsonObject>();
            v_gyro["bias_x"] = v_s.gyro_bias_x;
            v_gyro["bias_y"] = v_s.gyro_bias_y;
            v_gyro["bias_z"] = v_s.gyro_bias_z;
            v_gyro["rms"] = v_s.gyro_rms;

            v_e["cursor_rms"] = v_s.cursor_rms;
            v_e["temp_c"] = v_s.temp_c;

            JsonObject v_i2c = v_e["i2c"].to<JsonObject>();
            v_i2c["recover_count"] = v_s.i2c_recover_count;
            v_i2c["recover_last_ok"] = v_s.i2c_recover_last_ok;

            JsonObject v_err = v_e["err"].to<JsonObject>();
            v_err["mpu_nan"] = v_s.err_mpu_nan;
            v_err["mutex_miss"] = v_s.err_mutex_miss;
            v_err["task_overrun"] = v_s.err_task_overrun;

            JsonObject v_an = v_e["anomaly"].to<JsonObject>();
            v_an["spike_count_10s"] = v_s.spike_count_10s;
            v_an["consecutive_fail"] = v_s.consecutive_fail;
            v_an["consecutive_recover_fail"] = v_s.consecutive_recover_fail;

            JsonArray v_hist = v_e["err_hist"].to<JsonArray>();
            for (uint8_t v_i = 0; v_i < v_s.err_hist_n; v_i++) {
                JsonObject v_o = v_hist.add<JsonObject>();
                v_o["ts_ms"] = v_s.err_hist[v_i].ts_ms;
                v_o["code"] = v_s.err_hist[v_i].code;
                v_o["value"] = v_s.err_hist[v_i].value;
            }
        }

        JsonObject v_ota = v_doc["ota"].to<JsonObject>();
        v_ota["in_progress"] = _otaInProgress;
        v_ota["total"] = (uint32_t)_otaTotal;
        v_ota["written"] = (uint32_t)_otaWritten;
        v_ota["ok"] = _otaOk;
        v_ota["err"] = _otaErr;

        if (_cfg) {
            ST_C10_BootState_t v_bs;
            _cfg->getBootState(v_bs);
            JsonObject v_b = v_doc["boot"].to<JsonObject>();
            v_b["safe_mode"] = v_bs.safe_mode;
            v_b["fail_count"] = v_bs.fail_count;
            v_b["pending"] = v_bs.pending;
        }

        _sendJson(p_req, v_doc);
    }

    // ======================================================
    // /api/keycodes
    // ======================================================
    const char* _kbName(uint16_t p_code) {
        switch (p_code) {
            case 0x00: return "None";
            case 0x04: return "A"; case 0x05: return "B"; case 0x06: return "C"; case 0x07: return "D";
            case 0x08: return "E"; case 0x09: return "F"; case 0x0A: return "G"; case 0x0B: return "H";
            case 0x0C: return "I"; case 0x0D: return "J"; case 0x0E: return "K"; case 0x0F: return "L";
            case 0x10: return "M"; case 0x11: return "N"; case 0x12: return "O"; case 0x13: return "P";
            case 0x14: return "Q"; case 0x15: return "R"; case 0x16: return "S"; case 0x17: return "T";
            case 0x18: return "U"; case 0x19: return "V"; case 0x1A: return "W"; case 0x1B: return "X";
            case 0x1C: return "Y"; case 0x1D: return "Z";
            case 0x1E: return "1"; case 0x1F: return "2"; case 0x20: return "3"; case 0x21: return "4";
            case 0x22: return "5"; case 0x23: return "6"; case 0x24: return "7"; case 0x25: return "8";
            case 0x26: return "9"; case 0x27: return "0";
            case 0x28: return "Enter"; case 0x29: return "Esc"; case 0x2A: return "Backspace"; case 0x2B: return "Tab";
            case 0x2C: return "Space";
            case 0x3A: return "F1"; case 0x3B: return "F2"; case 0x3C: return "F3"; case 0x3D: return "F4";
            case 0x3E: return "F5"; case 0x3F: return "F6"; case 0x40: return "F7"; case 0x41: return "F8";
            case 0x42: return "F9"; case 0x43: return "F10"; case 0x44: return "F11"; case 0x45: return "F12";
            case 0x4B: return "PageUp"; case 0x4E: return "PageDown";
            case 0x4F: return "Right"; case 0x50: return "Left"; case 0x51: return "Down"; case 0x52: return "Up";
            default: return nullptr;
        }
    }

    void _apiKeycodes(AsyncWebServerRequest* p_req) {
        JsonDocument v_doc;

        JsonArray v_mods = v_doc["mods"].to<JsonArray>();
        for (size_t v_i = 0; v_i < (sizeof(G_W10_MODS) / sizeof(G_W10_MODS[0])); v_i++) {
            JsonObject v_o = v_mods.add<JsonObject>();
            v_o["name"] = G_W10_MODS[v_i].name;
            v_o["mask"] = G_W10_MODS[v_i].mask;
        }

        JsonArray v_kb = v_doc["kb"].to<JsonArray>();
        char v_nameBuf[8];
        for (uint16_t v_code = 0; v_code <= 0xE7; v_code++) {
            const char* v_n = _kbName(v_code);
            if (!v_n) {
                snprintf(v_nameBuf, sizeof(v_nameBuf), "0x%02X", (unsigned)v_code);
                v_n = v_nameBuf;
            }
            JsonObject v_o = v_kb.add<JsonObject>();
            v_o["name"] = v_n;
            v_o["code"] = v_code;
        }

        JsonArray v_con = v_doc["consumer"].to<JsonArray>();
        for (size_t v_i = 0; v_i < (sizeof(G_W10_CONSUMER) / sizeof(G_W10_CONSUMER[0])); v_i++) {
            JsonObject v_o = v_con.add<JsonObject>();
            v_o["name"] = G_W10_CONSUMER[v_i].name;
            v_o["mask"] = (uint32_t)G_W10_CONSUMER[v_i].mask;
        }

        v_doc["note"] = "mods mask == HID modifier byte. kb=usage-id(0x07), consumer=32-bit mask.";
        _sendJson(p_req, v_doc);
    }

    // ======================================================
    // /api/config (get/save/apply/export/import/rollback)
    // ======================================================
    void _apiGetConfig(AsyncWebServerRequest* p_req) {
        String v_json;
        if (!_cfg || !_cfg->exportJson(v_json)) {
            p_req->send(500, "application/json", "{\"ok\":false}");
            return;
        }
        p_req->send(200, "application/json", v_json);
    }

    void _apiConfigSaveBody(AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total) {
        String* v_body = _reqBody(p_req, p_index);
        if (!v_body) { p_req->send(500, "application/json", "{\"ok\":false}"); return; }

        for (size_t v_i = 0; v_i < p_len; v_i++) (*v_body) += (char)p_data[v_i];
        if (p_index + p_len < p_total) return;

        bool v_saved = false;
        bool v_applied = false;

        bool v_ok = (_cfg != nullptr) ? _cfg->importJson(*v_body, v_saved, v_applied) : false;
        _reqBodyFree(p_req);

        if (v_ok && v_saved && _applyFn) v_applied = _applyFn(_applyCtx);

        JsonDocument v_out;
        v_out["ok"] = v_ok;
        v_out["saved"] = v_saved;
        v_out["applied"] = v_applied;
        v_out["note"] = "WiFi changes require reboot.";
        _sendJson(p_req, v_out, v_ok ? 200 : 400);
    }

    void _apiConfigApplyBody(AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total) {
        String* v_body = _reqBody(p_req, p_index);
        if (!v_body) { p_req->send(500, "application/json", "{\"ok\":false}"); return; }

        for (size_t v_i = 0; v_i < p_len; v_i++) (*v_body) += (char)p_data[v_i];
        if (p_index + p_len < p_total) return;

        ST_C10_WiFiConfig_t v_w;
        ST_C10_E10Config_t v_e;

        bool v_ok = true;
        if (!_cfg) v_ok = false;

        if (v_ok) {
            _cfg->makeDefaultsWiFi(v_w);
            _cfg->makeDefaultsE10(v_e);
            (void)_cfg->loadAll(v_w, v_e);

            v_ok = v_ok && _cfg->patchFromJsonWiFi(*v_body, v_w);
            v_ok = v_ok && _cfg->patchFromJsonE10(*v_body, v_e);

            v_ok = v_ok && _cfg->validateWiFi(v_w);
            v_ok = v_ok && _cfg->validateE10(v_e);
        }

        bool v_applied = false;
        CL_E10_EliteAirMouse* v_e10 = (CL_E10_EliteAirMouse*)_applyCtx;
        if (v_ok && v_e10) {
            v_applied = v_e10->applyRuntimeE10(v_e);
        }

        _reqBodyFree(p_req);

        JsonDocument v_out;
        v_out["ok"] = v_ok;
        v_out["applied"] = v_applied;
        v_out["note"] = "apply-only: not saved. WiFi is validated but not applied to WiFi runtime.";
        _sendJson(p_req, v_out, v_ok ? 200 : 400);
    }

    void _apiExport(AsyncWebServerRequest* p_req) {
        String v_json;
        if (!_cfg || !_cfg->exportJson(v_json)) {
            p_req->send(500, "application/json", "{\"ok\":false}");
            return;
        }
        AsyncWebServerResponse* v_res = p_req->beginResponse(200, "application/json", v_json);
        v_res->addHeader("Content-Disposition", "attachment; filename=\"config_0272.json\"");
        v_res->addHeader("Cache-Control", G_W10_CC_NOSTORE);
        p_req->send(v_res);
    }

    void _apiImportBody(AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total) {
        String* v_body = _reqBody(p_req, p_index);
        if (!v_body) { p_req->send(500, "application/json", "{\"ok\":false}"); return; }

        for (size_t v_i = 0; v_i < p_len; v_i++) (*v_body) += (char)p_data[v_i];
        if (p_index + p_len < p_total) return;

        bool v_saved = false;
        bool v_applied = false;

        bool v_ok = (_cfg != nullptr) ? _cfg->importJson(*v_body, v_saved, v_applied) : false;
        _reqBodyFree(p_req);

        if (v_ok && v_saved && _applyFn) v_applied = _applyFn(_applyCtx);

        JsonDocument v_out;
        v_out["ok"] = v_ok;
        v_out["saved"] = v_saved;
        v_out["applied"] = v_applied;
        _sendJson(p_req, v_out, v_ok ? 200 : 400);
    }

    void _apiRollback(AsyncWebServerRequest* p_req) {
        bool v_ok = (_cfg != nullptr) ? _cfg->rollbackFromBak() : false;
        bool v_applied = false;
        if (v_ok && _applyFn) v_applied = _applyFn(_applyCtx);

        JsonDocument v_out;
        v_out["ok"] = v_ok;
        v_out["applied"] = v_applied;
        _sendJson(p_req, v_out, v_ok ? 200 : 400);
    }

    // ======================================================
    // /api/control
    // ======================================================
    void _apiControlBody(AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total) {
        String* v_body = _reqBody(p_req, p_index);
        if (!v_body) { p_req->send(500, "application/json", "{\"ok\":false}"); return; }

        for (size_t v_i = 0; v_i < p_len; v_i++) (*v_body) += (char)p_data[v_i];
        if (p_index + p_len < p_total) return;

        JsonDocument v_in;
        DeserializationError v_err = deserializeJson(v_in, *v_body);
        _reqBodyFree(p_req);
        if (v_err) { p_req->send(400, "application/json", "{\"ok\":false,\"err\":\"bad_json\"}"); return; }

        CL_E10_EliteAirMouse* v_e10 = (CL_E10_EliteAirMouse*)_applyCtx;
        bool v_ok = true;

        if (v_e10) {
            if (!v_in["ppt_mode"].isNull()) v_ok = v_ok && v_e10->setPptMode((bool)v_in["ppt_mode"]);
            if (!v_in["dpi_level"].isNull()) v_ok = v_ok && v_e10->setDpiLevel((uint8_t)v_in["dpi_level"]);
            if (!v_in["precision_mode"].isNull()) v_ok = v_ok && v_e10->setPrecisionMode((bool)v_in["precision_mode"]);
        } else {
            v_ok = false;
        }

        JsonDocument v_out;
        v_out["ok"] = v_ok;
        _sendJson(p_req, v_out, v_ok ? 200 : 500);
    }

    // ======================================================
    // /api/ppt
    // ======================================================
    void _apiGetPpt(AsyncWebServerRequest* p_req) {
        if (_cfg) (void)_cfg->loadAll(_wifi, _e10);

        JsonDocument v_doc;
        JsonObject v_map = v_doc["map"].to<JsonObject>();

        auto v_put = [&](const char* p_name, const ST_C10_PptKey2_t& p_k) {
            JsonObject v_o = v_map[p_name].to<JsonObject>();
            v_o["page"] = (p_k.page == (uint8_t)EN_C10_KEYPAGE_CONSUMER) ? "consumer" : "kb";
            v_o["mod"]  = p_k.mod;
            v_o["code"] = p_k.code;
        };

        v_put("start", _e10.ppt2_start);
        v_put("exit",  _e10.ppt2_exit);
        v_put("next",  _e10.ppt2_next);
        v_put("prev",  _e10.ppt2_prev);
        v_put("black", _e10.ppt2_black);
        v_put("laser", _e10.ppt2_laser);

        _sendJson(p_req, v_doc);
    }

    void _apiPostPptBody(AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total) {
        String* v_body = _reqBody(p_req, p_index);
        if (!v_body) { p_req->send(500, "application/json", "{\"ok\":false}"); return; }

        for (size_t v_i = 0; v_i < p_len; v_i++) (*v_body) += (char)p_data[v_i];
        if (p_index + p_len < p_total) return;

        JsonDocument v_in;
        DeserializationError v_err = deserializeJson(v_in, *v_body);
        _reqBodyFree(p_req);
        if (v_err) { p_req->send(400, "application/json", "{\"ok\":false,\"err\":\"bad_json\"}"); return; }

        bool v_save = true;
        if (!v_in["save"].isNull()) v_save = (bool)v_in["save"];

        JsonVariant v_map = v_in["map"];
        if (v_map.isNull()) { p_req->send(400, "application/json", "{\"ok\":false,\"err\":\"no_map\"}"); return; }

        ST_C10_WiFiConfig_t v_w;
        ST_C10_E10Config_t v_e;

        bool v_ok = true;
        if (!_cfg) v_ok = false;

        if (v_ok) {
            _cfg->makeDefaultsWiFi(v_w);
            _cfg->makeDefaultsE10(v_e);
            (void)_cfg->loadAll(v_w, v_e);

            auto v_load = [&](const char* p_name, ST_C10_PptKey2_t& p_k) {
                JsonVariant v_o = v_map[p_name];
                if (v_o.isNull()) return;

                if (!v_o["page"].isNull()) {
                    const char* v_s = (const char*)v_o["page"];
                    if (v_s && strcasecmp(v_s, "consumer") == 0) p_k.page = (uint8_t)EN_C10_KEYPAGE_CONSUMER;
                    else p_k.page = (uint8_t)EN_C10_KEYPAGE_KB;
                }
                if (!v_o["mod"].isNull())  p_k.mod  = (uint8_t)v_o["mod"];
                if (!v_o["code"].isNull()) p_k.code = (uint32_t)v_o["code"];
            };

            v_load("start", v_e.ppt2_start);
            v_load("exit",  v_e.ppt2_exit);
            v_load("next",  v_e.ppt2_next);
            v_load("prev",  v_e.ppt2_prev);
            v_load("black", v_e.ppt2_black);
            v_load("laser", v_e.ppt2_laser);

            v_ok = v_ok && _cfg->validateE10(v_e);
        }

        bool v_saved = false;
        if (v_ok && v_save) {
            v_ok = _cfg->saveAll(v_w, v_e);
            v_saved = v_ok;
        }

        bool v_applied = false;
        CL_E10_EliteAirMouse* v_e10 = (CL_E10_EliteAirMouse*)_applyCtx;
        if (v_ok && v_e10) v_applied = v_e10->applyRuntimeE10(v_e);

        JsonDocument v_out;
        v_out["ok"] = v_ok;
        v_out["saved"] = v_saved;
        v_out["applied"] = v_applied;
        _sendJson(p_req, v_out, v_ok ? 200 : 400);
    }

    void _apiPptTestBody(AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total) {
        String* v_body = _reqBody(p_req, p_index);
        if (!v_body) { p_req->send(500, "application/json", "{\"ok\":false}"); return; }

        for (size_t v_i = 0; v_i < p_len; v_i++) (*v_body) += (char)p_data[v_i];
        if (p_index + p_len < p_total) return;

        JsonDocument v_in;
        DeserializationError v_err = deserializeJson(v_in, *v_body);
        _reqBodyFree(p_req);
        if (v_err) { p_req->send(400, "application/json", "{\"ok\":false,\"err\":\"bad_json\"}"); return; }

        uint8_t v_page = (uint8_t)EN_C10_KEYPAGE_KB;
        uint8_t v_mod = 0;
        uint32_t v_code = 0;

        if (!v_in["page"].isNull()) {
            const char* v_s = (const char*)v_in["page"];
            if (v_s && strcasecmp(v_s, "consumer") == 0) v_page = (uint8_t)EN_C10_KEYPAGE_CONSUMER;
        }
        if (!v_in["mod"].isNull())  v_mod = (uint8_t)v_in["mod"];
        if (!v_in["code"].isNull()) v_code = (uint32_t)v_in["code"];

        CL_E10_EliteAirMouse* v_e10 = (CL_E10_EliteAirMouse*)_applyCtx;
        bool v_ok = false;
        if (v_e10) v_ok = v_e10->testPptKey2(v_page, v_mod, v_code);

        JsonDocument v_out;
        v_out["ok"] = v_ok;
        _sendJson(p_req, v_out, v_ok ? 200 : 500);
    }

    // ======================================================
    // OTA
    // ======================================================
    void _apiOtaUpload(AsyncWebServerRequest* p_req, const String& p_filename, size_t p_index, uint8_t* p_data, size_t p_len, bool p_final) {
        (void)p_filename;

        if (p_index == 0) {
            // 동시 업로드 방지
            if (_otaInProgress) {
                _otaOk = false;
                strlcpy(_otaErr, "busy", sizeof(_otaErr));
                return;
            }

            _otaInProgress = true;
            _otaWritten = 0;
            _otaTotal = (uint32_t)p_req->contentLength();
            _otaOk = false;
            strlcpy(_otaErr, "in_progress", sizeof(_otaErr));

            // 스케치 공간 검증
            const size_t v_sketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
            if (_otaTotal == 0 || _otaTotal > (uint32_t)v_sketchSpace) {
                _otaOk = false;
                strlcpy(_otaErr, "size_invalid", sizeof(_otaErr));
                _otaInProgress = false;
                return;
            }

            if (!Update.begin(v_sketchSpace)) {
                _otaOk = false;
                strlcpy(_otaErr, "Update.begin failed", sizeof(_otaErr));
                _otaInProgress = false;
                return;
            }
        }

        if (p_len) {
            const size_t v_w = Update.write(p_data, p_len);
            _otaWritten += (uint32_t)v_w;
            if (v_w != p_len) strlcpy(_otaErr, "Update.write mismatch", sizeof(_otaErr));
        }

        if (p_final) {
            if (!Update.end(true)) { strlcpy(_otaErr, "Update.end failed", sizeof(_otaErr)); _otaOk = false; }
            else if (Update.hasError()) { strlcpy(_otaErr, "Update.hasError", sizeof(_otaErr)); _otaOk = false; }
            else { _otaOk = true; strlcpy(_otaErr, "ok", sizeof(_otaErr)); }
            _otaInProgress = false;
        }
    }

    void _apiOtaStatus(AsyncWebServerRequest* p_req) {
        JsonDocument v_doc;
        v_doc["in_progress"] = _otaInProgress;
        v_doc["total"] = (uint32_t)_otaTotal;
        v_doc["written"] = (uint32_t)_otaWritten;
        v_doc["ok"] = _otaOk;
        v_doc["err"] = _otaErr;
        _sendJson(p_req, v_doc);
    }

    // ======================================================
    // SafeBoot / FactoryReset
    // ======================================================
    void _apiSafeBootGet(AsyncWebServerRequest* p_req) {
        JsonDocument v_doc;

        if (_cfg) {
            ST_C10_BootState_t v_bs;
            _cfg->getBootState(v_bs);

            v_doc["ok"] = true;
            v_doc["safe_mode"] = v_bs.safe_mode;
            v_doc["fail_count"] = v_bs.fail_count;
            v_doc["pending"] = v_bs.pending;
        } else {
            v_doc["ok"] = false;
        }

        _sendJson(p_req, v_doc, v_doc["ok"] ? 200 : 500);
    }

    void _apiSafeBootPostBody(AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total) {
        String* v_body = _reqBody(p_req, p_index);
        if (!v_body) { p_req->send(500, "application/json", "{\"ok\":false}"); return; }

        for (size_t v_i = 0; v_i < p_len; v_i++) (*v_body) += (char)p_data[v_i];
        if (p_index + p_len < p_total) return;

        JsonDocument v_in;
        DeserializationError v_err = deserializeJson(v_in, *v_body);
        _reqBodyFree(p_req);
        if (v_err) { p_req->send(400, "application/json", "{\"ok\":false,\"err\":\"bad_json\"}"); return; }

        bool v_ok = false;
        if (_cfg && !v_in["exit"].isNull() && (bool)v_in["exit"]) {
            v_ok = _cfg->clearSafeMode();
        }

        JsonDocument v_out;
        v_out["ok"] = v_ok;
        v_out["note"] = "If safe mode was active, reboot recommended after exit.";
        _sendJson(p_req, v_out, v_ok ? 200 : 500);
    }

    void _apiFactoryReset(AsyncWebServerRequest* p_req) {
        bool v_ok = false;
        if (_cfg) v_ok = _cfg->factoryReset(true);

        JsonDocument v_out;
        v_out["ok"] = v_ok;
        v_out["note"] = "Factory reset done. Rebooting...";
        _sendJson(p_req, v_out, v_ok ? 200 : 500);

        if (v_ok) { delay(200); ESP.restart(); }
    }
};

