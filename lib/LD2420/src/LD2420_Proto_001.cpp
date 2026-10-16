// =======================================================
// File: src/LD2420_Proto_001.cpp
// =======================================================

/*
 * ------------------------------------------------------
 * 소스명 : LD2420_Proto_001.cpp
 * 모듈약어 : LD2420 (L20)
 * 모듈명 : HLK-LD2420 프로토콜 프레임 빌더 및 파서 구현부
 * ------------------------------------------------------
 * 기능 요약
 *  - LD2420 명령 프레임(헤더, 길이, 명령코드, 페이로드, 푸터) 바이너리 인코딩
 *  - 설정 모드 진입/해제, 펌웨어 버전 조회, 모듈 재시작, 파라미터 쓰기 패킷 생성
 *  - 센서 응답 ACK 패킷 디코딩 및 에러 코드 판별
 *  - 16게이트 실시간 에너지 프레임 디코딩 (존재 여부, 거리, 게이트별 수신 신호 강도)
 *  - Simple 모드 아스키 텍스트("ON Range:xxx", "OFF") 파싱
 *  - 펌웨어 버전 문자열 추출
 *
 * [설계]
 *  - Little-Endian 바이트 순서 처리 (ARM Cortex 및 LD2420 센서 칩셋 표준)
 *  - ESPHome의 ld2420 컴포넌트 프레임 포맷 및 레지스터 주소 오프셋 매핑 완벽 준수
 *  - 동적 할당 없이 호출자가 전달한 고정 버퍼에 직접 기록하여 메모리 단편화 방지
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 *  - 정적 헬퍼 함수      : camelCase (putU16LE, writeHeader, writeFooter)
 *  - 클래스 메서드       : LD2420Protocol:: 접두사 + camelCase
 *  - 로컬 변수           : camelCase
 * ------------------------------------------------------
 */

#include "LD2420_Proto_001.h"

/**
 * @brief 16비트 정수를 리틀 엔디언(Little-Endian) 2바이트로 버퍼에 기록하는 인라인 헬퍼 함수
 * @param[out] p 2바이트가 기록될 시작 메모리 포인터
 * @param v 기록할 16비트 정수값
 */
static inline void putU16LE(uint8_t* p, uint16_t v) {
    p[0] = v & 0xFF;        // 하위 바이트 (LSB)
    p[1] = (v >> 8) & 0xFF; // 상위 바이트 (MSB)
}

/**
 * @brief 명령 프레임의 공통 헤더 8바이트를 버퍼에 기록
 * [구조]
 * - Byte 0~3: 헤더 매직 넘버 (0xFD, 0xFC, 0xFB, 0xFA)
 * - Byte 4~5: 페이로드 길이 (2바이트 리틀엔디언: 명령코드 2바이트 + 파라미터 바이트수)
 * - Byte 6~7: 명령 코드 (2바이트 리틀엔디언)
 *
 * @param[out] out 출력 버퍼
 * @param length 페이로드 바이트 길이
 * @param cmd 명령 코드 (예: 0x00FF, 0x0012 등)
 * @return 다음 데이터를 기록할 버퍼 오프셋 (항상 8)
 */
static size_t writeHeader(uint8_t* out, uint16_t length, uint16_t cmd) {
    out[0] = 0xFD;
    out[1] = 0xFC;
    out[2] = 0xFB;
    out[3] = 0xFA;
    putU16LE(&out[4], length);
    putU16LE(&out[6], cmd);
    return 8;
}

/**
 * @brief 명령 프레임의 끝을 알리는 공통 푸터 4바이트를 기록
 * [구조]
 * - Byte 0~3: 푸터 매직 넘버 (0x04, 0x03, 0x02, 0x01)
 *
 * @param[out] out 출력 버퍼
 * @param pos 푸터가 시작될 현재 버퍼 오프셋
 * @return 푸터가 추가된 최종 패킷 전체 바이트 길이 (pos + 4)
 */
static size_t writeFooter(uint8_t* out, size_t pos) {
    out[pos++] = 0x04;
    out[pos++] = 0x03;
    out[pos++] = 0x02;
    out[pos++] = 0x01;
    return pos;
}

// =======================================================
// [프레임 빌더 구현]
// =======================================================

/**
 * @brief 설정 모드 진입(Enable Config) 명령 프레임 빌드
 * - CMD: 0x00FF
 * - Payload: 0x0002 (LD2420 Protocol v2 지정 파라미터 2바이트)
 * - 총 길이: Header(8) + Param(2) + Footer(4) = 14바이트 (length 필드값: 4)
 */
size_t LD2420Protocol::buildEnableConfig(uint8_t* out) {
    size_t p = writeHeader(out, 4, LD2420_CMD_ENABLE_CONF);
    putU16LE(&out[p], 0x0002); // 프로토콜 버전 v2 지정
    p += 2;
    return writeFooter(out, p);
}

