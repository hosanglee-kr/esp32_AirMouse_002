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
 * [v0415 주요 변경]
 *  - reset()이 fast recalib 상태까지 초기화 (Phase 4 L4-A3-08)
 *    · wake 후 fast recalib 진행 중 사용자가 캘리브 요청 시
 *      300ms 뒤 평균값이 bias를 덮어쓰는 문제 방지
 *  - 매직 넘버 상수화 (15 샘플, 1.5 배수)
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <math.h>

class CL_M20_BiasTracker {
  private:
    float _biasX = 0.0f;
    float _biasY = 0.0f;
    float _biasZ = 0.0f;

    float    _stillTh    = 2.0f;
    uint16_t _stillWinMs = 250;
    float    _alpha      = 0.001f;

    uint32_t _stillStartMs = 0;
    bool     _stillActive  = false;
    bool     _seedSet      = false;

    // [v0415] Fast recalib 상수
    static constexpr uint16_t G_M20_FAST_RECALIB_MIN_SAMPLES = 15;   // ~120ms @8ms
    static constexpr float    G_M20_FAST_RECALIB_STILL_MULT  = 1.5f; // stillTh × 1.5

  public:
    CL_M20_BiasTracker() {}

    void setConfig(float p_stillTh, uint16_t p_stillWinMs, float p_alpha) {
        _stillTh    = p_stillTh;
        _stillWinMs = p_stillWinMs;
        _alpha      = p_alpha;
    }

    float    getStillTh()    const { return _stillTh; }
    uint16_t getStillWinMs() const { return _stillWinMs; }
    float    getAlpha()      const { return _alpha; }

    void setBias(float p_x, float p_y, float p_z) {
        _biasX = p_x;
        _biasY = p_y;
        _biasZ = p_z;
        _seedSet = true;
    }

    bool hasSeed() const { return _seedSet; }

    // --------------------------------------------------
    // [v0415 L4-A3-08] reset — bias + still + fast recalib 전부 초기화
    // --------------------------------------------------
    void reset() {
        _biasX = _biasY = _biasZ = 0.0f;
        _stillStartMs = 0;
        _stillActive  = false;
        _seedSet      = false;

        // fast recalib 상태 초기화
        _fastRecalibActive     = false;
        _fastRecalibStartMs    = 0;
        _fastRecalibDurationMs = 300;
        _fastRecalibSumX = _fastRecalibSumY = _fastRecalibSumZ = 0.0f;
        _fastRecalibCount      = 0;
    }

    float x() const { return _biasX; }
    float y() const { return _biasY; }
    float z() const { return _biasZ; }

    float correctX(float p_raw) const { return p_raw - _biasX; }
    float correctY(float p_raw) const { return p_raw - _biasY; }
    float correctZ(float p_raw) const { return p_raw - _biasZ; }

    bool isStill() const { return _stillActive; }

    void startFastRecalibrate(uint16_t p_durationMs) {
        _fastRecalibActive     = true;
        _fastRecalibStartMs    = (uint32_t)millis();
        _fastRecalibDurationMs = (p_durationMs < 50) ? 50 : p_durationMs;
        _fastRecalibSumX = _fastRecalibSumY = _fastRecalibSumZ = 0.0f;
        _fastRecalibCount      = 0;
    }

    bool isFastRecalibrating() const { return _fastRecalibActive; }

    void update(float p_rawX, float p_rawY, float p_rawZ, uint32_t p_nowMs) {
        if (_fastRecalibActive) {
            const float v_mag = fabsf(p_rawX) + fabsf(p_rawY) + fabsf(p_rawZ);
            if (v_mag < _stillTh * G_M20_FAST_RECALIB_STILL_MULT) {
                _fastRecalibSumX += p_rawX;
                _fastRecalibSumY += p_rawY;
                _fastRecalibSumZ += p_rawZ;
                _fastRecalibCount++;
            }

            if ((p_nowMs - _fastRecalibStartMs) >= _fastRecalibDurationMs) {
                if (_fastRecalibCount >= G_M20_FAST_RECALIB_MIN_SAMPLES) {
                    _biasX   = _fastRecalibSumX / _fastRecalibCount;
                    _biasY   = _fastRecalibSumY / _fastRecalibCount;
                    _biasZ   = _fastRecalibSumZ / _fastRecalibCount;
                    _seedSet = true;
                }
                _fastRecalibActive = false;
            }
            return;
        }

        const float v_mag = fabsf(p_rawX) + fabsf(p_rawY) + fabsf(p_rawZ);

        if (v_mag < _stillTh) {
            if (!_stillActive) {
                _stillActive  = true;
                _stillStartMs = p_nowMs;
                return;
            }
            if ((p_nowMs - _stillStartMs) < _stillWinMs) return;

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
