#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : C10_Config_015.h
 * 모듈약어 : C10
 * 모듈명 : Config Manager (LittleFS JSON, E10 Runtime + PPT Key Mapping)
 * ------------------------------------------------------
 * 기능 요약
 *  - LittleFS에 config.json 저장/로드
 *  - E10(에어마우스) 런타임 파라미터 + PPT 키매핑(ppt_keys) 관리
 *  - defaults 생성/리셋 지원
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

struct ST_C10_PptKey_t {
    uint8_t mod; // 예: KEY_LEFTCTRL (0이면 없음)
    uint8_t key; // 예: KEY_PAGEDOWN
};

struct ST_C10_E10Config_t {
    // dpi_level
    uint8_t dpi_level;

    // hard_click_lock
    bool hard_click_lock;

    // scale_base[3]
    float scale_base[3];

    // accel_gain[3]
    float accel_gain[3];

    // accel_threshold
    float accel_threshold;

    // wheel.threshold_deg
    float wheel_threshold_deg;

    // wheel.step_max
    int16_t wheel_step_max;

    // gesture.flick_deg
    float gesture_flick_deg;

    // gesture.cooldown_ms
    uint16_t gesture_cooldown_ms;

    // scroll_cursor_damp
    float scroll_cursor_damp;

    // ppt_keys.start/exit/next/prev/black/laser
    ST_C10_PptKey_t ppt_start;
    ST_C10_PptKey_t ppt_exit;
    ST_C10_PptKey_t ppt_next;
    ST_C10_PptKey_t ppt_prev;
    ST_C10_PptKey_t ppt_black;
    ST_C10_PptKey_t ppt_laser;
};

class CL_C10_Config {
  private:
    static constexpr const char* G_C10_PATH_CFG = "/config.json";
    static constexpr const char* G_C10_PATH_BAK = "/config.bak";

    bool _fsReady = false;

    bool writeTextFile(const char* p_path, const String& p_text) {
        File v_f = LittleFS.open(p_path, "w");
        if (!v_f) return false;
        size_t v_w = v_f.print(p_text);
        v_f.close();
        return (v_w == (size_t)p_text.length());
    }

    bool readTextFile(const char* p_path, String& p_out) {
        File v_f = LittleFS.open(p_path, "r");
        if (!v_f) return false;
        p_out = v_f.readString();
        v_f.close();
        return true;
    }

    void clampE10(ST_C10_E10Config_t& p_cfg) {
        if (p_cfg.dpi_level < 1) p_cfg.dpi_level = 1;
        if (p_cfg.dpi_level > 3) p_cfg.dpi_level = 3;

        if (p_cfg.wheel_step_max < 1) p_cfg.wheel_step_max = 1;
        if (p_cfg.wheel_step_max > 20) p_cfg.wheel_step_max = 20;

        if (p_cfg.gesture_cooldown_ms < 100) p_cfg.gesture_cooldown_ms = 100;
        if (p_cfg.gesture_cooldown_ms > 2000) p_cfg.gesture_cooldown_ms = 2000;

        if (p_cfg.scroll_cursor_damp < 0.0f) p_cfg.scroll_cursor_damp = 0.0f;
        if (p_cfg.scroll_cursor_damp > 1.0f) p_cfg.scroll_cursor_damp = 1.0f;
    }

  public:
    CL_C10_Config() {}

