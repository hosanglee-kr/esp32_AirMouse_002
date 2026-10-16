// File: src/v0415/C10_Config_Valid_0415.cpp
// =======================================================
/*
 * ------------------------------------------------------
 * 소스명 : C10_Config_Valid_0415.cpp
 * 모듈약어 : C10
 * 모듈명 : Config Manager (v0415) - Validation
 * ------------------------------------------------------
 * 기능 요약
 *  - 프로파일 무결성 검증 (Wi-Fi / E10 / Slot / Macro)
 *  - E10 파라미터 범위 규칙 테이블 (G_E10_RULES[], 68 규칙)
 *  - 관계 검증 5종 (테이블로 표현 불가한 항목)
 *    · button.press >= button.release
 *    · button.hold_2s < button.hold_3s
 *    · ema.alpha_min < ema.alpha_max
 *    · ema.deadzone_th < ema.fast_th
 *    · deep_idle_timeout_ms == 0 또는 300000~7200000
 *  - Consumer kind의 param32 24-bit 상한 검증 (G_C20_CONSUMER_MASK_MAX)
 *  - 매크로 step 검증 (kind 상한, holdMode NONE, delay ≤ 2000ms)
 *
 * [v0415 rev18.1 - Phase 5-A 분할]
 *  - 본 .cpp는 C10_Config_0415.cpp에서 Validation 파트 분리 (3-TU 분할)
 *    · C10_Config_0415.cpp           : lifecycle / CRUD / File IO / Hosts
 *    · C10_Config_Json_0415.cpp      : _build* / _patch* / JSON 직렬화
 *    · C10_Config_Validate_0415.cpp  : 본 파일 (validate* + G_E10_RULES[])
 *  - Phase 3-C 테이블화 반영
 *    · 기존 200줄 if-else → 68행 테이블 + 5개 관계 검증
 *    · 신규 필드 추가 시 G_E10_RULES[]에 1행만 추가
 *
 * [v0415 이전 주요 변경]
 *  - Consumer 24-bit 상한 검증 (스키마 411)
 *  - validateWiFi 강화 (AP SSID 필수, STA 모드 SSID 필수, mDNS 빈 값 금지)
 *  - gesture.* 필드 삭제 (스키마 412)
 *
 * [구현 규칙]
 *  - ArduinoJson v7.x.x 사용 (v6 이하 사용 금지)
 *  - JsonDocument 단일 타입만 사용
 *  - memset + strlcpy 기반 안전 초기화
 *  - 실패 시 D10_LOGW_C로 필드명·경계값 로그 출력
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 *   - namespace 명         : 모듈약어_ 접두사
 *   - namespace 내 상수     : 모듈약어 접두사 미사용
 *   - 전역 상수,매크로       : G_모듈약어_ 접두사
 *   - 전역 함수             : 모듈약어_ 접두사
 *   - enum 상수             : EN_모듈약어_ 접두사
 *   - 구조체                : ST_모듈약어_ 접두사
 *   - 클래스명              : CL_모듈약어_ 접두사 , 버전 제거
 *   - 클래스 private 멤버    : _ 접두사
 *   - 클래스 정적 멤버       : s_ 접두사
 *   - 함수 로컬 변수         : v_ 접두사
 *   - 함수 인자             : p_ 접두사
 * ------------------------------------------------------
 */

#include "C10_Config_0415.h"


