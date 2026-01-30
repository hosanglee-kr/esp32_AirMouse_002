// ======================================================
// File: src/v001/C10_Config_010.h
// ======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : C10_Config_010.h
 * 모듈약어 : C10
 * 모듈명 : LittleFS 기반 JSON 설정 로드/세이브 (E10 파라미터 관리)
 * ------------------------------------------------------
 * 기능 요약
 *  - LittleFS에 /config.json 저장/로드
 *  - 파일 없으면 기본값 생성
 *  - 저장 시 기존 파일을 .bak로 백업 후 갱신(안전 저장)
 *  - ArduinoJson v7 기반(단일 JsonDocument), E10 런타임 튜닝 파라미터 제공
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

static constexpr const char* G_C10_CFG_PATH     = "/config.json";
static constexpr const char* G_C10_CFG_PATH_BAK = "/config.bak";
static constexpr const char* G_C10_CFG_PATH_TMP = "/config.tmp";

typedef struct ST_C10_E10Config_t {
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

    // wheel.threshold_deg, wheel.step_max
    float   wheel_threshold_deg;
    int16_t wheel_step_max;

    // gesture.flick_deg, gesture.cooldown_ms
    float    gesture_flick_deg;
    uint16_t gesture_cooldown_ms;

    // scroll_cursor_damp
    float scroll_cursor_damp;
} ST_C10_E10Config_t;

class CL_C10_Config {
  private:
    bool _mounted = false;

    void setDefaults(ST_C10_E10Config_t& p_cfg) {
        memset(&p_cfg, 0, sizeof(p_cfg));

        // dpi_level
        p_cfg.dpi_level = 2;

        // hard_click_lock
        p_cfg.hard_click_lock = true;

        // scale_base
        p_cfg.scale_base[0] = 0.55f;
        p_cfg.scale_base[1] = 0.75f;
        p_cfg.scale_base[2] = 1.00f;

        // accel_gain
        p_cfg.accel_gain[0] = 0.35f;
        p_cfg.accel_gain[1] = 0.55f;
        p_cfg.accel_gain[2] = 0.85f;

        // accel_threshold
        p_cfg.accel_threshold = 8.0f;

        // wheel
        p_cfg.wheel_threshold_deg = 90.0f;
        p_cfg.wheel_step_max      = 6;

        // gesture
        p_cfg.gesture_flick_deg    = 200.0f;
        p_cfg.gesture_cooldown_ms  = 600;

        // scroll_cursor_damp
        p_cfg.scroll_cursor_damp = 0.25f;
    }

    bool safeWriteFile(const char* p_path, const uint8_t* p_data, size_t p_len) {
        File v_f = LittleFS.open(p_path, "w");
        if (!v_f) return false;
        const size_t v_w = v_f.write(p_data, p_len);
        v_f.close();
        return (v_w == p_len);
    }

  public:
    CL_C10_Config() {}

    bool begin(bool p_formatOnFail = true) {
        if (_mounted) return true;

        if (!LittleFS.begin(p_formatOnFail)) {
            _mounted = false;
            return false;
        }
        _mounted = true;
        return true;
    }

