// =======================================================
// File: src/v024/W10_WebConfig_024.h
// =======================================================
#pragma once
/*
 * (022 주석 동일 구조 유지)
 * 024 추가:
 *  - /api/status: dashboard alerts(색/경고/권장조치) + anomaly/health 기반
 *  - /api/keycodes: kb 0x00~0xE7 full + consumer presets + kb_meta(0xE0~0xE7 modifier usage)
 *  - /api/ppt: 저장 로직 정리(로드->패치->save(optional)->apply)
 *  - OTA 안전장치: in_progress 동안 config/ppt/control 잠금(423), cancel/reboot 추가
 */
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>

#include "C10_Config_023.h"
#include "E10_EliteAirMouse_024.h"

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
        { "/",              "/www/index_024.html", "/www/index_024.html.gz", "text/html", false },
        { "/www/",          "/www/index_024.html", "/www/index_024.html.gz", "text/html", false },
        { "/www/style_024.css","/www/style_024.css","/www/style_024.css.gz","text/css", true },
        { "/www/app_024.js","/www/app_024.js","/www/app_024.js.gz","application/javascript", true },
    };

    // mods mask == modifier byte (1:1)
    struct ST_Mod { const char* name; uint8_t mask; };
    static constexpr ST_Mod s_mods[] = {
        {"None",0x00},{"LCtrl",0x01},{"LShift",0x02},{"LAlt",0x04},{"LMeta",0x08},
        {"RCtrl",0x10},{"RShift",0x20},{"RAlt",0x40},{"RMeta",0x80},
    };

    // Consumer presets (mask)
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
    volatile uint32_t _otaLastChunkMs=0;
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
        _svr.on("/api/status",   HTTP_GET,  [this](AsyncWebServerRequest* req){ this->apiStatus_(req); });
        _svr.on("/api/keycodes", HTTP_GET,  [this](AsyncWebServerRequest* req){ this->apiKeycodes_(req); });

        _svr.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* req){ this->apiGetConfig_(req); });
        _svr.on("/api/config", HTTP_POST, [this](AsyncWebServerRequest* req){ (void)req; }, nullptr,
                [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
                    this->apiPostConfig_(req,data,len,index,total);
                });

        // export/import/rollback
        _svr.on("/api/config/export",   HTTP_GET,  [this](AsyncWebServerRequest* req){ this->apiExport_(req); });
        _svr.on("/api/config/import",   HTTP_POST, [this](AsyncWebServerRequest* req){ (void)req; }, nullptr,
                [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
                    this->apiImport_(req,data,len,index,total);
                });
        _svr.on("/api/config/rollback", HTTP_POST, [this](AsyncWebServerRequest* req){ (void)req; }, nullptr,
                [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
                    (void)data;(void)len;(void)index;(void)total;
                    this->apiRollback_(req);
                });

        // control
        _svr.on("/api/control", HTTP_POST, [this](AsyncWebServerRequest* req){ (void)req; }, nullptr,
                [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
                    this->apiControl_(req,data,len,index,total);
                });

        // PPT keymap
        _svr.on("/api/ppt", HTTP_GET,  [this](AsyncWebServerRequest* req){ this->apiGetPpt_(req); });
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

        // 업로드 최종 응답(클라가 여기서 ok 확인)
        _svr.on("/api/ota", HTTP_POST,
                [this](AsyncWebServerRequest* req){
                    JsonDocument d;
                    d["ok"]=_otaOk;
                    d["err"]=_otaErr;
                    d["written"]=(uint32_t)_otaWritten;
                    d["total"]=(uint32_t)_otaTotal;
                    d["in_progress"]=_otaInProgress;
                    String out; serializeJson(d,out);
                    req->send(_otaOk?200:500,"application/json",out);
                },
                [this](AsyncWebServerRequest* req, const String& filename, size_t index, uint8_t* data, size_t len, bool final){
                    this->apiOtaUpload_(req,filename,index,data,len,final);
                });

        // OTA cancel (현장 복구용)
        _svr.on("/api/ota/cancel", HTTP_POST, [this](AsyncWebServerRequest* req){
            bool ok=this->cancelOta_();
            req->send(ok?200:500,"application/json", ok?"{\"ok\":true}":"{\"ok\":false}");
        });

        // OTA reboot (명시적 재부팅)
        _svr.on("/api/ota/reboot", HTTP_POST, [this](AsyncWebServerRequest* req){
            req->send(200,"application/json","{\"ok\":true}");
            delay(80);
            ESP.restart();
        });

        _svr.on("/api/reboot", HTTP_POST, [this](AsyncWebServerRequest* req){ (void)req; }, nullptr,
                [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
                    (void)data;(void)len;(void)index;(void)total;
                    req->send(200,"application/json","{\"ok\":true}");
                    delay(80);
                    ESP.restart();
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
        if(a.gz && LittleFS.exists(a.gz) && acceptsGzip_(req)){ useGz=true; path=a.gz; }
        else if(!LittleFS.exists(a.plain)){ req->send(404,"text/plain","asset not found"); return; }

        AsyncWebServerResponse* res=req->beginResponse(LittleFS,path,a.type);
        if(useGz) res->addHeader("Content-Encoding","gzip");
        res->addHeader("Cache-Control", a.cache_immutable ? "public, max-age=31536000, immutable" : "no-store");
        req->send(res);
    }

    // ---------- helpers ----------
    bool isLockedByOta_() const {
        // 업로드 중이거나, 마지막 chunk 이후 15초 지나지 않았는데 in_progress로 남아있으면 잠금
        if(_otaInProgress) return true;
        return false;
    }

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
        // OTA timeout watchdog (현장: 브라우저/네트워크 끊김)
        if(_otaInProgress && (millis() - _otaLastChunkMs > 15000)){
            // 강제 종료
            cancelOta_();
        }

        JsonDocument d;
        d["uptime_ms"]=(uint32_t)millis();
        d["heap_free"]=(uint32_t)ESP.getFreeHeap();

        JsonObject net=d["net"].to<JsonObject>();
        net["mode"]=(WiFi.getMode()==WIFI_AP)?"AP":"STA";
        net["ip"]=(WiFi.getMode()==WIFI_AP)?WiFi.softAPIP().toString():WiFi.localIP().toString();
        net["ssid"]=(WiFi.getMode()==WIFI_AP)?String(_wifi.ap_ssid):WiFi.SSID();
        net["mdns"]=String(_wifi.mdns_host)+".local";

        // E10 status
        CL_E10_EliteAirMouse* e10=(CL_E10_EliteAirMouse*)_applyCtx;
        ST_E10_Status_t s;
        bool hasE10=false;
        if(e10){ e10->getStatus(s); hasE10=true; }

        // dashboard alerts (서버가 판단)
        JsonArray alerts=d["alerts"].to<JsonArray>();
        auto addAlert=[&](const char* level, const char* code, const char* msg, const char* hint){
            JsonObject a=alerts.add<JsonObject>();
            a["level"]=level; // "ok"|"warn"|"crit"
            a["code"]=code;
            a["msg"]=msg;
            a["hint"]=hint;
        };

        if(hasE10){
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

            // --- alerts rules (현장 감각) ---
            if(!s.ble_connected){
                addAlert("warn","BLE_DISCONNECTED","BLE 연결이 끊김","PC/태블릿에서 재페어링 또는 전원 재시작");
            }
            if(s.health==2){
                addAlert("crit","HEALTH_DEGRADED","장치 상태 저하(지터/오류 증가)","I2C 배선/전원/MPU 상태 확인, 필요시 재부팅");
            }else if(s.health==1){
                addAlert("warn","HEALTH_WARN","장치 상태 주의","노이즈 증가/일시 오류 가능, 센서 고정 상태 확인");
            }else{
                addAlert("ok","HEALTH_OK","장치 상태 정상","-");
            }

            if(s.consecutive_recover_fail>=2){
                addAlert("crit","I2C_RECOVER_FAIL","I2C Recover 반복 실패","SDA/SCL/전원 불안정 가능, 배선/납땜/케이블 교체");
            }else if(s.i2c_recover_count>=1 && !s.i2c_recover_last_ok){
                addAlert("warn","I2C_RECOVER_LAST_FAIL","최근 Recover 실패","전원/배선 흔들림 점검");
            }

            if(s.spike_count_10s>=6){
                addAlert("warn","GYRO_SPIKE","최근 10초 스파이크 다수","s_spikeThDeg 상향 또는 MPU 고정/필터 점검");
            }

            if(s.consecutive_fail>=10){
                addAlert("crit","SENSOR_FAIL_STREAK","센서 연속 실패 누적","MPU 통신 불안정: 배선/전원 점검 후 재부팅");
            }
        }else{
            addAlert("crit","E10_MISSING","E10 컨텍스트 없음","W10 begin()에 applyCtx(E10 인스턴스) 전달 확인");
        }

        JsonObject ota=d["ota"].to<JsonObject>();
        ota["in_progress"]=_otaInProgress;
        ota["total"]=(uint32_t)_otaTotal;
        ota["written"]=(uint32_t)_otaWritten;
        ota["ok"]=_otaOk;
        ota["err"]=_otaErr;

        sendJson_(req,d,200);
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

        JsonObject meta=d["kb_meta"].to<JsonObject>();
        meta["usage_min"]=0;
        meta["usage_max"]=0xE7;
        meta["modifier_usage_min"]=0xE0;
        meta["modifier_usage_max"]=0xE7;
        meta["note"]="0xE0~0xE7 are modifier usages. In E10 policy, modifiers must be set via mod-mask byte, not key usage.";

        JsonArray kb=d["kb"].to<JsonArray>();
        for(uint16_t code=0; code<=0xE7; code++){
            const char* n = kbName_(code);
            String name;
            if(n) name = n;
            else{
                char buf[8];
                snprintf(buf,sizeof(buf),"0x%02X",(unsigned)code);
                name = String(buf);
            }
            JsonObject o=kb.add<JsonObject>();
            o["name"]=name; // 안전 복사
            o["code"]=code;
            o["is_modifier_usage"] = (code>=0xE0 && code<=0xE7);
        }

        JsonArray con=d["consumer"].to<JsonArray>();
        for(size_t i=0;i<sizeof(s_consumer)/sizeof(s_consumer[0]);i++){
            JsonObject o=con.add<JsonObject>();
            o["name"]=s_consumer[i].name;
            o["mask"]= (uint32_t)s_consumer[i].mask;
        }

        d["note"] = "kb: usage-id(0x07). consumer: 32-bit mask(bitflag) for mediaKeyPress.";
        sendJson_(req,d,200);
    }

    // ---------- /api/config ----------
    void apiGetConfig_(AsyncWebServerRequest* req){
        String json;
        if(!_cfg->exportJson(json)){ req->send(500,"application/json","{\"ok\":false}"); return; }
        req->send(200,"application/json",json);
    }

    void apiPostConfig_(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
        if(isLockedByOta_()){ req->send(423,"application/json","{\"ok\":false,\"err\":\"ota_locked\"}"); return; }

        String* body=reqBody_(req,index);
        if(!body){ req->send(500,"application/json","{\"ok\":false}"); return; }
        for(size_t i=0;i<len;i++) (*body)+=(char)data[i];
        if(index+len<total) return;

        bool saved=false, applied=false;
        bool ok=_cfg->importJson(*body, saved, applied);
        reqBodyFree_(req);

        if(ok && saved && _applyFn) applied = _applyFn(_applyCtx);

        JsonDocument out;
        out["ok"]=ok;
        out["saved"]=saved;
        out["applied"]=applied;
        out["note"]="WiFi changes require reboot.";
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
        if(isLockedByOta_()){ req->send(423,"application/json","{\"ok\":false,\"err\":\"ota_locked\"}"); return; }

        String* body=reqBody_(req,index);
        if(!body){ req->send(500,"application/json","{\"ok\":false}"); return; }
        for(size_t i=0;i<len;i++) (*body)+=(char)data[i];
        if(index+len<total) return;

        bool saved=false, applied=false;
        bool ok=_cfg->importJson(*body, saved, applied);
        reqBodyFree_(req);

        if(ok && saved && _applyFn) applied=_applyFn(_applyCtx);

        JsonDocument out;
        out["ok"]=ok; out["saved"]=saved; out["applied"]=applied;
        sendJson_(req,out, ok?200:500);
    }

    void apiRollback_(AsyncWebServerRequest* req){
        if(isLockedByOta_()){ req->send(423,"application/json","{\"ok\":false,\"err\":\"ota_locked\"}"); return; }

        bool ok=_cfg->rollbackFromBak();
        bool applied=false;
        if(ok && _applyFn) applied=_applyFn(_applyCtx);

        JsonDocument out;
        out["ok"]=ok; out["applied"]=applied;
        sendJson_(req,out, ok?200:500);
    }

    // ---------- /api/control ----------
    void apiControl_(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
        if(isLockedByOta_()){ req->send(423,"application/json","{\"ok\":false,\"err\":\"ota_locked\"}"); return; }

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
            if(!d["ppt_mode"].isNull())       ok = ok && e10->setPptMode((bool)d["ppt_mode"]);
            if(!d["dpi_level"].isNull())      ok = ok && e10->setDpiLevel((uint8_t)d["dpi_level"]);
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

        sendJson_(req,d,200);
    }

    void apiPostPpt_(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
        if(isLockedByOta_()){ req->send(423,"application/json","{\"ok\":false,\"err\":\"ota_locked\"}"); return; }

        String* body=reqBody_(req,index);
        if(!body){ req->send(500,"application/json","{\"ok\":false}"); return; }
        for(size_t i=0;i<len;i++) (*body)+=(char)data[i];
        if(index+len<total) return;

        JsonDocument in;
        DeserializationError err=deserializeJson(in,*body);
        reqBodyFree_(req);
        if(err){ req->send(400,"application/json","{\"ok\":false,\"err\":\"bad_json\"}"); return; }

        bool save=true;
        if(!in["save"].isNull()) save=(bool)in["save"];
        JsonVariant map=in["map"];
        if(map.isNull()){ req->send(400,"application/json","{\"ok\":false,\"err\":\"no_map\"}"); return; }

        // load current
        ST_C10_WiFiConfig_t w; ST_C10_E10Config_t e;
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

        loadK("start", e.ppt2_start);
        loadK("exit",  e.ppt2_exit);
        loadK("next",  e.ppt2_next);
        loadK("prev",  e.ppt2_prev);
        loadK("black", e.ppt2_black);
        loadK("laser", e.ppt2_laser);

        bool ok=true, saved=false, applied=false;
        if(save){
            ok = _cfg->saveAll(w,e);
            saved = ok;
        }

        if(ok && _applyFn) applied = _applyFn(_applyCtx);

        JsonDocument out;
        out["ok"]=ok;
        out["saved"]=saved;
        out["applied"]=applied;
        sendJson_(req,out, ok?200:500);
    }

    // ---------- /api/ppt/test ----------
    void apiPptTest_(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
        if(isLockedByOta_()){ req->send(423,"application/json","{\"ok\":false,\"err\":\"ota_locked\"}"); return; }

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
        sendJson_(req,out, ok?200:500);
    }

    // ---------- OTA ----------
    bool cancelOta_(){
        if(!_otaInProgress){
            // in_progress가 아니어도 상태 리셋은 가능
            _otaTotal=0; _otaWritten=0; _otaOk=false;
            strlcpy(_otaErr,"canceled",sizeof(_otaErr));
            return true;
        }
        Update.abort();
        _otaInProgress=false;
        _otaTotal=0; _otaWritten=0; _otaOk=false;
        strlcpy(_otaErr,"canceled",sizeof(_otaErr));
        return true;
    }

    void apiOtaUpload_(AsyncWebServerRequest* req, const String& filename, size_t index, uint8_t* data, size_t len, bool final){
        (void)filename;

        _otaLastChunkMs = millis();

        if(index==0){
            _otaInProgress=true;
            _otaWritten=0;
            _otaTotal=(uint32_t)req->contentLength();
            _otaOk=false;
            memset(_otaErr,0,sizeof(_otaErr));
            strlcpy(_otaErr,"in_progress",sizeof(_otaErr));

            // basic validation
            if(_otaTotal==0){
                strlcpy(_otaErr,"bad_length",sizeof(_otaErr));
                _otaInProgress=false;
                return;
            }

            // begin update
            if(!Update.begin(UPDATE_SIZE_UNKNOWN)){
                strlcpy(_otaErr,"Update.begin failed",sizeof(_otaErr));
                _otaInProgress=false;
                return;
            }
        }

        if(len){
            size_t w=Update.write(data,len);
            _otaWritten += (uint32_t)w;
            if(w!=len){
                strlcpy(_otaErr,"Update.write mismatch",sizeof(_otaErr));
            }
        }

        if(final){
            bool ok=true;
            if(!Update.end(true)){ strlcpy(_otaErr,"Update.end failed",sizeof(_otaErr)); ok=false; }
            else if(Update.hasError()){ strlcpy(_otaErr,"Update.hasError",sizeof(_otaErr)); ok=false; }

            _otaOk=ok;
            if(ok) strlcpy(_otaErr,"ok",sizeof(_otaErr));

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
        sendJson_(req,d,200);
    }
};
CL_W10_WebConfig* CL_W10_WebConfig::s_instance=nullptr;

