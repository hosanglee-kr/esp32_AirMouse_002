// =======================================================
// File: src/v0415/C20_Action_0415.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : C20_Action_0415.h
 * 모듈약어 : C20
 * 모듈명 : Action Registry (v0415 3-Mode 통합)
 * ------------------------------------------------------
 * 기능 요약
 *  - Action Kind / Special / Button Event / Button ID enum
 *  - ST_ActionSlot_t (8 bytes) 정의
 *  - Action 파라미터 인코딩/디코딩 helper
 *
 * [v0415 Critical Fix — Consumer Mask Descriptor 정합]
 *  - EN_C20_Consumer_t 전면 재정의
 *  - ESP32-BLE-CompositeHID의 _mediakeysHIDReportDescriptor의
 *    Report ID 0x43 (24-bit Consumer Report) 비트 순서와 1:1 매핑
 *  - 이전 0412 정의는 16개 중 14개가 잘못된 비트 위치
 *    (예: EN_C20_CON_VOL_UP=0x0001 → Descriptor의 Bit 0은 "Play")
 *  - 실제 영향: 기본 프로파일 5개 중 3개, TV 모드 대부분 오작동
 *
 * [v0415 명칭 정정]
 *  - AC_BACK  → WWW_BACK  (HID Usage 0x0224)
 *  - AC_HOME  → WWW_HOME  (HID Usage 0x0223)
 *  - AC_SEARCH→ WWW_SEARCH(HID Usage 0x0221)
 *  - POWER/TV_INPUT/CH_UP/CH_DOWN 삭제 (Descriptor 대응 없음)
 *  - EN_C20_KB_POWER (0x66) 추가: TV Power 대체 (Q1-b 확정)
 *
 * [Descriptor SSOT]
 *  Bit  0: 0xB0 Play
 *  Bit  1: 0xB1 Pause
 *  Bit  2: 0xB2 Record
 *  Bit  3: 0xB3 Fast Forward
 *  Bit  4: 0xB4 Rewind
 *  Bit  5: 0xB5 Scan Next Track
 *  Bit  6: 0xB6 Scan Previous Track
 *  Bit  7: 0xB7 Stop
 *  Bit  8: 0xB8 Eject
 *  Bit  9: 0xB9 Random Play
 *  Bit 10: 0xBC Repeat
 *  Bit 11: 0xCD Play/Pause
 *  Bit 12: 0xE2 Mute
 *  Bit 13: 0xE9 Volume Increment
 *  Bit 14: 0xEA Volume Decrement
 *  Bit 15: 0x0223 WWW Home (AC Home)
 *  Bit 16: 0x0194 My Computer
 *  Bit 17: 0x0192 Calculator
 *  Bit 18: 0x022A WWW Favorites
 *  Bit 19: 0x0221 WWW Search (AC Search)
 *  Bit 20: 0x0226 WWW Stop
 *  Bit 21: 0x0224 WWW Back (AC Back)
 *  Bit 22: 0x0183 Media Select
 *  Bit 23: 0x018A Mail
 *
 *  [주의] Report Count = 24 → Bit 24~31은 전송되지 않음.
 *         validateSlot()에서 상한(0x00FFFFFF) 검증 필수.
 *
 * [마이그레이션]
 *  - 스키마 410 → 411
 *  - 매핑 테이블: C10_Config_0415.cpp의 _migrateConsumerFromV410()
 *  - POWER/TV_INPUT/CH_UP/CH_DOWN은 NONE으로 이관 (Descriptor 미지원)
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
    EN_C20_ACT_NONE = 0,

    // 마우스
    EN_C20_ACT_MOUSE_CLICK = 1, // p16 = mask, 1회 탭
    EN_C20_ACT_MOUSE_HOLD  = 2, // p16 = mask, down~up 유지
    EN_C20_ACT_MOUSE_WHEEL = 3, // p16 = axis(bit8-11)|dir(bit0-3)

    // 키보드
    EN_C20_ACT_KB_TAP    = 4, // p16 = usage, p32 = modifier
    EN_C20_ACT_KB_COMBO  = 5, // p32 = mod|u1<<8|u2<<16|u3<<24
    EN_C20_ACT_KB_REPEAT = 6, // p16 = usage, p32 = modifier, 반복

    // 컨슈머
    EN_C20_ACT_CONSUMER_TAP    = 7, // p32 = 32-bit mask, 1회
    EN_C20_ACT_CONSUMER_REPEAT = 8, // p32 = mask, 반복

    // 특수
    EN_C20_ACT_SPECIAL = 9, // p16 = EN_C20_Special_t

    // 매크로 [v0412 신규]
    EN_C20_ACT_MACRO = 10, // param32 = 매크로 인덱스 (0~7)

    EN_C20_ACT_MAX
};

