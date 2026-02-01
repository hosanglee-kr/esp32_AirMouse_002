#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : C10_Config_017.h
 * 모듈약어 : C10
 * 모듈명 : Config Manager (LittleFS JSON, WiFi STA/AP, ver+migrate+bak)
 * ------------------------------------------------------
 * 기능 요약
 *  - LittleFS 기반 config.json 저장/로드 + .bak 백업
 *  - 버전(ver) 기반 마이그레이션 (ver 불일치 시 defaults -> patchFromJson -> save)
 *  - WiFi 설정(STA/AP/Auto) + mDNS hostname 저장
 *  - E10 튜닝/키매핑 설정 제공
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
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#ifndef C10_CFG_PATH
  #define C10_CFG_PATH "/config.json"
#endif

#ifndef C10_CFG_BAK_PATH
  #define C10_CFG_BAK_PATH "/config.json.bak"
#endif

#ifndef C10_CFG_VER
  #define C10_CFG_VER 17
#endif

enum EN_C10_WiFiMode : uint8_t {
    EN_C10_WIFI_AUTO = 0, // ssid 있으면 STA 시도, 실패하면 AP
    EN_C10_WIFI_AP   = 1, // 항상 AP
    EN_C10_WIFI_STA  = 2  // 항상 STA (실패 시에도 AP로 안감)
};

struct ST_C10_PptKey_t {
    uint8_t mod; // bitmask: 0x01 ctrl, 0x02 shift, 0x04 alt, 0x08 meta
    uint8_t key; // HID usage ID(Keyboard page, 0x07)
};

struct ST_C10_WiFiConfig_t {
    uint16_t ver;

    uint8_t  mode;            // EN_C10_WiFiMode
    char     sta_ssid[33];
    char     sta_pass[65];

    char     ap_ssid[33];
    char     ap_pass[65];

    char     mdns_host[33];   // 예: "elite-airmouse"
};

struct ST_C10_E10Config_t {
    uint16_t ver;

    uint8_t dpi_level;
    bool    hard_click_lock;

    float scale_base[3];
    float accel_gain[3];
    float accel_threshold;

    float   wheel_threshold_deg;
    int16_t wheel_step_max;

    float    gesture_flick_deg;
    uint16_t gesture_cooldown_ms;

    float scroll_cursor_damp;

    ST_C10_PptKey_t ppt_start;
    ST_C10_PptKey_t ppt_exit;
    ST_C10_PptKey_t ppt_next;
    ST_C10_PptKey_t ppt_prev;
    ST_C10_PptKey_t ppt_black;
    ST_C10_PptKey_t ppt_laser;
};

class CL_C10_Config {
  private:
    bool _fsReady = false;

    bool ensureFs(bool p_formatOnFail) {
        if (_fsReady) return true;
        _fsReady = LittleFS.begin(p_formatOnFail);
        return _fsReady;
    }

    bool readFileToString(const char* p_path, String& p_out) {
        p_out = "";
        File v_f = LittleFS.open(p_path, "r");
        if (!v_f) return false;
        while (v_f.available()) p_out += (char)v_f.read();
        v_f.close();
        return true;
    }

    bool writeStringToFile(const char* p_path, const String& p_data) {
        File v_f = LittleFS.open(p_path, "w");
        if (!v_f) return false;
        size_t v_n = v_f.print(p_data);
        v_f.close();
        return (v_n == p_data.length());
    }

    static void patchPpt(const JsonVariant& p_root, const char* p_name, ST_C10_PptKey_t& p_k) {
        p_k.mod = (uint8_t)(p_root[p_name]["mod"] | p_k.mod);
        p_k.key = (uint8_t)(p_root[p_name]["key"] | p_k.key);
    }

    static void fillPpt(JsonObject p_root, const char* p_name, const ST_C10_PptKey_t& p_k) {
        JsonObject v_o = p_root[p_name].to<JsonObject>();
        v_o["mod"] = p_k.mod;
        v_o["key"] = p_k.key;
    }

  public:
    CL_C10_Config() {}

    bool begin(bool p_formatOnFail = true) {
        return ensureFs(p_formatOnFail);
    }