    bool begin(bool p_formatOnFail) {
        if (_fsReady) return true;
        _fsReady = LittleFS.begin(p_formatOnFail);
        return _fsReady;
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

        // ✅ ppt default mapping (KeyboardHIDCodes.h 숫자 값 그대로 사용)
        // start : Shift + F5
        p_out.ppt_start.mod = 225; // KEY_LEFTSHIFT
        p_out.ppt_start.key = 62;  // KEY_F5

        // exit : Esc
        p_out.ppt_exit.mod = 0;
        p_out.ppt_exit.key = 41;   // KEY_ESC

        // next : PageDown
        p_out.ppt_next.mod = 0;
        p_out.ppt_next.key = 78;   // KEY_PAGEDOWN

        // prev : PageUp
        p_out.ppt_prev.mod = 0;
        p_out.ppt_prev.key = 75;   // KEY_PAGEUP

        // black : 'b'
        p_out.ppt_black.mod = 0;
        p_out.ppt_black.key = 5;   // KEY_B

        // laser : Ctrl + L
        p_out.ppt_laser.mod = 224; // KEY_LEFTCTRL
        p_out.ppt_laser.key = 15;  // KEY_L
    }

    bool loadE10(ST_C10_E10Config_t& p_out) {
        if (!_fsReady) return false;

        String v_txt;
        if (!readTextFile(G_C10_PATH_CFG, v_txt)) {
            makeDefaultsE10(p_out);
            (void)saveE10(p_out);
            return true;
        }

        makeDefaultsE10(p_out); // defaults 먼저 채우고, 있는 값만 덮어씀

        JsonDocument v_doc;
        DeserializationError v_err = deserializeJson(v_doc, v_txt);
        if (v_err) {
            makeDefaultsE10(p_out);
            (void)saveE10(p_out);
            return true;
        }

        // ---- e10 core ----
        p_out.dpi_level = (uint8_t)(v_doc["dpi_level"] | p_out.dpi_level);
        p_out.hard_click_lock = (bool)(v_doc["hard_click_lock"] | p_out.hard_click_lock);

        p_out.scale_base[0] = (float)(v_doc["scale_base"][0] | p_out.scale_base[0]);
        p_out.scale_base[1] = (float)(v_doc["scale_base"][1] | p_out.scale_base[1]);
        p_out.scale_base[2] = (float)(v_doc["scale_base"][2] | p_out.scale_base[2]);

        p_out.accel_gain[0] = (float)(v_doc["accel_gain"][0] | p_out.accel_gain[0]);
        p_out.accel_gain[1] = (float)(v_doc["accel_gain"][1] | p_out.accel_gain[1]);
        p_out.accel_gain[2] = (float)(v_doc["accel_gain"][2] | p_out.accel_gain[2]);

        p_out.accel_threshold = (float)(v_doc["accel_threshold"] | p_out.accel_threshold);

        p_out.wheel_threshold_deg = (float)(v_doc["wheel"]["threshold_deg"] | p_out.wheel_threshold_deg);
        p_out.wheel_step_max      = (int16_t)(v_doc["wheel"]["step_max"] | p_out.wheel_step_max);

        p_out.gesture_flick_deg   = (float)(v_doc["gesture"]["flick_deg"] | p_out.gesture_flick_deg);
        p_out.gesture_cooldown_ms = (uint16_t)(v_doc["gesture"]["cooldown_ms"] | p_out.gesture_cooldown_ms);

        p_out.scroll_cursor_damp = (float)(v_doc["scroll_cursor_damp"] | p_out.scroll_cursor_damp);

        // ---- ppt_keys ----
        p_out.ppt_start.mod = (uint8_t)(v_doc["ppt_keys"]["start"]["mod"] | p_out.ppt_start.mod);
        p_out.ppt_start.key = (uint8_t)(v_doc["ppt_keys"]["start"]["key"] | p_out.ppt_start.key);

        p_out.ppt_exit.mod  = (uint8_t)(v_doc["ppt_keys"]["exit"]["mod"]  | p_out.ppt_exit.mod);
        p_out.ppt_exit.key  = (uint8_t)(v_doc["ppt_keys"]["exit"]["key"]  | p_out.ppt_exit.key);

        p_out.ppt_next.mod  = (uint8_t)(v_doc["ppt_keys"]["next"]["mod"]  | p_out.ppt_next.mod);
        p_out.ppt_next.key  = (uint8_t)(v_doc["ppt_keys"]["next"]["key"]  | p_out.ppt_next.key);

        p_out.ppt_prev.mod  = (uint8_t)(v_doc["ppt_keys"]["prev"]["mod"]  | p_out.ppt_prev.mod);
        p_out.ppt_prev.key  = (uint8_t)(v_doc["ppt_keys"]["prev"]["key"]  | p_out.ppt_prev.key);

        p_out.ppt_black.mod = (uint8_t)(v_doc["ppt_keys"]["black"]["mod"] | p_out.ppt_black.mod);
        p_out.ppt_black.key = (uint8_t)(v_doc["ppt_keys"]["black"]["key"] | p_out.ppt_black.key);

        p_out.ppt_laser.mod = (uint8_t)(v_doc["ppt_keys"]["laser"]["mod"] | p_out.ppt_laser.mod);
        p_out.ppt_laser.key = (uint8_t)(v_doc["ppt_keys"]["laser"]["key"] | p_out.ppt_laser.key);

        clampE10(p_out);
        return true;
    }

