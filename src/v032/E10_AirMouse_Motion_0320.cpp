// =======================================================
// File: E10_AirMouse_Motion_0320.cpp
// =======================================================
#include "E10_AirMouse_0320.h"

// =======================================================
// Precision shaping (joystick-like)
// =======================================================
void CL_E10_EliteAirMouse::_applyPrecision(float& p_fx, float& p_fy) {
    if (_precSub == EN_PREC_OFF) return;

    const uint8_t v_mode = (uint8_t)min((uint8_t)EN_C10_E10_PREC_PPT, _precision_mode);
    const ST_E10_PrecProfile_t& v_pf = G_E10_PREC_PROFILES[v_mode];

    if (fabsf(p_fx) < _precDeadzone) p_fx = 0.0f;
    if (fabsf(p_fy) < _precDeadzone) p_fy = 0.0f;

    auto v_shape = [&](float p_v) -> float {
        float v_a = fabsf(p_v);
        if (v_a < 0.0001f) return 0.0f;

        float v_n   = min(1.0f, v_a / (float)_precMaxStep);
        float v_b   = v_n + (_precAccel * v_n * v_n);
        float v_out = v_b * (float)_precMaxStep;

        v_out *= (_precGain * v_pf.gain);

        return (p_v >= 0.0f) ? v_out : -v_out;
    };

    float v_tx = constrain(v_shape(p_fx), -(float)_precMaxStep, (float)_precMaxStep);
    float v_ty = constrain(v_shape(p_fy), -(float)_precMaxStep, (float)_precMaxStep);

    const float v_pfSmooth = (float)v_pf.alpha / 255.0f;
    float v_sm = _precSmooth;
    if (v_pfSmooth > v_sm) v_sm = v_pfSmooth;

    if (_precProfile == 1) {
        v_sm = min(0.95f, max(0.60f, v_sm));
    }

    if (v_pf.accel_limit > 0.0f) {
        const float v_dx = v_tx - _precSmX;
        const float v_dy = v_ty - _precSmY;
        const float v_lim = v_pf.accel_limit;

        if (fabsf(v_dx) > v_lim) v_tx = _precSmX + (v_dx > 0 ? v_lim : -v_lim);
        if (fabsf(v_dy) > v_lim) v_ty = _precSmY + (v_dy > 0 ? v_lim : -v_lim);
    }

    _precSmX = _precSmX * v_sm + v_tx * (1.0f - v_sm);
    _precSmY = _precSmY * v_sm + v_ty * (1.0f - v_sm);

    p_fx = _precSmX;
    p_fy = _precSmY;
}

// =======================================================
// FSM update
// =======================================================
void CL_E10_EliteAirMouse::_fsmUpdate(bool p_btnScroll, bool p_btnModeLongToggle, float p_gyroAbs) {
    if (p_btnModeLongToggle) {
        const uint32_t v_nowMs = (uint32_t)millis();
        if ((v_nowMs - _lastModeToggleMs) >= _modeToggleCooldownMs) {
            _isPptMode = !_isPptMode;
            _lastModeToggleMs = v_nowMs;

            (void)forceReleaseButtons();
            _failsafeReleaseCount++;

            _precSub = EN_PREC_OFF;
            _precSmX = 0.0f;
            _precSmY = 0.0f;
        }
    }

    // 1) scroll 최우선
    if (p_btnScroll) {
        _fsm = EN_FSM_SCROLL;
        _precSub = EN_PREC_OFF;
        _precSmX = 0.0f;
        _precSmY = 0.0f;
        return;
    }

    // 2) base fsm
    _fsm = _isPptMode ? EN_FSM_PPT : EN_FSM_AIR;

    // 3) precision overlay
    if (_precision_mode == (uint8_t)EN_C10_E10_PREC_OFF) {
        _precSub = EN_PREC_OFF;
        return;
    }

    const uint32_t v_now = (uint32_t)millis();

    if (_precSub == EN_PREC_OFF) {
        _precSub = EN_PREC_ENTRY;
        _precT0  = v_now;
        return;
    }

    if (_precSub == EN_PREC_ENTRY) {
        if (p_gyroAbs <= _precEntryStillDeg) {
            if (v_now - _precT0 >= _precEntryMs) _precSub = EN_PREC_TRACK;
        } else {
            _precT0 = v_now;
        }
        return;
    }

    if (_precSub == EN_PREC_TRACK) {
        if (p_gyroAbs >= _precExitMoveDeg) {
            _precSub = EN_PREC_EXIT;
            _precT0  = v_now;
        }
        return;
    }

    if (_precSub == EN_PREC_EXIT) {
        if (p_gyroAbs <= _precEntryStillDeg) {
            if (v_now - _precT0 >= _precExitMs) _precSub = EN_PREC_TRACK;
        } else {
            _precT0 = v_now;
        }
        return;
    }

    _precSub = EN_PREC_ENTRY;
    _precT0  = v_now;
}