    // -----------------------------
    // Defaults
    // -----------------------------
    void makeDefaultsWiFi(ST_C10_WiFiConfig_t& p_out) {
        memset(&p_out, 0, sizeof(p_out));
        p_out.ver = (uint16_t)C10_CFG_VER;

        p_out.mode = (uint8_t)EN_C10_WIFI_AUTO;

        strlcpy(p_out.sta_ssid, "", sizeof(p_out.sta_ssid));
        strlcpy(p_out.sta_pass, "", sizeof(p_out.sta_pass));

        strlcpy(p_out.ap_ssid, "EliteAirMouse", sizeof(p_out.ap_ssid));
        strlcpy(p_out.ap_pass, "12345678", sizeof(p_out.ap_pass));

        strlcpy(p_out.mdns_host, "elite-airmouse", sizeof(p_out.mdns_host));
    }

    void makeDefaultsE10(ST_C10_E10Config_t& p_out) {
        memset(&p_out, 0, sizeof(p_out));
        p_out.ver = (uint16_t)C10_CFG_VER;

        p_out.dpi_level = 2;
        p_out.hard_click_lock = true;

        p_out.scale_base[0] = 0.55f;
        p_out.scale_base[1] = 0.75f;
        p_out.scale_base[2] = 1.00f;

        p_out.accel_gain[0] = 0.35f;
        p_out.accel_gain[1] = 0.55f;
        p_out.accel_gain[2] = 0.85f;

        p_out.accel_threshold = 8.0f;

        p_out.wheel_threshold_deg = 90.0f;
        p_out.wheel_step_max = 6;

        p_out.gesture_flick_deg = 200.0f;
        p_out.gesture_cooldown_ms = 600;

        p_out.scroll_cursor_damp = 0.25f;

        // 기본 PPT 매핑(Usage ID 기준)
        // START: Shift + F5 (Shift=0x02, F5=0x3E)
        p_out.ppt_start = { 0x02, 0x3E };
        // EXIT: Esc (0x29)
        p_out.ppt_exit  = { 0x00, 0x29 };
        // NEXT: PageDown (0x4E)
        p_out.ppt_next  = { 0x00, 0x4E };
        // PREV: PageUp (0x4B)
        p_out.ppt_prev  = { 0x00, 0x4B };
        // BLACK: b (0x05)
        p_out.ppt_black = { 0x00, 0x05 };
        // LASER: Ctrl + l (Ctrl=0x01, l=0x0F)
        p_out.ppt_laser = { 0x01, 0x0F };
    }

    // -----------------------------
    // Patch from JSON
    // -----------------------------
    bool patchFromJsonWiFi(const String& p_json, ST_C10_WiFiConfig_t& p_inOut) {
        JsonDocument v_doc;
        DeserializationError v_err = deserializeJson(v_doc, p_json);
        if (v_err) return false;

        JsonVariant v_w = v_doc["wifi"];
        p_inOut.mode = (uint8_t)(v_w["mode"] | p_inOut.mode);

        const char* v_staSsid = v_w["sta"]["ssid"] | p_inOut.sta_ssid;
        const char* v_staPass = v_w["sta"]["pass"] | p_inOut.sta_pass;
        const char* v_apSsid  = v_w["ap"]["ssid"]  | p_inOut.ap_ssid;
        const char* v_apPass  = v_w["ap"]["pass"]  | p_inOut.ap_pass;
        const char* v_mdns    = v_w["mdns"]["host"] | p_inOut.mdns_host;

        strlcpy(p_inOut.sta_ssid, v_staSsid, sizeof(p_inOut.sta_ssid));
        strlcpy(p_inOut.sta_pass, v_staPass, sizeof(p_inOut.sta_pass));
        strlcpy(p_inOut.ap_ssid,  v_apSsid,  sizeof(p_inOut.ap_ssid));
        strlcpy(p_inOut.ap_pass,  v_apPass,  sizeof(p_inOut.ap_pass));
        strlcpy(p_inOut.mdns_host, v_mdns,    sizeof(p_inOut.mdns_host));

        return true;
    }

