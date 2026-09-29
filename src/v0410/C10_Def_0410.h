// File: src/v0410/C10_Def_0410.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : C10_Def_0410.h
 * 모듈약어 : C10
 * 모듈명 : Config Definitions (v0410 프로파일 + 매크로 + Global/Override)
 * ------------------------------------------------------
 * 기능 요약
 *  - 스키마 v5 (프로파일 × 매크로 × Global+Override 슬롯)
 *  - 트리거 라이브러리 (27개) + 잠금 플래그 (4개)
 *  - 매크로 자료구조 (8×8, delay ≤ 2000ms)
 *  - Profile 슬롯 매트릭스 (Global + Mode별 Override)
 *  - E10 파라미터 (modes 제외, 프로파일 단위 관리)
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <string.h>
#include <stdio.h>

#include "C20_Action_0410.h"

static constexpr uint16_t G_C10_CFG_VER = 410;

namespace C10_DEF {
    static constexpr uint8_t TRIG_COUNT  = 27;
    static constexpr uint8_t MODE_COUNT  = 3;
    static constexpr uint8_t PROFILE_MAX = 5;

    static constexpr uint8_t  MACRO_MAX          = 8;
    static constexpr uint8_t  MACRO_STEP_MAX     = 8;
    static constexpr uint16_t MACRO_DELAY_MAX_MS = 2000;
    static constexpr uint8_t  MACRO_NAME_LEN     = 16;
    static constexpr uint8_t  PROFILE_NAME_LEN   = 16;

    static constexpr const char* PROFILES_DIR       = "/json/profiles";
    static constexpr const char* PROFILE_ACTIVE     = "/json/active_profile.json";
    static constexpr const char* PROFILE_ACTIVE_TMP = "/json/active_profile.json.tmp";
    static constexpr const char* BOOT_PATH          = "/json/boot_state_0410.json";
    static constexpr const char* BOOT_TMP_PATH      = "/json/boot_state_0410.json.tmp";

    static constexpr uint8_t SAFE_FAIL_THRESHOLD = 2;

    static constexpr size_t STA_SSID_MAX = 32;
    static constexpr size_t STA_PASS_MAX = 64;
    static constexpr size_t AP_SSID_MAX  = 32;
    static constexpr size_t AP_PASS_MAX  = 64;
    static constexpr size_t MDNS_MAX     = 32;

    static inline bool makeProfilePath(char* p_dst, size_t p_dstSize, uint8_t p_idx) {
        if (!p_dst || p_dstSize < 32) return false;
        const int v_n = snprintf(p_dst, p_dstSize, "%s/profile_%u.json", PROFILES_DIR, (unsigned)p_idx);
        return (v_n > 0 && (size_t)v_n < p_dstSize);
    }

    static inline bool makeProfileTmpPath(char* p_dst, size_t p_dstSize, uint8_t p_idx) {
        if (!p_dst || p_dstSize < 40) return false;
        const int v_n = snprintf(p_dst, p_dstSize, "%s/profile_%u.json.tmp", PROFILES_DIR, (unsigned)p_idx);
        return (v_n > 0 && (size_t)v_n < p_dstSize);
    }
}

