// File: src/v0415/L10_Led_0415.cpp
// =======================================================
#include "L10_Led_0415.h"

// =======================================================
// ctor / dtor
// =======================================================
CL_L10_Led::CL_L10_Led() {}

CL_L10_Led::~CL_L10_Led() {
    // 종료 시점에는 다른 태스크가 접근하지 않음을 가정
    if (_strip) {
        _strip->clear();
        _strip->show();
        delete _strip;
        _strip = nullptr;
    }
    if (_mutex) {
        vSemaphoreDelete(_mutex);
        _mutex = nullptr;
    }
}

// =======================================================
// 초기화
// =======================================================
void CL_L10_Led::begin(uint8_t p_brightness) {
    // [v0415] mutex 생성은 _strip 초기화 이전
    if (!_mutex) {
        _mutex = xSemaphoreCreateRecursiveMutex();
    }

    _brightness = p_brightness;

    // Adafruit_NeoPixel: (numLEDs, pin, type)
    // 단일 LED, GRB, 800kHz
    _strip = new (std::nothrow) Adafruit_NeoPixel(1, HW_DEF::PIN_LED_WS2812,
                                                  NEO_GRB + NEO_KHZ800);
    if (!_strip) {
        _state = EN_L10_ST_OFF;
        return;
    }

    _lock();
    _strip->begin();
    _strip->clear();
    _strip->show();

    _state      = EN_L10_ST_OFF;
    _baseColor  = EN_L10_COLOR_OFF;
    _evtColor   = EN_L10_COLOR_OFF;
    _unlock();
}

void CL_L10_Led::setBrightness(uint8_t p_b) {
    _lock();
    _brightness = p_b;
    if (_state == EN_L10_ST_IDLE) _applyBase();
    _unlock();
}

void CL_L10_Led::setFadeTimings(uint16_t p_fadeoutMs, uint16_t p_fadeinMs) {
    _lock();
    _fadeoutMs = (p_fadeoutMs == 0) ? G_L10_FADE_MIN_MS : p_fadeoutMs;
    _fadeinMs  = (p_fadeinMs  == 0) ? G_L10_FADE_MIN_MS : p_fadeinMs;
    _unlock();
}

// =======================================================
// 상태 제어 (외부 태스크 호출 가능, mutex 보호)
// =======================================================
void CL_L10_Led::setModeColor(uint8_t p_mode) {
    _lock();
    switch (p_mode) {
        case 1: _baseColor = EN_L10_COLOR_BLUE;   break;
        case 2: _baseColor = EN_L10_COLOR_GREEN;  break;
        case 3: _baseColor = EN_L10_COLOR_ORANGE; break;
        default: _baseColor = EN_L10_COLOR_OFF;   break;
    }
    _enterIdle((uint32_t)millis());
    _unlock();
}

void CL_L10_Led::flash(EN_L10_Color_t p_color, uint16_t p_ms) {
    _lock();
    _enterFlash(p_color, p_ms, (uint32_t)millis());
    _unlock();
}

void CL_L10_Led::blink(EN_L10_Color_t p_color, uint16_t p_periodMs, uint16_t p_durationMs) {
    _lock();
    _enterBlink(p_color, p_periodMs, p_durationMs, (uint32_t)millis());
    _unlock();
}

void CL_L10_Led::fadeout(EN_L10_Color_t p_color, uint16_t p_ms) {
    _lock();
    _enterFadeout(p_color, p_ms, (uint32_t)millis());
    _unlock();
}

void CL_L10_Led::fadein(EN_L10_Color_t p_color, uint16_t p_ms) {
    _lock();
    _enterFadein(p_color, p_ms, (uint32_t)millis());
    _unlock();
}

void CL_L10_Led::off() {
    _lock();
    _enterOff();
    _unlock();
}

// =======================================================
// [B-1] suspend — blocking RED fadeout → OFF
// ------------------------------------------------------
//  - Snapshot 백업 후 RED fadeout
//  - deadline = _fadeoutMs + G_L10_FADE_DEADLINE_MS
//  - blocking 대기 중 mutex 해제 (ledTask의 tick이 진행되도록)
//  - deadline 초과 시 강제 OFF
// =======================================================
void CL_L10_Led::suspend(ST_LedSnapshot_t& p_out) {
    // 1) 스냅샷 백업 + fadeout 진입
    _lock();
    p_out.state         = _state;
    p_out.baseColor     = _baseColor;
    p_out.evtColor      = _evtColor;
    p_out.stateDurMs    = _stateDurMs;
    p_out.blinkPeriodMs = _blinkPeriodMs;
    p_out.stateStartMs  = _stateStartMs;
    p_out.blinkOn       = _blinkOn;

    const uint16_t v_fadeoutMs = _fadeoutMs;
    _enterFadeout(EN_L10_COLOR_RED, v_fadeoutMs, (uint32_t)millis());
    _unlock();

    // 2) blocking 대기 (mutex 해제 상태에서)
    const uint32_t v_deadline = (uint32_t)millis() + v_fadeoutMs + G_L10_FADE_DEADLINE_MS;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(50));   // ledTask가 tick 진행하도록 yield

        _lock();
        const bool v_done = (_state != EN_L10_ST_FADEOUT);
        _unlock();

        if (v_done) break;
        if ((int32_t)((uint32_t)millis() - v_deadline) >= 0) break;
    }

    // 3) 강제 OFF (deadline 초과 시 안전망)
    _lock();
    if (_state != EN_L10_ST_OFF) _enterOff();
    _unlock();
}