    bool patchFromJsonE10(const String& p_json, ST_C10_E10Config_t& p_inOut) {
        JsonDocument v_doc;
        DeserializationError v_err = deserializeJson(v_doc, p_json);
        if (v_err) return false;

        JsonVariant v_e10 = v_doc["e10"];
        p_inOut.dpi_level = (uint8_t)(v_e10["dpi_level"] | p_inOut.dpi_level);
        p_inOut.hard_click_lock = (bool)(v_e10["hard_click_lock"] | p_inOut.hard_click_lock);

        p_inOut.scale_base[0] = (float)(v_e10["scale_base"][0] | p_inOut.scale_base[0]);
        p_inOut.scale_base[1] = (float)(v_e10["scale_base"][1] | p_inOut.scale_base[1]);
        p_inOut.scale_base[2] = (float)(v_e10["scale_base"][2] | p_inOut.scale_base[2]);

        p_inOut.accel_gain[0] = (float)(v_e10["accel_gain"][0] | p_inOut.accel_gain[0]);
        p_inOut.accel_gain[1] = (float)(v_e10["accel_gain"][1] | p_inOut.accel_gain[1]);
        p_inOut.accel_gain[2] = (float)(v_e10["accel_gain"][2] | p_inOut.accel_gain[2]);

        p_inOut.accel_threshold = (float)(v_e10["accel_threshold"] | p_inOut.accel_threshold);

        p_inOut.wheel_threshold_deg = (float)(v_e10["wheel"]["threshold_deg"] | p_inOut.wheel_threshold_deg);
        p_inOut.wheel_step_max = (int16_t)(v_e10["wheel"]["step_max"] | p_inOut.wheel_step_max);

        p_inOut.gesture_flick_deg = (float)(v_e10["gesture"]["flick_deg"] | p_inOut.gesture_flick_deg);
        p_inOut.gesture_cooldown_ms = (uint16_t)(v_e10["gesture"]["cooldown_ms"] | p_inOut.gesture_cooldown_ms);

        p_inOut.scroll_cursor_damp = (float)(v_e10["scroll_cursor_damp"] | p_inOut.scroll_cursor_damp);

        JsonVariant v_pk = v_e10["ppt_keys"];
        patchPpt(v_pk, "start", p_inOut.ppt_start);
        patchPpt(v_pk, "exit",  p_inOut.ppt_exit);
        patchPpt(v_pk, "next",  p_inOut.ppt_next);
        patchPpt(v_pk, "prev",  p_inOut.ppt_prev);
        patchPpt(v_pk, "black", p_inOut.ppt_black);
        patchPpt(v_pk, "laser", p_inOut.ppt_laser);

        if (p_inOut.dpi_level < 1) p_inOut.dpi_level = 1;
        if (p_inOut.dpi_level > 3) p_inOut.dpi_level = 3;

        if (p_inOut.wheel_step_max < 1) p_inOut.wheel_step_max = 1;
        if (p_inOut.wheel_step_max > 40) p_inOut.wheel_step_max = 40;

        if (p_inOut.gesture_cooldown_ms < 100) p_inOut.gesture_cooldown_ms = 100;
        if (p_inOut.gesture_cooldown_ms > 3000) p_inOut.gesture_cooldown_ms = 3000;

        if (p_inOut.scroll_cursor_damp < 0.0f) p_inOut.scroll_cursor_damp = 0.0f;
        if (p_inOut.scroll_cursor_damp > 1.0f) p_inOut.scroll_cursor_damp = 1.0f;

        return true;
    }