// ======================================================
// 2) Special Action
// ======================================================
enum EN_C20_Special_t : uint8_t {
    EN_C20_SP_NONE         = 0,
    EN_C20_SP_GYRO_RECALIB = 1,
    EN_C20_SP_SLEEP_NOW    = 2,
    EN_C20_SP_MODE_CYCLE   = 3, // 내부용
    EN_C20_SP_PAIRING      = 4, // 내부용
    EN_C20_SP_HOST_CYCLE   = 5, // 내부용
    EN_C20_SP_MAX
};

// ======================================================
// 3) 물리 버튼 ID (6버튼)
// ======================================================
enum EN_C20_BtnId_t : uint8_t {
    EN_C20_BTN_TOP_L  = 0, // GPIO 12
    EN_C20_BTN_TOP_M  = 1, // GPIO 16
    EN_C20_BTN_TOP_R  = 2, // GPIO 15
    EN_C20_BTN_SIDE_F = 3, // GPIO 14
    EN_C20_BTN_SIDE_C = 4, // GPIO 13
    EN_C20_BTN_SIDE_R = 5, // GPIO 7
    EN_C20_BTN_MAX
};

// ======================================================
// 4) 버튼 이벤트 (dispatcher 산출)
// ======================================================
enum EN_C20_BtnEvent_t : uint8_t {
    EN_C20_EVT_NONE    = 0,
    EN_C20_EVT_DOWN    = 1,
    EN_C20_EVT_UP      = 2,
    EN_C20_EVT_CLICK   = 3, // down-up < 300ms, 재입력 없음
    EN_C20_EVT_DOUBLE  = 4, // 300ms 내 2회
    EN_C20_EVT_LONG    = 5, // down 800ms 유지 (release 전 발동)
    EN_C20_EVT_HOLD_2S = 6, // Side Center 페어링
    EN_C20_EVT_HOLD_3S = 7, // Multi-Host 전환
    EN_C20_EVT_MAX
};

// ======================================================
// 5) Wheel 방향 인코딩 (p16)
//   p16 = (axis << 8) | dir
//   axis: 0=Y(수직), 1=X(수평)
//   dir : 0=Up/Left, 1=Down/Right
// ======================================================
enum EN_C20_WheelAxis_t : uint8_t {
    EN_C20_WHEEL_Y = 0, // 수직 휠
    EN_C20_WHEEL_X = 1  // 수평 (AC Pan)
};
enum EN_C20_WheelDir_t : uint8_t {
    EN_C20_WHEEL_UP   = 0, // Y: Up / X: Left
    EN_C20_WHEEL_DOWN = 1  // Y: Down / X: Right
};

static inline uint16_t C20_EncodeWheel(uint8_t p_axis, uint8_t p_dir) {
    return (uint16_t)(((uint16_t)p_axis << 8) | (uint16_t)(p_dir & 0x01));
}
static inline uint8_t C20_WheelAxis(uint16_t p16) {
    return (uint8_t)(p16 >> 8);
}
static inline uint8_t C20_WheelDir(uint16_t p16) {
    return (uint8_t)(p16 & 0xFF);
}

// ======================================================
// 6) 마우스 버튼 mask (5버튼)
// ======================================================
enum EN_C20_MouseMask_t : uint8_t {
    EN_C20_M_L = 0x01,
    EN_C20_M_R = 0x02,
    EN_C20_M_M = 0x04,
    EN_C20_M_B = 0x08, // Back
    EN_C20_M_F = 0x10  // Forward
};

// ======================================================
// 7) Action Slot (8 bytes)
// ======================================================
struct ST_C20_ActionSlot_t {
    uint8_t  kind;     // EN_C20_ActionKind_t
    uint8_t  holdMode; // 0=NONE, 1=PRESS_HOLD, 2=REPEAT
    uint16_t param16;
    uint32_t param32;
};

