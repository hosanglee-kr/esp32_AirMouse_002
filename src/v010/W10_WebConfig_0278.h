// =======================================================
// File: src/v010/W10_WebConfig_0278.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : W10_WebConfig_0278.h
 * 모듈약어 : W10
 * 모듈명 : Web Config/Status/UI/OTA Server (Dynamic Static Routing)
 * ------------------------------------------------------
 * 기능 요약
 *  - (0275) 자산 테이블 제거: 동적 정적파일 서빙(/www/*)
 *  - 보안: favicon.ico / robots.txt / www / json/public 만 허용
 *  - gzip: html/css/js 만 (.gz 존재 + Accept-Encoding:gzip) 시 사용
 *  - Cache-Control 자동 분류:
 *     * no-store: html, /json/public/*, /api/*
 *     * immutable: 버전 토큰 포함한 정적 리소스(css/js/svg/png/webp/ico)
 *     * short: 그 외 정적 리소스
 *  - /api/status, /api/keycodes, /api/config, /api/ppt, /api/ota, /api/safeboot, /api/factory_reset ...
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

#include "C10_Config_0274.h"
#include "E10_EliteAirMouse_0272.h"

#include "W10_Def_0278.h"

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

    static void s_wifiEvent(WiFiEvent_t p_e, WiFiEventInfo_t p_info){
        (void)p_info;
        if(s_instance) s_instance->onWifiEvent(p_e);
    }

    // OTA state
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

    void begin(CL_C10_Config* p_cfg, bool (*p_applyFn)(void*), void* p_applyCtx){
        _cfg = p_cfg;
        _applyFn = p_applyFn;
        _applyCtx = p_applyCtx;

        WiFi.onEvent(s_wifiEvent);
        (void)LittleFS.begin(true);
        (void)_cfg->loadAll(_wifi, _e10);

        setupWiFi();

        // ---- API 라우팅 ----
        _svr.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* req){ apiStatus(req); });
        _svr.on("/api/keycodes", HTTP_GET, [this](AsyncWebServerRequest* req){ apiKeycodes(req); });

        _svr.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* req){ apiGetConfig(req); });

        _svr.on("/api/config/save", HTTP_POST, [this](AsyncWebServerRequest* req){ (void)req; }, nullptr,
            [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
                apiConfigSave(req, data, len, index, total);
            });

        _svr.on("/api/config/apply", HTTP_POST, [this](AsyncWebServerRequest* req){ (void)req; }, nullptr,
            [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
                apiConfigApply(req, data, len, index, total);
            });

        _svr.on("/api/config/export", HTTP_GET, [this](AsyncWebServerRequest* req){ apiExport(req); });

        _svr.on("/api/config/import", HTTP_POST, [this](AsyncWebServerRequest* req){ (void)req; }, nullptr,
            [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
                apiImport(req, data, len, index, total);
            });

        _svr.on("/api/config/rollback", HTTP_POST, [this](AsyncWebServerRequest* req){ (void)req; }, nullptr,
            [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
                (void)data; (void)len; (void)index; (void)total;
                apiRollback(req);
            });

        _svr.on("/api/control", HTTP_POST, [this](AsyncWebServerRequest* req){ (void)req; }, nullptr,
            [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
                apiControl(req, data, len, index, total);
            });

        // PPT
        _svr.on("/api/ppt", HTTP_GET, [this](AsyncWebServerRequest* req){ apiGetPpt(req); });
        _svr.on("/api/ppt", HTTP_POST, [this](AsyncWebServerRequest* req){ (void)req; }, nullptr,
            [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
                apiPostPpt(req, data, len, index, total);
            });

        _svr.on("/api/ppt/test", HTTP_POST, [this](AsyncWebServerRequest* req){ (void)req; }, nullptr,
            [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
                apiPptTest(req, data, len, index, total);
            });

        // OTA
        _svr.on("/api/ota/status", HTTP_GET, [this](AsyncWebServerRequest* req){ apiOtaStatus(req); });

        _svr.on("/api/ota", HTTP_POST,
            [this](AsyncWebServerRequest* req){
                JsonDocument d;
                d["ok"] = _otaOk;
                d["err"] = _otaErr;
                d["written"] = (uint32_t)_otaWritten;
                d["total"] = (uint32_t)_otaTotal;
                String out; serializeJson(d, out);
                req->send(_otaOk ? 200 : 500, "application/json", out);
                if(_otaOk){ delay(200); ESP.restart(); }
            },
            [this](AsyncWebServerRequest* req, const String& filename, size_t index, uint8_t* data, size_t len, bool final){
                apiOtaUpload(req, filename, index, data, len, final);
            });

        // SafeBoot / FactoryReset
        _svr.on("/api/safeboot", HTTP_GET, [this](AsyncWebServerRequest* req){ apiSafeBootGet(req); });
        _svr.on("/api/safeboot", HTTP_POST, [this](AsyncWebServerRequest* req){ (void)req; }, nullptr,
            [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
                apiSafeBootPost(req, data, len, index, total);
            });

        _svr.on("/api/factory_reset", HTTP_POST, [this](AsyncWebServerRequest* req){ (void)req; }, nullptr,
            [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
                (void)data; (void)len; (void)index; (void)total;
                apiFactoryReset(req);
            });

        _svr.on("/api/reboot", HTTP_POST, [this](AsyncWebServerRequest* req){
            req->send(200, "application/json", "{\"ok\":true}");
            delay(50);
            ESP.restart();
        });

        // ---- 정적 라우팅(동적 서빙) ----
        // 1) 루트는 index로 유도
        _svr.on("/", HTTP_GET, [this](AsyncWebServerRequest* req){
            // no-store (항상 최신 HTML)
            req->redirect("/www/index_0276.html");
        });

        // 2) NotFound에서 화이트리스트 기반 동적 서빙 처리
        _svr.onNotFound([this](AsyncWebServerRequest* req){
            handleDynamicStatic(req);
        });

        _svr.begin();
    }

  private:
    // =====================================================
    // WiFi
    // =====================================================
    void setupWiFi(){
        WiFi.mode(WIFI_MODE_NULL);

        const bool v_hasSta = (_wifi.sta_ssid[0] != '\0');
        const bool v_autoM  = (_wifi.mode == (uint8_t)EN_C10_WIFI_AUTO);
        const bool v_forceAp  = (_wifi.mode == (uint8_t)EN_C10_WIFI_AP);
        const bool v_forceSta = (_wifi.mode == (uint8_t)EN_C10_WIFI_STA);

        // (0275) SafeBoot: 무조건 AP 진입
        if(_cfg && _cfg->isSafeMode()){
            char v_ssid[33]; memset(v_ssid, 0, sizeof(v_ssid));
            strlcpy(v_ssid, _wifi.ap_ssid, sizeof(v_ssid));
            if(strlen(v_ssid) <= 28) strlcat(v_ssid, "-SAFE", sizeof(v_ssid));

            WiFi.mode(WIFI_AP);
            if(_wifi.ap_pass[0] != '\0') WiFi.softAP(v_ssid, _wifi.ap_pass);
            else WiFi.softAP(v_ssid);
            return;
        }

        if(v_forceAp || (!v_hasSta && (v_autoM || !v_forceSta))){
            startAp();
            return;
        }

        // STA try
        WiFi.mode(WIFI_STA);
        WiFi.begin(_wifi.sta_ssid, _wifi.sta_pass);

        const uint32_t v_t0 = millis();
        bool v_ok = false;
        while(millis() - v_t0 < 8000){
            if(WiFi.status() == WL_CONNECTED){ v_ok = true; break; }
            delay(200);
        }

        if(v_ok){
            startMdns();
            return;
        }

        // STA 실패 시 AUTO면 AP fallback
        if(v_autoM){
            startAp();
            return;
        }

        // forceSta 실패면 그대로 유지(정책)
    }

    void startAp(){
        WiFi.mode(WIFI_AP);
        // pass 비어있으면 open AP 허용 가능하지만, 여기선 기존 정책 유지
        WiFi.softAP(_wifi.ap_ssid, _wifi.ap_pass);
    }

    void onWifiEvent(WiFiEvent_t p_e){
        if(p_e == ARDUINO_EVENT_WIFI_STA_GOT_IP) startMdns();
        if(p_e == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) stopMdns();
    }

    void startMdns(){
        if(_mdnsStarted) return;
        if(_wifi.mdns_host[0] == '\0') return;
        if(WiFi.getMode() != WIFI_STA || WiFi.status() != WL_CONNECTED) return;
        if(!MDNS.begin(_wifi.mdns_host)) return;
        MDNS.addService("http", "tcp", 80);
        _mdnsStarted = true;
    }

    void stopMdns(){
        if(!_mdnsStarted) return;
        MDNS.end();
        _mdnsStarted = false;
    }

    // =====================================================
    // Dynamic static serving (화이트리스트 + 확장자 제한)
    // =====================================================
    // 상세 설명:
    // - 자산 테이블 없이도 /www/* 경로를 LittleFS에서 직접 서빙
    // - 보안상 허용 경로/확장자를 엄격 제한:
    //    * /www/* : html/css/js/svg/png/webp/ico
    //    * /favicon.ico, /robots.txt : top-level만
    //    * /json/public/* : json만
    // - gzip는 html/css/js만 사용(.gz 존재 + Accept-Encoding:gzip)
    void handleDynamicStatic(AsyncWebServerRequest* req){
        const String v_uri = req->url();

        // 0) API는 여기서 처리하지 않음(위에서 라우팅됨)
        if(v_uri.startsWith("/api/")){
            req->send(404, "text/plain", "not found");
            return;
        }

        // 1) favicon / robots 허용
        if(v_uri == "/favicon.ico"){
            serveStaticFile(req, "/www/favicon.ico", true /*immutable candidate*/);
            return;
        }
        if(v_uri == "/robots.txt"){
            // robots는 자주 바뀌지 않지만 운영 정책에 따라 short
            serveStaticFile(req, "/www/robots.txt", false);
            return;
        }

        // 2) /www/* 만 허용 (디렉토리 트래버설 방지)
        if(v_uri.startsWith(G_W10_URI_WWW_PREFIX)){
            // 파일 경로로 변환: "/www/xxx" -> "/www/xxx"
            // (LittleFS 루트 기준 동일)
            const String v_path = v_uri; // 그대로
            if(!isPathSafe(v_path.c_str())){
                req->send(403, "text/plain", "forbidden");
                return;
            }
            serveStaticFile(req, v_path.c_str(), true /*immutable candidate*/);
            return;
        }

        // 3) /json/public/* 만 허용 (읽기 전용 공개 json)
        if(v_uri.startsWith(G_W10_URI_JSON_PUBLIC_PREFIX)){
            const String v_path = v_uri;
            if(!isPathSafe(v_path.c_str())){
                req->send(403, "text/plain", "forbidden");
                return;
            }
            servePublicJson(req, v_path.c_str());
            return;
        }

        // 그 외 모두 차단
        req->send(404, "text/plain", "not found");
    }

    // 경로 안전성 최소 검증:
    // - ".." 금지
    // - "//" 연속 금지(원하는 경우)
    bool isPathSafe(const char* p_path){
        if(!p_path) return false;
        if(strstr(p_path, "..")) return false;
        if(strstr(p_path, "//")) return false;
        return true;
    }

    bool acceptsGzip(AsyncWebServerRequest* req){
        if(!req->hasHeader("Accept-Encoding")) return false;
        const String v_ae = req->header("Accept-Encoding");
        return (v_ae.indexOf("gzip") >= 0);
    }

    // 확장자 추출(소문자)
    // - 반환 포인터는 p_outExt 버퍼
    const char* getLowerExt(const char* p_path, char* p_outExt, size_t p_outSize){
        if(!p_path || !p_outExt || p_outSize < 2) return nullptr;
        p_outExt[0] = '\0';

        const char* v_dot = strrchr(p_path, '.');
        if(!v_dot || v_dot == p_path) return nullptr;

        const char* v_ext = v_dot + 1;
        size_t v_len = strlen(v_ext);
        if(v_len == 0 || v_len >= p_outSize) return nullptr;

        for(size_t i=0;i<v_len;i++){
            char c = v_ext[i];
            if(c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
            p_outExt[i] = c;
        }
        p_outExt[v_len] = '\0';
        return p_outExt;
    }

    const char* contentTypeFromExt(const char* p_extLower){
        if(!p_extLower) return "application/octet-stream";
        if(strcmp(p_extLower,"html")==0) return "text/html";
        if(strcmp(p_extLower,"css")==0)  return "text/css";
        if(strcmp(p_extLower,"js")==0)   return "application/javascript";
        if(strcmp(p_extLower,"svg")==0)  return "image/svg+xml";
        if(strcmp(p_extLower,"png")==0)  return "image/png";
        if(strcmp(p_extLower,"webp")==0) return "image/webp";
        if(strcmp(p_extLower,"ico")==0)  return "image/x-icon";
        if(strcmp(p_extLower,"json")==0) return "application/json";
        if(strcmp(p_extLower,"txt")==0)  return "text/plain";
        return "application/octet-stream";
    }

    // Cache-Control 자동 분류 규칙
    // - no-store: html, json/public, api
    // - immutable: 버전 토큰 포함 + 정적 리소스
    // - short: 나머지 정적 리소스
    const char* cacheControlForStatic(const char* p_path, const char* p_extLower, bool p_isPublicJson){
        (void)p_extLower;

        if(p_isPublicJson) return G_W10_CACHE_NOSTORE;
        if(p_extLower && strcmp(p_extLower,"html")==0) return G_W10_CACHE_NOSTORE;

        // 버전 토큰 있으면 immutable
        if(W10_hasVersionToken(p_path)) return G_W10_CACHE_IMMUTABLE;

        // 그 외는 short
        return G_W10_CACHE_SHORT;
    }

    // /www/* 정적파일 서빙
    void serveStaticFile(AsyncWebServerRequest* req, const char* p_path, bool p_immutableCandidate){
        (void)p_immutableCandidate;

        char v_ext[12]; memset(v_ext,0,sizeof(v_ext));
        const char* v_extLower = getLowerExt(p_path, v_ext, sizeof(v_ext));

        // /www/* 허용 확장자 제한
        if(!v_extLower || !W10_isAllowedWwwExt(v_extLower)){
            req->send(403, "text/plain", "forbidden");
            return;
        }

        // 존재 확인(원본 또는 gz)
        const bool v_isGzipTarget = (strcmp(v_extLower,"html")==0) || (strcmp(v_extLower,"css")==0) || (strcmp(v_extLower,"js")==0);
        bool v_useGz = false;

        String v_gzPath;
        if(v_isGzipTarget){
            v_gzPath = String(p_path) + ".gz";
            if(LittleFS.exists(v_gzPath) && acceptsGzip(req)){
                v_useGz = true;
            }
        }

        const char* v_sendPath = p_path;
        if(v_useGz) v_sendPath = v_gzPath.c_str();

        if(!LittleFS.exists(v_sendPath)){
            req->send(404, "text/plain", "not found");
            return;
        }

        const char* v_ct = contentTypeFromExt(v_extLower);
        AsyncWebServerResponse* res = req->beginResponse(LittleFS, v_sendPath, v_ct);

        if(v_useGz) res->addHeader("Content-Encoding", "gzip");

        const char* v_cc = cacheControlForStatic(p_path, v_extLower, false);
        res->addHeader("Cache-Control", v_cc);

        req->send(res);
    }

    // /json/public/* 서빙 (json만 허용, no-store)
    void servePublicJson(AsyncWebServerRequest* req, const char* p_path){
        char v_ext[12]; memset(v_ext,0,sizeof(v_ext));
        const char* v_extLower = getLowerExt(p_path, v_ext, sizeof(v_ext));

        if(!v_extLower || !W10_isAllowedPublicJsonExt(v_extLower)){
            req->send(403, "text/plain", "forbidden");
            return;
        }

        if(!LittleFS.exists(p_path)){
            req->send(404, "text/plain", "not found");
            return;
        }

        AsyncWebServerResponse* res = req->beginResponse(LittleFS, p_path, "application/json");
        res->addHeader("Cache-Control", G_W10_CACHE_NOSTORE);
        req->send(res);
    }

    // =====================================================
    // Common helpers
    // =====================================================
    void sendJson(AsyncWebServerRequest* req, JsonDocument& d, int p_code=200){
        String out; serializeJson(d, out);
        req->send(p_code, "application/json", out);
    }

    // AsyncWebServer body accumulator (req->_tempObject 사용)
    String* reqBody(AsyncWebServerRequest* req, size_t p_index){
        if(p_index == 0){
            if(req->_tempObject){
                delete (String*)req->_tempObject;
                req->_tempObject = nullptr;
            }
            req->_tempObject = new String();
        }
        return (String*)req->_tempObject;
    }

    void reqBodyFree(AsyncWebServerRequest* req){
        if(req->_tempObject){
            delete (String*)req->_tempObject;
            req->_tempObject = nullptr;
        }
    }

    // =====================================================
    // /api/status
    // =====================================================
    void apiStatus(AsyncWebServerRequest* req){
        JsonDocument d;
        d["uptime_ms"] = (uint32_t)millis();
        d["heap_free"] = (uint32_t)ESP.getFreeHeap();

        JsonObject net = d["net"].to<JsonObject>();
        net["mode"] = (WiFi.getMode() == WIFI_AP) ? "AP" : "STA";
        net["ip"] = (WiFi.getMode() == WIFI_AP) ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
        net["ssid"] = (WiFi.getMode() == WIFI_AP) ? String(_wifi.ap_ssid) : WiFi.SSID();
        net["mdns"] = String(_wifi.mdns_host) + ".local";

        CL_E10_EliteAirMouse* e10 = (CL_E10_EliteAirMouse*)_applyCtx;
        if(e10){
            ST_E10_Status_t s; e10->getStatus(s);

            JsonObject e = d["e10"].to<JsonObject>();
            e["ble_connected"] = s.ble_connected;
            e["ppt_mode"] = s.ppt_mode;
            e["dpi_level"] = s.dpi_level;
            e["precision_enable"] = s.precision_enable;
            e["precision_mode"] = s.precision_mode;
            e["fsm_state"] = s.fsm_state;
            e["fsm_sub"] = s.fsm_sub;

            JsonObject h = e["health"].to<JsonObject>();
            h["state"] = s.health;
            h["score"] = s.health_score;

            JsonObject gyro = e["gyro"].to<JsonObject>();
            gyro["bias_x"] = s.gyro_bias_x;
            gyro["bias_y"] = s.gyro_bias_y;
            gyro["bias_z"] = s.gyro_bias_z;
            gyro["rms"] = s.gyro_rms;

            e["cursor_rms"] = s.cursor_rms;
            e["temp_c"] = s.temp_c;

            JsonObject i2c = e["i2c"].to<JsonObject>();
            i2c["recover_count"] = s.i2c_recover_count;
            i2c["recover_last_ok"] = s.i2c_recover_last_ok;

            JsonObject err = e["err"].to<JsonObject>();
            err["mpu_nan"] = s.err_mpu_nan;
            err["mutex_miss"] = s.err_mutex_miss;
            err["task_overrun"] = s.err_task_overrun;

            JsonObject an = e["anomaly"].to<JsonObject>();
            an["spike_count_10s"] = s.spike_count_10s;
            an["consecutive_fail"] = s.consecutive_fail;
            an["consecutive_recover_fail"] = s.consecutive_recover_fail;

            JsonArray hist = e["err_hist"].to<JsonArray>();
            for(uint8_t i=0;i<s.err_hist_n;i++){
                JsonObject o = hist.add<JsonObject>();
                o["ts_ms"] = s.err_hist[i].ts_ms;
                o["code"]  = s.err_hist[i].code;
                o["value"] = s.err_hist[i].value;
            }
        }

        JsonObject ota = d["ota"].to<JsonObject>();
        ota["in_progress"] = _otaInProgress;
        ota["total"] = (uint32_t)_otaTotal;
        ota["written"] = (uint32_t)_otaWritten;
        ota["ok"] = _otaOk;
        ota["err"] = _otaErr;

        if(_cfg){
            ST_C10_BootState_t bs; _cfg->getBootState(bs);
            JsonObject b = d["boot"].to<JsonObject>();
            b["safe_mode"] = bs.safe_mode;
            b["fail_count"] = bs.fail_count;
            b["pending"] = bs.pending;
        }

        sendJson(req, d);
    }

    // =====================================================
    // /api/keycodes
    // =====================================================
    const char* kbName(uint16_t p_code){
        switch(p_code){
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

    void apiKeycodes(AsyncWebServerRequest* req){
        JsonDocument d;

        JsonArray mods = d["mods"].to<JsonArray>();
        for(size_t i=0;i<sizeof(G_W10_MODS)/sizeof(G_W10_MODS[0]);i++){
            JsonObject o = mods.add<JsonObject>();
            o["name"] = G_W10_MODS[i].name;
            o["mask"] = G_W10_MODS[i].mask;
        }

        JsonArray kb = d["kb"].to<JsonArray>();
        char nameBuf[8];
        for(uint16_t code=0; code<=0xE7; code++){
            const char* n = kbName(code);
            if(!n){
                snprintf(nameBuf, sizeof(nameBuf), "0x%02X", (unsigned)code);
                n = nameBuf;
            }
            JsonObject o = kb.add<JsonObject>();
            o["name"] = n;
            o["code"] = code;
        }

        JsonArray con = d["consumer"].to<JsonArray>();
        for(size_t i=0;i<sizeof(G_W10_CONSUMER)/sizeof(G_W10_CONSUMER[0]);i++){
            JsonObject o = con.add<JsonObject>();
            o["name"] = G_W10_CONSUMER[i].name;
            o["mask"] = (uint32_t)G_W10_CONSUMER[i].mask;
        }

        d["note"] = "mods mask == HID modifier byte. kb=usage-id(0x07), consumer=32-bit mask.";
        sendJson(req, d);
    }

    // =====================================================
    // /api/config (export/import/save/apply/rollback)
    // =====================================================
    void apiGetConfig(AsyncWebServerRequest* req){
        String json;
        if(!_cfg->exportJson(json)){
            req->send(500, "application/json", "{\"ok\":false}");
            return;
        }
        // config 응답은 항상 no-store가 안전(브라우저 캐시로 인한 혼동 방지)
        AsyncWebServerResponse* res = req->beginResponse(200, "application/json", json);
        res->addHeader("Cache-Control", G_W10_CACHE_NOSTORE);
        req->send(res);
    }

    void apiConfigSave(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
        String* body = reqBody(req, index);
        if(!body){ req->send(500, "application/json", "{\"ok\":false}"); return; }

        for(size_t i=0;i<len;i++) (*body) += (char)data[i];
        if(index + len < total) return;

        bool saved=false, applied=false;
        bool ok = _cfg->importJson(*body, saved, applied);
        reqBodyFree(req);

        if(ok && saved && _applyFn) applied = _applyFn(_applyCtx);

        JsonDocument out;
        out["ok"] = ok;
        out["saved"] = saved;
        out["applied"] = applied;
        out["note"] = "WiFi changes require reboot.";
        sendJson(req, out, ok ? 200 : 400);
    }

    void apiConfigApply(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
        String* body = reqBody(req, index);
        if(!body){ req->send(500, "application/json", "{\"ok\":false}"); return; }

        for(size_t i=0;i<len;i++) (*body) += (char)data[i];
        if(index + len < total) return;

        ST_C10_WiFiConfig_t w; ST_C10_E10Config_t e;
        _cfg->makeDefaultsWiFi(w); _cfg->makeDefaultsE10(e);
        (void)_cfg->loadAll(w, e);

        bool ok = true;
        ok = ok && _cfg->patchFromJsonWiFi(*body, w);
        ok = ok && _cfg->patchFromJsonE10(*body, e);
        ok = ok && _cfg->validateWiFi(w);
        ok = ok && _cfg->validateE10(e);

        bool applied = false;
        CL_E10_EliteAirMouse* e10 = (CL_E10_EliteAirMouse*)_applyCtx;
        if(ok && e10){
            applied = e10->applyRuntimeE10(e);
        }

        reqBodyFree(req);

        JsonDocument out;
        out["ok"] = ok;
        out["applied"] = applied;
        out["note"] = "apply-only: not saved. WiFi fields validated but not applied to WiFi runtime.";
        sendJson(req, out, ok ? 200 : 400);
    }

    void apiExport(AsyncWebServerRequest* req){
        String json;
        if(!_cfg->exportJson(json)){
            req->send(500, "application/json", "{\"ok\":false}");
            return;
        }
        AsyncWebServerResponse* res = req->beginResponse(200, "application/json", json);
        res->addHeader("Content-Disposition", "attachment; filename=\"config_0272.json\"");
        res->addHeader("Cache-Control", G_W10_CACHE_NOSTORE);
        req->send(res);
    }

    void apiImport(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
        String* body = reqBody(req, index);
        if(!body){ req->send(500, "application/json", "{\"ok\":false}"); return; }

        for(size_t i=0;i<len;i++) (*body) += (char)data[i];
        if(index + len < total) return;

        bool saved=false, applied=false;
        bool ok = _cfg->importJson(*body, saved, applied);
        reqBodyFree(req);

        if(ok && saved && _applyFn) applied = _applyFn(_applyCtx);

        JsonDocument out;
        out["ok"] = ok;
        out["saved"] = saved;
        out["applied"] = applied;
        sendJson(req, out, ok ? 200 : 400);
    }

    void apiRollback(AsyncWebServerRequest* req){
        bool ok = _cfg->rollbackFromBak();
        bool applied = false;
        if(ok && _applyFn) applied = _applyFn(_applyCtx);

        JsonDocument out;
        out["ok"] = ok;
        out["applied"] = applied;
        sendJson(req, out, ok ? 200 : 400);
    }

    // =====================================================
    // /api/control
    // =====================================================
    void apiControl(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
        String* body = reqBody(req, index);
        if(!body){ req->send(500, "application/json", "{\"ok\":false}"); return; }

        for(size_t i=0;i<len;i++) (*body) += (char)data[i];
        if(index + len < total) return;

        JsonDocument d;
        DeserializationError err = deserializeJson(d, *body);
        reqBodyFree(req);
        if(err){ req->send(400, "application/json", "{\"ok\":false,\"err\":\"bad_json\"}"); return; }

        CL_E10_EliteAirMouse* e10 = (CL_E10_EliteAirMouse*)_applyCtx;
        bool ok = true;
        if(e10){
            if(!d["ppt_mode"].isNull()) ok = ok && e10->setPptMode((bool)d["ppt_mode"]);
            if(!d["dpi_level"].isNull()) ok = ok && e10->setDpiLevel((uint8_t)d["dpi_level"]);
            if(!d["precision_mode"].isNull()) ok = ok && e10->setPrecisionMode((bool)d["precision_mode"]);
        } else {
            ok = false;
        }

        JsonDocument out; out["ok"] = ok;
        sendJson(req, out, ok ? 200 : 500);
    }

    // =====================================================
    // /api/ppt
    // =====================================================
    void apiGetPpt(AsyncWebServerRequest* req){
        (void)_cfg->loadAll(_wifi, _e10);

        JsonDocument d;
        JsonObject map = d["map"].to<JsonObject>();

        auto put = [&](const char* n, const ST_C10_PptKey2_t& k){
            JsonObject o = map[n].to<JsonObject>();
            o["page"] = (k.page==(uint8_t)EN_C10_KEYPAGE_CONSUMER) ? "consumer" : "kb";
            o["mod"]  = k.mod;
            o["code"] = k.code;
        };

        put("start", _e10.ppt2_start);
        put("exit",  _e10.ppt2_exit);
        put("next",  _e10.ppt2_next);
        put("prev",  _e10.ppt2_prev);
        put("black", _e10.ppt2_black);
        put("laser", _e10.ppt2_laser);

        sendJson(req, d);
    }

    void apiPostPpt(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
        String* body = reqBody(req, index);
        if(!body){ req->send(500, "application/json", "{\"ok\":false}"); return; }

        for(size_t i=0;i<len;i++) (*body) += (char)data[i];
        if(index + len < total) return;

        JsonDocument d;
        DeserializationError err = deserializeJson(d, *body);
        reqBodyFree(req);
        if(err){ req->send(400, "application/json", "{\"ok\":false,\"err\":\"bad_json\"}"); return; }

        bool save = true;
        if(!d["save"].isNull()) save = (bool)d["save"];

        JsonVariant map = d["map"];
        if(map.isNull()){ req->send(400, "application/json", "{\"ok\":false,\"err\":\"no_map\"}"); return; }

        ST_C10_WiFiConfig_t w; ST_C10_E10Config_t e;
        _cfg->makeDefaultsWiFi(w); _cfg->makeDefaultsE10(e);
        (void)_cfg->loadAll(w, e);

        auto loadK = [&](const char* n, ST_C10_PptKey2_t& k){
            JsonVariant o = map[n];
            if(o.isNull()) return;

            if(!o["page"].isNull()){
                const char* s = (const char*)o["page"];
                if(s && strcasecmp(s, "consumer")==0) k.page = (uint8_t)EN_C10_KEYPAGE_CONSUMER;
                else k.page = (uint8_t)EN_C10_KEYPAGE_KB;
            }
            if(!o["mod"].isNull())  k.mod  = (uint8_t)o["mod"];
            if(!o["code"].isNull()) k.code = (uint32_t)o["code"];
        };

        loadK("start", e.ppt2_start);
        loadK("exit",  e.ppt2_exit);
        loadK("next",  e.ppt2_next);
        loadK("prev",  e.ppt2_prev);
        loadK("black", e.ppt2_black);
        loadK("laser", e.ppt2_laser);

        bool ok = _cfg->validateE10(e);
        bool saved = false;
        bool applied = false;

        if(ok && save){
            ok = _cfg->saveAll(w, e);
            saved = ok;
        }

        CL_E10_EliteAirMouse* e10 = (CL_E10_EliteAirMouse*)_applyCtx;
        if(ok && e10){
            applied = e10->applyRuntimeE10(e);
        }

        JsonDocument out;
        out["ok"] = ok;
        out["saved"] = saved;
        out["applied"] = applied;
        sendJson(req, out, ok ? 200 : 400);
    }

    void apiPptTest(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
        String* body = reqBody(req, index);
        if(!body){ req->send(500, "application/json", "{\"ok\":false}"); return; }

        for(size_t i=0;i<len;i++) (*body) += (char)data[i];
        if(index + len < total) return;

        JsonDocument d;
        DeserializationError err = deserializeJson(d, *body);
        reqBodyFree(req);
        if(err){ req->send(400, "application/json", "{\"ok\":false,\"err\":\"bad_json\"}"); return; }

        uint8_t page = (uint8_t)EN_C10_KEYPAGE_KB;
        uint8_t mod = 0;
        uint32_t code = 0;

        if(!d["page"].isNull()){
            const char* s = (const char*)d["page"];
            if(s && strcasecmp(s, "consumer")==0) page = (uint8_t)EN_C10_KEYPAGE_CONSUMER;
        }
        if(!d["mod"].isNull())  mod = (uint8_t)d["mod"];
        if(!d["code"].isNull()) code = (uint32_t)d["code"];

        CL_E10_EliteAirMouse* e10 = (CL_E10_EliteAirMouse*)_applyCtx;
        bool ok = false;
        if(e10) ok = e10->testPptKey2(page, mod, code);

        JsonDocument out; out["ok"] = ok;
        sendJson(req, out, ok ? 200 : 500);
    }

    // =====================================================
    // OTA
    // =====================================================
    void apiOtaUpload(AsyncWebServerRequest* req, const String& filename, size_t index, uint8_t* data, size_t len, bool final){
        (void)filename;

        if(index == 0){
            if(_otaInProgress){
                _otaOk = false;
                strlcpy(_otaErr, "busy", sizeof(_otaErr));
                return;
            }
            _otaInProgress = true;
            _otaWritten = 0;
            _otaTotal = (uint32_t)req->contentLength();
            _otaOk = false;
            strlcpy(_otaErr, "in_progress", sizeof(_otaErr));

            size_t sketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
            if(_otaTotal == 0 || _otaTotal > (uint32_t)sketchSpace){
                _otaOk = false;
                strlcpy(_otaErr, "size_invalid", sizeof(_otaErr));
                _otaInProgress = false;
                return;
            }

            if(!Update.begin(sketchSpace)){
                _otaOk = false;
                strlcpy(_otaErr, "Update.begin failed", sizeof(_otaErr));
                _otaInProgress = false;
                return;
            }
        }

        if(len){
            size_t w = Update.write(data, len);
            _otaWritten += (uint32_t)w;
            if(w != len) strlcpy(_otaErr, "Update.write mismatch", sizeof(_otaErr));
        }

        if(final){
            if(!Update.end(true)){
                strlcpy(_otaErr, "Update.end failed", sizeof(_otaErr));
                _otaOk = false;
            } else if(Update.hasError()){
                strlcpy(_otaErr, "Update.hasError", sizeof(_otaErr));
                _otaOk = false;
            } else {
                _otaOk = true;
                strlcpy(_otaErr, "ok", sizeof(_otaErr));
            }
            _otaInProgress = false;
        }
    }

    void apiOtaStatus(AsyncWebServerRequest* req){
        JsonDocument d;
        d["in_progress"] = _otaInProgress;
        d["total"] = (uint32_t)_otaTotal;
        d["written"] = (uint32_t)_otaWritten;
        d["ok"] = _otaOk;
        d["err"] = _otaErr;
        sendJson(req, d);
    }

    // =====================================================
    // SafeBoot / FactoryReset
    // =====================================================
    void apiSafeBootGet(AsyncWebServerRequest* req){
        JsonDocument d;
        if(_cfg){
            ST_C10_BootState_t bs; _cfg->getBootState(bs);
            d["ok"] = true;
            d["safe_mode"] = bs.safe_mode;
            d["fail_count"] = bs.fail_count;
            d["pending"] = bs.pending;
        } else {
            d["ok"] = false;
        }
        sendJson(req, d, d["ok"] ? 200 : 500);
    }

    void apiSafeBootPost(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
        String* body = reqBody(req, index);
        if(!body){ req->send(500, "application/json", "{\"ok\":false}"); return; }

        for(size_t i=0;i<len;i++) (*body) += (char)data[i];
        if(index + len < total) return;

        JsonDocument in;
        DeserializationError err = deserializeJson(in, *body);
        reqBodyFree(req);
        if(err){ req->send(400, "application/json", "{\"ok\":false,\"err\":\"bad_json\"}"); return; }

        bool ok = false;
        if(_cfg && !in["exit"].isNull() && (bool)in["exit"]){
            ok = _cfg->clearSafeMode();
        }

        JsonDocument out;
        out["ok"] = ok;
        out["note"] = "If safe mode was active, reboot recommended after exit.";
        sendJson(req, out, ok ? 200 : 500);
    }

    void apiFactoryReset(AsyncWebServerRequest* req){
        bool ok = false;
        if(_cfg){
            ok = _cfg->factoryReset(true);
        }
        JsonDocument out;
        out["ok"] = ok;
        out["note"] = "Factory reset done. Rebooting...";
        sendJson(req, out, ok ? 200 : 500);
        if(ok){ delay(200); ESP.restart(); }
    }
};