    bool loadE10(ST_C10_E10Config_t& p_out) {
        if (!_mounted) {
            if (!begin(true)) return false;
        }

        if (!LittleFS.exists(G_C10_CFG_PATH)) {
            // 파일이 없으면 기본값 생성
            setDefaults(p_out);
            (void)saveE10(p_out);
            return true;
        }

        File v_f = LittleFS.open(G_C10_CFG_PATH, "r");
        if (!v_f) {
            setDefaults(p_out);
            return false;
        }

        // 단일 JsonDocument
        JsonDocument v_doc;
        DeserializationError v_err = deserializeJson(v_doc, v_f);
        v_f.close();

        if (v_err) {
            // 파싱 실패: 기본값으로 복구 + 덮어쓰기
            setDefaults(p_out);
            (void)saveE10(p_out);
            return false;
        }

        // dpi_level
        p_out.dpi_level = (uint8_t)(v_doc["dpi_level"] | 2);

        // hard_click_lock
        p_out.hard_click_lock = (bool)(v_doc["hard_click_lock"] | true);

        // scale_base
        // (배열이 없거나 길이가 짧아도 default 유지)
        p_out.scale_base[0] = (float)(v_doc["scale_base"][0] | 0.55f);
        p_out.scale_base[1] = (float)(v_doc["scale_base"][1] | 0.75f);
        p_out.scale_base[2] = (float)(v_doc["scale_base"][2] | 1.00f);

        // accel_gain
        p_out.accel_gain[0] = (float)(v_doc["accel_gain"][0] | 0.35f);
        p_out.accel_gain[1] = (float)(v_doc["accel_gain"][1] | 0.55f);
        p_out.accel_gain[2] = (float)(v_doc["accel_gain"][2] | 0.85f);

        // accel_threshold
        p_out.accel_threshold = (float)(v_doc["accel_threshold"] | 8.0f);

        // wheel
        p_out.wheel_threshold_deg = (float)(v_doc["wheel"]["threshold_deg"] | 90.0f);
        p_out.wheel_step_max      = (int16_t)(v_doc["wheel"]["step_max"] | 6);

        // gesture
        p_out.gesture_flick_deg   = (float)(v_doc["gesture"]["flick_deg"] | 200.0f);
        p_out.gesture_cooldown_ms = (uint16_t)(v_doc["gesture"]["cooldown_ms"] | 600);

        // scroll_cursor_damp
        p_out.scroll_cursor_damp = (float)(v_doc["scroll_cursor_damp"] | 0.25f);

        // 값 범위 간단 보호
        if (p_out.dpi_level < 1) p_out.dpi_level = 1;
        if (p_out.dpi_level > 3) p_out.dpi_level = 3;
        if (p_out.wheel_step_max < 1) p_out.wheel_step_max = 1;
        if (p_out.wheel_step_max > 20) p_out.wheel_step_max = 20;
        if (p_out.gesture_cooldown_ms < 100) p_out.gesture_cooldown_ms = 100;
        if (p_out.gesture_cooldown_ms > 2000) p_out.gesture_cooldown_ms = 2000;

        return true;
    }

    bool saveE10(const ST_C10_E10Config_t& p_cfg) {
        if (!_mounted) {
            if (!begin(true)) return false;
        }

        // 단일 JsonDocument
        JsonDocument v_doc;

        // dpi_level
        v_doc["dpi_level"] = p_cfg.dpi_level;

        // hard_click_lock
        v_doc["hard_click_lock"] = p_cfg.hard_click_lock;

        // scale_base
        {
            JsonArray v_arr = v_doc["scale_base"].to<JsonArray>();
            v_arr.add(p_cfg.scale_base[0]);
            v_arr.add(p_cfg.scale_base[1]);
            v_arr.add(p_cfg.scale_base[2]);
        }

        // accel_gain
        {
            JsonArray v_arr = v_doc["accel_gain"].to<JsonArray>();
            v_arr.add(p_cfg.accel_gain[0]);
            v_arr.add(p_cfg.accel_gain[1]);
            v_arr.add(p_cfg.accel_gain[2]);
        }

        // accel_threshold
        v_doc["accel_threshold"] = p_cfg.accel_threshold;

        // wheel
        {
            JsonObject v_wheel = v_doc["wheel"].to<JsonObject>();
            v_wheel["threshold_deg"] = p_cfg.wheel_threshold_deg;
            v_wheel["step_max"]      = p_cfg.wheel_step_max;
        }

        // gesture
        {
            JsonObject v_g = v_doc["gesture"].to<JsonObject>();
            v_g["flick_deg"]    = p_cfg.gesture_flick_deg;
            v_g["cooldown_ms"]  = p_cfg.gesture_cooldown_ms;
        }

        // scroll_cursor_damp
        v_doc["scroll_cursor_damp"] = p_cfg.scroll_cursor_damp;

        // 직렬화
        String v_out;
        serializeJson(v_doc, v_out);

        // 1) tmp로 저장
        if (!safeWriteFile(G_C10_CFG_PATH_TMP, (const uint8_t*)v_out.c_str(), v_out.length())) {
            return false;
        }

        // 2) 기존 config.json -> config.bak
        if (LittleFS.exists(G_C10_CFG_PATH_BAK)) {
            LittleFS.remove(G_C10_CFG_PATH_BAK);
        }
        if (LittleFS.exists(G_C10_CFG_PATH)) {
            LittleFS.rename(G_C10_CFG_PATH, G_C10_CFG_PATH_BAK);
        }

        // 3) tmp -> config.json
        if (!LittleFS.rename(G_C10_CFG_PATH_TMP, G_C10_CFG_PATH)) {
            // 실패 시 tmp 제거(가능하면)
            if (LittleFS.exists(G_C10_CFG_PATH_TMP)) LittleFS.remove(G_C10_CFG_PATH_TMP);
            return false;
        }

        return true;
    }

    bool resetE10ToDefaults() {
        ST_C10_E10Config_t v_cfg;
        setDefaults(v_cfg);
        return saveE10(v_cfg);
    }
};