// =====================================================
// [v0415 Phase 3-C] validateE10 범위 규칙 테이블
// -----------------------------------------------------
//  - 정수/실수 통합을 위해 float로 정규화
//  - 관계 검증(press >= release 등)은 테이블로 표현 불가 → 별도 유지
//  - 신규 필드 추가 시 이 테이블에 1행만 추가
//  - 68개 범위 규칙
// =====================================================
namespace {

struct ST_RangeRule_t {
    const char* name;
    float       min;
    float       max;
    float       (*getter)(const ST_C10_E10Config_t&);
};

static const ST_RangeRule_t G_E10_RULES[] = {
    // ---- 기본 DPI / 하드클릭 ----
    {"dpi_level",             1.0f,    3.0f,
        [](const auto& e){ return (float)e.dpi_level; }},

    // ---- 가속도 / 스크롤 감쇠 ----
    {"accel_threshold",       0.0f,    50.0f,
        [](const auto& e){ return e.accel_threshold; }},
    {"wheel_threshold_deg",   1.0f,    360.0f,
        [](const auto& e){ return e.wheel_threshold_deg; }},
    {"wheel_step_max",        1.0f,    50.0f,
        [](const auto& e){ return (float)e.wheel_step_max; }},
    {"scroll_cursor_damp",    0.0f,    1.0f,
        [](const auto& e){ return e.scroll_cursor_damp; }},

    // ---- Precision ----
    {"precision_mode",        0.0f,    4.0f,
        [](const auto& e){ return (float)e.precision_mode; }},
    {"precision_deadzone",    0.0f,    50.0f,
        [](const auto& e){ return e.precision_deadzone; }},
    {"precision_gain",        0.0f,    5.0f,
        [](const auto& e){ return e.precision_gain; }},
    {"precision_accel",       0.0f,    5.0f,
        [](const auto& e){ return e.precision_accel; }},
    {"precision_max_step",    1.0f,    200.0f,
        [](const auto& e){ return (float)e.precision_max_step; }},
    {"precision_smooth",      0.0f,    1.0f,
        [](const auto& e){ return e.precision_smooth; }},
    {"prec_entry_ms",         50.0f,   5000.0f,
        [](const auto& e){ return (float)e.prec_entry_ms; }},
    {"prec_exit_ms",          50.0f,   5000.0f,
        [](const auto& e){ return (float)e.prec_exit_ms; }},
    {"prec_entry_still_deg",  0.1f,    20.0f,
        [](const auto& e){ return e.prec_entry_still_deg; }},
    {"prec_exit_move_deg",    0.1f,    50.0f,
        [](const auto& e){ return e.prec_exit_move_deg; }},
    {"prec_profile",          0.0f,    5.0f,
        [](const auto& e){ return (float)e.prec_profile; }},

    // ---- Gyro bias ----
    {"gyro_bias.still_th",    0.1f,    20.0f,
        [](const auto& e){ return e.gyro_bias.still_th; }},
    {"gyro_bias.still_win_ms",50.0f,   5000.0f,
        [](const auto& e){ return (float)e.gyro_bias.still_win_ms; }},
    {"gyro_bias.alpha",       0.0001f, 0.1f,
        [](const auto& e){ return e.gyro_bias.alpha; }},

    // ---- Linear ----
    {"linear.th",             0.05f,   5.0f,
        [](const auto& e){ return e.linear.th; }},
    {"linear.impulse_th",     0.1f,    5.0f,
        [](const auto& e){ return e.linear.impulse_th; }},
    {"linear.window_ms",      50.0f,   1000.0f,
        [](const auto& e){ return (float)e.linear.window_ms; }},

    // ---- Flick ----
    {"flick.p2p_th",          50.0f,   2000.0f,
        [](const auto& e){ return e.flick.p2p_th; }},
    {"flick.window_ms",       50.0f,   500.0f,
        [](const auto& e){ return (float)e.flick.window_ms; }},
    {"flick.cooldown_ms",     100.0f,  5000.0f,
        [](const auto& e){ return (float)e.flick.cooldown_ms; }},

    // ---- Tilt hold ----
    {"tilt_hold.angle_deg",   5.0f,    45.0f,
        [](const auto& e){ return e.tilt_hold.angle_deg; }},
    {"tilt_hold.hold_ms",     100.0f,  2000.0f,
        [](const auto& e){ return (float)e.tilt_hold.hold_ms; }},
    {"tilt_hold.repeat_hz",   1.0f,    20.0f,
        [](const auto& e){ return (float)e.tilt_hold.repeat_hz; }},

    // ---- Sleep / Mode ----
    {"sleep_idle_timeout_ms", 5000.0f, 3600000.0f,
        [](const auto& e){ return (float)e.sleep_idle_timeout_ms; }},
    {"active_mode",           1.0f,    3.0f,
        [](const auto& e){ return (float)e.active_mode; }},
    {"active_peer_index",     0.0f,    2.0f,
        [](const auto& e){ return (float)e.active_peer_index; }},

    // ---- Motion Advanced: click_freeze ----
    {"motion_adv.click_freeze.gyro_th",        5.0f,  30.0f,
        [](const auto& e){ return e.motion_adv.click_freeze.gyro_th; }},
    {"motion_adv.click_freeze.max_ms",         50.0f, 300.0f,
        [](const auto& e){ return (float)e.motion_adv.click_freeze.max_ms; }},
    {"motion_adv.click_freeze.hold_ms",        0.0f,  50.0f,
        [](const auto& e){ return (float)e.motion_adv.click_freeze.hold_ms; }},
    {"motion_adv.click_freeze.fadeout_ms",     0.0f,  80.0f,
        [](const auto& e){ return (float)e.motion_adv.click_freeze.fadeout_ms; }},
    {"motion_adv.click_freeze.move_th",        1.0f,  10.0f,
        [](const auto& e){ return e.motion_adv.click_freeze.move_th; }},
    {"motion_adv.click_freeze.freeze_move_th", 10.0f, 60.0f,
        [](const auto& e){ return e.motion_adv.click_freeze.freeze_move_th; }},

    // ---- Motion Advanced: ema ----
    {"motion_adv.ema.alpha_min",    0.01f, 0.2f,
        [](const auto& e){ return e.motion_adv.ema.alpha_min; }},
    {"motion_adv.ema.alpha_max",    0.5f,  0.95f,
        [](const auto& e){ return e.motion_adv.ema.alpha_max; }},
    {"motion_adv.ema.deadzone_th",  1.0f,  10.0f,
        [](const auto& e){ return e.motion_adv.ema.deadzone_th; }},
    {"motion_adv.ema.fast_th",      10.0f, 30.0f,
        [](const auto& e){ return e.motion_adv.ema.fast_th; }},
    {"motion_adv.ema.reversal_th",  5.0f,  20.0f,
        [](const auto& e){ return e.motion_adv.ema.reversal_th; }},

    // ---- Motion Advanced: snap ----
    {"motion_adv.snap.mode_mask",      0.0f, 7.0f,
        [](const auto& e){ return (float)e.motion_adv.snap.mode_mask; }},
    {"motion_adv.snap.axis_mode",      0.0f, 2.0f,
        [](const auto& e){ return (float)e.motion_adv.snap.axis_mode; }},
    {"motion_adv.snap.confirm_frames", 1.0f, 10.0f,
        [](const auto& e){ return (float)e.motion_adv.snap.confirm_frames; }},
    {"motion_adv.snap.ratio_enter",    2.0f, 10.0f,
        [](const auto& e){ return e.motion_adv.snap.ratio_enter; }},
    {"motion_adv.snap.strength",       0.5f, 1.0f,
        [](const auto& e){ return e.motion_adv.snap.strength; }},

    // ---- Power (idle_timeout 배열 개별) ----
    {"power.idle_timeout_ms[0]", 5000.0f, 3600000.0f,
        [](const auto& e){ return (float)e.power.idle_timeout_ms[0]; }},
    {"power.idle_timeout_ms[1]", 5000.0f, 3600000.0f,
        [](const auto& e){ return (float)e.power.idle_timeout_ms[1]; }},
    {"power.idle_timeout_ms[2]", 5000.0f, 3600000.0f,
        [](const auto& e){ return (float)e.power.idle_timeout_ms[2]; }},
    {"power.idle_timeout_ble_ms", 60000.0f, 3600000.0f,
        [](const auto& e){ return (float)e.power.idle_timeout_ble_ms; }},
    {"power.pairing_idle_timeout_ms", 10000.0f, 60000.0f,
        [](const auto& e){ return (float)e.power.pairing_idle_timeout_ms; }},
    {"power.wake_min_active_ms", 100.0f, 2000.0f,
        [](const auto& e){ return (float)e.power.wake_min_active_ms; }},
    {"power.wom_threshold",      5.0f,   100.0f,
        [](const auto& e){ return (float)e.power.wom_threshold; }},
    {"power.wom_duration",       1.0f,   50.0f,
        [](const auto& e){ return (float)e.power.wom_duration; }},
    {"power.fast_recalib_ms",    100.0f, 1000.0f,
        [](const auto& e){ return (float)e.power.fast_recalib_ms; }},
    {"power.led_fadeout_ms",     0.0f,   1000.0f,
        [](const auto& e){ return (float)e.power.led_fadeout_ms; }},
    {"power.led_fadein_ms",      0.0f,   1000.0f,
        [](const auto& e){ return (float)e.power.led_fadein_ms; }},

    // ---- Button ----
    {"button.debounce_press_ms",   8.0f,    100.0f,
        [](const auto& e){ return (float)e.button.debounce_press_ms; }},
    {"button.debounce_release_ms", 8.0f,    100.0f,
        [](const auto& e){ return (float)e.button.debounce_release_ms; }},
    {"button.long_delay_ms",       300.0f,  2000.0f,
        [](const auto& e){ return (float)e.button.long_delay_ms; }},
    {"button.double_delay_ms",     150.0f,  800.0f,
        [](const auto& e){ return (float)e.button.double_delay_ms; }},
    {"button.hold_2s_ms",          1000.0f, 5000.0f,
        [](const auto& e){ return (float)e.button.hold_2s_ms; }},
    {"button.hold_3s_ms",          2000.0f, 8000.0f,
        [](const auto& e){ return (float)e.button.hold_3s_ms; }},
    {"button.min_click_ms",        0.0f,    100.0f,
        [](const auto& e){ return (float)e.button.min_click_ms; }},
    {"button.debounce_min_ticks",  1.0f,    10.0f,
        [](const auto& e){ return (float)e.button.debounce_min_ticks; }},
};

static constexpr size_t G_E10_RULES_COUNT =
    sizeof(G_E10_RULES) / sizeof(G_E10_RULES[0]);

}  // namespace


