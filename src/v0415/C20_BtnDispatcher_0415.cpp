// File: src/v0415/C20_BtnDispatcher_0415.cpp
// =======================================================
#include "C20_BtnDispatcher_0415.h"

CL_C20_BtnDispatcher::CL_C20_BtnDispatcher() {
    memset(_btn, 0, sizeof(_btn));
    for (uint8_t i = 0; i < EN_C20_BTN_MAX; i++) {
        _btn[i].phase = PHASE_IDLE;
    }
}

void CL_C20_BtnDispatcher::begin() {
    const uint32_t v_now = (uint32_t)millis();
    for (uint8_t i = 0; i < EN_C20_BTN_MAX; i++) {
        pinMode(G_PINS[i], INPUT_PULLUP);
        _btn[i].stableState     = false;
        _btn[i].lastRaw         = false;
        _btn[i].lastRawChangeMs = v_now;
        _btn[i].phase           = PHASE_IDLE;
        _btn[i].longFired       = false;
        _btn[i].hold2sFired     = false;
        _btn[i].hold3sFired     = false;
        _btn[i].stableCount     = 0;
        _btn[i].rawDownMs       = 0;
    }
}

void CL_C20_BtnDispatcher::update() {
    const uint32_t v_now = (uint32_t)millis();
    for (uint8_t i = 0; i < EN_C20_BTN_MAX; i++) {
        _updateOne(i, v_now);
    }
}

void CL_C20_BtnDispatcher::_updateOne(uint8_t p_btnId, uint32_t p_now) {
    ST_BtnState_t& b = _btn[p_btnId];

    const bool v_raw = (digitalRead(G_PINS[p_btnId]) == LOW);

    if (v_raw != b.lastRaw) {
        b.lastRaw         = v_raw;
        b.lastRawChangeMs = p_now;
        b.stableCount     = 1;

        if (v_raw && b.rawDownMs == 0) {
            b.rawDownMs = p_now;
        } else if (!v_raw && !b.stableState) {
            b.rawDownMs = 0;
        }
    } else {
        if (b.stableCount < 255) b.stableCount++;
    }

    const uint16_t v_debounceMs = b.stableState ? _debounceReleaseMs
                                                : _debouncePressMs;

    const bool v_timeOk  = (p_now - b.lastRawChangeMs) >= v_debounceMs;
    const bool v_countOk = b.stableCount >= _debounceMinTicks;

    if (v_timeOk && v_countOk && (v_raw != b.stableState)) {
        b.stableState = v_raw;
        b.stableCount = 0;
        _onStableChange(p_btnId, v_raw, p_now);
    }

    _checkTimers(p_btnId, p_now);
}

void CL_C20_BtnDispatcher::_onStableChange(uint8_t p_btnId, bool p_stable, uint32_t p_now) {
    ST_BtnState_t& b = _btn[p_btnId];

    if (p_stable) {
        b.downMs      = (b.rawDownMs > 0) ? b.rawDownMs : p_now;
        b.longFired   = false;
        b.hold2sFired = false;
        b.hold3sFired = false;

        if (b.phase == PHASE_WAIT_CLICK) {
            _emit(p_btnId, EN_C20_EVT_DOUBLE);
            b.phase = PHASE_DOUBLE;
        } else {
            _emit(p_btnId, EN_C20_EVT_DOWN);
            b.phase = PHASE_PRESSED;
        }
    } else {
        b.upMs = p_now;
        _emit(p_btnId, EN_C20_EVT_UP);

        const uint32_t v_heldMs = (b.rawDownMs > 0) ? (b.lastRawChangeMs - b.rawDownMs)
                                                    : (p_now - b.downMs);

        if (b.phase == PHASE_PRESSED) {
            if (b.longFired || b.hold2sFired || b.hold3sFired) {
                b.phase = PHASE_IDLE;
            } else if (v_heldMs < _minClickMs) {
                b.phase = PHASE_IDLE;   // DISCARD
            } else {
                b.phase            = PHASE_WAIT_CLICK;
                b.waitClickStartMs = p_now;
            }
        } else if (b.phase == PHASE_DOUBLE) {
            b.phase = PHASE_IDLE;
        }

        b.rawDownMs = 0;
    }
}

void CL_C20_BtnDispatcher::_checkTimers(uint8_t p_btnId, uint32_t p_now) {
    ST_BtnState_t& b = _btn[p_btnId];

    if (b.phase == PHASE_PRESSED) {
        const uint32_t v_held = p_now - b.downMs;

        if (!b.longFired && v_held >= _longDelayMs) {
            _emit(p_btnId, EN_C20_EVT_LONG);
            b.longFired = true;
        }
        if (!b.hold2sFired && v_held >= _hold2sMs) {
            _emit(p_btnId, EN_C20_EVT_HOLD_2S);
            b.hold2sFired = true;
        }
        if (!b.hold3sFired && v_held >= _hold3sMs) {
            _emit(p_btnId, EN_C20_EVT_HOLD_3S);
            b.hold3sFired = true;
        }
    }

    if (b.phase == PHASE_WAIT_CLICK && (p_now - b.waitClickStartMs) >= _doubleDelayMs) {
        _emit(p_btnId, EN_C20_EVT_CLICK);
        b.phase = PHASE_IDLE;
    }
}

void CL_C20_BtnDispatcher::_emit(uint8_t p_btnId, uint8_t p_evt) {
    if (_cb) _cb(_ctx, p_btnId, p_evt);
}


// =======================================================
// [v0415 L5-A3-11 fix] resetButton — 방어적 타임스탬프 초기화
// -------------------------------------------------------
// 이전 (v0415 초기):
//   b.downMs           = 0;
//   b.upMs             = 0;
//   b.waitClickStartMs = 0;
//   → reset 직후 release edge 발생 시 heldMs 계산이 거대값 → LONG 오발화 위험
//     (phase가 IDLE이라 현재는 미발화이지만 방어적 초기화가 안전)
//
// v0415 fix:
//   b.downMs           = v_now;
//   b.upMs             = v_now;
//   b.waitClickStartMs = v_now;
//   → heldMs = (rawDownMs>0) ? ... : (now - v_now) = 0~수ms (안전)
// =======================================================
void CL_C20_BtnDispatcher::resetButton(uint8_t p_btnId) {
    if (p_btnId >= EN_C20_BTN_MAX) return;
    ST_BtnState_t& b = _btn[p_btnId];

    const uint32_t v_now = (uint32_t)millis();

    b.phase            = PHASE_IDLE;
    b.downMs           = v_now;    // [v0415 fix] 0 대신 현재 시각
    b.upMs             = v_now;    // [v0415 fix]
    b.waitClickStartMs = v_now;    // [v0415 fix]
    b.longFired        = false;
    b.hold2sFired      = false;
    b.hold3sFired      = false;

    // [v0415 정책] stableState/lastRaw 유지 (물리 상태 반영)
    b.lastRawChangeMs = v_now;
    b.stableCount     = 0;
    b.rawDownMs       = 0;
}

void CL_C20_BtnDispatcher::resetAll() {
    for (uint8_t i = 0; i < EN_C20_BTN_MAX; i++) resetButton(i);
}


