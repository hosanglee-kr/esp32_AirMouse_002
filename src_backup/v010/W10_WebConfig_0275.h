// =======================================================
// File: src/v010/W10_WebConfig_0275.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : W10_WebConfig_0275.h
 * 모듈약어 : W10
 * 모듈명 : Web Config/Status/UI/OTA Server (Field Dashboard + PPT + WiFi/E10 Editor)
 * ------------------------------------------------------
 * 기능 요약
 *  - 자산(HTML/CSS/JS) gzip 서빙 + 캐시 정책(자동 분류)
 *    - HTML(및 /) : no-store
 *    - css/js + 이미지(svg/png/webp/ico 등) : immutable(1y)
 *    - 기타 : public max-age(1h)
 *  - /api/status : 현장용 대시보드 데이터(uptime/heap/net/e10/ota/boot)
 *  - /api/keycodes : kb(0x00~0xE7) + consumer presets + mods(Modifier byte mask)
 *  - /api/ppt, /api/ppt/test : PPT Keymap(v2) 편집/즉시 테스트
 *  - /api/config : 현재 config_0272.json 원본 리턴
 *  - /api/config/save : 저장(atomic + .bak) + 런타임 apply(E10) + WiFi는 reboot 안내
 *  - /api/config/apply : 저장 없이 E10 런타임만 반영(검증 포함)
 *  - /api/config/export/import/rollback : 백업/복구(rollback 후 reload+apply까지)
 *  - /api/safeboot : SafeBoot 상태 조회/해제
 *  - /api/factory_reset : Factory reset 후 재부팅
 *  - /api/ota : Web OTA 업로드 + /api/ota/status
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

#include "C10_Config_0274.h"
#include "E10_EliteAirMouse_0272.h"

class CL_W10_WebConfig {
  private:
    AsyncWebServer _svr;

    CL_C10_Config* _cfg;
    bool (*_applyFn)(void*);
    void* _applyCtx;

    ST_C10_WiFiConfig_t _wifi;
    ST_C10_E10Config_t  _e10;

    bool _mdnsStarted;

    // OTA state (간단 진행률)
    volatile bool _otaInProgress;
    volatile uint32_t _otaTotal;
    volatile uint32_t _otaWritten;
    volatile bool _otaOk;
    char _otaErr[64];

    // ---- Static instance for WiFi event callback ----
    // (요구) inline static pointer 형태로 제공
    inline static CL_W10_WebConfig* s_instance = nullptr;

    // ---- Keycode presets ----
    struct ST_W10_Mod_t { const char* name; uint8_t mask; };
    static constexpr ST_W10_Mod_t s_mods[] = {
        {"None",0x00},{"LCtrl",0x01},{"LShift",0x02},{"LAlt",0x04},{"LMeta",0x08},
        {"RCtrl",0x10},{"RShift",0x20},{"RAlt",0x40},{"RMeta",0x80},
    };

    struct ST_W10_Consumer_t { const char* name; uint32_t mask; };
    static constexpr ST_W10_Consumer_t s_consumer[] = {
        {"None", 0x00000000},
        {"Play",0x00000001},{"Pause",0x00000002},{"Record",0x00000004},
        {"FastForward",0x00000008},{"Rewind",0x00000010},
        {"NextTrack",0x00000020},{"PrevTrack",0x00000040},
        {"Stop",0x00000080},{"Eject",0x00000100},{"RandomPlay",0x00000200},
        {"Repeat",0x00000400},{"PlayPause",0x00000800},
        {"Mute",0x00001000},{"VolumeUp",0x00002000},{"VolumeDown",0x00004000},
        {"WWWHome",0x00008000},{"MyComputer",0x00010000},{"Calculator",0x00020000},
        {"WWWFavorites",0x00040000},{"WWWSearch",0x00080000},{"WWWStop",0x00100000},
        {"WWWBack",0x00200000},{"MediaSelect",0x00400000},{"Mail",0x00800000},
    };

    // ---- Asset roots (LittleFS) ----
    // - UI 배포 규칙: /www 하위로 배치
    // - gzip 파일(.gz)은 html/css/js에만 존재한다고 가정
    static constexpr const char* s_wwwRoot = "/www";
    static constexpr const char* s_index   = "/www/index_0274.html";   // 파일명은 사용자가 FS에 올린대로 유지
    static constexpr const char* s_style   = "/www/style_0273.css";
    static constexpr const char* s_app     = "/www/app_0273.js";