// =======================================================
// validateWiFi
// -------------------------------------------------------
//  - AP SSID 필수
//  - AP 비밀번호: 비어있거나 8자 이상
//  - STA 모드: sta_ssid 필수
//  - mDNS 호스트: 빈 문자열 금지 + MDNS_MAX 이하
// =======================================================
bool CL_C10_Config::validateWiFi(const ST_C10_WiFiConfig_t& p_w) const {
    if (p_w.mode > (uint8_t)EN_C10_WIFI_STA) return false;

    // AP SSID 필수 (빈 문자열 금지)
    if (p_w.ap_ssid[0] == '\0') return false;

    // AP 비밀번호: 비어있거나 8자 이상
    const size_t v_apLen = strlen(p_w.ap_pass);
    if (v_apLen > 0 && v_apLen < 8) return false;

    // STA 모드일 때 SSID 필수
    if (p_w.mode == (uint8_t)EN_C10_WIFI_STA && p_w.sta_ssid[0] == '\0') {
        return false;
    }

    // mDNS 호스트: 빈 문자열 금지 + 길이 제한
    const size_t v_mdnsLen = strlen(p_w.mdns_host);
    if (v_mdnsLen == 0) return false;
    if (v_mdnsLen > C10_DEF::MDNS_MAX) return false;

    return true;
}


