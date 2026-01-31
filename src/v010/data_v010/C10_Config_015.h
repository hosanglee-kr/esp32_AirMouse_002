#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : C10_Config_015.h
 * 모듈약어 : C10
 * 모듈명 : Config Manager (LittleFS JSON, ver+migrate+bak)
 * ------------------------------------------------------
 * 기능 요약
 *  - LittleFS 기반 config.json 저장/로드
 *  - 버전(ver) 기반 마이그레이션: ver 불일치 시 defaults -> patchFromJson -> save
 *  - 저장 시 .bak 백업 생성(원본 보존)
 *  - E10(에어마우스) 튜닝 파라미터 구조체 제공
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
  #define C10_CFG_VER 11
#endif

// ------------------------------
// E10 Key struct (ppt_keys.*)
// ------------------------------
struct ST_C10_PptKey_t {
    uint8_t mod; // HID modifier bit mask
    uint8_t key; // HID keycode
};

// ------------------------------
// E10 Config struct
// ------------------------------
struct ST_C10_E10Config_t {
    // ver
    uint16_t ver;

    // basic
    uint8_t dpi_level;        // 1~3
    bool    hard_click_lock;  // click-lock hard mode

    // tuning arrays
    float scale_base[3];      // per dpi: 1..3
    float accel_gain[3];      // per dpi
    float accel_threshold;    // magnitude threshold

    // scroll
    float   wheel_threshold_deg;
    int16_t wheel_step_max;

    // gesture
    float    gesture_flick_deg;
    uint16_t gesture_cooldown_ms;

    // scroll cursor damp (0..1)
    float scroll_cursor_damp;

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

    void patchPpt(const JsonVariant& p_root, const char* p_name, ST_C10_PptKey_t& p_k) {
        p_k.mod = (uint8_t)(p_root[p_name]["mod"] | p_k.mod);
        p_k.key = (uint8_t)(p_root[p_name]["key"] | p_k.key);
    }

    void fillPpt(JsonObject p_root, const char* p_name, const ST_C10_PptKey_t& p_k) {
        JsonObject v_o = p_root[p_name].to<JsonObject>();
        v_o["mod"] = p_k.mod;
        v_o["key"] = p_k.key;
    }

  public:
    CL_C10_Config() {}

    bool begin(bool p_formatOnFail = true) {
        return ensureFs(p_formatOnFail);
    }

    // ----------------------------------------------------
    // defaults
    // ----------------------------------------------------
    void makeDefaultsE10(ST_C10_E10Config_t& p_out) {
        memset(&p_out, 0, sizeof(p_out));

        p_out.ver = (uint16_t)C10_CFG_VER;

        p_out.dpi_level = 2;
        p_out.hard_click_lock = true;

        // scale_base (E10 010 값 기반)
        p_out.scale_base[0] = 0.55f;
        p_out.scale_base[1] = 0.75f;
        p_out.scale_base[2] = 1.00f;

        // accel_gain
        p_out.accel_gain[0] = 0.35f;
        p_out.accel_gain[1] = 0.55f;
        p_out.accel_gain[2] = 0.85f;

        p_out.accel_threshold = 8.0f;

        // wheel
        p_out.wheel_threshold_deg = 90.0f;
        p_out.wheel_step_max = 6;

        // gesture
        p_out.gesture_flick_deg = 200.0f;
        p_out.gesture_cooldown_ms = 600;

        p_out.scroll_cursor_damp = 0.25f;

        // ppt keys (기본: 기존 E10 sendPPTCommand 기준)
        p_out.ppt_start = { 0x02, 0x00 }; // Shift + F5 -> (mod=LeftShift, key=F5) / key는 W10에서 덮어씀
        p_out.ppt_exit  = { 0x00, 0x00 }; // Esc
        p_out.ppt_next  = { 0x00, 0x00 }; // PageDown
        p_out.ppt_prev  = { 0x00, 0x00 }; // PageUp
        p_out.ppt_black = { 0x00, 0x00 }; // 'b'
        p_out.ppt_laser = { 0x01, 0x00 }; // Ctrl + L

        // 위 key들은 “정확한 KEY_* 값”을 모르므로,
        // W10 UI에서 선택한 값으로 저장되면 그게 진실이 됨.
        // (즉, 초기 기본값은 mod만 의미 있고 key는 0이라도 됨)
    }

