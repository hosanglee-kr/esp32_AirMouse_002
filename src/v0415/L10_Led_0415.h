// File: src/v0415/L10_Led_0415.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : L10_Led_0415.h
 * 모듈약어 : L10
 * 모듈명 : WS2812 LED Controller (단일, Adafruit NeoPixel)
 * ------------------------------------------------------
 * 기능 요약
 *  - 단일 WS2812 (Adafruit_NeoPixel)
 *  - Mode 색상 / Flash / Blink / Fadeout / Fadein / Off
 *  - 사용자 밝기 (0~255)
 *  - 비동기 상태머신 (tick 기반, 50ms 권장)
 *
 * [상태머신]
 *   IDLE    : base 색 (Mode 색) 상시 점등
 *   FLASH   : 짧은 단발 점등
 *   BLINK   : 주기적 on/off
 *   FADEOUT : 밝기 감소 후 off
 *   FADEIN  : 점진 점등 후 IDLE
 *   OFF     : 완전 소등
 *
 * [v0415 주요 변경 — Thread-Safety]
 *  - FreeRTOS Recursive Mutex 도입 (_mutex)
 *  - 이유: setModeColor/flash/blink 등은 "어느 태스크에서나" 호출 가능
 *          (SPEC §"상태 소유권" 면제 조항)하나, 내부 상태 변경 + _strip->show()는
 *          원자적이어야 함. _ledTask의 tick()과 race 방지.
 *  - Recursive Mutex 선택 이유:
 *      portMUX critical section 내부에서 Adafruit_NeoPixel::show()가
 *      ESP32 RMT semaphore를 take하므로 portMUX 사용 불가.
 *  - suspend() blocking loop는 lock/unlock 반복 (deadlock 회피)
 *
 * [호출 규약]
 *  - tick(): _ledTask 단독 (50ms 주기)
 *  - 그 외 API: 어느 태스크에서나 (내부 mutex 보호)
 *  - 서로 다른 태스크에서 동시 호출해도 안전
 *
 * [GPIO]
 *   HW_DEF::PIN_LED_WS2812 (GPIO 21)
 * ------------------------------------------------------
 * [구현 규칙]
 *  - 항상 소스 시작 주석 부분 체계 유지 및 내용 업데이트
 *  - ArduinoJson v7.x.x 사용 (v6 이하 사용 금지)
 *  - memset + strlcpy 기반 안전 초기화
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <string.h>
#include <new>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <Adafruit_NeoPixel.h>
#include "HW_Def_0415.h"

// -------------------------------------------------------
// [v0415] 매직 넘버 상수화
// -------------------------------------------------------
static constexpr uint16_t G_L10_BLINK_MIN_PERIOD_MS = 20;   // [L4-A2-03]
static constexpr uint16_t G_L10_FADE_MIN_MS         = 1;    // [L4-A2-04] 0 방어
static constexpr uint32_t G_L10_FADE_DEADLINE_MS    = 200;  // suspend 여유
static constexpr uint32_t G_L10_SCALE_MAX           = 255;  // 밝기 최대

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
    enum EN_L10_State_t : uint8_t {
        EN_L10_ST_IDLE    = 0,
        EN_L10_ST_FLASH   = 1,
        EN_L10_ST_BLINK   = 2,
        EN_L10_ST_FADEOUT = 3,
        EN_L10_ST_FADEIN  = 4,
        EN_L10_ST_OFF     = 5,
    };

    struct ST_LedSnapshot_t {
        EN_L10_State_t state;
        EN_L10_Color_t baseColor;
        EN_L10_Color_t evtColor;
        uint16_t       stateDurMs;
        uint16_t       blinkPeriodMs;
        uint32_t       stateStartMs;
        bool           blinkOn;
    };

    CL_L10_Led();
    ~CL_L10_Led();

    void begin(uint8_t p_brightness = 128);
    void setBrightness(uint8_t p_b);

    void setModeColor(uint8_t p_mode);
    void flash(EN_L10_Color_t p_color, uint16_t p_ms = 50);
    void blink(EN_L10_Color_t p_color, uint16_t p_periodMs, uint16_t p_durationMs = 0);
    void fadeout(EN_L10_Color_t p_color, uint16_t p_ms = 800);
    void fadein(EN_L10_Color_t p_color, uint16_t p_ms = 300);
    void off();

    // Sleep/wake 시 사용할 페이드 타이밍 (프로파일에서 주입)
    void setFadeTimings(uint16_t p_fadeoutMs, uint16_t p_fadeinMs);

    // Suspend/Resume (Sleep 진입/복귀)
    void suspend(ST_LedSnapshot_t& p_out);
    void resume(const ST_LedSnapshot_t& p_in);

    // [단독 실행] _ledTask에서 50ms 주기 호출
    void tick();

    EN_L10_Color_t getBaseColor() const { return _baseColor; }
    uint8_t        getBrightness() const { return _brightness; }

  private:
    // --------------------------------------------------
    // [v0415] Thread-safety
    // --------------------------------------------------
    SemaphoreHandle_t _mutex = nullptr;

    void _lock() {
        if (_mutex) (void)xSemaphoreTakeRecursive(_mutex, portMAX_DELAY);
    }
    void _unlock() {
        if (_mutex) xSemaphoreGiveRecursive(_mutex);
    }

    // --------------------------------------------------
    // 상태
    // --------------------------------------------------
    Adafruit_NeoPixel* _strip = nullptr;
    uint8_t  _brightness = 128;
    uint16_t _fadeoutMs  = 500;   // suspend 시 RED fadeout
    uint16_t _fadeinMs   = 300;   // resume 시 mode color fadein

    EN_L10_State_t _state       = EN_L10_ST_OFF;
    EN_L10_Color_t _baseColor   = EN_L10_COLOR_OFF;
    EN_L10_Color_t _evtColor    = EN_L10_COLOR_OFF;

    uint32_t _stateStartMs   = 0;
    uint16_t _stateDurMs     = 0;
    uint16_t _blinkPeriodMs  = 0;
    bool     _blinkOn        = false;

    // [v0415] _lastTickMs 삭제 (Dead)

    // --------------------------------------------------
    // 내부 helper
    // --------------------------------------------------
    static void _rgbOf(EN_L10_Color_t p_c, uint8_t& p_r, uint8_t& p_g, uint8_t& p_b);
    void _apply(EN_L10_Color_t p_color, uint8_t p_scale);
    void _applyBase();

    void _enterIdle(uint32_t p_now);
    void _enterFlash(EN_L10_Color_t p_c, uint16_t p_ms, uint32_t p_now);
    void _enterBlink(EN_L10_Color_t p_c, uint16_t p_period, uint16_t p_dur, uint32_t p_now);
    void _enterFadeout(EN_L10_Color_t p_c, uint16_t p_ms, uint32_t p_now);
    void _enterFadein(EN_L10_Color_t p_c, uint16_t p_ms, uint32_t p_now);
    void _enterOff();

    // tick()의 실 구현 (lock 보유 가정)
    void _tickInternal();
};
