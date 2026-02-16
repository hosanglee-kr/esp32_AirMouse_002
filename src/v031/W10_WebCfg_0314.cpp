// =======================================================
// File: W10_WebCfg_0314.cpp
// =======================================================

/*
 * ------------------------------------------------------
 * 소스명 : W10_WebCfg_0314.cpp
 * 모듈약어 : W10
 * 모듈명 : Web Config/Status/UI/OTA Server (Split: begin/wifi)
 * ------------------------------------------------------
 * 기능 요약
 *  - (0314) 0313 분할: begin() 라우팅 + WiFi/MDNS 담당
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

#include "W10_WebCfg_0314.h"

void CL_W10_WebConfig::s_wifiEvent(WiFiEvent_t p_e, WiFiEventInfo_t p_info) {
    (void)p_info;
    if (s_instance) s_instance->_onWifiEvent(p_e);
}

CL_W10_WebConfig::CL_W10_WebConfig() : _svr(80) {
    s_instance = this;
    memset(&_wifi, 0, sizeof(_wifi));
    memset(&_e10, 0, sizeof(_e10));
    memset(_otaErr, 0, sizeof(_otaErr));
    strlcpy(_otaErr, "none", sizeof(_otaErr));
}

void CL_W10_WebConfig::begin(CL_C10_Config* p_cfg,
                            bool (*p_applyFn)(void*),
                            void* p_applyCtx,
                            ST_W10_E10If_t* p_e10if) {
    _cfg      = p_cfg;
    _applyFn  = p_applyFn;
    _applyCtx = p_applyCtx;
    _e10if    = p_e10if;

    WiFi.onEvent(s_wifiEvent);

    if (_cfg) {
        (void)_cfg->loadAll(_wifi, _e10);
    }

    _setupWiFi();

    // ==============================
    // API 라우팅 (전부 no-store)
    // ==============================
    _svr.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* req) { _apiStatus(req); });
    _svr.on("/api/diag", HTTP_GET, [this](AsyncWebServerRequest* req) { _apiDiag(req); });
    _svr.on("/api/diag/clear", HTTP_POST, [this](AsyncWebServerRequest* req) { _apiDiagClear(req); });

    _svr.on("/api/keycodes", HTTP_GET, [this](AsyncWebServerRequest* req) { apiKeycodes(req); });

    _svr.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* req) { apiGetConfig(req); });

    _svr.on("/api/config/save", HTTP_POST,
        [this](AsyncWebServerRequest* req) { (void)req; },
        nullptr,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            apiConfigSave(req, data, len, index, total);
        });

    _svr.on("/api/config/apply", HTTP_POST,
        [this](AsyncWebServerRequest* req) { (void)req; },
        nullptr,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            apiConfigApply(req, data, len, index, total);
        });

    _svr.on("/api/config/export", HTTP_GET, [this](AsyncWebServerRequest* req) { apiExport(req); });
    _svr.on("/api/export", HTTP_GET, [this](AsyncWebServerRequest* req) { apiExport(req); });

    _svr.on("/api/config/import", HTTP_POST,
        [this](AsyncWebServerRequest* req) { (void)req; },
        nullptr,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            apiImport(req, data, len, index, total);
        });

    _svr.on("/api/config/rollback", HTTP_POST,
        [this](AsyncWebServerRequest* req) { (void)req; },
        nullptr,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            (void)data; (void)len; (void)index; (void)total;
            apiRollback(req);
        });

    _svr.on("/api/control", HTTP_POST,
        [this](AsyncWebServerRequest* req) { (void)req; },
        nullptr,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            apiControl(req, data, len, index, total);
        });

    // PPT
    _svr.on("/api/ppt", HTTP_GET, [this](AsyncWebServerRequest* req) { apiGetPpt(req); });
    _svr.on("/api/ppt", HTTP_POST,
        [this](AsyncWebServerRequest* req) { (void)req; },
        nullptr,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            apiPostPpt(req, data, len, index, total);
        });

    _svr.on("/api/ppt/test", HTTP_POST,
        [this](AsyncWebServerRequest* req) { (void)req; },
        nullptr,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            apiPptTest(req, data, len, index, total);
        });

    // OTA
    _svr.on("/api/ota/status", HTTP_GET, [this](AsyncWebServerRequest* req) { apiOtaStatus(req); });

    _svr.on("/api/ota", HTTP_POST,
        [this](AsyncWebServerRequest* req) {
            JsonDocument v_doc;
            v_doc["written"] = (uint32_t)_otaWritten;
            v_doc["total"]   = (uint32_t)_otaTotal;
            if (_otaOk) _sendOk(req, "ota", "ok", &v_doc, 200);
            else _sendErr(req, "ota_failed", _otaErr, &v_doc);

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
    _svr.on("/api/safeboot", HTTP_POST,
        [this](AsyncWebServerRequest* req) { (void)req; },
        nullptr,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            apiSafeBootPost(req, data, len, index, total);
        });

    _svr.on("/api/factory_reset", HTTP_POST,
        [this](AsyncWebServerRequest* req) { (void)req; },
        nullptr,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            (void)data; (void)len; (void)index; (void)total;
            apiFactoryReset(req);
        });

    _svr.on("/api/reboot", HTTP_POST,
        [this](AsyncWebServerRequest* req) { (void)req; },
        nullptr,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            apiRebootPost(req, data, len, index, total);
        });

    _svr.on("/api/reboot/check", HTTP_GET, [this](AsyncWebServerRequest* req) { apiRebootCheck(req); });

    // ==============================
    // 정적 라우팅(동적 서빙)
    // ==============================
    _svr.on("/", HTTP_GET, [this](AsyncWebServerRequest* req) {
        req->redirect(G_W10_DEFAULT_INDEX_PATH);
    });

    _svr.onNotFound([this](AsyncWebServerRequest* req) {
        _handleDynamicStatic(req);
    });

    _svr.begin();
}

// =====================================================
// WiFi
// =====================================================
void CL_W10_WebConfig::_setupWiFi() {
    WiFi.mode(WIFI_MODE_NULL);

    const bool v_hasSta = (_wifi.sta_ssid[0] != '\0');
    const bool v_autoMode = (_wifi.mode == (uint8_t)EN_C10_WIFI_AUTO);
    const bool v_forceAp = (_wifi.mode == (uint8_t)EN_C10_WIFI_AP);
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
    bool v_ok = false;
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

void CL_W10_WebConfig::_startAp() {
    WiFi.mode(WIFI_AP);
    if (_wifi.ap_pass[0] != '\0')
        WiFi.softAP(_wifi.ap_ssid, _wifi.ap_pass);
    else
        WiFi.softAP(_wifi.ap_ssid);
}

void CL_W10_WebConfig::_onWifiEvent(WiFiEvent_t p_e) {
    if (p_e == ARDUINO_EVENT_WIFI_STA_GOT_IP) _startMdns();
    if (p_e == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) _stopMdns();
}

void CL_W10_WebConfig::_startMdns() {
    if (_mdnsStarted) return;
    if (_wifi.mdns_host[0] == '\0') return;
    if (WiFi.getMode() != WIFI_STA || WiFi.status() != WL_CONNECTED) return;
    if (!MDNS.begin(_wifi.mdns_host)) return;
    MDNS.addService("http", "tcp", 80);
    _mdnsStarted = true;
}

void CL_W10_WebConfig::_stopMdns() {
    if (!_mdnsStarted) return;
    MDNS.end();
    _mdnsStarted = false;
}
