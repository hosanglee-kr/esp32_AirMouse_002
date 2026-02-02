#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : C10_Config_021.h
 * 모듈약어 : C10
 * 모듈명 : Config Manager (LittleFS JSON, Backup/Rollback/Export/Import)
 * ------------------------------------------------------
 * 기능 요약
 *  - LittleFS 기반 config.json 로드/저장/기본값 생성
 *  - 저장 시 .bak 자동 백업(원본 존재 시)
 *  - /api/export: 현재 config.json 원문 export
 *  - /api/import: JSON 원문 import + 검증 + 저장(+백업)
 *  - rollbackFromBak(): .bak로 롤백
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

static constexpr uint16_t C10_CFG_VER = 21;

enum EN_C10_WiFiMode : uint8_t {
    EN_C10_WIFI_AUTO = 0,
    EN_C10_WIFI_AP   = 1,
    EN_C10_WIFI_STA  = 2
};

struct ST_C10_PptKey_t {
    uint8_t mod;
    uint8_t key;
};

struct ST_C10_WiFiConfig_t {
    uint8_t mode;
    char ap_ssid[33];
    char ap_pass[65];
    char sta_ssid[33];
    char sta_pass[65];
    char mdns_host[33];
};

struct ST_C10_E10Config_t {
    uint8_t dpi_level;
    bool    hard_click_lock;

    float scale_base[3];
    float accel_gain[3];
    float accel_threshold;

    float wheel_threshold_deg;
    uint8_t wheel_step_max;

    float gesture_flick_deg;
    uint16_t gesture_cooldown_ms;

    float scroll_cursor_damp;

    // drift / idle tuning
    float idle_gyro_th_deg;
    uint16_t idle_hold_ms;
    float bias_track_alpha;
    float zero_snap_th;

    // precision (joystick precision mode concept)
    bool precision_enable;
    float precision_scale;
    uint16_t precision_hold_ms;
    uint16_t precision_cooldown_ms;

    // ppt keys
    ST_C10_PptKey_t ppt_start;
    ST_C10_PptKey_t ppt_exit;
    ST_C10_PptKey_t ppt_next;
    ST_C10_PptKey_t ppt_prev;
    ST_C10_PptKey_t ppt_black;
    ST_C10_PptKey_t ppt_laser;
};

class CL_C10_Config {
  private:
    static constexpr const char* s_cfgPath = "/json/config.json";
    static constexpr const char* s_bakPath = "/json/config.bak";

    bool _mounted = false;

    bool readFileToString(const char* p_path, String& p_out) {
        File v_f = LittleFS.open(p_path, "r");
        if (!v_f) return false;

        p_out = "";
        p_out.reserve((size_t)v_f.size() + 8);
        while (v_f.available()) p_out += (char)v_f.read();
        v_f.close();
        return true;
    }

    bool writeStringToFile(const char* p_path, const String& p_in) {
        File v_f = LittleFS.open(p_path, "w");
        if (!v_f) return false;
        v_f.print(p_in);
        v_f.close();
        return true;
    }

    bool copyFile(const char* p_src, const char* p_dst) {
        File v_in = LittleFS.open(p_src, "r");
        if (!v_in) return false;
        File v_out = LittleFS.open(p_dst, "w");
        if (!v_out) { v_in.close(); return false; }

        uint8_t v_buf[256];
        while (v_in.available()) {
            size_t v_n = v_in.read(v_buf, sizeof(v_buf));
            if (v_n > 0) v_out.write(v_buf, v_n);
        }
        v_out.close();
        v_in.close();
        return true;
    }

  public:
    CL_C10_Config() {}

    void begin(bool p_formatOnFail) {
        _mounted = LittleFS.begin(p_formatOnFail);
        if (!_mounted) {
            Serial.println("[C10] LittleFS mount failed");
            return;
        }
        (void)LittleFS.open(s_cfgPath, "a").close();
    }

    bool isMounted() const { return _mounted; }

    // -------- defaults --------
    void makeDefaultsWiFi(ST_C10_WiFiConfig_t& p_out) {
        memset(&p_out, 0, sizeof(p_out));
        p_out.mode = (uint8_t)EN_C10_WIFI_AUTO;

        strlcpy(p_out.ap_ssid, "EliteAirMouseS3", sizeof(p_out.ap_ssid));
        strlcpy(p_out.ap_pass, "12345678", sizeof(p_out.ap_pass));

        p_out.sta_ssid[0] = '\0';
        p_out.sta_pass[0] = '\0';

        strlcpy(p_out.mdns_host, "elite-airmouse", sizeof(p_out.mdns_host));
    }

