// =======================================================
// File: src/v0410/C10_Def_0410.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : C10_Def_0410.h
 * 모듈약어 : C10
 * 모듈명 : Config Definitions (v0410 3-Mode)
 * ------------------------------------------------------
 * 기능 요약
 *  - 스키마 버전 / 경로 상수
 *  - WiFi / Mode / Slot / E10 확장 config struct
 *  - Boot state struct (기존 유지)
 * ------------------------------------------------------
 * [구현 규칙]
 *  - ArduinoJson v7.x.x 사용 (v6 이하 사용 금지)
 *  - JsonDocument 단일 타입만 사용
 *  - createNestedArray/Object/containsKey 사용 금지
 *  - memset + strlcpy 기반 안전 초기화
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 *   - namespace 명        : 모듈약어_ 접두사
 *   - 전역 상수,매크로      : G_모듈약어_ 접두사
 *   - enum 상수             : EN_모듈약어_ 접두사
 *   - 구조체                : ST_모듈약어_ 접두사
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <string.h>

#include "C20_Action_0410.h"

// -------------------------------------------------------
// 스키마 버전 / 경로
// -------------------------------------------------------
static constexpr uint16_t G_C10_CFG_VER = 400;

namespace C10_DEF {
    // ---------- Paths ----------
    static constexpr const char* CFG_PATH = "/json/config_0410.json";
    static constexpr const char* CFG_TMP  = "/json/config_0410.json.tmp";
    static constexpr const char* CFG_BAK  = "/json/config_0410.json.bak";

    static constexpr const char* BOOT_PATH     = "/json/boot_state_0410.json";
    static constexpr const char* BOOT_TMP_PATH = "/json/boot_state_0410.json.tmp";

    static constexpr uint8_t SAFE_FAIL_THRESHOLD = 2;

    // ---------- Limits ----------
    static constexpr size_t STA_SSID_MAX = 32;
    static constexpr size_t STA_PASS_MAX = 64;
    static constexpr size_t AP_SSID_MAX  = 32;
    static constexpr size_t AP_PASS_MAX  = 64;
    static constexpr size_t MDNS_MAX     = 32;

    // ---------- Slot 개수 ----------
    // Slot 배열 순서 (고정)
    //   slots[0..14]  : S1..S15 (물리 버튼)
    //   flick[0..3]   : G1..G4 (L, R, U, D)
    //   linear[0..3]  : G5..G8 (L, R, U, D)
    //   tilt[0..3]    : T1..T4 (U, D, L, R) — Mode 3 전용
    static constexpr uint8_t SLOT_BTN_COUNT    = 15;
    static constexpr uint8_t SLOT_FLICK_COUNT  = 4;
    static constexpr uint8_t SLOT_LINEAR_COUNT = 4;
    static constexpr uint8_t SLOT_TILT_COUNT   = 4;

    // ---------- Mode ----------
    static constexpr uint8_t MODE_COUNT = 3;
}


// -------------------------------------------------------
// WiFi (기존 유지)
// -------------------------------------------------------
enum EN_C10_WIFI_MODE_t : uint8_t {
    EN_C10_WIFI_AUTO = 0,
    EN_C10_WIFI_AP   = 1,
    EN_C10_WIFI_STA  = 2
};

struct ST_C10_WiFiConfig_t {
    uint8_t mode;
    char sta_ssid[33];
    char sta_pass[65];
    char ap_ssid[33];
    char ap_pass[65];
    char mdns_host[33];
};

// -------------------------------------------------------
// Mode Config
// -------------------------------------------------------
struct ST_C10_ModeConfig_t {
    ST_C20_ActionSlot_t slots [C10_DEF::SLOT_BTN_COUNT];     // S1..S15
    ST_C20_ActionSlot_t flick [C10_DEF::SLOT_FLICK_COUNT];   // G1..G4
    ST_C20_ActionSlot_t linear[C10_DEF::SLOT_LINEAR_COUNT];  // G5..G8
    ST_C20_ActionSlot_t tilt  [C10_DEF::SLOT_TILT_COUNT];    // T1..T4
};


// -------------------------------------------------------
// Precision Mode (v0320 유지)
// -------------------------------------------------------
enum EN_C10_E10PrecisionMode_t : uint8_t {
    EN_C10_E10_PREC_OFF  = 0,
    EN_C10_E10_PREC_LOW  = 1,
    EN_C10_E10_PREC_MED  = 2,
    EN_C10_E10_PREC_HIGH = 3,
    EN_C10_E10_PREC_PPT  = 4,
    EN_C10_E10_PREC_MAX
};

// -------------------------------------------------------
// E10 Config (v0410 확장)
// -------------------------------------------------------
struct ST_C10_E10Config_t {
    // ---- 기존 물리 엔진 파라미터 (v0320에서 유지) ----
    uint8_t dpi_level;
    bool    hard_click_lock;

    float scale_base[3];
    float accel_gain[3];
    float accel_threshold;

    float   wheel_threshold_deg;
    uint8_t wheel_step_max;

    float    gesture_flick_deg;
    uint16_t gesture_cooldown_ms;

    float scroll_cursor_damp;

    // ---- Precision (v0320 유지, web 설정 전용) ----
    uint8_t  precision_mode;         // EN_C10_E10PrecisionMode_t
    float    precision_deadzone;
    float    precision_gain;
    float    precision_accel;
    uint8_t  precision_max_step;
    float    precision_smooth;
    uint16_t prec_entry_ms;
    uint16_t prec_exit_ms;
    float    prec_entry_still_deg;
    float    prec_exit_move_deg;
    uint8_t  prec_profile;

    // ---- v0410 신규 ----
    uint8_t led_brightness;         // 0~255
    bool    battery_adc_enabled;    // 향후

    struct {
        float    still_th;          // deg/s
        uint16_t still_win_ms;
        float    alpha;             // 0~1 (bias 추종 계수)
    } gyro_bias;

    struct {
        float    th;                // m/s^2
        float    impulse_th;        // m/s (임펄스 임계)
        uint16_t window_ms;
    } linear;

    struct {
        float    p2p_th;            // deg/s
        uint16_t window_ms;
        uint16_t cooldown_ms;
    } flick;

    struct {
        float    angle_deg;
        uint16_t hold_ms;
        uint8_t  repeat_hz;         // 1~20
    } tilt_hold;

    uint32_t sleep_idle_timeout_ms;
    uint8_t  active_mode;           // 1/2/3 (마지막 사용)
    uint8_t  active_peer_index;     // 0/1/2 (마지막 peer)

    // ---- Mode별 슬롯 매트릭스 ----
    ST_C10_ModeConfig_t modes[C10_DEF::MODE_COUNT];
};

// -------------------------------------------------------
// Boot State (기존 유지)
// -------------------------------------------------------
struct ST_C10_BootState_t {
    bool     safe_mode;
    uint8_t  fail_count;
    bool     pending;
    uint32_t boot_ms;
    uint8_t  last_reset_reason;
};
