// =======================================================
// File: src/LD2420_Calib_001.cpp
// =======================================================

/*
 * ------------------------------------------------------
 * 소스명 : LD2420_Calib_001.cpp
 * 모듈약어 : LD2420 (L20)
 * 모듈명 : HLK-LD2420 자동 캘리브레이션 구현부
 * ------------------------------------------------------
 * 기능 요약
 *  - 무인 환경에서 16개 거리 게이트의 배경 노이즈 수집
 *  - 5초 주기 샘플링 필터링, 총 64회 샘플 평균치(Noise Floor) 산출
 *  - 산출된 노이즈를 바탕으로 이동/정지 임계값 자동 연산 및 클램핑
 *
 * [설계]
 *  - 센서 주변의 환경적 반사파(가구, 벽면, 전자파 등)의 기저 에너지를 측정
 *  - 오탐 방지를 위해 이동 감도는 노이즈 대비 5배(10~100 범위),
 *    재실 유지를 위한 미세 정지 감도는 노이즈 대비 3배(5~80 범위)로 보정
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 *  - 클래스 메서드       : LD2420Calibration:: 접두사 + camelCase
 *  - 로컬 변수           : camelCase
 * ------------------------------------------------------
 */

#include "LD2420_Calib_001.h"

/**
 * @brief 캘리브레이션 객체 생성자
 * 내부 상태 및 누적 버퍼를 IDLE 상태로 초기화합니다.
 */
LD2420Calibration::LD2420Calibration() {
    reset();
}

/**
 * @brief 누적 통계치와 상태를 초기 상태(IDLE)로 완전 리셋
 * 모든 합산 버퍼, 피크값, 노이즈 플로어, 샘플 카운터를 0으로 초기화합니다.
 */
void LD2420Calibration::reset() {
    _state = LD2420CalState::IDLE;
    memset(_noiseFloor, 0, sizeof(_noiseFloor));
    memset(_peak, 0, sizeof(_peak));
    memset(_sum, 0, sizeof(_sum));
    _sampleCount  = 0;
    _lastSampleMs = 0;
}

/**
 * @brief 캘리브레이션 수집 시작
 * 기존 데이터를 리셋하고 상태를 COLLECTING으로 변경하며 타이머를 시작합니다.
 */
void LD2420Calibration::start() {
    reset();
    _state        = LD2420CalState::COLLECTING;
    _lastSampleMs = millis();
}

/**
 * @brief 진행 중인 캘리브레이션 취소
 */
void LD2420Calibration::cancel() {
    reset();
}

/**
 * @brief 실시간 에너지 데이터 유입 시 호출되는 샘플링 함수
 * @param data 센서로부터 수신된 16개 게이트의 실시간 에너지 수치
 * @param now_ms 현재 시간 (millis())
 *
 * [동작 흐름]
 * 1. 수집 중(COLLECTING) 상태가 아니면 즉시 무시
 * 2. 마지막 샘플링 시점으로부터 5초(LD2420_CAL_INTERVAL_MS)가 지나지 않았으면 스킵 (레이더의 순간 변동 평활화)
 * 3. 16개 게이트별로 에너지를 누적 합산(_sum)하고 피크값(_peak) 갱신
 * 4. 누적 샘플 수가 64개(LD2420_CAL_SAMPLES)에 도달하면 각 게이트의 평균값(_sum / 64)을 구해
 *    _noiseFloor에 저장하고 FSM 상태를 READY로 전이
 */
void LD2420Calibration::feed(const LD2420TargetData& data, uint32_t now_ms) {
    if (_state != LD2420CalState::COLLECTING) return;
    if (now_ms - _lastSampleMs < LD2420_CAL_INTERVAL_MS) return;
    _lastSampleMs = now_ms;

    for (int g = 0; g < LD2420_MAX_GATES; g++) {
        uint16_t e  = data.gate_energy[g];
        _sum[g]    += e;
        if (e > _peak[g]) _peak[g] = e;
    }
    _sampleCount++;

    // 총 64회 샘플 수집 완료 시 평균값 산출
    if (_sampleCount >= LD2420_CAL_SAMPLES) {
        for (int g = 0; g < LD2420_MAX_GATES; g++) {
            _noiseFloor[g] = _sum[g] / _sampleCount;
        }
        _state = LD2420CalState::READY;
    }
}

/**
 * @brief 수집 진행률 반환 (0 ~ 100%)
 * @return 0부터 100 사이의 퍼센트 값
 */
uint8_t LD2420Calibration::progress() const {
    if (_state == LD2420CalState::READY || _state == LD2420CalState::APPLIED) return 100;
    if (_sampleCount >= LD2420_CAL_SAMPLES) return 100;
    return (_sampleCount * 100) / LD2420_CAL_SAMPLES;
}

/**
 * @brief 측정된 노이즈 플로어를 기반으로 센서에 주입할 게이트별 임계값(LD2420GateConfig) 생성
 *
 * [알고리즘 공식 (ESPHome 호환)]
 * - 움직임(이동) 감지 임계값 (move_threshold) = 노이즈 × 5배
 *   - 최소 10, 최대 100으로 클램핑 (10 미만이면 너무 민감하여 허위 감지 발생, 100 초과 방지)
 * - 정지(재실) 감지 임계값 (still_threshold) = 노이즈 × 3배
 *   - 최소 5, 최대 80으로 클램핑 (호흡 등의 미세 신호를 감지하되 80 이하로 유지)
 *
 * @return 완성된 16개 게이트 설정 구조체
 */
LD2420GateConfig LD2420Calibration::computeConfig() const {
    LD2420GateConfig cfg;
    for (int g = 0; g < LD2420_MAX_GATES; g++) {
        cfg.move_threshold[g]  = clampThreshold(_noiseFloor[g] * 5, 10, 100);
        cfg.still_threshold[g] = clampThreshold(_noiseFloor[g] * 3, 5, 80);
    }
    return cfg;
}