    void makeDefaultsE10(ST_C10_E10Config_t& p_out) {
        memset(&p_out, 0, sizeof(p_out));

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

        p_out.idle_gyro_th_deg = 2.0f;
        p_out.idle_hold_ms = 1200;
        p_out.bias_track_alpha = 0.008f;
        p_out.zero_snap_th = 0.6f;

        // precision
        p_out.precision_enable = false;
        p_out.precision_scale = 0.35f;
        p_out.precision_hold_ms = 450;
        p_out.precision_cooldown_ms = 350;

        // defaults keymap
        p_out.ppt_start.mod = 0x02; p_out.ppt_start.key = 0x3E; // LShift + F5
        p_out.ppt_exit.mod  = 0x00; p_out.ppt_exit.key  = 0x29; // Esc
        p_out.ppt_next.mod  = 0x00; p_out.ppt_next.key  = 0x4E; // PageDown
        p_out.ppt_prev.mod  = 0x00; p_out.ppt_prev.key  = 0x4B; // PageUp
        p_out.ppt_black.mod = 0x00; p_out.ppt_black.key = 0x05; // B
        p_out.ppt_laser.mod = 0x01; p_out.ppt_laser.key = 0x0F; // Ctrl+L
    }

    // -------- backup/rollback --------
    bool backupNow() {
        if (!_mounted) return false;
        if (!LittleFS.exists(s_cfgPath)) return false;
        return copyFile(s_cfgPath, s_bakPath);
    }

    bool rollbackFromBak() {
        if (!_mounted) return false;
        if (!LittleFS.exists(s_bakPath)) return false;
        return copyFile(s_bakPath, s_cfgPath);
    }

    bool hasBackup() const {
        if (!_mounted) return false;
        return LittleFS.exists(s_bakPath);
    }

    // -------- export/import raw --------
    bool exportRaw(String& p_outJson) {
        if (!_mounted) return false;
        return readFileToString(s_cfgPath, p_outJson);
    }

    // import raw JSON -> validate parse -> save (with backup)
    bool importRaw(const String& p_inJson, bool p_makeBackup) {
        if (!_mounted) return false;

        // validate JSON parse first (do not accept garbage)
        {
            JsonDocument v_doc;
            DeserializationError v_err = deserializeJson(v_doc, p_inJson);
            if (v_err) return false;
        }

        if (p_makeBackup) (void)backupNow();
        return writeStringToFile(s_cfgPath, p_inJson);
    }