// holdMode 상수
enum EN_C20_HoldMode_t : uint8_t { EN_C20_HOLD_NONE = 0, EN_C20_HOLD_PRESS = 1, EN_C20_HOLD_REPEAT = 2 };

// ======================================================
// 8) Slot 편의 생성자
// ======================================================
static inline ST_C20_ActionSlot_t C20_MakeNone() {
    ST_C20_ActionSlot_t s = {EN_C20_ACT_NONE, EN_C20_HOLD_NONE, 0, 0};
    return s;
}

static inline ST_C20_ActionSlot_t C20_MakeMouseClick(uint8_t p_mask) {
    ST_C20_ActionSlot_t s = {EN_C20_ACT_MOUSE_CLICK, EN_C20_HOLD_NONE, (uint16_t)p_mask, 0};
    return s;
}
static inline ST_C20_ActionSlot_t C20_MakeMouseHold(uint8_t p_mask) {
    ST_C20_ActionSlot_t s = {EN_C20_ACT_MOUSE_HOLD, EN_C20_HOLD_PRESS, (uint16_t)p_mask, 0};
    return s;
}
static inline ST_C20_ActionSlot_t C20_MakeWheel(uint8_t p_axis, uint8_t p_dir) {
    ST_C20_ActionSlot_t s = {EN_C20_ACT_MOUSE_WHEEL, EN_C20_HOLD_NONE, C20_EncodeWheel(p_axis, p_dir), 0};
    return s;
}
static inline ST_C20_ActionSlot_t C20_MakeKbTap(uint8_t p_mod, uint8_t p_usage) {
    ST_C20_ActionSlot_t s = {EN_C20_ACT_KB_TAP, EN_C20_HOLD_NONE, (uint16_t)p_usage, (uint32_t)p_mod};
    return s;
}
static inline ST_C20_ActionSlot_t C20_MakeKbRepeat(uint8_t p_mod, uint8_t p_usage) {
    ST_C20_ActionSlot_t s = {EN_C20_ACT_KB_REPEAT, EN_C20_HOLD_REPEAT, (uint16_t)p_usage, (uint32_t)p_mod};
    return s;
}
static inline ST_C20_ActionSlot_t C20_MakeConsumer(uint32_t p_mask) {
    ST_C20_ActionSlot_t s = {EN_C20_ACT_CONSUMER_TAP, EN_C20_HOLD_NONE, 0, p_mask};
    return s;
}
static inline ST_C20_ActionSlot_t C20_MakeConsumerRepeat(uint32_t p_mask) {
    ST_C20_ActionSlot_t s = {EN_C20_ACT_CONSUMER_REPEAT, EN_C20_HOLD_REPEAT, 0, p_mask};
    return s;
}
static inline ST_C20_ActionSlot_t C20_MakeSpecial(uint8_t p_special) {
    ST_C20_ActionSlot_t s = {EN_C20_ACT_SPECIAL, EN_C20_HOLD_NONE, (uint16_t)p_special, 0};
    return s;
}

static inline ST_C20_ActionSlot_t C20_MakeMacro(uint8_t p_macroIdx) {
    ST_C20_ActionSlot_t s = {EN_C20_ACT_MACRO, EN_C20_HOLD_NONE, 0, (uint32_t)p_macroIdx};
    return s;
}

// ======================================================
// 9) KB usage / modifier 상수
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
    // [v0415 신규] HID Keyboard Page Power (TV 전원 대체, Q1-b 확정)
    EN_C20_KB_POWER     = 0x66,
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
    EN_C20_KB_A         = 0x04,
    EN_C20_KB_B         = 0x05,
    EN_C20_KB_C         = 0x06,
    EN_C20_KB_D         = 0x07,
    EN_C20_KB_E         = 0x08,
    EN_C20_KB_F         = 0x09,
    EN_C20_KB_G         = 0x0A,
    EN_C20_KB_H         = 0x0B,
    EN_C20_KB_I         = 0x0C,
    EN_C20_KB_J         = 0x0D,
    EN_C20_KB_K         = 0x0E,
    EN_C20_KB_L         = 0x0F,
    EN_C20_KB_M         = 0x10,
    EN_C20_KB_N         = 0x11,
    EN_C20_KB_O         = 0x12,
    EN_C20_KB_P         = 0x13,
    EN_C20_KB_Q         = 0x14,
    EN_C20_KB_R         = 0x15,
    EN_C20_KB_S         = 0x16,
    EN_C20_KB_T         = 0x17,
    EN_C20_KB_U         = 0x18,
    EN_C20_KB_V         = 0x19,
    EN_C20_KB_W         = 0x1A,
    EN_C20_KB_X         = 0x1B,
    EN_C20_KB_Y         = 0x1C,
    EN_C20_KB_Z         = 0x1D
};

