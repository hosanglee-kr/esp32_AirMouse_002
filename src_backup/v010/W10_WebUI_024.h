#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : W10_WebUI_024.h
 * 모듈약어 : W10
 * 모듈명 : WebUI (AirMouse Full, Keymap Editor + Field Dashboard + Test API)
 * ------------------------------------------------------
 * 기능 요약
 *  - /api/ppt/keymap : GET(조회), POST(저장+적용)
 *  - /api/ppt/test   : POST(임시 적용 Apply without Save), DELETE(임시 해제)
 *  - /api/ppt/keys   : GET(키 목록: page=7|12, q=검색)
 *  - /api/dashboard  : GET(현장용 대시보드: 색상/경고/알림 + 이상치 감지 지표)
 *  - JSON 응답은 createNestedArray/Object 없이 "JSON Text"로 생성
 * ------------------------------------------------------
 * [구현 규칙]
 *  - 항상 소스 시작 주석 부분 체계 유지 및 내용 업데이트
 *  - 소스 시작 주석 부분 구현규칙, 코드네이밍규칙 내용 그대로 유지, 수정금지
 *  - ArduinoJson v7.x.x 사용 (v6 이하 사용 금지)
 *  - JsonDocument 단일 타입만 사용
 *  - createNestedArray/Object/containsKey 사용 금지
 *  - memset + strlcpy 기반 안전 초기화
 *  - 주석/필드명은 JSON 구조와 동일하게 유지
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 * - 전역 상수,매크로      : G_모듈약어_ 접두사
 * - 전역 변수             : g_모듈약어_ 접두사
 * - 전역 함수             : 모듈약어_ 접두사
 * - type                  : T_모듈약어_ 접두사
 * - typedef               : _t  접미사
 * - enum 상수             : EN_모듈약어_ 접두사
 * - 구조체                : ST_모듈약어_ 접두사
 * - 클래스명              : CL_모듈약어_ 접두사
 * - 클래스 private 멤버   : _ 접두사
 * - 클래스 멤버(함수/변수) : 모듈약어 접두사 미사용
 * - 클래스 정적 멤버      : s_ 접두사
 * - 함수 로컬 변수        : v_ 접두사
 * - 함수 인자             : p_접두사
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <ArduinoJson.h>

#include "A40_ComFunc_070.h"
#include "D10_Logger_061.h"
#include "E10_PPT_Keymap_024.h"

// AsyncWebServer 사용 전제(프로젝트 기존 의존에 맞춰 include 조정)
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#ifndef G_W10_HTTP_MAX_BODY
#define G_W10_HTTP_MAX_BODY 8192
#endif

// ------------------------------------------------------
// (현장 대시보드) 상태 모델
// ------------------------------------------------------
typedef struct {
    char     color[12];     // "green|yellow|red|gray"
    char     title[32];     // "OK|WARN|ALERT"
    char     note[96];      // 요약 메시지
    uint32_t ms;            // uptime ms
    uint32_t drop;
    uint32_t consecFail;
    uint32_t spike;
    uint32_t lastFailMs;
    uint32_t lastErrMs;
    uint32_t lastWarnMs;
} ST_W10_Dash_t;

class CL_W10_WebUI {
  public:
    static void begin(AsyncWebServer& p_srv) {
        s_srv = &p_srv;
        _registerRoutes(*s_srv);
        D10_LOGI("[W10] WebUI routes registered");
    }

    // 외부에서 대시보드 정책을 바꾸고 싶을 때(옵션)
    static void setFailThreshold(uint32_t p_consecFailRed, uint32_t p_spikeYellow) {
        s_thConsecFailRed = (p_consecFailRed == 0 ? 3 : p_consecFailRed);
        s_thSpikeYellow   = p_spikeYellow;
    }

    // --------------------------------------------------
    // 대시보드 JSON 생성
    // --------------------------------------------------
    static void getDashboardAsJsonText(String& p_out) {
        ST_D10_Stats_t st;
        CL_D10_Logger::getStats(st);

        ST_W10_Dash_t d;
        _buildDashFromStats(d, st);

        p_out.reserve(2048);
        p_out = "{";
        p_out += "\"color\":\""; _appendJsonEscaped(p_out, d.color); p_out += "\"";
        p_out += ",\"title\":\""; _appendJsonEscaped(p_out, d.title); p_out += "\"";
        p_out += ",\"note\":\"";  _appendJsonEscaped(p_out, d.note);  p_out += "\"";
        p_out += ",\"ms\":";      p_out += String((unsigned long)d.ms);

        // logger stats + anomaly
        p_out += ",\"logger\":{";
        p_out += "\"drop\":";      p_out += String((unsigned long)d.drop);
        p_out += ",\"lastErrMs\":";p_out += String((unsigned long)d.lastErrMs);
        p_out += ",\"lastWarnMs\":";p_out += String((unsigned long)d.lastWarnMs);
        p_out += "}";

        p_out += ",\"anomaly\":{";
        p_out += "\"consecFail\":"; p_out += String((unsigned long)d.consecFail);
        p_out += ",\"spike\":";     p_out += String((unsigned long)d.spike);
        p_out += ",\"lastFailMs\":";p_out += String((unsigned long)d.lastFailMs);
        p_out += "}";

        p_out += "}";
    }