    // ----------------------------------------------------
    // json -> struct (patch)
    // ----------------------------------------------------
    bool patchFromJsonE10(const String& p_json, ST_C10_E10Config_t& p_inOut) {
        JsonDocument v_doc;
        DeserializationError v_err = deserializeJson(v_doc, p_json);
        if (v_err) return false;

        p_inOut.dpi_level = (uint8_t)(v_doc["dpi_level"] | p_inOut.dpi_level);
        p_inOut.hard_click_lock = (bool)(v_doc["hard_click_lock"] | p_inOut.hard_click_lock);

        p_inOut.scale_base[0] = (float)(v_doc["scale_base"][0] | p_inOut.scale_base[0]);
        p_inOut.scale_base[1] = (float)(v_doc["scale_base"][1] | p_inOut.scale_base[1]);
        p_inOut.scale_base[2] = (float)(v_doc["scale_base"][2] | p_inOut.scale_base[2]);

        p_inOut.accel_gain[0] = (float)(v_doc["accel_gain"][0] | p_inOut.accel_gain[0]);
        p_inOut.accel_gain[1] = (float)(v_doc["accel_gain"][1] | p_inOut.accel_gain[1]);
        p_inOut.accel_gain[2] = (float)(v_doc["accel_gain"][2] | p_inOut.accel_gain[2]);

        p_inOut.accel_threshold = (float)(v_doc["accel_threshold"] | p_inOut.accel_threshold);

        p_inOut.wheel_threshold_deg = (float)(v_doc["wheel"]["threshold_deg"] | p_inOut.wheel_threshold_deg);
        p_inOut.wheel_step_max = (int16_t)(v_doc["wheel"]["step_max"] | p_inOut.wheel_step_max);

        p_inOut.gesture_flick_deg = (float)(v_doc["gesture"]["flick_deg"] | p_inOut.gesture_flick_deg);
        p_inOut.gesture_cooldown_ms = (uint16_t)(v_doc["gesture"]["cooldown_ms"] | p_inOut.gesture_cooldown_ms);

        p_inOut.scroll_cursor_damp = (float)(v_doc["scroll_cursor_damp"] | p_inOut.scroll_cursor_damp);

        JsonVariant v_pk = v_doc["ppt_keys"];
        patchPpt(v_pk, "start", p_inOut.ppt_start);
        patchPpt(v_pk, "exit",  p_inOut.ppt_exit);
        patchPpt(v_pk, "next",  p_inOut.ppt_next);
        patchPpt(v_pk, "prev",  p_inOut.ppt_prev);
        patchPpt(v_pk, "black", p_inOut.ppt_black);
        patchPpt(v_pk, "laser", p_inOut.ppt_laser);

        // clamp
        if (p_inOut.dpi_level < 1) p_inOut.dpi_level = 1;
        if (p_inOut.dpi_level > 3) p_inOut.dpi_level = 3;

        if (p_inOut.wheel_step_max < 1) p_inOut.wheel_step_max = 1;
        if (p_inOut.wheel_step_max > 20) p_inOut.wheel_step_max = 20;

        if (p_inOut.gesture_cooldown_ms < 100) p_inOut.gesture_cooldown_ms = 100;
        if (p_inOut.gesture_cooldown_ms > 2000) p_inOut.gesture_cooldown_ms = 2000;

        if (p_inOut.scroll_cursor_damp < 0.0f) p_inOut.scroll_cursor_damp = 0.0f;
        if (p_inOut.scroll_cursor_damp > 1.0f) p_inOut.scroll_cursor_damp = 1.0f;

        return true;
    }