    bool saveE10(const ST_C10_E10Config_t& p_in) {
        if (!_fsReady) return false;

        // 백업
        if (LittleFS.exists(G_C10_PATH_CFG)) {
            String v_old;
            if (readTextFile(G_C10_PATH_CFG, v_old)) (void)writeTextFile(G_C10_PATH_BAK, v_old);
        }

        JsonDocument v_doc;

        v_doc["dpi_level"] = p_in.dpi_level;
        v_doc["hard_click_lock"] = p_in.hard_click_lock;

        JsonArray v_sb = v_doc["scale_base"].to<JsonArray>();
        v_sb.add(p_in.scale_base[0]);
        v_sb.add(p_in.scale_base[1]);
        v_sb.add(p_in.scale_base[2]);

        JsonArray v_ag = v_doc["accel_gain"].to<JsonArray>();
        v_ag.add(p_in.accel_gain[0]);
        v_ag.add(p_in.accel_gain[1]);
        v_ag.add(p_in.accel_gain[2]);

        v_doc["accel_threshold"] = p_in.accel_threshold;

        JsonObject v_w = v_doc["wheel"].to<JsonObject>();
        v_w["threshold_deg"] = p_in.wheel_threshold_deg;
        v_w["step_max"] = p_in.wheel_step_max;

        JsonObject v_g = v_doc["gesture"].to<JsonObject>();
        v_g["flick_deg"] = p_in.gesture_flick_deg;
        v_g["cooldown_ms"] = p_in.gesture_cooldown_ms;

        v_doc["scroll_cursor_damp"] = p_in.scroll_cursor_damp;

        JsonObject v_pk = v_doc["ppt_keys"].to<JsonObject>();
        JsonObject v_start = v_pk["start"].to<JsonObject>(); v_start["mod"] = p_in.ppt_start.mod; v_start["key"] = p_in.ppt_start.key;
        JsonObject v_exit  = v_pk["exit"].to<JsonObject>();  v_exit["mod"]  = p_in.ppt_exit.mod;  v_exit["key"]  = p_in.ppt_exit.key;
        JsonObject v_next  = v_pk["next"].to<JsonObject>();  v_next["mod"]  = p_in.ppt_next.mod;  v_next["key"]  = p_in.ppt_next.key;
        JsonObject v_prev  = v_pk["prev"].to<JsonObject>();  v_prev["mod"]  = p_in.ppt_prev.mod;  v_prev["key"]  = p_in.ppt_prev.key;
        JsonObject v_black = v_pk["black"].to<JsonObject>(); v_black["mod"] = p_in.ppt_black.mod; v_black["key"] = p_in.ppt_black.key;
        JsonObject v_laser = v_pk["laser"].to<JsonObject>(); v_laser["mod"] = p_in.ppt_laser.mod; v_laser["key"] = p_in.ppt_laser.key;

        String v_out;
        serializeJson(v_doc, v_out);
        return writeTextFile(G_C10_PATH_CFG, v_out);
    }
};