// ======================================================
// 10) Consumer mask (HID Consumer Page, 24-bit)
//     ★ [v0415 Critical Fix] Descriptor SSOT 정합 ★
// ------------------------------------------------------
//  - ESP32-BLE-CompositeHID의 _mediakeysHIDReportDescriptor
//    (Report ID 0x43, Report Size 1bit, Report Count 24)의
//    USAGE 순서와 1:1 매핑.
//  - 비트 위치 = Descriptor의 USAGE 등록 순서.
//  - Report Count = 24 → Bit 24 이상은 전송되지 않음.
//    (validateSlot에서 상한 검증 필수)
//
//  [이전 0412 정의와의 관계]
//  - 0412: 자체 16-bit 압축 (16개 중 14개가 Descriptor와 불일치)
//  - 0415: Descriptor 24-bit 순서 정합 (100% 일치)
//  - 마이그레이션: C10_Config_0415.cpp::_migrateConsumerFromV410()
// ======================================================
enum EN_C20_Consumer_t : uint32_t {
    EN_C20_CON_NONE       = 0x00000000,

    // ---- 표준 미디어 제어 (Bit 0~14) ----
    EN_C20_CON_PLAY       = 0x00000001,   // Bit 0 : 0xB0 Play
    EN_C20_CON_PAUSE      = 0x00000002,   // Bit 1 : 0xB1 Pause
    EN_C20_CON_RECORD     = 0x00000004,   // Bit 2 : 0xB2 Record
    EN_C20_CON_FF         = 0x00000008,   // Bit 3 : 0xB3 Fast Forward
    EN_C20_CON_REWIND     = 0x00000010,   // Bit 4 : 0xB4 Rewind
    EN_C20_CON_NEXT_TRACK = 0x00000020,   // Bit 5 : 0xB5 Scan Next Track
    EN_C20_CON_PREV_TRACK = 0x00000040,   // Bit 6 : 0xB6 Scan Previous Track
    EN_C20_CON_STOP       = 0x00000080,   // Bit 7 : 0xB7 Stop
    EN_C20_CON_EJECT      = 0x00000100,   // Bit 8 : 0xB8 Eject
    EN_C20_CON_RANDOM     = 0x00000200,   // Bit 9 : 0xB9 Random Play
    EN_C20_CON_REPEAT     = 0x00000400,   // Bit 10: 0xBC Repeat
    EN_C20_CON_PLAY_PAUSE = 0x00000800,   // Bit 11: 0xCD Play/Pause
    EN_C20_CON_MUTE       = 0x00001000,   // Bit 12: 0xE2 Mute
    EN_C20_CON_VOL_UP     = 0x00002000,   // Bit 13: 0xE9 Volume Increment
    EN_C20_CON_VOL_DOWN   = 0x00004000,   // Bit 14: 0xEA Volume Decrement

    // ---- 애플리케이션 제어 (Bit 15~23) ----
    EN_C20_CON_WWW_HOME   = 0x00008000,   // Bit 15: 0x0223 (AC Home)
    EN_C20_CON_MY_COMP    = 0x00010000,   // Bit 16: 0x0194
    EN_C20_CON_CALC       = 0x00020000,   // Bit 17: 0x0192
    EN_C20_CON_WWW_FAV    = 0x00040000,   // Bit 18: 0x022A
    EN_C20_CON_WWW_SEARCH = 0x00080000,   // Bit 19: 0x0221 (AC Search)
    EN_C20_CON_WWW_STOP   = 0x00100000,   // Bit 20: 0x0226
    EN_C20_CON_WWW_BACK   = 0x00200000,   // Bit 21: 0x0224 (AC Back)
    EN_C20_CON_MEDIA_SEL  = 0x00400000,   // Bit 22: 0x0183
    EN_C20_CON_MAIL       = 0x00800000,   // Bit 23: 0x018A

