// =======================================================
// File: src/v025/W10_WebConfig_026.h
// =======================================================
#pragma once
/*
 * (기존 022 주석 동일 구조 유지, 생략)
 * v0.2.5 추가:
 *  - /api/status: alerts[] 서버 생성(현장용)
 *  - /api/ppt: save=false => E10 runtime overlay 적용(진짜 Apply only), save=true => C10 저장 + apply
 *  - OTA: lock(423) 확장 + /api/ota/cancel + /api/ota/reboot(완료 후 재부팅 분리)
 *  - /api/keycodes: kb(0x00~0xE7) 제공 + modifier usage(0xE0~0xE7) is_modifier_usage 플래그
 */
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>

#include "C10_Config_025.h"          // ✅ 프로젝트에서 025로 바뀌었으면 교체
#include "E10_EliteAirMouse_025.h"   // ✅

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
        { "/", "/www/index_0251.html", "/www/index_0251.html.gz", "text/html", false },
        { "/www/", "/www/index_0251.html", "/www/index_0251.html.gz", "text/html", false },
        { "/www/style_0251.css", "/www/style_0251.css", "/www/style_0251.css.gz", "text/css", true },
        { "/www/app_0251.js", "/www/app_0251.js", "/www/app_0251.js.gz", "application/javascript", true },
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
    volatile bool _otaCancel=false;
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

        // config get/save (OTA 중 lock)
        _svr.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* req){ this->apiGetConfig_(req); });
        _svr.on("/api/config", HTTP_POST, [this](AsyncWebServerRequest* req){ (void)req; }, nullptr,
                [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
                    this->apiPostConfig_(req,data,len,index,total);
                });

        // export/import/rollback (OTA 중 lock)
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

        // control (OTA 중 lock)
        _svr.on("/api/control", HTTP_POST, [this](AsyncWebServerRequest* req){ (void)req; }, nullptr,
                [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
                    this->apiControl_(req,data,len,index,total);
                });

        // PPT keymap (OTA 중 lock)
        _svr.on("/api/ppt", HTTP_GET, [this](AsyncWebServerRequest* req){ this->apiGetPpt_(req); });
        _svr.on("/api/ppt", HTTP_POST, [this](AsyncWebServerRequest* req){ (void)req; }, nullptr,
                [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
                    this->apiPostPpt_(req,data,len,index,total);
                });

        // PPT test (OTA 중 lock)
        _svr.on("/api/ppt/test", HTTP_POST, [this](AsyncWebServerRequest* req){ (void)req; }, nullptr,
                [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
                    this->apiPptTest_(req,data,len,index,total);
                });

        // OTA
        _svr.on("/api/ota/status", HTTP_GET, [this](AsyncWebServerRequest* req){ this->apiOtaStatus_(req); });
        _svr.on("/api/ota/cancel", HTTP_POST, [this](AsyncWebServerRequest* req){ (void)req; }, nullptr,
                [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
                    (void)data;(void)len;(void)index;(void)total;
                    this->apiOtaCancel_(req);
                });
        _svr.on("/api/ota/reboot", HTTP_POST, [this](AsyncWebServerRequest* req){ (void)req; }, nullptr,
                [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
                    (void)data;(void)len;(void)index;(void)total;
                    req->send(200,"application/json","{\"ok\":true}");
                    delay(120);
                    ESP.restart();
                });

        _svr.on("/api/ota", HTTP_POST,
                [this](AsyncWebServerRequest* req){
                    JsonDocument d;
                    d["ok"]=_otaOk;
                    d["err"]=_otaErr;
                    d["written"]=(uint32_t)_otaWritten;
                    d["total"]=(uint32_t)_otaTotal;
                    String out; serializeJson(d,out);
                    req->send(_otaOk?200:500,"application/json",out);
                    // ✅ 여기서는 reboot 자동 수행하지 않음: /api/ota/reboot로 분리(브릭 방지/현장 제어)
                },
                [this](AsyncWebServerRequest* req, const String& filename, size_t index, uint8_t* data, size_t len, bool final){
                    this->apiOtaUpload_(req,filename,index,data,len,final);
                });

        _svr.on("/api/reboot", HTTP_POST, [this](AsyncWebServerRequest* req){ (void)req; }, nullptr,
                [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
                    (void)data;(void)len;(void)index;(void)total;
                    req->send(200,"application/json","{\"ok\":true}");
                    delay(120);
                    ESP.restart();
                });

        _svr.onNotFound([](AsyncWebServerRequest* req){ req->send(404,"text/plain","not found"); });

        _svr.begin();
    }

  private:
    // ---------- common ----------
    bool isLocked_() const { return _otaInProgress; }

    void send423_(AsyncWebServerRequest* req, const char* reason){
        JsonDocument d;
        d["ok"]=false;
        d["err"]="locked";
        d["reason"]=reason;
        d["ota_in_progress"]=_otaInProgress;
        String out; serializeJson(d,out);
        req->send(423,"application/json",out);
    }

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

    // ---------- /api/status (alerts 포함) ----------
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

        // alerts (server-side)
        JsonArray alerts=d["alerts"].to<JsonArray>();
        auto addAlert=[&](const char* level, const char* code, const char* msg, const char* hint){
            JsonObject a=alerts.add<JsonObject>();
            a["level"]=level;   // "ok" | "warn" | "crit"
            a["code"]=code;     // stable id
            a["msg"]=msg;
            if(hint && hint[0]) a["hint"]=hint;
        };

        if(e10){
            ST_E10_Status_t s; e10->getStatus(s);

            JsonObject e=d["e10"].to<JsonObject>();
            e["ble_connected"]=s.ble_connected;
            e["ppt_mode"]=s.ppt_mode;
            e["dpi_level"]=s.dpi_level;
            e["precision_mode"]=s.precision_mode;
            e["ppt_runtime_override"]=s.ppt_runtime_override;

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

            // ---- alerts policy (현장용) ----
            if(s.health_score < 620) addAlert("crit","HEALTH_DEGRADED","Health DEGRADED","Check sensor wiring/power; consider I2C recover history.");
            else if(s.health_score < 820) addAlert("warn","HEALTH_WARN","Health WARN","Observe noise RMS and spikes; tune thresholds if needed.");

            if(s.gyro_rms > 8.0f) addAlert("warn","GYRO_NOISE_HIGH","High gyro noise RMS","Try tightening sensor mounting; reduce vibration; tune filter/bandwidth.");
            if(s.cursor_rms > 6.0f) addAlert("warn","CURSOR_NOISE_HIGH","High cursor noise RMS","Tune accel/deadzone/precision; check dt jitter/overrun.");

            if(s.spike_count_10s >= 3) addAlert("warn","SPIKE_BURST","Gyro spike burst detected","Check sudden rotation/vibration; tune spike threshold.");
            if(s.consecutive_fail >= 5) addAlert("crit","CONSEC_FAIL","Consecutive sensor failures","I2C may be unstable; check SDA/SCL pullups and cable length.");

            if(s.i2c_recover_count >= 2) addAlert("warn","I2C_RECOVER","I2C recover occurred","If recurring: power stability / pullups / I2C speed.");
            if(!s.i2c_recover_last_ok) addAlert("crit","I2C_RECOVER_FAIL","I2C recover last FAILED","Hard fault risk; consider reboot/power-cycle.");

            if(s.ppt_runtime_override) addAlert("ok","PPT_RUNTIME","PPT map is runtime override","Reboot will revert unless saved.");

            if(alerts.size()==0) addAlert("ok","NO_ALERTS","No alerts","-");
        } else {
            addAlert("crit","E10_MISSING","E10 instance missing","WebConfig applyCtx is null.");
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

            // ✅ modifier usage range (0xE0~0xE7) : UI에서 선택 비권장 (mods mask로 처리)
            const bool isModUsage = (code >= 0xE0 && code <= 0xE7);
            o["is_modifier_usage"]=isModUsage;
        }

        JsonArray con=d["consumer"].to<JsonArray>();
        for(size_t i=0;i<sizeof(s_consumer)/sizeof(s_consumer[0]);i++){
            JsonObject o=con.add<JsonObject>();
            o["name"]=s_consumer[i].name;
            o["mask"]= (uint32_t)s_consumer[i].mask;
        }

        d["note"] = "mods mask == HID modifier byte. kb: usage-id(0x07). consumer: 32-bit mask (mediaKeyPress).";
        sendJson_(req,d);
    }

    // ---------- /api/config ----------
    void apiGetConfig_(AsyncWebServerRequest* req){
        if(isLocked_()){ send423_(req,"ota_in_progress"); return; }

        (void)_cfg->loadAll(_wifi,_e10);
        String json;
        if(!_cfg->exportJson(json)){ req->send(500,"application/json","{\"ok\":false}"); return; }
        req->send(200,"application/json",json);
    }

    void apiPostConfig_(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
        if(isLocked_()){ send423_(req,"ota_in_progress"); return; }

        String* body=reqBody_(req,index);
        if(!body){ req->send(500,"application/json","{\"ok\":false}"); return; }
        for(size_t i=0;i<len;i++) (*body)+=(char)data[i];
        if(index+len<total) return;

        bool saved=false, applied=false;
        bool ok=_cfg->importJson(*body, saved, applied);
        reqBodyFree_(req);

        // ✅ config import 성공 후 apply
        if(ok && saved && _applyFn) applied = _applyFn(_applyCtx);

        JsonDocument out;
        out["ok"]=ok;
        out["saved"]=saved;
        out["applied"]=applied;
        out["note"]="WiFi changes require reboot.";
        sendJson_(req,out);
    }

    // ---------- export/import/rollback ----------
    void apiExport_(AsyncWebServerRequest* req){
        if(isLocked_()){ send423_(req,"ota_in_progress"); return; }

        String json;
        if(!_cfg->exportJson(json)){ req->send(500,"application/json","{\"ok\":false}"); return; }
        AsyncWebServerResponse* res=req->beginResponse(200,"application/json",json);
        res->addHeader("Content-Disposition","attachment; filename=\"config.json\"");
        res->addHeader("Cache-Control","no-store");
        req->send(res);
    }

    void apiImport_(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
        if(isLocked_()){ send423_(req,"ota_in_progress"); return; }

        String* body=reqBody_(req,index);
        if(!body){ req->send(500,"application/json","{\"ok\":false}"); return; }
        for(size_t i=0;i<len;i++) (*body)+=(char)data[i];
        if(index+len<total) return;

        bool saved=false, applied=false;
        bool ok=_cfg->importJson(*body, saved, applied);
        reqBodyFree_(req);
        if(ok && saved && _applyFn) applied=_applyFn(_applyCtx);

        JsonDocument out; out["ok"]=ok; out["saved"]=saved; out["applied"]=applied;
        sendJson_(req,out);
    }

    void apiRollback_(AsyncWebServerRequest* req){
        if(isLocked_()){ send423_(req,"ota_in_progress"); return; }

        bool ok=_cfg->rollbackFromBak();
        bool applied=false;
        if(ok && _applyFn) applied=_applyFn(_applyCtx);
        JsonDocument out; out["ok"]=ok; out["applied"]=applied;
        sendJson_(req,out);
    }

    // ---------- /api/control ----------
    void apiControl_(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
        if(isLocked_()){ send423_(req,"ota_in_progress"); return; }

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
        sendJson_(req,out);
    }

    // ---------- /api/ppt (GET/POST) ----------
    void apiGetPpt_(AsyncWebServerRequest* req){
        if(isLocked_()){ send423_(req,"ota_in_progress"); return; }

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

    static void parseKey_(JsonVariant o, ST_C10_PptKey2_t& k){
        if(o.isNull()) return;

        if(!o["page"].isNull()){
            const char* s=(const char*)o["page"];
            if(s && strcasecmp(s,"consumer")==0) k.page=(uint8_t)EN_C10_KEYPAGE_CONSUMER;
            else k.page=(uint8_t)EN_C10_KEYPAGE_KB;
        }
        if(!o["mod"].isNull())  k.mod = (uint8_t)o["mod"];
        if(!o["code"].isNull()) k.code = (uint32_t)o["code"];

        // 안전: consumer page면 mod 강제 0
        if(k.page==(uint8_t)EN_C10_KEYPAGE_CONSUMER) k.mod=0;
    }

    void apiPostPpt_(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
        if(isLocked_()){ send423_(req,"ota_in_progress"); return; }

        String* body=reqBody_(req,index);
        if(!body){ req->send(500,"application/json","{\"ok\":false}"); return; }
        for(size_t i=0;i<len;i++) (*body)+=(char)data[i];
        if(index+len<total) return;

        JsonDocument d;
        DeserializationError err=deserializeJson(d,*body);
        reqBodyFree_(req);
        if(err){ req->send(400,"application/json","{\"ok\":false,\"err\":\"bad_json\"}"); return; }

        bool save=true;
        if(!d["save"].isNull()) save=(bool)d["save"];

        JsonVariant map=d["map"];
        if(map.isNull()){ req->send(400,"application/json","{\"ok\":false,\"err\":\"no_map\"}"); return; }

        // load current config
        (void)_cfg->loadAll(_wifi,_e10);

        // build new keys (starting from current)
        ST_C10_PptKey2_t k_start=_e10.ppt2_start;
        ST_C10_PptKey2_t k_exit =_e10.ppt2_exit;
        ST_C10_PptKey2_t k_next =_e10.ppt2_next;
        ST_C10_PptKey2_t k_prev =_e10.ppt2_prev;
        ST_C10_PptKey2_t k_black=_e10.ppt2_black;
        ST_C10_PptKey2_t k_laser=_e10.ppt2_laser;

        parseKey_(map["start"],k_start);
        parseKey_(map["exit"], k_exit);
        parseKey_(map["next"], k_next);
        parseKey_(map["prev"], k_prev);
        parseKey_(map["black"],k_black);
        parseKey_(map["laser"],k_laser);

        CL_E10_EliteAirMouse* e10=(CL_E10_EliteAirMouse*)_applyCtx;
        bool ok=true, saved=false, applied=false;

        if(!e10){
            JsonDocument out; out["ok"]=false; out["err"]="e10_null";
            sendJson_(req,out,500);
            return;
        }

        if(!save){
            // ✅ 진짜 Apply only: runtime overlay 적용 + applyFn 호출 금지(파일로 덮어쓰기 방지)
            ok = e10->setPptMapRuntime(k_start,k_exit,k_next,k_prev,k_black,k_laser);
            applied = ok;
        }else{
            // save=true : C10 저장 + apply
            ST_C10_WiFiConfig_t w; ST_C10_E10Config_t e;
            _cfg->makeDefaultsWiFi(w); _cfg->makeDefaultsE10(e);
            (void)_cfg->loadAll(w,e);

            e.ppt2_start=k_start; e.ppt2_exit=k_exit; e.ppt2_next=k_next;
            e.ppt2_prev=k_prev;  e.ppt2_black=k_black; e.ppt2_laser=k_laser;

            ok = _cfg->saveAll(w,e);
            saved = ok;

            if(ok && _applyFn) applied = _applyFn(_applyCtx);
            // 저장 성공 후에는 runtime override를 끄는 게 안전(저장값이 기준)
            (void)e10->clearPptMapRuntime();
        }

        JsonDocument out;
        out["ok"]=ok;
        out["saved"]=saved;
        out["applied"]=applied;
        out["mode"]= save ? "saved" : "runtime_only";
        sendJson_(req,out);
    }

    // ---------- /api/ppt/test ----------
    void apiPptTest_(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
        if(isLocked_()){ send423_(req,"ota_in_progress"); return; }

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
        if(page==(uint8_t)EN_C10_KEYPAGE_CONSUMER) mod=0;

        CL_E10_EliteAirMouse* e10=(CL_E10_EliteAirMouse*)_applyCtx;
        bool ok=false;
        if(e10) ok = e10->testPptKey2(page,mod,code);

        JsonDocument out;
        out["ok"]=ok;
        if(!ok) out["err"]="send_failed_or_not_connected";
        sendJson_(req,out);
    }

    // ---------- OTA ----------
    void apiOtaCancel_(AsyncWebServerRequest* req){
        if(!_otaInProgress){
            req->send(200,"application/json","{\"ok\":true,\"note\":\"not_in_progress\"}");
            return;
        }
        _otaCancel=true;
        req->send(200,"application/json","{\"ok\":true,\"note\":\"cancel_requested\"}");
    }

    void apiOtaUpload_(AsyncWebServerRequest* req, const String& filename, size_t index, uint8_t* data, size_t len, bool final){
        (void)filename;

        if(index==0){
            _otaInProgress=true;
            _otaCancel=false;
            _otaWritten=0;
            _otaTotal=(uint32_t)req->contentLength();
            _otaOk=false;

            memset(_otaErr,0,sizeof(_otaErr));
            strlcpy(_otaErr,"in_progress",sizeof(_otaErr));

            if(!Update.begin(UPDATE_SIZE_UNKNOWN)){
                strlcpy(_otaErr,"Update.begin failed",sizeof(_otaErr));
                _otaInProgress=false;
                return;
            }
        }

        if(_otaCancel){
            Update.abort();
            strlcpy(_otaErr,"canceled",sizeof(_otaErr));
            _otaOk=false;
            _otaInProgress=false;
            return;
        }

        if(len){
            size_t w=Update.write(data,len);
            _otaWritten += (uint32_t)w;
            if(w!=len) strlcpy(_otaErr,"Update.write mismatch",sizeof(_otaErr));
        }

        if(final){
            if(_otaCancel){
                Update.abort();
                strlcpy(_otaErr,"canceled_final",sizeof(_otaErr));
                _otaOk=false;
            }else if(!Update.end(true)){
                strlcpy(_otaErr,"Update.end failed",sizeof(_otaErr));
                _otaOk=false;
            }else if(Update.hasError()){
                strlcpy(_otaErr,"Update.hasError",sizeof(_otaErr));
                _otaOk=false;
            }else{
                _otaOk=true;
                strlcpy(_otaErr,"ok",sizeof(_otaErr));
            }
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
        d["cancel_requested"]=_otaCancel;
        sendJson_(req,d);
    }
};

CL_W10_WebConfig* CL_W10_WebConfig::s_instance=nullptr;

