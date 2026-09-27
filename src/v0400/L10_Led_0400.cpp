// =======================================================
// File: src/v040/L10_Led_0400.cpp
// =======================================================
#include "L10_Led_0400.h"

// =======================================================
// ctor / dtor
// =======================================================
CL_L10_Led::CL_L10_Led() {
    memset(&_strip, 0, sizeof(_strip));
}

CL_L10_Led::~CL_L10_Led() {
    if (_strip) {
        led_strip_clear(_strip);
        led_strip_del(_strip);
        _strip = nullptr;
    }
}

// =======================================================
// 초기화
// =======================================================
void CL_L10_Led::begin(uint8_t p_brightness) {
    _brightness = p_brightness;

    led_strip_config_t v_stripCfg = {
        .strip_gpio_num = G_L10_PIN,
        .max_leds       = 1,
        .led_model      = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        }
    };

    led_strip_rmt_config_t v_rmtCfg = {
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .resolution_hz     = 10 * 1000 * 1000,   // 10MHz
        .mem_block_symbols = 64,
        .flags = {
            .with_dma = 0,
        }
    };

    esp_err_t v_err = led_strip_new_rmt_device(&v_stripCfg, &v_rmtCfg, &_strip);
    if (v_err != ESP_OK || !_strip) {
        _strip = nullptr;
        _state = EN_L10_ST_OFF;
        return;
    }

    led_strip_clear(_strip);
    _state     = EN_L10_ST_OFF;
    _baseColor = EN_L10_COLOR_OFF;
    _evtColor  = EN_L10_COLOR_OFF;
    _lastTickMs = (uint32_t)millis();
}

void CL_L10_Led::setBrightness(uint8_t p_b) {
    _brightness = p_b;
    // IDLE이면 즉시 반영
    if (_state == EN_L10_ST_IDLE) _applyBase();
}

// =======================================================
// 상태 제어
// =======================================================
void CL_L10_Led::setModeColor(uint8_t p_mode) {
    switch (p_mode) {
        case 1: _baseColor = EN_L10_COLOR_BLUE;   break;
        case 2: _baseColor = EN_L10_COLOR_GREEN;  break;
        case 3: _baseColor = EN_L10_COLOR_ORANGE; break;
        default: _baseColor = EN_L10_COLOR_OFF;   break;
    }
    _enterIdle((uint32_t)millis());
}

void CL_L10_Led::flash(EN_L10_Color_t p_color, uint16_t p_ms) {
    _enterFlash(p_color, p_ms, (uint32_t)millis());
}

void CL_L10_Led::blink(EN_L10_Color_t p_color, uint16_t p_periodMs, uint16_t p_durationMs) {
    _enterBlink(p_color, p_periodMs, p_durationMs, (uint32_t)millis());
}

void CL_L10_Led::stopBlink() {
    if (_state == EN_L10_ST_BLINK) _enterIdle((uint32_t)millis());
}

void CL_L10_Led::fadeout(EN_L10_Color_t p_color, uint16_t p_ms) {
    _enterFadeout(p_color, p_ms, (uint32_t)millis());
}

void CL_L10_Led::off() {
    _enterOff();
}

// =======================================================
// tick (50ms 주기 권장)
// =======================================================
void CL_L10_Led::tick() {
    if (!_strip) return;

    const uint32_t v_now = (uint32_t)millis();
    _lastTickMs = v_now;

    switch (_state) {
        // -------------------------------------------------
        case EN_L10_ST_IDLE:
            // 변화 없음. base 색 유지.
            break;

        // -------------------------------------------------
        case EN_L10_ST_FLASH:
            if ((v_now - _stateStartMs) >= _stateDurMs) {
                _enterIdle(v_now);
            }
            break;

        // -------------------------------------------------
        case EN_L10_ST_BLINK: {
            // duration 만료 시 IDLE
            if (_stateDurMs > 0 && (v_now - _stateStartMs) >= _stateDurMs) {
                _enterIdle(v_now);
                break;
            }
            // 반주기마다 toggle
            const uint32_t v_elapsed = v_now - _stateStartMs;
            const uint16_t v_half    = (_blinkPeriodMs > 0) ? (_blinkPeriodMs / 2) : 0;
            if (v_half == 0) break;

            const bool v_shouldOn = ((v_elapsed / v_half) % 2) == 0;
            if (v_shouldOn != _blinkOn) {
                _blinkOn = v_shouldOn;
                if (_blinkOn) _apply(_evtColor, 255);
                else          _apply(EN_L10_COLOR_OFF, 0);
            }
            break;
        }

        // -------------------------------------------------
        case EN_L10_ST_FADEOUT: {
            if (_stateDurMs == 0) {
                _enterOff();
                break;
            }
            const uint32_t v_elapsed = v_now - _stateStartMs;
            if (v_elapsed >= _stateDurMs) {
                _enterOff();
                break;
            }
            // scale: 255 → 0
            const uint32_t v_scale = 255u - (uint32_t)(255u * v_elapsed / _stateDurMs);
            _apply(_evtColor, (uint8_t)v_scale);
            break;
        }

        // -------------------------------------------------
        case EN_L10_ST_OFF:
            break;

        default:
            break;
    }
}