// =====================================================
// [v0415 Phase 3-C] validateE10 (테이블 기반)
// -----------------------------------------------------
//  - G_E10_RULES[] 순회 (범위 규칙 68개)
//  - 관계 검증은 별도 유지 (테이블로 표현 불가)
//  - deep_idle_timeout은 0 허용 예외
//  - 실패 시 D10_LOGW_C로 필드명·경계값 로그
// =====================================================
bool CL_C10_Config::validateE10(const ST_C10_E10Config_t& p_e) const {
    // 1) 범위 규칙 (테이블)
    for (size_t i = 0; i < G_E10_RULES_COUNT; i++) {
        const ST_RangeRule_t& r = G_E10_RULES[i];
        const float v = r.getter(p_e);
        if (v < r.min || v > r.max) {
            D10_LOGW_C("C10::validateE10",
                       "%s=%.4f out of [%.4f, %.4f]",
                       r.name, (double)v, (double)r.min, (double)r.max);
            return false;
        }
    }

    // 2) 관계 검증 (테이블로 표현 불가)
    if (p_e.button.debounce_press_ms < p_e.button.debounce_release_ms) {
        D10_LOGW_C("C10::validateE10", "press(%u) < release(%u)",
                   (unsigned)p_e.button.debounce_press_ms,
                   (unsigned)p_e.button.debounce_release_ms);
        return false;
    }
    if (p_e.button.hold_2s_ms >= p_e.button.hold_3s_ms) {
        D10_LOGW_C("C10::validateE10", "hold_2s >= hold_3s");
        return false;
    }
    if (p_e.motion_adv.ema.alpha_min >= p_e.motion_adv.ema.alpha_max) {
        D10_LOGW_C("C10::validateE10", "ema.alpha_min >= alpha_max");
        return false;
    }
    if (p_e.motion_adv.ema.deadzone_th >= p_e.motion_adv.ema.fast_th) {
        D10_LOGW_C("C10::validateE10", "ema.deadzone >= fast_th");
        return false;
    }

    // 3) deep_idle_timeout 예외 (0 허용)
    if (p_e.power.deep_idle_timeout_ms != 0 &&
        (p_e.power.deep_idle_timeout_ms < 300000 ||
         p_e.power.deep_idle_timeout_ms > 7200000)) {
        D10_LOGW_C("C10::validateE10", "deep_idle_timeout_ms invalid");
        return false;
    }

    return true;
}


