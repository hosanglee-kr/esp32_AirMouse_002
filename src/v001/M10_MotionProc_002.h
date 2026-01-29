#pragma once

#include <Arduino.h>

class AdvancedMotionProcessor {
private:
    float _lpfX, _lpfY;
    float _roll = 0;
    float _dpiGain = 22.0f;
    unsigned long _lastClickTime = 0;
    bool _isClickStabilizing = false;

public:
    AdvancedMotionProcessor() : _lpfX(0), _lpfY(0) {}

    void setDPI(int level) { _dpiGain = 15.0f + (level * 7.0f); }
    void notifyClick() { _lastClickTime = millis(); _isClickStabilizing = true; }

    // 상보 필터 기반 기울기 업데이트
    void updateOrientation(float ay, float az, float gx, float dt) {
        float accelRoll = atan2(ay, az);
        _roll = 0.98f * (_roll + (gx / 131.0f) * dt) + 0.02f * accelRoll;
    }

    void process(float rawX, float rawY, int &outX, int &outY) {
        float cosR = cos(_roll);
        float sinR = sin(_roll);
        float compX = rawX * cosR - rawY * sinR;
        float compY = rawX * sinR + rawY * cosR;

        if (_isClickStabilizing && (millis() - _lastClickTime < 150)) {
            compX *= 0.05f; compY *= 0.05f;
        } else { _isClickStabilizing = false; }

        float delta = sqrt(compX*compX + compY*compY);
        float alpha = (delta > 3.0f) ? 0.5f : 0.12f;
        _lpfX = (compX * alpha) + (_lpfX * (1.0f - alpha));
        _lpfY = (compY * alpha) + (_lpfY * (1.0f - alpha));

        // Sigmoid 가속 적용
        auto applySigmoid = [&](float input) {
            float av = abs(input);
            if (av < 0.4f) return 0.0f;
            return (_dpiGain / (1.0f + exp(-0.8f * (av - 2.0f)))) * (input > 0 ? 1 : -1);
        };

        outX = (int)applySigmoid(_lpfX);
        outY = (int)applySigmoid(_lpfY);
    }
};
