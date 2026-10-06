// =======================================================
// File: src/LD2420.h
// =======================================================

#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : LD2420.h
 * 모듈약어 : LD2420 (L20)
 * 모듈명 : HLK-LD2420 24GHz FMCW 레이더 센서 메인 컨트롤러 클래스
 * ------------------------------------------------------
 * 기능 요약
 *  - FreeRTOS 듀얼 태스크(rxTask, cmdTask) 기반의 완전 비동기/논블로킹 레이더 제어기
 *  - 코어 0에 태스크를 격리/고정하여 코어 1(Arduino loopTask / AirMouse)과의 간섭 완전 차단
 *  - 16개 거리 게이트(각 0.7m, 최대 11.2m) 개별/일괄 감도 임계값 제어 및 범위 제한
 *  - ESPHome 호환 자동 캘리브레이션 (노이즈 플로어 측정 → 최적 임계값 자동 적용)
 *  - 이벤트 비트 및 콜백 함수를 통한 비동기 상태 알림 및 감지 데이터 전달
 *
 * [설계]
 *  - 아키텍처: 파사드 패턴(Facade Pattern). 복잡한 프로토콜 및 비동기 큐/태스크 처리를 단일 클래스로 추상화
 *  - 비동기 API: 모든 `request*()` 함수는 즉시 반환(non-blocking)되며, 내부 FreeRTOS 큐에 적재되어 cmdTask가 순차 처리
 *  - 스레드 안전성:
 *    - `_dataMutex`: 최신 타겟 데이터(`_latest`)의 일관성 보장
 *    - `_cmdQueue`: 태스크 간 명령 전달용 FreeRTOS 큐 (타임아웃 50ms)
 *    - `_events`: 동기화 및 셧다운 신호 전달용 FreeRTOS 이벤트 그룹
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 *  - 클래스명            : LD2420
 *  - 멤버 변수           : _ 접두사 + camelCase
 *  - 메서드명            : camelCase (요청 API는 request* 접두사)
 *  - 상용 상수           : 대문자 스네이크 케이스
 * ------------------------------------------------------
 */

#include "LD2420_Types_001.h"
#include "LD2420_Proto_001.h"
#include "LD2420_Calib_001.h"

#include <HardwareSerial.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/event_groups.h>

// =======================================================
// [콜백 함수 타입 정의]
// =======================================================

/**
 * @brief 타겟 감지 데이터 수신 시 호출되는 콜백 함수 포인터
 * @param data 최신 타겟 감지 데이터 스냅샷
 * @param ctx 사용자가 등록한 사용자 컨텍스트 포인터
 * @warning rxTask 컨텍스트에서 직접 호출되므로 블로킹 작업이나 긴 지연을 피해야 합니다.
 */
typedef void (*LD2420DataCallback)(const LD2420TargetData& data, void* ctx);

/**
 * @brief 센서 이벤트(부팅 완료, 타임아웃, 캘리브레이션 등) 발생 시 호출되는 콜백 함수 포인터
 * @param event 이벤트 이름 문자열 (예: "startup_done", "cmd_timeout", "cal_ready")
 * @param ctx 사용자가 등록한 사용자 컨텍스트 포인터
 */
typedef void (*LD2420EventCallback)(const char* event, void* ctx);

// =======================================================
// [메인 파사드 클래스]
// =======================================================

/**
 * @brief HLK-LD2420 레이더 센서 제어 클래스
 */
class LD2420 {
  public:
    /**
     * @brief 생성자
     * @param serial 하드웨어 시리얼 포트 참조 (예: Serial1, Serial2)
     * @param rxPin ESP32 수신 핀 (센서의 TX와 연결)
     * @param txPin ESP32 송신 핀 (센서의 RX와 연결)
     * @param baud 통신 속도 (기본값: 115200bps, 펌웨어 v1.5.3+ 기준)
     */
    LD2420(HardwareSerial& serial, uint8_t rxPin, uint8_t txPin, uint32_t baud = 115200);

    /**
     * @brief 소멸자 (동작 중인 FreeRTOS 태스크 및 리소스 안전 해제)
     */
    ~LD2420();

    // =======================================================
    // [수명주기 초기화 및 종료]
    // =======================================================

    /**
     * @brief 센서 통신 및 FreeRTOS 듀얼 태스크 시작
     * @param rxStackSize rxTask 스택 크기 (기본값: 4096 바이트)
     * @param cmdStackSize cmdTask 스택 크기 (기본값: 3072 바이트)
     * @param rxPriority rxTask 우선순위 (기본값: 5, 실시간 수신 보장)
     * @param cmdPriority cmdTask 우선순위 (기본값: 4)
     * @return true: 모든 태스크 및 IPC 리소스가 정상 생성됨
     * @note 태스크는 코어 0에 고정되어 코어 1의 메인 루프에 영향을 주지 않습니다.
     */
    bool begin(uint32_t    rxStackSize  = 4096,
               uint32_t    cmdStackSize = 3072,
               UBaseType_t rxPriority   = 5,
               UBaseType_t cmdPriority  = 4);

    /**
     * @brief 태스크 종료 및 모든 세마포어, 큐 리소스 해제
     */
    void end();

