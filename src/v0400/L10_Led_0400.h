// =======================================================
// File: src/v040/L10_Led_0400.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : L10_Led_0400.h
 * 모듈약어 : L10
 * 모듈명 : WS2812 LED Controller (단일, Adafruit_NeoPixel)
 * ------------------------------------------------------
 * 기능 요약
 *  - 단일 WS2812 (Adafruit_NeoPixel, ESP32-S3 RMT 백엔드)
 *  - Mode 색상 / Flash / Blink / Fadeout / Off
 *  - 사용자 밝기 (0~255)
 *  - 비동기 상태머신 (tick 기반, 50ms)
 *
 * [상태머신]
 *   IDLE    : base 색 상시 점등
 *   FLASH   : 단발 점등 (사용자 액션 피드백)
 *   BLINK   : 주기적 on/off (페어링 대기)
 *   FADEOUT : 밝기 감소 후 off (슬립 진입)
 *   OFF     : 완전 off
 *
 * [GPIO]
 *   G_L10_PIN = 21
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

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

    void begin(uint8_t p_brightness = 128);
    void setBrightness(uint8_t p_b);

    void setModeColor(uint8_t p_mode);
    void flash(EN_L10_Color_t p_color, uint16_t p_ms = 50);
    void blink(EN_L10_Color_t p_color, uint16_t p_periodMs, uint16_t p_durationMs = 0);
    void stopBlink();
    void fadeout(EN_L10_Color_t p_color, uint16_t p_ms = 800);
    void off();

    void tick();

    EN_L10_Color_t getBaseColor() const { return _baseColor; }
    uint8_t        getBrightness() const { return _brightness; }

  private:
    enum EN_L10_State_t : uint8_t {
        EN_L10_ST_IDLE    = 0,
        EN_L10_ST_FLASH   = 1,
        EN_L10_ST_BLINK   = 2,
        EN_L10_ST_FADEOUT = 3,
        EN_L10_ST_OFF     = 4,
    };

    Adafruit_NeoPixel* _strip = nullptr;
    uint8_t            _brightness = 128;

    EN_L10_State_t _state       = EN_L10_ST_OFF;
    EN_L10_Color_t _baseColor   = EN_L10_COLOR_OFF;
    EN_L10_Color_t _evtColor    = EN_L10_COLOR_OFF;

    uint32_t _stateStartMs   = 0;
    uint16_t _stateDurMs     = 0;
    uint16_t _blinkPeriodMs  = 0;
    bool     _blinkOn        = false;

    static void _rgbOf(EN_L10_Color_t p_c, uint8_t& p_r, uint8_t& p_g, uint8_t& p_b);
    void _apply(EN_L10_Color_t p_color, uint8_t p_scale);
    void _applyBase();

    void _enterIdle(uint32_t p_now);
    void _enterFlash(EN_L10_Color_t p_c, uint16_t p_ms, uint32_t p_now);
    void _enterBlink(EN_L10_Color_t p_c, uint16_t p_period, uint16_t p_dur, uint32_t p_now);
    void _enterFadeout(EN_L10_Color_t p_c, uint16_t p_ms, uint32_t p_now);
    void _enterOff();
};