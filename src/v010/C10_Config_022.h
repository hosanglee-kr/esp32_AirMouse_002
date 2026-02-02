// =======================================================
// File: src/v022/C10_Config_022.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : C10_Config_022.h
 * 모듈약어 : C10
 * 모듈명 : Config Manager (Atomic Save, .bak Rollback, Export/Import, P0 Safe)
 * ------------------------------------------------------
 * 기능 요약
 *  - LittleFS /json/config.json 단일 설정 관리
 *  - P0: Atomic 저장(tmp→rename) + .bak 자동 백업/롤백(브릭 방지)
 *  - Export/Import: JSON 직렬화/역직렬화 기반
 *  - patchFromJson: 부분 패치 지원
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
#include <string.h>

static constexpr uint16_t G_C10_CFG_VER = 22;

// -----------------------------
// Enums
// -----------------------------
enum EN_C10_WIFI_MODE_t : uint8_t {
    EN_C10_WIFI_AUTO = 0,
    EN_C10_WIFI_AP   = 1,
    EN_C10_WIFI_STA  = 2
};

// -----------------------------
// Structs
// -----------------------------
struct ST_C10_WiFiConfig_t {
    // wifi.mode
    uint8_t mode;

    // wifi.sta.ssid/pass
    char sta_ssid[33];
    char sta_pass[65];

    // wifi.ap.ssid/pass
    char ap_ssid[33];
    char ap_pass[65];

    // wifi.mdns.host
    char mdns_host[33];
};

struct ST_C10_PptKey_t {
    // e10.ppt_keys.*.mod / e10.ppt_keys.*.key
    uint8_t  mod; // HID modifier mask (KeyboardInputReport.modifiers)
    uint16_t key; // HID usage id (page 0x07). 0이면 none
};

struct ST_C10_E10Config_t {
    // e10.dpi_level
    uint8_t dpi_level;

    // e10.hard_click_lock
    bool hard_click_lock;

    // e10.scale_base[3]
    float scale_base[3];

    // e10.accel_gain[3]
    float accel_gain[3];

    // e10.accel_threshold
    float accel_threshold;

    // e10.wheel.threshold_deg / step_max
    float wheel_threshold_deg;
    uint8_t wheel_step_max;

    // e10.gesture.flick_deg / cooldown_ms
    float gesture_flick_deg;
    uint16_t gesture_cooldown_ms;

    // e10.scroll_cursor_damp
    float scroll_cursor_damp;

    // e10.precision.*  (P2)
    bool  precision_enable;
    float precision_deadzone;
    float precision_gain;
    float precision_accel;
    uint8_t precision_max_step;
    float precision_smooth; // 0~1 (높을수록 부드러움)

    // e10.ppt_keys.*
    ST_C10_PptKey_t ppt_start;
    ST_C10_PptKey_t ppt_exit;
    ST_C10_PptKey_t ppt_next;
    ST_C10_PptKey_t ppt_prev;
    ST_C10_PptKey_t ppt_black;
    ST_C10_PptKey_t ppt_laser;
};

// -----------------------------
// Class
// -----------------------------
class CL_C10_Config {
  private:
    static constexpr const char* s_path_cfg = "/json/config.json";
    static constexpr const char* s_path_tmp = "/json/config.json.tmp";
    static constexpr const char* s_path_bak = "/json/config.json.bak";

  public:
    void begin(bool p_formatOnFail = true) {
        (void)LittleFS.begin(p_formatOnFail);
        if (!LittleFS.exists("/json")) {
            (void)LittleFS.mkdir("/json");
        }
        if (!LittleFS.exists(s_path_cfg)) {
            ST_C10_WiFiConfig_t v_w;
            ST_C10_E10Config_t  v_e;
            makeDefaultsWiFi(v_w);
            makeDefaultsE10(v_e);
            (void)saveAll(v_w, v_e);
        }
    }

    // -------------------------
    // Defaults
    // -------------------------
    void makeDefaultsWiFi(ST_C10_WiFiConfig_t& p_out) {
        memset(&p_out, 0, sizeof(p_out));
        p_out.mode = (uint8_t)EN_C10_WIFI_AUTO;

        strlcpy(p_out.sta_ssid, "", sizeof(p_out.sta_ssid));
        strlcpy(p_out.sta_pass, "", sizeof(p_out.sta_pass));

        strlcpy(p_out.ap_ssid, "EliteAirMouse", sizeof(p_out.ap_ssid));
        strlcpy(p_out.ap_pass, "12345678", sizeof(p_out.ap_pass));

        strlcpy(p_out.mdns_host, "elite-airmouse", sizeof(p_out.mdns_host));
    }

