#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : HW_Def_0415.h
 * 모듈약어 : HW
 * 모듈명 : Hardware Pin Definitions (Single Source of Truth)
 * ------------------------------------------------------
 * 기능 요약
 *  - 모든 GPIO 핀의 단일 정의 위치 (매직넘버 5중복 제거)
 *  - E10(센서/I2C), C20(버튼), P20(전원/WoM), L10(LED)이 참조
 *  - EXT1 wake mask를 constexpr로 자동 계산 (하드코딩 제거)
 *
 * [설계]
 *  - namespace HW_DEF: 모듈약어 접두사 규칙 준수
 *  - 상수 이름은 C20 EN_C20_BtnId_t 순서와 1:1 (TOP_L/TOP_M/TOP_R/SIDE_F/SIDE_C/SIDE_R)
 *  - BTN_PINS[] 배열이 C20_BtnDispatcher::G_PINS와 동일 순서
 *  - buildWakeMaskAll()/buildWakeMaskButtons()로 EXT1 mask 자동 생성
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
 *   - namespace 내 상수    : 모둘약어 접두시 미사용
 *   - 전역 상수,매크로      : G_모듈약어_ 접두사
 *   - 함수 로컬 변수        : v_ 접두사
 *   - 함수 인자             : p_ 접두사
 * ------------------------------------------------------
 */

#include <Arduino.h>

namespace HW_DEF {

// =====================================================
// [버튼 GPIO] C20_BtnDispatcher::EN_C20_BtnId_t 순서와 1:1
// =====================================================
static constexpr int PIN_BTN_TOP_L  = 12;   // EN_C20_BTN_TOP_L  (좌클릭/Drag)
static constexpr int PIN_BTN_TOP_M  = 16;   // EN_C20_BTN_TOP_M  (Move Gate)
static constexpr int PIN_BTN_TOP_R  = 15;   // EN_C20_BTN_TOP_R  (우클릭)
static constexpr int PIN_BTN_SIDE_F = 14;   // EN_C20_BTN_SIDE_F (Front Hold)
static constexpr int PIN_BTN_SIDE_C = 13;   // EN_C20_BTN_SIDE_C (Mode/Pairing/Host Cycle)
static constexpr int PIN_BTN_SIDE_R = 7;    // EN_C20_BTN_SIDE_R (보조)

static constexpr uint8_t BTN_COUNT = 6;

// C20_BtnDispatcher::G_PINS 순서와 1:1 (EN_C20_BtnId_t 순서)
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
// =====================================================
static constexpr int PIN_BATT_ADC = 1;

// =====================================================
// [EXT1 wake mask 빌더] (constexpr, 매직넘버 제거)
// -------------------------------------------------------
// ESP32 EXT1 wake는 LOW active, 64-bit mask 사용
//  - buildWakeMaskAll()     : MPU INT + 6버튼 (Light-sleep)
//  - buildWakeMaskButtons() : 6버튼만 (Deep-sleep)
// =====================================================
static constexpr uint64_t buildWakeMaskAll() {
    uint64_t v = (1ULL << PIN_MPU_INT);
    for (uint8_t i = 0; i < BTN_COUNT; i++) {
        v |= (1ULL << BTN_PINS[i]);
    }
    return v;
}

static constexpr uint64_t buildWakeMaskButtons() {
    uint64_t v = 0;
    for (uint8_t i = 0; i < BTN_COUNT; i++) {
        v |= (1ULL << BTN_PINS[i]);
    }
    return v;
}

} // namespace HW_DEF
