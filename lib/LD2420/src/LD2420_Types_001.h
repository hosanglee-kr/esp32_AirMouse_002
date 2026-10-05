// =======================================================
// File: src/LD2420_Types_001.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : LD2420_Types_001.h
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
 
#include <Arduino.h>

// ==================== 상수 ====================
#define LD2420_MAX_GATES            16
#define LD2420_GATE_DISTANCE_M      0.7f
#define LD2420_CMD_QUEUE_SIZE       8
#define LD2420_DATA_QUEUE_SIZE      4
#define LD2420_UART_RX_BUFFER       256

// 타이밍 (ESPHome과 동일)
#define LD2420_CMD_ACK_TIMEOUT_MS   1000
#define LD2420_CMD_MAX_RETRIES      3
#define LD2420_STARTUP_TIMEOUT_MS   10000
#define LD2420_STARTUP_SETTLE_MS    500
#define LD2420_CAL_INTERVAL_MS      5000
#define LD2420_CAL_SAMPLES          64

// 프로토콜
#define LD2420_FRAME_HEADER         0xFAFBFCFD
#define LD2420_FRAME_FOOTER         0x01020304
#define LD2420_ENERGY_HEADER        0xF1F2F3F4
#define LD2420_ENERGY_FOOTER        0xF5F6F7F8

// 명령 코드
#define LD2420_CMD_ENABLE_CONF      0x00FF
#define LD2420_CMD_DISABLE_CONF     0x00FE
#define LD2420_CMD_WRITE_SYS_PARAM  0x0012
#define LD2420_CMD_WRITE_GATE_PARAM 0x0007
#define LD2420_CMD_READ_VERSION     0x0000
#define LD2420_CMD_RESTART          0x0068

// ==================== enum ====================
enum class LD2420State : uint8_t {
  IDLE, 
  STARTUP, 
  LISTEN_SETTLE, 
  LISTEN, 
  RUN, 
  CONFIG, 
  CALIBRATE, 
  ERROR
};

enum class LD2420CmdState : uint8_t {
  NONE, 
  SENDING, 
  WAIT_ACK, 
  RETRY
};

enum class LD2420CalState : uint8_t {
  IDLE, 
  COLLECTING, 
  READY, 
  APPLIED, 
  FAILED
};

enum class LD2420Mode : uint8_t {
  SIMPLE  = 0x0064,   // ON/OFF + Range 텍스트
  ENERGY  = 0x0004,   // 게이트별 에너지
  DEBUG   = 0x0000    // 디버그
};

// ==================== 구조체 ====================
struct LD2420GateConfig {
  uint16_t move_threshold[LD2420_MAX_GATES];
  uint16_t still_threshold[LD2420_MAX_GATES];

  void setDefault() {
    for (int g = 0; g < LD2420_MAX_GATES; g++) {
      move_threshold[g]  = (g <= 5) ? 10 : 100;
      still_threshold[g] = (g <= 5) ? 10 : 100;
    }
  }
};

struct LD2420TargetData {
  bool     presence;
  uint16_t distance_cm;
  uint16_t gate_energy[LD2420_MAX_GATES];
  uint32_t timestamp_ms;
  uint32_t sequence;
};

struct LD2420VersionInfo {
  char     firmware[16];
  uint16_t protocol;
};

struct LD2420CmdFrame {
  uint16_t command;
  uint8_t  data[18];
  uint8_t  data_length;
};