    // -----------------------------
    // Serialize to JSON (single doc)
    // -----------------------------
    bool toJsonAll(const ST_C10_WiFiConfig_t& p_w, const ST_C10_E10Config_t& p_e, String& p_outJson) {
        JsonDocument v_doc;

        v_doc["ver"] = (uint16_t)C10_CFG_VER;

        // wifi
        JsonObject v_w = v_doc["wifi"].to<JsonObject>();
        v_w["mode"] = p_w.mode;
        JsonObject v_sta = v_w["sta"].to<JsonObject>();
        v_sta["ssid"] = p_w.sta_ssid;
        v_sta["pass"] = p_w.sta_pass;
        JsonObject v_ap  = v_w["ap"].to<JsonObject>();
        v_ap["ssid"] = p_w.ap_ssid;
        v_ap["pass"] = p_w.ap_pass;
        JsonObject v_mdns = v_w["mdns"].to<JsonObject>();
        v_mdns["host"] = p_w.mdns_host;

        // e10
        JsonObject v_e10 = v_doc["e10"].to<JsonObject>();
        v_e10["dpi_level"] = p_e.dpi_level;
        v_e10["hard_click_lock"] = p_e.hard_click_lock;

        JsonArray v_sb = v_e10["scale_base"].to<JsonArray>();
        v_sb.add(p_e.scale_base[0]); v_sb.add(p_e.scale_base[1]); v_sb.add(p_e.scale_base[2]);

        JsonArray v_ag = v_e10["accel_gain"].to<JsonArray>();
        v_ag.add(p_e.accel_gain[0]); v_ag.add(p_e.accel_gain[1]); v_ag.add(p_e.accel_gain[2]);

        v_e10["accel_threshold"] = p_e.accel_threshold;

        JsonObject v_wh = v_e10["wheel"].to<JsonObject>();
        v_wh["threshold_deg"] = p_e.wheel_threshold_deg;
        v_wh["step_max"] = p_e.wheel_step_max;

        JsonObject v_g = v_e10["gesture"].to<JsonObject>();
        v_g["flick_deg"] = p_e.gesture_flick_deg;
        v_g["cooldown_ms"] = p_e.gesture_cooldown_ms;

        v_e10["scroll_cursor_damp"] = p_e.scroll_cursor_damp;

        JsonObject v_pk = v_e10["ppt_keys"].to<JsonObject>();
        fillPpt(v_pk, "start", p_e.ppt_start);
        fillPpt(v_pk, "exit",  p_e.ppt_exit);
        fillPpt(v_pk, "next",  p_e.ppt_next);
        fillPpt(v_pk, "prev",  p_e.ppt_prev);
        fillPpt(v_pk, "black", p_e.ppt_black);
        fillPpt(v_pk, "laser", p_e.ppt_laser);

        p_outJson = "";
        serializeJson(v_doc, p_outJson);
        return true;
    }

    // -----------------------------
    // Load/Save All
    // -----------------------------
    bool loadAll(ST_C10_WiFiConfig_t& p_outWiFi, ST_C10_E10Config_t& p_outE10) {
        makeDefaultsWiFi(p_outWiFi);
        makeDefaultsE10(p_outE10);

        if (!_fsReady) return false;
        if (!LittleFS.exists(C10_CFG_PATH)) return true;

        String v_json;
        if (!readFileToString(C10_CFG_PATH, v_json)) return false;

        uint16_t v_fileVer = 0;
        {
            JsonDocument v_doc;
            if (!deserializeJson(v_doc, v_json)) {
                v_fileVer = (uint16_t)(v_doc["ver"] | 0);
            }
        }

        (void)patchFromJsonWiFi(v_json, p_outWiFi);
        (void)patchFromJsonE10(v_json, p_outE10);

        if (v_fileVer != (uint16_t)C10_CFG_VER) {
            p_outWiFi.ver = (uint16_t)C10_CFG_VER;
            p_outE10.ver  = (uint16_t)C10_CFG_VER;
            (void)saveAll(p_outWiFi, p_outE10);
        }
        return true;
    }

    bool saveAll(const ST_C10_WiFiConfig_t& p_w, const ST_C10_E10Config_t& p_e) {
        if (!_fsReady) return false;

        if (LittleFS.exists(C10_CFG_PATH)) {
            String v_old;
            if (readFileToString(C10_CFG_PATH, v_old)) (void)writeStringToFile(C10_CFG_BAK_PATH, v_old);
        }

        String v_json;
        ST_C10_WiFiConfig_t v_w = p_w;
        ST_C10_E10Config_t  v_e = p_e;
        v_w.ver = (uint16_t)C10_CFG_VER;
        v_e.ver = (uint16_t)C10_CFG_VER;

        (void)toJsonAll(v_w, v_e, v_json);
        return writeStringToFile(C10_CFG_PATH, v_json);
    }
};