    void makeDefaultsE10(ST_C10_E10Config_t& p_out) {
        memset(&p_out, 0, sizeof(p_out));
        p_out.dpi_level = 2;
        p_out.hard_click_lock = true;

        p_out.scale_base[0] = 0.55f; p_out.scale_base[1] = 0.75f; p_out.scale_base[2] = 1.00f;
        p_out.accel_gain[0] = 0.35f; p_out.accel_gain[1] = 0.55f; p_out.accel_gain[2] = 0.85f;
        p_out.accel_threshold = 8.0f;

        p_out.wheel_threshold_deg = 90.0f;
        p_out.wheel_step_max = 6;

        p_out.gesture_flick_deg = 200.0f;
        p_out.gesture_cooldown_ms = 600;

        p_out.scroll_cursor_damp = 0.25f;

        // Precision (joystick-like feel)
        p_out.precision_enable = false;
        p_out.precision_deadzone = 1.2f;  // deg/s equivalent domain after engine output
        p_out.precision_gain = 0.65f;
        p_out.precision_accel = 0.25f;
        p_out.precision_max_step = 18;
        p_out.precision_smooth = 0.85f;

        // PPT default keys (usage id)
        // START: LShift + F5
        p_out.ppt_start.mod = 0x02; p_out.ppt_start.key = 0x3E; // Shift + F5
        // EXIT: Esc
        p_out.ppt_exit.mod = 0x00;  p_out.ppt_exit.key = 0x29;
        // NEXT: PageDown
        p_out.ppt_next.mod = 0x00;  p_out.ppt_next.key = 0x4E;
        // PREV: PageUp
        p_out.ppt_prev.mod = 0x00;  p_out.ppt_prev.key = 0x4B;
        // BLACK: B
        p_out.ppt_black.mod = 0x00; p_out.ppt_black.key = 0x05;
        // LASER: LCtrl + L
        p_out.ppt_laser.mod = 0x01; p_out.ppt_laser.key = 0x0F;
    }

    // -------------------------
    // Load/Save (P0 safe)
    // -------------------------
    bool loadAll(ST_C10_WiFiConfig_t& p_wifi, ST_C10_E10Config_t& p_e10) {
        File v_f = LittleFS.open(s_path_cfg, "r");
        if (!v_f) return false;

        JsonDocument v_doc;
        DeserializationError v_err = deserializeJson(v_doc, v_f);
        v_f.close();
        if (v_err) return false;

        // defaults
        makeDefaultsWiFi(p_wifi);
        makeDefaultsE10(p_e10);

        // patch
        String v_out;
        serializeJson(v_doc, v_out);
        (void)patchFromJsonWiFi(v_out, p_wifi);
        (void)patchFromJsonE10(v_out, p_e10);
        return true;
    }

    bool saveAll(const ST_C10_WiFiConfig_t& p_wifi, const ST_C10_E10Config_t& p_e10) {
        JsonDocument v_doc;
        buildJson(p_wifi, p_e10, v_doc);

        // (P0) atomic write: write tmp -> flush/close -> rotate bak -> rename tmp -> cfg
        // 1) tmp write
        File v_tmp = LittleFS.open(s_path_tmp, "w");
        if (!v_tmp) return false;

        if (serializeJson(v_doc, v_tmp) == 0) {
            v_tmp.close();
            (void)LittleFS.remove(s_path_tmp);
            return false;
        }
        v_tmp.flush();
        v_tmp.close();

        // 2) verify tmp parse (브릭 방지)
        if (!verifyJsonFile(s_path_tmp)) {
            (void)LittleFS.remove(s_path_tmp);
            return false;
        }

        // 3) rotate backup: cfg -> bak
        if (LittleFS.exists(s_path_bak)) (void)LittleFS.remove(s_path_bak);
        if (LittleFS.exists(s_path_cfg)) {
            // rename cfg -> bak
            if (!LittleFS.rename(s_path_cfg, s_path_bak)) {
                // fallback: copy then keep cfg
                (void)copyFile(s_path_cfg, s_path_bak);
            }
        }

        // 4) rename tmp -> cfg
        if (!LittleFS.rename(s_path_tmp, s_path_cfg)) {
            // restore from bak if possible
            (void)LittleFS.remove(s_path_cfg);
            if (LittleFS.exists(s_path_bak)) (void)LittleFS.rename(s_path_bak, s_path_cfg);
            (void)LittleFS.remove(s_path_tmp);
            return false;
        }

        return true;
    }