    // -------- load/save structured --------
    bool loadAll(ST_C10_WiFiConfig_t& p_wifi, ST_C10_E10Config_t& p_e10) {
        makeDefaultsWiFi(p_wifi);
        makeDefaultsE10(p_e10);

        String v_json;
        if (!readFileToString(s_cfgPath, v_json)) return false;

        JsonDocument v_doc;
        DeserializationError v_err = deserializeJson(v_doc, v_json);
        if (v_err) return false;

        // wifi
        if (!v_doc["wifi"].isNull()) {
            JsonVariant v_w = v_doc["wifi"];
            if (!v_w["mode"].isNull()) p_wifi.mode = (uint8_t)v_w["mode"];

            if (!v_w["ap"].isNull()) {
                JsonVariant v_ap = v_w["ap"];
                if (!v_ap["ssid"].isNull()) strlcpy(p_wifi.ap_ssid, (const char*)v_ap["ssid"], sizeof(p_wifi.ap_ssid));
                if (!v_ap["pass"].isNull()) strlcpy(p_wifi.ap_pass, (const char*)v_ap["pass"], sizeof(p_wifi.ap_pass));
            }
            if (!v_w["sta"].isNull()) {
                JsonVariant v_sta = v_w["sta"];
                if (!v_sta["ssid"].isNull()) strlcpy(p_wifi.sta_ssid, (const char*)v_sta["ssid"], sizeof(p_wifi.sta_ssid));
                if (!v_sta["pass"].isNull()) strlcpy(p_wifi.sta_pass, (const char*)v_sta["pass"], sizeof(p_wifi.sta_pass));
            }
            if (!v_w["mdns"].isNull()) {
                JsonVariant v_md = v_w["mdns"];
                if (!v_md["host"].isNull()) strlcpy(p_wifi.mdns_host, (const char*)v_md["host"], sizeof(p_wifi.mdns_host));
            }
        }

        // e10
        if (!v_doc["e10"].isNull()) {
            JsonVariant v_e = v_doc["e10"];
            if (!v_e["dpi_level"].isNull()) p_e10.dpi_level = (uint8_t)v_e["dpi_level"];
            if (!v_e["hard_click_lock"].isNull()) p_e10.hard_click_lock = (bool)v_e["hard_click_lock"];

            if (!v_e["scale_base"].isNull() && v_e["scale_base"].is<JsonArray>()) {
                JsonArray a = v_e["scale_base"].as<JsonArray>();
                uint8_t i = 0; for (JsonVariant x : a) { if (i < 3) p_e10.scale_base[i++] = (float)x; }
            }
            if (!v_e["accel_gain"].isNull() && v_e["accel_gain"].is<JsonArray>()) {
                JsonArray a = v_e["accel_gain"].as<JsonArray>();
                uint8_t i = 0; for (JsonVariant x : a) { if (i < 3) p_e10.accel_gain[i++] = (float)x; }
            }
            if (!v_e["accel_threshold"].isNull()) p_e10.accel_threshold = (float)v_e["accel_threshold"];

            if (!v_e["wheel"].isNull()) {
                JsonVariant v_w = v_e["wheel"];
                if (!v_w["threshold_deg"].isNull()) p_e10.wheel_threshold_deg = (float)v_w["threshold_deg"];
                if (!v_w["step_max"].isNull()) p_e10.wheel_step_max = (uint8_t)v_w["step_max"];
            }
            if (!v_e["gesture"].isNull()) {
                JsonVariant v_g = v_e["gesture"];
                if (!v_g["flick_deg"].isNull()) p_e10.gesture_flick_deg = (float)v_g["flick_deg"];
                if (!v_g["cooldown_ms"].isNull()) p_e10.gesture_cooldown_ms = (uint16_t)v_g["cooldown_ms"];
            }
            if (!v_e["scroll_cursor_damp"].isNull()) p_e10.scroll_cursor_damp = (float)v_e["scroll_cursor_damp"];

            if (!v_e["drift"].isNull()) {
                JsonVariant v_d = v_e["drift"];
                if (!v_d["idle_gyro_th_deg"].isNull()) p_e10.idle_gyro_th_deg = (float)v_d["idle_gyro_th_deg"];
                if (!v_d["idle_hold_ms"].isNull()) p_e10.idle_hold_ms = (uint16_t)v_d["idle_hold_ms"];
                if (!v_d["bias_track_alpha"].isNull()) p_e10.bias_track_alpha = (float)v_d["bias_track_alpha"];
                if (!v_d["zero_snap_th"].isNull()) p_e10.zero_snap_th = (float)v_d["zero_snap_th"];
            }

            if (!v_e["precision"].isNull()) {
                JsonVariant v_p = v_e["precision"];
                if (!v_p["enable"].isNull()) p_e10.precision_enable = (bool)v_p["enable"];
                if (!v_p["scale"].isNull()) p_e10.precision_scale = (float)v_p["scale"];
                if (!v_p["hold_ms"].isNull()) p_e10.precision_hold_ms = (uint16_t)v_p["hold_ms"];
                if (!v_p["cooldown_ms"].isNull()) p_e10.precision_cooldown_ms = (uint16_t)v_p["cooldown_ms"];
            }

            if (!v_e["ppt_keys"].isNull()) {
                JsonVariant v_pk = v_e["ppt_keys"];
                auto rd = [&](const char* n, ST_C10_PptKey_t& k) {
                    if (v_pk[n].isNull()) return;
                    JsonVariant o = v_pk[n];
                    if (!o["mod"].isNull()) k.mod = (uint8_t)o["mod"];
                    if (!o["key"].isNull()) k.key = (uint8_t)o["key"];
                };
                rd("start", p_e10.ppt_start);
                rd("exit",  p_e10.ppt_exit);
                rd("next",  p_e10.ppt_next);
                rd("prev",  p_e10.ppt_prev);
                rd("black", p_e10.ppt_black);
                rd("laser", p_e10.ppt_laser);
            }
        }

        // sanitize
        if (p_e10.dpi_level < 1) p_e10.dpi_level = 1;
        if (p_e10.dpi_level > 3) p_e10.dpi_level = 3;
        if (p_e10.wheel_step_max < 1) p_e10.wheel_step_max = 1;
        if (p_e10.wheel_step_max > 32) p_e10.wheel_step_max = 32;

        if (p_e10.precision_scale < 0.05f) p_e10.precision_scale = 0.05f;
        if (p_e10.precision_scale > 1.0f) p_e10.precision_scale = 1.0f;

        return true;
    }

