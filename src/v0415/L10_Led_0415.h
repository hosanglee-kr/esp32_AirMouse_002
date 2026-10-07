// =======================================================
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
 *  - Mode 색상 / Flash / Blink / Fadeout / Off
 *  - 사용자 밝기 (0~255)
 *  - 비동기 상태머신 (tick 기반, 50ms 권장)
 *
 * [상태머신]
 *   IDLE    : base 색 (Mode 색) 상시 점등
 *   FLASH   : 짧은 단발 점등
 *   BLINK   : 주기적 on/off
 *   FADEOUT : 밝기 감소 후 off
 *
 * [GPIO]
 *   G_L10_PIN = 21
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
#include <Adafruit_NeoPixel.h>
#include "HW_Def_0415.h"

// -------------------------------------------------------
// GPIO (HW_DEF 참조)
// -------------------------------------------------------
static constexpr int G_L10_PIN = HW_DEF::PIN_LED_WS2812;

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
    
    // [B-1/B-2] Sleep/wake 시 사용할 페이드 타이밍 (프로파일에서 주입)
    void setFadeTimings(uint16_t p_fadeoutMs, uint16_t p_fadeinMs);

    void suspend(ST_LedSnapshot_t& p_out);
    void resume(const ST_LedSnapshot_t& p_in);

    void tick();

    EN_L10_Color_t getBaseColor() const { return _baseColor; }
    uint8_t        getBrightness() const { return _brightness; }

  private:
    Adafruit_NeoPixel* _strip = nullptr;
    uint8_t  _brightness = 128;
    uint16_t _fadeoutMs  = 500;   // [B-1] suspend 시 RED fadeout
    uint16_t _fadeinMs   = 300;   // [B-2] resume 시 mode color fadein

    EN_L10_State_t _state       = EN_L10_ST_OFF;
    EN_L10_Color_t _baseColor   = EN_L10_COLOR_OFF;
    EN_L10_Color_t _evtColor    = EN_L10_COLOR_OFF;

    uint32_t _stateStartMs   = 0;
    uint16_t _stateDurMs     = 0;
    uint16_t _blinkPeriodMs  = 0;
    bool     _blinkOn        = false;

    uint32_t _lastTickMs     = 0;

    static void _rgbOf(EN_L10_Color_t p_c, uint8_t& p_r, uint8_t& p_g, uint8_t& p_b);
    void _apply(EN_L10_Color_t p_color, uint8_t p_scale);
    void _applyBase();

    void _enterIdle(uint32_t p_now);
    void _enterFlash(EN_L10_Color_t p_c, uint16_t p_ms, uint32_t p_now);
    void _enterBlink(EN_L10_Color_t p_c, uint16_t p_period, uint16_t p_dur, uint32_t p_now);
    void _enterFadeout(EN_L10_Color_t p_c, uint16_t p_ms, uint32_t p_now);
    void _enterFadein(EN_L10_Color_t p_c, uint16_t p_ms, uint32_t p_now);
    void _enterOff();
};