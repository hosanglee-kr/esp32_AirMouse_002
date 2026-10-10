#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : M10_MotionProc_0415.h
 * 모듈약어 : M10
 * 모듈명 : Motion Processor (v0415, Roll + Pitch)
 * ------------------------------------------------------
 * 기능 요약
 *  - 상보 필터: Roll + Pitch 2축 자세 추정
 *  - [Phase 2] Adaptive Variable EMA (Smoothstep + Reversal Reset)
 *  - Sigmoid 가속 + Zero Snap
 *  - 자세 기반 커서 보정 (기울기 보상)
 *
 * [v0415 주요 변경 — Click-Lock 전체 삭제]
 *  - Phase 11.5에서 Click-Freeze가 도입되며 Click-Lock이 실질 대체됨.
 *  - Phase 5 Q1-a 확정: 코드/API 전체 삭제.
 *    · notifyClick()              삭제
 *    · _isClickStabilizing        삭제
 *    · _lastClickTime             삭제
 *    · _hardClickLock             삭제
 *    · setHardClickLock()         삭제
 *    · G_CLICK_LOCK_MS            삭제
 *    · process() 내 Click-Lock 분기 삭제
 *  - E10 config의 hard_click_lock 필드는 잔존 (스키마 호환) 하나
 *    M10이 더 이상 사용하지 않음 → 향후 v0416에서 제거 예정.
 *
 * [매직 넘버 상수화]
 *  - G_M10_DPI_BASE = 15.0f
 *  - G_M10_DPI_STEP = 7.0f
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

    float _zeroSnapTh = 0.6f;

    // 상보 필터 계수
    static constexpr float G_COMP_ALPHA = 0.98f;

    // 시그모이드 파라미터
    static constexpr float G_SIG_SLOPE    = 0.8f;
    static constexpr float G_SIG_OFFSET   = 2.0f;
    static constexpr float G_SIG_DEADBAND = 0.4f;

    // [v0415] DPI gain 상수화
    static constexpr float G_M10_DPI_BASE = 15.0f;
    static constexpr float G_M10_DPI_STEP = 7.0f;

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

    void setDPI(int p_level) {
        _dpiGain = G_M10_DPI_BASE + ((float)p_level * G_M10_DPI_STEP);
    }
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

    void resetOrientation() {
        _roll  = 0.0f;
        _pitch = 0.0f;
    }

    float getRoll()  const { return _roll; }
    float getPitch() const { return _pitch; }

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
    //   [v0415] Click-Lock 분기 삭제
    // ================================================
    void process(float p_rawX, float p_rawY, int& p_outX, int& p_outY) {
        // Roll 보상
        float v_cosR = cosf(_roll);
        float v_sinR = sinf(_roll);

        float v_compX = p_rawX * v_cosR - p_rawY * v_sinR;
        float v_compY = p_rawX * v_sinR + p_rawY * v_cosR;

        // ====================================================
        // [Phase 2] Adaptive Variable EMA
        // ====================================================
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