  private:
    // --------------------------------------------------
    // Routes
    // --------------------------------------------------
    static void _registerRoutes(AsyncWebServer& p_srv) {
        // 1) Keymap GET
        p_srv.on("/api/ppt/keymap", HTTP_GET, [](AsyncWebServerRequest* req) {
            String out;
            CL_E10_PPT_Keymap::getKeymapAsJsonText(out, true);
            req->send(200, "application/json", out);
        });

        // 2) Key list GET: /api/ppt/keys?page=7|12&q=xxx
        p_srv.on("/api/ppt/keys", HTTP_GET, [](AsyncWebServerRequest* req) {
            uint16_t page = G_E10_PAGE_KBD;
            if (req->hasParam("page")) {
                page = (uint16_t)req->getParam("page")->value().toInt();
                if (page == 7) page = G_E10_PAGE_KBD;
                if (page == 12) page = G_E10_PAGE_CONS;
            }
            const char* q = "";
            if (req->hasParam("q")) q = req->getParam("q")->value().c_str();

            String out;
            CL_E10_PPT_Keymap::getKeyListAsJsonText(out, page, q, 400);
            req->send(200, "application/json", out);
        });

        // 3) Dashboard GET (현장용)
        p_srv.on("/api/dashboard", HTTP_GET, [](AsyncWebServerRequest* req) {
            String out;
            CL_W10_WebUI::getDashboardAsJsonText(out);
            req->send(200, "application/json", out);
        });

        // 4) Save keymap POST: body json
        p_srv.on("/api/ppt/keymap", HTTP_POST, [](AsyncWebServerRequest* req) {
            // body는 onBody에서 처리
            req->send(202, "application/json", "{\"ok\":true,\"msg\":\"accepted\"}");
        }, nullptr, _onBody_KeymapSave);

        // 5) Test apply POST: /api/ppt/test
        p_srv.on("/api/ppt/test", HTTP_POST, [](AsyncWebServerRequest* req) {
            req->send(202, "application/json", "{\"ok\":true,\"msg\":\"accepted\"}");
        }, nullptr, _onBody_KeymapTest);

        // 6) Test clear DELETE: /api/ppt/test
        p_srv.on("/api/ppt/test", HTTP_DELETE, [](AsyncWebServerRequest* req) {
            CL_E10_PPT_Keymap::clearTestOverlay();
            req->send(200, "application/json", "{\"ok\":true,\"msg\":\"test_cleared\"}");
        });
    }

    // --------------------------------------------------
    // body handlers (정책: JsonDocument 단일)
    // --------------------------------------------------
    static void _onBody_KeymapSave(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
        (void)index;

        if (!_ensureBodyBuffer(total)) {
            req->send(413, "application/json", "{\"ok\":false,\"err\":\"too_large\"}");
            return;
        }

        memcpy(s_bodyBuf + index, data, len);
        s_bodyLen = total;
        s_bodyBuf[s_bodyLen] = '\0';

        if (index + len != total) return; // 아직 덜 옴

        const bool ok = CL_E10_PPT_Keymap::applySavedFromJson((const char*)s_bodyBuf);
        if (!ok) {
            CL_D10_Logger::markFail("[W10] keymap save/apply failed");
            req->send(400, "application/json", "{\"ok\":false,\"err\":\"apply_failed\"}");
            return;
        }
        req->send(200, "application/json", "{\"ok\":true,\"msg\":\"saved\"}");
    }

    static void _onBody_KeymapTest(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
        (void)index;

        if (!_ensureBodyBuffer(total)) {
            req->send(413, "application/json", "{\"ok\":false,\"err\":\"too_large\"}");
            return;
        }

        memcpy(s_bodyBuf + index, data, len);
        s_bodyLen = total;
        s_bodyBuf[s_bodyLen] = '\0';

        if (index + len != total) return;

        const bool ok = CL_E10_PPT_Keymap::applyTestFromJson((const char*)s_bodyBuf);
        if (!ok) {
            CL_D10_Logger::markFail("[W10] keymap test apply failed");
            req->send(400, "application/json", "{\"ok\":false,\"err\":\"test_apply_failed\"}");
            return;
        }
        req->send(200, "application/json", "{\"ok\":true,\"msg\":\"test_applied\"}");
    }