/**
 * @brief 설정 모드 종료(Disable Config) 명령 프레임 빌드
 * - CMD: 0x00FE
 * - Payload: 없음 (length 필드값: 2 = 명령코드 2바이트)
 * - 총 길이: Header(8) + Footer(4) = 12바이트
 */
size_t LD2420Protocol::buildDisableConfig(uint8_t* out) {
    size_t p = writeHeader(out, 2, LD2420_CMD_DISABLE_CONF);
    return writeFooter(out, p);
}

/**
 * @brief 펌웨어 버전 조회(Read Version) 명령 프레임 빌드
 * - CMD: 0x0000
 * - Payload: 없음 (length 필드값: 2)
 * - 총 길이: 12바이트
 */
size_t LD2420Protocol::buildVersionRead(uint8_t* out) {
    size_t p = writeHeader(out, 2, LD2420_CMD_READ_VERSION);
    return writeFooter(out, p);
}

/**
 * @brief 센서 모듈 재시작(Restart) 명령 프레임 빌드
 * - CMD: 0x0068
 * - Payload: 없음 (length 필드값: 2)
 * - 총 길이: 12바이트
 */
size_t LD2420Protocol::buildRestart(uint8_t* out) {
    size_t p = writeHeader(out, 2, LD2420_CMD_RESTART);
    return writeFooter(out, p);
}

/**
 * @brief 시스템 파라미터 설정 명령 프레임 빌드
 * - CMD: 0x0012
 * - Payload: [모드 하위 1B][모드 상위 1B][최소게이트 1B][최대게이트 1B][타임아웃 2B]
 * - length 필드값: 8 (명령 2B + 파라미터 6B)
 * - 총 길이: Header(8) + Param(6) + Footer(4) = 18바이트
 */
size_t LD2420Protocol::buildSysParam(uint8_t* out, uint8_t minGate, uint8_t maxGate, uint16_t timeout, LD2420Mode mode) {
    size_t p = writeHeader(out, 8, LD2420_CMD_WRITE_SYS_PARAM);
    out[p++] = (uint8_t)mode;                    // 모드 하위 바이트 (예: 0x64 또는 0x04)
    out[p++] = (uint8_t)((uint16_t)mode >> 8);  // 모드 상위 바이트 (0x00)
    out[p++] = minGate;                          // 최소 감지 게이트 (0~15)
    out[p++] = maxGate;                          // 최대 감지 게이트 (0~15)
    putU16LE(&out[p], timeout);                  // 인체 부재 판정 지연 시간 (초 단위)
    p += 2;
    return writeFooter(out, p);
}

/**
 * @brief 특정 게이트의 감도 파라미터 설정 프레임 빌드
 * - CMD: 0x0007
 * - ESPHome 레지스터 주소 맵 규칙:
 *   - 움직임 감도 레지스터 오프셋 = 0x0010 + (gate * 2)
 *   - 정지 감도 레지스터 오프셋   = 0x0020 + (gate * 2)
 * - Payload 구조:
 *   [moveOff: 2B][moveThresh: 2B][dummy 0: 2B][stillOff: 2B][stillThresh: 2B][dummy 0: 2B]
 * - length 필드값: 14 (0x000E = 명령 2B + 파라미터 12B)
 * - 총 길이: Header(8) + Param(12) + Footer(4) = 24바이트
 */
size_t LD2420Protocol::buildGateParam(uint8_t* out, uint8_t gate, uint16_t moveThresh, uint16_t stillThresh) {
    size_t   p        = writeHeader(out, 14, LD2420_CMD_WRITE_GATE_PARAM);
    uint16_t moveOff  = 0x0010 + (gate * 2);
    uint16_t stillOff = 0x0020 + (gate * 2);

    // 움직임 감도 파라미터 블록
    putU16LE(&out[p], moveOff);
    p += 2;
    putU16LE(&out[p], moveThresh);
    p += 2;
    putU16LE(&out[p], 0x0000); // 패딩
    p += 2;

    // 정지 감도 파라미터 블록
    putU16LE(&out[p], stillOff);
    p += 2;
    putU16LE(&out[p], stillThresh);
    p += 2;
    putU16LE(&out[p], 0x0000); // 패딩
    p += 2;

    return writeFooter(out, p);
}

// =======================================================
// [프레임 검사 및 길이 유틸리티]
// =======================================================

/**
 * @brief 버퍼 시작이 제어 명령 또는 ACK 응답 프레임 헤더인지 확인
 * 헤더: 0xFD 0xFC 0xFB 0xFA
 */
bool LD2420Protocol::isCommandFrame(const uint8_t* buf, size_t len) {
    if (len < 4) return false;
    return buf[0] == 0xFD && buf[1] == 0xFC && buf[2] == 0xFB && buf[3] == 0xFA;
}

