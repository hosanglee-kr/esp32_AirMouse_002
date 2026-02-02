// ======================================================
// File: src/v001/W10_WebConfig_011.h
// - /api/reset (defaults로 저장)
// - /api/reboot (ESP.restart())
// - POST /api/config 응답에 apply 결과 포함
// ======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : W10_WebConfig_011.h
 * 모듈약어 : W10
 * 모듈명 : Web Config Server (LittleFS Static + E10 Config API + Reset/Reboot)
 * ------------------------------------------------------
 * 기능 요약
 *  - ESPAsyncWebServer 기반 Web 설정 UI 제공(HTML/JS/CSS 분리, LittleFS에서 서빙)
 *  - GET  /              : /www/index.html 제공
 *  - GET  /www/app.js    : JS 제공
 *  - GET  /www/style.css : CSS 제공
 *  - GET  /api/config    : 현재 config.json 조회(JSON)
 *  - POST /api/config    : config.json 저장(JSON) + (옵션) apply 콜백 호출
 *  - POST /api/reset     : defaults로 config.json 저장(+백업) 후 apply
 *  - POST /api/reboot    : ESP.restart()
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
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include <ESPAsyncWebServer.h>

#include "C10_Config_010.h"

#ifndef W10_ENABLE_AP
#define W10_ENABLE_AP 1
#endif

#ifndef W10_AP_SSID
#define W10_AP_SSID "EliteAirMouse-Setup"
#endif

#ifndef W10_AP_PASS
#define W10_AP_PASS ""
#endif

class CL_W10_WebConfig {
  private:
    AsyncWebServer _server;

    CL_C10_Config* _cfg = nullptr;

    typedef bool (*T_W10_ApplyFn_t)(void* p_ctx);
    T_W10_ApplyFn_t _applyFn = nullptr;
    void*           _applyCtx = nullptr;

    String _rxBody;

    static constexpr const char* G_W10_WWW_INDEX = "/www/index.html";
    static constexpr const char* G_W10_WWW_JS    = "/www/app.js";
    static constexpr const char* G_W10_WWW_CSS   = "/www/style.css";

    static constexpr size_t G_W10_MAX_BODY = 4096;

    bool ensureFs() {
        if (!LittleFS.begin(true)) return false;
        return true;
    }

    bool makeJsonResponseFromCfg(const ST_C10_E10Config_t& p_cfg, String& p_outJson) {
        JsonDocument v_doc;

        v_doc["dpi_level"] = p_cfg.dpi_level;
        v_doc["hard_click_lock"] = p_cfg.hard_click_lock;

        JsonArray v_sb = v_doc["scale_base"].to<JsonArray>();
        v_sb.add(p_cfg.scale_base[0]);
        v_sb.add(p_cfg.scale_base[1]);
        v_sb.add(p_cfg.scale_base[2]);

        JsonArray v_ag = v_doc["accel_gain"].to<JsonArray>();
        v_ag.add(p_cfg.accel_gain[0]);
        v_ag.add(p_cfg.accel_gain[1]);
        v_ag.add(p_cfg.accel_gain[2]);

        v_doc["accel_threshold"] = p_cfg.accel_threshold;

        JsonObject v_w = v_doc["wheel"].to<JsonObject>();
        v_w["threshold_deg"] = p_cfg.wheel_threshold_deg;
        v_w["step_max"] = p_cfg.wheel_step_max;

        JsonObject v_g = v_doc["gesture"].to<JsonObject>();
        v_g["flick_deg"] = p_cfg.gesture_flick_deg;
        v_g["cooldown_ms"] = p_cfg.gesture_cooldown_ms;

        v_doc["scroll_cursor_damp"] = p_cfg.scroll_cursor_damp;

        p_outJson = "";
        serializeJson(v_doc, p_outJson);
        return true;
    }

