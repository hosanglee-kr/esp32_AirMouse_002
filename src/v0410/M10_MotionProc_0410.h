#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : M10_MotionProc_0410.h
 * 모듈약어 : M10
 * 모듈명 : Motion Processor (v0410, Roll + Pitch)
 * ------------------------------------------------------
 * 기능 요약
 *  - 상보 필터: Roll + Pitch 2축 자세 추정
 *  - [Phase 2] Adaptive Variable EMA (Smoothstep + Reversal Reset)
 *  - Sigmoid 가속 + Zero Snap
 *  - Click Lock (Hard / Soft)
 *  - 자세 기반 커서 보정 (기울기 보상)
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <math.h>

class CL_M10_AdvancedMotionProcessor {
  public:
    // ==================================================
    // [Phase 2] Adaptive EMA Config
    // ==================================================
    struct ST_EmaCfg_t {
        float alpha_min;
        float alpha_max;
        float deadzone_th;
        float fast_th;
        float reversal_th;
        bool  reversal_reset;
    };

  private:
    float _lpfX = 0.0f;
    float _lpfY = 0.0f;

    // 자세 상태 (rad)
    float _roll  = 0.0f;
    float _pitch = 0.0f;

    float _dpiGain = 22.0f;

    unsigned long _lastClickTime      = 0;
    bool          _isClickStabilizing = false;

    bool  _hardClickLock = false;
    float _zeroSnapTh    = 0.6f;

    // 상보 필터 계수
    static constexpr float G_COMP_ALPHA = 0.98f;

    // 클릭 안정화 시간
    static constexpr uint32_t G_CLICK_LOCK_MS = 150;

    // 시그모이드 파라미터
    static constexpr float G_SIG_SLOPE    = 0.8f;
    static constexpr float G_SIG_OFFSET   = 2.0f;
    static constexpr float G_SIG_DEADBAND = 0.4f;

    // ==================================================
    // [Phase 2] Adaptive Variable EMA
    // ==================================================
    ST_EmaCfg_t _emaCfg = {
        0.05f, 0.80f, 3.0f, 15.0f, 8.0f, true
    };
    bool _prevSignX_neg = false;
    bool _prevSignY_neg = false;
    bool _emaInitDone   = false;

    static float _computeEmaAlpha(float p_mag, const ST_EmaCfg_t& p_cfg);

  public:
    CL_M10_AdvancedMotionProcessor() {}

    void setDPI(int p_level) { _dpiGain = 15.0f + (p_level * 7.0f); }
    void setHardClickLock(bool p_enable) { _hardClickLock = p_enable; }
    void setZeroSnapTh(float p_th) { _zeroSnapTh = p_th; }

    // ==================================================
    // [Phase 2] Adaptive EMA setters
    // ==================================================
    void setEmaConfig(const ST_EmaCfg_t& p_cfg) { _emaCfg = p_cfg; }
    const ST_EmaCfg_t& getEmaConfig() const { return _emaCfg; }

    void resetEmaState() {
        _lpfX = 0.0f;
        _lpfY = 0.0f;
        _prevSignX_neg = false;
        _prevSignY_neg = false;
        _emaInitDone   = false;
    }

    // 자세 초기화
    void resetOrientation() {
        _roll  = 0.0f;
        _pitch = 0.0f;
    }

    float getRoll()  const { return _roll; }
    float getPitch() const { return _pitch; }

    void notifyClick() {
        _lastClickTime      = millis();
        _isClickStabilizing = true;
    }