// =======================================================
// validateSlot — Consumer 24-bit 상한 검증
// =======================================================
bool CL_C10_Config::validateSlot(const ST_C20_ActionSlot_t& p_s, uint8_t p_macroCount) const {
    if (p_s.kind >= (uint8_t)EN_C20_ACT_MAX)         return false;
    if (p_s.holdMode > (uint8_t)EN_C20_HOLD_REPEAT)  return false;

    switch ((EN_C20_ActionKind_t)p_s.kind) {
        case EN_C20_ACT_NONE:            return true;
        case EN_C20_ACT_MOUSE_CLICK:
        case EN_C20_ACT_MOUSE_HOLD:      return (p_s.param16 & 0xFF) != 0;
        case EN_C20_ACT_MOUSE_WHEEL:     return C20_WheelAxis(p_s.param16) <= 1 &&
                                                C20_WheelDir (p_s.param16) <= 1;
        case EN_C20_ACT_KB_TAP:
        case EN_C20_ACT_KB_REPEAT:       return (uint8_t)p_s.param16 <= 0xE7;
        case EN_C20_ACT_KB_COMBO:        return true;

        // [v0415] Consumer 24-bit 상한 (Descriptor Report Count = 24)
        case EN_C20_ACT_CONSUMER_TAP:
        case EN_C20_ACT_CONSUMER_REPEAT:
            return p_s.param32 != 0 &&
                   ((p_s.param32 & ~G_C20_CONSUMER_MASK_MAX) == 0);

        case EN_C20_ACT_SPECIAL:         return (uint8_t)p_s.param16 < (uint8_t)EN_C20_SP_MAX;
        case EN_C20_ACT_MACRO:           return p_s.param32 < (uint32_t)p_macroCount;
        default:                         return false;
    }
}

bool CL_C10_Config::validateSlots(const ST_C10_ProfileSlots_t& p_s, uint8_t p_macroCount) const {
    for (uint8_t i = 0; i < EN_C10_TRIG_MAX; i++) {
        if (!validateSlot(p_s.global[i], p_macroCount)) {
            D10_LOGW_C("C10::validateSlots",
                       "global slot trig=%u failed: kind=%u, p16=0x%04X, p32=0x%08X",
                       (unsigned)i, (unsigned)p_s.global[i].kind,
                       (unsigned)p_s.global[i].param16,
                       (unsigned)p_s.global[i].param32);
            return false;
        }
    }
    for (uint8_t m = 0; m < C10_DEF::MODE_COUNT; m++) {
        for (uint8_t i = 0; i < EN_C10_TRIG_MAX; i++) {
            if (!validateSlot(p_s.modes[m][i], p_macroCount)) {
                D10_LOGW_C("C10::validateSlots",
                           "mode=%u slot trig=%u failed: kind=%u, p16=0x%04X, p32=0x%08X",
                           (unsigned)m, (unsigned)i, (unsigned)p_s.modes[m][i].kind,
                           (unsigned)p_s.modes[m][i].param16,
                           (unsigned)p_s.modes[m][i].param32);
                return false;
            }
        }
    }
    return true;
}


