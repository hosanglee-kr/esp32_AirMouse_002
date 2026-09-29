#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : M10_MotionProc_0410.h
 * 모듈약어 : M10
 * 모듈명 : Motion Processor (v0410, Roll + Pitch)
 * ------------------------------------------------------
 * 기능 요약
 *  - 상보 필터: Roll + Pitch 2축 자세 추정
 *  - Sigmoid 가속 + 적응형 LPF + Zero Snap
 *  - Click Lock (Hard / Soft)
 *  - 자세 기반 커서 보정 (기울기 보상)
 *
 * [v0410 변경]
 *  - Pitch 추적 추가 (기존 Roll만)
 *  - 좌표계 용어 통일 (Roll/Pitch/Yaw)
 *  - updateOrientation2() 신규 (6축 입력)
 *  - 기존 updateOrientation()은 하위 호환 유지
 *
 * [좌표계 규약]  (반드시 준수)
 *   물리 축 (MPU6050 raw)      사용자 용어       용도
 *   gx (긴 축 회전)        →   Roll              커서 Y, 자세 보정
 *   gy (좌우 축 회전)      →   Pitch             휠, 자세 보정
 *   gz (수직 축 회전)      →   Yaw               커서 X, Flick L/R
 *
 *   Accel 기준:
 *   - Roll  ≈ atan2(ay, az)   (X축 주위 회전)
 *   - Pitch ≈ atan2(-ax, az)  (Y축 주위 회전)
 * ------------------------------------------------------
 * [구현 규칙]
 *  - 항상 소스 시작 주석 부분 체계 유지 및 내용 업데이트
 *  - 소스 시작 주석 부분 구현규칙, 코드네이밍규칙 내용 그대로 유지, 수정금지
 *  - ArduinoJson v7.x.x 사용 (v6 이하 사용 금지)
 *  - JsonDocument 단일 타입만 사용
 *  - createNestedArray/Object/containsKey 사용 금지
 *  - memset + strlcpy 기반 안전 초기화
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 *   - 클래스명              : CL_모듈약어_ 접두사 , 버전 제거
 *   - 클래스 private 멤버   : _ 접두사
 *   - 클래스 정적 멤버      : s_ 접두사
 *   - 함수 로컬 변수        : v_ 접두사
 *   - 함수 인자             : p_ 접두사
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <math.h>

class CL_M10_AdvancedMotionProcessor {
  private:
    float _lpfX = 0.0f;
    float _lpfY = 0.0f;

    // 자세 상태 (rad)
    float _roll  = 0.0f;   // X축 주위 (긴 축)
    float _pitch = 0.0f;   // Y축 주위 (좌우 축)

    float _dpiGain = 22.0f;

    unsigned long _lastClickTime      = 0;
    bool          _isClickStabilizing = false;

    bool  _hardClickLock = false;
    float _zeroSnapTh    = 0.6f;

    // 상보 필터 계수 (자이로 적분 가중치)
    static constexpr float G_COMP_ALPHA = 0.98f;

    // 클릭 안정화 시간 (ms)
    static constexpr uint32_t G_CLICK_LOCK_MS = 150;

    // 적응형 LPF 임계 (rad/s)
    static constexpr float G_LPF_FAST_TH  = 3.0f;
    static constexpr float G_LPF_FAST_A   = 0.50f;
    static constexpr float G_LPF_SLOW_A   = 0.12f;

    // 시그모이드 파라미터
    static constexpr float G_SIG_SLOPE = 0.8f;
    static constexpr float G_SIG_OFFSET = 2.0f;
    static constexpr float G_SIG_DEADBAND = 0.4f;

  public:
    CL_M10_AdvancedMotionProcessor() {}

    void setDPI(int p_level) { _dpiGain = 15.0f + (p_level * 7.0f); }
    void setHardClickLock(bool p_enable) { _hardClickLock = p_enable; }
    void setZeroSnapTh(float p_th) { _zeroSnapTh = p_th; }

    // 자세 초기화 (재캘리브레이션 시)
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
    // 상보 필터 (Roll + Pitch 2축)
    //   p_ax/p_ay/p_az : accel (g)
    //   p_gx/p_gy      : gyro (deg/s, bias-corrected)
    //   p_dt_s         : dt (sec)
    // ================================================
    void updateOrientation2(float p_ax, float p_ay, float p_az,
                            float p_gx, float p_gy,
                            float p_dt_s) {
        // Roll: X축 주위 (긴 축)
        float v_accelRoll = atan2f(p_ay, p_az);
        _roll  = G_COMP_ALPHA * (_roll  + (p_gx * DEG_TO_RAD) * p_dt_s)
               + (1.0f - G_COMP_ALPHA) * v_accelRoll;

        // Pitch: Y축 주위 (좌우 축)
        float v_accelPitch = atan2f(-p_ax, p_az);
        _pitch = G_COMP_ALPHA * (_pitch + (p_gy * DEG_TO_RAD) * p_dt_s)
               + (1.0f - G_COMP_ALPHA) * v_accelPitch;
    }

    // 기존 시그니처 (Roll만, 하위 호환)
    void updateOrientation(float p_ay, float p_az, float p_gx_deg_s, float p_dt_s) {
        float v_accelRoll = atan2f(p_ay, p_az);
        _roll  = G_COMP_ALPHA * (_roll + (p_gx_deg_s * DEG_TO_RAD) * p_dt_s)
               + (1.0f - G_COMP_ALPHA) * v_accelRoll;
    }

    // ================================================
    // 커서 좌표 계산 (Roll 보상 + LPF + Sigmoid + Zero Snap)
    // ================================================
    void process(float p_rawX, float p_rawY, int& p_outX, int& p_outY) {
        // Roll 보상 (기울기 정렬)
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

        // 적응형 LPF
        float v_delta = sqrtf(v_compX * v_compX + v_compY * v_compY);
        float v_alpha = (v_delta > G_LPF_FAST_TH) ? G_LPF_FAST_A : G_LPF_SLOW_A;

        _lpfX = (v_compX * v_alpha) + (_lpfX * (1.0f - v_alpha));
        _lpfY = (v_compY * v_alpha) + (_lpfY * (1.0f - v_alpha));

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
