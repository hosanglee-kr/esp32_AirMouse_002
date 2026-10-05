// =======================================================
// File: src/LD2420_Ptoto_001.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : LD2420_Ptoto_001.h
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
#include <Stream.h>

class LD2420Protocol {
public:
  // 프레임 빌더 (순수 함수, ESPHome의 build helper와 동일 역할)
  static size_t buildEnableConfig(uint8_t *out);
  static size_t buildDisableConfig(uint8_t *out);
  static size_t buildVersionRead(uint8_t *out);
  static size_t buildRestart(uint8_t *out);
  static size_t buildSysParam(uint8_t *out, uint8_t minGate, uint8_t maxGate,
                               uint16_t timeout, LD2420Mode mode);
  static size_t buildGateParam(uint8_t *out, uint8_t gate,
                                uint16_t moveThresh, uint16_t stillThresh);

  // 파서
  static bool parseAck(const uint8_t *frame, size_t len,
                       uint16_t *cmd, uint16_t *error);
  static bool parseEnergyFrame(const uint8_t *frame, size_t len,
                               LD2420TargetData *out);
  static bool parseSimpleLine(const char *line, size_t len,
                              LD2420TargetData *out);
  static bool parseVersion(const uint8_t *frame, size_t len,
                           LD2420VersionInfo *out);

  // 유틸
  static bool isCommandFrame(const uint8_t *buf, size_t len);
  static bool isEnergyFrame(const uint8_t *buf, size_t len);
  static size_t frameTotalLength(const uint8_t *buf);
};

