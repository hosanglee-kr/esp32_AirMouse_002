#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : M20_BiasTracker_0415.h
 * 모듈약어 : M20
 * 모듈명 : IMU Zero-rate Bias Tracker
 * ------------------------------------------------------
 * 기능 요약
 *  - 자이로 정지 구간 감지 → 느린 드리프트 보정
 *  - 부팅 초기 캘리브레이션 결과를 seed로 받음
 *  - 상시 백그라운드 실행 (sensorTask)
 *
 * [알고리즘]
 *   1. raw gyro 합 벡터가 still_th 미만이면 "정지" 상태
 *   2. still_win_ms 연속 정지 유지 시 → bias 추종 시작
 *   3. bias ← bias + α × (raw - bias)
 *      - α 매우 작음 (0.001) → 드리프트만 서서히 흡수
 *      - 사용자 움직임은 반영 안 됨
 *
 * [튜닝 가이드]
 *   - still_th: 2.0 deg/s (사용자 손 안정도 감안)
 *   - still_win_ms: 250ms (순간 정지 오탐 방지)
 *   - α: 0.001 (매우 느림. 30초 정지 시 ~18% 수렴)
 * ------------------------------------------------------
 * [구현 규칙]
 *  - ArduinoJson v7.x.x 사용 (v6 이하 사용 금지)
 *  - memset + strlcpy 기반 안전 초기화
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 *   - 클래스명              : CL_모듈약어_ 접두사
 *   - 클래스 private 멤버   : _ 접두사
 *   - 함수 로컬 변수        : v_ 접두사
 *   - 함수 인자             : p_ 접두사
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <math.h>

class CL_M20_BiasTracker {
  private:
    // Bias 값 (deg/s)
    float _biasX = 0.0f;
    float _biasY = 0.0f;
    float _biasZ = 0.0f;

    // Config
    float    _stillTh    = 2.0f;      // deg/s (합 벡터)
    uint16_t _stillWinMs = 250;       // 연속 정지 시간
    float    _alpha      = 0.001f;    // 추종 계수

    // 런타임 상태
    uint32_t _stillStartMs = 0;
    bool     _stillActive  = false;
    bool     _seedSet      = false;

  public:
    CL_M20_BiasTracker() {}

    // ================================================
    // Config
    // ================================================
    void setConfig(float p_stillTh, uint16_t p_stillWinMs, float p_alpha) {
        _stillTh    = p_stillTh;
        _stillWinMs = p_stillWinMs;
        _alpha      = p_alpha;
    }

    float    getStillTh()    const { return _stillTh; }
    uint16_t getStillWinMs() const { return _stillWinMs; }
    float    getAlpha()      const { return _alpha; }

    // ================================================
    // Bias 시드 (부팅 초기 캘리브 결과)
    // ================================================
    void setBias(float p_x, float p_y, float p_z) {
        _biasX = p_x;
        _biasY = p_y;
        _biasZ = p_z;
        _seedSet = true;
    }

    bool hasSeed() const { return _seedSet; }

    // 재캘리브레이션 (사용자 요청)
    void reset() {
        _biasX = _biasY = _biasZ = 0.0f;
        _stillStartMs = 0;
        _stillActive  = false;
        _seedSet      = false;
    }

    // ================================================
    // Getter (bias-corrected 값 얻기)
    // ================================================
    float x() const { return _biasX; }
    float y() const { return _biasY; }
    float z() const { return _biasZ; }

    // 원시값 → 보정값
    float correctX(float p_raw) const { return p_raw - _biasX; }
    float correctY(float p_raw) const { return p_raw - _biasY; }
    float correctZ(float p_raw) const { return p_raw - _biasZ; }

    bool isStill() const { return _stillActive; }

    // ================================================
    // [Phase 11.6 / I-3] Sleep 후 Fast Recalibrate
    // ================================================
    void startFastRecalibrate(uint16_t p_durationMs) {
        _fastRecalibActive     = true;
        _fastRecalibStartMs    = (uint32_t)millis();
        _fastRecalibDurationMs = (p_durationMs < 50) ? 50 : p_durationMs;
        _fastRecalibSumX = _fastRecalibSumY = _fastRecalibSumZ = 0.0f;
        _fastRecalibCount      = 0;
    }

    bool isFastRecalibrating() const { return _fastRecalibActive; }

    // ================================================
    // 런타임 update (매 프레임)
    //   p_rawX/p_rawY/p_rawZ: raw gyro (deg/s)
    //   p_nowMs: millis()
    // ================================================
    void update(float p_rawX, float p_rawY, float p_rawZ, uint32_t p_nowMs) {
        // [I-3] Fast recalib 모드 (Sleep 복귀 후 단시간 집중 재수집)
        if (_fastRecalibActive) {
            const float v_mag = fabsf(p_rawX) + fabsf(p_rawY) + fabsf(p_rawZ);
            if (v_mag < _stillTh * 1.5f) {
                _fastRecalibSumX += p_rawX;
                _fastRecalibSumY += p_rawY;
                _fastRecalibSumZ += p_rawZ;
                _fastRecalibCount++;
            }

            if ((p_nowMs - _fastRecalibStartMs) >= _fastRecalibDurationMs) {
                if (_fastRecalibCount >= 15) {  // 최소 15 샘플 (약 120ms)
                    _biasX   = _fastRecalibSumX / _fastRecalibCount;
                    _biasY   = _fastRecalibSumY / _fastRecalibCount;
                    _biasZ   = _fastRecalibSumZ / _fastRecalibCount;
                    _seedSet = true;
                }
                _fastRecalibActive = false;
            }
            return;   // 정규 bias 추종 skip
        }

        // 1) 정지 판정 (합 벡터 vs still_th)
        const float v_mag = fabsf(p_rawX) + fabsf(p_rawY) + fabsf(p_rawZ);

        if (v_mag < _stillTh) {
            if (!_stillActive) {
                _stillActive  = true;
                _stillStartMs = p_nowMs;
                return;
            }
            if ((p_nowMs - _stillStartMs) < _stillWinMs) return;
            // 2) 추종 (정지 유지 중)
            _biasX += _alpha * (p_rawX - _biasX);
            _biasY += _alpha * (p_rawY - _biasY);
            _biasZ += _alpha * (p_rawZ - _biasZ);
            _seedSet = true;
        } else {
            _stillActive  = false;
            _stillStartMs = 0;
        }
    }

  private:
    bool     _fastRecalibActive     = false;
    uint32_t _fastRecalibStartMs    = 0;
    uint16_t _fastRecalibDurationMs = 300;
    float    _fastRecalibSumX       = 0.0f;
    float    _fastRecalibSumY       = 0.0f;
    float    _fastRecalibSumZ       = 0.0f;
    uint16_t _fastRecalibCount      = 0;
};
