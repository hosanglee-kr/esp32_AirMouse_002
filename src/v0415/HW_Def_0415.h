// File: src/v0415/HW_Def_0415.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : HW_Def_0415.h
 * 모듈약어 : HW
 * 모듈명 : Hardware Pin Definitions (Single Source of Truth)
 * ------------------------------------------------------
 * 기능 요약
 *  - 모든 GPIO 핀의 단일 정의 위치 (매직넘버 다중 중복 제거)
 *  - E10(센서/I2C), C20(버튼), P20(전원/WoM), L10(LED)이 참조
 *  - EXT1 wake mask를 constexpr로 자동 계산 (하드코딩 제거)
 *
 * [v0415 주요 변경 — SPEC rev8 명명 정합]
 *  - buildWakeMaskAll()     → buildWakeMaskNormal()   (rename)
 *  - buildWakeMaskSafe()    → 신설 (Safe/Pairing 모드)
 *    · SPEC rev8: "안전/페어링 모드: Side C 단독"
 *    · Phase 1 방안 2-A 확정
 *  - buildWakeMaskButtons() → 유지 (Deep-sleep: MPU INT 제외)
 *  - PIN_BATTERY_ADC → PIN_BATT_ADC (명명 통일, SPEC 표와 일치)
 *
 * [SSOT 계약]
 *  - 본 파일이 GPIO 핀 번호, RTC Wakeup 마스크, BTN_PINS 순서의 유일한 정의
 *  - 타 모듈은 로컬 매크로/상수로 재정의 금지 (CONTRACT rev7 §"HW SSOT")
 *  - BTN_PINS[] 순서는 EN_C20_BtnId_t 순서와 반드시 1:1 유지
 *  - C20_BtnDispatcher::G_PINS는 본 배열을 참조해야 함
 *
 * [EXT1 Wake Mask 정책 — Phase 1 방안 2-A]
 *   ┌──────────────────┬──────────────────────────┐
 *   │ 시나리오         │ 사용 마스크              │
 *   ├──────────────────┼──────────────────────────┤
 *   │ Light-sleep 일반 │ buildWakeMaskNormal()    │
 *   │ Light-sleep Safe │ buildWakeMaskSafe()      │
 *   │ Light-sleep Pair │ buildWakeMaskSafe()      │
 *   │ Deep-sleep 일반  │ buildWakeMaskButtons()   │
 *   │ Deep-sleep Safe  │ buildWakeMaskSafe()      │
 *   └──────────────────┴──────────────────────────┘
 *   → 세부 분기는 P20_Power_0415.cpp의 _armExt1() / deepSleepNow()가 담당
 *
 * [하드웨어 배치]
 *        [Top 면]
 *     [L]   [M]   [R]
 *
 *   ┌─────────────┐
 * [F]│             │
 * [C]│             │  ← 좌측면 (Front/Center/Rear)
 * [R]│             │
 *   └─────────────┘
 * ------------------------------------------------------
 * [구현 규칙]
 *  - 항상 소스 시작 주석 부분 체계 유지 및 내용 업데이트
 *  - 소스 시작 주석 부분 구현규칙, 코드네이밍규칙 내용 그대로 유지, 수정금지
 *  - ArduinoJson v7.x.x 사용 (v6 이하 사용 금지)
 *  - memset + strlcpy 기반 안전 초기화
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 *   - namespace 명        : 모듈약어_ 접두사
 *   - namespace 내 상수    : 모듈약어 접두사 미사용
 *   - 전역 상수,매크로      : G_모듈약어_ 접두사
 *   - 함수 로컬 변수        : v_ 접두사
 *   - 함수 인자             : p_ 접두사
 * ------------------------------------------------------
 */

#include <Arduino.h>

