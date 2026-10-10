// =======================================================
// File: src/LD2420_Types_001.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : LD2420_Types_001.h
 * 모듈약어 : LD2420 (L20)
 * 모듈명 : HLK-LD2420 레이더 센서 공용 타입 및 상수 정의
 * ------------------------------------------------------
 * 기능 요약
 *  - HLK-LD2420 24GHz FMCW 레이더 센서용 프로토콜 매크로, 타이밍 상수 정의
 *  - 센서 상태 머신(FSM), 명령 처리 상태, 캘리브레이션 상태, 동작 모드 enum 정의
 *  - 게이트별 감도 설정(LD2420GateConfig), 타겟 감지 데이터(LD2420TargetData),
 *    버전 정보(LD2420VersionInfo), 비동기 명령 프레임(LD2420CmdFrame) 구조체 정의
 *
 * [설계]
 *  - ESPHome LD2420 컴포넌트와 1:1 호환되는 프로토콜 바이트 규격 및 타이밍 채택
 *  - 최대 16개 거리 게이트(각 0.7m, 최대 약 11.2m)를 기준으로 데이터 구조 설계
 *  - FreeRTOS 큐 및 이벤트 그룹 간 데이터 교환에 최적화된 경량 구조체 구성
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 *  - 매크로 상수         : LD2420_ 접두사 + 대문자
 *  - enum class         : LD2420 접두사 + PascalCase
 *  - 구조체(struct)     : LD2420 접두사 + PascalCase
 *  - 구조체 멤버 변수   : snake_case
 * ------------------------------------------------------
 */

#include <Arduino.h>

// =======================================================
// [기본 하드웨어 및 버퍼 상수]
// =======================================================

/** 레이더가 지원하는 최대 거리 게이트(Gate) 개수 (Gate 0 ~ Gate 15) */
#define LD2420_MAX_GATES            16

/** 게이트 1개당 실제 감지 거리 환산 계수 (단위: 미터, 약 0.7m 간격) */
#define LD2420_GATE_DISTANCE_M      0.7f

/** FreeRTOS 비동기 명령 큐(Command Queue) 최대 보관 개수 */
#define LD2420_CMD_QUEUE_SIZE       8

/** 데이터 수신 큐 기본 크기 (내부/확장용) */
#define LD2420_DATA_QUEUE_SIZE      4

/** UART RX 수신 바이트 누적용 내부 링 버퍼 크기 (바이트) */
#define LD2420_UART_RX_BUFFER       256

// =======================================================
// [타이밍 상수] (ESPHome LD2420 기본값과 동일)
// =======================================================

/** 명령 전송 후 ACK 응답 대기 최대 시간 (밀리초, 1초 초과 시 타임아웃 판정) */
#define LD2420_CMD_ACK_TIMEOUT_MS   1000

/** 명령 ACK 미수신 시 재전송 최대 시도 횟수 */
#define LD2420_CMD_MAX_RETRIES      3

/** 부팅(시작) 시 첫 데이터 수신 대기 최대 시간 (10초 초과 시 Give-up 후 RUN 강제 진입) */
#define LD2420_STARTUP_TIMEOUT_MS   10000

/** 시리얼 포트 오픈 직후 신호선 안정화 대기 시간 (밀리초) */
#define LD2420_STARTUP_SETTLE_MS    500

/** 자동 캘리브레이션 진행 시 에너지 샘플 수집 간격 (밀리초, 5초마다 1회 샘플링) */
#define LD2420_CAL_INTERVAL_MS      5000

/** 자동 캘리브레이션에 필요한 총 샘플 수 (64회 * 5초 = 약 320초 / 5.3분) */
#define LD2420_CAL_SAMPLES          64

// =======================================================
// [프로토콜 프레임 헤더 및 푸터]
// =======================================================

/** 일반 제어 명령 및 ACK 응답 프레임 헤더 (Little Endian: FD FC FB FA) */
#define LD2420_FRAME_HEADER         0xFAFBFCFD

/** 일반 제어 명령 및 ACK 응답 프레임 푸터 (Little Endian: 04 03 02 01) */
#define LD2420_FRAME_FOOTER         0x01020304

/** 에너지 리포트 프레임 헤더 (Little Endian: F4 F3 F2 F1) */
#define LD2420_ENERGY_HEADER        0xF1F2F3F4

/** 에너지 리포트 프레임 푸터 (Little Endian: F8 F7 F6 F5) */
#define LD2420_ENERGY_FOOTER        0xF5F6F7F8

// =======================================================
// [LD2420 명령 코드]
// =======================================================

/** 설정 모드 진입 명령 (Protocol v2) */
#define LD2420_CMD_ENABLE_CONF      0x00FF

/** 설정 모드 종료 및 실행(탐지) 모드 복귀 명령 */
#define LD2420_CMD_DISABLE_CONF     0x00FE

/** 시스템 파라미터 쓰기 명령 (동작 모드, 감지 범위, 유지 타임아웃 등) */
#define LD2420_CMD_WRITE_SYS_PARAM  0x0012

/** 특정 게이트 감도 파라미터 쓰기 명령 (이동 및 정지 감도 임계값) */
#define LD2420_CMD_WRITE_GATE_PARAM 0x0007

