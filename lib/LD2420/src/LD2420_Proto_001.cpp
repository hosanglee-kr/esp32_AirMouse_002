// =======================================================
// File: src/LD2420_Proto_001.cpp
// =======================================================

/*
 * ------------------------------------------------------
 * 소스명 : LD2420_Proto_001.cpp
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
 
 
#include "LD2420_Proto_001.h"

static inline void putU16LE(uint8_t *p, uint16_t v) {
  p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF;
}

static size_t writeHeader(uint8_t *out, uint16_t length, uint16_t cmd) {
  out[0] = 0xFD; out[1] = 0xFC; out[2] = 0xFB; out[3] = 0xFA;
  putU16LE(&out[4], length);
  putU16LE(&out[6], cmd);
  return 8;
}

static size_t writeFooter(uint8_t *out, size_t pos) {
  out[pos++] = 0x04; out[pos++] = 0x03;
  out[pos++] = 0x02; out[pos++] = 0x01;
  return pos;
}

size_t LD2420Protocol::buildEnableConfig(uint8_t *out) {
  size_t p = writeHeader(out, 4, LD2420_CMD_ENABLE_CONF);
  putU16LE(&out[p], 0x0002); p += 2;  // protocol v2
  return writeFooter(out, p);
}

size_t LD2420Protocol::buildDisableConfig(uint8_t *out) {
  size_t p = writeHeader(out, 2, LD2420_CMD_DISABLE_CONF);
  return writeFooter(out, p);
}

size_t LD2420Protocol::buildVersionRead(uint8_t *out) {
  size_t p = writeHeader(out, 2, LD2420_CMD_READ_VERSION);
  return writeFooter(out, p);
}

size_t LD2420Protocol::buildRestart(uint8_t *out) {
  size_t p = writeHeader(out, 2, LD2420_CMD_RESTART);
  return writeFooter(out, p);
}

size_t LD2420Protocol::buildSysParam(uint8_t *out, uint8_t minGate,
                                      uint8_t maxGate, uint16_t timeout,
                                      LD2420Mode mode) {
  size_t p = writeHeader(out, 8, LD2420_CMD_WRITE_SYS_PARAM);
  out[p++] = (uint8_t)mode;      // mode low byte
  out[p++] = (uint8_t)((uint16_t)mode >> 8);
  out[p++] = minGate;
  out[p++] = maxGate;
  putU16LE(&out[p], timeout); p += 2;
  return writeFooter(out, p);
}

size_t LD2420Protocol::buildGateParam(uint8_t *out, uint8_t gate,
                                       uint16_t moveThresh,
                                       uint16_t stillThresh) {
  // ESPHome: FD FC FB FA 0E 00 07 00 10 00 60 EA 00 00 20 00 60 EA 00 00 04 03 02 01
  size_t p = writeHeader(out, 14, LD2420_CMD_WRITE_GATE_PARAM);
  uint16_t moveOff  = 0x0010 + (gate * 2);
  uint16_t stillOff = 0x0020 + (gate * 2);

  putU16LE(&out[p], moveOff);     p += 2;
  putU16LE(&out[p], moveThresh);  p += 2;
  putU16LE(&out[p], 0x0000);      p += 2;

  putU16LE(&out[p], stillOff);    p += 2;
  putU16LE(&out[p], stillThresh); p += 2;
  putU16LE(&out[p], 0x0000);      p += 2;

  return writeFooter(out, p);
}

bool LD2420Protocol::isCommandFrame(const uint8_t *buf, size_t len) {
  if (len < 4) return false;
  return buf[0] == 0xFD && buf[1] == 0xFC &&
         buf[2] == 0xFB && buf[3] == 0xFA;
}

bool LD2420Protocol::isEnergyFrame(const uint8_t *buf, size_t len) {
  if (len < 4) return false;
  return buf[0] == 0xF4 && buf[1] == 0xF3 &&
         buf[2] == 0xF2 && buf[3] == 0xF1;
}

size_t LD2420Protocol::frameTotalLength(const uint8_t *buf) {
  uint16_t payload = buf[4] | (buf[5] << 8);
  return 6 + payload + 4;  // header + len + payload + footer
}

bool LD2420Protocol::parseAck(const uint8_t *frame, size_t len,
                               uint16_t *cmd, uint16_t *error) {
  if (len < 10) return false;
  *cmd   = frame[6] | (frame[7] << 8);
  *error = frame[8] | (frame[9] << 8);
  return true;
}

bool LD2420Protocol::parseEnergyFrame(const uint8_t *frame, size_t len,
                                       LD2420TargetData *out) {
  // F4 F3 F2 F1 | LL LL | PP | DD DD | 16×EE EE | F8 F7 F6 F5
  if (len < 4 + 2 + 1 + 2 + 32 + 4) return false;
  out->presence    = (frame[6] != 0);
  out->distance_cm = frame[7] | (frame[8] << 8);
  for (int g = 0; g < LD2420_MAX_GATES; g++) {
    out->gate_energy[g] = frame[9 + g*2] | (frame[9 + g*2 + 1] << 8);
  }
  out->timestamp_ms = millis();
  return true;
}

bool LD2420Protocol::parseSimpleLine(const char *line, size_t len,
                                      LD2420TargetData *out) {
  if (len == 0) return false;
  if (strncmp(line, "ON", 2) == 0) {
    out->presence = true;
    const char *r = strstr(line, "Range");
    if (r) out->distance_cm = atoi(r + 5);
    out->timestamp_ms = millis();
    return true;
  } else if (strncmp(line, "OFF", 3) == 0) {
    out->presence = false;
    out->distance_cm = 0;
    out->timestamp_ms = millis();
    return true;
  }
  return false;
}

bool LD2420Protocol::parseVersion(const uint8_t *frame, size_t len,
                                   LD2420VersionInfo *out) {
  if (len < 12) return false;
  int j = 0;
  for (size_t i = 8; i < len - 4 && j < 15; i++) {
    if (frame[i] >= 0x20 && frame[i] < 0x7F) out->firmware[j++] = (char)frame[i];
  }
  out->firmware[j] = '\0';
  return true;
}