  public:
    CL_W10_WebConfig()
    : _svr(80), _cfg(nullptr), _applyFn(nullptr), _applyCtx(nullptr),
      _mdnsStarted(false),
      _otaInProgress(false), _otaTotal(0), _otaWritten(0), _otaOk(false) {
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

        // FS는 main에서 mount 했어도, 여기서 한 번 더 호출해도 안전하게 실패/성공만 반환
        (void)LittleFS.begin(true);

        // config load
        if(_cfg){
            (void)_cfg->loadAll(_wifi, _e10);
        }

        // WiFi 이벤트 훅
        WiFi.onEvent(CL_W10_WebConfig::onWifiEventStatic);

        // WiFi bring-up
        setupWiFi();

        // ---------- Asset endpoints ----------
        // "/" -> index
        _svr.on("/", HTTP_GET, [this](AsyncWebServerRequest* p_req){
            this->serveFixedAsset(p_req, s_index);
        });

        // "/www/" -> index
        _svr.on("/www/", HTTP_GET, [this](AsyncWebServerRequest* p_req){
            this->serveFixedAsset(p_req, s_index);
        });

        // 고정 URI (편의)
        _svr.on("/www/style_0273.css", HTTP_GET, [this](AsyncWebServerRequest* p_req){
            this->serveFixedAsset(p_req, s_style);
        });
        _svr.on("/www/app_0273.js", HTTP_GET, [this](AsyncWebServerRequest* p_req){
            this->serveFixedAsset(p_req, s_app);
        });

        // "/www/*" 범용 서빙
        // - data/www/ 하위 폴더/파일 구조 동일 유지 요구 대응
        // - svg/png/webp 등은 원본만 복사되므로 gzip은 자동 제외
        _svr.on("^\\/www\\/(.*)$", HTTP_GET, [this](AsyncWebServerRequest* p_req){
            this->serveAnyWww(p_req);
        });

        // ---------- API endpoints ----------
        _svr.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* p_req){ this->apiStatus(p_req); });
        _svr.on("/api/keycodes", HTTP_GET, [this](AsyncWebServerRequest* p_req){ this->apiKeycodes(p_req); });

        // config raw
        _svr.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* p_req){ this->apiGetConfig(p_req); });

        // 저장(save) : atomic 저장 + apply(E10)
        _svr.on("/api/config/save", HTTP_POST,
            [this](AsyncWebServerRequest* p_req){ (void)p_req; },
            nullptr,
            [this](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total){
                this->apiConfigSave(p_req, p_data, p_len, p_index, p_total);
            });

        // apply-only : 저장 없이 런타임만 반영
        _svr.on("/api/config/apply", HTTP_POST,
            [this](AsyncWebServerRequest* p_req){ (void)p_req; },
            nullptr,
            [this](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total){
                this->apiConfigApply(p_req, p_data, p_len, p_index, p_total);
            });

        // export/import/rollback
        _svr.on("/api/config/export", HTTP_GET, [this](AsyncWebServerRequest* p_req){ this->apiExport(p_req); });

        _svr.on("/api/config/import", HTTP_POST,
            [this](AsyncWebServerRequest* p_req){ (void)p_req; },
            nullptr,
            [this](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total){
                this->apiImport(p_req, p_data, p_len, p_index, p_total);
            });

        // (중요) rollback 후 reload + apply까지 수행
        _svr.on("/api/config/rollback", HTTP_POST,
            [this](AsyncWebServerRequest* p_req){ (void)p_req; },
            nullptr,
            [this](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total){
                (void)p_data; (void)p_len; (void)p_index; (void)p_total;
                this->apiRollback(p_req);
            });

        // control
        _svr.on("/api/control", HTTP_POST,
            [this](AsyncWebServerRequest* p_req){ (void)p_req; },
            nullptr,
            [this](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total){
                this->apiControl(p_req, p_data, p_len, p_index, p_total);
            });

        // PPT
        _svr.on("/api/ppt", HTTP_GET, [this](AsyncWebServerRequest* p_req){ this->apiGetPpt(p_req); });

        _svr.on("/api/ppt", HTTP_POST,
            [this](AsyncWebServerRequest* p_req){ (void)p_req; },
            nullptr,
            [this](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total){
                this->apiPostPpt(p_req, p_data, p_len, p_index, p_total);
            });

        _svr.on("/api/ppt/test", HTTP_POST,
            [this](AsyncWebServerRequest* p_req){ (void)p_req; },
            nullptr,
            [this](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total){
                this->apiPptTest(p_req, p_data, p_len, p_index, p_total);
            });

        // SafeBoot / Factory reset
        _svr.on("/api/safeboot", HTTP_GET, [this](AsyncWebServerRequest* p_req){ this->apiSafeBootGet(p_req); });
        _svr.on("/api/safeboot", HTTP_POST,
            [this](AsyncWebServerRequest* p_req){ (void)p_req; },
            nullptr,
            [this](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total){
                this->apiSafeBootPost(p_req, p_data, p_len, p_index, p_total);
            });

        _svr.on("/api/factory_reset", HTTP_POST,
            [this](AsyncWebServerRequest* p_req){ (void)p_req; },
            nullptr,
            [this](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total){
                (void)p_data; (void)p_len; (void)p_index; (void)p_total;
                this->apiFactoryReset(p_req);
            });

        // OTA
        _svr.on("/api/ota/status", HTTP_GET, [this](AsyncWebServerRequest* p_req){ this->apiOtaStatus(p_req); });

        _svr.on("/api/ota", HTTP_POST,
            [this](AsyncWebServerRequest* p_req){
                // 업로드 완료 시점 응답
                JsonDocument v_doc;
                v_doc["ok"] = _otaOk;
                v_doc["err"] = _otaErr;
                v_doc["written"] = (uint32_t)_otaWritten;
                v_doc["total"] = (uint32_t)_otaTotal;

                String v_out;
                serializeJson(v_doc, v_out);

                p_req->send(_otaOk ? 200 : 500, "application/json", v_out);

                // 성공 시 재부팅
                if(_otaOk){
                    delay(200);
                    ESP.restart();
                }
            },
            [this](AsyncWebServerRequest* p_req, const String& p_filename, size_t p_index, uint8_t* p_data, size_t p_len, bool p_final){
                this->apiOtaUpload(p_req, p_filename, p_index, p_data, p_len, p_final);
            }
        );

        _svr.on("/api/reboot", HTTP_POST, [this](AsyncWebServerRequest* p_req){
            p_req->send(200, "application/json", "{\"ok\":true}");
            delay(50);
            ESP.restart();
        });

        _svr.onNotFound([this](AsyncWebServerRequest* p_req){
            // /www 로 시작하는데 못 찾은 경우, 404로 명확히
            p_req->send(404, "text/plain", "not found");
        });

        _svr.begin();
    }

  private:
    // =====================================================
    // WiFi / mDNS
    // =====================================================
    static void onWifiEventStatic(WiFiEvent_t p_event, WiFiEventInfo_t p_info){
        (void)p_info;
        if(s_instance){
            s_instance->onWifiEvent(p_event);
        }
    }

    void setupWiFi(){
        // WiFi 모드 초기화
        WiFi.mode(WIFI_MODE_NULL);

        const bool v_hasSta   = (_wifi.sta_ssid[0] != '\0');
        const bool v_autoM    = (_wifi.mode == (uint8_t)EN_C10_WIFI_AUTO);
        const bool v_forceAp  = (_wifi.mode == (uint8_t)EN_C10_WIFI_AP);
        const bool v_forceSta = (_wifi.mode == (uint8_t)EN_C10_WIFI_STA);

        // (0273~) Safe Boot: 무조건 AP 진입
        if(_cfg && _cfg->isSafeMode()){
            char v_ssid[33];
            memset(v_ssid, 0, sizeof(v_ssid));
            strlcpy(v_ssid, _wifi.ap_ssid, sizeof(v_ssid));
            if(strlen(v_ssid) <= 28){
                strlcat(v_ssid, "-SAFE", sizeof(v_ssid));
            }

            WiFi.mode(WIFI_AP);
            if(_wifi.ap_pass[0] != '\0'){
                WiFi.softAP(v_ssid, _wifi.ap_pass);
            }else{
                WiFi.softAP(v_ssid);
            }
            return;
        }

        // AP 강제 or AUTO에서 STA 정보가 없으면 AP
        if(v_forceAp || (!v_hasSta && (v_autoM || !v_forceSta))){
            startAp();
            return;
        }

        // STA 시도
        WiFi.mode(WIFI_STA);
        WiFi.begin(_wifi.sta_ssid, _wifi.sta_pass);

        const uint32_t v_t0 = millis();
        bool v_ok = false;
        while(millis() - v_t0 < 8000){
            if(WiFi.status() == WL_CONNECTED){
                v_ok = true;
                break;
            }
            delay(200);
        }

        if(v_ok){
            startMdns();
            return;
        }

        // STA 실패: AUTO면 AP로 폴백
        if(v_autoM){
            startAp();
            return;
        }

        // STA 강제인데 실패면 그대로 유지(정책)
        // 필요 시 여기서 "forceSta 실패 시 AP 폴백 허용" 옵션을 별도 키로 추가 가능
    }

    void startAp(){
        WiFi.mode(WIFI_AP);
        // pass 비어있으면 open AP도 가능하도록
        if(_wifi.ap_pass[0] != '\0'){
            WiFi.softAP(_wifi.ap_ssid, _wifi.ap_pass);
        }else{
            WiFi.softAP(_wifi.ap_ssid);
        }
    }

    void onWifiEvent(WiFiEvent_t p_event){
        if(p_event == ARDUINO_EVENT_WIFI_STA_GOT_IP){
            startMdns();
        }else if(p_event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED){
            stopMdns();
        }
    }

    void startMdns(){
        if(_mdnsStarted) return;
        if(_wifi.mdns_host[0] == '\0') return;
        if(WiFi.getMode() != WIFI_STA) return;
        if(WiFi.status() != WL_CONNECTED) return;

        if(!MDNS.begin(_wifi.mdns_host)){
            return;
        }
        MDNS.addService("http", "tcp", 80);
        _mdnsStarted = true;
    }

    void stopMdns(){
        if(!_mdnsStarted) return;
        MDNS.end();
        _mdnsStarted = false;
    }

    // =====================================================
    // Asset Serving (gzip + cache auto policy)
    // =====================================================
    bool acceptsGzip(AsyncWebServerRequest* p_req){
        if(!p_req->hasHeader("Accept-Encoding")) return false;
        const String v_ae = p_req->header("Accept-Encoding");
        return (v_ae.indexOf("gzip") >= 0);
    }

    // 확장자 추출(소문자 비교용)
    const char* extOf(const char* p_path){
        const char* v_dot = strrchr(p_path, '.');
        if(!v_dot) return "";
        return v_dot + 1;
    }

    bool isGzipTargetExt(const char* p_ext){
        // gzip은 html/css/js만
        return (strcasecmp(p_ext, "html") == 0 ||
                strcasecmp(p_ext, "css")  == 0 ||
                strcasecmp(p_ext, "js")   == 0);
    }

    const char* contentTypeByExt(const char* p_path){
        const char* v_ext = extOf(p_path);

        if(strcasecmp(v_ext, "html") == 0) return "text/html";
        if(strcasecmp(v_ext, "css")  == 0) return "text/css";
        if(strcasecmp(v_ext, "js")   == 0) return "application/javascript";
        if(strcasecmp(v_ext, "json") == 0) return "application/json";

        if(strcasecmp(v_ext, "svg")  == 0) return "image/svg+xml";
        if(strcasecmp(v_ext, "png")  == 0) return "image/png";
        if(strcasecmp(v_ext, "webp") == 0) return "image/webp";
        if(strcasecmp(v_ext, "ico")  == 0) return "image/x-icon";

        if(strcasecmp(v_ext, "txt")  == 0) return "text/plain";

        return "application/octet-stream";
    }

    // Cache-Control 자동 분류 규칙(요구 반영)
    // - HTML: no-store (config/ppt 같은 UI 동적 반영 때문)
    // - CSS/JS/이미지: immutable (배포 시 파일명에 버전이 들어가므로 캐시해도 안전)
    // - 나머지: public max-age=3600
    const char* cacheControlForPath(const char* p_uri, const char* p_fsPath){
        (void)p_uri;
        const char* v_ext = extOf(p_fsPath);

        // html은 무조건 no-store
        if(strcasecmp(v_ext, "html") == 0){
            return "no-store";
        }

        // css/js/이미지류는 immutable
        if(strcasecmp(v_ext, "css") == 0 ||
           strcasecmp(v_ext, "js")  == 0 ||
           strcasecmp(v_ext, "svg") == 0 ||
           strcasecmp(v_ext, "png") == 0 ||
           strcasecmp(v_ext, "webp")== 0 ||
           strcasecmp(v_ext, "ico") == 0){
            return "public, max-age=31536000, immutable";
        }

        // 기타는 적당히 캐시
        return "public, max-age=3600";
    }

    // 고정 자산(경로가 이미 FS 경로로 결정된 경우)
    void serveFixedAsset(AsyncWebServerRequest* p_req, const char* p_fsPath){
        serveFileWithOptionalGzip(p_req, p_req->url().c_str(), p_fsPath);
    }

    // /www/* 범용
    void serveAnyWww(AsyncWebServerRequest* p_req){
        // request url 예: /www/img/logo.png
        const String v_url = p_req->url();
        if(!v_url.startsWith("/www/")){
            p_req->send(404, "text/plain", "not found");
            return;
        }

        // FS path = "/www/..." 그대로 사용 (LittleFS는 절대경로)
        // 유저 요구: data/www/ 구조를 동일하게 유지하여 업로드하므로 그대로 매핑 가능
        const String v_fsPath = v_url;

        serveFileWithOptionalGzip(p_req, v_url.c_str(), v_fsPath.c_str());
    }

    void serveFileWithOptionalGzip(AsyncWebServerRequest* p_req, const char* p_uri, const char* p_fsPath){
        const char* v_type = contentTypeByExt(p_fsPath);
        const char* v_ext  = extOf(p_fsPath);

        bool v_useGz = false;
        String v_gzPath = String(p_fsPath) + ".gz";

        // gzip 대상(ext html/css/js) + 파일 존재 + 클라이언트 gzip 수용 시에만 사용
        if(isGzipTargetExt(v_ext) && acceptsGzip(p_req) && LittleFS.exists(v_gzPath)){
            v_useGz = true;
        }

        const char* v_pathToServe = p_fsPath;
        if(v_useGz){
            v_pathToServe = v_gzPath.c_str();
        }else{
            if(!LittleFS.exists(p_fsPath)){
                p_req->send(404, "text/plain", "asset not found");
                return;
            }
        }

        AsyncWebServerResponse* v_res = p_req->beginResponse(LittleFS, v_pathToServe, v_type);

        if(v_useGz){
            v_res->addHeader("Content-Encoding", "gzip");
        }

        v_res->addHeader("Cache-Control", cacheControlForPath(p_uri, p_fsPath));
        p_req->send(v_res);
    }

    // =====================================================
    // Request body helpers (chunked upload)
    // =====================================================
    String* reqBody(AsyncWebServerRequest* p_req, size_t p_index){
        // AsyncWebServerRequest::_tempObject를 이용한 단순 버퍼링
        if(p_index == 0){
            if(p_req->_tempObject){
                delete (String*)p_req->_tempObject;
                p_req->_tempObject = nullptr;
            }
            p_req->_tempObject = new String();
        }
        return (String*)p_req->_tempObject;
    }

    void reqBodyFree(AsyncWebServerRequest* p_req){
        if(p_req->_tempObject){
            delete (String*)p_req->_tempObject;
            p_req->_tempObject = nullptr;
        }
    }

    void sendJson(AsyncWebServerRequest* p_req, JsonDocument& p_doc, int p_code){
        String v_out;
        serializeJson(p_doc, v_out);
        p_req->send(p_code, "application/json", v_out);
    }

    // =====================================================
    // /api/status
    // =====================================================
    void apiStatus(AsyncWebServerRequest* p_req){
        JsonDocument v_doc;

        v_doc["uptime_ms"] = (uint32_t)millis();
        v_doc["heap_free"] = (uint32_t)ESP.getFreeHeap();

        JsonObject v_net = v_doc["net"].to<JsonObject>();
        v_net["mode"] = (WiFi.getMode() == WIFI_AP) ? "AP" : "STA";
        v_net["ip"]   = (WiFi.getMode() == WIFI_AP) ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
        v_net["ssid"] = (WiFi.getMode() == WIFI_AP) ? String(_wifi.ap_ssid) : WiFi.SSID();
        v_net["mdns"] = String(_wifi.mdns_host) + ".local";

        // E10 status
        CL_E10_EliteAirMouse* v_e10 = (CL_E10_EliteAirMouse*)_applyCtx;
        if(v_e10){
            ST_E10_Status_t v_s;
            v_e10->getStatus(v_s);

            JsonObject v_e = v_doc["e10"].to<JsonObject>();
            v_e["ble_connected"] = v_s.ble_connected;
            v_e["ppt_mode"]      = v_s.ppt_mode;
            v_e["dpi_level"]     = v_s.dpi_level;
            v_e["precision_enable"] = v_s.precision_enable;
            v_e["precision_mode"]   = v_s.precision_mode;
            v_e["fsm_state"] = v_s.fsm_state;
            v_e["fsm_sub"]   = v_s.fsm_sub;

            JsonObject v_h = v_e["health"].to<JsonObject>();
            v_h["state"] = v_s.health;
            v_h["score"] = v_s.health_score;

            JsonObject v_gyro = v_e["gyro"].to<JsonObject>();
            v_gyro["bias_x"] = v_s.gyro_bias_x;
            v_gyro["bias_y"] = v_s.gyro_bias_y;
            v_gyro["bias_z"] = v_s.gyro_bias_z;
            v_gyro["rms"]    = v_s.gyro_rms;

            v_e["cursor_rms"] = v_s.cursor_rms;
            v_e["temp_c"]     = v_s.temp_c;

            JsonObject v_i2c = v_e["i2c"].to<JsonObject>();
            v_i2c["recover_count"]   = v_s.i2c_recover_count;
            v_i2c["recover_last_ok"] = v_s.i2c_recover_last_ok;

            JsonObject v_err = v_e["err"].to<JsonObject>();
            v_err["mpu_nan"]     = v_s.err_mpu_nan;
            v_err["mutex_miss"]  = v_s.err_mutex_miss;
            v_err["task_overrun"]= v_s.err_task_overrun;

            JsonObject v_an = v_e["anomaly"].to<JsonObject>();
            v_an["spike_count_10s"] = v_s.spike_count_10s;
            v_an["consecutive_fail"] = v_s.consecutive_fail;
            v_an["consecutive_recover_fail"] = v_s.consecutive_recover_fail;

            JsonArray v_hist = v_e["err_hist"].to<JsonArray>();
            for(uint8_t i = 0; i < v_s.err_hist_n; i++){
                JsonObject v_o = v_hist.add<JsonObject>();
                v_o["ts_ms"] = v_s.err_hist[i].ts_ms;
                v_o["code"]  = v_s.err_hist[i].code;
                v_o["value"] = v_s.err_hist[i].value;
            }
        }

        // OTA
        JsonObject v_ota = v_doc["ota"].to<JsonObject>();
        v_ota["in_progress"] = _otaInProgress;
        v_ota["total"]       = (uint32_t)_otaTotal;
        v_ota["written"]     = (uint32_t)_otaWritten;
        v_ota["ok"]          = _otaOk;
        v_ota["err"]         = _otaErr;

        // SafeBoot state
        if(_cfg){
            ST_C10_BootState_t v_bs;
            _cfg->getBootState(v_bs);

            JsonObject v_b = v_doc["boot"].to<JsonObject>();
            v_b["safe_mode"]  = v_bs.safe_mode;
            v_b["fail_count"] = v_bs.fail_count;
            v_b["pending"]    = v_bs.pending;
        }

        sendJson(p_req, v_doc, 200);
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

    void apiKeycodes(AsyncWebServerRequest* p_req){
        JsonDocument v_doc;

        JsonArray v_mods = v_doc["mods"].to<JsonArray>();
        for(size_t i = 0; i < sizeof(s_mods)/sizeof(s_mods[0]); i++){
            JsonObject v_o = v_mods.add<JsonObject>();
            v_o["name"] = s_mods[i].name;
            v_o["mask"] = s_mods[i].mask;
        }

        JsonArray v_kb = v_doc["kb"].to<JsonArray>();
        char v_nameBuf[8];

        for(uint16_t code = 0; code <= 0xE7; code++){
            const char* v_n = kbName(code);
            if(!v_n){
                snprintf(v_nameBuf, sizeof(v_nameBuf), "0x%02X", (unsigned)code);
                v_n = v_nameBuf;
            }
            JsonObject v_o = v_kb.add<JsonObject>();
            v_o["name"] = v_n;
            v_o["code"] = code;
        }

        JsonArray v_con = v_doc["consumer"].to<JsonArray>();
        for(size_t i = 0; i < sizeof(s_consumer)/sizeof(s_consumer[0]); i++){
            JsonObject v_o = v_con.add<JsonObject>();
            v_o["name"] = s_consumer[i].name;
            v_o["mask"] = (uint32_t)s_consumer[i].mask;
        }

        v_doc["note"] = "mods mask == HID modifier byte. kb=usage-id(0x07), consumer=32-bit mask.";
        sendJson(p_req, v_doc, 200);
    }

    // =====================================================
    // /api/config
    // =====================================================
    void apiGetConfig(AsyncWebServerRequest* p_req){
        if(!_cfg){
            p_req->send(500, "application/json", "{\"ok\":false}");
            return;
        }

        String v_json;
        if(!_cfg->exportJson(v_json)){
            p_req->send(500, "application/json", "{\"ok\":false}");
            return;
        }
        p_req->send(200, "application/json", v_json);
    }

    void apiConfigSave(AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total){
        String* v_body = reqBody(p_req, p_index);
        if(!v_body){
            p_req->send(500, "application/json", "{\"ok\":false}");
            return;
        }
        for(size_t i = 0; i < p_len; i++){
            (*v_body) += (char)p_data[i];
        }
        if(p_index + p_len < p_total) return;

        bool v_saved = false;
        bool v_applied = false;

        bool v_ok = false;
        if(_cfg){
            v_ok = _cfg->importJson(*v_body, v_saved, v_applied);
        }

        reqBodyFree(p_req);

        // E10 런타임 적용(저장 성공 시)
        if(v_ok && v_saved && _applyFn){
            v_applied = _applyFn(_applyCtx);
        }

        JsonDocument v_out;
        v_out["ok"] = v_ok;
        v_out["saved"] = v_saved;
        v_out["applied"] = v_applied;
        v_out["note"] = "WiFi changes require reboot.";

        sendJson(p_req, v_out, v_ok ? 200 : 400);
    }

    void apiConfigApply(AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total){
        String* v_body = reqBody(p_req, p_index);
        if(!v_body){
            p_req->send(500, "application/json", "{\"ok\":false}");
            return;
        }
        for(size_t i = 0; i < p_len; i++){
            (*v_body) += (char)p_data[i];
        }
        if(p_index + p_len < p_total) return;

        bool v_ok = true;

        ST_C10_WiFiConfig_t v_w;
        ST_C10_E10Config_t  v_e;

        if(!_cfg){
            v_ok = false;
        }else{
            _cfg->makeDefaultsWiFi(v_w);
            _cfg->makeDefaultsE10(v_e);
            (void)_cfg->loadAll(v_w, v_e);

            v_ok = v_ok && _cfg->patchFromJsonWiFi(*v_body, v_w);
            v_ok = v_ok && _cfg->patchFromJsonE10(*v_body, v_e);

            v_ok = v_ok && _cfg->validateWiFi(v_w);
            v_ok = v_ok && _cfg->validateE10(v_e);
        }

        bool v_applied = false;
        if(v_ok){
            CL_E10_EliteAirMouse* v_e10 = (CL_E10_EliteAirMouse*)_applyCtx;
            if(v_e10){
                v_applied = v_e10->applyRuntimeE10(v_e);
            }else{
                v_ok = false;
            }
        }

        reqBodyFree(p_req);

        JsonDocument v_out;
        v_out["ok"] = v_ok;
        v_out["applied"] = v_applied;
        v_out["note"] = "apply-only: not saved. WiFi fields are validated but not applied to WiFi runtime.";

        sendJson(p_req, v_out, v_ok ? 200 : 400);
    }

    // =====================================================
    // export/import/rollback
    // =====================================================
    void apiExport(AsyncWebServerRequest* p_req){
        if(!_cfg){
            p_req->send(500, "application/json", "{\"ok\":false}");
            return;
        }

        String v_json;
        if(!_cfg->exportJson(v_json)){
            p_req->send(500, "application/json", "{\"ok\":false}");
            return;
        }

        AsyncWebServerResponse* v_res = p_req->beginResponse(200, "application/json", v_json);
        v_res->addHeader("Content-Disposition", "attachment; filename=\"config_0272.json\"");
        v_res->addHeader("Cache-Control", "no-store");
        p_req->send(v_res);
    }

    void apiImport(AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total){
        String* v_body = reqBody(p_req, p_index);
        if(!v_body){
            p_req->send(500, "application/json", "{\"ok\":false}");
            return;
        }
        for(size_t i = 0; i < p_len; i++){
            (*v_body) += (char)p_data[i];
        }
        if(p_index + p_len < p_total) return;

        bool v_saved = false;
        bool v_applied = false;

        bool v_ok = false;
        if(_cfg){
            v_ok = _cfg->importJson(*v_body, v_saved, v_applied);
        }

        reqBodyFree(p_req);

        if(v_ok && v_saved && _applyFn){
            v_applied = _applyFn(_applyCtx);
        }

        JsonDocument v_out;
        v_out["ok"] = v_ok;
        v_out["saved"] = v_saved;
        v_out["applied"] = v_applied;

        sendJson(p_req, v_out, v_ok ? 200 : 400);
    }

    void apiRollback(AsyncWebServerRequest* p_req){
        bool v_ok = false;
        bool v_applied = false;

        if(!_cfg){
            v_ok = false;
        }else{
            v_ok = _cfg->rollbackFromBak();

            // (중요) rollback은 파일만 되돌리므로, 반드시 reload 후 apply
            if(v_ok){
                (void)_cfg->loadAll(_wifi, _e10);

                CL_E10_EliteAirMouse* v_e10 = (CL_E10_EliteAirMouse*)_applyCtx;
                if(v_e10){
                    v_applied = v_e10->applyRuntimeE10(_e10);
                }else if(_applyFn){
                    // 혹시 applyFn이 내부에서 load+apply를 해주는 정책이라면 이 경로도 유지 가능
                    v_applied = _applyFn(_applyCtx);
                }
            }
        }

        JsonDocument v_out;
        v_out["ok"] = v_ok;
        v_out["applied"] = v_applied;

        sendJson(p_req, v_out, v_ok ? 200 : 400);
    }

    // =====================================================
    // /api/control
    // =====================================================
    void apiControl(AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total){
        String* v_body = reqBody(p_req, p_index);
        if(!v_body){
            p_req->send(500, "application/json", "{\"ok\":false}");
            return;
        }
        for(size_t i = 0; i < p_len; i++){
            (*v_body) += (char)p_data[i];
        }
        if(p_index + p_len < p_total) return;

        JsonDocument v_in;
        DeserializationError v_err = deserializeJson(v_in, *v_body);
        reqBodyFree(p_req);

        if(v_err){
            p_req->send(400, "application/json", "{\"ok\":false,\"err\":\"bad_json\"}");
            return;
        }

        CL_E10_EliteAirMouse* v_e10 = (CL_E10_EliteAirMouse*)_applyCtx;
        bool v_ok = true;

        if(!v_e10){
            v_ok = false;
        }else{
            if(!v_in["ppt_mode"].isNull())       v_ok = v_ok && v_e10->setPptMode((bool)v_in["ppt_mode"]);
            if(!v_in["dpi_level"].isNull())      v_ok = v_ok && v_e10->setDpiLevel((uint8_t)v_in["dpi_level"]);
            if(!v_in["precision_mode"].isNull()) v_ok = v_ok && v_e10->setPrecisionMode((bool)v_in["precision_mode"]);
        }

        JsonDocument v_out;
        v_out["ok"] = v_ok;
        sendJson(p_req, v_out, v_ok ? 200 : 500);
    }

    // =====================================================
    // /api/ppt
    // =====================================================
    void apiGetPpt(AsyncWebServerRequest* p_req){
        if(!_cfg){
            p_req->send(500, "application/json", "{\"ok\":false}");
            return;
        }

        (void)_cfg->loadAll(_wifi, _e10);

        JsonDocument v_doc;
        JsonObject v_map = v_doc["map"].to<JsonObject>();

        auto putKey = [&](const char* p_name, const ST_C10_PptKey2_t& p_k){
            JsonObject v_o = v_map[p_name].to<JsonObject>();
            v_o["page"] = (p_k.page == (uint8_t)EN_C10_KEYPAGE_CONSUMER) ? "consumer" : "kb";
            v_o["mod"]  = p_k.mod;
            v_o["code"] = p_k.code;
        };

        putKey("start", _e10.ppt2_start);
        putKey("exit",  _e10.ppt2_exit);
        putKey("next",  _e10.ppt2_next);
        putKey("prev",  _e10.ppt2_prev);
        putKey("black", _e10.ppt2_black);
        putKey("laser", _e10.ppt2_laser);

        sendJson(p_req, v_doc, 200);
    }

    void apiPostPpt(AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total){
        String* v_body = reqBody(p_req, p_index);
        if(!v_body){
            p_req->send(500, "application/json", "{\"ok\":false}");
            return;
        }
        for(size_t i = 0; i < p_len; i++){
            (*v_body) += (char)p_data[i];
        }
        if(p_index + p_len < p_total) return;

        JsonDocument v_in;
        DeserializationError v_err = deserializeJson(v_in, *v_body);
        reqBodyFree(p_req);

        if(v_err){
            p_req->send(400, "application/json", "{\"ok\":false,\"err\":\"bad_json\"}");
            return;
        }

        bool v_save = true;
        if(!v_in["save"].isNull()){
            v_save = (bool)v_in["save"];
        }

        JsonVariant v_map = v_in["map"];
        if(v_map.isNull()){
            p_req->send(400, "application/json", "{\"ok\":false,\"err\":\"no_map\"}");
            return;
        }

        if(!_cfg){
            p_req->send(500, "application/json", "{\"ok\":false}");
            return;
        }

        ST_C10_WiFiConfig_t v_w;
        ST_C10_E10Config_t  v_e;

        _cfg->makeDefaultsWiFi(v_w);
        _cfg->makeDefaultsE10(v_e);
        (void)_cfg->loadAll(v_w, v_e);

        auto loadKey = [&](const char* p_name, ST_C10_PptKey2_t& p_k){
            JsonVariant v_o = v_map[p_name];
            if(v_o.isNull()) return;

            if(!v_o["page"].isNull()){
                const char* v_s = (const char*)v_o["page"];
                if(v_s && strcasecmp(v_s, "consumer") == 0) p_k.page = (uint8_t)EN_C10_KEYPAGE_CONSUMER;
                else p_k.page = (uint8_t)EN_C10_KEYPAGE_KB;
            }
            if(!v_o["mod"].isNull())  p_k.mod  = (uint8_t)v_o["mod"];
            if(!v_o["code"].isNull()) p_k.code = (uint32_t)v_o["code"];
        };

        loadKey("start", v_e.ppt2_start);
        loadKey("exit",  v_e.ppt2_exit);
        loadKey("next",  v_e.ppt2_next);
        loadKey("prev",  v_e.ppt2_prev);
        loadKey("black", v_e.ppt2_black);
        loadKey("laser", v_e.ppt2_laser);

        bool v_ok = _cfg->validateE10(v_e);
        bool v_saved = false;
        bool v_applied = false;

        if(v_ok && v_save){
            v_ok = _cfg->saveAll(v_w, v_e);
            v_saved = v_ok;
        }

        CL_E10_EliteAirMouse* v_e10 = (CL_E10_EliteAirMouse*)_applyCtx;
        if(v_ok && v_e10){
            v_applied = v_e10->applyRuntimeE10(v_e);
        }

        JsonDocument v_out;
        v_out["ok"] = v_ok;
        v_out["saved"] = v_saved;
        v_out["applied"] = v_applied;

        sendJson(p_req, v_out, v_ok ? 200 : 400);
    }

    void apiPptTest(AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total){
        String* v_body = reqBody(p_req, p_index);
        if(!v_body){
            p_req->send(500, "application/json", "{\"ok\":false}");
            return;
        }
        for(size_t i = 0; i < p_len; i++){
            (*v_body) += (char)p_data[i];
        }
        if(p_index + p_len < p_total) return;

        JsonDocument v_in;
        DeserializationError v_err = deserializeJson(v_in, *v_body);
        reqBodyFree(p_req);

        if(v_err){
            p_req->send(400, "application/json", "{\"ok\":false,\"err\":\"bad_json\"}");
            return;
        }

        uint8_t v_page = (uint8_t)EN_C10_KEYPAGE_KB;
        uint8_t v_mod  = 0;
        uint32_t v_code = 0;

        if(!v_in["page"].isNull()){
            const char* v_s = (const char*)v_in["page"];
            if(v_s && strcasecmp(v_s, "consumer") == 0){
                v_page = (uint8_t)EN_C10_KEYPAGE_CONSUMER;
            }
        }
        if(!v_in["mod"].isNull())  v_mod  = (uint8_t)v_in["mod"];
        if(!v_in["code"].isNull()) v_code = (uint32_t)v_in["code"];

        CL_E10_EliteAirMouse* v_e10 = (CL_E10_EliteAirMouse*)_applyCtx;
        bool v_ok = false;
        if(v_e10){
            v_ok = v_e10->testPptKey2(v_page, v_mod, v_code);
        }

        JsonDocument v_out;
        v_out["ok"] = v_ok;
        sendJson(p_req, v_out, v_ok ? 200 : 500);
    }

    // =====================================================
    // /api/safeboot
    // =====================================================
    void apiSafeBootGet(AsyncWebServerRequest* p_req){
        JsonDocument v_doc;

        if(_cfg){
            ST_C10_BootState_t v_bs;
            _cfg->getBootState(v_bs);

            v_doc["ok"] = true;
            v_doc["safe_mode"] = v_bs.safe_mode;
            v_doc["fail_count"] = v_bs.fail_count;
            v_doc["pending"] = v_bs.pending;

            sendJson(p_req, v_doc, 200);
        }else{
            v_doc["ok"] = false;
            sendJson(p_req, v_doc, 500);
        }
    }

    void apiSafeBootPost(AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total){
        String* v_body = reqBody(p_req, p_index);
        if(!v_body){
            p_req->send(500, "application/json", "{\"ok\":false}");
            return;
        }
        for(size_t i = 0; i < p_len; i++){
            (*v_body) += (char)p_data[i];
        }
        if(p_index + p_len < p_total) return;

        JsonDocument v_in;
        DeserializationError v_err = deserializeJson(v_in, *v_body);
        reqBodyFree(p_req);

        if(v_err){
            p_req->send(400, "application/json", "{\"ok\":false,\"err\":\"bad_json\"}");
            return;
        }

        bool v_ok = false;
        if(_cfg && !v_in["exit"].isNull() && (bool)v_in["exit"]){
            v_ok = _cfg->clearSafeMode();
        }

        JsonDocument v_out;
        v_out["ok"] = v_ok;
        v_out["note"] = "If safe mode was active, reboot recommended after exit.";

        sendJson(p_req, v_out, v_ok ? 200 : 500);
    }

    // =====================================================
    // /api/factory_reset
    // =====================================================
    void apiFactoryReset(AsyncWebServerRequest* p_req){
        bool v_ok = false;
        if(_cfg){
            v_ok = _cfg->factoryReset(true);
        }

        JsonDocument v_out;
        v_out["ok"] = v_ok;
        v_out["note"] = "Factory reset done. Rebooting...";

        sendJson(p_req, v_out, v_ok ? 200 : 500);

        if(v_ok){
            delay(200);
            ESP.restart();
        }
    }

    // =====================================================
    // OTA upload + status
    // =====================================================
    void apiOtaUpload(AsyncWebServerRequest* p_req, const String& p_filename, size_t p_index, uint8_t* p_data, size_t p_len, bool p_final){
        (void)p_filename;

        // 업로드 시작 시점
        if(p_index == 0){
            if(_otaInProgress){
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
            if(_otaTotal == 0 || _otaTotal > (uint32_t)v_sketchSpace){
                _otaOk = false;
                strlcpy(_otaErr, "size_invalid", sizeof(_otaErr));
                _otaInProgress = false;
                return;
            }

            if(!Update.begin(v_sketchSpace)){
                _otaOk = false;
                strlcpy(_otaErr, "Update.begin failed", sizeof(_otaErr));
                _otaInProgress = false;
                return;
            }
        }

        // chunk write
        if(p_len){
            const size_t v_w = Update.write(p_data, p_len);
            _otaWritten += (uint32_t)v_w;
            if(v_w != p_len){
                strlcpy(_otaErr, "Update.write mismatch", sizeof(_otaErr));
            }
        }

        // finalize
        if(p_final){
            if(!Update.end(true)){
                strlcpy(_otaErr, "Update.end failed", sizeof(_otaErr));
                _otaOk = false;
            }else if(Update.hasError()){
                strlcpy(_otaErr, "Update.hasError", sizeof(_otaErr));
                _otaOk = false;
            }else{
                _otaOk = true;
                strlcpy(_otaErr, "ok", sizeof(_otaErr));
            }
            _otaInProgress = false;
        }
    }

    void apiOtaStatus(AsyncWebServerRequest* p_req){
        JsonDocument v_doc;
        v_doc["in_progress"] = _otaInProgress;
        v_doc["total"]       = (uint32_t)_otaTotal;
        v_doc["written"]     = (uint32_t)_otaWritten;
        v_doc["ok"]          = _otaOk;
        v_doc["err"]         = _otaErr;
        sendJson(p_req, v_doc, 200);
    }
};
