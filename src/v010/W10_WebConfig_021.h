#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : W10_WebConfig_021.h
 * 모듈약어 : W10
 * 모듈명 : Web Config Server (Backup/Rollback/Export/Import + OTA + Extended Status)
 * ------------------------------------------------------
 * 기능 요약
 *  - /api/export : config.json 원문 반환(text/json)
 *  - /api/import : config.json 원문 입력 -> validate -> backup -> 저장 -> apply
 *  - /api/rollback : config.bak -> config.json 롤백 -> apply
 *  - OTA(Web):
 *     - GET  /ota            : 업로드 페이지
 *     - POST /api/ota/upload : firmware.bin 업로드(Update)
 *     - GET  /api/ota/status : 진행률/결과
 *  - /api/status 확장:
 *     - noise(분산), recover 결과, hist(최근 N초 에러)
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
#include <Update.h>

#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include <ESPAsyncWebServer.h>

#include "C10_Config_021.h"
#include "E10_EliteAirMouse_021.h"

class CL_W10_WebConfig {
  private:
    AsyncWebServer _svr;

    CL_C10_Config* _cfg = nullptr;
    T_E10_ApplyFn  _applyFn = nullptr;
    void*          _applyCtx = nullptr;

    ST_C10_WiFiConfig_t _wifi;
    ST_C10_E10Config_t  _e10;

    bool _mdnsStarted = false;

    // OTA state
    struct ST_W10_Ota_t {
        bool     running;
        bool     done;
        bool     ok;
        uint32_t total;
        uint32_t written;
        int      err;
        uint32_t started_ms;
        uint32_t ended_ms;
    } _ota;

    static void s_handleWifiEvent(WiFiEvent_t p_event, WiFiEventInfo_t p_info);
    void _handleWifi(WiFiEvent_t p_event);
    static CL_W10_WebConfig* s_instance;

  public:
    CL_W10_WebConfig() : _svr(80) {
        s_instance = this;
        memset(&_wifi, 0, sizeof(_wifi));
        memset(&_e10,  0, sizeof(_e10));
        memset(&_ota,  0, sizeof(_ota));
    }

