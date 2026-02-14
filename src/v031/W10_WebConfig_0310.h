// =======================================================
// File: W10_WebConfig_0310.h
// =======================================================
#pragma once

/*
 * ------------------------------------------------------
 * 소스명 : W10_WebConfig_0310.h
 * 모듈약어 : W10
 * 모듈명 : Web Config/Status/UI/OTA Server (Dynamic Static Routing, No Asset Table)
 * ------------------------------------------------------
 * 기능 요약
 *  - 자산 테이블 제거: 동적 정적파일 서빙(/www/*)
 *  - 보안: /favicon.ico /robots.txt /www/* /json/public/* 만 허용
 *  - gzip: html/css/js 만 (.gz 존재 + Accept-Encoding:gzip) 시 사용
 *  - Cache-Control 자동 분류:
 *     * no-store: html, /json/public/*, /api/*
 *     * immutable: 버전 토큰 포함한 정적 리소스(css/js/svg/png/webp/ico)
 *     * short: 그 외 정적 리소스
 *  - API: /api/status, /api/keycodes, /api/config, /api/ppt, /api/ota, /api/safeboot, /api/factory_reset ...
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

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <string.h>

#include "C10_Config_0310.h"
#include "E10_Def_0310.h"
#include "W10_Def_0310.h"


// =======================================================
// [W10-E10 Interface] (decouple include dependency)
// - W10은 E10 class header를 include하지 않고, 함수 포인터 인터페이스로만 호출
// =======================================================
struct ST_W10_E10If_t {
    void* ctx;

    // status snapshot
    bool (*getStatus)(void* ctx, ST_E10_Status_t* out);

    // runtime apply-only (no persist)
    bool (*applyRuntimeE10)(void* ctx, const ST_C10_E10Config_t* e10);

    // runtime controls
    bool (*setPptMode)(void* ctx, bool en);
    bool (*setDpiLevel)(void* ctx, uint8_t level);
    bool (*setPrecisionMode)(void* ctx, uint8_t mode);
    bool (*setHardClickLock)(void* ctx, bool en);

    bool (*setSafeMode)(void* ctx, bool en);
    bool (*setOtaGuard)(void* ctx, bool en);

    bool (*forceReleaseButtons)(void* ctx);
    bool (*requestGyroCalibration)(void* ctx);
    bool (*requestI2CRecover)(void* ctx);
    bool (*clearDiagnostics)(void* ctx);

    // PPT test
    bool (*testPptKey2)(void* ctx, uint8_t page, uint8_t mod, uint32_t code);
};

class CL_W10_WebConfig {
  private:
    AsyncWebServer _svr;

    CL_C10_Config* _cfg     = nullptr;
    bool (*_applyFn)(void*) = nullptr;
    void* _applyCtx         = nullptr;

    ST_C10_WiFiConfig_t _wifi;
    ST_C10_E10Config_t  _e10;

    bool _mdnsStarted = false;

    // (NEW) config 반영 후 재부팅 필요 플래그
    // - WiFi 설정(모드/SSID/비번/MDNS 등)은 런타임 반영이 제한적이므로
    //   save/import 이후 변경이 감지되면 웹 UI 배너용으로 true 설정
    bool _needReboot = false;

    // reboot reason mask (배너 상세 이유)
    // - 여러 원인이 동시에 발생할 수 있음(OR)
    uint32_t _needRebootMask = 0;


    // ----------------------------------------------------
    // (STEP17) Config apply report (UI/운영용)
    // - 마지막 설정 적용(save/import/apply/rollback)의 결과를 status로 노출
    // ----------------------------------------------------
    bool     _lastApplyOk   = true;
    uint32_t _lastApplyMs   = 0;      // millis()
    char     _lastApplyCode[32] = ""; // code (ok/error)
    char     _lastApplySrc[16]  = ""; // src: save/import/apply/rollback/factory

    // (요구) inline static
    inline static CL_W10_WebConfig* s_instance = nullptr;

    // ----------------------------------------------------
    // Reboot reason bits
    // ----------------------------------------------------
    static constexpr uint32_t G_W10_REBOOT_WIFI_MODE  = 0x00000001;
    static constexpr uint32_t G_W10_REBOOT_WIFI_STA   = 0x00000002;
    static constexpr uint32_t G_W10_REBOOT_WIFI_AP    = 0x00000004;
    static constexpr uint32_t G_W10_REBOOT_WIFI_MDNS  = 0x00000008;
    static constexpr uint32_t G_W10_REBOOT_OTHER      = 0x80000000;


    // (C) API schema version
    static constexpr uint16_t G_W10_API_VER = 304;


    // ----------------------------------------------------
    // POST body accumulator (고정 슬롯 + 고정 버퍼)
    // - ESPAsyncWebServer의 request body 콜백에서 (data,len,index,total) 단위로 누적
    // - unordered_map/String 누적을 제거하여 힙 단편화/메모리 폭주 위험을 낮춤
    // - 동시 POST가 극히 드물다는 전제에서, 2슬롯로 충분 (필요 시 확대)
    // ----------------------------------------------------
    struct ST_W10_BodySlot {
        AsyncWebServerRequest* req;
        size_t   len;
        uint32_t lastMs;
        char     buf[G_W10_BODY_MAX + 1];
    };

    static constexpr uint8_t G_W10_BODY_SLOTS = 4;// step19: increase POST body slots for concurrency
    inline static ST_W10_BodySlot s_bodySlots[G_W10_BODY_SLOTS];

    // (AB) observability counters
    uint32_t _cnt_body_too_large = 0;
    uint32_t _cnt_body_no_slot   = 0;
    uint32_t _cnt_json_bad       = 0;
    uint32_t _cnt_safe_blocked   = 0;
    uint32_t _cnt_ota_blocked    = 0;

    static void s_wifiEvent(WiFiEvent_t p_e, WiFiEventInfo_t p_info) {
        (void)p_info;
        if (s_instance) s_instance->_onWifiEvent(p_e);
    }

    // OTA state
    volatile bool     _otaInProgress = false;
    volatile uint32_t _otaTotal      = 0;
    volatile uint32_t _otaWritten    = 0;
    volatile bool     _otaOk         = false;
    char              _otaErr[64];

  public:
    CL_W10_WebConfig() : _svr(80) {
        s_instance = this;
        memset(&_wifi, 0, sizeof(_wifi));
        memset(&_e10, 0, sizeof(_e10));
        memset(_otaErr, 0, sizeof(_otaErr));
        strlcpy(_otaErr, "none", sizeof(_otaErr));
    }

    void begin(CL_C10_Config* p_cfg, bool (*p_applyFn)(void*), void* p_applyCtx) {
        _cfg      = p_cfg;
        _applyFn  = p_applyFn;
        _applyCtx = p_applyCtx;

        WiFi.onEvent(s_wifiEvent);
        // C10 config.begin()에서 초기화 (void)LittleFS.begin(true);

        if (_cfg) (void)_cfg->loadAll(_wifi, _e10);

        _setupWiFi();

        // ==============================
        // API 라우팅 (전부 no-store)
        // ==============================
        _svr.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* req) { _apiStatus(req); });
        _svr.on("/api/keycodes", HTTP_GET, [this](AsyncWebServerRequest* req) { apiKeycodes(req); });

        _svr.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* req) { apiGetConfig(req); });

        _svr.on(
            "/api/config/save",
            HTTP_POST,
            [this](AsyncWebServerRequest* req) { (void)req; },
            nullptr,
            [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
                apiConfigSave(req, data, len, index, total);
            });

        _svr.on(
            "/api/config/apply",
            HTTP_POST,
            [this](AsyncWebServerRequest* req) { (void)req; },
            nullptr,
            [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
                apiConfigApply(req, data, len, index, total);
            });

        _svr.on("/api/config/export", HTTP_GET, [this](AsyncWebServerRequest* req) { apiExport(req); });
        // (STEP12) alias for simpler client usage
        _svr.on("/api/export", HTTP_GET, [this](AsyncWebServerRequest* req) { apiExport(req); });

        _svr.on(
            "/api/config/import",
            HTTP_POST,
            [this](AsyncWebServerRequest* req) { (void)req; },
            nullptr,
            [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
                apiImport(req, data, len, index, total);
            });

        _svr.on(
            "/api/config/rollback",
            HTTP_POST,
            [this](AsyncWebServerRequest* req) { (void)req; },
            nullptr,
            [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
                (void)data;
                (void)len;
                (void)index;
                (void)total;
                apiRollback(req);
            });

        _svr.on(
            "/api/control",
            HTTP_POST,
            [this](AsyncWebServerRequest* req) { (void)req; },
            nullptr,
            [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
                apiControl(req, data, len, index, total);
            });

        // PPT
        _svr.on("/api/ppt", HTTP_GET, [this](AsyncWebServerRequest* req) { apiGetPpt(req); });
        _svr.on(
            "/api/ppt",
            HTTP_POST,
            [this](AsyncWebServerRequest* req) { (void)req; },
            nullptr,
            [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
                apiPostPpt(req, data, len, index, total);
            });

        _svr.on(
            "/api/ppt/test",
            HTTP_POST,
            [this](AsyncWebServerRequest* req) { (void)req; },
            nullptr,
            [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
                apiPptTest(req, data, len, index, total);
            });

        // OTA
        _svr.on("/api/ota/status", HTTP_GET, [this](AsyncWebServerRequest* req) { apiOtaStatus(req); });

        _svr.on(
            "/api/ota",
            HTTP_POST,
            [this](AsyncWebServerRequest* req) {
                                // 업로드 완료 후 응답 (C schema)
                JsonDocument v_doc;
                v_doc["written"] = (uint32_t)_otaWritten;
                v_doc["total"]   = (uint32_t)_otaTotal;
                if (_otaOk) _sendOk(req, "ota", "ok", &v_doc, 200);
                else        _sendErr(req, 500, "ota_failed", _otaErr, &v_doc);

                if (_otaOk) {
                    delay(200);
                    ESP.restart();
                }
            },
            [this](AsyncWebServerRequest* req, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
                apiOtaUpload(req, filename, index, data, len, final);
            });

        // SafeBoot / FactoryReset
        _svr.on("/api/safeboot", HTTP_GET, [this](AsyncWebServerRequest* req) { apiSafeBootGet(req); });
        _svr.on(
            "/api/safeboot",
            HTTP_POST,
            [this](AsyncWebServerRequest* req) { (void)req; },
            nullptr,
            [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
                apiSafeBootPost(req, data, len, index, total);
            });

        _svr.on(
            "/api/factory_reset",
            HTTP_POST,
            [this](AsyncWebServerRequest* req) { (void)req; },
            nullptr,
            [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
                (void)data;
                (void)len;
                (void)index;
                (void)total;
                apiFactoryReset(req);
            });

        _svr.on(
            "/api/reboot",
            HTTP_POST,
            [this](AsyncWebServerRequest* req) { (void)req; },
            nullptr,
            [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
                apiRebootPost(req, data, len, index, total);
            });

        // ==============================
        // 정적 라우팅(동적 서빙)
        // ==============================
        // 루트는 index로 유도(HTML은 no-store 정책)
        _svr.on("/", HTTP_GET, [this](AsyncWebServerRequest* req) { req->redirect(G_W10_DEFAULT_INDEX_PATH); });

        // NotFound에서 화이트리스트 기반 동적 서빙 처리
        _svr.onNotFound([this](AsyncWebServerRequest* req) { _handleDynamicStatic(req); });

        _svr.begin();
    }

  private:
    // =====================================================
    // WiFi
    // =====================================================
    void _setupWiFi() {
        WiFi.mode(WIFI_MODE_NULL);

        const bool v_hasSta   = (_wifi.sta_ssid[0] != '\0');
        const bool v_autoMode = (_wifi.mode == (uint8_t)EN_C10_WIFI_AUTO);
        const bool v_forceAp  = (_wifi.mode == (uint8_t)EN_C10_WIFI_AP);
        const bool v_forceSta = (_wifi.mode == (uint8_t)EN_C10_WIFI_STA);

        // SafeBoot: 무조건 AP
        if (_cfg && _cfg->isSafeMode()) {
            char v_ssid[33];
            memset(v_ssid, 0, sizeof(v_ssid));
            strlcpy(v_ssid, _wifi.ap_ssid, sizeof(v_ssid));
            if (strlen(v_ssid) <= 28) strlcat(v_ssid, "-SAFE", sizeof(v_ssid));

            WiFi.mode(WIFI_AP);
            if (_wifi.ap_pass[0] != '\0')
                WiFi.softAP(v_ssid, _wifi.ap_pass);
            else
                WiFi.softAP(v_ssid);
            return;
        }

        if (v_forceAp || (!v_hasSta && (v_autoMode || !v_forceSta))) {
            _startAp();
            return;
        }

        // STA try
        WiFi.mode(WIFI_STA);
        WiFi.begin(_wifi.sta_ssid, _wifi.sta_pass);

        const uint32_t v_t0 = millis();
        bool           v_ok = false;
        while (millis() - v_t0 < 8000) {
            if (WiFi.status() == WL_CONNECTED) {
                v_ok = true;
                break;
            }
            delay(200);
        }

        if (v_ok) {
            _startMdns();
            return;
        }

        // STA 실패 시 AUTO면 AP fallback
        if (v_autoMode) {
            _startAp();
            return;
        }

        // forceSta 실패면 그대로 유지(정책)
    }

    void _startAp() {
        WiFi.mode(WIFI_AP);
        // pass 비어있으면 open AP 허용(필요하면 정책 조정)
        if (_wifi.ap_pass[0] != '\0')
            WiFi.softAP(_wifi.ap_ssid, _wifi.ap_pass);
        else
            WiFi.softAP(_wifi.ap_ssid);
    }

    void _onWifiEvent(WiFiEvent_t p_e) {
        if (p_e == ARDUINO_EVENT_WIFI_STA_GOT_IP) _startMdns();
        if (p_e == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) _stopMdns();
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

    // =====================================================
    // Request Body (fixed slots)
    // =====================================================
    ST_W10_BodySlot* _bodySlotAlloc(AsyncWebServerRequest* req) {
        // 1) 기존 슬롯
        for (uint8_t i = 0; i < G_W10_BODY_SLOTS; i++) {
            if (s_bodySlots[i].req == req) return &s_bodySlots[i];
        }
        // 2) 빈 슬롯
        for (uint8_t i = 0; i < G_W10_BODY_SLOTS; i++) {
            if (s_bodySlots[i].req == nullptr) {
                s_bodySlots[i].req    = req;
                s_bodySlots[i].len    = 0;
                s_bodySlots[i].lastMs = millis();
                memset(s_bodySlots[i].buf, 0, sizeof(s_bodySlots[i].buf));
                return &s_bodySlots[i];
            }
        }
        // 3) 가장 오래된 슬롯 재사용(비정상/동시요청 대비)
        uint8_t  v_oldIdx = 0;
        uint32_t v_oldMs  = s_bodySlots[0].lastMs;
        for (uint8_t i = 1; i < G_W10_BODY_SLOTS; i++) {
            if (s_bodySlots[i].lastMs < v_oldMs) {
                v_oldMs  = s_bodySlots[i].lastMs;
                v_oldIdx = i;
            }
        }
        s_bodySlots[v_oldIdx].req    = req;
        s_bodySlots[v_oldIdx].len    = 0;
        s_bodySlots[v_oldIdx].lastMs = millis();
        memset(s_bodySlots[v_oldIdx].buf, 0, sizeof(s_bodySlots[v_oldIdx].buf));
        return &s_bodySlots[v_oldIdx];
    }

    ST_W10_BodySlot* _bodyGetSlot(AsyncWebServerRequest* req, size_t index) {
        if (index == 0) {
            // 새 요청 시작: 슬롯 할당/초기화
            return _bodySlotAlloc(req);
        }
        // 중간 청크: 기존 슬롯만 사용
        for (uint8_t i = 0; i < G_W10_BODY_SLOTS; i++) {
            if (s_bodySlots[i].req == req) return &s_bodySlots[i];
        }
        // 슬롯이 없으면 할당(예외)
        return _bodySlotAlloc(req);
    }

    void _bodyFree(AsyncWebServerRequest* req) {
        for (uint8_t i = 0; i < G_W10_BODY_SLOTS; i++) {
            if (s_bodySlots[i].req == req) {
                s_bodySlots[i].req = nullptr;
                s_bodySlots[i].len = 0;
                s_bodySlots[i].lastMs = 0;
                memset(s_bodySlots[i].buf, 0, sizeof(s_bodySlots[i].buf));
                return;
            }
        }
    }

    // =====================================================
    // gzip Accept
    // =====================================================
    bool _acceptsGzip(AsyncWebServerRequest* req) {
        if (!req->hasHeader("Accept-Encoding")) return false;
        const String v_ae = req->header("Accept-Encoding");
        return (v_ae.indexOf("gzip") >= 0);
    }

    // =====================================================
    // Dynamic static serving (화이트리스트 + 확장자 제한)
    // =====================================================
    // 허용:
    //  - /favicon.ico       => /www/favicon.ico
    //  - /robots.txt        => /www/robots.txt
    //  - /www/*             => (html/css/js/svg/png/webp/ico) only
    //  - /json/public/*     => (json) only
    // 차단:
    //  - 그 외 전부 404/403
    void _handleDynamicStatic(AsyncWebServerRequest* req) {
        const String v_uri = req->url();

        // API는 여기서 처리하지 않음(위에서 라우팅됨)
        // - 단, 미정의 API로 들어오면 JSON 표준 응답으로 404 반환
        if (v_uri.startsWith("/api/")) {
            _sendErr(req, 404, "api_not_found", "API endpoint not found.");
            return;
        }

        // favicon / robots (top-level only)
        if (v_uri == "/favicon.ico") {
            _serveWwwStatic(req, "/www/favicon.ico");
            return;
        }
        if (v_uri == "/robots.txt") {
            _serveWwwStatic(req, "/www/robots.txt");
            return;
        }

        // /www/*
        if (v_uri.startsWith(G_W10_URI_WWW_PREFIX)) {
            if (!W10_isPathSafe(v_uri.c_str())) {
                _sendStaticErr(req, 403, "static_forbidden", "forbidden");
                return;
            }
            _serveWwwStatic(req, v_uri.c_str());
            return;
        }

        // /json/public/*
        if (v_uri.startsWith(G_W10_URI_JSON_PUBLIC_PREFIX)) {
            if (!W10_isPathSafe(v_uri.c_str())) {
                _sendStaticErr(req, 403, "static_forbidden", "forbidden");
                return;
            }
            _servePublicJson(req, v_uri.c_str());
            return;
        }

        _sendStaticErr(req, 404, "static_not_found", "not found");
    }

    // -----------------------------------------------------
    // /www/* 서빙
    // - 확장자 화이트리스트
    // - gzip 대상(html/css/js) + .gz 존재 + Accept-Encoding:gzip => .gz 사용
    // - Cache-Control: W10_Def 규칙 적용
    // -----------------------------------------------------
    void _serveWwwStatic(AsyncWebServerRequest* req, const char* p_path) {
        if (!p_path) {
            _sendStaticErr(req, 404, "static_not_found", "not found");
            return;
        }

        char v_ext[12];
        memset(v_ext, 0, sizeof(v_ext));
        const char* v_extLower = W10_getLowerExt(p_path, v_ext, sizeof(v_ext));

        if (!v_extLower || !W10_isAllowedWwwExt(v_extLower)) {
            _sendStaticErr(req, 403, "static_forbidden", "forbidden");
            return;
        }

        bool   v_useGz = false;
        String v_gzPath;

        if (W10_isGzipTargetExt(v_extLower)) {
            v_gzPath = String(p_path) + ".gz";
            if (LittleFS.exists(v_gzPath) && _acceptsGzip(req)) {
                v_useGz = true;
            }
        }

        const char* v_sendPath = p_path;
        if (v_useGz) v_sendPath = v_gzPath.c_str();

        if (!LittleFS.exists(v_sendPath)) {
            _sendStaticErr(req, 404, "static_not_found", "not found");
            return;
        }

        const char*             v_ct = W10_contentTypeFromExt(v_extLower);
        AsyncWebServerResponse* res  = req->beginResponse(LittleFS, v_sendPath, v_ct);

        if (v_useGz) res->addHeader("Content-Encoding", "gzip");

        const char* v_cc = W10_cacheControlForStatic(p_path, v_extLower, false);
        res->addHeader("Cache-Control", v_cc);

        req->send(res);
    }

    // -----------------------------------------------------
    // /json/public/* 서빙 (json only, no-store)
    // -----------------------------------------------------
    void _servePublicJson(AsyncWebServerRequest* req, const char* p_path) {
        if (!p_path) {
            _sendStaticErr(req, 404, "static_not_found", "not found");
            return;
        }

        char v_ext[12];
        memset(v_ext, 0, sizeof(v_ext));
        const char* v_extLower = W10_getLowerExt(p_path, v_ext, sizeof(v_ext));

        if (!v_extLower || !W10_isAllowedPublicJsonExt(v_extLower)) {
            _sendStaticErr(req, 403, "static_forbidden", "forbidden");
            return;
        }

        if (!LittleFS.exists(p_path)) {
            _sendStaticErr(req, 404, "static_not_found", "not found");
            return;
        }

        AsyncWebServerResponse* res = req->beginResponse(LittleFS, p_path, "application/json");
        res->addHeader("Cache-Control", G_W10_CACHE_NOSTORE);
        req->send(res);
    }

    // =====================================================
    // Common JSON response helper (API는 무조건 no-store)
    // =====================================================

    // -----------------------
    // SafeMode API Gate Policy
    // - SafeMode에서도 "복구에 필요한 기능"은 허용
    // - 나머지는 차단 (특히 HID 제어/프리젠터 기능)
    // -----------------------
    bool _isSafeMode() const {
        return (_cfg && _cfg->isSafeMode());
    }
    
    bool _isApiAllowedInSafeMode(const char* p_uri) const {
        if (!p_uri) return false;
    
        // 항상 허용
        if (strcmp(p_uri, "/api/status") == 0) return true;
        if (strcmp(p_uri, "/api/keycodes") == 0) return true;
        if (strcmp(p_uri, "/api/safeboot") == 0) return true;
    
        // OTA + 복구
        if (strcmp(p_uri, "/api/ota/status") == 0) return true;
        if (strcmp(p_uri, "/api/ota") == 0) return true;
        if (strcmp(p_uri, "/api/import") == 0) return true;
        if (strcmp(p_uri, "/api/factory_reset") == 0) return true;
        if (strcmp(p_uri, "/api/reboot") == 0) return true;
    
        // config는 SafeMode에서도 허용 (복구/변경 필요)
        if (strcmp(p_uri, "/api/config/save") == 0) return true;
        if (strcmp(p_uri, "/api/config/apply") == 0) return true;
        if (strcmp(p_uri, "/api/config/export") == 0) return true;
        if (strcmp(p_uri, "/api/export") == 0) return true;
    
        // 그 외는 SafeMode에서는 막음
        return false;
    }

    void _sendJson(AsyncWebServerRequest* req, JsonDocument& d, int p_code = 200) {
        String out;
        serializeJson(d, out);
        AsyncWebServerResponse* res = req->beginResponse(p_code, "application/json", out);
        res->addHeader("Cache-Control", G_W10_CACHE_NOSTORE);
        req->send(res);
    }

    // =====================================================
    // (C) Standard API response helpers
    // - { ok, code, msg, data }
    // =====================================================
    void _sendOk(AsyncWebServerRequest* req, const char* p_code, const char* p_msg, JsonDocument* p_data = nullptr, int p_http = 200) {
        JsonDocument d;
        d["ok"]   = true;
        d["code"] = (p_code ? p_code : "ok");
        d["msg"]  = (p_msg  ? p_msg  : "");
        if (p_data) {
            d["data"] = (*p_data);
        }
        _sendJson(req, d, p_http);
    }

    void _sendErr(AsyncWebServerRequest* req, int p_http, const char* p_code, const char* p_msg, JsonDocument* p_data = nullptr) {
        JsonDocument d;
        d["ok"]   = false;
        d["code"] = (p_code ? p_code : "error");
        d["msg"]  = (p_msg  ? p_msg  : "");
        if (p_data) {
            d["data"] = (*p_data);
        }
        _sendJson(req, d, p_http);
    }

    // =====================================================
    // Static response helper (non-API)
    // - 정적 요청(/www/*, /json/public/*)에서 403/404 처리
    // - 기본은 text/plain 이지만, 클라이언트가 JSON을 원하면 표준 envelope로 응답
    // =====================================================
    bool _wantsJson(AsyncWebServerRequest* req) const {
        if (!req) return false;
        if (!req->hasHeader("Accept")) return false;
        AsyncWebHeader* h = req->getHeader("Accept");
        if (!h) return false;
        const String v = h->value();
        return (v.indexOf("application/json") >= 0);
    }

    void _sendStaticErr(AsyncWebServerRequest* req, int p_http, const char* p_code, const char* p_msg) {
        if (!req) return;
        if (_wantsJson(req)) {
            _sendErr(req, p_http, p_code, p_msg);
            return;
        }
        req->send(p_http, "text/plain", (p_msg ? p_msg : "error"));
    }

    // (STEP12) Envelope selector helper
    // - Query:
    //    * envelope=1    -> force envelope
    //    * envelope=0    -> force raw
    //    * envelope=auto -> decide by Accept header
    // - Header:
    //    * Accept: application/vnd.snw.envelope+json -> envelope
    // =====================================================
    bool _wantsEnvelope(AsyncWebServerRequest* req) {
        if (!req) return false;

        // query override
        if (req->hasParam("envelope")) {
            AsyncWebParameter* p = req->getParam("envelope");
            if (p) {
                const String v = p->value();
                if (v == "1") return true;
                if (v == "0") return false;
                if (v == "auto") {
                    // fallthrough to Accept
                } else {
                    // unknown -> default raw
                    return false;
                }
            }
        }

        // Accept based
        if (req->hasHeader("Accept")) {
            AsyncWebHeader* h = req->getHeader("Accept");
            if (h) {
                const String a = h->value();
                if (a.indexOf("application/vnd.snw.envelope+json") >= 0) return true;
            }
        }
        return false;
    }
    
    

    // =====================================================
    // Body Collector (fixed slots) - common for POST JSON APIs
    // - chunk 누적 후, 마지막에만 String으로 변환(1회)
    // - 에러 시 즉시 응답하고 slot 정리
    // =====================================================
    bool _collectBodyOrReply(AsyncWebServerRequest* req,
                             uint8_t* data, size_t len,
                             size_t index, size_t total,
                             String& p_outBody) {
        if (total > G_W10_BODY_MAX) {
            _cnt_body_too_large++;
            JsonDocument v_doc;
            v_doc["max"] = (uint32_t)G_W10_BODY_MAX;
            v_doc["total"] = (uint32_t)total;
            _sendErr(req, 413, "body_too_large", "Request body too large.", &v_doc);
            return false;
        }
    
        ST_W10_BodySlot* v_slot = _bodyGetSlot(req, index);
        if (!v_slot) {
            _cnt_body_no_slot++;
            _sendErr(req, 503, "no_body_slot", "Server is busy. Try again.");
            return false;
        }
    
        if ((v_slot->len + len) > G_W10_BODY_MAX) {
            _cnt_body_too_large++;
            JsonDocument v_doc;
            v_doc["max"] = (uint32_t)G_W10_BODY_MAX;
            v_doc["total"] = (uint32_t)total;
            _bodyFree(req);
            _sendErr(req, 413, "body_too_large", "Request body too large.", &v_doc);
            return false;
        }
    
        memcpy((void*)(v_slot->buf + v_slot->len), (const void*)data, len);
        v_slot->len += len;
        v_slot->lastMs = millis();
        v_slot->buf[v_slot->len] = '\0';
    
        // 아직 전체 바디를 못 모았으면, 여기서 종료(응답 없음)
        if (index + len < total) return false;
    
        // complete
        p_outBody = String(v_slot->buf);
        _bodyFree(req);
        return true;
    }
    
    
    // =====================================================
    // reboot reason helpers
    // =====================================================
    uint32_t _wifiDiffMask(const ST_C10_WiFiConfig_t& a, const ST_C10_WiFiConfig_t& b) {
        uint32_t m = 0;
        if (a.mode != b.mode) m |= G_W10_REBOOT_WIFI_MODE;
        if (strncmp(a.sta_ssid, b.sta_ssid, sizeof(a.sta_ssid)) != 0 ||
            strncmp(a.sta_pass, b.sta_pass, sizeof(a.sta_pass)) != 0) {
            m |= G_W10_REBOOT_WIFI_STA;
        }
        if (strncmp(a.ap_ssid, b.ap_ssid, sizeof(a.ap_ssid)) != 0 ||
            strncmp(a.ap_pass, b.ap_pass, sizeof(a.ap_pass)) != 0) {
            m |= G_W10_REBOOT_WIFI_AP;
        }
        if (strncmp(a.mdns_host, b.mdns_host, sizeof(a.mdns_host)) != 0) {
            m |= G_W10_REBOOT_WIFI_MDNS;
        }
        return m;
    }
    
    void _markNeedReboot(uint32_t reasonMask) {
        _needReboot = true;
        _needRebootMask |= (reasonMask ? reasonMask : G_W10_REBOOT_OTHER);
    }
    
    // (STEP17) 마지막 설정 적용 결과 기록
    void _markLastApply(bool ok, const char* src, const char* code) {
        _lastApplyOk = ok;
        _lastApplyMs = (uint32_t)millis();
        if (src) {
            strlcpy(_lastApplySrc, src, sizeof(_lastApplySrc));
        } else {
            _lastApplySrc[0] = '\0';
        }
        if (code) {
            strlcpy(_lastApplyCode, code, sizeof(_lastApplyCode));
        } else {
            _lastApplyCode[0] = '\0';
        }
    }
    
    String _rebootReasonsString(uint32_t m) {
        String s;
        auto add = [&](const char* t) {
            if (!s.isEmpty()) s += ",";
            s += t;
        };
        if (m & G_W10_REBOOT_WIFI_MODE) add("wifi_mode");
        if (m & G_W10_REBOOT_WIFI_STA)  add("wifi_sta");
        if (m & G_W10_REBOOT_WIFI_AP)   add("wifi_ap");
        if (m & G_W10_REBOOT_WIFI_MDNS) add("mdns");
        if (m & G_W10_REBOOT_OTHER)     add("other");
        return s;
    }
    // =====================================================
    // /api/config/save, /api/import 공통 처리
    // =====================================================
    void _apiConfigSaveImportCommon(AsyncWebServerRequest* req,
                                   uint8_t* data, size_t len,
                                   size_t index, size_t total,
                                   const char* p_note,
                                   bool p_applyAfterSave) {
        String v_body;
        if (!_collectBodyOrReply(req, data, len, index, total, v_body)) return;
    
        // 변경 감지를 위해 이전 WiFi snapshot 저장
        ST_C10_WiFiConfig_t v_prevWiFi = _wifi;
    
        bool v_saved = false;
        bool v_applied = false;
    
        bool v_ok = (_cfg && _cfg->importJson(v_body, v_saved, v_applied));
    
        // 저장 성공 시: 최신 config를 다시 로드하여 변경 감지
        if (v_ok && v_saved && _cfg) {
            (void)_cfg->loadAll(_wifi, _e10);
            uint32_t v_m = _wifiDiffMask(v_prevWiFi, _wifi);
            if (v_m != 0) {
                _markNeedReboot(v_m);
            }
        }
    
        if (v_ok && v_saved && p_applyAfterSave && _applyFn) {
            v_applied = _applyFn(_applyCtx);
        }
    
        JsonDocument v_doc;
        v_doc["saved"] = v_saved;
        v_doc["applied"] = v_applied;
        v_doc["note"] = (p_note ? p_note : "");
        if (v_ok) {
            _markLastApply(true, p_note ? p_note : "save", "config_save");
            _sendOk(req, "config_save", "", &v_doc, 200);
        } else {
            _markLastApply(false, p_note ? p_note : "save", "config_save_failed");
            _sendErr(req, 400, "config_save_failed", "Save/import failed.", &v_doc);
        }
    }

    void _fillE10StatusFromSnapshot(JsonObject e, const ST_E10_Status_t& s) {
        e["ble_connected"] = s.ble_connected;
        e["ppt_mode"]      = s.ppt_mode;
        e["dpi_level"]     = s.dpi_level;
    
        // (C10/E10 기준) precision_enable 필드 제거 (E10_Status_t에 없음)
        e["precision_mode"] = s.precision_mode;
    
        e["fsm_state"] = s.fsm_state;
        e["fsm_sub"]   = s.fsm_sub;
        e["btn_mask"]  = s.btn_mask;
    
        // ---- gates (E10_Status_t 기반) ----
        e["safe_mode"] = s.safe_mode;
    
        JsonObject gate = e["gate"].to<JsonObject>();
        gate["ota_guard"]           = s.ota_guard;
        gate["ota_guard_count"]     = (uint32_t)s.ota_guard_count;
        gate["ota_guard_uptime_ms"] = (uint32_t)s.ota_guard_uptime_ms;
    
        JsonObject h = e["health"].to<JsonObject>();
        h["state"] = s.health;
        h["score"] = s.health_score;
    
        JsonObject gyro = e["gyro"].to<JsonObject>();
        gyro["bias_x"] = s.gyro_bias_x;
        gyro["bias_y"] = s.gyro_bias_y;
        gyro["bias_z"] = s.gyro_bias_z;
        gyro["rms"]    = s.gyro_rms;
    
        e["cursor_rms"] = s.cursor_rms;
        e["temp_c"]     = s.temp_c;
    
        JsonObject samp = e["sampling"].to<JsonObject>();
        samp["ms_target"] = (uint32_t)s.sampling_ms_target;
        samp["ms_avg"]    = s.sampling_ms_avg;
    
        JsonObject i2c = e["i2c"].to<JsonObject>();
        i2c["recover_count"]   = (uint32_t)s.i2c_recover_count;
        i2c["recover_last_ok"] = s.i2c_recover_last_ok;
    
        JsonObject err = e["err"].to<JsonObject>();
        err["mpu_nan"]      = (uint32_t)s.err_mpu_nan;
        err["mutex_miss"]   = (uint32_t)s.err_mutex_miss;
        err["task_overrun"] = (uint32_t)s.err_task_overrun;
    
        JsonObject an = e["anomaly"].to<JsonObject>();
        an["spike_count_10s"]          = (uint32_t)s.spike_count_10s;
        an["consecutive_fail"]         = (uint32_t)s.consecutive_fail;
        an["consecutive_recover_fail"] = (uint32_t)s.consecutive_recover_fail;
    

        JsonObject obs = e["obs"].to<JsonObject>();
        obs["task_stack_sensor_min_words"] = (uint32_t)s.task_stack_sensor_min_words;
        obs["task_stack_comm_min_words"]   = (uint32_t)s.task_stack_comm_min_words;
        obs["sensor_dt_max_ms"]            = s.sensor_dt_max_ms;
        obs["sensor_overrun_count"]        = (uint32_t)s.sensor_overrun_count;
        obs["comm_dt_avg_ms"]              = s.comm_dt_avg_ms;
        obs["comm_dt_max_ms"]              = s.comm_dt_max_ms;
        obs["comm_overrun_count"]          = (uint32_t)s.comm_overrun_count;
        obs["failsafe_release_count"]      = (uint32_t)s.failsafe_release_count;

        JsonArray hist = e["err_hist"].to<JsonArray>();
        for (uint8_t i = 0; i < s.err_hist_n; i++) {
            JsonObject o = hist.add<JsonObject>();
            o["ts_ms"] = (uint32_t)s.err_hist[i].ts_ms;
            o["code"]  = (uint32_t)s.err_hist[i].code;
            o["value"] = (uint32_t)s.err_hist[i].value;
        }
    }

    void _fillE10Status(JsonObject e, ST_W10_E10If_t* e10if) {
        if (!e10if) return;
        ST_E10_Status_t s;
        if (!e10if->getStatus || !e10if->getStatus(e10if->ctx, &s)) return;
        _fillE10StatusFromSnapshot(e, s);
    }


    // =====================================================
    // /api/status
    // =====================================================
    void _apiStatus(AsyncWebServerRequest* req) {
        JsonDocument data;
        const uint32_t v_uptime = (uint32_t)millis();
        const uint32_t v_heapFree = (uint32_t)ESP.getFreeHeap();
        const uint32_t v_heapMin  = (uint32_t)ESP.getMinFreeHeap();
        const uint32_t v_heapMaxA = (uint32_t)ESP.getMaxAllocHeap();

        // flat(legacy)
        data["uptime_ms"]     = v_uptime;
        data["heap_free"]     = v_heapFree;
        data["heap_min_free"] = v_heapMin;
        data["heap_max_alloc"] = v_heapMaxA;
        data["api_ver"]       = (uint16_t)G_W10_API_VER;

        // grouped(UI-friendly)
        JsonObject sys = data["sys"].to<JsonObject>();
        sys["uptime_ms"] = v_uptime;
        sys["api_ver"]   = (uint16_t)G_W10_API_VER;

        JsonObject mem = data["mem"].to<JsonObject>();
        mem["heap_free"]      = v_heapFree;
        mem["heap_min_free"]  = v_heapMin;
        mem["heap_max_alloc"] = v_heapMaxA;

        JsonObject feat = data["features"].to<JsonObject>();
        feat["etag_config"] = true;
        feat["reboot_api"]  = true;
        feat["safe_mode_policy"] = true;
        feat["ota_guard"] = true;
        feat["e10_observability"] = true;

        JsonObject diag = data["diag"].to<JsonObject>();
        diag["body_too_large"] = (uint32_t)_cnt_body_too_large;
        diag["body_no_slot"]   = (uint32_t)_cnt_body_no_slot;
        diag["json_bad"]       = (uint32_t)_cnt_json_bad;
        diag["safe_blocked"]   = (uint32_t)_cnt_safe_blocked;
        diag["ota_blocked"]    = (uint32_t)_cnt_ota_blocked;

        JsonObject net = data["net"].to<JsonObject>();
        net["mode"]    = (WiFi.getMode() == WIFI_AP) ? "AP" : "STA";
        net["ip"]      = (WiFi.getMode() == WIFI_AP) ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
        net["ssid"]    = (WiFi.getMode() == WIFI_AP) ? String(_wifi.ap_ssid) : WiFi.SSID();
        net["mdns"]    = String(_wifi.mdns_host) + ".local";

        // E10 status snapshot
        bool v_otaGuard = false;
        ST_W10_E10If_t* e10if = (ST_W10_E10If_t*)_applyCtx;
        if (e10if) {
            ST_E10_Status_t s;
            if (e10if->getStatus && e10if->getStatus(e10if->ctx, &s)) {
                JsonObject e = data["e10"].to<JsonObject>();
                _fillE10StatusFromSnapshot(e, s);
                // policy 요약(배너용)
                v_otaGuard = s.ota_guard;
            }
        }

        JsonObject ota     = data["ota"].to<JsonObject>();
        ota["in_progress"] = _otaInProgress;
        ota["total"]       = (uint32_t)_otaTotal;
        ota["written"]     = (uint32_t)_otaWritten;
        ota["ok"]          = _otaOk;
        ota["err"]         = _otaErr;

        if (_cfg) {
            ST_C10_BootState_t bs;
            _cfg->getBootState(bs);
            JsonObject b    = data["boot"].to<JsonObject>();
            b["safe_mode"]  = bs.safe_mode;
            b["fail_count"] = bs.fail_count;
            b["pending"]    = bs.pending;
            b["last_reset_reason"] = bs.last_reset_reason;

            // 정책/배너 노출
            JsonObject pol = data["policy"].to<JsonObject>();
            pol["reboot_required"]       = _needReboot;
            pol["reboot_reason_mask"]    = (uint32_t)_needRebootMask;
            pol["reboot_reasons"]        = _rebootReasonsString(_needRebootMask);
            pol["reboot_reason_detail"]  = _rebootReasonsDetail(_needRebootMask, _needReboot);
            pol["safe_mode_api_limited"] = bs.safe_mode;
            pol["ota_upload_blocked"]    = v_otaGuard;

            // config etag (UI 캐시/갱신용)
            uint32_t v_etag = 0;
            size_t   v_cfgSize = 0;
            bool v_etagOk = _cfg->getConfigEtag(v_etag, &v_cfgSize);
            JsonObject cfg = data["config"].to<JsonObject>();
            cfg["ver"]  = (uint16_t)G_C10_CFG_VER;
            cfg["etag_ok"] = v_etagOk;
            cfg["etag"] = (uint32_t)v_etag;
            cfg["size"] = (uint32_t)v_cfgSize;
            // (STEP17) last apply report
            cfg["last_apply_ok"]   = _lastApplyOk;
            cfg["last_apply_ms"]   = _lastApplyMs;
            cfg["last_apply_age_ms"] = (_lastApplyMs == 0) ? (uint32_t)0 : (uint32_t)((uint32_t)v_uptime - (uint32_t)_lastApplyMs);
            cfg["last_apply_code"] = _lastApplyCode;
            cfg["last_apply_src"]  = _lastApplySrc;
        }

        // groups(UI-friendly, single root)
        JsonObject groups = data["groups"].to<JsonObject>();
        {
            JsonObject gSys = groups["sys"].to<JsonObject>();
            gSys["uptime_ms"] = v_uptime;
            gSys["api_ver"]   = (uint16_t)G_W10_API_VER;

            JsonObject gMem = groups["mem"].to<JsonObject>();
            gMem["heap_free"]      = v_heapFree;
            gMem["heap_min_free"]  = v_heapMin;
            gMem["heap_max_alloc"] = v_heapMaxA;

            JsonObject gNet = groups["net"].to<JsonObject>();
            gNet["mode"] = net["mode"];
            gNet["ip"]   = net["ip"];
            gNet["ssid"] = net["ssid"];
            gNet["mdns"] = net["mdns"];

            JsonObject gDiag = groups["diag"].to<JsonObject>();
            gDiag["body_too_large"] = diag["body_too_large"];
            gDiag["body_no_slot"]   = diag["body_no_slot"];
            gDiag["json_bad"]       = diag["json_bad"];
            gDiag["safe_blocked"]   = diag["safe_blocked"];
            gDiag["ota_blocked"]    = diag["ota_blocked"];

            if (_cfg) {
                JsonObject gPol = groups["policy"].to<JsonObject>();
                JsonObject pol = data["policy"].as<JsonObject>();
                gPol["reboot_required"]       = pol["reboot_required"];
                gPol["reboot_reason_mask"]    = pol["reboot_reason_mask"];
                gPol["reboot_reasons"]        = pol["reboot_reasons"];
                gPol["reboot_reason_detail"]  = pol["reboot_reason_detail"];
                gPol["safe_mode_api_limited"] = pol["safe_mode_api_limited"];
                gPol["ota_upload_blocked"]    = pol["ota_upload_blocked"];

                JsonObject gBoot = groups["boot"].to<JsonObject>();
                JsonObject boot = data["boot"].as<JsonObject>();
                gBoot["safe_mode"] = boot["safe_mode"];
                gBoot["fail_count"] = boot["fail_count"];
                gBoot["pending"] = boot["pending"];
                gBoot["last_reset_reason"] = boot["last_reset_reason"];

                JsonObject gCfg = groups["config"].to<JsonObject>();
                JsonObject cfg = data["config"].as<JsonObject>();
                gCfg["ver"] = cfg["ver"];
                gCfg["etag_ok"] = cfg["etag_ok"];
                gCfg["etag"] = cfg["etag"];
                gCfg["size"] = cfg["size"];
                gCfg["last_apply_ok"]   = cfg["last_apply_ok"];
                gCfg["last_apply_ms"]   = cfg["last_apply_ms"];
                gCfg["last_apply_age_ms"] = cfg["last_apply_age_ms"];
                gCfg["last_apply_code"] = cfg["last_apply_code"];
                gCfg["last_apply_src"]  = cfg["last_apply_src"];
            }

            JsonObject gOta = groups["ota"].to<JsonObject>();
            gOta["in_progress"] = ota["in_progress"];
            gOta["total"] = ota["total"];
            gOta["written"] = ota["written"];
            gOta["ok"] = ota["ok"];
            gOta["err"] = ota["err"];

            // e10 group (optional)
            JsonVariant e10v = data["e10"];
            if (!e10v.isNull()) {
                JsonObject gE10 = groups["e10"].to<JsonObject>();
                JsonObject e10 = e10v.as<JsonObject>();

                JsonVariant v;
                v = e10["connected"]; if (!v.isNull()) gE10["connected"] = v;
                v = e10["mode"]; if (!v.isNull()) gE10["mode"] = v;
                v = e10["dpi"]; if (!v.isNull()) gE10["dpi"] = v;
                v = e10["precision"]; if (!v.isNull()) gE10["precision"] = v;
                v = e10["ota_guard"]; if (!v.isNull()) gE10["ota_guard"] = v;

                // observability fields (may exist)
                v = e10["task_stack_sensor_min_words"]; if (!v.isNull()) gE10["task_stack_sensor_min_words"] = v;
                v = e10["task_stack_comm_min_words"]; if (!v.isNull()) gE10["task_stack_comm_min_words"] = v;
                v = e10["sensor_dt_max_ms"]; if (!v.isNull()) gE10["sensor_dt_max_ms"] = v;
                v = e10["comm_dt_avg_ms"]; if (!v.isNull()) gE10["comm_dt_avg_ms"] = v;
                v = e10["comm_dt_max_ms"]; if (!v.isNull()) gE10["comm_dt_max_ms"] = v;
                v = e10["sensor_overrun_count"]; if (!v.isNull()) gE10["sensor_overrun_count"] = v;
                v = e10["comm_overrun_count"]; if (!v.isNull()) gE10["comm_overrun_count"] = v;
                v = e10["failsafe_release_count"]; if (!v.isNull()) gE10["failsafe_release_count"] = v;
            }
        }


        _sendOk(req, "status", "", &data, 200);
    }

    // =====================================================
    // /api/keycodes
    // =====================================================
    const char* _kbName(uint16_t p_code) {
        switch (p_code) {
            case 0x00:
                return "None";
            case 0x04:
                return "A";
            case 0x05:
                return "B";
            case 0x06:
                return "C";
            case 0x07:
                return "D";
            case 0x08:
                return "E";
            case 0x09:
                return "F";
            case 0x0A:
                return "G";
            case 0x0B:
                return "H";
            case 0x0C:
                return "I";
            case 0x0D:
                return "J";
            case 0x0E:
                return "K";
            case 0x0F:
                return "L";
            case 0x10:
                return "M";
            case 0x11:
                return "N";
            case 0x12:
                return "O";
            case 0x13:
                return "P";
            case 0x14:
                return "Q";
            case 0x15:
                return "R";
            case 0x16:
                return "S";
            case 0x17:
                return "T";
            case 0x18:
                return "U";
            case 0x19:
                return "V";
            case 0x1A:
                return "W";
            case 0x1B:
                return "X";
            case 0x1C:
                return "Y";
            case 0x1D:
                return "Z";
            case 0x1E:
                return "1";
            case 0x1F:
                return "2";
            case 0x20:
                return "3";
            case 0x21:
                return "4";
            case 0x22:
                return "5";
            case 0x23:
                return "6";
            case 0x24:
                return "7";
            case 0x25:
                return "8";
            case 0x26:
                return "9";
            case 0x27:
                return "0";
            case 0x28:
                return "Enter";
            case 0x29:
                return "Esc";
            case 0x2A:
                return "Backspace";
            case 0x2B:
                return "Tab";
            case 0x2C:
                return "Space";
            case 0x3A:
                return "F1";
            case 0x3B:
                return "F2";
            case 0x3C:
                return "F3";
            case 0x3D:
                return "F4";
            case 0x3E:
                return "F5";
            case 0x3F:
                return "F6";
            case 0x40:
                return "F7";
            case 0x41:
                return "F8";
            case 0x42:
                return "F9";
            case 0x43:
                return "F10";
            case 0x44:
                return "F11";
            case 0x45:
                return "F12";
            case 0x4B:
                return "PageUp";
            case 0x4E:
                return "PageDown";
            case 0x4F:
                return "Right";
            case 0x50:
                return "Left";
            case 0x51:
                return "Down";
            case 0x52:
                return "Up";
            default:
                return nullptr;
        }
    }

    void apiKeycodes(AsyncWebServerRequest* req) {
        JsonDocument d;

        JsonArray mods = d["mods"].to<JsonArray>();
        for (size_t i = 0; i < sizeof(G_W10_MODS) / sizeof(G_W10_MODS[0]); i++) {
            JsonObject o = mods.add<JsonObject>();
            o["name"]    = G_W10_MODS[i].name;
            o["mask"]    = G_W10_MODS[i].mask;
        }

        JsonArray kb = d["kb"].to<JsonArray>();
        char      nameBuf[8];
        for (uint16_t code = 0; code <= 0xE7; code++) {
            const char* n = _kbName(code);
            if (!n) {
                snprintf(nameBuf, sizeof(nameBuf), "0x%02X", (unsigned)code);
                n = nameBuf;
            }
            JsonObject o = kb.add<JsonObject>();
            o["name"]    = n;
            o["code"]    = code;
        }

        JsonArray con = d["consumer"].to<JsonArray>();
        for (size_t i = 0; i < sizeof(G_W10_CONSUMER) / sizeof(G_W10_CONSUMER[0]); i++) {
            JsonObject o = con.add<JsonObject>();
            o["name"]    = G_W10_CONSUMER[i].name;
            o["mask"]    = (uint32_t)G_W10_CONSUMER[i].mask;
        }
        
        // ---------------------------
        // precision modes (owned by C10)
        // ---------------------------
        JsonArray pm = d["precision_modes"].to<JsonArray>();
        for (uint8_t m = 0; m < (uint8_t)EN_C10_E10_PREC_MAX; m++) {
            JsonObject o = pm.add<JsonObject>();
            const char* n = _precModeName(m);
            if (!n) n = "unknown";
            o["name"]  = n;
            o["value"] = m;
        }


        d["note"] = "mods mask == HID modifier byte. kb=usage-id(0x07), consumer=32-bit mask. precision_modes owned by C10.";
     
        _sendOk(req, "keycodes", "", &d, 200);
    }
    
    const char* _precModeName(uint8_t p_mode) {
        switch (p_mode) {
            case (uint8_t)EN_C10_E10_PREC_OFF:  return "OFF";
            case (uint8_t)EN_C10_E10_PREC_LOW:  return "LOW";
            case (uint8_t)EN_C10_E10_PREC_MED:  return "MED";
            case (uint8_t)EN_C10_E10_PREC_HIGH: return "HIGH";
            case (uint8_t)EN_C10_E10_PREC_PPT:  return "PPT";
            default: return "UNKNOWN";
        }
    }


    // =====================================================
    // /api/config
    // =====================================================
    
    void apiGetConfig(AsyncWebServerRequest* req) {
        // (STEP12) Optional envelope response selector
        // - default: raw JSON (backward compatible)
        // - envelope=1 / Accept: application/vnd.snw.envelope+json / envelope=auto
        const bool v_envelope = _wantsEnvelope(req);
    
        // ETag (config file hash) 지원: UI 캐시/갱신 안정화
        uint32_t v_etag = 0;
        size_t   v_size = 0;
        bool     v_hasEtag = (_cfg && _cfg->getConfigEtag(v_etag, &v_size));
    
        if (v_hasEtag) {
            // If-None-Match 지원 (간단 비교)
            if (req->hasHeader("If-None-Match")) {
                AsyncWebHeader* h = req->getHeader("If-None-Match");
                if (h) {
                    String v_inm = h->value();
                    char   v_tag[16];
                    snprintf(v_tag, sizeof(v_tag), "%08X", (unsigned int)v_etag);
                    if (v_inm == String(v_tag)) {
                        AsyncWebServerResponse* res304 = req->beginResponse(304);
                        res304->addHeader("ETag", v_tag);
                        res304->addHeader("Cache-Control", G_W10_CACHE_NOCACHE);
                        req->send(res304);
                        return;
                    }
                }
            }
        }
    
        String json;
        if (!_cfg || !_cfg->exportJson(json)) {
            _sendErr(req, 500, "config_get_failed", "Failed to export config.");
            return;
        }
    
        // (STEP11) envelope response uses stream to avoid double-encoding huge JSON
        if (v_envelope) {
            AsyncResponseStream* res = req->beginResponseStream("application/json");
            res->addHeader("Cache-Control", G_W10_CACHE_NOSTORE);
            if (v_hasEtag) {
                char v_tag[16];
                snprintf(v_tag, sizeof(v_tag), "%08X", (unsigned int)v_etag);
                res->addHeader("ETag", v_tag);
                res->addHeader("X-Config-Size", String((unsigned int)v_size));
            }
    
            // { ok, code, msg, data:{ etag, size, config:<raw> } }
            res->print("{\"ok\":true,\"code\":\"config_get\",\"msg\":\"\",\"data\":{");
    
            if (v_hasEtag) {
                char v_tag2[16];
                snprintf(v_tag2, sizeof(v_tag2), "%08X", (unsigned int)v_etag);
                res->print("\"etag\":\"");
                res->print(v_tag2);
                res->print("\",");
                res->print("\"size\":");
                res->print((unsigned int)v_size);
                res->print(",");
            }
    
            res->print("\"config\":");
            res->print(json);
            res->print("}}");
            req->send(res);
            return;
        }
    
        // default: raw JSON response (backward compatible)
        AsyncWebServerResponse* res = req->beginResponse(200, "application/json", json);
        res->addHeader("Cache-Control", G_W10_CACHE_NOSTORE);
        if (v_hasEtag) {
            char v_tag[16];
            snprintf(v_tag, sizeof(v_tag), "%08X", (unsigned int)v_etag);
            res->addHeader("ETag", v_tag);
            res->addHeader("X-Config-Size", String((unsigned int)v_size));
        }
        req->send(res);
    }


    
    void apiConfigSave(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
        _apiConfigSaveImportCommon(req, data, len, index, total,
                                   "WiFi changes require reboot.",
                                   true);
    }

    
    void apiConfigApply(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
        String v_body;
        if (!_collectBodyOrReply(req, data, len, index, total, v_body)) return;
    
        bool v_ok      = true;
        bool v_applied = false;
    
        if (!_cfg) {
            v_ok = false;
        } else {
            ST_C10_E10Config_t v_e;
            v_ok = _cfg->buildPatchedE10(v_body, v_e, true);
    
            ST_W10_E10If_t* v_e10if = (ST_W10_E10If_t*)_applyCtx;
            if (v_ok && v_e10if && v_e10if->applyRuntimeE10) {
                v_applied = v_e10if->applyRuntimeE10(v_e10if->ctx, &v_e);
            }
        }
    
        JsonDocument v_doc;
        v_doc["applied"] = v_applied;
        v_doc["note"] = "apply-only: not saved. WiFi fields are ignored (E10 only).";
        if (v_ok) {
            _markLastApply(true, "apply", "config_apply");
            _sendOk(req, "config_apply", "", &v_doc, 200);
        } else {
            _markLastApply(false, "apply", "config_apply_failed");
            _sendErr(req, 400, "config_apply_failed", "Apply failed.", &v_doc);
        }
    }

    void apiExport(AsyncWebServerRequest* req) {
        // (STEP12) export supports raw/envelope and attachment toggle
        const bool v_envelope = _wantsEnvelope(req);

        // (STEP13) format selector:
        // - format=pretty  -> pretty JSON output
        // - format=minified(default) -> minified JSON output
        // - pretty=1 -> same as format=pretty
        bool v_pretty = false;
        if (req && req->hasParam("pretty")) {
            AsyncWebParameter* p = req->getParam("pretty");
            if (p && p->value() == "1") v_pretty = true;
        }
        if (req && req->hasParam("format")) {
            AsyncWebParameter* p = req->getParam("format");
            if (p) {
                const String v = p->value();
                if (v == "pretty") v_pretty = true;
                else if (v == "minified") v_pretty = false;
            }
        }

        bool v_attach = true;
        if (req && req->hasParam("attachment")) {
            AsyncWebParameter* p = req->getParam("attachment");
            if (p && p->value() == "0") v_attach = false;
        }
        if (req && req->hasParam("download")) {
            AsyncWebParameter* p = req->getParam("download");
            if (p && p->value() == "0") v_attach = false;
        }

        String v_filename;
        if (req && req->hasParam("filename")) {
            AsyncWebParameter* p = req->getParam("filename");
            if (p) v_filename = p->value();
        }
        if (v_filename.length() == 0) {
            char buf[32];
            snprintf(buf, sizeof(buf), "config_%u.json", (unsigned int)G_C10_CFG_VER);
            v_filename = String(buf);
        }

        // ETag (config file hash)
        uint32_t v_etag = 0;
        size_t   v_size = 0;
        bool     v_hasEtag = (_cfg && _cfg->getConfigEtag(v_etag, &v_size));

        if (v_hasEtag && req && req->hasHeader("If-None-Match")) {
            AsyncWebHeader* h = req->getHeader("If-None-Match");
            if (h) {
                String v_inm = h->value();
                char   v_tag[16];
                snprintf(v_tag, sizeof(v_tag), "%08X", (unsigned int)v_etag);
                if (v_inm == String(v_tag)) {
                    AsyncWebServerResponse* res304 = req->beginResponse(304);
                    res304->addHeader("ETag", v_tag);
                    res304->addHeader("Cache-Control", G_W10_CACHE_NOCACHE);
                    req->send(res304);
                    return;
                }
            }
        }

        String json;
        if (!_cfg || !_cfg->exportJson(json)) {
            _sendErr(req, 500, "config_export_failed", "Failed to export config.");
            return;
        }

        if (v_pretty) {
            JsonDocument vd;
            DeserializationError verr = deserializeJson(vd, json);
            if (!verr) {
                String v_out;
                serializeJsonPretty(vd, v_out);
                json = v_out;
            }
        }

        if (v_envelope) {
            // envelope JSON (no attachment by default)
            AsyncResponseStream* res = req->beginResponseStream("application/json");
            res->addHeader("Cache-Control", G_W10_CACHE_NOSTORE);
            if (v_hasEtag) {
                char v_tag[16];
                snprintf(v_tag, sizeof(v_tag), "%08X", (unsigned int)v_etag);
                res->addHeader("ETag", v_tag);
                res->addHeader("X-Config-Size", String((unsigned int)v_size));
            }
            if (v_attach) {
                res->addHeader("Content-Disposition", String("attachment; filename=\"") + v_filename + "\"");
            }

            res->print("{\"ok\":true,\"code\":\"config_export\",\"msg\":\"\",\"data\":{");
            res->print("\"filename\":\"");
            res->print(v_filename);
            res->print("\",");
            if (v_hasEtag) {
                char v_tag2[16];
                snprintf(v_tag2, sizeof(v_tag2), "%08X", (unsigned int)v_etag);
                res->print("\"etag\":\"");
                res->print(v_tag2);
                res->print("\",");
                res->print("\"size\":");
                res->print((unsigned int)v_size);
                res->print(",");
            }
            res->print("\"config\":");
            res->print(json);
            res->print("}}");
            req->send(res);
            return;
        }

        // raw JSON (attachment optional)
        AsyncWebServerResponse* res = req->beginResponse(200, "application/json", json);
        if (v_attach) {
            res->addHeader("Content-Disposition", String("attachment; filename=\"") + v_filename + "\"");
        }
        res->addHeader("Cache-Control", G_W10_CACHE_NOSTORE);
        if (v_hasEtag) {
            char v_tag[16];
            snprintf(v_tag, sizeof(v_tag), "%08X", (unsigned int)v_etag);
            res->addHeader("ETag", v_tag);
            res->addHeader("X-Config-Size", String((unsigned int)v_size));
        }
        req->send(res);
    }

    
    void apiImport(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
        _apiConfigSaveImportCommon(req, data, len, index, total,
                                   "import: saved and applied (runtime). WiFi changes require reboot.",
                                   true);
    }

    void apiRollback(AsyncWebServerRequest* req) {
        bool ok      = (_cfg && _cfg->rollbackFromBak());
        bool applied = false;
        if (ok && _applyFn) applied = _applyFn(_applyCtx);

        JsonDocument data;
        data["applied"] = applied;
        if (ok) {
            _markLastApply(true, "rollback", "config_rollback");
            _sendOk(req, "config_rollback", "", &data, 200);
        } else {
            _markLastApply(false, "rollback", "config_rollback_failed");
            _sendErr(req, 400, "config_rollback_failed", "Rollback failed.", &data);
        }
    }

    // =====================================================
    // /api/control
    // =====================================================
    void apiControl(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {

        if (_isSafeMode() && !_isApiAllowedInSafeMode(req->url().c_str())) {
                    _cnt_safe_blocked++;
                    _sendErr(req, 403, "safe_mode_blocked", "Blocked in safe mode.");
                    return;
                }
        String v_body;
        if (!_collectBodyOrReply(req, data, len, index, total, v_body)) return;
        
        JsonDocument         d;
        DeserializationError err = deserializeJson(d, v_body);
        if (err) {
                    _cnt_json_bad++;
                    _sendErr(req, 400, "bad_json", "Invalid JSON.");
                    return;
        }


        ST_W10_E10If_t* e10if = (ST_W10_E10If_t*)_applyCtx;
        bool ok = true;

        // D+ 옵션: snapshot
        const bool v_snapshot = (!d["snapshot"].isNull()) ? (bool)d["snapshot"] : false;

        // D+ 옵션: cmd 우선 처리
        const char* v_cmd = nullptr;
        if (!d["cmd"].isNull()) v_cmd = (const char*)d["cmd"];

        if (!e10if) ok = false;

        if (ok && v_cmd && v_cmd[0] != '\0') {
            // ---- cmd mode ----
            if (strcmp(v_cmd, "set_ppt") == 0) {
                bool v_en = false;
                if (!d["enable"].isNull()) v_en = (bool)d["enable"];
                ok = ok && (e10if && e10if->setPptMode ? e10if->setPptMode(e10if->ctx, v_en) : false);

            } else if (strcmp(v_cmd, "set_dpi") == 0) {
                uint8_t v_lv = 2;
                if (!d["level"].isNull()) v_lv = (uint8_t)d["level"];
                ok = ok && (e10if && e10if->setDpiLevel ? e10if->setDpiLevel(e10if->ctx, v_lv) : false);
                
            } else if (strcmp(v_cmd, "set_precision") == 0) {

                uint8_t v_mode = (uint8_t)EN_C10_E10_PREC_OFF;
                if (!d["mode"].isNull()) v_mode = (uint8_t)d["mode"];
            
                // (C10 contract) invalid -> reject
                if (v_mode >= (uint8_t)EN_C10_E10_PREC_MAX) {
                    ok = false;
                } else {
                    ok = ok && (e10if && e10if->setPrecisionMode ? e10if->setPrecisionMode(e10if->ctx, v_mode) : false);
                }
                
            /*
            } else if (strcmp(v_cmd, "set_precision") == 0) {
                
                uint8_t v_mode = 0;
                // 1) mode 우선 (0이면 OFF, 그 외 ON)
                if (!d["mode"].isNull()) {
                    v_mode = (uint8_t)d["mode"];
                }
            
                ok = ok && (e10if && e10if->setPrecisionMode ? e10if->setPrecisionMode(e10if->ctx, v_mode) : false);
            */

            } else if (strcmp(v_cmd, "force_release") == 0) {
                // SafeMode에서도 허용: 강제 릴리즈
                ok = ok && (e10if && e10if->forceReleaseButtons ? e10if->forceReleaseButtons(e10if->ctx) : false);

            } else if (strcmp(v_cmd, "gyro_calib") == 0) {
                ok = ok && (e10if && e10if->requestGyroCalibration ? e10if->requestGyroCalibration(e10if->ctx) : false);

            } else if (strcmp(v_cmd, "i2c_recover") == 0) {
                ok = ok && (e10if && e10if->requestI2CRecover ? e10if->requestI2CRecover(e10if->ctx) : false);

            } else if (strcmp(v_cmd, "clear_diag") == 0) {
                ok = ok && (e10if && e10if->clearDiagnostics ? e10if->clearDiagnostics(e10if->ctx) : false);

            } else if (strcmp(v_cmd, "set_safe_mode") == 0) {
                bool v_en = false;
                if (!d["enable"].isNull()) v_en = (bool)d["enable"];
                ok = ok && (e10if && e10if->setSafeMode ? e10if->setSafeMode(e10if->ctx, v_en) : false);
            } else if (strcmp(v_cmd, "set_ota_guard") == 0) {
                bool v_en = false;
                if (!d["enable"].isNull()) v_en = (bool)d["enable"];
                ok = ok && (e10if && e10if->setOtaGuard ? e10if->setOtaGuard(e10if->ctx, v_en) : false);
    

            } else {
                ok = false;
            }

        } else if (ok) {
            // ---- legacy field mode (기존 호환 유지) ----
            if (!d["ppt_mode"].isNull())  ok = ok && (e10if && e10if->setPptMode ? e10if->setPptMode(e10if->ctx, (bool)d["ppt_mode"]) : false);
            if (!d["dpi_level"].isNull()) ok = ok && (e10if && e10if->setDpiLevel ? e10if->setDpiLevel(e10if->ctx, (uint8_t)d["dpi_level"]) : false);
            
            if (!d["precision_mode"].isNull()) {
                uint8_t v_mode = (uint8_t)d["precision_mode"];
                if (v_mode >= (uint8_t)EN_C10_E10_PREC_MAX) ok = false;
                else ok = ok && (e10if && e10if->setPrecisionMode ? e10if->setPrecisionMode(e10if->ctx, v_mode) : false);
            }
            // if (!d["precision_mode"].isNull())  ok = ok && (e10if && e10if->setPrecisionMode ? e10if->setPrecisionMode(e10if->ctx, (uint8_t) : false)d["precision_mode"]);
            if (!d["safe_mode"].isNull())       ok = ok && (e10if && e10if->setSafeMode ? e10if->setSafeMode(e10if->ctx, (bool)d["safe_mode"]) : false);
        }

        JsonDocument data;
        data["cmd"] = (v_cmd ? v_cmd : "");
        if (v_snapshot && e10if) {
            JsonObject e = data["e10"].to<JsonObject>();
            _fillE10Status(e, e10if);
        }
        if (ok) _sendOk(req, "control", "", &data, 200);
        else    _sendErr(req, 400, "control_failed", "Control failed.", &data);
    }
    
    // =====================================================
    // /api/ppt
    // =====================================================
    void apiGetPpt(AsyncWebServerRequest* req) {
        if (_cfg) (void)_cfg->loadAll(_wifi, _e10);

        JsonDocument d;
        JsonObject   map = d["map"].to<JsonObject>();

        auto put = [&](const char* n, const ST_C10_PptKey2_t& k) {
            JsonObject o = map[n].to<JsonObject>();
            o["page"]    = (k.page == (uint8_t)EN_C10_KEYPAGE_CONSUMER) ? "consumer" : "kb";
            o["mod"]     = k.mod;
            o["code"]    = k.code;
        };

        put("start", _e10.ppt2_start);
        put("exit", _e10.ppt2_exit);
        put("next", _e10.ppt2_next);
        put("prev", _e10.ppt2_prev);
        put("black", _e10.ppt2_black);
        put("laser", _e10.ppt2_laser);

        _sendOk(req, "ppt", "", &d, 200);
    }

    void apiPostPpt(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {

        if (_isSafeMode() && !_isApiAllowedInSafeMode(req->url().c_str())) {
                    _cnt_safe_blocked++;
                    _sendErr(req, 403, "safe_mode_blocked", "Blocked in safe mode.");
                    return;
                }
        String v_body;
        if (!_collectBodyOrReply(req, data, len, index, total, v_body)) return;
        
        JsonDocument         d;
        DeserializationError err = deserializeJson(d, v_body);
        if (err) {
                    _cnt_json_bad++;
                    _sendErr(req, 400, "bad_json", "Invalid JSON.");
                    return;
        }


        bool save = true;
        if (!d["save"].isNull()) save = (bool)d["save"];

        JsonVariant map = d["map"];
        if (map.isNull()) {
            _sendErr(req, 400, "no_map", "Missing map field.");
            return;
        }

        bool ok      = true;
        bool saved   = false;
        bool applied = false;

        if (!_cfg) {
            ok = false;
        } else {
            ST_C10_WiFiConfig_t w;
            ST_C10_E10Config_t  e;
            _cfg->makeDefaultsWiFi(w);
            _cfg->makeDefaultsE10(e);
            (void)_cfg->loadAll(w, e);

            auto loadK = [&](const char* n, ST_C10_PptKey2_t& k) {
                JsonVariant o = map[n];
                if (o.isNull()) return;

                if (!o["page"].isNull()) {
                    const char* s = (const char*)o["page"];
                    if (s && strcasecmp(s, "consumer") == 0)
                        k.page = (uint8_t)EN_C10_KEYPAGE_CONSUMER;
                    else
                        k.page = (uint8_t)EN_C10_KEYPAGE_KB;
                }
                if (!o["mod"].isNull()) k.mod = (uint8_t)o["mod"];
                if (!o["code"].isNull()) k.code = (uint32_t)o["code"];
            };

            loadK("start", e.ppt2_start);
            loadK("exit", e.ppt2_exit);
            loadK("next", e.ppt2_next);
            loadK("prev", e.ppt2_prev);
            loadK("black", e.ppt2_black);
            loadK("laser", e.ppt2_laser);

            ok = ok && _cfg->validateE10(e);

            if (ok && save) {
                ok    = _cfg->saveAll(w, e);
                saved = ok;
            }

            ST_W10_E10If_t* e10if = (ST_W10_E10If_t*)_applyCtx;
            if (ok && e10if && e10if->applyRuntimeE10) {
                applied = e10if->applyRuntimeE10(e10if->ctx, &e);
            }
        }

        JsonDocument data;
        data["saved"] = saved;
        data["applied"] = applied;
        if (ok) _sendOk(req, "ppt_set", "", &data, 200);
        else    _sendErr(req, 400, "ppt_set_failed", "Failed to update mapping.", &data);
    }

    void apiPptTest(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {

        if (_isSafeMode() && !_isApiAllowedInSafeMode(req->url().c_str())) {
                    _cnt_safe_blocked++;
                    _sendErr(req, 403, "safe_mode_blocked", "Blocked in safe mode.");
                    return;
                }
        String v_body;
        if (!_collectBodyOrReply(req, data, len, index, total, v_body)) return;
        
        JsonDocument         d;
        DeserializationError err = deserializeJson(d, v_body);
        if (err) {
                    _cnt_json_bad++;
                    _sendErr(req, 400, "bad_json", "Invalid JSON.");
                    return;
        }


        uint8_t  page = (uint8_t)EN_C10_KEYPAGE_KB;
        uint8_t  mod  = 0;
        uint32_t code = 0;

        if (!d["page"].isNull()) {
            const char* s = (const char*)d["page"];
            if (s && strcasecmp(s, "consumer") == 0) page = (uint8_t)EN_C10_KEYPAGE_CONSUMER;
        }
        if (!d["mod"].isNull()) mod = (uint8_t)d["mod"];
        if (!d["code"].isNull()) code = (uint32_t)d["code"];

        ST_W10_E10If_t* e10if = (ST_W10_E10If_t*)_applyCtx;
        bool                  ok  = (e10if && e10if->testPptKey2 ? e10if->testPptKey2(e10if->ctx, page, mod, code) : false);

        if (ok) _sendOk(req, "ppt_test", "", nullptr, 200);
        else    _sendErr(req, 500, "ppt_test_failed", "Test failed.");
    }

    // =====================================================
    // OTA
    // =====================================================
    void apiOtaUpload(AsyncWebServerRequest* req, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {

        // OTA Guard: E10 status 기반으로 OTA 차단 가능 (SafeMode에서는 항상 허용)
        if (!_isSafeMode()) {
            ST_W10_E10If_t* e10if = (ST_W10_E10If_t*)_applyCtx;
            if (e10if && e10if->getStatus) {
                ST_E10_Status_t s;
                memset(&s, 0, sizeof(s));
                if (e10if->getStatus(e10if->ctx, &s)) {
                    if (s.ota_guard) {
                        _cnt_ota_blocked++;
                        if (index == 0) {
                            JsonDocument data;
                            data["reason"] = "ota_guard";
                            _sendErr(req, 403, "ota_guard_blocked", "OTA upload is blocked by guard.", &data);
                        }
                        return;
                    }
                }
            }
        }

        (void)filename;

        if (index == 0) {
            if (_otaInProgress) {
                _otaOk = false;
                strlcpy(_otaErr, "busy", sizeof(_otaErr));
                return;
            }

            _otaInProgress = true;
            _otaWritten    = 0;
            _otaTotal      = (uint32_t)req->contentLength();
            _otaOk         = false;
            strlcpy(_otaErr, "in_progress", sizeof(_otaErr));

            size_t sketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
            if (_otaTotal == 0 || _otaTotal > (uint32_t)sketchSpace) {
                _otaOk = false;
                strlcpy(_otaErr, "size_invalid", sizeof(_otaErr));
                _otaInProgress = false;
                return;
            }

            if (!Update.begin(sketchSpace)) {
                _otaOk = false;
                strlcpy(_otaErr, "Update.begin failed", sizeof(_otaErr));
                _otaInProgress = false;
                return;
            }
        }

        if (len) {
            size_t w     = Update.write(data, len);
            _otaWritten += (uint32_t)w;
            if (w != len) strlcpy(_otaErr, "Update.write mismatch", sizeof(_otaErr));
        }

        if (final) {
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

    void apiOtaStatus(AsyncWebServerRequest* req) {
        JsonDocument data;
        data["in_progress"] = _otaInProgress;
        data["total"]       = (uint32_t)_otaTotal;
        data["written"]     = (uint32_t)_otaWritten;
        data["ok"]          = _otaOk;
        data["err"]         = _otaErr;

        _sendOk(req, "ota_status", "", &data, 200);
    }

    // =====================================================
    // SafeBoot / FactoryReset
    // =====================================================
    void apiSafeBootGet(AsyncWebServerRequest* req) {
        if (_cfg) {
            ST_C10_BootState_t bs;
            _cfg->getBootState(bs);
            JsonDocument data;
            data["safe_mode"]  = bs.safe_mode;
            data["fail_count"] = bs.fail_count;
            data["pending"]    = bs.pending;
            _sendOk(req, "safeboot", "", &data, 200);
            return;
        }
        _sendErr(req, 500, "no_config", "Config manager not ready.");
    }

    void apiSafeBootPost(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
        String v_body;
        if (!_collectBodyOrReply(req, data, len, index, total, v_body)) return;
        
        JsonDocument         d;
        DeserializationError err = deserializeJson(d, v_body);
        if (err) {
                    _cnt_json_bad++;
                    _sendErr(req, 400, "bad_json", "Invalid JSON.");
                    return;
        }

    
        bool v_exit = false;
        if (!d["exit"].isNull()) v_exit = (bool)d["exit"];
    
        bool ok = false;
        if (_cfg && v_exit) {
            ok = _cfg->clearSafeMode();
        }
    
        JsonDocument data;
        data["exit"] = v_exit;
        data["note"] = "exit=true -> clears safe_mode and reboots.";
        if (ok) _sendOk(req, "safeboot_exit", "", &data, 200);
        else    _sendErr(req, 500, "safeboot_exit_failed", "Failed.", &data);
    
        if (ok && v_exit) {
            delay(150);
            ESP.restart();
        }
    }

    void apiFactoryReset(AsyncWebServerRequest* req) {
        bool ok = (_cfg && _cfg->factoryReset(true));

        JsonDocument data;
        data["note"] = "Factory reset done. Rebooting...";
        if (ok) {
            _markLastApply(true, "factory", "factory_reset");
            _sendOk(req, "factory_reset", "", &data, 200);
        } else {
            _markLastApply(false, "factory", "factory_reset_failed");
            _sendErr(req, 500, "factory_reset_failed", "Failed.", &data);
        }

        if (ok) {
            delay(200);
            ESP.restart();
        }
    }

    // =====================================================
    // /api/reboot (POST)
    // - optional JSON body:
    //    { "reason_mask": <uint32>, "force": <bool> }
    // - reason_mask provided: allow reboot only when (needRebootMask & reason_mask) == reason_mask
    // - force=true: bypass mask check
    // =====================================================
    static void _taskReboot(void* p_arg) {
        (void)p_arg;
        delay(200);
        ESP.restart();
    }

    void apiRebootPost(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
        String v_body;
        if (!_collectBodyOrReply(req, data, len, index, total, v_body)) return;

        bool     v_force = false;
        uint32_t v_mask  = 0;
        bool     v_hasMask = false;

        if (v_body.length() > 0) {
            JsonDocument in;
            DeserializationError err = deserializeJson(in, v_body);
            if (err) {
                _cnt_json_bad++;
                _sendErr(req, 400, "bad_json", "Invalid JSON.");
                return;
            }

            JsonVariant v_rm = in["reason_mask"];
            if (!v_rm.isNull()) {
                v_mask = (uint32_t)v_rm.as<uint32_t>();
                v_hasMask = true;
            }

            JsonVariant v_f = in["force"];
            if (!v_f.isNull()) {
                v_force = v_f.as<bool>();
            }
        }

        // If not forced and no reason_mask provided, allow reboot only when a reboot is currently required.
        if (!v_force && !v_hasMask) {
            if (!_needReboot) {
                JsonDocument data;
                data["need_reboot"] = _needReboot;
                data["need_reboot_mask"] = (uint32_t)_needRebootMask;
                data["reboot_reasons"] = _rebootReasonsString(_needRebootMask);
                _sendErr(req, 409, "no_reboot_needed", "Reboot is not required.", &data);
                return;
            }
        }

        if (!v_force && v_hasMask) {
            if (((_needRebootMask & v_mask) != v_mask)) {
                JsonDocument data;
                data["need_reboot"] = _needReboot;
                data["need_reboot_mask"] = (uint32_t)_needRebootMask;
                _sendErr(req, 409, "mask_mismatch", "Reboot is not allowed for the given reason_mask.", &data);
                return;
            }
        }

        JsonDocument data;
        data["need_reboot"] = _needReboot;
        data["need_reboot_mask"] = (uint32_t)_needRebootMask;
        _sendOk(req, "reboot_scheduled", "Reboot scheduled.", &data, 200);

        // reboot after response flush
        xTaskCreatePinnedToCore(_taskReboot, "w10_reboot", 2048, nullptr, 1, nullptr, 0);
    }

};