namespace HW_DEF {

// =====================================================
// [버튼 GPIO] C20_BtnDispatcher::EN_C20_BtnId_t 순서와 1:1
// ------------------------------------------------------
//  EN_C20_BtnId_t 순서 (C20_Action_0415.h):
//    0 = TOP_L, 1 = TOP_M, 2 = TOP_R,
//    3 = SIDE_F, 4 = SIDE_C, 5 = SIDE_R
// =====================================================
static constexpr int PIN_BTN_TOP_L  = 12;   // EN_C20_BTN_TOP_L  (좌클릭/Drag)
static constexpr int PIN_BTN_TOP_M  = 16;   // EN_C20_BTN_TOP_M  (Move Gate)
static constexpr int PIN_BTN_TOP_R  = 15;   // EN_C20_BTN_TOP_R  (우클릭)
static constexpr int PIN_BTN_SIDE_F = 14;   // EN_C20_BTN_SIDE_F (Front Hold)
static constexpr int PIN_BTN_SIDE_C = 13;   // EN_C20_BTN_SIDE_C (Mode/Pairing/Host Cycle)
static constexpr int PIN_BTN_SIDE_R = 7;    // EN_C20_BTN_SIDE_R (보조)

static constexpr uint8_t BTN_COUNT = 6;

// C20_BtnDispatcher::G_PINS 순서와 1:1 (EN_C20_BtnId_t 순서)
//   - C20_Action_0415.h의 EN_C20_BtnId_t 순서와 반드시 일치
//   - 정적 검증: Round E에서 C20_BtnDispatcher::G_PINS가 이 배열 참조
static constexpr int BTN_PINS[BTN_COUNT] = {
    PIN_BTN_TOP_L,
    PIN_BTN_TOP_M,
    PIN_BTN_TOP_R,
    PIN_BTN_SIDE_F,
    PIN_BTN_SIDE_C,
    PIN_BTN_SIDE_R,
};

// =====================================================
// [I2C 버스]
// =====================================================
static constexpr int PIN_I2C_SDA = 4;
static constexpr int PIN_I2C_SCL = 5;

// =====================================================
// [MPU6050 INT1] WoM wake 소스
// =====================================================
static constexpr int PIN_MPU_INT = 6;

// =====================================================
// [LED] WS2812 단일 LED (DIN)
// =====================================================
static constexpr int PIN_LED_WS2812 = 21;

// =====================================================
// [배터리 ADC] (미구현, 향후)
//   - v0412 명칭: PIN_BATTERY_ADC
//   - v0415 명칭: PIN_BATT_ADC (SPEC 표 정합)
// =====================================================
static constexpr int PIN_BATT_ADC = 1;

// =====================================================
// [EXT1 wake mask 빌더] (constexpr, 매직넘버 제거)
// -------------------------------------------------------
// ESP32 EXT1 wake는 LOW active, 64-bit mask 사용
//
//   buildWakeMaskNormal()  : MPU INT + 6버튼 (7소스) — Light-sleep 일반
//   buildWakeMaskSafe()    : Side C 단독 (1소스) — Safe/Pairing 모드
//   buildWakeMaskButtons() : 6버튼 (MPU 제외)   — Deep-sleep 일반
//
//  ※ 세 함수는 모두 ESP_EXT1_WAKEUP_ANY_LOW와 함께 사용.
//  ※ Side C는 부팅 시 6초 hold 팩토리 리셋의 트리거이기도 하므로
//    Safe 모드에서 단독 wake 소스로 지정해도 브릭 복구 가능.
// =====================================================
static constexpr uint64_t buildWakeMaskNormal() {
    uint64_t v = (1ULL << PIN_MPU_INT);
    for (uint8_t i = 0; i < BTN_COUNT; i++) {
        v |= (1ULL << BTN_PINS[i]);
    }
    return v;
}

static constexpr uint64_t buildWakeMaskSafe() {
    return (1ULL << PIN_BTN_SIDE_C);
}

static constexpr uint64_t buildWakeMaskButtons() {
    uint64_t v = 0;
    for (uint8_t i = 0; i < BTN_COUNT; i++) {
        v |= (1ULL << BTN_PINS[i]);
    }
    return v;
}


} // namespace HW_DEF
