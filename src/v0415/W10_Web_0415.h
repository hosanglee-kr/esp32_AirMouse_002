// File: src/v0415/W10_Web_0415.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : W10_Web_0415.h
 * 모듈약어 : W10
 * 모듈명 : Web Config/Status/UI/OTA Server
 * ------------------------------------------------------
 * [v0415 주요 변경]
 *  - begin() 4-arg 오버로드 삭제 (Phase 7 L7a-A1-01, CONTRACT rev3 위반 해소)
 *    · 2-arg(cfg, e10if) 단일 진입
 *  - _bootWifi 멤버 추가 — 부팅 시 WiFi 스냅샷
 *    · _markNeedReboot 되돌림 fix (Phase 7 L7e-A3-01)
 *    · 저장 시마다 boot → 현재 비교로 마스크 재계산
 *  - _bumpRebootMaskFromBoot() 신규 private
 *  - include 경로 _0415
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

#include "W10_Def_0415.h"

class CL_W10_WebConfig {
private:
    AsyncWebServer _svr;

    CL_C10_Config*  _cfg   = nullptr;
    ST_W10_E10If_t* _e10if = nullptr;

    ST_C10_WiFiConfig_t _wifi;

    // [v0415] 부팅 시 WiFi 스냅샷 — 되돌림 감지용
    ST_C10_WiFiConfig_t _bootWifi;

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
    uint32_t _otaStartedMs = 0;

    static void s_wifiEvent(WiFiEvent_t p_e, WiFiEventInfo_t p_info);

public:
    CL_W10_WebConfig();

    // [v0415] 단일 진입 (4-arg 오버로드 삭제)
    void begin(CL_C10_Config* p_cfg,
               ST_W10_E10If_t* p_e10if);

private:
    // WiFi
    void _setupWiFi();
    void _startAp();
    void _onWifiEvent(WiFiEvent_t p_e);
    void _startMdns();
    void _stopMdns();

    // Body slots
    ST_W10_BodySlot* _bodySlotAlloc(AsyncWebServerRequest* req);
    ST_W10_BodySlot* _bodyGetSlot(AsyncWebServerRequest* req, size_t index);
    void _bodyFree(AsyncWebServerRequest* req);

    // gzip
    bool _acceptsGzip(AsyncWebServerRequest* req);

    // Static serving
    void _handleDynamicStatic(AsyncWebServerRequest* req);
    void _serveWwwStatic(AsyncWebServerRequest* req, const char* p_path);
    void _servePublicJson(AsyncWebServerRequest* req, const char* p_path);

    // SafeMode gate
    bool _isSafeMode() const;
    bool _isApiAllowedInSafeMode(const char* p_uri, WebRequestMethodComposite p_method) const;
    bool _gateSafeModeOrReply(AsyncWebServerRequest* req);

    // JSON helpers
    void _sendJsonStream(AsyncWebServerRequest* req, JsonDocument& d, int p_code = 200);
    bool _ifNoneMatchHit(AsyncWebServerRequest* req, uint32_t p_etag) const;
    void _formatEtagQuoted(uint32_t p_etag, char* p_out, size_t p_outSize) const;
    void _send304NoStoreEtag(AsyncWebServerRequest* req, uint32_t p_etag);
    bool _calcFileEtag32(const char* p_path, uint32_t& p_outEtag, size_t* p_outSize);
    void _send304StaticWithCacheControl(AsyncWebServerRequest* req, uint32_t p_etag,
                                        const char* p_cacheControl, bool p_varyAcceptEncoding);
    void _sendStaticWithCacheControlEtag(AsyncWebServerRequest* req,
                                         const char* p_sendPath,
                                         const char* p_contentType,
                                         const char* p_cacheControl,
                                         bool p_useGz,
                                         bool p_varyAcceptEncoding,
                                         bool p_hasEtag,
                                         uint32_t p_etag,
                                         size_t p_size);

    void _sendOk(AsyncWebServerRequest* req, const char* p_code, const char* p_msg,
                 JsonDocument* p_data = nullptr, int p_http = 200);
    void _sendErr(AsyncWebServerRequest* req, const char* code, const char* msg,
                  JsonDocument* data = nullptr);
    int _httpFromCode(const char* p_code);

    void _diagPush(const char* p_code);

    bool _wantsJson(AsyncWebServerRequest* req) const;
    void _sendStaticErr(AsyncWebServerRequest* req, int p_http, const char* p_code, const char* p_msg);

    bool _collectBodyOrReply(AsyncWebServerRequest* req,
                             uint8_t* data, size_t len,
                             size_t index, size_t total,
                             String& p_outBody);

    // reboot helpers
    uint32_t _wifiDiffMask(const ST_C10_WiFiConfig_t& a, const ST_C10_WiFiConfig_t& b);
    void _markNeedReboot(uint32_t reasonMask);

    // [v0415] boot 스냅샷 기반 마스크 재계산 (되돌림 fix)
    void _bumpRebootMaskFromBoot();

    void _markLastApply(bool ok, const char* src, const char* code);
    String _rebootReasonsString(uint32_t m);

    // [v0412 유지] /api/profiles
    void apiProfilesList    (AsyncWebServerRequest* req);
    void apiProfilesSwitch  (AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
    void apiProfilesCreate  (AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
    void apiProfilesDelete  (AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
    void apiProfilesRename  (AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
    void apiProfilesActiveGet(AsyncWebServerRequest* req);
    void apiProfilesActivePost(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);

    void apiTriggers        (AsyncWebServerRequest* req);
    void apiActionTest      (AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
    void apiActionTestMacro (AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);

    void _fillE10StatusFromSnapshot(JsonObject e, const ST_E10_Status_t& s);
    void _fillE10Status(JsonObject e, ST_W10_E10If_t* e10if);

    void _apiStatus(AsyncWebServerRequest* req);
    void _apiDiag(AsyncWebServerRequest* req);
    void _apiDiagClear(AsyncWebServerRequest* req);

    const char* _kbName(uint16_t p_code);
    const char* _precModeName(uint8_t p_mode);
    void apiKeycodes(AsyncWebServerRequest* req);

    void apiGetConfig(AsyncWebServerRequest* req);
    void apiConfigSave(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
    void apiConfigApply(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
    void apiExport(AsyncWebServerRequest* req);
    void apiImport(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
    void apiRollback(AsyncWebServerRequest* req);

    void apiControl(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
    void apiPptTest(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);

    void apiOtaUpload(AsyncWebServerRequest* req, const String& filename, size_t index, uint8_t* data, size_t len, bool final);
    void apiOtaStatus(AsyncWebServerRequest* req);

    void apiSafeBootGet(AsyncWebServerRequest* req);
    void apiSafeBootPost(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
    void apiFactoryReset(AsyncWebServerRequest* req);

    static void _taskReboot(void* p_arg);
    void apiRebootPost(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);
    void apiRebootCheck(AsyncWebServerRequest* req);
};
