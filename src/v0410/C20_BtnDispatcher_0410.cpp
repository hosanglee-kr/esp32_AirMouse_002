// =======================================================
// File: src/v0410/C20_BtnDispatcher_0410.cpp
// =======================================================
#include "C20_BtnDispatcher_0410.h"

CL_C20_BtnDispatcher::CL_C20_BtnDispatcher() {
    memset(_btn, 0, sizeof(_btn));
    for (uint8_t i = 0; i < EN_C20_BTN_MAX; i++) {
        _btn[i].phase = PHASE_IDLE;
    }
}

void CL_C20_BtnDispatcher::begin() {
    for (uint8_t i = 0; i < EN_C20_BTN_MAX; i++) {
        pinMode(G_PINS[i], INPUT_PULLUP);
        _btn[i].stableState      = false;   // released (HIGH)
        _btn[i].lastRaw          = false;
        _btn[i].lastRawChangeMs  = millis();
        _btn[i].phase            = PHASE_IDLE;
        _btn[i].longFired        = false;
        _btn[i].hold2sFired      = false;
        _btn[i].hold3sFired      = false;
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

    // ---- Debounce ----
    const bool v_raw = (digitalRead(G_PINS[p_btnId]) == LOW);
    if (v_raw != b.lastRaw) {
        b.lastRaw = v_raw;
        b.lastRawChangeMs = p_now;
    }

    if ((p_now - b.lastRawChangeMs) >= _debounceMs) {
        if (v_raw != b.stableState) {
            b.stableState = v_raw;
            _onStableChange(p_btnId, v_raw, p_now);
        }
    }

    // ---- Timers ----
    _checkTimers(p_btnId, p_now);
}

void CL_C20_BtnDispatcher::_onStableChange(uint8_t p_btnId, bool p_stable, uint32_t p_now) {
    ST_BtnState_t& b = _btn[p_btnId];

    if (p_stable) {
        // =========== PRESS ===========
        b.downMs       = p_now;
        b.longFired    = false;
        b.hold2sFired  = false;
        b.hold3sFired  = false;

        if (b.phase == PHASE_WAIT_CLICK) {
            // 두 번째 클릭 → DOUBLE
            _emit(p_btnId, EN_C20_EVT_DOUBLE);
            b.phase = PHASE_DOUBLE;
            // (UP은 이후 무시)
        } else {
            _emit(p_btnId, EN_C20_EVT_DOWN);
            b.phase = PHASE_PRESSED;
        }
    } else {
        // =========== RELEASE ===========
        b.upMs = p_now;
        _emit(p_btnId, EN_C20_EVT_UP);

        if (b.phase == PHASE_PRESSED) {
            if (b.longFired || b.hold2sFired || b.hold3sFired) {
                // Long/Hold로 이미 소비
                b.phase = PHASE_IDLE;
            } else {
                // 클릭 후보 → double 대기
                b.phase = PHASE_WAIT_CLICK;
                b.waitClickStartMs = p_now;
            }
        } else if (b.phase == PHASE_DOUBLE) {
            // 두 번째 UP → IDLE
            b.phase = PHASE_IDLE;
        }
    }
}

void CL_C20_BtnDispatcher::_checkTimers(uint8_t p_btnId, uint32_t p_now) {
    ST_BtnState_t& b = _btn[p_btnId];

    // Long / Hold (press 유지 중)
    if (b.phase == PHASE_PRESSED || b.phase == PHASE_DOUBLE) {
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

    // Click 타임아웃 (double 대기 종료)
    if (b.phase == PHASE_WAIT_CLICK &&
        (p_now - b.waitClickStartMs) >= _doubleDelayMs) {
        _emit(p_btnId, EN_C20_EVT_CLICK);
        b.phase = PHASE_IDLE;
    }
}

void CL_C20_BtnDispatcher::_emit(uint8_t p_btnId, uint8_t p_evt) {
    if (_cb) _cb(_ctx, p_btnId, p_evt);
}

void CL_C20_BtnDispatcher::resetButton(uint8_t p_btnId) {
    if (p_btnId >= EN_C20_BTN_MAX) return;
    ST_BtnState_t& b = _btn[p_btnId];
    b.phase          = PHASE_IDLE;
    b.longFired      = false;
    b.hold2sFired    = false;
    b.hold3sFired    = false;
}

void CL_C20_BtnDispatcher::resetAll() {
    for (uint8_t i = 0; i < EN_C20_BTN_MAX; i++) resetButton(i);
}