    // ----------------------------------------------------
    // struct -> json
    // ----------------------------------------------------
    bool toJsonE10(const ST_C10_E10Config_t& p_cfg, String& p_outJson) {
        JsonDocument v_doc;

        v_doc["ver"] = p_cfg.ver;

        v_doc["dpi_level"] = p_cfg.dpi_level;
        v_doc["hard_click_lock"] = p_cfg.hard_click_lock;

        JsonArray v_sb = v_doc["scale_base"].to<JsonArray>();
        v_sb.add(p_cfg.scale_base[0]); v_sb.add(p_cfg.scale_base[1]); v_sb.add(p_cfg.scale_base[2]);

        JsonArray v_ag = v_doc["accel_gain"].to<JsonArray>();
        v_ag.add(p_cfg.accel_gain[0]); v_ag.add(p_cfg.accel_gain[1]); v_ag.add(p_cfg.accel_gain[2]);

        v_doc["accel_threshold"] = p_cfg.accel_threshold;

        JsonObject v_w = v_doc["wheel"].to<JsonObject>();
        v_w["threshold_deg"] = p_cfg.wheel_threshold_deg;
        v_w["step_max"] = p_cfg.wheel_step_max;

        JsonObject v_g = v_doc["gesture"].to<JsonObject>();
        v_g["flick_deg"] = p_cfg.gesture_flick_deg;
        v_g["cooldown_ms"] = p_cfg.gesture_cooldown_ms;

        v_doc["scroll_cursor_damp"] = p_cfg.scroll_cursor_damp;

        JsonObject v_pk = v_doc["ppt_keys"].to<JsonObject>();
        fillPpt(v_pk, "start", p_cfg.ppt_start);
        fillPpt(v_pk, "exit",  p_cfg.ppt_exit);
        fillPpt(v_pk, "next",  p_cfg.ppt_next);
        fillPpt(v_pk, "prev",  p_cfg.ppt_prev);
        fillPpt(v_pk, "black", p_cfg.ppt_black);
        fillPpt(v_pk, "laser", p_cfg.ppt_laser);

        p_outJson = "";
        serializeJson(v_doc, p_outJson);
        return true;
    }

    // ----------------------------------------------------
    // load/save (migrate + bak)
    // ----------------------------------------------------
    bool loadE10(ST_C10_E10Config_t& p_out) {
        makeDefaultsE10(p_out);

        if (!_fsReady) return false;
        if (!LittleFS.exists(C10_CFG_PATH)) return true; // defaults

        String v_json;
        if (!readFileToString(C10_CFG_PATH, v_json)) return false;

        // 버전 체크
        uint16_t v_fileVer = 0;
        {
            JsonDocument v_doc;
            if (!deserializeJson(v_doc, v_json)) {
                v_fileVer = (uint16_t)(v_doc["ver"] | 0);
            }
        }

        // patch
        (void)patchFromJsonE10(v_json, p_out);

        // migrate if mismatch
        if (v_fileVer != (uint16_t)C10_CFG_VER) {
            p_out.ver = (uint16_t)C10_CFG_VER;

            // save migrated (backup first)
            (void)saveE10(p_out);
        }

        return true;
    }

    bool saveE10(const ST_C10_E10Config_t& p_cfg) {
        if (!_fsReady) return false;

        // backup existing
        if (LittleFS.exists(C10_CFG_PATH)) {
            String v_old;
            if (readFileToString(C10_CFG_PATH, v_old)) {
                (void)writeStringToFile(C10_CFG_BAK_PATH, v_old);
            }
        }

        String v_json;
        ST_C10_E10Config_t v_tmp = p_cfg;
        v_tmp.ver = (uint16_t)C10_CFG_VER;
        (void)toJsonE10(v_tmp, v_json);

        return writeStringToFile(C10_CFG_PATH, v_json);
    }
};
