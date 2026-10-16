// =======================================================
// File: src/v0415/M30_Gesture_0415.cpp
// =======================================================
#include "M30_Gesture_0415.h"

// =======================================================
// ctor
// =======================================================
CL_M30_Gesture::CL_M30_Gesture() {
    memset(&_flick,  0, sizeof(_flick));
    memset(&_linear, 0, sizeof(_linear));
    memset(&_tilt,   0, sizeof(_tilt));

    // 합리적 기본값
    _cfg.flick_p2p_th      = 400.0f;
    _cfg.flick_window_ms   = 200;
    _cfg.flick_cooldown_ms = 600;

    _cfg.linear_th         = 0.3f;
    _cfg.linear_impulse_th = 0.5f;
    _cfg.linear_window_ms  = 300;

    _cfg.tilt_angle_deg    = 15.0f;
    _cfg.tilt_hold_ms      = 300;
    _cfg.tilt_repeat_hz    = 3;
}

// =======================================================
// reset
// =======================================================
void CL_M30_Gesture::reset() {
    memset(&_flick,  0, sizeof(_flick));
    memset(&_linear, 0, sizeof(_linear));
    memset(&_tilt,   0, sizeof(_tilt));
}

// =======================================================
// update
// =======================================================
CL_M30_Gesture::ST_Output_t CL_M30_Gesture::update(
    float p_gx, float p_gy, float p_gz,
    float p_ax, float p_ay, float p_az,
    float p_roll, float p_pitch,
    bool  p_moveGateHeld,
    uint8_t p_mode,
    uint32_t p_nowMs)
{
    ST_Output_t v_out;

    // Flick는 Middle Hold 중에도 활성 (기존 정책 유지)
    // 상하 Flick만 Middle Hold 해제 시만 (D15)
    _flickPush(p_gz, p_gx);

    v_out.flick = _detectFlickYaw(p_gz, p_nowMs);           // 좌/우
    if (v_out.flick == EN_M30_DIR_NONE && !p_moveGateHeld) {
        v_out.flick = _detectFlickRoll(p_gx, p_nowMs);      // 상/하 (D15)
    }

    // Linear: Middle Hold 중만 (D14)
    if (p_moveGateHeld) {
        v_out.linear = _detectLinear(p_ax, p_ay, p_az,
                                     p_roll, p_pitch, p_nowMs);
    } else {
        _linear.accumRoll  = 0.0f;
        _linear.accumPitch = 0.0f;
        _linear.windowOpen = false;
    }

    // Tilt Hold: Mode 3 + Middle Hold 해제 시만 (D23)
    const bool v_tiltActive = (p_mode == 3) && !p_moveGateHeld;
    v_out.tilt = _detectTilt(p_roll, p_pitch, v_tiltActive, p_nowMs);

    return v_out;
}

// =======================================================
// Flick: 버퍼 push / peak-to-peak
// =======================================================
void CL_M30_Gesture::_flickPush(float p_yaw, float p_roll) {
    _flick.bufYaw [_flick.head] = p_yaw;
    _flick.bufRoll[_flick.head] = p_roll;
    _flick.head = (_flick.head + 1) % G_FLICK_BUF;
    if (_flick.count < G_FLICK_BUF) _flick.count++;
}

void CL_M30_Gesture::_flickPeakToPeak(float& p_outYawMin,  float& p_outYawMax,
                                      float& p_outRollMin, float& p_outRollMax) const {
    if (_flick.count == 0) {
        p_outYawMin = p_outYawMax = p_outRollMin = p_outRollMax = 0.0f;
        return;
    }

    float v_ymin = _flick.bufYaw[0],  v_ymax = v_ymin;
    float v_rmin = _flick.bufRoll[0], v_rmax = v_rmin;

    for (uint8_t i = 1; i < _flick.count; i++) {
        const float y = _flick.bufYaw [i];
        const float r = _flick.bufRoll[i];
        if (y < v_ymin) v_ymin = y;
        if (y > v_ymax) v_ymax = y;
        if (r < v_rmin) v_rmin = r;
        if (r > v_rmax) v_rmax = r;
    }

    p_outYawMin  = v_ymin;  p_outYawMax  = v_ymax;
    p_outRollMin = v_rmin;  p_outRollMax = v_rmax;
}