/** 펌웨어 버전 정보 조회 명령 */
#define LD2420_CMD_READ_VERSION     0x0000

/** 센서 모듈 재시작(리셋) 명령 */
#define LD2420_CMD_RESTART          0x0068

// =======================================================
// [열거형 (Enums)]
// =======================================================

/**
 * @brief LD2420 메인 상태 머신 (FSM)
 * rxTask가 주도적으로 상태를 제어하며 전이합니다.
 */
enum class LD2420State : uint8_t {
    IDLE = 0,       ///< 미초기화 또는 대기 상태
    STARTUP,        ///< 포트 초기화 및 부팅 안정화 대기 단계
    LISTEN_SETTLE,  ///< 초기 쓰레기 데이터 drain 및 청취 안정화
    LISTEN,         ///< 첫 패킷 수신 대기 (10초 타임아웃 감시)
    RUN,            ///< 정상 감지 및 데이터 스트리밍 상태
    CONFIG,         ///< 파라미터 변경을 위한 설정 모드 진입 상태
    CALIBRATE,      ///< 배경 노이즈 수집 중인 캘리브레이션 상태
    ERROR           ///< 통신 실패 또는 하드웨어 오류 상태
};

/**
 * @brief cmdTask 비동기 명령 송신 상태 머신
 */
enum class LD2420CmdState : uint8_t {
    NONE = 0,       ///< 송신 대기 명령 없음 (유휴 상태)
    SENDING,        ///< UART 프레임 전송 중
    WAIT_ACK,       ///< 센서의 ACK 패킷 수신 대기 중
    RETRY           ///< ACK 미수신에 따른 재전송 대기 상태
};

/**
 * @brief 자동 캘리브레이션 FSM 상태
 */
enum class LD2420CalState : uint8_t {
    IDLE = 0,       ///< 캘리브레이션 미실행 상태
    COLLECTING,     ///< 5초 주기로 에너지 샘플(총 64개) 수집 진행 중
    READY,          ///< 샘플 수집 완료 및 노이즈 플로어 기반 최적 임계값 연산 완료
    APPLIED,        ///< 연산된 최적 임계값이 센서에 성공적으로 반영됨
    FAILED          ///< 데이터 이상 또는 타임아웃으로 실패
};

/**
 * @brief 센서 데이터 출력 모드
 */
enum class LD2420Mode : uint8_t {
    SIMPLE = 0x0064, ///< 기본 모드: ON/OFF 상태 + 거리(Range) 텍스트 출력
    ENERGY = 0x0004, ///< 엔지니어링 모드: 16개 게이트별 실시간 에너지 수치 바이너리 전송
    DEBUG  = 0x0000  ///< 디버그 모드
};

// =======================================================
// [데이터 구조체 (Structs)]
// =======================================================

/**
 * @brief 16개 거리 게이트별 감도 임계값 설정 구조체
 * 값이 낮을수록 민감하고, 높을수록 둔감합니다. (범위: 0 ~ 100)
 */
struct LD2420GateConfig {
    uint16_t move_threshold[LD2420_MAX_GATES];  ///< 게이트별 움직임(이동) 감지 임계값
    uint16_t still_threshold[LD2420_MAX_GATES]; ///< 게이트별 정지(호흡 등 미세 움직임) 감지 임계값

    /**
     * @brief 기본 권장 임계값 설정
     * 0~5번 게이트(약 4.2m 이내)는 민감하게(10), 6번 이상 원거리는 오탐 방지를 위해 둔감하게(100) 설정
     */
    void setDefault() {
        for (int g = 0; g < LD2420_MAX_GATES; g++) {
            move_threshold[g]  = (g <= 5) ? 10 : 100;
            still_threshold[g] = (g <= 5) ? 10 : 100;
        }
    }
};

/**
 * @brief 타겟 감지 결과 스냅샷 구조체
 * 락프리 및 뮤텍스 복사를 통해 사용자 루프나 콜백으로 전달됩니다.
 */
struct LD2420TargetData {
    bool     presence;                          ///< 인체 감지 여부 (true: 존재, false: 부재)
    uint16_t distance_cm;                       ///< 감지된 타겟의 거리 (단위: cm)
    uint16_t gate_energy[LD2420_MAX_GATES];     ///< 16개 게이트별 수신 신호 강도 (에너지 모드일 때 유효)
    uint32_t timestamp_ms;                      ///< 데이터 파싱 완료 시점의 시스템 시간 (millis())
    uint32_t sequence;                          ///< 프레임 수신 시퀀스 번호 (누적 카운터)
};

/**
 * @brief 모듈 버전 정보 구조체
 */
struct LD2420VersionInfo {
    char     firmware[16];                      ///< 펌웨어 버전 문자열 (예: "V1.5.3")
    uint16_t protocol;                          ///< 통신 프로토콜 버전 (예: 2)
};

/**
 * @brief 비동기 명령 큐에 적재되는 패킷 프레임 구조체
 */
struct LD2420CmdFrame {
    uint16_t command;                           ///< 전송할 명령 코드 (예: LD2420_CMD_WRITE_GATE_PARAM)
    uint8_t  data[18];                          ///< 명령 페이로드 데이터 버퍼
    uint8_t  data_length;                       ///< 페이로드 데이터의 유효 바이트 길이
};