    bool rollbackFromBak() {
        if (!LittleFS.exists(s_path_bak)) return false;
        // verify bak first
        if (!verifyJsonFile(s_path_bak)) return false;

        if (LittleFS.exists(s_path_cfg)) (void)LittleFS.remove(s_path_cfg);
        return LittleFS.rename(s_path_bak, s_path_cfg);
    }

    // export: returns JSON string
    bool exportJson(String& p_out) {
        File v_f = LittleFS.open(s_path_cfg, "r");
        if (!v_f) return false;
        p_out = v_f.readString();
        v_f.close();
        return (p_out.length() > 0);
    }

    // import: validate JSON then save
    bool importJson(const String& p_json, bool& p_saved, bool& p_applied) {
        p_saved = false;
        p_applied = false;

        // validate json
        {
            JsonDocument v_doc;
            DeserializationError v_err = deserializeJson(v_doc, p_json);
            if (v_err) return false;
        }

        // load current -> defaults -> patch -> saveAll
        ST_C10_WiFiConfig_t v_w;
        ST_C10_E10Config_t  v_e;
        makeDefaultsWiFi(v_w);
        makeDefaultsE10(v_e);

        // if current exists, use it as base
        (void)loadAll(v_w, v_e);

        (void)patchFromJsonWiFi(p_json, v_w);
        (void)patchFromJsonE10(p_json, v_e);

        p_saved = saveAll(v_w, v_e);
        return p_saved;
    }

    // -------------------------
    // patchFromJson (부분 패치)
    // -------------------------
    bool patchFromJsonWiFi(const String& p_json, ST_C10_WiFiConfig_t& p_wifi) {
        JsonDocument v_doc;
        DeserializationError v_err = deserializeJson(v_doc, p_json);
        if (v_err) return false;

        JsonVariant v_wifi = v_doc["wifi"];
        if (v_wifi.isNull()) return true;

        if (!v_wifi["mode"].isNull()) p_wifi.mode = (uint8_t)v_wifi["mode"];

        JsonVariant v_sta = v_wifi["sta"];
        if (!v_sta.isNull()) {
            if (!v_sta["ssid"].isNull()) strlcpy(p_wifi.sta_ssid, (const char*)v_sta["ssid"], sizeof(p_wifi.sta_ssid));
            if (!v_sta["pass"].isNull()) strlcpy(p_wifi.sta_pass, (const char*)v_sta["pass"], sizeof(p_wifi.sta_pass));
        }

        JsonVariant v_ap = v_wifi["ap"];
        if (!v_ap.isNull()) {
            if (!v_ap["ssid"].isNull()) strlcpy(p_wifi.ap_ssid, (const char*)v_ap["ssid"], sizeof(p_wifi.ap_ssid));
            if (!v_ap["pass"].isNull()) strlcpy(p_wifi.ap_pass, (const char*)v_ap["pass"], sizeof(p_wifi.ap_pass));
        }

        JsonVariant v_mdns = v_wifi["mdns"];
        if (!v_mdns.isNull()) {
            if (!v_mdns["host"].isNull()) strlcpy(p_wifi.mdns_host, (const char*)v_mdns["host"], sizeof(p_wifi.mdns_host));
        }

        return true;
    }

