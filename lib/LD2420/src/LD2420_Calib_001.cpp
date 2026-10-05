// =======================================================
// File: src/LD2420_Calib_001.cpp
// =======================================================

/*
 * ------------------------------------------------------
 * 소스명 : LD2420_Calib_001.cpp
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
 
 
#include "LD2420_Calib_001.h"

LD2420Calibration::LD2420Calibration() { reset(); }

void LD2420Calibration::reset() {
  _state = LD2420CalState::IDLE;
  memset(_noiseFloor, 0, sizeof(_noiseFloor));
  memset(_peak, 0, sizeof(_peak));
  memset(_sum, 0, sizeof(_sum));
  _sampleCount = 0;
  _lastSampleMs = 0;
}

void LD2420Calibration::start() {
  reset();
  _state = LD2420CalState::COLLECTING;
  _lastSampleMs = millis();
}

void LD2420Calibration::cancel() {
  reset();
}

void LD2420Calibration::feed(const LD2420TargetData &data, uint32_t now_ms) {
  if (_state != LD2420CalState::COLLECTING) return;
  if (now_ms - _lastSampleMs < LD2420_CAL_INTERVAL_MS) return;
  _lastSampleMs = now_ms;

  for (int g = 0; g < LD2420_MAX_GATES; g++) {
    uint16_t e = data.gate_energy[g];
    _sum[g] += e;
    if (e > _peak[g]) _peak[g] = e;
  }
  _sampleCount++;

  if (_sampleCount >= LD2420_CAL_SAMPLES) {
    // 평균 = 노이즈 플로어
    for (int g = 0; g < LD2420_MAX_GATES; g++) {
      _noiseFloor[g] = _sum[g] / _sampleCount;
    }
    _state = LD2420CalState::READY;
  }
}

uint8_t LD2420Calibration::progress() const {
  if (_state == LD2420CalState::READY ||
      _state == LD2420CalState::APPLIED) return 100;
  if (_sampleCount >= LD2420_CAL_SAMPLES) return 100;
  return (_sampleCount * 100) / LD2420_CAL_SAMPLES;
}

LD2420GateConfig LD2420Calibration::computeConfig() const {
  LD2420GateConfig cfg;
  for (int g = 0; g < LD2420_MAX_GATES; g++) {
    cfg.move_threshold[g]  = clampThreshold(_noiseFloor[g] * 5, 10, 100);
    cfg.still_threshold[g] = clampThreshold(_noiseFloor[g] * 3, 5,  80);
  }
  return cfg;
}