    static bool _ensureBodyBuffer(size_t p_total) {
        if (p_total == 0) return false;
        if (p_total > G_W10_HTTP_MAX_BODY) return false;
        if (s_bodyBuf == nullptr) {
            s_bodyBuf = (uint8_t*)malloc(G_W10_HTTP_MAX_BODY + 1);
            if (!s_bodyBuf) return false;
            memset(s_bodyBuf, 0, G_W10_HTTP_MAX_BODY + 1);
        }
        return true;
    }

    // --------------------------------------------------
    // 현장 대시보드 정책(색상/경고/알림)
    // --------------------------------------------------
    static void _buildDashFromStats(ST_W10_Dash_t& p_out, const ST_D10_Stats_t& st) {
        memset(&p_out, 0, sizeof(p_out));
        p_out.ms         = millis();
        p_out.drop       = st.drop;
        p_out.consecFail = st.consecFail;
        p_out.spike      = st.spike;
        p_out.lastFailMs = st.lastFailMs;
        p_out.lastErrMs  = st.lastErrMs;
        p_out.lastWarnMs = st.lastWarnMs;

        // 기본: OK
        strlcpy(p_out.color, "green", sizeof(p_out.color));
        strlcpy(p_out.title, "OK", sizeof(p_out.title));
        strlcpy(p_out.note,  "System nominal", sizeof(p_out.note));

        // ALERT 조건: 연속 실패
        if (st.consecFail >= s_thConsecFailRed) {
            strlcpy(p_out.color, "red", sizeof(p_out.color));
            strlcpy(p_out.title, "ALERT", sizeof(p_out.title));
            snprintf(p_out.note, sizeof(p_out.note),
                     "Consecutive failures=%lu (check sensors/ble/i2c)", (unsigned long)st.consecFail);
            return;
        }

        // WARN 조건: spike 누적이 있거나 최근 warn/err
        const uint32_t nowMs = millis();
        const bool recentErr = (st.lastErrMs > 0 && (nowMs - st.lastErrMs) < 30000UL);
        const bool recentWrn = (st.lastWarnMs > 0 && (nowMs - st.lastWarnMs) < 30000UL);
        const bool spikeHigh = (s_thSpikeYellow > 0 && st.spike >= s_thSpikeYellow);

        if (spikeHigh || recentErr || recentWrn) {
            strlcpy(p_out.color, "yellow", sizeof(p_out.color));
            strlcpy(p_out.title, "WARN", sizeof(p_out.title));
            if (spikeHigh) {
                snprintf(p_out.note, sizeof(p_out.note), "Spike count=%lu (monitor stability)", (unsigned long)st.spike);
            } else if (recentErr) {
                strlcpy(p_out.note, "Recent ERROR detected (see logs)", sizeof(p_out.note));
            } else {
                strlcpy(p_out.note, "Recent WARN detected (see logs)", sizeof(p_out.note));
            }
        }

        // drop이 크면 경고 강화(버퍼 오버런)
        if (st.drop > 100) {
            strlcpy(p_out.color, "yellow", sizeof(p_out.color));
            strlcpy(p_out.title, "WARN", sizeof(p_out.title));
            snprintf(p_out.note, sizeof(p_out.note), "Log drops=%lu (increase buffer or reduce logs)", (unsigned long)st.drop);
        }
    }

    static void _appendJsonEscaped(String& p_out, const char* p_s) {
        if (!p_s) return;
        for (size_t i = 0; p_s[i] != '\0'; i++) {
            char c = p_s[i];
            if (c == '\"' || c == '\\') { p_out += '\\'; p_out += c; }
            else if (c == '\n') p_out += "\\n";
            else if (c == '\r') p_out += "\\r";
            else if (c == '\t') p_out += "\\t";
            else p_out += c;
        }
    }

  private:
    static AsyncWebServer* s_srv;

    static uint8_t*  s_bodyBuf;
    static size_t    s_bodyLen;

    // dashboard thresholds
    static uint32_t  s_thConsecFailRed;
    static uint32_t  s_thSpikeYellow;
};

// ------------------------------------------------------
// static storage
// ------------------------------------------------------
inline AsyncWebServer* CL_W10_WebUI::s_srv = nullptr;

inline uint8_t* CL_W10_WebUI::s_bodyBuf = nullptr;
inline size_t   CL_W10_WebUI::s_bodyLen = 0;

inline uint32_t CL_W10_WebUI::s_thConsecFailRed = 3;
inline uint32_t CL_W10_WebUI::s_thSpikeYellow   = 5;