    bool patchFromJsonE10(const String& p_json, ST_C10_E10Config_t& p_e10) {
        JsonDocument v_doc;
        DeserializationError v_err = deserializeJson(v_doc, p_json);
        if (v_err) return false;

        JsonVariant v_e10 = v_doc["e10"];
        if (v_e10.isNull()) return true;

        if (!v_e10["dpi_level"].isNull()) p_e10.dpi_level = (uint8_t)v_e10["dpi_level"];
        if (!v_e10["hard_click_lock"].isNull()) p_e10.hard_click_lock = (bool)v_e10["hard_click_lock"];

        // arrays
        JsonVariant v_sb = v_e10["scale_base"];
        if (v_sb.is<JsonArray>()) {
            JsonArray a = v_sb.as<JsonArray>();
            if (a.size() >= 3) {
                p_e10.scale_base[0] = (float)a[0];
                p_e10.scale_base[1] = (float)a[1];
                p_e10.scale_base[2] = (float)a[2];
            }
        }

        JsonVariant v_ag = v_e10["accel_gain"];
        if (v_ag.is<JsonArray>()) {
            JsonArray a = v_ag.as<JsonArray>();
            if (a.size() >= 3) {
                p_e10.accel_gain[0] = (float)a[0];
                p_e10.accel_gain[1] = (float)a[1];
                p_e10.accel_gain[2] = (float)a[2];
            }
        }

        if (!v_e10["accel_threshold"].isNull()) p_e10.accel_threshold = (float)v_e10["accel_threshold"];

        JsonVariant v_wh = v_e10["wheel"];
        if (!v_wh.isNull()) {
            if (!v_wh["threshold_deg"].isNull()) p_e10.wheel_threshold_deg = (float)v_wh["threshold_deg"];
            if (!v_wh["step_max"].isNull()) p_e10.wheel_step_max = (uint8_t)v_wh["step_max"];
        }

        JsonVariant v_g = v_e10["gesture"];
        if (!v_g.isNull()) {
            if (!v_g["flick_deg"].isNull()) p_e10.gesture_flick_deg = (float)v_g["flick_deg"];
            if (!v_g["cooldown_ms"].isNull()) p_e10.gesture_cooldown_ms = (uint16_t)v_g["cooldown_ms"];
        }

        if (!v_e10["scroll_cursor_damp"].isNull()) p_e10.scroll_cursor_damp = (float)v_e10["scroll_cursor_damp"];

        JsonVariant v_p = v_e10["precision"];
        if (!v_p.isNull()) {
            if (!v_p["enable"].isNull()) p_e10.precision_enable = (bool)v_p["enable"];
            if (!v_p["deadzone"].isNull()) p_e10.precision_deadzone = (float)v_p["deadzone"];
            if (!v_p["gain"].isNull()) p_e10.precision_gain = (float)v_p["gain"];
            if (!v_p["accel"].isNull()) p_e10.precision_accel = (float)v_p["accel"];
            if (!v_p["max_step"].isNull()) p_e10.precision_max_step = (uint8_t)v_p["max_step"];
            if (!v_p["smooth"].isNull()) p_e10.precision_smooth = (float)v_p["smooth"];
        }

        JsonVariant v_pk = v_e10["ppt_keys"];
        if (!v_pk.isNull()) {
            auto loadKey = [&](const char* p_name, ST_C10_PptKey_t& p_k){
                JsonVariant o = v_pk[p_name];
                if (o.isNull()) return;
                if (!o["mod"].isNull()) p_k.mod = (uint8_t)o["mod"];
                if (!o["key"].isNull()) p_k.key = (uint16_t)o["key"];
            };
            loadKey("start", p_e10.ppt_start);
            loadKey("exit",  p_e10.ppt_exit);
            loadKey("next",  p_e10.ppt_next);
            loadKey("prev",  p_e10.ppt_prev);
            loadKey("black", p_e10.ppt_black);
            loadKey("laser", p_e10.ppt_laser);
        }

        // sanitize
        if (p_e10.dpi_level < 1) p_e10.dpi_level = 1;
        if (p_e10.dpi_level > 3) p_e10.dpi_level = 3;

        if (p_e10.wheel_step_max < 1) p_e10.wheel_step_max = 1;
        if (p_e10.wheel_step_max > 12) p_e10.wheel_step_max = 12;

        if (p_e10.gesture_cooldown_ms < 150) p_e10.gesture_cooldown_ms = 150;
        if (p_e10.precision_max_step < 4) p_e10.precision_max_step = 4;
        if (p_e10.precision_max_step > 60) p_e10.precision_max_step = 60;

        if (p_e10.precision_smooth < 0.0f) p_e10.precision_smooth = 0.0f;
        if (p_e10.precision_smooth > 0.98f) p_e10.precision_smooth = 0.98f;

        return true;
    }

