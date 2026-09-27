// =======================================================
// File: src/v040/L10_Led_0400.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : L10_Led_0400.h
 * 모듈약어 : L10
 * 모듈명 : WS2812 LED Controller (단일)
 * ------------------------------------------------------
 * 기능 요약
 *  - ESP-IDF led_strip (RMT) 기반 단일 WS2812
 *  - Mode 색상 / Flash / Blink / Fadeout / Off
 *  - 사용자 밝기 (0~255)
 *  - 비동기 상태머신 (tick 기반)
 *
 * [상태머신]
 *   IDLE    : base 색 (Mode 색) 상시 점등
 *   FLASH   : 짧은 단발 점등 (사용자 액션 피드백)
 *   BLINK   : 주기적 on/off (페어링 대기)
 *   FADEOUT : 밝기 감소 후 off (슬립 진입)
 *
 * [tick 호출 주기]
 *   50ms 권장. 별도 저우선 태스크 또는 sensorTask 카운터.
 *
 * [GPIO]
 *   G_L10_PIN = 21 (RMT)
 * ------------------------------------------------------
 * [구현 규칙]
 *  - 항상 소스 시작 주석 부분 체계 유지 및 내용 업데이트
 *  - 소스 시작 주석 부분 구현규칙, 코드네이밍규칙 내용 그대로 유지, 수정금지
 *  - ArduinoJson v7.x.x 사용 (v6 이하 사용 금지)
 *  - memset + strlcpy 기반 안전 초기화
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 *   - 클래스명              : CL_모듈약어_ 접두사
 *   - 클래스 private 멤버   : _ 접두사
 *   - 전역 상수,매크로      : G_모듈약어_ 접두사
 *   - enum 상수             : EN_모듈약어_ 접두사
 *   - 함수 로컬 변수        : v_ 접두사
 *   - 함수 인자             : p_ 접두사
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <string.h>

#include "led_strip.h"

// -------------------------------------------------------
// GPIO
// -------------------------------------------------------
static constexpr int G_L10_PIN = 21;

// -------------------------------------------------------
// 논리 색상
// -------------------------------------------------------
enum EN_L10_Color_t : uint8_t {
    EN_L10_COLOR_OFF     = 0,
    EN_L10_COLOR_BLUE    = 1,   // Mode 1
    EN_L10_COLOR_GREEN   = 2,   // Mode 2
    EN_L10_COLOR_ORANGE  = 3,   // Mode 3
    EN_L10_COLOR_WHITE   = 4,   // flash
    EN_L10_COLOR_RED     = 5,   // 경고/슬립
};

class CL_L10_Led {
  public:
    CL_L10_Led();
    ~CL_L10_Led();

    // ====================================================
    // 초기화
    //   p_brightness: 사용자 밝기 (0~255). 0이면 off 상태로 시작
    // ====================================================
    void begin(uint8_t p_brightness = 128);

    // 사용자 밝기 갱신 (config 반영)
    void setBrightness(uint8_t p_b);

    // ====================================================
    // 상태 제어
    // ====================================================

    // Mode 색상 설정 (base 색). 진행 중인 flash/blink 즉시 종료.
    //   p_mode: 1/2/3
    void setModeColor(uint8_t p_mode);

    // 단발 flash (예: 사용자 액션 성공)
    //   p_color : flash 색
    //   p_ms    : 지속 시간
    void flash(EN_L10_Color_t p_color, uint16_t p_ms = 50);

    // Blink (예: 페어링 대기)
    //   p_periodMs  : on/off 반주기 (예: 500 = 1Hz)
    //   p_durationMs: 지속 시간 (0 = 수동 stopBlink까지 무한)
    void blink(EN_L10_Color_t p_color, uint16_t p_periodMs, uint16_t p_durationMs = 0);

    // 진행 중 blink 종료 → base 색 복귀
    void stopBlink();

    // Fadeout 후 off (슬립 진입 직전)
    //   완료 후 tick이 off 상태 유지
    void fadeout(EN_L10_Color_t p_color, uint16_t p_ms = 800);

    // 즉시 off
    void off();

    // ====================================================
    // 상태머신 tick (50ms 주기 권장)
    // ====================================================
    void tick();

    // 현재 base 색 (진단용)
    EN_L10_Color_t getBaseColor() const { return _baseColor; }
    uint8_t        getBrightness() const { return _brightness; }

  private:
    // ----------------------------------------------------
    // 내부 상태머신
    // ----------------------------------------------------
    enum EN_L10_State_t : uint8_t {
        EN_L10_ST_IDLE    = 0,
        EN_L10_ST_FLASH   = 1,
        EN_L10_ST_BLINK   = 2,
        EN_L10_ST_FADEOUT = 3,
        EN_L10_ST_OFF     = 4,
    };

    led_strip_handle_t _strip = nullptr;
    uint8_t            _brightness = 128;

    EN_L10_State_t _state       = EN_L10_ST_OFF;
    EN_L10_Color_t _baseColor   = EN_L10_COLOR_OFF;   // IDLE 시 색
    EN_L10_Color_t _evtColor    = EN_L10_COLOR_OFF;   // flash/blink/fade 색

    uint32_t _stateStartMs   = 0;
    uint16_t _stateDurMs     = 0;       // flash/fade 지속
    uint16_t _blinkPeriodMs  = 0;       // on/off 반주기
    bool     _blinkOn        = false;

    uint32_t _lastTickMs     = 0;

    // ----------------------------------------------------
    // 내부 유틸
    // ----------------------------------------------------
    static void _rgbOf(EN_L10_Color_t p_c, uint8_t& p_r, uint8_t& p_g, uint8_t& p_b);

    // 밝기 스케일 (p_scale: 0~255 추가 스케일. 예: fadeout 진행도)
    void _apply(EN_L10_Color_t p_color, uint8_t p_scale);

    // base 색을 즉시 반영
    void _applyBase();

    // 상태 진입
    void _enterIdle(uint32_t p_now);
    void _enterFlash(EN_L10_Color_t p_c, uint16_t p_ms, uint32_t p_now);
    void _enterBlink(EN_L10_Color_t p_c, uint16_t p_period, uint16_t p_dur, uint32_t p_now);
    void _enterFadeout(EN_L10_Color_t p_c, uint16_t p_ms, uint32_t p_now);
    void _enterOff();
};
