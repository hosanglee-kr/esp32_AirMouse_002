// =======================================================
// File: src/v0271/W10_WebConfig_0271.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : W10_WebConfig_0271.h
 * 모듈약어 : W10
 * 모듈명 : Web Config/Status/UI/OTA Server (Field Dashboard + PPT + WiFi/E10 Editor)
 * ------------------------------------------------------
 * 기능 요약
 *  - 자산(HTML/CSS/JS) gzip 서빙 + 캐시 정책
 *  - /api/status : 현장용 대시보드 데이터(health/anomaly/i2c/err/ota)
 *  - /api/keycodes : kb(0x00~0xE7) + consumer presets + mods(Modifier byte mask)
 *  - /api/ppt, /api/ppt/test : PPT Keymap(v2) 편집/즉시 테스트
 *  - /api/config/ui : WiFi/E10 전체 편집(검증/적용/저장/리부트 안내)
 *  - /api/config/export/import/rollback : 백업/복구
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
#include <Update.h>

#include "C10_Config_0271.h"

// E10은 기존 v025 기반 유지 가능: 아래 include만 프로젝트에 맞게 연결
#include "E10_EliteAirMouse_025.h"  // 또는 E10_EliteAirMouse_0271.h 로 교체

class CL_W10_WebConfig {
  private:
    AsyncWebServer _svr;

    CL_C10_Config* _cfg=nullptr;
    bool (*_applyFn)(void*)=nullptr;
    void* _applyCtx=nullptr;

    ST_C10_WiFiConfig_t _wifi;
    ST_C10_E10Config_t  _e10;

    struct ST_W10_Asset_t {
        const char* uri;
        const char* plain;
        const char* gz;
        const char* type;
        bool cache_immutable;
    };

    static constexpr ST_W10_Asset_t s_assets[] = {
        { "/", "/www/index_0271.html", "/www/index_0271.html.gz", "text/html", false },
        { "/www/", "/www/index_0271.html", "/www/index_0271.html.gz", "text/html", false },
        { "/www/style_0271.css", "/www/style_0271.css.gz", "/www/style_0271.css.gz", "text/css", true },
        { "/www/app_0271.js", "/www/app_0271.js.gz", "/www/app_0271.js.gz", "application/javascript", true },
    };

    // mods mask == HID report "modifier byte" (1:1). UI/W10 정책과 E10 전송이 동일.
    struct ST_Mod { const char* name; uint8_t mask; };
    static constexpr ST_Mod s_mods[] = {
        {"None",0x00},{"LCtrl",0x01},{"LShift",0x02},{"LAlt",0x04},{"LMeta",0x08},
        {"RCtrl",0x10},{"RShift",0x20},{"RAlt",0x40},{"RMeta",0x80},
    };

    // Consumer presets (mask). (대표값만 제공; Raw 입력 가능)
    struct ST_Consumer { const char* name; uint32_t mask; };
    static constexpr ST_Consumer s_consumer[] = {
        {"None", 0x00000000},

        {"Play",        0x00000001},
        {"Pause",       0x00000002},
        {"Record",      0x00000004},
        {"FastForward", 0x00000008},
        {"Rewind",      0x00000010},
        {"NextTrack",   0x00000020},
        {"PrevTrack",   0x00000040},
        {"Stop",        0x00000080},
        {"Eject",       0x00000100},
        {"RandomPlay",  0x00000200},
        {"Repeat",      0x00000400},
        {"PlayPause",   0x00000800},

        {"Mute",        0x00001000},
        {"VolumeUp",    0x00002000},
        {"VolumeDown",  0x00004000},

        {"WWWHome",     0x00008000},
        {"MyComputer",  0x00010000},
        {"Calculator",  0x00020000},
        {"WWWFavorites",0x00040000},
        {"WWWSearch",   0x00080000},
        {"WWWStop",     0x00100000},
        {"WWWBack",     0x00200000},
        {"MediaSelect", 0x00400000},
        {"Mail",        0x00800000},
    };

    bool _mdnsStarted=false;
    static CL_W10_WebConfig* s_instance;
    static void s_wifiEvent(WiFiEvent_t e, WiFiEventInfo_t info){ (void)info; if(s_instance) s_instance->onWifiEvent_(e); }

    // OTA state
    volatile bool _otaInProgress=false;
    volatile uint32_t _otaTotal=0, _otaWritten=0;
    volatile bool _otaOk=false;
    char _otaErr[64];

  public:
    CL_W10_WebConfig() : _svr(80) {
        s_instance=this;
        memset(&_wifi,0,sizeof(_wifi));
        memset(&_e10,0,sizeof(_e10));
        memset(_otaErr,0,sizeof(_otaErr));
        strlcpy(_otaErr,"none",sizeof(_otaErr));
    }