// =======================================================
// Flick 좌/우 (Yaw = gz)
// =======================================================
EN_M30_Dir_t CL_M30_Gesture::_detectFlickYaw(float /*p_gz*/, uint32_t p_nowMs) {
    if (_flick.count < 4) return EN_M30_DIR_NONE;
    if ((p_nowMs - _flick.lastFireMs) < _cfg.flick_cooldown_ms) return EN_M30_DIR_NONE;

    float v_ymin, v_ymax, v_rmin, v_rmax;
    _flickPeakToPeak(v_ymin, v_ymax, v_rmin, v_rmax);

    const float v_p2p = v_ymax - v_ymin;
    if (v_p2p < _cfg.flick_p2p_th) return EN_M30_DIR_NONE;

    // 부호 판정: |peak_max| > |peak_min| → +
    const float v_absMax = fabsf(v_ymax);
    const float v_absMin = fabsf(v_ymin);

    EN_M30_Dir_t v_dir = EN_M30_DIR_NONE;
    if (v_absMax > v_absMin) {
        // + 방향 = Left (프로젝트 관례: gz+ → 이전 슬라이드/좌)
        v_dir = EN_M30_DIR_LEFT;
    } else {
        v_dir = EN_M30_DIR_RIGHT;
    }

    _flick.lastFireMs = p_nowMs;
    // 쿨다운 동안 오탐 방지: 버퍼 리셋
    _flick.head  = 0;
    _flick.count = 0;
    return v_dir;
}

// =======================================================
// Flick 상/하 (Roll = gx)
// =======================================================
EN_M30_Dir_t CL_M30_Gesture::_detectFlickRoll(float /*p_gx*/, uint32_t p_nowMs) {
    if (_flick.count < 4) return EN_M30_DIR_NONE;
    if ((p_nowMs - _flick.lastFireMs) < _cfg.flick_cooldown_ms) return EN_M30_DIR_NONE;

    float v_ymin, v_ymax, v_rmin, v_rmax;
    _flickPeakToPeak(v_ymin, v_ymax, v_rmin, v_rmax);

    const float v_p2p = v_rmax - v_rmin;
    if (v_p2p < _cfg.flick_p2p_th) return EN_M30_DIR_NONE;

    const float v_absMax = fabsf(v_rmax);
    const float v_absMin = fabsf(v_rmin);

    EN_M30_Dir_t v_dir = EN_M30_DIR_NONE;
    if (v_absMax > v_absMin) v_dir = EN_M30_DIR_UP;
    else                     v_dir = EN_M30_DIR_DOWN;

    _flick.lastFireMs = p_nowMs;
    _flick.head  = 0;
    _flick.count = 0;
    return v_dir;
}

// =======================================================
// Linear: 선형 가속 임펄스 적분
// =======================================================
EN_M30_Dir_t CL_M30_Gesture::_detectLinear(
    float p_ax, float p_ay, float p_az,
    float p_roll, float p_pitch,
    uint32_t p_nowMs)
{

    // ---- 1) 중력 성분 제거 (raw - R * g) ----
    // 단순화: roll/pitch로 중력 벡터 회전시켜 빼기
    const float v_g = 1.0f;   // 1g (정규화)

    const float v_cr = cosf(p_roll);
    const float v_sr = sinf(p_roll);
    const float v_cp = cosf(p_pitch);
    const float v_sp = sinf(p_pitch);

    // 중력 벡터 (센서 좌표계)
    //   gx_g = -sin(pitch) * g
    //   gy_g =  sin(roll) * cos(pitch) * g
    //   gz_g =  cos(roll) * cos(pitch) * g
    const float v_gx_g = -v_sp * v_g;
    const float v_gy_g =  v_sr * v_cp * v_g;
    const float v_gz_g =  v_cr * v_cp * v_g;

    // 선형 가속도 (g 단위 → m/s²)
    const float v_lin_x = (p_ax - v_gx_g) * G_M30_GRAVITY;
    const float v_lin_y = (p_ay - v_gy_g) * G_M30_GRAVITY;
    const float v_lin_z = (p_az - v_gz_g) * G_M30_GRAVITY;

    // ---- 2) 윈도우 관리 ----
    if (!_linear.windowOpen) {
        _linear.windowOpen    = true;
        _linear.windowStartMs = p_nowMs;
        _linear.accumRoll     = 0.0f;
        _linear.accumPitch    = 0.0f;
    }

    // ---- 3) 임계 초과 시 적분 (dt = 8ms 가정) ----
    //   커서 X는 gy(gx), 커서 Y는 gx(gy) 관례 반영:
    //   여기선 물리 축 그대로 판정하고, 결과를 4방향으로 매핑
    const float v_dt = (float)G_M30_LINEAR_DT_MS / 1000.0f;

    // Roll 축(긴 축, gx 물리) → Linear U/D
    if (fabsf(v_lin_x) > _cfg.linear_th) {
        _linear.accumRoll += v_lin_x * v_dt;
    }
    // Pitch 축(좌우 축, gy 물리) → Linear L/R
    if (fabsf(v_lin_y) > _cfg.linear_th) {
        _linear.accumPitch += v_lin_y * v_dt;
    }

    // ---- 4) 윈도우 종료 판정 ----
    const uint32_t v_elapsed = p_nowMs - _linear.windowStartMs;
    if (v_elapsed < _cfg.linear_window_ms) return EN_M30_DIR_NONE;

    _linear.windowOpen = false;

    // ---- 5) 지배축 + 부호 판정 ----
    const float v_absR = fabsf(_linear.accumRoll);
    const float v_absP = fabsf(_linear.accumPitch);

    EN_M30_Dir_t v_dir = EN_M30_DIR_NONE;

    if (v_absR > v_absP) {
        if (v_absR > _cfg.linear_impulse_th) {
            v_dir = (_linear.accumRoll > 0) ? EN_M30_DIR_UP : EN_M30_DIR_DOWN;
        }
    } else {
        if (v_absP > _cfg.linear_impulse_th) {
            v_dir = (_linear.accumPitch > 0) ? EN_M30_DIR_RIGHT : EN_M30_DIR_LEFT;
        }
    }

    _linear.accumRoll  = 0.0f;
    _linear.accumPitch = 0.0f;
    return v_dir;
}

