// File: src/v0415/C10_Def_0415.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : C10_Def_0415.h
 * 모듈약어 : C10
 * 모듈명 : Config Definitions (v0415 프로파일 + 매크로 + Global/Override)
 * ------------------------------------------------------
 * 기능 요약
 *  - 스키마 v411 (프로파일 × 매크로 × Global+Override 슬롯)
 *  - 트리거 라이브러리 (27개) + 잠금 플래그 (4개)
 *  - 매크로 자료구조 (8×8, delay ≤ 2000ms)
 *  - Profile 슬롯 매트릭스 (Global + Mode별 Override)
 *  - E10 파라미터 (modes 제외, 프로파일 단위 관리)
 *  - [Phase 1~3] Motion Advanced (Click-Freeze / EMA / Snap)
 *
 * [v0415 주요 변경]
 *  - G_C10_CFG_VER: 410 → 411
 *    · Consumer mask Descriptor 정합 (EN_C20_Consumer_t 전면 재정의)
 *    · C10_Config_0415.cpp::_migrateProfileV410ToV411()에서 마이그레이션
 *  - G_C10_CFG_VER_V410 상수 추가 (마이그레이션 감지용)
 *  - TRIG_COUNT 상수 삭제 → EN_C10_TRIG_MAX 단독 사용 (SSOT)
 *  - G_C10_FULL_OVERRIDE_MASK 상수 추가
 *  - ST_C10_MacroStep_t 정렬 최적화 (_pad 제거, 16B → 12B)
 *  - BOOT_PATH 경로: _0412 → _0415 bump
 *  - PROFILES_DIR / PROFILE_ACTIVE: 버전-프리 유지
 *    (스키마 버전은 JSON 내부 "ver" 필드로 관리)
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <string.h>
#include <stdio.h>

#include "C20_Action_0415.h"

// ======================================================
// [v0415] 스키마 버전
// ------------------------------------------------------
//  - G_C10_CFG_VER       : 현재 스키마 (저장 시 이 값 기록)
//  - G_C10_CFG_VER_V410  : legacy (로드 시 마이그레이션 트리거)
// ------------------------------------------------------
static constexpr uint16_t G_C10_CFG_VER      = 411;
static constexpr uint16_t G_C10_CFG_VER_V410 = 410;

namespace C10_DEF {
    // --------------------------------------------------
    // 구조 크기 상수 (SSOT: EN_C10_TRIG_MAX)
    //   - v0412의 C10_DEF::TRIG_COUNT는 삭제됨
    //   - 실제 크기는 EN_C10_TRIG_MAX 사용
    // --------------------------------------------------
    static constexpr uint8_t MODE_COUNT  = 3;
    static constexpr uint8_t PROFILE_MAX = 5;

    static constexpr uint8_t  MACRO_MAX          = 8;
    static constexpr uint8_t  MACRO_STEP_MAX     = 8;
    static constexpr uint16_t MACRO_DELAY_MAX_MS = 2000;
    static constexpr uint8_t  MACRO_NAME_LEN     = 16;
    static constexpr uint8_t  PROFILE_NAME_LEN   = 16;

    // --------------------------------------------------
    // 파일 경로
    //   - PROFILES_DIR / PROFILE_ACTIVE: 버전-프리 (스키마는 JSON 내부 "ver")
    //   - BOOT_PATH: v0415 신규 경로 (_0415 suffix)
    // --------------------------------------------------
    static constexpr const char* PROFILES_DIR       = "/json/profiles";
    static constexpr const char* PROFILE_ACTIVE     = "/json/active_profile.json";
    static constexpr const char* PROFILE_ACTIVE_TMP = "/json/active_profile.json.tmp";
    static constexpr const char* BOOT_PATH          = "/json/boot_state_0415.json";
    static constexpr const char* BOOT_TMP_PATH      = "/json/boot_state_0415.json.tmp";

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

// ------------------------------------------------------
// [v0415] Mode Override 완전 활성화 마스크
//   - 27비트 전부 1 (Mode 1/2/3 기본값에서 사용)
//   - 이전 v0412에서 0x07FFFFFFu 매직 넘버로 사용되던 값
// ------------------------------------------------------
static constexpr uint32_t G_C10_FULL_OVERRIDE_MASK = (1u << EN_C10_TRIG_MAX) - 1u;

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

// =====================================================
// [Phase 1~3] Motion Advanced
// =====================================================
struct ST_C10_MotionAdv_ClickFreeze_t {
    bool     enable;           // 기능 온/오프
    float    gyro_th;          // 정지 클릭 판정 (deg/s)
    uint16_t max_ms;           // 드래그 판정 임계 (ms)
    uint16_t hold_ms;          // UP 후 완전 동결 (ms)
    uint16_t fadeout_ms;       // 감쇠 시간 (ms)
    float    move_th;          // 강제 이탈 변위 (px)
    float    freeze_move_th;   // 이동 의도 판정 (deg/s)
    uint8_t  _pad[2];
};

struct ST_C10_MotionAdv_Ema_t {
    float    alpha_min;         // 저속 최대 스무딩 (0.01~0.2)
    float    alpha_max;         // 고속 최소 스무딩 (0.5~0.95)
    float    deadzone_th;       // 저속 경계 (rad/s)
    float    fast_th;           // 고속 경계 (rad/s)
    float    reversal_th;       // 방향 전환 임계 (rad/s)
    bool     reversal_reset;    // 방향 전환 시 EMA 리셋
    uint8_t  _pad[3];
};

struct ST_C10_MotionAdv_Snap_t {
    bool     enable;           // 기능 온/오프
    uint8_t  mode_mask;        // 0x01=Mode1, 0x02=Mode2, 0x04=Mode3
    uint8_t  axis_mode;        // 0=both, 1=horizontal, 2=vertical
    uint8_t  confirm_frames;   // 축 확정 프레임 수 (Chattering 방지)
    float    ratio_enter;      // 축 판정 임계 (4.0 → 4:1)
    float    strength;         // Soft Snap 강도 (0.85 → 부축 15% 투과)
    uint8_t  _pad[2];
};

struct ST_C10_MotionAdv_t {
    ST_C10_MotionAdv_ClickFreeze_t click_freeze;
    ST_C10_MotionAdv_Ema_t         ema;
    ST_C10_MotionAdv_Snap_t        snap;
};

// =====================================================
// [Phase 11.6] Power Management Config
// =====================================================
struct ST_C10_PowerConfig_t {
    // [C-2, I-7] Idle timeout (Mode별 + BLE 연결 시)
    uint32_t idle_timeout_ms[3];      // Mode 1/2/3 (기본 60s/120s/300s)
    uint32_t idle_timeout_ble_ms;     // BLE 연결 중 (기본 300s)

