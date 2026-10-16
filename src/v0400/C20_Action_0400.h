// =======================================================
// File: src/v0400/C20_Action_0400.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : C20_Action_0400.h
 * 모듈약어 : C20
 * 모듈명 : Action Registry (v0400 3-Mode 통합)
 * ------------------------------------------------------
 * 기능 요약
 *  - Action Kind / Special / Button Event / Button ID enum
 *  - ST_ActionSlot_t (8 bytes) 정의
 *  - Action 파라미터 인코딩/디코딩 helper
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

// ======================================================
// 1) Action Kind
// ======================================================
enum EN_C20_ActionKind_t : uint8_t {
    EN_C20_ACT_NONE            = 0,

    // 마우스
    EN_C20_ACT_MOUSE_CLICK     = 1,   // p16 = mask, 1회 탭
    EN_C20_ACT_MOUSE_HOLD      = 2,   // p16 = mask, down~up 유지
    EN_C20_ACT_MOUSE_WHEEL     = 3,   // p16 = axis(bit8-11)|dir(bit0-3)

    // 키보드
    EN_C20_ACT_KB_TAP          = 4,   // p16 = usage, p32 = modifier
    EN_C20_ACT_KB_COMBO        = 5,   // p32 = mod|u1<<8|u2<<16|u3<<24
    EN_C20_ACT_KB_REPEAT       = 6,   // p16 = usage, p32 = modifier, 반복

    // 컨슈머
    EN_C20_ACT_CONSUMER_TAP    = 7,   // p32 = 32-bit mask, 1회
    EN_C20_ACT_CONSUMER_REPEAT = 8,   // p32 = mask, 반복

    // 특수
    EN_C20_ACT_SPECIAL         = 9,   // p16 = EN_C20_Special_t

    EN_C20_ACT_MAX
};

// ======================================================
// 2) Special Action
// ======================================================
enum EN_C20_Special_t : uint8_t {
    EN_C20_SP_NONE           = 0,
    EN_C20_SP_GYRO_RECALIB   = 1,
    EN_C20_SP_SLEEP_NOW      = 2,
    EN_C20_SP_MODE_CYCLE     = 3,   // 내부용
    EN_C20_SP_PAIRING        = 4,   // 내부용
    EN_C20_SP_HOST_CYCLE     = 5,   // 내부용
    EN_C20_SP_MAX
};

// ======================================================
// 3) 물리 버튼 ID (6버튼)
// ======================================================
enum EN_C20_BtnId_t : uint8_t {
    EN_C20_BTN_TOP_L    = 0,   // GPIO 12
    EN_C20_BTN_TOP_M    = 1,   // GPIO 16
    EN_C20_BTN_TOP_R    = 2,   // GPIO 15
    EN_C20_BTN_SIDE_F   = 3,   // GPIO 14
    EN_C20_BTN_SIDE_C   = 4,   // GPIO 13
    EN_C20_BTN_SIDE_R   = 5,   // GPIO 7
    EN_C20_BTN_MAX
};

// ======================================================
// 4) 버튼 이벤트 (dispatcher 산출)
// ======================================================
enum EN_C20_BtnEvent_t : uint8_t {
    EN_C20_EVT_NONE       = 0,
    EN_C20_EVT_DOWN       = 1,
    EN_C20_EVT_UP         = 2,
    EN_C20_EVT_CLICK      = 3,   // down-up < 300ms, 재입력 없음
    EN_C20_EVT_DOUBLE     = 4,   // 300ms 내 2회
    EN_C20_EVT_LONG       = 5,   // down 800ms 유지 (release 전 발동)
    EN_C20_EVT_HOLD_2S    = 6,   // Side Center 페어링
    EN_C20_EVT_HOLD_3S    = 7,   // Multi-Host 전환
    EN_C20_EVT_MAX
};

// ======================================================
// 5) Wheel 방향 인코딩 (p16)
//   p16 = (axis << 8) | dir
//   axis: 0=Y(수직), 1=X(수평)
//   dir : 0=Up/Left, 1=Down/Right
// ======================================================
enum EN_C20_WheelAxis_t : uint8_t {
    EN_C20_WHEEL_Y = 0,   // 수직 휠
    EN_C20_WHEEL_X = 1    // 수평 (AC Pan)
};
enum EN_C20_WheelDir_t : uint8_t {
    EN_C20_WHEEL_UP    = 0,   // Y: Up / X: Left
    EN_C20_WHEEL_DOWN  = 1    // Y: Down / X: Right
};