// =======================================================
// Tilt Hold (Mode 3, Middle Hold 해제 시만)
// =======================================================
EN_M30_Dir_t CL_M30_Gesture::_detectTilt(
    float p_roll, float p_pitch,
    bool p_active,
    uint32_t p_nowMs)
{
    if (!p_active) {
        _tilt.activeDir   = EN_M30_DIR_NONE;
        _tilt.dirStartMs  = 0;
        _tilt.lastRepeatMs = 0;
        _tilt.fired       = false;
        return EN_M30_DIR_NONE;
    }

    const float v_angleTh = _cfg.tilt_angle_deg * DEG_TO_RAD;

    // 방향 판정 (지배축)
    const float v_absRoll  = fabsf(p_roll);
    const float v_absPitch = fabsf(p_pitch);

    EN_M30_Dir_t v_newDir = EN_M30_DIR_NONE;

    if (v_absRoll < v_angleTh && v_absPitch < v_angleTh) {
        v_newDir = EN_M30_DIR_NONE;
    } else if (v_absRoll > v_absPitch) {
        v_newDir = (p_roll  > 0) ? EN_M30_DIR_UP   : EN_M30_DIR_DOWN;
    } else {
        v_newDir = (p_pitch > 0) ? EN_M30_DIR_RIGHT : EN_M30_DIR_LEFT;
    }

    // 방향 변경 → 리셋
    if (v_newDir != _tilt.activeDir) {
        _tilt.activeDir    = v_newDir;
        _tilt.dirStartMs   = p_nowMs;
        _tilt.lastRepeatMs = 0;
        _tilt.fired        = false;
        return EN_M30_DIR_NONE;
    }

    // 방향 유지 중
    if (_tilt.activeDir == EN_M30_DIR_NONE) return EN_M30_DIR_NONE;

    // 첫 발동: hold_ms 유지 필요
    if (!_tilt.fired) {
        if ((p_nowMs - _tilt.dirStartMs) >= _cfg.tilt_hold_ms) {
            _tilt.fired        = true;
            _tilt.lastRepeatMs = p_nowMs;
            return _tilt.activeDir;
        }
        return EN_M30_DIR_NONE;
    }

    // 반복 발동 (repeat_hz)
    const uint32_t v_repeatMs = (_cfg.tilt_repeat_hz > 0)
                              ? (1000u / _cfg.tilt_repeat_hz) : 333u;
    if ((p_nowMs - _tilt.lastRepeatMs) >= v_repeatMs) {
        _tilt.lastRepeatMs = p_nowMs;
        return _tilt.activeDir;
    }
    return EN_M30_DIR_NONE;
}
