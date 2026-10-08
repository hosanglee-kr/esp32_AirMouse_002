// =======================================================
// File: src/v0415/E10_AirMouse_Motion_0415.cpp
// =======================================================
#include "E10_AirMouse_0415.h"

// =======================================================
// [v0415] Precision/FSM 매직 넘버 상수화 (L5-A2-01, L6d-A2-01)
// =======================================================
namespace {

// Precision FSM
static constexpr uint8_t G_PREC_ALPHA_MAX  = 255;   // ST_E10_PrecProfile_t.alpha 상한
static constexpr float   G_PREC_SMOOTH_MIN = 0.60f; // profile=1 클램프 하한
static constexpr float   G_PREC_SMOOTH_MAX = 0.95f; // profile=1 클램프 상한
static constexpr uint8_t G_PREC_PROFILE_JOYSTICK = 1;  // profile 1 = 조이스틱 스타일

}  // namespace

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

    const float v_pfSmooth = (float)v_pf.alpha / (float)G_PREC_ALPHA_MAX;
    float v_sm = _precSmooth;
    if (v_pfSmooth > v_sm) v_sm = v_pfSmooth;

    if (_precProfile == G_PREC_PROFILE_JOYSTICK) {
        v_sm = min(G_PREC_SMOOTH_MAX, max(G_PREC_SMOOTH_MIN, v_sm));
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
// [v0415 L6d-A1-01] FSM update — Dead 분기 정리
// -------------------------------------------------------
// 이전 (v0412):
//   _fsmUpdate(bool p_btnScroll, bool p_btnModeLongToggle, float p_gyroAbs)
//   - 호출부에서 항상 (false, false, v_gyroAbs) 전달 → 두 분기 Dead
//   - p_btnScroll: EN_FSM_SCROLL 상태 진입 (미사용)
//   - p_btnModeLongToggle: _isPptMode 토글 + releaseAll (미사용)
//
// v0415:
//   _fsmUpdate(float p_gyroAbs)
//   - FSM 판정: _activeMode == 2 → EN_FSM_PPT, 그 외 → EN_FSM_AIR
//   - Precision sub 상태머신만 유지 (ENTRY/TRACK/EXIT)
//   - _isPptMode 필드 삭제됨 (Round G)
// =======================================================
void CL_E10_EliteAirMouse::_fsmUpdate(float p_gyroAbs) {
    // 1) base fsm (active_mode 기반)
    _fsm = (_activeMode == 2) ? EN_FSM_PPT : EN_FSM_AIR;

    // 2) precision overlay
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

// =======================================================
// [Phase 3] Snap-to-Axis
// =======================================================
void CL_E10_EliteAirMouse::_applySnapToAxis(float& p_fx, float& p_fy) {
    // [R2-C-1] config 스냅샷 (락 하 read)
    ST_C10_MotionAdv_Snap_t cfg;
    _lock();
    cfg = _cfgProfile.e10.motion_adv.snap;
    _unlock();

    const bool v_active =
        cfg.enable &&
        (cfg.mode_mask & (1 << (_activeMode - 1))) != 0;

    if (!v_active) {
        _snapActiveAxis      = E10_SNAP_NONE;
        _snapCandidate       = E10_SNAP_NONE;
        _snapCandidateFrames = 0;
        return;
    }

    const float v_absX = fabsf(p_fx);
    const float v_absY = fabsf(p_fy);

    const bool v_canH = (cfg.axis_mode == 0) || (cfg.axis_mode == 1);
    const bool v_canV = (cfg.axis_mode == 0) || (cfg.axis_mode == 2);

    // 1) 활성 상태 → 이탈만 검사
    if (_snapActiveAxis == E10_SNAP_HORIZ) {
        const float v_exitTh = 1.0f / cfg.ratio_enter;
        if (v_absY > v_absX * v_exitTh) {
            _snapActiveAxis      = E10_SNAP_NONE;
            _snapCandidate       = E10_SNAP_NONE;
            _snapCandidateFrames = 0;
        }
    } else if (_snapActiveAxis == E10_SNAP_VERT) {
        const float v_exitTh = 1.0f / cfg.ratio_enter;
        if (v_absX > v_absY * v_exitTh) {
            _snapActiveAxis      = E10_SNAP_NONE;
            _snapCandidate       = E10_SNAP_NONE;
            _snapCandidateFrames = 0;
        }
    }

    // 2) NONE → 후보 + Confirm Frames
    if (_snapActiveAxis == E10_SNAP_NONE) {
        EN_E10_SnapAxis_t v_newCand = E10_SNAP_NONE;

        if (v_canH && v_absX > v_absY * cfg.ratio_enter) {
            v_newCand = E10_SNAP_HORIZ;
        } else if (v_canV && v_absY > v_absX * cfg.ratio_enter) {
            v_newCand = E10_SNAP_VERT;
        }

        if (v_newCand == _snapCandidate) {
            if (_snapCandidateFrames < 255) _snapCandidateFrames++;
            if (_snapCandidate != E10_SNAP_NONE &&
                _snapCandidateFrames >= cfg.confirm_frames) {
                _snapActiveAxis = _snapCandidate;
            }
        } else {
            _snapCandidate       = v_newCand;
            _snapCandidateFrames = (v_newCand == E10_SNAP_NONE) ? 0 : 1;
        }
    }

    // 3) Soft Snap 감쇠
    if (_snapActiveAxis == E10_SNAP_HORIZ) {
        p_fy *= (1.0f - cfg.strength);
    } else if (_snapActiveAxis == E10_SNAP_VERT) {
        p_fx *= (1.0f - cfg.strength);
    }
}

// =======================================================
// [Phase 1] Click-Freeze FSM
// =======================================================
void CL_E10_EliteAirMouse::_applyClickFreeze(float& p_fx, float& p_fy,
                                             float p_rawDx, float p_rawDy,
                                             float p_gyroAbs,
                                             bool  p_btnDown) {
    // [R2-C-1] config 스냅샷 (락 하 read)
    ST_C10_MotionAdv_ClickFreeze_t cfg;
    _lock();
    cfg = _cfgProfile.e10.motion_adv.click_freeze;
    _unlock();

    // Move Gate가 활성 상태일 때만 커서 이동 → Move Gate 풀리면 IDLE 리셋
    if (!cfg.enable || !_moveGateHeld) {
        _freezeState = E10_FREEZE_IDLE;
        return;
    }

    constexpr uint32_t v_dtMs = 8;

    switch (_freezeState) {
        case E10_FREEZE_IDLE:
            if (p_btnDown && p_gyroAbs < cfg.gyro_th) {
                _freezeState = E10_FREEZE_LOCKED;
                _freezeTimer = 0;
                _accumDx     = 0.0f;
                _accumDy     = 0.0f;
                p_fx = 0.0f;
                p_fy = 0.0f;
            }
            break;

        case E10_FREEZE_LOCKED:
            _freezeTimer += v_dtMs;
            if (!p_btnDown) {
                _freezeState = E10_FREEZE_HOLD;
                _holdTimer   = 0;
                _accumDx     = 0.0f;
                _accumDy     = 0.0f;
                p_fx = 0.0f;
                p_fy = 0.0f;
            } else if (_freezeTimer > cfg.max_ms || p_gyroAbs > cfg.freeze_move_th) {
                _freezeState = E10_FREEZE_IDLE;
            } else {
                p_fx = 0.0f;
                p_fy = 0.0f;
            }
            break;

        case E10_FREEZE_HOLD: {
            _holdTimer += v_dtMs;
            _accumDx += p_rawDx;
            _accumDy += p_rawDy;

            const float v_distSq = _accumDx * _accumDx + _accumDy * _accumDy;
            const float v_thSq   = cfg.move_th * cfg.move_th;

            if (v_distSq > v_thSq || p_btnDown) {
                _freezeState = E10_FREEZE_IDLE;
            } else if (_holdTimer >= cfg.hold_ms) {
                _freezeState = E10_FREEZE_FADEOUT;
                _fadeTimer   = 0;
                p_fx = 0.0f;
                p_fy = 0.0f;
            } else {
                p_fx = 0.0f;
                p_fy = 0.0f;
            }
            break;
        }

        case E10_FREEZE_FADEOUT: {
            _fadeTimer += v_dtMs;
            _accumDx += p_rawDx;
            _accumDy += p_rawDy;

            const float v_distSq = _accumDx * _accumDx + _accumDy * _accumDy;
            const float v_thSq   = cfg.move_th * cfg.move_th;

            if (v_distSq > v_thSq || _fadeTimer >= cfg.fadeout_ms || p_btnDown) {
                _freezeState = E10_FREEZE_IDLE;
            } else {
                const float v_scale = (float)_fadeTimer / (float)cfg.fadeout_ms;
                p_fx *= v_scale;
                p_fy *= v_scale;
            }
            break;
        }
    }
}