static inline uint16_t C20_EncodeWheel(uint8_t p_axis, uint8_t p_dir) {
    return (uint16_t)(((uint16_t)p_axis << 8) | (uint16_t)(p_dir & 0x01));
}
static inline uint8_t C20_WheelAxis(uint16_t p16) { return (uint8_t)(p16 >> 8); }
static inline uint8_t C20_WheelDir (uint16_t p16) { return (uint8_t)(p16 & 0xFF); }

// ======================================================
// 6) 마우스 버튼 mask (5버튼)
// ======================================================
enum EN_C20_MouseMask_t : uint8_t {
    EN_C20_M_L = 0x01,
    EN_C20_M_R = 0x02,
    EN_C20_M_M = 0x04,
    EN_C20_M_B = 0x08,   // Back
    EN_C20_M_F = 0x10    // Forward
};

// ======================================================
// 7) Action Slot (8 bytes)
// ======================================================
struct ST_C20_ActionSlot_t {
    uint8_t  kind;      // EN_C20_ActionKind_t
    uint8_t  holdMode;  // 0=NONE, 1=PRESS_HOLD, 2=REPEAT
    uint16_t param16;
    uint32_t param32;
};

// holdMode 상수
enum EN_C20_HoldMode_t : uint8_t {
    EN_C20_HOLD_NONE   = 0,
    EN_C20_HOLD_PRESS  = 1,
    EN_C20_HOLD_REPEAT = 2
};

// ======================================================
// 8) Slot 편의 생성자
// ======================================================
static inline ST_C20_ActionSlot_t C20_MakeNone() {
    ST_C20_ActionSlot_t s = { EN_C20_ACT_NONE, EN_C20_HOLD_NONE, 0, 0 };
    return s;
}

static inline ST_C20_ActionSlot_t C20_MakeMouseClick(uint8_t p_mask) {
    ST_C20_ActionSlot_t s = { EN_C20_ACT_MOUSE_CLICK, EN_C20_HOLD_NONE,
                              (uint16_t)p_mask, 0 };
    return s;
}
static inline ST_C20_ActionSlot_t C20_MakeMouseHold(uint8_t p_mask) {
    ST_C20_ActionSlot_t s = { EN_C20_ACT_MOUSE_HOLD, EN_C20_HOLD_PRESS,
                              (uint16_t)p_mask, 0 };
    return s;
}
static inline ST_C20_ActionSlot_t C20_MakeWheel(uint8_t p_axis, uint8_t p_dir) {
    ST_C20_ActionSlot_t s = { EN_C20_ACT_MOUSE_WHEEL, EN_C20_HOLD_NONE,
                              C20_EncodeWheel(p_axis, p_dir), 0 };
    return s;
}
static inline ST_C20_ActionSlot_t C20_MakeKbTap(uint8_t p_mod, uint8_t p_usage) {
    ST_C20_ActionSlot_t s = { EN_C20_ACT_KB_TAP, EN_C20_HOLD_NONE,
                              (uint16_t)p_usage, (uint32_t)p_mod };
    return s;
}
static inline ST_C20_ActionSlot_t C20_MakeKbRepeat(uint8_t p_mod, uint8_t p_usage) {
    ST_C20_ActionSlot_t s = { EN_C20_ACT_KB_REPEAT, EN_C20_HOLD_REPEAT,
                              (uint16_t)p_usage, (uint32_t)p_mod };
    return s;
}
static inline ST_C20_ActionSlot_t C20_MakeConsumer(uint32_t p_mask) {
    ST_C20_ActionSlot_t s = { EN_C20_ACT_CONSUMER_TAP, EN_C20_HOLD_NONE,
                              0, p_mask };
    return s;
}
static inline ST_C20_ActionSlot_t C20_MakeConsumerRepeat(uint32_t p_mask) {
    ST_C20_ActionSlot_t s = { EN_C20_ACT_CONSUMER_REPEAT, EN_C20_HOLD_REPEAT,
                              0, p_mask };
    return s;
}
static inline ST_C20_ActionSlot_t C20_MakeSpecial(uint8_t p_special) {
    ST_C20_ActionSlot_t s = { EN_C20_ACT_SPECIAL, EN_C20_HOLD_NONE,
                              (uint16_t)p_special, 0 };
    return s;
}

// ======================================================
// 9) KB usage / modifier 상수 (자주 쓰는 것)
// ======================================================
enum EN_C20_Mod_t : uint8_t {
    EN_C20_MOD_NONE   = 0x00,
    EN_C20_MOD_LCTRL  = 0x01,
    EN_C20_MOD_LSHIFT = 0x02,
    EN_C20_MOD_LALT   = 0x04,
    EN_C20_MOD_LGUI   = 0x08,
    EN_C20_MOD_RCTRL  = 0x10,
    EN_C20_MOD_RSHIFT = 0x20,
    EN_C20_MOD_RALT   = 0x40,
    EN_C20_MOD_RGUI   = 0x80
};