    // [I-6] Pairing 중 idle timeout
    uint32_t pairing_idle_timeout_ms; // 기본 30s

    // [N-1] Deep-sleep idle timeout (0 = 비활성)
    uint32_t deep_idle_timeout_ms;    // 기본 600s

    // [C-4] Wake Backoff
    uint16_t wake_min_active_ms;      // 기본 500ms

    // [C-5] MPU WoM 파라미터
    uint8_t  wom_threshold;           // MOT_THR (기본 25 = 800mg)
    uint8_t  wom_duration;            // MOT_DUR (기본 4ms)

    // [I-3] Sleep 후 Fast Recalibration
    uint16_t fast_recalib_ms;         // 기본 300ms

    // [N-5] LED fade
    uint16_t led_fadeout_ms;          // 기본 500ms
    uint16_t led_fadein_ms;           // 기본 300ms

    uint8_t  _pad[4];
};

// =====================================================
// [Phase 11.7] Button Timing Config
// =====================================================
struct ST_C10_ButtonConfig_t {
    // [C-1, C-4] 비대칭 Debounce (Press/Release 분리)
    uint16_t debounce_press_ms;      // 20~50 (기본 32, 8ms 배수)
    uint16_t debounce_release_ms;    // 10~40 (기본 16, 8ms 배수)

    // [I-1] Long press 임계 (실제 누름 시점 기준)
    uint16_t long_delay_ms;          // 500~1500 (기본 800)

    // [I-2] Double click 윈도우 (debounce 여유 반영)
    uint16_t double_delay_ms;        // 250~500 (기본 320)

    // Hold 판정
    uint16_t hold_2s_ms;             // 1500~3000 (기본 2000)
    uint16_t hold_3s_ms;             // 2500~5000 (기본 3000)

    // [I-3] 최소 클릭 시간 (초단 클릭 discard)
    uint16_t min_click_ms;           // 5~50 (기본 16, 8ms 배수)

    // [C-3] 하이브리드 debounce: 연속 tick 카운트
    uint8_t  debounce_min_ticks;     // 2~5 (기본 3)

    uint8_t  _pad[3];
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

    ST_C10_MotionAdv_t    motion_adv;   // [Phase 1~3]
    ST_C10_PowerConfig_t  power;        // [Phase 11.6]
    ST_C10_ButtonConfig_t button;       // [Phase 11.7]
};

// =====================================================
// [v0415] 매크로 Step (12 Bytes, 이전 16 Bytes)
// ------------------------------------------------------
//  v0412: kind(1) holdMode(1) delayMs(2) param16(2) _pad(2) param32(4) = 16B
//  v0415: kind(1) holdMode(1) delayMs(2) param16(2) param32(4)         = 12B
//         (_pad 제거, 구조체 정렬 최적화)
//
//  영향: 매크로 라이브러리 1개당 32B 절감 → 총 256B 절감
//  JSON 직렬화는 필드별로 이루어지므로 무영향
// =====================================================
struct ST_C10_MacroStep_t {
    uint8_t  kind;       // 1~8 (Primitive만 허용)
    uint8_t  holdMode;   // 0=NONE, 1=PRESS, 2=REPEAT
    uint16_t delayMs;    // 0~2000 ms
    uint16_t param16;
    uint32_t param32;
};

// 매크로 개별 정의 (12 + 3 + 8*12 = 111B, alignment 4 → 112B)
struct ST_C10_Macro_t {
    char     name[C10_DEF::MACRO_NAME_LEN];
    uint8_t  stepCount;
    uint8_t  _pad[3];
    ST_C10_MacroStep_t steps[C10_DEF::MACRO_STEP_MAX];
};

// 프로파일 매크로 라이브러리 (4 + 8*112 = 900B)
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

// Profile 통합 구조체
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