// 27개 트리거 enum (0~26)
enum EN_C10_Trigger_t : uint8_t {
    EN_C10_TRIG_TOP_L_CLICK      = 0,
    EN_C10_TRIG_TOP_L_DOUBLE     = 1,
    EN_C10_TRIG_TOP_L_LONG       = 2,
    EN_C10_TRIG_TOP_M_CLICK      = 3,
    EN_C10_TRIG_TOP_M_HOLD       = 4,
    EN_C10_TRIG_TOP_R_CLICK      = 5,
    EN_C10_TRIG_TOP_R_DOUBLE     = 6,
    EN_C10_TRIG_TOP_R_LONG       = 7,
    EN_C10_TRIG_SIDE_F_CLICK     = 8,
    EN_C10_TRIG_SIDE_F_LONG      = 9,
    EN_C10_TRIG_SIDE_C_CLICK     = 10,
    EN_C10_TRIG_SIDE_C_DOUBLE    = 11,
    EN_C10_TRIG_SIDE_C_HOLD_2S   = 12,
    EN_C10_TRIG_SIDE_R_CLICK     = 13,
    EN_C10_TRIG_SIDE_R_LONG      = 14,
    EN_C10_TRIG_FLICK_LEFT       = 15,
    EN_C10_TRIG_FLICK_RIGHT      = 16,
    EN_C10_TRIG_FLICK_UP         = 17,
    EN_C10_TRIG_FLICK_DOWN       = 18,
    EN_C10_TRIG_LINEAR_LEFT      = 19,
    EN_C10_TRIG_LINEAR_RIGHT     = 20,
    EN_C10_TRIG_LINEAR_UP        = 21,
    EN_C10_TRIG_LINEAR_DOWN      = 22,
    EN_C10_TRIG_TILT_LEFT        = 23,
    EN_C10_TRIG_TILT_RIGHT       = 24,
    EN_C10_TRIG_TILT_UP          = 25,
    EN_C10_TRIG_TILT_DOWN        = 26,
    EN_C10_TRIG_MAX
};

// 잠금 트리거 플래그 (0: Top L Click, 4: Top M Hold, 11: Side C Double, 12: Side C 2s Hold)
static constexpr bool G_C10_TRIG_LOCKED[EN_C10_TRIG_MAX] = {
    true,  false, false, false, true,
    false, false, false, false, false,
    false, true,  true,  false, false,
    false, false, false, false,
    false, false, false, false,
    false, false, false, false
};

static inline const char* C10_TriggerName(uint8_t p_trig) {
    switch ((EN_C10_Trigger_t)p_trig) {
        case EN_C10_TRIG_TOP_L_CLICK:    return "Top L Click";
        case EN_C10_TRIG_TOP_L_DOUBLE:   return "Top L Double";
        case EN_C10_TRIG_TOP_L_LONG:     return "Top L Long";
        case EN_C10_TRIG_TOP_M_CLICK:    return "Top M Click";
        case EN_C10_TRIG_TOP_M_HOLD:     return "Top M Hold";
        case EN_C10_TRIG_TOP_R_CLICK:    return "Top R Click";
        case EN_C10_TRIG_TOP_R_DOUBLE:   return "Top R Double";
        case EN_C10_TRIG_TOP_R_LONG:     return "Top R Long";
        case EN_C10_TRIG_SIDE_F_CLICK:   return "Side F Click";
        case EN_C10_TRIG_SIDE_F_LONG:    return "Side F Long";
        case EN_C10_TRIG_SIDE_C_CLICK:   return "Side C Click";
        case EN_C10_TRIG_SIDE_C_DOUBLE:  return "Side C Double";
        case EN_C10_TRIG_SIDE_C_HOLD_2S: return "Side C 2s Hold";
        case EN_C10_TRIG_SIDE_R_CLICK:   return "Side R Click";
        case EN_C10_TRIG_SIDE_R_LONG:    return "Side R Long";
        case EN_C10_TRIG_FLICK_LEFT:     return "Flick Left";
        case EN_C10_TRIG_FLICK_RIGHT:    return "Flick Right";
        case EN_C10_TRIG_FLICK_UP:       return "Flick Up";
        case EN_C10_TRIG_FLICK_DOWN:     return "Flick Down";
        case EN_C10_TRIG_LINEAR_LEFT:    return "Linear Left";
        case EN_C10_TRIG_LINEAR_RIGHT:   return "Linear Right";
        case EN_C10_TRIG_LINEAR_UP:      return "Linear Up";
        case EN_C10_TRIG_LINEAR_DOWN:    return "Linear Down";
        case EN_C10_TRIG_TILT_LEFT:      return "Tilt Left";
        case EN_C10_TRIG_TILT_RIGHT:     return "Tilt Right";
        case EN_C10_TRIG_TILT_UP:        return "Tilt Up";
        case EN_C10_TRIG_TILT_DOWN:      return "Tilt Down";
        default:                         return "?";
    }
}

static inline const char* C10_TriggerGroup(uint8_t p_trig) {
    if (p_trig <= 14) return "button";
    if (p_trig <= 22) return "gesture";
    return "tilt";
}

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