  private:
    void buildJson(const ST_C10_WiFiConfig_t& p_wifi, const ST_C10_E10Config_t& p_e10, JsonDocument& p_doc) {
        p_doc["ver"] = (uint16_t)G_C10_CFG_VER;

        JsonObject v_w = p_doc["wifi"].to<JsonObject>();
        v_w["mode"] = p_wifi.mode;

        JsonObject v_sta = v_w["sta"].to<JsonObject>();
        v_sta["ssid"] = p_wifi.sta_ssid;
        v_sta["pass"] = p_wifi.sta_pass;

        JsonObject v_ap = v_w["ap"].to<JsonObject>();
        v_ap["ssid"] = p_wifi.ap_ssid;
        v_ap["pass"] = p_wifi.ap_pass;

        JsonObject v_mdns = v_w["mdns"].to<JsonObject>();
        v_mdns["host"] = p_wifi.mdns_host;

        JsonObject v_e = p_doc["e10"].to<JsonObject>();
        v_e["dpi_level"] = p_e10.dpi_level;
        v_e["hard_click_lock"] = p_e10.hard_click_lock;

        JsonArray v_sb = v_e["scale_base"].to<JsonArray>();
        v_sb.add(p_e10.scale_base[0]); v_sb.add(p_e10.scale_base[1]); v_sb.add(p_e10.scale_base[2]);

        JsonArray v_ag = v_e["accel_gain"].to<JsonArray>();
        v_ag.add(p_e10.accel_gain[0]); v_ag.add(p_e10.accel_gain[1]); v_ag.add(p_e10.accel_gain[2]);

        v_e["accel_threshold"] = p_e10.accel_threshold;

        JsonObject v_wh = v_e["wheel"].to<JsonObject>();
        v_wh["threshold_deg"] = p_e10.wheel_threshold_deg;
        v_wh["step_max"] = p_e10.wheel_step_max;

        JsonObject v_g = v_e["gesture"].to<JsonObject>();
        v_g["flick_deg"] = p_e10.gesture_flick_deg;
        v_g["cooldown_ms"] = p_e10.gesture_cooldown_ms;

        v_e["scroll_cursor_damp"] = p_e10.scroll_cursor_damp;

        JsonObject v_p = v_e["precision"].to<JsonObject>();
        v_p["enable"] = p_e10.precision_enable;
        v_p["deadzone"] = p_e10.precision_deadzone;
        v_p["gain"] = p_e10.precision_gain;
        v_p["accel"] = p_e10.precision_accel;
        v_p["max_step"] = p_e10.precision_max_step;
        v_p["smooth"] = p_e10.precision_smooth;

        JsonObject v_pk = v_e["ppt_keys"].to<JsonObject>();
        auto putKey = [&](const char* p_name, const ST_C10_PptKey_t& p_k){
            JsonObject o = v_pk[p_name].to<JsonObject>();
            o["mod"] = p_k.mod;
            o["key"] = p_k.key;
        };
        putKey("start", p_e10.ppt_start);
        putKey("exit",  p_e10.ppt_exit);
        putKey("next",  p_e10.ppt_next);
        putKey("prev",  p_e10.ppt_prev);
        putKey("black", p_e10.ppt_black);
        putKey("laser", p_e10.ppt_laser);
    }

    bool verifyJsonFile(const char* p_path) {
        File v_f = LittleFS.open(p_path, "r");
        if (!v_f) return false;
        JsonDocument v_doc;
        DeserializationError v_err = deserializeJson(v_doc, v_f);
        v_f.close();
        return (!v_err);
    }

    bool copyFile(const char* p_src, const char* p_dst) {
        File s = LittleFS.open(p_src, "r");
        if (!s) return false;
        File d = LittleFS.open(p_dst, "w");
        if (!d) { s.close(); return false; }

        uint8_t v_buf[256];
        while (true) {
            int v_n = s.read(v_buf, sizeof(v_buf));
            if (v_n <= 0) break;
            if (d.write(v_buf, (size_t)v_n) != (size_t)v_n) { s.close(); d.close(); return false; }
        }
        d.flush();
        s.close();
        d.close();
        return true;
    }
};


