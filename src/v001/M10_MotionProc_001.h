#ifndef ADVANCED_MOTION_PROCESSOR_H
#define ADVANCED_MOTION_PROCESSOR_H

#include <Arduino.h>

/**
 * @class AdvancedMotionProcessor
 * @brief 마우스의 움직임을 정밀하게 계산하는 물리 엔진 클래스
 */
class AdvancedMotionProcessor {
private:
    float _lpfX, _lpfY;        // 저대역 통과 필터(LPF) 상태 저장
    float _baseAlpha = 0.12f;  // 필터 강도 (낮을수록 부드러움)
    unsigned long _lastClickTime = 0;
    bool _isClickStabilizing = false;

public:
    AdvancedMotionProcessor() : _lpfX(0), _lpfY(0) {}

    // [기능 1] 클릭 보정: 클릭 시 발생하는 손떨림 억제 활성화
    void notifyClick() {
        _lastClickTime = millis();
        _isClickStabilizing = true;
    }

    // [기능 2] 기울기 보정: 마우스가 기울어진 상태에서도 수평/수직 이동 유지
    void compensateRoll(float &x, float &y, float ay, float az) {
        float roll = atan2(ay, az); // 중력 가속도를 이용한 Roll 각도 계산
        float cosR = cos(roll);
        float sinR = sin(roll);
        
        // 회전 행렬을 적용하여 좌표축 보정
        float newX = x * cosR - y * sinR;
        float newY = x * sinR + y * cosR;
        x = newX;
        y = newY;
    }

    // [기능 3] 시그모이드 가속도: 인간의 신경계와 유사한 비선형 속도 곡선
    float applySigmoidAccel(float input) {
        float absVal = abs(input);
        if (absVal < 0.4f) return 0; // 데드존: 미세한 노이즈 무시

        float gain = 25.0f; // 최대 속도 가중치
        float k = 0.8f;     // 곡선의 가파른 정도
        float x0 = 2.0f;    // 변곡점
        
        // Sigmoid 함수 적용
        float accel = gain / (1.0f + exp(-k * (absVal - x0)));
        return accel * (input > 0 ? 1 : -1);
    }

    /**
     * @brief 센서 데이터를 입력받아 최종 마우스 이동량을 산출
     */
    void process(float rawX, float rawY, float ay, float az, int &outX, int &outY) {
        // 1. 쥐고 있는 각도에 따른 좌표 보정
        compensateRoll(rawX, rawY, ay, az);

        // 2. 클릭 안정화 로직 (클릭 직후 150ms간 감도 95% 감소)
        if (_isClickStabilizing) {
            if (millis() - _lastClickTime < 150) {
                rawX *= 0.05f; 
                rawY *= 0.05f;
            } else {
                _isClickStabilizing = false;
            }
        }

        // 3. 적응형 EMA 필터링 (움직임이 크면 반응속도 우선, 작으면 부드러움 우선)
        float delta = sqrt(rawX * rawX + rawY * rawY);
        float alpha = (delta > 3.0f) ? 0.5f : _baseAlpha;
        _lpfX = (rawX * alpha) + (_lpfX * (1.0f - alpha));
        _lpfY = (rawY * alpha) + (_lpfY * (1.0f - alpha));

        // 4. 최종 가속도 곡선 적용
        outX = (int)applySigmoidAccel(_lpfX);
        outY = (int)applySigmoidAccel(_lpfY);
    }
};

#endif