    // =====================================================
    // [삭제됨 - Descriptor 대응 없음]
    //   EN_C20_CON_POWER    → EN_C20_KB_POWER (Keyboard Page 0x66) 사용
    //   EN_C20_CON_TV_INPUT → NONE (또는 WWW_HOME 대체)
    //   EN_C20_CON_CH_UP    → NONE
    //   EN_C20_CON_CH_DOWN  → NONE
    //   EN_C20_CON_AC_BACK  → EN_C20_CON_WWW_BACK (rename)
    //   EN_C20_CON_AC_HOME  → EN_C20_CON_WWW_HOME (rename)
    //   EN_C20_CON_AC_SEARCH→ EN_C20_CON_WWW_SEARCH (rename)
    // =====================================================
};

// ------------------------------------------------------
// [v0415] Consumer Report 상한 (24-bit)
//   - validateSlot() / validateMacroStep() 에서 검증
// ------------------------------------------------------
static constexpr uint32_t G_C20_CONSUMER_MASK_MAX = 0x00FFFFFFu;

// ------------------------------------------------------
// [v0415] Legacy 0412 Consumer 값 → 0415 값 매핑 테이블
//   - C10_Config_0415.cpp의 마이그레이션에서 사용
//   - 배열 index = 0412 값의 비트 위치 (0~15)
//   - 값 0은 매핑 없음(NONE)
// ------------------------------------------------------
static constexpr uint32_t G_C20_LEGACY_410_TO_415[16] = {
    /* idx 0 : 0x0001 (old VOL_UP)     */ (uint32_t)EN_C20_CON_VOL_UP,
    /* idx 1 : 0x0002 (old VOL_DOWN)   */ (uint32_t)EN_C20_CON_VOL_DOWN,
    /* idx 2 : 0x0004 (old MUTE)       */ (uint32_t)EN_C20_CON_MUTE,
    /* idx 3 : 0x0008 (old PLAY_PAUSE) */ (uint32_t)EN_C20_CON_PLAY_PAUSE,
    /* idx 4 : 0x0010 (old STOP)       */ (uint32_t)EN_C20_CON_STOP,
    /* idx 5 : 0x0020 (old NEXT_TRACK) */ (uint32_t)EN_C20_CON_NEXT_TRACK,
    /* idx 6 : 0x0040 (old PREV_TRACK) */ (uint32_t)EN_C20_CON_PREV_TRACK,
    /* idx 7 : 0x0080 (old FF)         */ (uint32_t)EN_C20_CON_FF,
    /* idx 8 : 0x0100 (old REWIND)     */ (uint32_t)EN_C20_CON_REWIND,
    /* idx 9 : 0x0200 (old AC_BACK)    */ (uint32_t)EN_C20_CON_WWW_BACK,
    /* idx 10: 0x0400 (old AC_HOME)    */ (uint32_t)EN_C20_CON_WWW_HOME,
    /* idx 11: 0x0800 (old AC_SEARCH)  */ (uint32_t)EN_C20_CON_WWW_SEARCH,
    /* idx 12: 0x1000 (old POWER)      */ 0u,   // Descriptor 미지원 → NONE
    /* idx 13: 0x2000 (old TV_INPUT)   */ 0u,   // Descriptor 미지원 → NONE
    /* idx 14: 0x4000 (old CH_UP)      */ 0u,   // Descriptor 미지원 → NONE
    /* idx 15: 0x8000 (old CH_DOWN)    */ 0u,   // Descriptor 미지원 → NONE
};

// ======================================================
// 11) Combo 인코딩 (KB_COMBO용)
//   p32 = mod | (u1 << 8) | (u2 << 16) | (u3 << 24)
// ======================================================
static inline uint32_t C20_EncodeCombo(uint8_t p_mod, uint8_t p_u1, uint8_t p_u2 = 0, uint8_t p_u3 = 0) {
    return (uint32_t)p_mod | ((uint32_t)p_u1 << 8) | ((uint32_t)p_u2 << 16) | ((uint32_t)p_u3 << 24);
}
static inline uint8_t C20_ComboMod(uint32_t p32) {
    return (uint8_t)(p32 & 0xFF);
}
static inline uint8_t C20_ComboU1(uint32_t p32) {
    return (uint8_t)((p32 >> 8) & 0xFF);
}
static inline uint8_t C20_ComboU2(uint32_t p32) {
    return (uint8_t)((p32 >> 16) & 0xFF);
}
static inline uint8_t C20_ComboU3(uint32_t p32) {
    return (uint8_t)((p32 >> 24) & 0xFF);
}