enum EN_C10_E10PrecisionMode_t : uint8_t {
    EN_C10_E10_PREC_OFF  = 0,
    EN_C10_E10_PREC_LOW  = 1,
    EN_C10_E10_PREC_MED  = 2,
    EN_C10_E10_PREC_HIGH = 3,
    EN_C10_E10_PREC_PPT  = 4,
    EN_C10_E10_PREC_MAX
};

struct ST_C10_E10Config_t {
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

    uint8_t  precision_mode;
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

    uint8_t led_brightness;
    bool    battery_adc_enabled;

    struct {
        float    still_th;
        uint16_t still_win_ms;
        float    alpha;
    } gyro_bias;

    struct {
        float    th;
        float    impulse_th;
        uint16_t window_ms;
    } linear;

    struct {
        float    p2p_th;
        uint16_t window_ms;
        uint16_t cooldown_ms;
    } flick;

    struct {
        float    angle_deg;
        uint16_t hold_ms;
        uint8_t  repeat_hz;
    } tilt_hold;

    uint32_t sleep_idle_timeout_ms;
    uint8_t  active_mode;
    uint8_t  active_peer_index;
};

// 매크로 Step (16 Bytes)
struct ST_C10_MacroStep_t {
    uint8_t  kind;       // 1~9 (Primitive만 허용)
    uint8_t  holdMode;   // 0=NONE, 1=PRESS, 2=REPEAT
    uint16_t delayMs;    // 0~2000 ms
    uint16_t param16;
    uint16_t _pad;
    uint32_t param32;
};

// 매크로 개별 정의 (148 Bytes)
struct ST_C10_Macro_t {
    char     name[C10_DEF::MACRO_NAME_LEN];
    uint8_t  stepCount;
    uint8_t  _pad[3];
    ST_C10_MacroStep_t steps[C10_DEF::MACRO_STEP_MAX];
};

// 프로파일 매크로 라이브러리 (1188 Bytes)
struct ST_C10_MacroLib_t {
    uint8_t  count;
    uint8_t  _pad[3];
    ST_C10_Macro_t macros[C10_DEF::MACRO_MAX];
};

// 슬롯 매트릭스 (876 Bytes)
struct ST_C10_ProfileSlots_t {
    ST_C20_ActionSlot_t global[EN_C10_TRIG_MAX];
    ST_C20_ActionSlot_t modes [C10_DEF::MODE_COUNT][EN_C10_TRIG_MAX];
    uint32_t            overrideMask[C10_DEF::MODE_COUNT];
};

// 런타임 슬롯 해석 인라인 함수
static inline ST_C20_ActionSlot_t C10_ResolveSlot(
    const ST_C10_ProfileSlots_t& p_slots, uint8_t p_mode, uint8_t p_trig)
{
    if (p_mode < 1 || p_mode > C10_DEF::MODE_COUNT) p_mode = 1;
    if (p_trig >= EN_C10_TRIG_MAX) {
        ST_C20_ActionSlot_t v_none = { EN_C20_ACT_NONE, EN_C20_HOLD_NONE, 0, 0 };
        return v_none;
    }
    const uint8_t m = (uint8_t)(p_mode - 1);
    if (p_slots.overrideMask[m] & (1u << p_trig)) {
        return p_slots.modes[m][p_trig];
    }
    return p_slots.global[p_trig];
}

// Profile 통합 구조체 (~2.4 KB)
struct ST_C10_ProfileConfig_t {
    uint16_t ver;
    char     name[C10_DEF::PROFILE_NAME_LEN];

    ST_C10_WiFiConfig_t   wifi;
    ST_C10_E10Config_t    e10;
    ST_C10_ProfileSlots_t slots;
    ST_C10_MacroLib_t     macros;
};

struct ST_C10_ProfileIndex_t {
    uint8_t activeIndex;
    uint8_t profileCount;
};

struct ST_C10_BootState_t {
    bool     safe_mode;
    uint8_t  fail_count;
    bool     pending;
    uint32_t boot_ms;
    uint8_t  last_reset_reason;
};
