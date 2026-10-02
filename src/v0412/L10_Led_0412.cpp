// =======================================================
// File: src/v0412/L10_Led_0412.cpp
// =======================================================
#include "L10_Led_0412.h"

// =======================================================
// ctor / dtor
// =======================================================
CL_L10_Led::CL_L10_Led() {}

CL_L10_Led::~CL_L10_Led() {
    if (_strip) {
        _strip->clear();
        _strip->show();
        delete _strip;
        _strip = nullptr;
    }
}

// =======================================================
// 초기화
// =======================================================
void CL_L10_Led::begin(uint8_t p_brightness) {
    _brightness = p_brightness;

    // Adafruit_NeoPixel: (numLEDs, pin, type)
    // 단일 LED, GRB, 800kHz
    _strip = new (std::nothrow) Adafruit_NeoPixel(1, G_L10_PIN, NEO_GRB + NEO_KHZ800);
    if (!_strip) {
        _state = EN_L10_ST_OFF;
        return;
    }

    _strip->begin();
    _strip->clear();
    _strip->show();

    _state      = EN_L10_ST_OFF;
    _baseColor  = EN_L10_COLOR_OFF;
    _evtColor   = EN_L10_COLOR_OFF;
    _lastTickMs = (uint32_t)millis();
}

void CL_L10_Led::setBrightness(uint8_t p_b) {
    _brightness = p_b;
    if (_state == EN_L10_ST_IDLE) _applyBase();
}

