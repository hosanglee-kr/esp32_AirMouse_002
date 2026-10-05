// =======================================================
// File: src/LD2420_001.h
// =======================================================

#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : LD2420_001.h
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
#include "LD2420_Ptoto_001.h"
#include "LD2420_Calib_001.h"

#include <HardwareSerial.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/event_groups.h>

// ==================== 콜백 ====================
typedef void (*LD2420DataCallback)(const LD2420TargetData &data, void *ctx);
typedef void (*LD2420EventCallback)(const char *event, void *ctx);

// ==================== 메인 클래스 ====================
class LD2420 {
public:
  LD2420(HardwareSerial &serial,
         uint8_t rxPin, uint8_t txPin,
         uint32_t baud = 115200);

  ~LD2420();

  // ==================== 초기화 ====================
  // begin(): FreeRTOS 태스크 생성 + 초기 상태 머신 진입
  bool begin(uint32_t rxStackSize = 4096,
             uint32_t cmdStackSize = 3072,
             UBaseType_t rxPriority  = 5,
             UBaseType_t cmdPriority = 4);

  void end();                     // 태스크 종료 + 리소스 해제

  // ==================== 상태 조회 (락프리, 최신값 즉시 반환) ====================
  bool     isReady() const   { return _state == LD2420State::RUN; }
  bool     isPresent() const { return _latest.presence; }
  uint16_t getDistance() const { return _latest.distance_cm; }
  uint16_t getGateEnergy(uint8_t gate) const {
    return (gate < LD2420_MAX_GATES) ? _latest.gate_energy[gate] : 0;
  }
  LD2420TargetData getLatestData();  // 뮤텍스 보호 전체 복사
  LD2420State state() const { return _state; }

  // ==================== 비동기 설정 명령 ====================
  // 모두 즉시 반환. cmdTask가 큐에서 꺼내 순차 처리.
  bool requestGateThreshold(uint8_t gate, uint16_t move, uint16_t still);
  bool requestAllGateThresholds(const LD2420GateConfig &cfg);
  bool requestMinMaxDistance(uint8_t minGate, uint8_t maxGate);
  bool requestTimeout(uint16_t seconds);
  bool requestMode(LD2420Mode mode);
  bool requestFirmwareVersion();
  bool requestRestart();
  bool requestConfigMode(bool enable);

  // ==================== 프리셋 ====================
  bool requestDefaultThresholds();
  bool requestSensitiveThresholds();
  bool requestBalancedThresholds();

  // ==================== 캘리브레이션 (비동기) ====================
  bool startCalibration();
  bool applyCalibration();
  void cancelCalibration();
  uint8_t calibrationProgress() const { return _cal.progress(); }
  LD2420CalState calibrationState() const { return _cal.state(); }
  uint16_t calibrationNoiseFloor(uint8_t g) const { return _cal.noiseFloor(g); }

  // ==================== 콜백 ====================
  void onData(LD2420DataCallback cb, void *ctx = nullptr);
  void onEvent(LD2420EventCallback cb, void *ctx = nullptr);

  // ==================== 유틸 ====================
  static float    gateToMeters(uint8_t gate) { return gate * LD2420_GATE_DISTANCE_M; }
  static uint8_t  metersToGate(float m);

private:
  // ---------- 하드웨어 ----------
  HardwareSerial &_serial;
  uint8_t  _rxPin, _txPin;
  uint32_t _baud;

  // ---------- FreeRTOS 핸들 ----------
  TaskHandle_t     _rxTask  = nullptr;
  TaskHandle_t     _cmdTask = nullptr;
  QueueHandle_t    _cmdQueue = nullptr;     // LD2420CmdFrame
  SemaphoreHandle_t _dataMutex = nullptr;   // _latest 보호
  EventGroupHandle_t _events = nullptr;

  // 이벤트 비트
  static const EventBits_t EV_STARTUP_DONE = BIT0;
  static const EventBits_t EV_CONFIG_MODE  = BIT1;
  static const EventBits_t EV_CAL_READY    = BIT2;
  static const EventBits_t EV_SHUTDOWN     = BIT3;

  // ---------- 상태 (rxTask가 갱신) ----------
  volatile LD2420State _state = LD2420State::IDLE;
  volatile uint32_t    _stateStartMs = 0;

  // ---------- 공유 데이터 ----------
  LD2420TargetData _latest;       // dataMutex로 보호

  // ---------- 수신 버퍼 (rxTask 전용) ----------
  uint8_t  _rxBuf[LD2420_UART_RX_BUFFER];
  size_t   _rxPos = 0;
  char     _lineBuf[64];
  size_t   _linePos = 0;

  // ---------- 캘리브레이션 ----------
  LD2420Calibration _cal;

  // ---------- 명령 재시도 상태 (cmdTask 전용) ----------
  LD2420CmdFrame _currentCmd;
  uint8_t  _retryCount = 0;
  uint32_t _cmdSentMs  = 0;
  LD2420CmdState _cmdState = LD2420CmdState::NONE;

  // ---------- 콜백 ----------
  LD2420DataCallback  _dataCb  = nullptr;
  void               *_dataCtx = nullptr;
  LD2420EventCallback _eventCb = nullptr;
  void               *_eventCtx = nullptr;

  // ---------- 태스크 본체 ----------
  static void rxTaskTrampoline(void *arg);
  static void cmdTaskTrampoline(void *arg);
  void rxTaskLoop();
  void cmdTaskLoop();

  // ---------- 내부 처리 ----------
  void processStateMachine(uint32_t now);
  void handleByte(uint8_t b);
  void handleCommandFrame(const uint8_t *buf, size_t len);
  void handleEnergyFrame(const uint8_t *buf, size_t len);
  void handleSimpleLine(const char *line, size_t len);
  void publishData(const LD2420TargetData &d);
  void emitEvent(const char *evt);

  bool enqueueCommand(const LD2420CmdFrame &frame,
                      TickType_t timeout = pdMS_TO_TICKS(50));
  bool sendFrameRaw(const uint8_t *data, size_t len);

  void setState(LD2420State s) {
    _state = s;
    _stateStartMs = millis();
  }
};