enum EN_C20_KbUsage_t : uint16_t {
    EN_C20_KB_NONE      = 0x00,
    EN_C20_KB_ENTER     = 0x28,
    EN_C20_KB_ESC       = 0x29,
    EN_C20_KB_BACKSPACE = 0x2A,
    EN_C20_KB_TAB       = 0x2B,
    EN_C20_KB_SPACE     = 0x2C,
    EN_C20_KB_F5        = 0x3E,
    EN_C20_KB_F11       = 0x44,
    EN_C20_KB_F12       = 0x45,
    EN_C20_KB_PAGEUP    = 0x4B,
    EN_C20_KB_PAGEDOWN  = 0x4E,
    EN_C20_KB_RIGHT     = 0x4F,
    EN_C20_KB_LEFT      = 0x50,
    EN_C20_KB_DOWN      = 0x51,
    EN_C20_KB_UP        = 0x52,
    // 문자 (a~z)
    EN_C20_KB_A = 0x04, EN_C20_KB_B = 0x05, EN_C20_KB_C = 0x06,
    EN_C20_KB_D = 0x07, EN_C20_KB_E = 0x08, EN_C20_KB_F = 0x09,
    EN_C20_KB_G = 0x0A, EN_C20_KB_H = 0x0B, EN_C20_KB_I = 0x0C,
    EN_C20_KB_J = 0x0D, EN_C20_KB_K = 0x0E, EN_C20_KB_L = 0x0F,
    EN_C20_KB_M = 0x10, EN_C20_KB_N = 0x11, EN_C20_KB_O = 0x12,
    EN_C20_KB_P = 0x13, EN_C20_KB_Q = 0x14, EN_C20_KB_R = 0x15,
    EN_C20_KB_S = 0x16, EN_C20_KB_T = 0x17, EN_C20_KB_U = 0x18,
    EN_C20_KB_V = 0x19, EN_C20_KB_W = 0x1A, EN_C20_KB_X = 0x1B,
    EN_C20_KB_Y = 0x1C, EN_C20_KB_Z = 0x1D
};

// ======================================================
// 10) Consumer mask (HID Consumer page, 16-bit)
// ======================================================
enum EN_C20_Consumer_t : uint32_t {
    EN_C20_CON_NONE         = 0x00000000,
    EN_C20_CON_VOL_UP       = 0x00000001,
    EN_C20_CON_VOL_DOWN     = 0x00000002,
    EN_C20_CON_MUTE         = 0x00000004,
    EN_C20_CON_PLAY_PAUSE   = 0x00000008,
    EN_C20_CON_STOP         = 0x00000010,
    EN_C20_CON_NEXT_TRACK   = 0x00000020,
    EN_C20_CON_PREV_TRACK   = 0x00000040,
    EN_C20_CON_FF           = 0x00000080,
    EN_C20_CON_REWIND       = 0x00000100,
    EN_C20_CON_AC_BACK      = 0x00000200,
    EN_C20_CON_AC_HOME      = 0x00000400,
    EN_C20_CON_AC_SEARCH    = 0x00000800,
    EN_C20_CON_POWER        = 0x00001000,
    EN_C20_CON_TV_INPUT     = 0x00002000,
    EN_C20_CON_CH_UP        = 0x00004000,
    EN_C20_CON_CH_DOWN      = 0x00008000
};

// ======================================================
// 11) Combo 인코딩 (KB_COMBO용)
//   p32 = mod | (u1 << 8) | (u2 << 16) | (u3 << 24)
// ======================================================
static inline uint32_t C20_EncodeCombo(uint8_t p_mod, uint8_t p_u1,
                                       uint8_t p_u2 = 0, uint8_t p_u3 = 0) {
    return (uint32_t)p_mod
         | ((uint32_t)p_u1 << 8)
         | ((uint32_t)p_u2 << 16)
         | ((uint32_t)p_u3 << 24);
}
static inline uint8_t C20_ComboMod(uint32_t p32) { return (uint8_t)(p32 & 0xFF); }
static inline uint8_t C20_ComboU1 (uint32_t p32) { return (uint8_t)((p32 >> 8) & 0xFF); }
static inline uint8_t C20_ComboU2 (uint32_t p32) { return (uint8_t)((p32 >> 16) & 0xFF); }
static inline uint8_t C20_ComboU3 (uint32_t p32) { return (uint8_t)((p32 >> 24) & 0xFF); }