    // =======================================================
    // [상태 조회 API (논블로킹, 최신값 즉시 반환)]
    // =======================================================

    /** 센서가 정상 감지 실행 모드(RUN)인지 확인 */
    bool isReady() const { return _state == LD2420State::RUN; }

    /** 인체 감지 여부 반환 (true: 사람 감지됨, false: 미감지) */
    bool isPresent() const { return _latest.presence; }

    /** 감지된 타겟의 거리 반환 (단위: cm) */
    uint16_t getDistance() const { return _latest.distance_cm; }

    /** 특정 게이트의 실시간 반사 신호 에너지 반환 (에너지 모드 시 유효) */
    uint16_t getGateEnergy(uint8_t gate) const { return (gate < LD2420_MAX_GATES) ? _latest.gate_energy[gate] : 0; }

    /** 뮤텍스 보호를 통해 안전하게 전체 타겟 데이터 구조체 복사본 취득 */
    LD2420TargetData getLatestData();

    /** 센서 내부 FSM 상태 반환 */
    LD2420State state() const { return _state; }

    // =======================================================
    // [비동기 설정 명령 (Non-blocking Request API)]
    // 호출 시 즉시 반환되며, cmdTask가 백그라운드에서 순차 처리합니다.
    // =======================================================

    /** 특정 게이트의 감도 임계값 설정 요청 (0~15 게이트) */
    bool requestGateThreshold(uint8_t gate, uint16_t move, uint16_t still);

    /** 16개 전체 게이트의 감도 임계값 일괄 설정 요청 */
    bool requestAllGateThresholds(const LD2420GateConfig& cfg);

    /** 감지할 최소/최대 거리 게이트 범위 제한 (배경 잡음이나 벽면 반사 제외) */
    bool requestMinMaxDistance(uint8_t minGate, uint8_t maxGate);

    /** 인체 부재(미감지) 판정 유지 타임아웃(초) 설정 요청 */
    bool requestTimeout(uint16_t seconds);

    /** 데이터 출력 모드(SIMPLE, ENERGY, DEBUG) 변경 요청 */
    bool requestMode(LD2420Mode mode);

    /** 펌웨어 버전 정보 조회 요청 */
    bool requestFirmwareVersion();

    /** 모듈 소프트웨어 재시작 요청 */
    bool requestRestart();

    /** 설정 모드 진입(true) 또는 종료(false) 요청 */
    bool requestConfigMode(bool enable);

    // =======================================================
    // [감도 프리셋 편의 함수]
    // =======================================================

    /** 기본 권장 감도 설정 적용 (0~5번 게이트: 10, 6~15번 게이트: 100) */
    bool requestDefaultThresholds();

    /** 초고감도 설정 적용 (모든 게이트 임계값: 5) - 미세 움직임 감지 */
    bool requestSensitiveThresholds();

    /** 균형 설정 적용 (0~7번: 15, 8~15번: 50) */
    bool requestBalancedThresholds();

    // =======================================================
    // [자동 캘리브레이션 API]
    // =======================================================

    /**
     * @brief 자동 캘리브레이션 시작 (ENERGY 모드로 자동 전환하여 노이즈 수집)
     * @note 약 5.3분(64개 샘플) 동안 무인 상태를 유지해야 최적의 결과가 나옵니다.
     */
    bool startCalibration();

    /**
     * @brief 캘리브레이션 완료 후 계산된 최적 임계값을 센서에 적용하고 SIMPLE 모드로 복귀
     */
    bool applyCalibration();

    /**
     * @brief 진행 중인 캘리브레이션 취소 및 RUN 모드 복귀
     */
    void cancelCalibration();

    /** 캘리브레이션 수집 진행률 반환 (0 ~ 100%) */
    uint8_t calibrationProgress() const { return _cal.progress(); }

    /** 캘리브레이션 내부 상태 머신 상태 반환 */
    LD2420CalState calibrationState() const { return _cal.state(); }

    /** 특정 게이트의 측정된 노이즈 플로어 반환 */
    uint16_t calibrationNoiseFloor(uint8_t g) const { return _cal.noiseFloor(g); }

    // =======================================================
    // [콜백 등록 API]
    // =======================================================

    /** 데이터 수신 콜백 함수 및 사용자 컨텍스트 포인터 등록 */
    void onData(LD2420DataCallback cb, void* ctx = nullptr);

    /** 이벤트 수신 콜백 함수 및 사용자 컨텍스트 포인터 등록 */
    void onEvent(LD2420EventCallback cb, void* ctx = nullptr);

    // =======================================================
    // [거리 및 게이트 변환 유틸리티]
    // =======================================================

    /** 게이트 번호를 미터(m) 단위 거리로 변환 (게이트 * 0.7m) */
    static float gateToMeters(uint8_t gate) { return gate * LD2420_GATE_DISTANCE_M; }

    /** 미터(m) 단위 거리를 게이트 번호(0~15)로 변환 */
    static uint8_t metersToGate(float m);