void CL_L10_Led::setFadeTimings(uint16_t p_fadeoutMs, uint16_t p_fadeinMs) {
    _fadeoutMs = (p_fadeoutMs == 0) ? 1 : p_fadeoutMs;
    _fadeinMs  = (p_fadeinMs  == 0) ? 1 : p_fadeinMs;
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


void CL_L10_Led::fadeout(EN_L10_Color_t p_color, uint16_t p_ms) {
    _enterFadeout(p_color, p_ms, (uint32_t)millis());
}

void CL_L10_Led::fadein(EN_L10_Color_t p_color, uint16_t p_ms) {
    _enterFadein(p_color, p_ms, (uint32_t)millis());
}

void CL_L10_Led::off() {
    _enterOff();
}

void CL_L10_Led::suspend(ST_LedSnapshot_t& p_out) {
    p_out.state         = _state;
    p_out.baseColor     = _baseColor;
    p_out.evtColor      = _evtColor;
    p_out.stateDurMs    = _stateDurMs;
    p_out.blinkPeriodMs = _blinkPeriodMs;
    p_out.stateStartMs  = _stateStartMs;
    p_out.blinkOn       = _blinkOn;

    // [B-1] RED fadeout → OFF (동기 대기, 최대 _fadeoutMs+200ms)
    //   - _ledTask(Core 0, prio 1)가 병렬로 tick하며 상태 전이를 완료함
    //   - 센서 태스크 블로킹이지만, 곧이어 sleepNow() 진입이므로 허용
    _enterFadeout(EN_L10_COLOR_RED, _fadeoutMs, (uint32_t)millis());

    const uint32_t v_deadline = (uint32_t)millis() + _fadeoutMs + 200;
    while (_state == EN_L10_ST_FADEOUT &&
           (int32_t)((uint32_t)millis() - v_deadline) < 0) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    if (_state != EN_L10_ST_OFF) _enterOff();
}


void CL_L10_Led::resume(const ST_LedSnapshot_t& p_in) {
    _baseColor = p_in.baseColor;
    _evtColor  = p_in.evtColor;

    // 이전 상태가 OFF였으면 그대로 OFF 유지
    if (p_in.state == EN_L10_ST_OFF) {
        _enterOff();
        return;
    }

    // [B-2] OFF → base color 로 fadein (비동기, _ledTask가 FADEIN→IDLE 전이)
    //   - 지속 상태(FLASH/BLINK)는 sleep과 겹치지 않으므로 IDLE 복원으로 충분
    //   - 원본 stateStartMs/DurMs는 참고용으로 보존(디버깅 목적)하며 실 사용 안 함
    _enterFadein(_baseColor, _fadeinMs, (uint32_t)millis());
}

// =======================================================
// tick (50ms 주기 권장)
// =======================================================
void CL_L10_Led::tick() {
    if (!_strip) return;

    const uint32_t v_now = (uint32_t)millis();
    _lastTickMs = v_now;

    switch (_state) {
        case EN_L10_ST_IDLE:
            break;

        case EN_L10_ST_FLASH:
            if ((v_now - _stateStartMs) >= _stateDurMs) {
                _enterIdle(v_now);
            }
            break;

        case EN_L10_ST_BLINK: {
            if (_stateDurMs > 0 && (v_now - _stateStartMs) >= _stateDurMs) {
                _enterIdle(v_now);
                break;
            }
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

        case EN_L10_ST_FADEOUT: {
            if (_stateDurMs == 0) { _enterOff(); break; }
            const uint32_t v_elapsed = v_now - _stateStartMs;
            if (v_elapsed >= _stateDurMs) { _enterOff(); break; }
            const uint32_t v_scale = 255u - (uint32_t)(255u * v_elapsed / _stateDurMs);
            _apply(_evtColor, (uint8_t)v_scale);
            break;
        }

        case EN_L10_ST_FADEIN: {
            if (_stateDurMs == 0) { _enterIdle((uint32_t)millis()); break; }
            const uint32_t v_elapsed = v_now - _stateStartMs;
            if (v_elapsed >= _stateDurMs) {
                _enterIdle(v_now);
                break;
            }
            const uint32_t v_scale = (uint32_t)(255u * v_elapsed / _stateDurMs);
            _apply(_evtColor, (uint8_t)v_scale);
            break;
        }

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

    uint8_t v_r = 0, v_g = 0, v_bl = 0;
    _rgbOf(p_color, v_r, v_g, v_bl);

    // 밝기 × 추가 스케일 (0~255)
    const uint32_t v_br  = (uint32_t)_brightness;
    const uint32_t v_scl = (uint32_t)p_scale;

    const uint8_t v_outR = (uint8_t)(((uint32_t)v_r  * v_br * v_scl) / (255u * 255u));
    const uint8_t v_outG = (uint8_t)(((uint32_t)v_g  * v_br * v_scl) / (255u * 255u));
    const uint8_t v_outB = (uint8_t)(((uint32_t)v_bl * v_br * v_scl) / (255u * 255u));

    _strip->setPixelColor(0, v_outR, v_outG, v_outB);
    _strip->show();
}

void CL_L10_Led::_applyBase() {
    _apply(_baseColor, 255);
}

// =======================================================
// 내부: 상태 진입
// =======================================================
void CL_L10_Led::_enterIdle(uint32_t p_now) {
    _state         = EN_L10_ST_IDLE;
    _stateStartMs  = p_now;
    _stateDurMs    = 0;
    _blinkPeriodMs = 0;
    _blinkOn       = true;
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
    _state         = EN_L10_ST_BLINK;
    _evtColor      = p_c;
    _stateStartMs  = p_now;
    _stateDurMs    = p_dur;
    _blinkPeriodMs = (p_period < 20) ? 20 : p_period;
    _blinkOn       = true;
    _apply(_evtColor, 255);
}

void CL_L10_Led::_enterFadeout(EN_L10_Color_t p_c, uint16_t p_ms, uint32_t p_now) {
    _state        = EN_L10_ST_FADEOUT;
    _evtColor     = p_c;
    _stateStartMs = p_now;
    _stateDurMs   = (p_ms == 0) ? 1 : p_ms;
    _apply(_evtColor, 255);
}

void CL_L10_Led::_enterFadein(EN_L10_Color_t p_c, uint16_t p_ms, uint32_t p_now) {
    _state        = EN_L10_ST_FADEIN;
    _evtColor     = p_c;
    _stateStartMs = p_now;
    _stateDurMs   = (p_ms == 0) ? 1 : p_ms;
    _apply(p_c, 0);   // 0 밝기에서 시작
}

void CL_L10_Led::_enterOff() {
    _state        = EN_L10_ST_OFF;
    _stateStartMs = (uint32_t)millis();
    _stateDurMs   = 0;
    _apply(EN_L10_COLOR_OFF, 0);
}