    bool saveAll(const ST_C10_WiFiConfig_t& p_wifi, const ST_C10_E10Config_t& p_e10) {
        JsonDocument v_doc;
        v_doc["ver"] = (uint16_t)C10_CFG_VER;

        JsonObject w = v_doc["wifi"].to<JsonObject>();
        w["mode"] = p_wifi.mode;

        JsonObject w_sta = w["sta"].to<JsonObject>();
        w_sta["ssid"] = p_wifi.sta_ssid;
        w_sta["pass"] = p_wifi.sta_pass;

        JsonObject w_ap = w["ap"].to<JsonObject>();
        w_ap["ssid"] = p_wifi.ap_ssid;
        w_ap["pass"] = p_wifi.ap_pass;

        JsonObject w_md = w["mdns"].to<JsonObject>();
        w_md["host"] = p_wifi.mdns_host;

        JsonObject e = v_doc["e10"].to<JsonObject>();
        e["dpi_level"] = p_e10.dpi_level;
        e["hard_click_lock"] = p_e10.hard_click_lock;

        JsonArray sb = e["scale_base"].to<JsonArray>();
        sb.add(p_e10.scale_base[0]); sb.add(p_e10.scale_base[1]); sb.add(p_e10.scale_base[2]);

        JsonArray ag = e["accel_gain"].to<JsonArray>();
        ag.add(p_e10.accel_gain[0]); ag.add(p_e10.accel_gain[1]); ag.add(p_e10.accel_gain[2]);

        e["accel_threshold"] = p_e10.accel_threshold;

        JsonObject wh = e["wheel"].to<JsonObject>();
        wh["threshold_deg"] = p_e10.wheel_threshold_deg;
        wh["step_max"] = p_e10.wheel_step_max;

        JsonObject g = e["gesture"].to<JsonObject>();
        g["flick_deg"] = p_e10.gesture_flick_deg;
        g["cooldown_ms"] = p_e10.gesture_cooldown_ms;

        e["scroll_cursor_damp"] = p_e10.scroll_cursor_damp;

        JsonObject d = e["drift"].to<JsonObject>();
        d["idle_gyro_th_deg"] = p_e10.idle_gyro_th_deg;
        d["idle_hold_ms"] = p_e10.idle_hold_ms;
        d["bias_track_alpha"] = p_e10.bias_track_alpha;
        d["zero_snap_th"] = p_e10.zero_snap_th;

        JsonObject p = e["precision"].to<JsonObject>();
        p["enable"] = p_e10.precision_enable;
        p["scale"] = p_e10.precision_scale;
        p["hold_ms"] = p_e10.precision_hold_ms;
        p["cooldown_ms"] = p_e10.precision_cooldown_ms;

        JsonObject pk = e["ppt_keys"].to<JsonObject>();
        auto wr = [&](const char* n, const ST_C10_PptKey_t& k) {
            JsonObject o = pk[n].to<JsonObject>();
            o["mod"] = k.mod;
            o["key"] = k.key;
        };
        wr("start", p_e10.ppt_start);
        wr("exit",  p_e10.ppt_exit);
        wr("next",  p_e10.ppt_next);
        wr("prev",  p_e10.ppt_prev);
        wr("black", p_e10.ppt_black);
        wr("laser", p_e10.ppt_laser);

        String v_out;
        serializeJson(v_doc, v_out);

        // ✅ save 정책: 기존 config가 있으면 .bak 생성 후 덮어쓰기
        if (LittleFS.exists(s_cfgPath)) (void)backupNow();
        return writeStringToFile(s_cfgPath, v_out);
    }
};