    void begin(CL_C10_Config* p_cfg, T_E10_ApplyFn p_applyFn, void* p_applyCtx) {
        _cfg = p_cfg;
        _applyFn = p_applyFn;
        _applyCtx = p_applyCtx;

        WiFi.onEvent(s_handleWifiEvent);

        (void)LittleFS.begin(true);
        (void)_cfg->loadAll(_wifi, _e10);

        setupWiFi();

        // --- Basic web ---
        _svr.on("/", HTTP_GET, [this](AsyncWebServerRequest* p_req){
            p_req->send(200, "text/plain", "Elite AirMouse S3 - v021 (WebConfig)");
        });

        // --- Export/Import/Rollback ---
        _svr.on("/api/export", HTTP_GET, [this](AsyncWebServerRequest* p_req){ this->apiExport(p_req); });

        _svr.on("/api/import", HTTP_POST,
            [this](AsyncWebServerRequest* p_req){ (void)p_req; },
            nullptr,
            [this](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total){
                this->apiImport(p_req, p_data, p_len, p_index, p_total);
            }
        );

        _svr.on("/api/rollback", HTTP_POST,
            [this](AsyncWebServerRequest* p_req){ (void)p_req; },
            nullptr,
            [this](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total){
                (void)p_data; (void)p_len; (void)p_index; (void)p_total;
                this->apiRollback(p_req);
            }
        );

        // --- Status + Control (minimal) ---
        _svr.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* p_req){ this->apiStatus(p_req); });

        _svr.on("/api/control", HTTP_POST,
            [this](AsyncWebServerRequest* p_req){ (void)p_req; },
            nullptr,
            [this](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total){
                this->apiControl(p_req, p_data, p_len, p_index, p_total);
            }
        );

        // --- OTA page ---
        _svr.on("/ota", HTTP_GET, [this](AsyncWebServerRequest* p_req){ this->otaPage(p_req); });

        _svr.on("/api/ota/status", HTTP_GET, [this](AsyncWebServerRequest* p_req){ this->apiOtaStatus(p_req); });

        // OTA upload (multipart)
        _svr.on("/api/ota/upload", HTTP_POST,
            [this](AsyncWebServerRequest* p_req){
                // finalize response
                JsonDocument v_doc;
                v_doc["ok"] = _ota.ok;
                v_doc["done"] = _ota.done;
                v_doc["err"] = _ota.err;
                String v_out; serializeJson(v_doc, v_out);
                p_req->send(_ota.ok ? 200 : 500, "application/json", v_out);

                if (_ota.ok) {
                    delay(200);
                    ESP.restart();
                }
            },
            [this](AsyncWebServerRequest* p_req, const String& p_filename, size_t p_index, uint8_t* p_data, size_t p_len, bool p_final){
                this->otaUpload(p_req, p_filename, p_index, p_data, p_len, p_final);
            }
        );

        _svr.onNotFound([](AsyncWebServerRequest* p_req){
            p_req->send(404, "text/plain", "not found");
        });

        _svr.begin();
        Serial.println("[W10] started");
    }

  private:
    // -------- WiFi + mDNS --------
    void setupWiFi() {
        WiFi.mode(WIFI_MODE_NULL);

        const bool v_hasSta   = (_wifi.sta_ssid[0] != '\0');
        const bool v_auto     = (_wifi.mode == (uint8_t)EN_C10_WIFI_AUTO);
        const bool v_forceAp  = (_wifi.mode == (uint8_t)EN_C10_WIFI_AP);
        const bool v_forceSta = (_wifi.mode == (uint8_t)EN_C10_WIFI_STA);

        if (v_forceAp || (!v_hasSta && (v_auto || !v_forceSta))) {
            WiFi.mode(WIFI_AP);
            WiFi.softAP(_wifi.ap_ssid, _wifi.ap_pass);
            Serial.printf("[W10] AP: %s ip=%s\n", _wifi.ap_ssid, WiFi.softAPIP().toString().c_str());
            return;
        }

        WiFi.mode(WIFI_STA);
        WiFi.begin(_wifi.sta_ssid, _wifi.sta_pass);

        const uint32_t v_t0 = millis();
        bool v_ok = false;
        while (millis() - v_t0 < 8000) {
            if (WiFi.status() == WL_CONNECTED) { v_ok = true; break; }
            delay(200);
        }
        if (!v_ok && v_auto) {
            WiFi.mode(WIFI_AP);
            WiFi.softAP(_wifi.ap_ssid, _wifi.ap_pass);
            Serial.printf("[W10] STA fail -> AP: %s ip=%s\n", _wifi.ap_ssid, WiFi.softAPIP().toString().c_str());
        }
    }

    void startMdnsIfPossible() {
        if (_wifi.mdns_host[0] == '\0') return;
        if (WiFi.getMode() != WIFI_STA) return;
        if (WiFi.status() != WL_CONNECTED) return;
        if (_mdnsStarted) return;

        if (!MDNS.begin(_wifi.mdns_host)) {
            Serial.println("[W10] mDNS begin failed");
            _mdnsStarted = false;
            return;
        }
        MDNS.addService("http", "tcp", 80);
        _mdnsStarted = true;
        Serial.printf("[W10] mDNS: http://%s.local/\n", _wifi.mdns_host);
    }

    void stopMdnsIfRunning() {
        if (!_mdnsStarted) return;
        MDNS.end();
        _mdnsStarted = false;
        Serial.println("[W10] mDNS stopped");
    }

    // -------- response helpers --------
    void sendJson(AsyncWebServerRequest* p_req, JsonDocument& p_doc) {
        String v_out;
        serializeJson(p_doc, v_out);
        p_req->send(200, "application/json", v_out);
    }

    String* getOrCreateReqBody(AsyncWebServerRequest* p_req, size_t p_index) {
        if (p_index == 0) {
            if (p_req->_tempObject != nullptr) {
                delete (String*)p_req->_tempObject;
                p_req->_tempObject = nullptr;
            }
            p_req->_tempObject = new String();
        }
        return (String*)p_req->_tempObject;
    }

    void finalizeReqBody(AsyncWebServerRequest* p_req) {
        if (p_req->_tempObject != nullptr) {
            delete (String*)p_req->_tempObject;
            p_req->_tempObject = nullptr;
        }
    }

    // -------- /api/export --------
    void apiExport(AsyncWebServerRequest* p_req) {
        String v_raw;
        bool v_ok = _cfg->exportRaw(v_raw);
        if (!v_ok) { p_req->send(500, "text/plain", "export failed"); return; }

        // raw config is JSON text
        AsyncWebServerResponse* v_res = p_req->beginResponse(200, "application/json", v_raw);
        v_res->addHeader("Cache-Control", "no-store");
        p_req->send(v_res);
    }

    // -------- /api/import --------
    void apiImport(AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total) {
        String* v_body = getOrCreateReqBody(p_req, p_index);
        if (v_body == nullptr) { p_req->send(500, "application/json", "{\"ok\":false}"); return; }

        for (size_t i = 0; i < p_len; i++) (*v_body) += (char)p_data[i];
        if (p_index + p_len < p_total) return;

        String v_in = *v_body;
        finalizeReqBody(p_req);

        bool v_ok = _cfg->importRaw(v_in, true);
        bool v_applied = false;
        if (v_ok && _applyFn != nullptr) v_applied = _applyFn(_applyCtx);

        JsonDocument v_doc;
        v_doc["ok"] = v_ok;
        v_doc["applied"] = v_applied;
        v_doc["has_backup"] = _cfg->hasBackup();
        sendJson(p_req, v_doc);
    }

    // -------- /api/rollback --------
    void apiRollback(AsyncWebServerRequest* p_req) {
        bool v_ok = _cfg->rollbackFromBak();
        bool v_applied = false;
        if (v_ok && _applyFn != nullptr) v_applied = _applyFn(_applyCtx);

        JsonDocument v_doc;
        v_doc["ok"] = v_ok;
        v_doc["applied"] = v_applied;
        sendJson(p_req, v_doc);
    }

    // -------- /api/control (P1: precision toggle 포함) --------
    void apiControl(AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total) {
        String* v_body = getOrCreateReqBody(p_req, p_index);
        if (v_body == nullptr) { p_req->send(500, "application/json", "{\"ok\":false}"); return; }

        for (size_t i = 0; i < p_len; i++) (*v_body) += (char)p_data[i];
        if (p_index + p_len < p_total) return;

        JsonDocument v_doc;
        DeserializationError v_err = deserializeJson(v_doc, *v_body);
        finalizeReqBody(p_req);

        if (v_err) { p_req->send(400, "application/json", "{\"ok\":false,\"err\":\"bad_json\"}"); return; }

        CL_E10_EliteAirMouse* v_e10 = (CL_E10_EliteAirMouse*)_applyCtx;
        bool v_ok = true;

        if (v_e10 != nullptr) {
            if (!v_doc["ppt_mode"].isNull()) v_ok = v_ok && v_e10->setPptMode((bool)v_doc["ppt_mode"]);
            if (!v_doc["dpi_level"].isNull()) v_ok = v_ok && v_e10->setDpiLevel((uint8_t)v_doc["dpi_level"]);
            if (!v_doc["hard_click_lock"].isNull()) v_ok = v_ok && v_e10->setHardClickLock((bool)v_doc["hard_click_lock"]);

            if (!v_doc["precision"].isNull()) {
                JsonVariant p = v_doc["precision"];
                if (!p["enable"].isNull()) v_ok = v_ok && v_e10->setPrecisionEnable((bool)p["enable"]);
                if (!p["scale"].isNull())  v_ok = v_ok && v_e10->setPrecisionScale((float)p["scale"]);
            }
        } else v_ok = false;

        JsonDocument v_out;
        v_out["ok"] = v_ok;
        sendJson(p_req, v_out);
    }

    // -------- /api/status (noise/recover/history 포함) --------
    void apiStatus(AsyncWebServerRequest* p_req) {
        JsonDocument v_doc;

        JsonObject v_net = v_doc["net"].to<JsonObject>();
        v_net["mode"] = (WiFi.getMode() == WIFI_AP) ? "AP" : "STA";
        v_net["ip"] = (WiFi.getMode() == WIFI_AP) ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
        v_net["ssid"] = (WiFi.getMode() == WIFI_AP) ? String(_wifi.ap_ssid) : WiFi.SSID();
        v_net["mdns"] = String(_wifi.mdns_host) + ".local";

        v_doc["heap_free"] = (uint32_t)ESP.getFreeHeap();
        v_doc["uptime_ms"] = (uint32_t)millis();

        CL_E10_EliteAirMouse* v_e10 = (CL_E10_EliteAirMouse*)_applyCtx;
        if (v_e10 != nullptr) {
            ST_E10_Status_t v_s;
            v_e10->getStatus(v_s);

            JsonObject e = v_doc["e10"].to<JsonObject>();
            e["ble_connected"] = v_s.ble_connected;
            e["ppt_mode"] = v_s.ppt_mode;
            e["dpi_level"] = v_s.dpi_level;
            e["degraded"] = v_s.degraded;

            JsonObject pr = e["precision"].to<JsonObject>();
            pr["enable"] = v_s.precision_enable;
            pr["state"] = v_s.precision_state;
            pr["scale"] = v_s.precision_scale;

            e["rssi"] = v_s.rssi;
            e["reconnect_count"] = v_s.reconnect_count;

            JsonObject gyro = e["gyro"].to<JsonObject>();
            gyro["calib_done"] = v_s.gyro_calib_done;
            gyro["bias_x"] = v_s.gyro_bias_x;
            gyro["bias_y"] = v_s.gyro_bias_y;
            gyro["bias_z"] = v_s.gyro_bias_z;

            e["temp_c"] = v_s.temp_c;

            JsonObject t = e["timing"].to<JsonObject>();
            t["sampling_ms_target"] = v_s.sampling_ms_target;
            t["sampling_ms_avg"] = v_s.sampling_ms_avg;

            JsonObject n = e["noise"].to<JsonObject>();
            JsonObject ng = n["gyro_var"].to<JsonObject>();
            ng["x"] = v_s.gyro_var_x; ng["y"] = v_s.gyro_var_y; ng["z"] = v_s.gyro_var_z;
            JsonObject na = n["acc_var"].to<JsonObject>();
            na["x"] = v_s.acc_var_x; na["y"] = v_s.acc_var_y; na["z"] = v_s.acc_var_z;

            JsonObject r = e["recover"].to<JsonObject>();
            r["last_ok"] = v_s.recover_last_ok;
            r["last_ms"] = v_s.recover_last_ms;
            r["fail_streak"] = v_s.mpu_fail_streak;
            r["err_mpu_recover"] = v_s.err_mpu_recover;

            JsonObject err = e["err"].to<JsonObject>();
            err["mpu_read"] = v_s.err_mpu_read;
            err["task_overrun"] = v_s.err_task_overrun;

            JsonObject hist = e["hist"].to<JsonObject>();
            hist["len"] = v_s.hist_len;
            hist["pos"] = v_s.hist_pos;

            // arrays
            JsonArray a1 = hist["mpu_read"].to<JsonArray>();
            JsonArray a2 = hist["recover"].to<JsonArray>();
            JsonArray a3 = hist["overrun"].to<JsonArray>();
            for (uint8_t i = 0; i < v_s.hist_len; i++) {
                a1.add((uint16_t)v_s.hist_mpu_read[i]);
                a2.add((uint16_t)v_s.hist_recover[i]);
                a3.add((uint16_t)v_s.hist_overrun[i]);
            }

            JsonObject st = e["stack"].to<JsonObject>();
            st["sensor_min_words"] = v_s.stack_sensor_min_words;
            st["comm_min_words"] = v_s.stack_comm_min_words;
        }

        sendJson(p_req, v_doc);
    }

    // -------- OTA page --------
    void otaPage(AsyncWebServerRequest* p_req) {
        const char* html =
            "<!doctype html><html><head><meta charset='utf-8'/>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'/>"
            "<title>OTA Update</title></head><body style='font-family:system-ui;padding:16px'>"
            "<h2>Elite AirMouse S3 - OTA</h2>"
            "<p>firmware.bin 업로드</p>"
            "<input id='f' type='file' accept='.bin'/><button id='b'>Upload</button>"
            "<pre id='log' style='background:#111;color:#0f0;padding:12px;border-radius:10px;white-space:pre-wrap'></pre>"
            "<script>"
            "const log=t=>document.getElementById('log').textContent+=t+'\\n';"
            "document.getElementById('b').onclick=async()=>{"
            "const fi=document.getElementById('f');if(!fi.files.length){alert('select bin');return;}"
            "const fd=new FormData();fd.append('firmware',fi.files[0],'firmware.bin');"
            "log('upload start...');"
            "const r=await fetch('/api/ota/upload',{method:'POST',body:fd});"
            "const txt=await r.text();log('resp: '+txt);"
            "};"
            "setInterval(async()=>{try{const r=await fetch('/api/ota/status',{cache:'no-store'});log('status: '+await r.text());}catch(e){}} , 1500);"
            "</script></body></html>";

        AsyncWebServerResponse* v_res = p_req->beginResponse(200, "text/html", html);
        v_res->addHeader("Cache-Control", "no-store");
        p_req->send(v_res);
    }

    // -------- OTA upload handler --------
    void otaUpload(AsyncWebServerRequest* p_req, const String& p_filename, size_t p_index, uint8_t* p_data, size_t p_len, bool p_final) {
        (void)p_req; (void)p_filename;

        if (p_index == 0) {
            memset(&_ota, 0, sizeof(_ota));
            _ota.running = true;
            _ota.started_ms = millis();

            // total size unknown here; will stay 0 unless client gives Content-Length (AsyncWebServer does not always provide)
            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                _ota.ok = false;
                _ota.err = (int)Update.getError();
                _ota.running = false;
                _ota.done = true;
                _ota.ended_ms = millis();
                return;
            }
            Update.setMD5(nullptr);
        }

        if (_ota.running) {
            size_t v_written = Update.write(p_data, p_len);
            _ota.written += (uint32_t)v_written;
            if (v_written != p_len) {
                _ota.ok = false;
                _ota.err = (int)Update.getError();
            } else {
                _ota.ok = true;
            }
        }

        if (p_final) {
            bool v_endOk = Update.end(true);
            _ota.ok = _ota.ok && v_endOk;
            if (!v_endOk) _ota.err = (int)Update.getError();
            _ota.running = false;
            _ota.done = true;
            _ota.ended_ms = millis();
        }
    }

    void apiOtaStatus(AsyncWebServerRequest* p_req) {
        JsonDocument v_doc;
        v_doc["running"] = _ota.running;
        v_doc["done"] = _ota.done;
        v_doc["ok"] = _ota.ok;
        v_doc["total"] = _ota.total;
        v_doc["written"] = _ota.written;
        v_doc["err"] = _ota.err;
        v_doc["started_ms"] = _ota.started_ms;
        v_doc["ended_ms"] = _ota.ended_ms;
        sendJson(p_req, v_doc);
    }
};

void CL_W10_WebConfig::s_handleWifiEvent(WiFiEvent_t p_event, WiFiEventInfo_t p_info) {
    (void)p_info;
    if (s_instance) s_instance->_handleWifi(p_event);
}

void CL_W10_WebConfig::_handleWifi(WiFiEvent_t p_event) {
    switch (p_event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.println("[W10] WiFi Connected (STA)");
            startMdnsIfPossible();
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.println("[W10] WiFi Disconnected");
            stopMdnsIfRunning();
            break;
        default:
            break;
    }
}

CL_W10_WebConfig* CL_W10_WebConfig::s_instance = nullptr;

