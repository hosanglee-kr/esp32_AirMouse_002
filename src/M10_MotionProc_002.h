#ifndef ADVANCED_MOTION_PROCESSOR_H
#define ADVANCED_MOTION_PROCESSOR_H

#include <Arduino.h>

class AdvancedMotionProcessor {
private:
    float _lpfX, _lpfY;
    float _roll = 0;           // 상보 필터로 계산된 기울기
    float _dpiGain = 22.0f;    // 현재 DPI (감도)
    unsigned long _lastClickTime = 0;
    bool _isClickStabilizing = false;

public:
    AdvancedMotionProcessor() : _lpfX(0), _lpfY(0) {}

    // DPI 변경 (1.0 ~ 3.0 단계별 조절 가능)
    void setDPI(int level) {
        _dpiGain = 15.0f + (level * 7.0f); 
    }

    void notifyClick() {
        _lastClickTime = millis();
        _isClickStabilizing = true;
    }

    // 상보 필터: 가속도(정확하지만 노이즈) + 자이로(빠르지만 드리프트) 결합
    void updateOrientation(float ay, float az, float gx, float dt) {
        float accelRoll = atan2(ay, az);
        // 0.98, 0.02 비중으로 자이로의 빠른 반응과 가속도의 절대각 결합
        _roll = 0.98f * (_roll + (gx / 131.0f) * dt) + 0.02f * accelRoll;
    }

    float applySigmoidAccel(float input) {
        float absVal = abs(input);
        if (absVal < 0.4f) return 0;
        // 시그모이드 곡선: 중앙 부근에서 정밀하고 끝에서 가속
        float accel = _dpiGain / (1.0f + exp(-0.8f * (absVal - 2.0f)));
        return accel * (input > 0 ? 1 : -1);
    }

    void process(float rawX, float rawY, int &outX, int &outY) {
        // 기울기 보정 (Rotation Matrix)
        float cosR = cos(_roll);
        float sinR = sin(_roll);
        float compX = rawX * cosR - rawY * sinR;
        float compY = rawX * sinR + rawY * cosR;

        // 클릭 보정
        if (_isClickStabilizing && (millis() - _lastClickTime < 150)) {
            compX *= 0.05f; compY *= 0.05f;
        } else { _isClickStabilizing = false; }

        // 적응형 LPF
        float delta = sqrt(compX*compX + compY*compY);
        float alpha = (delta > 3.0f) ? 0.5f : 0.12f;
        _lpfX = (compX * alpha) + (_lpfX * (1.0f - alpha));
        _lpfY = (compY * alpha) + (_lpfY * (1.0f - alpha));

        outX = (int)applySigmoidAccel(_lpfX);
        outY = (int)applySigmoidAccel(_lpfY);
    }
};

#endif