// =======================================================
// [B-2] resume — async fadein (base color)
// ------------------------------------------------------
//  - 이전 상태가 OFF였으면 OFF 유지
//  - 그 외 → base color fadein (FADEIN → IDLE, _ledTask가 전이)
// =======================================================
void CL_L10_Led::resume(const ST_LedSnapshot_t& p_in) {
    _lock();
    _baseColor = p_in.baseColor;
    _evtColor  = p_in.evtColor;

    if (p_in.state == EN_L10_ST_OFF) {
        _enterOff();
        _unlock();
        return;
    }

    _enterFadein(_baseColor, _fadeinMs, (uint32_t)millis());
    _unlock();
}

// =======================================================
// tick (50ms 주기, _ledTask 단독)
// =======================================================
void CL_L10_Led::tick() {
    if (!_strip) return;

    _lock();
    _tickInternal();
    _unlock();
}

void CL_L10_Led::_tickInternal() {
    if (!_strip) return;

    const uint32_t v_now = (uint32_t)millis();

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
                if (_blinkOn) _apply(_evtColor, G_L10_SCALE_MAX);
                else          _apply(EN_L10_COLOR_OFF, 0);
            }
            break;
        }

        case EN_L10_ST_FADEOUT: {
            if (_stateDurMs == 0) { _enterOff(); break; }
            const uint32_t v_elapsed = v_now - _stateStartMs;
            if (v_elapsed >= _stateDurMs) { _enterOff(); break; }
            const uint32_t v_scale = G_L10_SCALE_MAX
                                   - (uint32_t)(G_L10_SCALE_MAX * v_elapsed / _stateDurMs);
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
            const uint32_t v_scale = (uint32_t)(G_L10_SCALE_MAX * v_elapsed / _stateDurMs);
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

    // 밝기 × 추가 스케일 (0~255) 정규화
    const uint32_t v_br  = (uint32_t)_brightness;
    const uint32_t v_scl = (uint32_t)p_scale;

    const uint8_t v_outR = (uint8_t)(((uint32_t)v_r  * v_br * v_scl) / (G_L10_SCALE_MAX * G_L10_SCALE_MAX));
    const uint8_t v_outG = (uint8_t)(((uint32_t)v_g  * v_br * v_scl) / (G_L10_SCALE_MAX * G_L10_SCALE_MAX));
    const uint8_t v_outB = (uint8_t)(((uint32_t)v_bl * v_br * v_scl) / (G_L10_SCALE_MAX * G_L10_SCALE_MAX));

    _strip->setPixelColor(0, v_outR, v_outG, v_outB);
    _strip->show();
}

void CL_L10_Led::_applyBase() {
    _apply(_baseColor, G_L10_SCALE_MAX);
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
    _apply(_evtColor, G_L10_SCALE_MAX);
}

void CL_L10_Led::_enterBlink(EN_L10_Color_t p_c, uint16_t p_period, uint16_t p_dur, uint32_t p_now) {
    _state         = EN_L10_ST_BLINK;
    _evtColor      = p_c;
    _stateStartMs  = p_now;
    _stateDurMs    = p_dur;
    _blinkPeriodMs = (p_period < G_L10_BLINK_MIN_PERIOD_MS) ? G_L10_BLINK_MIN_PERIOD_MS : p_period;
    _blinkOn       = true;
    _apply(_evtColor, G_L10_SCALE_MAX);
}

void CL_L10_Led::_enterFadeout(EN_L10_Color_t p_c, uint16_t p_ms, uint32_t p_now) {
    _state        = EN_L10_ST_FADEOUT;
    _evtColor     = p_c;
    _stateStartMs = p_now;
    _stateDurMs   = (p_ms == 0) ? G_L10_FADE_MIN_MS : p_ms;
    _apply(_evtColor, G_L10_SCALE_MAX);
}

void CL_L10_Led::_enterFadein(EN_L10_Color_t p_c, uint16_t p_ms, uint32_t p_now) {
    _state        = EN_L10_ST_FADEIN;
    _evtColor     = p_c;
    _stateStartMs = p_now;
    _stateDurMs   = (p_ms == 0) ? G_L10_FADE_MIN_MS : p_ms;
    _apply(p_c, 0);   // 0 밝기에서 시작
}

void CL_L10_Led::_enterOff() {
    _state        = EN_L10_ST_OFF;
    _stateStartMs = (uint32_t)millis();
    _stateDurMs   = 0;
    _apply(EN_L10_COLOR_OFF, 0);
}