    void begin(CL_C10_Config* p_cfg, bool (*p_applyFn)(void*), void* p_applyCtx){
        _cfg=p_cfg; _applyFn=p_applyFn; _applyCtx=p_applyCtx;

        WiFi.onEvent(s_wifiEvent);
        (void)LittleFS.begin(true);

        (void)_cfg->loadAll(_wifi,_e10);
        setupWiFi_();

        // assets
        for(size_t i=0;i<sizeof(s_assets)/sizeof(s_assets[0]);i++){
            const ST_W10_Asset_t& a=s_assets[i];
            _svr.on(a.uri, HTTP_GET, [this,a](AsyncWebServerRequest* req){ this->serveAsset_(req,a); });
        }

        // APIs
        _svr.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* req){ this->apiStatus_(req); });
        _svr.on("/api/keycodes", HTTP_GET, [this](AsyncWebServerRequest* req){ this->apiKeycodes_(req); });

        // config get/save(legacy)
        _svr.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* req){ this->apiGetConfig_(req); });
        _svr.on("/api/config", HTTP_POST, [this](AsyncWebServerRequest* req){ (void)req; }, nullptr,
                [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
                    this->apiPostConfig_(req,data,len,index,total);
                });

        // ✅ UI 전용: WiFi/E10 전체 편집
        _svr.on("/api/config/ui", HTTP_GET, [this](AsyncWebServerRequest* req){ this->apiConfigUiGet_(req); });
        _svr.on("/api/config/ui", HTTP_POST, [this](AsyncWebServerRequest* req){ (void)req; }, nullptr,
                [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
                    this->apiConfigUiPost_(req,data,len,index,total);
                });

        // export/import/rollback
        _svr.on("/api/config/export", HTTP_GET, [this](AsyncWebServerRequest* req){ this->apiExport_(req); });
        _svr.on("/api/config/import", HTTP_POST, [this](AsyncWebServerRequest* req){ (void)req; }, nullptr,
                [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
                    this->apiImport_(req,data,len,index,total);
                });
        _svr.on("/api/config/rollback", HTTP_POST, [this](AsyncWebServerRequest* req){ (void)req; }, nullptr,
                [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
                    (void)data;(void)len;(void)index;(void)total;
                    this->apiRollback_(req);
                });

        // control (legacy)
        _svr.on("/api/control", HTTP_POST, [this](AsyncWebServerRequest* req){ (void)req; }, nullptr,
                [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
                    this->apiControl_(req,data,len,index,total);
                });

        // PPT keymap
        _svr.on("/api/ppt", HTTP_GET, [this](AsyncWebServerRequest* req){ this->apiGetPpt_(req); });
        _svr.on("/api/ppt", HTTP_POST, [this](AsyncWebServerRequest* req){ (void)req; }, nullptr,
                [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
                    this->apiPostPpt_(req,data,len,index,total);
                });

        // PPT test (Apply without Save)
        _svr.on("/api/ppt/test", HTTP_POST, [this](AsyncWebServerRequest* req){ (void)req; }, nullptr,
                [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
                    this->apiPptTest_(req,data,len,index,total);
                });

        // OTA
        _svr.on("/api/ota/status", HTTP_GET, [this](AsyncWebServerRequest* req){ this->apiOtaStatus_(req); });
        _svr.on("/api/ota", HTTP_POST,
                [this](AsyncWebServerRequest* req){
                    JsonDocument d;
                    d["ok"]=_otaOk;
                    d["err"]=_otaErr;
                    d["written"]=(uint32_t)_otaWritten;
                    d["total"]=(uint32_t)_otaTotal;
                    String out; serializeJson(d,out);
                    req->send(_otaOk?200:500,"application/json",out);
                    if(_otaOk){ delay(200); ESP.restart(); }
                },
                [this](AsyncWebServerRequest* req, const String& filename, size_t index, uint8_t* data, size_t len, bool final){
                    this->apiOtaUpload_(req,filename,index,data,len,final);
                });

        _svr.on("/api/reboot", HTTP_POST, [this](AsyncWebServerRequest* req){ (void)req; }, nullptr,
                [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
                    (void)data;(void)len;(void)index;(void)total;
                    req->send(200,"application/json","{\"ok\":true}");
                    delay(50); ESP.restart();
                });

        _svr.onNotFound([](AsyncWebServerRequest* req){ req->send(404,"text/plain","not found"); });

        _svr.begin();
    }

  private:
    // ---------- WiFi ----------
    void setupWiFi_(){
        WiFi.mode(WIFI_MODE_NULL);
        const bool hasSta = (_wifi.sta_ssid[0] != '\0');
        const bool autoM  = (_wifi.mode == (uint8_t)EN_C10_WIFI_AUTO);
        const bool forceAp  = (_wifi.mode == (uint8_t)EN_C10_WIFI_AP);
        const bool forceSta = (_wifi.mode == (uint8_t)EN_C10_WIFI_STA);

        if(forceAp || (!hasSta && (autoM || !forceSta))){
            startAp_(); return;
        }

        WiFi.mode(WIFI_STA);
        WiFi.begin(_wifi.sta_ssid,_wifi.sta_pass);

        uint32_t t0=millis(); bool ok=false;
        while(millis()-t0<8000){
            if(WiFi.status()==WL_CONNECTED){ ok=true; break; }
            delay(200);
        }
        if(ok) return;
        if(autoM){ startAp_(); return; }
    }

    void startAp_(){ WiFi.mode(WIFI_AP); WiFi.softAP(_wifi.ap_ssid,_wifi.ap_pass); }
    void onWifiEvent_(WiFiEvent_t e){
        if(e==ARDUINO_EVENT_WIFI_STA_GOT_IP) startMdns_();
        if(e==ARDUINO_EVENT_WIFI_STA_DISCONNECTED) stopMdns_();
    }
    void startMdns_(){
        if(_mdnsStarted) return;
        if(_wifi.mdns_host[0]=='\0') return;
        if(WiFi.getMode()!=WIFI_STA || WiFi.status()!=WL_CONNECTED) return;
        if(!MDNS.begin(_wifi.mdns_host)) return;
        MDNS.addService("http","tcp",80);
        _mdnsStarted=true;
    }
    void stopMdns_(){ if(!_mdnsStarted) return; MDNS.end(); _mdnsStarted=false; }

    // ---------- gzip asset ----------
    bool acceptsGzip_(AsyncWebServerRequest* req){
        if(!req->hasHeader("Accept-Encoding")) return false;
        String ae=req->header("Accept-Encoding");
        return (ae.indexOf("gzip")>=0);
    }
    void serveAsset_(AsyncWebServerRequest* req, const ST_W10_Asset_t& a){
        bool useGz=false; const char* path=a.plain;

        // a.plain이 이미 .gz인 경우(위 assets 테이블에서 편의상)
        if(a.gz && LittleFS.exists(a.gz) && acceptsGzip_(req)){ useGz=true; path=a.gz; }
        else if(!LittleFS.exists(a.plain)){ req->send(404,"text/plain","asset not found"); return; }

        AsyncWebServerResponse* res=req->beginResponse(LittleFS,path,a.type);
        if(useGz || endsWithGz_(path)) res->addHeader("Content-Encoding","gzip");
        res->addHeader("Cache-Control", a.cache_immutable ? "public, max-age=31536000, immutable" : "no-store");
        req->send(res);
    }
    bool endsWithGz_(const char* p){
        if(!p) return false;
        size_t n=strlen(p);
        if(n<3) return false;
        return (p[n-3]=='.' && p[n-2]=='g' && p[n-1]=='z');
    }

    // ---------- helpers ----------
    void sendJson_(AsyncWebServerRequest* req, JsonDocument& d, int code=200){
        String out; serializeJson(d,out);
        req->send(code,"application/json",out);
    }
    String* reqBody_(AsyncWebServerRequest* req, size_t index){
        if(index==0){
            if(req->_tempObject){ delete (String*)req->_tempObject; req->_tempObject=nullptr; }
            req->_tempObject=new String();
        }
        return (String*)req->_tempObject;
    }
    void reqBodyFree_(AsyncWebServerRequest* req){
        if(req->_tempObject){ delete (String*)req->_tempObject; req->_tempObject=nullptr; }
    }

    // ---------- /api/status ----------
    void apiStatus_(AsyncWebServerRequest* req){
        JsonDocument d;
        d["uptime_ms"]=(uint32_t)millis();
        d["heap_free"]=(uint32_t)ESP.getFreeHeap();

        JsonObject net=d["net"].to<JsonObject>();
        net["mode"]=(WiFi.getMode()==WIFI_AP)?"AP":"STA";
        net["ip"]=(WiFi.getMode()==WIFI_AP)?WiFi.softAPIP().toString():WiFi.localIP().toString();
        net["ssid"]=(WiFi.getMode()==WIFI_AP)?String(_wifi.ap_ssid):WiFi.SSID();
        net["mdns"]=String(_wifi.mdns_host)+".local";

        CL_E10_EliteAirMouse* e10=(CL_E10_EliteAirMouse*)_applyCtx;
        if(e10){
            ST_E10_Status_t s; e10->getStatus(s);

            JsonObject e=d["e10"].to<JsonObject>();
            e["ble_connected"]=s.ble_connected;
            e["ppt_mode"]=s.ppt_mode;
            e["dpi_level"]=s.dpi_level;
            e["precision_mode"]=s.precision_mode;

            JsonObject h=e["health"].to<JsonObject>();
            h["state"]=s.health;
            h["score"]=s.health_score;

            JsonObject gyro=e["gyro"].to<JsonObject>();
            gyro["bias_x"]=s.gyro_bias_x;
            gyro["bias_y"]=s.gyro_bias_y;
            gyro["bias_z"]=s.gyro_bias_z;
            gyro["rms"]=s.gyro_rms;

            e["cursor_rms"]=s.cursor_rms;
            e["temp_c"]=s.temp_c;

            JsonObject i2c=e["i2c"].to<JsonObject>();
            i2c["recover_count"]=s.i2c_recover_count;
            i2c["recover_last_ok"]=s.i2c_recover_last_ok;

            JsonObject err=e["err"].to<JsonObject>();
            err["mpu_nan"]=s.err_mpu_nan;
            err["mutex_miss"]=s.err_mutex_miss;
            err["task_overrun"]=s.err_task_overrun;

            JsonObject an=e["anomaly"].to<JsonObject>();
            an["spike_count_10s"]=s.spike_count_10s;
            an["consecutive_fail"]=s.consecutive_fail;
            an["consecutive_recover_fail"]=s.consecutive_recover_fail;

            JsonArray hist=e["err_hist"].to<JsonArray>();
            for(uint8_t i=0;i<s.err_hist_n;i++){
                JsonObject o=hist.add<JsonObject>();
                o["ts_ms"]=s.err_hist[i].ts_ms;
                o["code"]=s.err_hist[i].code;
                o["value"]=s.err_hist[i].value;
            }
        }

        JsonObject ota=d["ota"].to<JsonObject>();
        ota["in_progress"]=_otaInProgress;
        ota["total"]=(uint32_t)_otaTotal;
        ota["written"]=(uint32_t)_otaWritten;
        ota["ok"]=_otaOk;
        ota["err"]=_otaErr;

        sendJson_(req,d);
    }

    // ---------- /api/keycodes ----------
    const char* kbName_(uint16_t code){
        switch(code){
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

    void apiKeycodes_(AsyncWebServerRequest* req){
        JsonDocument d;

        JsonArray mods=d["mods"].to<JsonArray>();
        for(size_t i=0;i<sizeof(s_mods)/sizeof(s_mods[0]);i++){
            JsonObject o=mods.add<JsonObject>();
            o["name"]=s_mods[i].name;
            o["mask"]=s_mods[i].mask;
        }

        JsonArray kb=d["kb"].to<JsonArray>();
        char nameBuf[8];

        for(uint16_t code=0; code<=0xE7; code++){
            const char* n = kbName_(code);
            if(!n){
                snprintf(nameBuf,sizeof(nameBuf),"0x%02X",(unsigned)code);
                n = nameBuf;
            }
            JsonObject o=kb.add<JsonObject>();
            o["name"]=n;
            o["code"]=code;
        }

        JsonArray con=d["consumer"].to<JsonArray>();
        for(size_t i=0;i<sizeof(s_consumer)/sizeof(s_consumer[0]);i++){
            JsonObject o=con.add<JsonObject>();
            o["name"]=s_consumer[i].name;
            o["mask"]= (uint32_t)s_consumer[i].mask;
        }

        d["note"] = "kb: usage-id(0x07), consumer: 32-bit mask (mediaKeyPress), mods: HID modifier byte mask.";
        sendJson_(req,d);
    }

    // ---------- /api/config (legacy) ----------
    void apiGetConfig_(AsyncWebServerRequest* req){
        String json;
        if(!_cfg->exportJson(json)){ req->send(500,"application/json","{\"ok\":false}"); return; }
        req->send(200,"application/json",json);
    }

    void apiPostConfig_(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
        String* body=reqBody_(req,index);
        if(!body){ req->send(500,"application/json","{\"ok\":false}"); return; }
        for(size_t i=0;i<len;i++) (*body)+=(char)data[i];
        if(index+len<total) return;

        bool saved=false;
        bool ok=_cfg->importJson(*body, saved);
        reqBodyFree_(req);

        bool applied=false;
        if(ok && saved && _applyFn) applied = _applyFn(_applyCtx);

        JsonDocument out;
        out["ok"]=ok;
        out["saved"]=saved;
        out["applied"]=applied;
        out["note"]="WiFi changes require reboot.";
        sendJson_(req,out, ok?200:400);
    }

    // ---------- /api/config/ui (GET) ----------
    void apiConfigUiGet_(AsyncWebServerRequest* req){
        (void)_cfg->loadAll(_wifi,_e10);

        ST_C10_E10Ranges_t r;
        _cfg->getRangesE10(r);

        JsonDocument d;
        d["ver"]=(uint16_t)G_C10_CFG_VER;

        JsonObject w=d["wifi"].to<JsonObject>();
        w["mode"]=_wifi.mode;
        JsonObject wsta=w["sta"].to<JsonObject>(); wsta["ssid"]=_wifi.sta_ssid; wsta["pass"]=_wifi.sta_pass;
        JsonObject wap=w["ap"].to<JsonObject>();  wap["ssid"]=_wifi.ap_ssid;  wap["pass"]=_wifi.ap_pass;
        JsonObject wmd=w["mdns"].to<JsonObject>(); wmd["host"]=_wifi.mdns_host;

        JsonObject e=d["e10"].to<JsonObject>();
        e["dpi_level"]=_e10.dpi_level;
        e["hard_click_lock"]=_e10.hard_click_lock;

        JsonArray sb=e["scale_base"].to<JsonArray>(); sb.add(_e10.scale_base[0]); sb.add(_e10.scale_base[1]); sb.add(_e10.scale_base[2]);
        JsonArray ag=e["accel_gain"].to<JsonArray>(); ag.add(_e10.accel_gain[0]); ag.add(_e10.accel_gain[1]); ag.add(_e10.accel_gain[2]);
        e["accel_threshold"]=_e10.accel_threshold;

        JsonObject wh=e["wheel"].to<JsonObject>();
        wh["threshold_deg"]=_e10.wheel_threshold_deg;
        wh["step_max"]=_e10.wheel_step_max;

        JsonObject ge=e["gesture"].to<JsonObject>();
        ge["flick_deg"]=_e10.gesture_flick_deg;
        ge["cooldown_ms"]=_e10.gesture_cooldown_ms;

        e["scroll_cursor_damp"]=_e10.scroll_cursor_damp;

        JsonObject pr=e["precision"].to<JsonObject>();
        pr["enable"]=_e10.precision_enable;
        pr["deadzone"]=_e10.precision_deadzone;
        pr["gain"]=_e10.precision_gain;
        pr["accel"]=_e10.precision_accel;
        pr["max_step"]=_e10.precision_max_step;
        pr["smooth"]=_e10.precision_smooth;

        JsonObject ranges=d["ranges"].to<JsonObject>();
        JsonObject rr=ranges["e10"].to<JsonObject>();
        rr["dpi_min"]=r.dpi_min; rr["dpi_max"]=r.dpi_max;
        rr["scale_min"]=r.scale_min; rr["scale_max"]=r.scale_max;
        rr["accel_gain_min"]=r.accel_gain_min; rr["accel_gain_max"]=r.accel_gain_max;
        rr["accel_th_min"]=r.accel_th_min; rr["accel_th_max"]=r.accel_th_max;
        rr["wheel_th_min"]=r.wheel_th_min; rr["wheel_th_max"]=r.wheel_th_max;
        rr["wheel_step_min"]=r.wheel_step_min; rr["wheel_step_max"]=r.wheel_step_max;
        rr["flick_min"]=r.flick_min; rr["flick_max"]=r.flick_max;
        rr["cooldown_min"]=r.cooldown_min; rr["cooldown_max"]=r.cooldown_max;
        rr["damp_min"]=r.damp_min; rr["damp_max"]=r.damp_max;
        rr["prec_dead_min"]=r.prec_dead_min; rr["prec_dead_max"]=r.prec_dead_max;
        rr["prec_gain_min"]=r.prec_gain_min; rr["prec_gain_max"]=r.prec_gain_max;
        rr["prec_acc_min"]=r.prec_acc_min; rr["prec_acc_max"]=r.prec_acc_max;
        rr["prec_step_min"]=r.prec_step_min; rr["prec_step_max"]=r.prec_step_max;
        rr["prec_smooth_min"]=r.prec_smooth_min; rr["prec_smooth_max"]=r.prec_smooth_max;

        sendJson_(req,d);
    }

    // ---------- /api/config/ui (POST) ----------
    // body: { save:true/false, apply:true/false, wifi:{...}, e10:{...} }
    void apiConfigUiPost_(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
        String* body=reqBody_(req,index);
        if(!body){ req->send(500,"application/json","{\"ok\":false}"); return; }
        for(size_t i=0;i<len;i++) (*body)+=(char)data[i];
        if(index+len<total) return;

        JsonDocument d;
        DeserializationError err=deserializeJson(d,*body);
        reqBodyFree_(req);
        if(err){ req->send(400,"application/json","{\"ok\":false,\"err\":\"bad_json\"}"); return; }

        bool wantSave=true;
        bool wantApply=true;
        if(!d["save"].isNull())  wantSave = (bool)d["save"];
        if(!d["apply"].isNull()) wantApply = (bool)d["apply"];

        // load current, patch, normalize, diff 판단, save/apply
        ST_C10_WiFiConfig_t w;
        ST_C10_E10Config_t  e;
        _cfg->makeDefaultsWiFi(w);
        _cfg->makeDefaultsE10(e);
        (void)_cfg->loadAll(w,e);

        ST_C10_WiFiConfig_t w_old = w;
        ST_C10_E10Config_t  e_old = e;

        // patch from doc variants(추가 JsonDocument 생성 없이)
        _cfg->patchFromDocWiFi(d["wifi"], w);
        _cfg->patchFromDocE10(d["e10"], e);

        _cfg->normalizeWiFi(w);
        _cfg->normalizeE10(e);

        bool wifiChanged = (memcmp(&w, &w_old, sizeof(w)) != 0);
        bool e10Changed  = (memcmp(&e, &e_old, sizeof(e)) != 0);

        bool saved=false;
        bool applied=false;
        bool ok=true;

        if(wantSave){
            ok = _cfg->saveAll(w,e);
            saved = ok;
        }
        if(ok && wantApply && _applyFn){
            applied = _applyFn(_applyCtx);
        }

        // response
        JsonDocument out;
        out["ok"]=ok;
        out["saved"]=saved;
        out["applied"]=applied;
        out["wifi_changed"]=wifiChanged;
        out["e10_changed"]=e10Changed;
        out["reboot_needed"]=wifiChanged; // WiFi는 즉시 반영 어려움(현 정책)

        // warnings (간단)
        JsonArray warn = out["warnings"].to<JsonArray>();
        if(strlen(w.ap_pass) < 8) warn.add("wifi.ap.pass too short -> normalized to default(min 8).");
        if(strlen(w.mdns_host) == 0) warn.add("wifi.mdns.host empty -> normalized to default.");

        // preview(정규화 결과)
        JsonObject prev = out["preview"].to<JsonObject>();
        JsonObject pw=prev["wifi"].to<JsonObject>();
        pw["mode"]=w.mode;
        JsonObject pwsta=pw["sta"].to<JsonObject>(); pwsta["ssid"]=w.sta_ssid; pwsta["pass"]=w.sta_pass;
        JsonObject pwap=pw["ap"].to<JsonObject>();  pwap["ssid"]=w.ap_ssid;  pwap["pass"]=w.ap_pass;
        JsonObject pwmd=pw["mdns"].to<JsonObject>(); pwmd["host"]=w.mdns_host;

        JsonObject pe=prev["e10"].to<JsonObject>();
        pe["dpi_level"]=e.dpi_level;
        pe["hard_click_lock"]=e.hard_click_lock;

        JsonArray psb=pe["scale_base"].to<JsonArray>(); psb.add(e.scale_base[0]); psb.add(e.scale_base[1]); psb.add(e.scale_base[2]);
        JsonArray pag=pe["accel_gain"].to<JsonArray>(); pag.add(e.accel_gain[0]); pag.add(e.accel_gain[1]); pag.add(e.accel_gain[2]);
        pe["accel_threshold"]=e.accel_threshold;

        JsonObject pwh=pe["wheel"].to<JsonObject>(); pwh["threshold_deg"]=e.wheel_threshold_deg; pwh["step_max"]=e.wheel_step_max;
        JsonObject pge=pe["gesture"].to<JsonObject>(); pge["flick_deg"]=e.gesture_flick_deg; pge["cooldown_ms"]=e.gesture_cooldown_ms;
        pe["scroll_cursor_damp"]=e.scroll_cursor_damp;
        JsonObject ppr=pe["precision"].to<JsonObject>();
        ppr["enable"]=e.precision_enable; ppr["deadzone"]=e.precision_deadzone; ppr["gain"]=e.precision_gain;
        ppr["accel"]=e.precision_accel; ppr["max_step"]=e.precision_max_step; ppr["smooth"]=e.precision_smooth;

        sendJson_(req,out, ok?200:500);
    }

    // ---------- export/import/rollback ----------
    void apiExport_(AsyncWebServerRequest* req){
        String json;
        if(!_cfg->exportJson(json)){ req->send(500,"application/json","{\"ok\":false}"); return; }
        AsyncWebServerResponse* res=req->beginResponse(200,"application/json",json);
        res->addHeader("Content-Disposition","attachment; filename=\"config.json\"");
        res->addHeader("Cache-Control","no-store");
        req->send(res);
    }

    void apiImport_(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
        String* body=reqBody_(req,index);
        if(!body){ req->send(500,"application/json","{\"ok\":false}"); return; }
        for(size_t i=0;i<len;i++) (*body)+=(char)data[i];
        if(index+len<total) return;

        bool saved=false;
        bool ok=_cfg->importJson(*body, saved);
        reqBodyFree_(req);

        bool applied=false;
        if(ok && saved && _applyFn) applied=_applyFn(_applyCtx);

        JsonDocument out; out["ok"]=ok; out["saved"]=saved; out["applied"]=applied;
        sendJson_(req,out, ok?200:400);
    }

    void apiRollback_(AsyncWebServerRequest* req){
        bool ok=_cfg->rollbackFromBak();
        bool applied=false;
        if(ok && _applyFn) applied=_applyFn(_applyCtx);
        JsonDocument out; out["ok"]=ok; out["applied"]=applied;
        sendJson_(req,out, ok?200:500);
    }

    // ---------- /api/control (legacy) ----------
    void apiControl_(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
        String* body=reqBody_(req,index);
        if(!body){ req->send(500,"application/json","{\"ok\":false}"); return; }
        for(size_t i=0;i<len;i++) (*body)+=(char)data[i];
        if(index+len<total) return;

        JsonDocument d;
        DeserializationError err=deserializeJson(d,*body);
        reqBodyFree_(req);
        if(err){ req->send(400,"application/json","{\"ok\":false,\"err\":\"bad_json\"}"); return; }

        CL_E10_EliteAirMouse* e10=(CL_E10_EliteAirMouse*)_applyCtx;
        bool ok=true;
        if(e10){
            if(!d["ppt_mode"].isNull()) ok = ok && e10->setPptMode((bool)d["ppt_mode"]);
            if(!d["dpi_level"].isNull()) ok = ok && e10->setDpiLevel((uint8_t)d["dpi_level"]);
            if(!d["precision_mode"].isNull()) ok = ok && e10->setPrecisionMode((bool)d["precision_mode"]);
        } else ok=false;

        JsonDocument out; out["ok"]=ok;
        sendJson_(req,out, ok?200:500);
    }

    // ---------- /api/ppt (GET/POST) ----------
    void apiGetPpt_(AsyncWebServerRequest* req){
        (void)_cfg->loadAll(_wifi,_e10);
        JsonDocument d;

        JsonObject map=d["map"].to<JsonObject>();
        auto put=[&](const char* n, const ST_C10_PptKey2_t& k){
            JsonObject o=map[n].to<JsonObject>();
            o["page"] = (k.page==(uint8_t)EN_C10_KEYPAGE_CONSUMER)?"consumer":"kb";
            o["mod"]  = k.mod;
            o["code"] = k.code;
        };
        put("start",_e10.ppt2_start);
        put("exit", _e10.ppt2_exit);
        put("next", _e10.ppt2_next);
        put("prev", _e10.ppt2_prev);
        put("black",_e10.ppt2_black);
        put("laser",_e10.ppt2_laser);

        sendJson_(req,d);
    }

    void apiPostPpt_(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
        String* body=reqBody_(req,index);
        if(!body){ req->send(500,"application/json","{\"ok\":false}"); return; }
        for(size_t i=0;i<len;i++) (*body)+=(char)data[i];
        if(index+len<total) return;

        JsonDocument d;
        DeserializationError err=deserializeJson(d,*body);
        reqBodyFree_(req);
        if(err){ req->send(400,"application/json","{\"ok\":false,\"err\":\"bad_json\"}"); return; }

        bool save = true;
        if(!d["save"].isNull()) save = (bool)d["save"];

        JsonVariant map=d["map"];
        if(map.isNull()){ req->send(400,"application/json","{\"ok\":false,\"err\":\"no_map\"}"); return; }

        ST_C10_WiFiConfig_t w;
        ST_C10_E10Config_t  e;
        _cfg->makeDefaultsWiFi(w);
        _cfg->makeDefaultsE10(e);
        (void)_cfg->loadAll(w,e);

        auto loadK=[&](const char* n, ST_C10_PptKey2_t& k){
            JsonVariant o = map[n];
            if(o.isNull()) return;
            if(!o["page"].isNull()){
                const char* s=(const char*)o["page"];
                if(s && strcasecmp(s,"consumer")==0) k.page=(uint8_t)EN_C10_KEYPAGE_CONSUMER;
                else k.page=(uint8_t)EN_C10_KEYPAGE_KB;
            }
            if(!o["mod"].isNull())  k.mod = (uint8_t)o["mod"];
            if(!o["code"].isNull()) k.code = (uint32_t)o["code"];
        };

        loadK("start",e.ppt2_start);
        loadK("exit", e.ppt2_exit);
        loadK("next", e.ppt2_next);
        loadK("prev", e.ppt2_prev);
        loadK("black",e.ppt2_black);
        loadK("laser",e.ppt2_laser);

        _cfg->normalizeE10(e);

        bool saved=false;
        bool ok=true;
        if(save){
            ok = _cfg->saveAll(w,e);
            saved = ok;
        }
        bool applied=false;
        if(ok && _applyFn) applied = _applyFn(_applyCtx);

        JsonDocument out;
        out["ok"]=ok;
        out["saved"]=saved;
        out["applied"]=applied;
        sendJson_(req,out, ok?200:500);
    }

    // ---------- /api/ppt/test ----------
    void apiPptTest_(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
        String* body=reqBody_(req,index);
        if(!body){ req->send(500,"application/json","{\"ok\":false}"); return; }
        for(size_t i=0;i<len;i++) (*body)+=(char)data[i];
        if(index+len<total) return;

        JsonDocument d;
        DeserializationError err=deserializeJson(d,*body);
        reqBodyFree_(req);
        if(err){ req->send(400,"application/json","{\"ok\":false,\"err\":\"bad_json\"}"); return; }

        uint8_t page=(uint8_t)EN_C10_KEYPAGE_KB;
        uint8_t mod=0;
        uint32_t code=0;

        if(!d["page"].isNull()){
            const char* s=(const char*)d["page"];
            if(s && strcasecmp(s,"consumer")==0) page=(uint8_t)EN_C10_KEYPAGE_CONSUMER;
        }
        if(!d["mod"].isNull())  mod=(uint8_t)d["mod"];
        if(!d["code"].isNull()) code=(uint32_t)d["code"];

        CL_E10_EliteAirMouse* e10=(CL_E10_EliteAirMouse*)_applyCtx;
        bool ok=false;
        if(e10) ok = e10->testPptKey2(page,mod,code);

        JsonDocument out;
        out["ok"]=ok;
        sendJson_(req,out);
    }

    // ---------- OTA ----------
    void apiOtaUpload_(AsyncWebServerRequest* req, const String& filename, size_t index, uint8_t* data, size_t len, bool final){
        (void)filename;
        if(index==0){
            _otaInProgress=true;
            _otaWritten=0;
            _otaTotal=(uint32_t)req->contentLength();
            _otaOk=false;
            memset(_otaErr,0,sizeof(_otaErr));
            strlcpy(_otaErr,"in_progress",sizeof(_otaErr));

            if(_otaTotal < 64*1024){
                strlcpy(_otaErr,"payload too small",sizeof(_otaErr));
                _otaInProgress=false;
                return;
            }

            if(!Update.begin(UPDATE_SIZE_UNKNOWN)){
                strlcpy(_otaErr,"Update.begin failed",sizeof(_otaErr));
                _otaInProgress=false;
                return;
            }
        }

        if(len){
            size_t w=Update.write(data,len);
            _otaWritten += (uint32_t)w;
            if(w!=len) strlcpy(_otaErr,"Update.write mismatch",sizeof(_otaErr));
        }

        if(final){
            if(!Update.end(true)){ strlcpy(_otaErr,"Update.end failed",sizeof(_otaErr)); _otaOk=false; }
            else if(Update.hasError()){ strlcpy(_otaErr,"Update.hasError",sizeof(_otaErr)); _otaOk=false; }
            else { _otaOk=true; strlcpy(_otaErr,"ok",sizeof(_otaErr)); }
            _otaInProgress=false;
        }
    }

    void apiOtaStatus_(AsyncWebServerRequest* req){
        JsonDocument d;
        d["in_progress"]=_otaInProgress;
        d["total"]=(uint32_t)_otaTotal;
        d["written"]=(uint32_t)_otaWritten;
        d["ok"]=_otaOk;
        d["err"]=_otaErr;
        sendJson_(req,d);
    }
};

CL_W10_WebConfig* CL_W10_WebConfig::s_instance=nullptr;

