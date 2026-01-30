

/**
 * @file M10_MotionProc_003.h
 * @brief 에어마우스 물리 연산 및 필터링 엔진
 * @details 
 * - 상보 필터(Complementary Filter)를 이용한 기울기 보정
 * - 시그모이드(Sigmoid) 함수 기반 가변 가속도 적용
 * - 클릭 시 흔들림 방지(Stabilization) 및 적응형 LPF 적용
 */

#pragma once

#include <Arduino.h>

class AdvancedMotionProcessor {
private:
    float _lpfX = 0, _lpfY = 0;   // 저대역 통과 필터 상태 변수
    float _roll = 0;              // 현재 기기의 기울기(라디안)
    float _dpiGain = 22.0f;       // 마우스 감도 가중치
    unsigned long _lastClickTime = 0;
    bool _isClickStabilizing = false;

public:
    AdvancedMotionProcessor() {}

    /** @brief DPI 레벨 설정 (1: 저속, 2: 중속, 3: 고속) */
    void setDPI(int level) { _dpiGain = 15.0f + (level * 7.0f); }

    /** @brief 클릭 발생 알림 (일시적으로 감도를 낮춰 흔들림 방지) */
    void notifyClick() { _lastClickTime = millis(); _isClickStabilizing = true; }

    /** * @brief 상보 필터를 이용한 기기 기울기 추정 
     * @param ay 가속도 Y, az 가속도 Z, gx 자이로 X(deg/s), dt 경과 시간(s)
     */
    void updateOrientation(float ay, float az, float gx, float dt) {
        float accelRoll = atan2(ay, az);
        // 자이로 적분값(98%)과 가속도계 보정값(2%)을 결합하여 드리프트 방지
        _roll = 0.98f * (_roll + (gx * DEG_TO_RAD) * dt) + 0.02f * accelRoll;
    }

    /**
     * @brief 센서 로우 데이터를 마우스 좌표로 변환
     * @param rawX 자이로 Z축 기반 Raw X, rawY 자이로 X축 기반 Raw Y
     * @param ay, az 기울기 보정용 가속도 데이터
     * @param outX, outY 최종 계산된 마우스 이동량(정수)
     */
    void process(float rawX, float rawY, int &outX, int &outY) {
        // 1. 기울기 보정 (회전 행렬 적용)
        float cosR = cos(_roll);
        float sinR = sin(_roll);
        float compX = rawX * cosR - rawY * sinR;
        float compY = rawX * sinR + rawY * cosR;

        // 2. 클릭 안정화 (클릭 후 150ms 동안 감도를 95% 차단)
        if (_isClickStabilizing) {
            if (millis() - _lastClickTime < 150) {
                compX *= 0.05f; compY *= 0.05f;
            } else { _isClickStabilizing = false; }
        }

        // 3. 적응형 LPF (움직임 크기에 따라 반응성 조절)
        float delta = sqrt(compX * compX + compY * compY);
        float alpha = (delta > 3.0f) ? 0.5f : 0.12f; // 빠른 이동 시 필터 약화
        _lpfX = (compX * alpha) + (_lpfX * (1.0f - alpha));
        _lpfY = (compY * alpha) + (_lpfY * (1.0f - alpha));

        // 4. 시그모이드 가속 곡선 적용 (미세 이동은 정밀하게, 빠른 이동은 가속)
        auto applySigmoid = [&](float input) {
            float av = abs(input);
            if (av < 0.4f) return 0.0f; // 데드존
            return (_dpiGain / (1.0f + exp(-0.8f * (av - 2.0f)))) * (input > 0 ? 1 : -1);
        };

        outX = (int)applySigmoid(_lpfX);
        outY = (int)applySigmoid(_lpfY);
    }
};