// =======================================================
// 내부: 색 매핑
// =======================================================
void CL_L10_Led::_rgbOf(EN_L10_Color_t p_c, uint8_t& p_r, uint8_t& p_g, uint8_t& p_b) {
    switch (p_c) {
        case EN_L10_COLOR_OFF:    p_r = 0;   p_g = 0;   p_b = 0;   break;
        case EN_L10_COLOR_BLUE:   p_r = 0;   p_g = 0;   p_b = 255; break;
        case EN_L10_COLOR_GREEN:  p_r = 0;   p_g = 255; p_b = 0;   break;
        case EN_L10_COLOR_ORANGE: p_r = 255; p_g = 128; p_b = 0;   break;
        case EN_L10_COLOR_WHITE:  p_r = 255; p_g = 255; p_b = 255; break;
        case EN_L10_COLOR_RED:    p_r = 255; p_g = 0;   p_b = 0;   break;
        default:                  p_r = 0;   p_g = 0;   p_b = 0;   break;
    }
}

void CL_L10_Led::_apply(EN_L10_Color_t p_color, uint8_t p_scale) {
    if (!_strip) return;

    uint8_t v_r = 0, v_g = 0, v_b = 0;
    _rgbOf(p_color, v_r, v_g, v_b);

    // 밝기 × 추가 스케일 (0~255)
    const uint32_t v_b = (uint32_t)_brightness;
    const uint32_t v_s = (uint32_t)p_scale;

    v_r = (uint8_t)(((uint32_t)v_r * v_b * v_s) / (255u * 255u));
    v_g = (uint8_t)(((uint32_t)v_g * v_b * v_s) / (255u * 255u));
    v_b = (uint8_t)(((uint32_t)v_b * v_b * v_s) / (255u * 255u));

    led_strip_set_pixel(_strip, 0, v_r, v_g, v_b);
    led_strip_refresh(_strip);
}

void CL_L10_Led::_applyBase() {
    _apply(_baseColor, 255);
}

// =======================================================
// 내부: 상태 진입
// =======================================================
void CL_L10_Led::_enterIdle(uint32_t p_now) {
    _state        = EN_L10_ST_IDLE;
    _stateStartMs = p_now;
    _stateDurMs   = 0;
    _blinkPeriodMs = 0;
    _blinkOn      = true;
    _applyBase();
}

void CL_L10_Led::_enterFlash(EN_L10_Color_t p_c, uint16_t p_ms, uint32_t p_now) {
    _state        = EN_L10_ST_FLASH;
    _evtColor     = p_c;
    _stateStartMs = p_now;
    _stateDurMs   = (p_ms == 0) ? 1 : p_ms;
    _apply(_evtColor, 255);
}

void CL_L10_Led::_enterBlink(EN_L10_Color_t p_c, uint16_t p_period, uint16_t p_dur, uint32_t p_now) {
    _state          = EN_L10_ST_BLINK;
    _evtColor       = p_c;
    _stateStartMs   = p_now;
    _stateDurMs     = p_dur;
    _blinkPeriodMs  = (p_period < 20) ? 20 : p_period;
    _blinkOn        = true;
    _apply(_evtColor, 255);
}

void CL_L10_Led::_enterFadeout(EN_L10_Color_t p_c, uint16_t p_ms, uint32_t p_now) {
    _state        = EN_L10_ST_FADEOUT;
    _evtColor     = p_c;
    _stateStartMs = p_now;
    _stateDurMs   = (p_ms == 0) ? 1 : p_ms;
    _apply(_evtColor, 255);
}

void CL_L10_Led::_enterOff() {
    _state        = EN_L10_ST_OFF;
    _stateStartMs = (uint32_t)millis();
    _stateDurMs   = 0;
    _apply(EN_L10_COLOR_OFF, 0);
}