    // ================================================
    // 상보 필터 (Roll + Pitch)
    // ================================================
    void updateOrientation2(float p_ax, float p_ay, float p_az,
                            float p_gx, float p_gy,
                            float p_dt_s) {
        float v_accelRoll = atan2f(p_ay, p_az);
        _roll  = G_COMP_ALPHA * (_roll  + (p_gx * DEG_TO_RAD) * p_dt_s)
               + (1.0f - G_COMP_ALPHA) * v_accelRoll;

        float v_accelPitch = atan2f(-p_ax, p_az);
        _pitch = G_COMP_ALPHA * (_pitch + (p_gy * DEG_TO_RAD) * p_dt_s)
               + (1.0f - G_COMP_ALPHA) * v_accelPitch;
    }

 
    // ================================================
    // 커서 좌표 계산 (Roll 보상 + Adaptive EMA + Sigmoid + Zero Snap)
    // ================================================
    void process(float p_rawX, float p_rawY, int& p_outX, int& p_outY) {
        // Roll 보상
        float v_cosR = cosf(_roll);
        float v_sinR = sinf(_roll);

        float v_compX = p_rawX * v_cosR - p_rawY * v_sinR;
        float v_compY = p_rawX * v_sinR + p_rawY * v_cosR;

        // 클릭 안정화
        if (_isClickStabilizing) {
            if (millis() - _lastClickTime < G_CLICK_LOCK_MS) {
                if (_hardClickLock) {
                    p_outX = 0;
                    p_outY = 0;
                    return;
                }
                v_compX *= 0.05f;
                v_compY *= 0.05f;
            } else {
                _isClickStabilizing = false;
            }
        }

        // ====================================================
        // [Phase 2] Adaptive Variable EMA
        //   - Smoothstep α + Reversal Reset
        // ====================================================
        // 첫 프레임 초기화
        if (!_emaInitDone) {
            _prevSignX_neg = (v_compX < 0.0f);
            _prevSignY_neg = (v_compY < 0.0f);
            _lpfX = v_compX;
            _lpfY = v_compY;
            _emaInitDone = true;
        }

        const float v_mag   = sqrtf(v_compX * v_compX + v_compY * v_compY);
        const float v_alpha = _computeEmaAlpha(v_mag, _emaCfg);

        // Reversal Reset
        if (_emaCfg.reversal_reset) {
            const bool v_signX_neg = (v_compX < 0.0f);
            const bool v_signY_neg = (v_compY < 0.0f);

            if (v_signX_neg != _prevSignX_neg && fabsf(v_compX) > _emaCfg.reversal_th) {
                _lpfX = 0.0f;
            }
            if (v_signY_neg != _prevSignY_neg && fabsf(v_compY) > _emaCfg.reversal_th) {
                _lpfY = 0.0f;
            }

            _prevSignX_neg = v_signX_neg;
            _prevSignY_neg = v_signY_neg;
        }

        // EMA 적용
        _lpfX = _lpfX + v_alpha * (v_compX - _lpfX);
        _lpfY = _lpfY + v_alpha * (v_compY - _lpfY);

        // Sigmoid 비선형 가속
        auto v_applySigmoid = [&](float p_input) -> float {
            float v_av = fabsf(p_input);
            if (v_av < G_SIG_DEADBAND) return 0.0f;
            float v_out = (_dpiGain / (1.0f + expf(-G_SIG_SLOPE * (v_av - G_SIG_OFFSET))));
            return v_out * (p_input >= 0 ? 1.0f : -1.0f);
        };

        float v_x = v_applySigmoid(_lpfX);
        float v_y = v_applySigmoid(_lpfY);

        // Zero Snap
        if (fabsf(v_x) < _zeroSnapTh) v_x = 0.0f;
        if (fabsf(v_y) < _zeroSnapTh) v_y = 0.0f;

        p_outX = (int)v_x;
        p_outY = (int)v_y;
    }
};

// ====================================================
// [Phase 2] Smoothstep 기반 α 연속 계산
// ====================================================
inline float CL_M10_AdvancedMotionProcessor::_computeEmaAlpha(
    float p_mag, const ST_EmaCfg_t& p_cfg)
{
    if (p_mag <= p_cfg.deadzone_th) return p_cfg.alpha_min;
    if (p_mag >= p_cfg.fast_th)     return p_cfg.alpha_max;

    const float v_t = (p_mag - p_cfg.deadzone_th) / (p_cfg.fast_th - p_cfg.deadzone_th);
    const float v_t_smooth = v_t * v_t * (3.0f - 2.0f * v_t);

    return p_cfg.alpha_min + v_t_smooth * (p_cfg.alpha_max - p_cfg.alpha_min);
}
