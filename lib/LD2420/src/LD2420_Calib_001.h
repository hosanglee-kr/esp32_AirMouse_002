// =======================================================
// File: src/LD2420_Calib_001.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : LD2420_Calib_001.h
 * 모듈약어 : LD2420 (L20)
 * 모듈명 : HLK-LD2420 자동 캘리브레이션 클래스 선언부
 * ------------------------------------------------------
 * 기능 요약
 *  - 무인 환경에서 16개 거리 게이트의 배경 노이즈 플로어(Noise Floor) 측정 및 수집
 *  - 5초 주기 샘플링, 총 64회(약 5.3분) 누적 평균 산출
 *  - 노이즈 플로어 기반 최적 움직임(move) 및 정지(still) 감도 임계값 자동 연산
 *  - 수집 진행률(0~100%) 및 상태 머신(IDLE, COLLECTING, READY 등) 제공
 *
 * [설계]
 *  - ESPHome LD2420 컴포넌트의 캘리브레이션 알고리즘과 동일하게 동작:
 *    - 움직임 감도 임계값 = clamp(노이즈 × 5, 10, 100)
 *    - 정지 감도 임계값   = clamp(노이즈 × 3, 5, 80)
 *  - 호출자가 에너지 프레임마다 feed()를 호출하면 내부 타이머로 5초 주기 필터링 수행
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 *  - 클래스명            : LD2420 접두사 + PascalCase (LD2420Calibration)
 *  - 멤버 변수           : _ 접두사 + camelCase
 *  - 메서드명            : camelCase
 * ------------------------------------------------------
 */

#include "LD2420_Types_001.h"

/**
 * @brief LD2420 배경 노이즈 수집 및 자동 임계값 연산 클래스
 */
class LD2420Calibration {
  public:
    /**
     * @brief 생성자 (내부 버퍼 및 상태 초기화)
     */
    LD2420Calibration();

    /**
     * @brief 캘리브레이션 내부 누적치 및 상태 완전 리셋
     */
    void reset();

    /**
     * @brief 캘리브레이션 수집 시작 (COLLECTING 상태 진입)
     */
    void start();

    /**
     * @brief 진행 중인 캘리브레이션 중단 및 리셋
     */
    void cancel();

    /**
     * @brief 실시간 에너지 프레임 수신 시 호출되는 데이터 주입 함수
     * @param data 수신된 타겟 데이터 (16개 게이트 에너지 수치 포함)
     * @param now_ms 현재 시스템 시간 (millis())
     * @note 5초(LD2420_CAL_INTERVAL_MS) 주기로 유효 샘플을 취득하며 64개 도달 시 READY로 전이
     */
    void feed(const LD2420TargetData& data, uint32_t now_ms);

    /**
     * @brief 샘플 수집 완료 및 최적 설정값 연산 완료 여부 확인
     * @return true: READY 상태 (임계값 적용 가능)
     */
    bool isReady() const { return _state == LD2420CalState::READY; }

    /**
     * @brief 현재 캘리브레이션 데이터 수집 중인지 여부 확인
     * @return true: COLLECTING 상태
     */
    bool isCollecting() const { return _state == LD2420CalState::COLLECTING; }

    /**
     * @brief 현재 캘리브레이션 FSM 상태 반환
     */
    LD2420CalState state() const { return _state; }

    /**
     * @brief 수집 진행률 계산 (0 ~ 100%)
     * @return 백분율 진행도 (0 ~ 100)
     */
    uint8_t progress() const;

    /**
     * @brief 특정 게이트의 측정된 평균 배경 노이즈 수치 반환
     * @param gate 게이트 번호 (0 ~ 15)
     * @return 평균 노이즈 플로어 값
     */
    uint16_t noiseFloor(uint8_t gate) const { return _noiseFloor[gate]; }

    /**
     * @brief 특정 게이트의 측정 중 관측된 최대 노이즈 피크값 반환
     * @param gate 게이트 번호 (0 ~ 15)
     * @return 최대 피크 에너지 값
     */
    uint16_t peak(uint8_t gate) const { return _peak[gate]; }

    /**
     * @brief 수집된 노이즈 플로어를 기반으로 센서에 적용할 게이트 설정 구조체 계산
     * - 움직임 감도(move_threshold)  = clamp(noise * 5, 10, 100)
     * - 정지 감도(still_threshold) = clamp(noise * 3, 5, 80)
     * @return 계산된 LD2420GateConfig 구조체
     */
    LD2420GateConfig computeConfig() const;

  private:
    LD2420CalState _state;                          ///< 캘리브레이션 상태 머신 상태
    uint16_t       _noiseFloor[LD2420_MAX_GATES];   ///< 게이트별 계산된 평균 노이즈 수치
    uint16_t       _peak[LD2420_MAX_GATES];         ///< 게이트별 관측된 최대 노이즈 피크값
    uint32_t       _sum[LD2420_MAX_GATES];          ///< 64개 샘플의 게이트별 에너지 누적 합산값
    uint16_t       _sampleCount;                    ///< 현재까지 취득된 유효 샘플 수 (0 ~ 64)
    uint32_t       _lastSampleMs;                   ///< 직전 샘플 취득 시점 (millis(), 5초 간격 유지용)

    /**
     * @brief 계산된 임계값을 안전 허용 범위[lo, hi] 내로 제한하는 인라인 헬퍼 함수
     * @param v 원본 계산값
     * @param lo 최소 한계값
     * @param hi 최대 한계값
     * @return 제한된 16비트 임계값
     */
    static uint16_t clampThreshold(uint32_t v, uint16_t lo, uint16_t hi) {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return (uint16_t)v;
    }
};

