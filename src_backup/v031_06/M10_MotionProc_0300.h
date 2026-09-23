#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : M10_MotionProc_0300.h
 * 모듈약어 : M10
 * 모듈명 : 에어마우스 물리 연산 및 필터링 엔진 (Idle/ZeroSnap/HardLock)
 * ------------------------------------------------------
 * 기능 요약
 *  - 상보 필터(Complementary Filter) 기반 기울기(roll) 보정
 *  - Sigmoid 기반 비선형 가속(픽셀 정밀 + 빠른 스윕)
 *  - 적응형 LPF(정지 시 강, 이동 시 약) 형태의 EMA
 *  - Click Stabilization:
 *     - Soft: 150ms 동안 강 감쇠
 *     - Hard: 150ms 동안 outX/outY=0 (완전 고정)
 *  - Zero Snap: 극저속 영역에서 커서 0으로 붙이는 스냅
 * ------------------------------------------------------
 * [구현 규칙]
 *  - 항상 소스 시작 주석 부분 체계 유지 및 내용 업데이트
 *  - 소스 시작 주석 부분 구현규칙, 코드네이밍규칙 내용 그대로 유지, 수정금지
 *  - ArduinoJson v7.x.x 사용 (v6 이하 사용 금지)
 *  - JsonDocument 단일 타입만 사용
 *  - createNestedArray/Object/containsKey 사용 금지
 *  - memset + strlcpy 기반 안전 초기화
 *  - 주석/필드명은 JSON 구조와 동일하게 유지
 *  - 변수명은 가능한 해석 가능하게
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 *   - namespace 명        : 모듈약어_ 접두사
 *   - namespace 내 상수    : 모둘약어 접두시 미사용
 *   - 전역 상수,매크로      : G_모듈약어_ 접두사
 *   - 전역 변수             : g_모듈약어_ 접두사
 *   - 전역 함수             : 모듈약어_ 접두사
 *   - type                  : T_모듈약어_ 접두사
 *   - typedef               : _t  접미사
 *   - enum 상수             : EN_모듈약어_ 접두사
 *   - 구조체                : ST_모듈약어_ 접두사
 *   - 클래스명              : CL_모듈약어_ 접두사 , 버전 제거
 *   - 클래스 private 멤버   : _ 접두사
 *   - 클래스 멤버(함수/변수) : 모듈약어 접두사 미사용
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

    float _roll    = 0.0f;
    float _dpiGain = 22.0f;

    unsigned long _lastClickTime      = 0;
    bool          _isClickStabilizing = false;

    bool  _hardClickLock = false;
    float _zeroSnapTh    = 0.6f;

  public:
    CL_M10_AdvancedMotionProcessor() {}

    void setDPI(int p_level) { _dpiGain = 15.0f + (p_level * 7.0f); }
    void setHardClickLock(bool p_enable) { _hardClickLock = p_enable; }
    void setZeroSnapTh(float p_th) { _zeroSnapTh = p_th; }

    void notifyClick() {
        _lastClickTime      = millis();
        _isClickStabilizing = true;
    }

    void updateOrientation(float p_ay, float p_az, float p_gx_deg_s, float p_dt_s) {
        float v_accelRoll = atan2f(p_ay, p_az);
        _roll             = 0.98f * (_roll + (p_gx_deg_s * DEG_TO_RAD) * p_dt_s) + 0.02f * v_accelRoll;
    }

    void process(float p_rawX, float p_rawY, int& p_outX, int& p_outY) {
        // roll compensation
        float v_cosR = cosf(_roll);
        float v_sinR = sinf(_roll);

        float v_compX = p_rawX * v_cosR - p_rawY * v_sinR;
        float v_compY = p_rawX * v_sinR + p_rawY * v_cosR;

        // click stabilization
        if (_isClickStabilizing) {
            if (millis() - _lastClickTime < 150) {
                if (_hardClickLock) {
                    p_outX = 0;
                    p_outY = 0;
                    return;
                } else {
                    v_compX *= 0.05f;
                    v_compY *= 0.05f;
                }
            } else {
                _isClickStabilizing = false;
            }
        }

        // adaptive EMA
        float v_delta = sqrtf(v_compX * v_compX + v_compY * v_compY);
        float v_alpha = (v_delta > 3.0f) ? 0.50f : 0.12f;

        _lpfX = (v_compX * v_alpha) + (_lpfX * (1.0f - v_alpha));
        _lpfY = (v_compY * v_alpha) + (_lpfY * (1.0f - v_alpha));

        auto v_applySigmoid = [&](float p_input) -> float {
            float v_av = fabsf(p_input);
            if (v_av < 0.4f) return 0.0f;
            float v_out = (_dpiGain / (1.0f + expf(-0.8f * (v_av - 2.0f))));
            return v_out * (p_input >= 0 ? 1.0f : -1.0f);
        };

        float v_x = v_applySigmoid(_lpfX);
        float v_y = v_applySigmoid(_lpfY);

        // zero snap (극저속 영역을 0으로 붙여 떨림 제거)
        if (fabsf(v_x) < _zeroSnapTh) v_x = 0.0f;
        if (fabsf(v_y) < _zeroSnapTh) v_y = 0.0f;

        p_outX = (int)v_x;
        p_outY = (int)v_y;
    }
};