    bool parseBodyToCfg(const String& p_body, ST_C10_E10Config_t& p_outCfg) {
        if (_cfg != nullptr) {
            (void)_cfg->loadE10(p_outCfg);
        } else {
            memset(&p_outCfg, 0, sizeof(p_outCfg));
        }

        JsonDocument v_doc;
        DeserializationError v_err = deserializeJson(v_doc, p_body);
        if (v_err) return false;

        p_outCfg.dpi_level = (uint8_t)(v_doc["dpi_level"] | p_outCfg.dpi_level);
        p_outCfg.hard_click_lock = (bool)(v_doc["hard_click_lock"] | p_outCfg.hard_click_lock);

        p_outCfg.scale_base[0] = (float)(v_doc["scale_base"][0] | p_outCfg.scale_base[0]);
        p_outCfg.scale_base[1] = (float)(v_doc["scale_base"][1] | p_outCfg.scale_base[1]);
        p_outCfg.scale_base[2] = (float)(v_doc["scale_base"][2] | p_outCfg.scale_base[2]);

        p_outCfg.accel_gain[0] = (float)(v_doc["accel_gain"][0] | p_outCfg.accel_gain[0]);
        p_outCfg.accel_gain[1] = (float)(v_doc["accel_gain"][1] | p_outCfg.accel_gain[1]);
        p_outCfg.accel_gain[2] = (float)(v_doc["accel_gain"][2] | p_outCfg.accel_gain[2]);

        p_outCfg.accel_threshold = (float)(v_doc["accel_threshold"] | p_outCfg.accel_threshold);

        p_outCfg.wheel_threshold_deg = (float)(v_doc["wheel"]["threshold_deg"] | p_outCfg.wheel_threshold_deg);
        p_outCfg.wheel_step_max      = (int16_t)(v_doc["wheel"]["step_max"] | p_outCfg.wheel_step_max);

        p_outCfg.gesture_flick_deg   = (float)(v_doc["gesture"]["flick_deg"] | p_outCfg.gesture_flick_deg);
        p_outCfg.gesture_cooldown_ms = (uint16_t)(v_doc["gesture"]["cooldown_ms"] | p_outCfg.gesture_cooldown_ms);

        p_outCfg.scroll_cursor_damp = (float)(v_doc["scroll_cursor_damp"] | p_outCfg.scroll_cursor_damp);

        // 범위 보호
        if (p_outCfg.dpi_level < 1) p_outCfg.dpi_level = 1;
        if (p_outCfg.dpi_level > 3) p_outCfg.dpi_level = 3;

        if (p_outCfg.wheel_step_max < 1) p_outCfg.wheel_step_max = 1;
        if (p_outCfg.wheel_step_max > 20) p_outCfg.wheel_step_max = 20;

        if (p_outCfg.gesture_cooldown_ms < 100) p_outCfg.gesture_cooldown_ms = 100;
        if (p_outCfg.gesture_cooldown_ms > 2000) p_outCfg.gesture_cooldown_ms = 2000;

        if (p_outCfg.scroll_cursor_damp < 0.0f) p_outCfg.scroll_cursor_damp = 0.0f;
        if (p_outCfg.scroll_cursor_damp > 1.0f) p_outCfg.scroll_cursor_damp = 1.0f;

        return true;
    }

    bool callApply(bool& p_outApplied) {
        p_outApplied = true;
        if (_applyFn != nullptr) {
            p_outApplied = _applyFn(_applyCtx);
            return true;
        }
        return true;
    }

