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
 * [v0415 주요 변경 — SPEC rev8 명명 정합 및 핀 재배치]
 *  - buildWakeMaskAll()     → buildWakeMaskNormal()   (rename)
 *  - buildWakeMaskSafe()    → 신설 (Safe/Pairing 모드: Side C 단독)
 *  - buildWakeMaskButtons() → 유지 (Deep-sleep: MPU INT 제외)
 *  - PIN_BATTERY_ADC        → PIN_BATT_ADC (명명 통일, SPEC 표와 일치)
 *  - ESP32-S3-Zero 전용 재매핑: GPIO 13 이하 핀만 사용 (GP14~16 배제)
 *  - GP11 검증 반영: SPI Flash(GP26~32) 간섭 없음 확인 완료, PIN_BTN_SIDE_R로 복귀
 *  - 우측 핀헤더(GP7~GP13) 결선 최적화: Side 3버튼 및 Top 3버튼 순차 연속 배치
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
 *
 * =======================================================================================================
 * [Waveshare ESP32-S3-Zero 좌/우 대칭 핀아웃 다이어그램 ]
 * =======================================================================================================
 *  - 좌측 핀: 전원(5V, GND, 3V3) 및 아날로그 ADC / I2C / 인터럽트 센서 계통
 *  - 우측 핀: UART0 디버그 핀 및 조작 버튼(Top 3개, Side 3개) 계통
 * =======================================================================================================
 *
 *                                               ┌───────────────┐
 *                                               │   USB-C 포트  │
 *                                               └───────┬───────┘
 *                                                       │
 *                        ┌──────────────────────────────┴──────────────────────────────┐
 *                        │                                                             │
 *  PIN_5V_IN   [전원] ── │ [ 5V ]                                             [ TX ]   │ ── [통신] UART0 TX (GP43)
 *  PIN_GND     [전원] ── │ [GND ]                                             [ RX ]   │ ── [통신] UART0 RX (GP44)
 *  PIN_3V3_OUT [전원] ── │ [3V3 ]                                             [ 13 ]   │ ── [할당] PIN_BTN_SIDE_C (GP13)
 *  PIN_BATT_ADC[할당] ── │ [ 1  ]                                             [ 12 ]   │ ── [할당] PIN_BTN_SIDE_F (GP12)
 *  (여유 핀)   [여유] ── │ [ 2  ]             ┌────────────────┐              [ 11 ]   │ ── [할당] PIN_BTN_SIDE_R (GP11)
 *  (여유 핀)   [여유] ── │ [ 3  ]             │  ESP32-S3-FH4R2│              [ 10 ]   │ ── [여유] (ADC1_CH9 / RTC)
 *  PIN_I2C_SDA [할당] ── │ [ 4  ]             │     (MCU)      │              [ 9  ]   │ ── [할당] PIN_BTN_TOP_L  (GP9)
 *  PIN_I2C_SCL [할당] ── │ [ 5  ]             └────────────────┘              [ 8  ]   │ ── [할당] PIN_BTN_TOP_M  (GP8)
 *  PIN_MPU_INT [할당] ── │ [ 6  ]                                             [ 7  ]   │ ── [할당] PIN_BTN_TOP_R  (GP7)
 *                        │                                                             │
 *                        └─────────────────────────────────────────────────────────────┘
 *
 *                        ───────────────────────────────────────────────────────────────
 *                        [온보드 내장 및 배제 핀]
 *                        - GP21 : [할당] PIN_LED_WS2812 (온보드 RGB LED)
 *                        - GP0  : [배제] Boot 스트래핑 핀 (미할당)
 *                        - GP14, GP15, GP16 : [배제] 하단/모서리 핀 (미할당)
 *                        ───────────────────────────────────────────────────────────────
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
static constexpr int PIN_BTN_TOP_L  = 9;    // GP9  (좌클릭/Drag)
static constexpr int PIN_BTN_TOP_M  = 8;    // GP8  (Move Gate)
static constexpr int PIN_BTN_TOP_R  = 7;    // GP7  (우클릭)
static constexpr int PIN_BTN_SIDE_F = 12;   // GP12 (Front Hold)
static constexpr int PIN_BTN_SIDE_C = 13;   // GP13 (Mode/Pairing/Host Cycle)
static constexpr int PIN_BTN_SIDE_R = 11;   // GP11 (보조)

static constexpr uint8_t BTN_COUNT = 6;

// C20_BtnDispatcher::G_PINS 순서와 1:1 대응 (EN_C20_BtnId_t 순서)
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
static constexpr int PIN_I2C_SDA = 4;       // GP4
static constexpr int PIN_I2C_SCL = 5;       // GP5

// =====================================================
// [MPU6050 INT1] WoM wake 소스
// =====================================================
static constexpr int PIN_MPU_INT = 6;       // GP6

// =====================================================
// [LED] WS2812 온보드 내장 RGB LED (DIN)
// =====================================================
static constexpr int PIN_LED_WS2812 = 21;   // GP21

// =====================================================
// [배터리 ADC]
// =====================================================
static constexpr int PIN_BATT_ADC = 1;      // GP1 (ADC1_CH0)

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