/**
 * @brief 버퍼 시작이 16게이트 실시간 에너지 리포트 프레임 헤더인지 확인
 * 헤더: 0xF4 0xF3 0xF2 0xF1
 */
bool LD2420Protocol::isEnergyFrame(const uint8_t* buf, size_t len) {
    if (len < 4) return false;
    return buf[0] == 0xF4 && buf[1] == 0xF3 && buf[2] == 0xF2 && buf[3] == 0xF1;
}

/**
 * @brief 가변 프레임의 전체 길이 계산
 * - buf[4~5]: 헤더 직후에 위치한 페이로드 길이(Length) 필드
 * - 전체 길이 = 헤더(4) + 길이필드(2) + 페이로드(payload) + 푸터(4) = payload + 10바이트
 */
size_t LD2420Protocol::frameTotalLength(const uint8_t* buf) {
    uint16_t payload = buf[4] | (buf[5] << 8);
    return 6 + payload + 4; // header(4) + len(2) + payload + footer(4)
}

// =======================================================
// [프레임 파서 구현]
// =======================================================

/**
 * @brief 명령 ACK 응답 패킷 디코딩
 * [ACK 패킷 구조]
 * - Byte 0~3: Header (FD FC FB FA)
 * - Byte 4~5: Length
 * - Byte 6~7: Command 코드
 * - Byte 8~9: Error/Status 코드 (0x0000 = 성공, 0x0001 = 실패)
 * - Byte 10~13: Footer (04 03 02 01)
 */
bool LD2420Protocol::parseAck(const uint8_t* frame, size_t len, uint16_t* cmd, uint16_t* error) {
    if (len < 10) return false;
    *cmd   = frame[6] | (frame[7] << 8); // 어떤 명령에 대한 ACK인지 추출
    *error = frame[8] | (frame[9] << 8); // 0이면 성공
    return true;
}

/**
 * @brief 16개 게이트 에너지 수치 바이너리 프레임 디코딩
 * [패킷 구조]
 * - Byte 0~3  : Header (F4 F3 F2 F1)
 * - Byte 4~5  : Payload Length (0x23 = 35바이트)
 * - Byte 6    : Presence 상태 (0: 부재, 1: 인체 감지됨)
 * - Byte 7~8  : 감지 거리 Distance (cm, 리틀 엔디언)
 * - Byte 9~40 : 16개 게이트별 신호 강도 Energy (각 2바이트 리틀엔디언 * 16 = 32바이트)
 * - Byte 41~44: Footer (F8 F7 F6 F5)
 * - 최소 패킷 길이 = 4 + 2 + 1 + 2 + 32 + 4 = 45바이트
 */
bool LD2420Protocol::parseEnergyFrame(const uint8_t* frame, size_t len, LD2420TargetData* out) {
    if (len < 4 + 2 + 1 + 2 + 32 + 4) return false;

    out->presence    = (frame[6] != 0);
    out->distance_cm = frame[7] | (frame[8] << 8);

    // 16개 게이트의 신호 강도 배열 복사
    for (int g = 0; g < LD2420_MAX_GATES; g++) {
        out->gate_energy[g] = frame[9 + g * 2] | (frame[9 + g * 2 + 1] << 8);
    }
    out->timestamp_ms = millis();
    return true;
}

/**
 * @brief Simple 모드 ASCII 텍스트 라인 파싱
 * 센서가 기본 모드일 때 개행 문자로 구분된 문자열을 전송합니다.
 * - 감지 시 : "ON Range: 150" 형식
 * - 부재 시 : "OFF" 형식
 */
bool LD2420Protocol::parseSimpleLine(const char* line, size_t len, LD2420TargetData* out) {
    if (len == 0) return false;

    if (strncmp(line, "ON", 2) == 0) {
        out->presence = true;
        const char* r = strstr(line, "Range");
        if (r) {
            out->distance_cm = atoi(r + 5); // "Range" 이후 거리(cm) 정수 변환
        }
        out->timestamp_ms = millis();
        return true;
    } else if (strncmp(line, "OFF", 3) == 0) {
        out->presence     = false;
        out->distance_cm  = 0;
        out->timestamp_ms = millis();
        return true;
    }
    return false;
}

/**
 * @brief 버전 응답 바이너리 프레임에서 문자열 펌웨어 버전 추출
 * - Byte 8부터 Footer 이전까지 ASCII 출력 가능 문자(0x20~0x7E)를 모아 문자열 생성
 */
bool LD2420Protocol::parseVersion(const uint8_t* frame, size_t len, LD2420VersionInfo* out) {
    if (len < 12) return false;
    int j = 0;
    for (size_t i = 8; i < len - 4 && j < 15; i++) {
        if (frame[i] >= 0x20 && frame[i] < 0x7F) {
            out->firmware[j++] = (char)frame[i];
        }
    }
    out->firmware[j] = '\0'; // 널 종료 문자 추가
    return true;
}