    void setupRoutes() {
        _server.on("/", HTTP_GET, [&](AsyncWebServerRequest* p_req) {
            if (!LittleFS.exists(G_W10_WWW_INDEX)) {
                p_req->send(500, "text/plain", "Missing /www/index.html (uploadfs required)");
                return;
            }
            p_req->send(LittleFS, G_W10_WWW_INDEX, "text/html");
        });

        _server.on("/www/app.js", HTTP_GET, [&](AsyncWebServerRequest* p_req) {
            if (!LittleFS.exists(G_W10_WWW_JS)) {
                p_req->send(404, "text/plain", "Missing /www/app.js");
                return;
            }
            p_req->send(LittleFS, G_W10_WWW_JS, "application/javascript");
        });

        _server.on("/www/style.css", HTTP_GET, [&](AsyncWebServerRequest* p_req) {
            if (!LittleFS.exists(G_W10_WWW_CSS)) {
                p_req->send(404, "text/plain", "Missing /www/style.css");
                return;
            }
            p_req->send(LittleFS, G_W10_WWW_CSS, "text/css");
        });

        _server.on("/api/ping", HTTP_GET, [&](AsyncWebServerRequest* p_req) {
            p_req->send(200, "application/json", "{\"ok\":true}");
        });

        _server.on("/api/config", HTTP_GET, [&](AsyncWebServerRequest* p_req) {
            if (_cfg == nullptr) {
                p_req->send(500, "application/json", "{\"ok\":false,\"err\":\"cfg_null\"}");
                return;
            }

            ST_C10_E10Config_t v_cfg;
            memset(&v_cfg, 0, sizeof(v_cfg));
            (void)_cfg->loadE10(v_cfg);

            String v_json;
            makeJsonResponseFromCfg(v_cfg, v_json);
            p_req->send(200, "application/json", v_json);
        });

        _server.on(
            "/api/config",
            HTTP_POST,
            [&](AsyncWebServerRequest* p_req) {
                if (_cfg == nullptr) {
                    p_req->send(500, "application/json", "{\"ok\":false,\"err\":\"cfg_null\"}");
                    return;
                }

                if (_rxBody.length() == 0) {
                    p_req->send(400, "application/json", "{\"ok\":false,\"err\":\"empty_body\"}");
                    return;
                }

                ST_C10_E10Config_t v_new;
                memset(&v_new, 0, sizeof(v_new));

                if (!parseBodyToCfg(_rxBody, v_new)) {
                    _rxBody = "";
                    p_req->send(400, "application/json", "{\"ok\":false,\"err\":\"json_parse\"}");
                    return;
                }

                const bool v_saved = _cfg->saveE10(v_new);
                _rxBody = "";

                if (!v_saved) {
                    p_req->send(500, "application/json", "{\"ok\":false,\"err\":\"save_fail\"}");
                    return;
                }

                bool v_applied = true;
                callApply(v_applied);

                JsonDocument v_doc;
                v_doc["ok"] = true;
                v_doc["applied"] = v_applied;

                String v_out;
                serializeJson(v_doc, v_out);
                p_req->send(200, "application/json", v_out);
            },
            nullptr,
            [&](AsyncWebServerRequest* p_req, uint8_t* p_data, size_t p_len, size_t p_index, size_t p_total) {
                if (p_index == 0) _rxBody = "";
                if (p_total > G_W10_MAX_BODY) {
                    _rxBody = "";
                    return;
                }
                for (size_t v_i = 0; v_i < p_len; v_i++) {
                    _rxBody += (char)p_data[v_i];
                    if (_rxBody.length() > G_W10_MAX_BODY) {
                        _rxBody = "";
                        return;
                    }
                }
            });

        // ✅ defaults로 리셋
        _server.on("/api/reset", HTTP_POST, [&](AsyncWebServerRequest* p_req) {
            if (_cfg == nullptr) {
                p_req->send(500, "application/json", "{\"ok\":false,\"err\":\"cfg_null\"}");
                return;
            }

            ST_C10_E10Config_t v_def;
            memset(&v_def, 0, sizeof(v_def));
            _cfg->makeDefaultsE10(v_def);   // ✅ C10에 함수 추가 필요(아래 “수정해야 할 부분” 참고)

            const bool v_saved = _cfg->saveE10(v_def);
            if (!v_saved) {
                p_req->send(500, "application/json", "{\"ok\":false,\"err\":\"save_fail\"}");
                return;
            }

            bool v_applied = true;
            callApply(v_applied);

            JsonDocument v_doc;
            v_doc["ok"] = true;
            v_doc["applied"] = v_applied;

            String v_out;
            serializeJson(v_doc, v_out);
            p_req->send(200, "application/json", v_out);
        });

        // ✅ 재부팅
        _server.on("/api/reboot", HTTP_POST, [&](AsyncWebServerRequest* p_req) {
            p_req->send(200, "application/json", "{\"ok\":true}");
            delay(80);
            ESP.restart();
        });
    }

  public:
    CL_W10_WebConfig() : _server(80) {}

    bool begin(CL_C10_Config* p_cfg, T_W10_ApplyFn_t p_applyFn = nullptr, void* p_applyCtx = nullptr) {
        _cfg = p_cfg;
        _applyFn = p_applyFn;
        _applyCtx = p_applyCtx;

        if (!ensureFs()) return false;

#if (W10_ENABLE_AP == 1)
        WiFi.mode(WIFI_AP);
        if (strlen(W10_AP_PASS) == 0) WiFi.softAP(W10_AP_SSID);
        else WiFi.softAP(W10_AP_SSID, W10_AP_PASS);

        IPAddress v_ip = WiFi.softAPIP();
        Serial.printf("[W10] AP: %s  IP: %s\n", W10_AP_SSID, v_ip.toString().c_str());
#endif

        setupRoutes();
        _server.begin();
        Serial.println("[W10] Web server started.");
        return true;
    }
};