// =======================================================
// validateMacroStep — Consumer 24-bit 상한 검증
// =======================================================
bool CL_C10_Config::validateMacroStep(const ST_C10_MacroStep_t& p_s) const {
    if (p_s.kind == (uint8_t)EN_C20_ACT_MACRO) return false;
    if (p_s.kind >= (uint8_t)EN_C20_ACT_MAX)   return false;
    if (p_s.holdMode > (uint8_t)EN_C20_HOLD_REPEAT) return false;
    if (p_s.delayMs > C10_DEF::MACRO_DELAY_MAX_MS) return false;

    switch ((EN_C20_ActionKind_t)p_s.kind) {
        case EN_C20_ACT_NONE:            return true;
        case EN_C20_ACT_MOUSE_CLICK:
        case EN_C20_ACT_MOUSE_HOLD:      return (p_s.param16 & 0xFF) != 0;
        case EN_C20_ACT_MOUSE_WHEEL:     return C20_WheelAxis(p_s.param16) <= 1 &&
                                                C20_WheelDir (p_s.param16) <= 1;
        case EN_C20_ACT_KB_TAP:
        case EN_C20_ACT_KB_REPEAT:       return (uint8_t)p_s.param16 <= 0xE7;
        case EN_C20_ACT_KB_COMBO:        return true;

        // [v0415] Consumer 24-bit 상한
        case EN_C20_ACT_CONSUMER_TAP:
        case EN_C20_ACT_CONSUMER_REPEAT:
            return p_s.param32 != 0 &&
                   ((p_s.param32 & ~G_C20_CONSUMER_MASK_MAX) == 0);

        case EN_C20_ACT_SPECIAL:         return (uint8_t)p_s.param16 < (uint8_t)EN_C20_SP_MAX;
        default:                         return false;
    }
}

bool CL_C10_Config::validateMacro(const ST_C10_Macro_t& p_m) const {
    if (p_m.stepCount < 1 || p_m.stepCount > C10_DEF::MACRO_STEP_MAX) return false;
    for (uint8_t i = 0; i < p_m.stepCount; i++) {
        if (!validateMacroStep(p_m.steps[i])) return false;
    }
    return true;
}

bool CL_C10_Config::validateMacros(const ST_C10_MacroLib_t& p_m) const {
    if (p_m.count > C10_DEF::MACRO_MAX) return false;
    for (uint8_t i = 0; i < p_m.count; i++) {
        if (!validateMacro(p_m.macros[i])) return false;
    }
    return true;
}


// =======================================================
// validateProfile (엔트리 포인트)
// -------------------------------------------------------
//  - 순차 검증: WiFi → E10 → Macros → Slots
//  - 각 실패 시 D10_LOGW_C로 사유 기록
// =======================================================
bool CL_C10_Config::validateProfile(const ST_C10_ProfileConfig_t& p_p) const {
    if (!validateWiFi(p_p.wifi)) {
        D10_LOGW_C("C10::validateProfile", "validateWiFi failed");
        return false;
    }
    if (!validateE10(p_p.e10)) {
        D10_LOGW_C("C10::validateProfile", "validateE10 failed");
        return false;
    }
    if (!validateMacros(p_p.macros)) {
        D10_LOGW_C("C10::validateProfile", "validateMacros failed");
        return false;
    }
    if (!validateSlots(p_p.slots, p_p.macros.count)) {
        D10_LOGW_C("C10::validateProfile", "validateSlots failed");
        return false;
    }
    return true;
}
