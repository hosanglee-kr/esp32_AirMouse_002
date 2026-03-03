// =======================================================
// File: W10_Web_0315.h
// =======================================================
#pragma once

/*
 * ------------------------------------------------------
 * 소스명 : W10_Web_0315.h
 * 모듈약어 : W10
 * 모듈명 : Web Config/Status/UI/OTA Server (Dynamic Static Routing, No Asset Table)
 * ------------------------------------------------------
 * 기능 요약
 *  - (0314) 0313을 h+cpp 4개 파일로 분할
 *  - 자산 테이블 제거: 동적 정적파일 서빙(/www/*)
 *  - 보안: /favicon.ico /robots.txt /www/* /json/public/* 만 허용
 *  - gzip: html/css/js 만 (.gz 존재 + Accept-Encoding:gzip) 시 사용
 *  - Cache-Control 자동 분류:
 *     * no-store: html, /json/public/*, /api/*
 *     * immutable: 버전 토큰 포함한 정적 리소스(css/js/svg/png/webp/ico)
 *     * short: 그 외 정적 리소스
 *  - API: /api/status, /api/keycodes, /api/config, /api/ppt, /api/ota, /api/safeboot, /api/factory_reset ...
 *  - (0314) AsyncResponseStream 모두 적용(모든 JSON 응답 스트리밍)
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
#include <string.h>

#include <ESPAsyncWebServer.h>
#include <Update.h>

#include "C10_Config_0310.h"
#include "E10_Def_0310.h"
#include "W10_Def_0314.h"

class CL_W10_WebConfig {
private:
    AsyncWebServer _svr;

    CL_C10_Config* _cfg = nullptr;
    bool (*_applyFn)(void*) = nullptr;
    void* _applyCtx = nullptr;
    ST_W10_E10If_t* _e10if = nullptr; // (NEW) E10 interface pointer (no cast from _applyCtx)

    ST_C10_WiFiConfig_t _wifi;
    ST_C10_E10Config_t _e10;

    bool _mdnsStarted = false;

    bool _needReboot = false;
    uint32_t _needRebootMask = 0;

    bool _lastApplyOk = true;
    uint32_t _lastApplyMs = 0;
    char _lastApplyCode[32] = "";
    char _lastApplySrc[16] = "";

    inline static CL_W10_WebConfig* s_instance = nullptr;

    inline static ST_W10_BodySlot s_bodySlots[G_W10_BODY_SLOTS];

    uint32_t _cnt_body_too_large = 0;
    uint32_t _cnt_body_no_slot = 0;
    uint32_t _cnt_json_bad = 0;
    uint32_t _cnt_safe_blocked = 0;
    uint32_t _cnt_ota_blocked = 0;

    ST_W10_DiagEvt_t _diagEvt[G_W10_DIAG_EVT_MAX];
    uint8_t _diagEvtHead = 0;
    uint8_t _diagEvtCount = 0;

    // OTA state
    volatile bool _otaInProgress = false;
    volatile uint32_t _otaTotal = 0;
    volatile uint32_t _otaWritten = 0;
    volatile bool _otaOk = false;
    char _otaErr[64];

    static void s_wifiEvent(WiFiEvent_t p_e, WiFiEventInfo_t p_info);

public:
    CL_W10_WebConfig();

    void begin(CL_C10_Config* p_cfg,
               bool (*p_applyFn)(void*),
               void* p_applyCtx,
               ST_W10_E10If_t* p_e10if);

private:
    // =====================================================
    // WiFi
    // =====================================================
    void _setupWiFi();
    void _startAp();
    void _onWifiEvent(WiFiEvent_t p_e);
    void _startMdns();
    void _stopMdns();

    // =====================================================
    // Request Body (fixed slots)
    // =====================================================
    ST_W10_BodySlot* _bodySlotAlloc(AsyncWebServerRequest* req);
    ST_W10_BodySlot* _bodyGetSlot(AsyncWebServerRequest* req, size_t index);
    void _bodyFree(AsyncWebServerRequest* req);

    // =====================================================
    // gzip Accept
    // =====================================================
    bool _acceptsGzip(AsyncWebServerRequest* req);

    // =====================================================
    // Dynamic static serving
    // =====================================================
    void _handleDynamicStatic(AsyncWebServerRequest* req);
    void _serveWwwStatic(AsyncWebServerRequest* req, const char* p_path);
    void _servePublicJson(AsyncWebServerRequest* req, const char* p_path);

    // =====================================================
    // SafeMode API Gate Policy
    // =====================================================
    bool _isSafeMode() const;
    bool _isApiAllowedInSafeMode(const char* p_uri) const;
    
    // =====================================================
    // SafeMode Gate (공통)
    //  - SafeMode + 비허용 API면 표준 에러 응답 후 true 반환
    // =====================================================
    bool _gateSafeModeOrReply(AsyncWebServerRequest* req);
    

    // =====================================================
    // JSON stream helpers (AsyncResponseStream)
    // =====================================================
    void _sendJsonStream(AsyncWebServerRequest* req, JsonDocument& d, int p_code = 200);
    
    // Envelope streaming safe string writer (Option-2)
    void _resPrintJsonString(AsyncResponseStream* res, const String& v);
    void _resPrintJsonString(AsyncResponseStream* res, const char* v);
    
    // =====================================================
    // ETag/If-None-Match 최소 호환
    //  - 따옴표/Weak ETag/콤마 리스트 대응
    // =====================================================
    bool _ifNoneMatchHit(AsyncWebServerRequest* req, uint32_t p_etag) const;
    void _formatEtagQuoted(uint32_t p_etag, char* p_out, size_t p_outSize) const;
    
    // 공통 응답 헬퍼 (no-store + ETag) : API/public json에서 사용
    void _send304NoStoreEtag(AsyncWebServerRequest* req, uint32_t p_etag);
    
    // Static용: 파일 ETag 계산(32-bit) + 파일 크기(optional)
    bool _calcFileEtag32(const char* p_path, uint32_t& p_outEtag, size_t* p_outSize);
    
    // 304 공통 (정적): Cache-Control 지정 + (선택) Vary:Accept-Encoding
    void _send304StaticWithCacheControl(AsyncWebServerRequest* req, uint32_t p_etag, const char* p_cacheControl, bool p_varyAcceptEncoding);
    
        // 200 공통 (정적): Cache-Control + (선택) Content-Encoding:gzip + (선택) Vary + ETag/X-Size
    // - p_varyAcceptEncoding은 "실제로 gzip을 사용한 경우에만 true"로 넣는 것을 권장(더 엄격)
    void _sendStaticWithCacheControlEtag(AsyncWebServerRequest* req,
                                         const char* p_sendPath,
                                         const char* p_contentType,
                                         const char* p_cacheControl,
                                         bool p_useGz,
                                         bool p_varyAcceptEncoding,
                                         bool p_hasEtag,
                                         uint32_t p_etag,
                                         size_t p_size);

    // 200 공통 (API/config/export): no-store + ETag(+size) 헤더 부착
    void _addEtagHeadersNoStore(AsyncWebServerResponse* res, bool p_hasEtag, uint32_t p_etag, size_t p_size);



    void _sendOk(AsyncWebServerRequest* req,
                 const char* p_code,
                 const char* p_msg,
                 JsonDocument* p_data = nullptr,
                 int p_http = 200);

    void _sendErr(AsyncWebServerRequest* req,
                  const char* code,
                  const char* msg,
                  JsonDocument* data = nullptr);

    int _httpFromCode(const char* p_code);

    // =====================================================
    // Diagnostics Event Push (ring buffer)
    // =====================================================
    void _diagPush(const char* p_code);

    // =====================================================
    // Static response helper (non-API)
    // =====================================================
    bool _wantsJson(AsyncWebServerRequest* req) const;
    void _sendStaticErr(AsyncWebServerRequest* req, int p_http, const char* p_code, const char* p_msg);

    // =====================================================
    // Envelope selector helper
    // =====================================================
    bool _wantsEnvelope(AsyncWebServerRequest* req);

    // =====================================================
    // Body Collector
    // =====================================================
    bool _collectBodyOrReply(AsyncWebServerRequest* req,
                             uint8_t* data, size_t len,
                             size_t index, size_t total,
                             String& p_outBody);

    // =====================================================
    // reboot helpers
    // =====================================================
    uint32_t _wifiDiffMask(const ST_C10_WiFiConfig_t& a, const ST_C10_WiFiConfig_t& b);
    void _markNeedReboot(uint32_t reasonMask);
    void _markLastApply(bool ok, const char* src, const char* code);
    String _rebootReasonsString(uint32_t m);

    // =====================================================
    // config save/import common
    // =====================================================
    void _apiConfigSaveImportCommon(
        AsyncWebServerRequest* req,
        uint8_t* data, size_t len,
        size_t index, size_t total,
        const char* p_src,
        const char* p_note,
        bool p_applyAfterSave);

    // =====================================================
    // E10 status fill
    // =====================================================
    void _fillE10StatusFromSnapshot(JsonObject e, const ST_E10_Status_t& s);
    void _fillE10Status(JsonObject e, ST_W10_E10If_t* e10if);

    // =====================================================
    // /api/status
    // =====================================================
    void _apiStatus(AsyncWebServerRequest* req);

    // =====================================================
    // /api/diag
    // =====================================================
    void _apiDiag(AsyncWebServerRequest* req);
    void _apiDiagClear(AsyncWebServerRequest* req);

    // =====================================================
    // /api/keycodes
    // =====================================================
    const char* _kbName(uint16_t p_code);
    const char* _precModeName(uint8_t p_mode);
    void apiKeycodes(AsyncWebServerRequest* req);

    // =====================================================
    // /api/config
    // =====================================================
    void apiGetConfig(AsyncWebServerRequest* req);
    void apiConfigSave(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
    void apiConfigApply(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
    void apiExport(AsyncWebServerRequest* req);
    void apiImport(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
    void apiRollback(AsyncWebServerRequest* req);

    // =====================================================
    // /api/control
    // =====================================================
    void apiControl(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);

    // =====================================================
    // /api/ppt
    // =====================================================
    void apiGetPpt(AsyncWebServerRequest* req);
    void apiPostPpt(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
    void apiPptTest(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);

    // =====================================================
    // OTA
    // =====================================================
    void apiOtaUpload(AsyncWebServerRequest* req, const String& filename, size_t index, uint8_t* data, size_t len, bool final);
    void apiOtaStatus(AsyncWebServerRequest* req);

    // =====================================================
    // SafeBoot / FactoryReset / Reboot
    // =====================================================
    void apiSafeBootGet(AsyncWebServerRequest* req);
    void apiSafeBootPost(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
    void apiFactoryReset(AsyncWebServerRequest* req);

    static void _taskReboot(void* p_arg);
    void apiRebootPost(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
    void apiRebootCheck(AsyncWebServerRequest* req);
};
