// =======================================================
// File: src/LD2420_Calib_001.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : LD2420_Calib_001.h
 * 모듈약어 : 
 * 모듈명 : 
 * ------------------------------------------------------
 * 기능 요약
 *  - 
 *  - 
 *  - 
 *
 * [설계]
 *  - 
 *  - .
 *  - 
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 *   - 
 * ------------------------------------------------------
 */
 

#include "LD2420_Types_001.h"

class LD2420Calibration {
public:
  LD2420Calibration();

  void reset();
  void start();
  void cancel();

  // 에너지 프레임 수신 시 호출 (5초 주기로 누적)
  void feed(const LD2420TargetData &data, uint32_t now_ms);

  // 수집 완료 여부
  bool isReady() const { return _state == LD2420CalState::READY; }
  bool isCollecting() const { return _state == LD2420CalState::COLLECTING; }
  LD2420CalState state() const { return _state; }
  uint8_t progress() const;

  // 결과 조회
  uint16_t noiseFloor(uint8_t gate) const { return _noiseFloor[gate]; }
  uint16_t peak(uint8_t gate) const { return _peak[gate]; }

  // 계산된 임계값 (노이즈 × 5 = 트리거, × 3 = 유지)
  LD2420GateConfig computeConfig() const;

private:
  LD2420CalState _state;
  uint16_t _noiseFloor[LD2420_MAX_GATES];
  uint16_t _peak[LD2420_MAX_GATES];
  uint32_t _sum[LD2420_MAX_GATES];
  uint16_t _sampleCount;
  uint32_t _lastSampleMs;

  static uint16_t clampThreshold(uint32_t v, uint16_t lo, uint16_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return (uint16_t)v;
  }
};