  private:
    // ---------- 하드웨어 인터페이스 ----------
    HardwareSerial& _serial;        ///< 하드웨어 시리얼 참조
    uint8_t         _rxPin, _txPin; ///< RX, TX GPIO 핀 번호
    uint32_t        _baud;          ///< 보레이트

    // ---------- FreeRTOS 동기화 핸들 ----------
    TaskHandle_t       _rxTask    = nullptr; ///< 데이터 수신 및 파싱 태스크 핸들
    TaskHandle_t       _cmdTask   = nullptr; ///< 비동기 명령 송신 및 재시도 태스크 핸들
    QueueHandle_t      _cmdQueue  = nullptr; ///< 명령 프레임 대기 큐 (크기: LD2420_CMD_QUEUE_SIZE)
    SemaphoreHandle_t  _dataMutex = nullptr; ///< 최신 타겟 데이터(_latest) 보호용 뮤텍스
    EventGroupHandle_t _events    = nullptr; ///< 시스템 이벤트 통지용 이벤트 그룹

    // 이벤트 비트 플래그 정의
    static const EventBits_t EV_STARTUP_DONE = BIT0; ///< 부팅 안정화 및 첫 데이터 수신 완료
    static const EventBits_t EV_CONFIG_MODE  = BIT1; ///< 설정 모드 진입 확인 플래그
    static const EventBits_t EV_CAL_READY    = BIT2; ///< 캘리브레이션 샘플 수집 완료 플래그
    static const EventBits_t EV_SHUTDOWN     = BIT3; ///< 태스크 안전 종료 요청 플래그

    // ---------- 상태 머신 변수 (rxTask 전용 갱신) ----------
    volatile LD2420State _state        = LD2420State::IDLE; ///< 메인 FSM 상태
    volatile uint32_t    _stateStartMs = 0;                 ///< 현재 상태 진입 시각 (millis())

    // ---------- 공유 데이터 (뮤텍스 보호) ----------
    LD2420TargetData _latest; ///< 최신 타겟 감지 데이터 스냅샷

    // ---------- UART 수신 버퍼 (rxTask 전용) ----------
    uint8_t _rxBuf[LD2420_UART_RX_BUFFER]; ///< 바이너리 수신 버퍼
    size_t  _rxPos = 0;                    ///< 버퍼 쓰기 오프셋
    char    _lineBuf[64];                  ///< Simple 텍스트 모드 1라인 수신 버퍼
    size_t  _linePos = 0;                  ///< 라인 버퍼 쓰기 오프셋

    // ---------- 자동 캘리브레이션 인스턴스 ----------
    LD2420Calibration _cal;

    // ---------- 명령 송신 및 재시도 상태 (cmdTask 전용) ----------
    LD2420CmdFrame _currentCmd;                        ///< 현재 전송 처리 중인 명령 프레임
    uint8_t        _retryCount = 0;                    ///< 현재 명령 재전송 횟수
    uint32_t       _cmdSentMs  = 0;                    ///< 직전 명령 송신 시각 (ACK 타임아웃 감시용)
    LD2420CmdState _cmdState   = LD2420CmdState::NONE; ///< 명령 상태 머신

    // ---------- 등록된 콜백 ----------
    LD2420DataCallback  _dataCb   = nullptr; ///< 데이터 콜백
    void*               _dataCtx  = nullptr; ///< 데이터 콜백 컨텍스트
    LD2420EventCallback _eventCb  = nullptr; ///< 이벤트 콜백
    void*               _eventCtx = nullptr; ///< 이벤트 콜백 컨텍스트

    // ---------- FreeRTOS 태스크 본체 및 루프 ----------
    static void rxTaskTrampoline(void* arg);  ///< rxTask 정적 진입점
    static void cmdTaskTrampoline(void* arg); ///< cmdTask 정적 진입점
    void        rxTaskLoop();                 ///< 수신 태스크 메인 루프 (5ms 주기)
    void        cmdTaskLoop();                ///< 송신 태스크 메인 루프 (5ms 주기)

    // ---------- 내부 처리 함수 ----------
    void processStateMachine(uint32_t now);                  ///< 상태 전이 타이머 및 FSM 처리
    void handleByte(uint8_t b);                              ///< 수신 1바이트 처리 및 프레임 구분
    void handleCommandFrame(const uint8_t* buf, size_t len); ///< 명령 ACK/응답 처리
    void handleEnergyFrame(const uint8_t* buf, size_t len);  ///< 실시간 에너지 프레임 처리
    void handleSimpleLine(const char* line, size_t len);     ///< Simple 모드 텍스트 라인 처리
    void publishData(const LD2420TargetData& d);             ///< 최신 데이터 갱신 및 콜백 디스패치
    void emitEvent(const char* evt);                         ///< 이벤트 콜백 디스패치

    bool enqueueCommand(const LD2420CmdFrame& frame, TickType_t timeout = pdMS_TO_TICKS(50));
    bool sendFrameRaw(const uint8_t* data, size_t len); ///< UART 시리얼 직접 쓰기

    /**
     * @brief 내부 상태 변경 및 상태 진입 시각 갱신
     */
    void setState(LD2420State s) {
        _state        = s;
        _stateStartMs = millis();
    }
};
