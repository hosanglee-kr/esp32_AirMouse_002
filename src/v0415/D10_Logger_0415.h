#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : D10_Logger_0415.h
 * 모듈약어 : D10
 * 모듈명 : Logger (RingBuffer + Serial only)
 * ------------------------------------------------------
 * 기능 요약
 *  - INFO / WARN / ERROR / DEBUG 레벨
 *  - RingBuffer(64) 순환 로그 + overwrite 카운터 + truncation 카운터
 *  - Serial 실시간 출력 (ANSI 색상, 타임스탬프, 단일 write)
 *  - Two-Phase Locking (인덱스 확보만 critical, 메시지 복사는 critical 외부)
 *
 * [v0415 rev2 — 권고안 반영]
 *  - SRAM 최적화: BUFFER_SIZE 256→64, MSG_MAX 128→96 (34KB → 6.5KB)
 *  - Two-Phase Locking: 인덱스만 portENTER_CRITICAL, 96B 복사는 critical 외부
 *  - 타임스탬프 단일 취득 (log 진입 시 millis() 1회)
 *  - Serial 라인 단일 버퍼 write (printf 분할 인터리빙 방지)
 *  - Dead 제거: s_lvCount, s_lastErrMs, s_lastWarnMs, G_D10_LEVEL_MAX
 *  - 네이밍 정정: s_dropCount → s_overwriteCount
 *  - 신규 관측: s_truncCount, getOverwriteCount(), getTruncCount()
 *  - begin() delay(30) 제거 → setup() 대기 위임 (USB CDC 이식성)
 *  - inline static 정의 → .cpp 이동 (ESP32 Core 2.x/3.x 이식성)
 *  - volatile: _logLevel, _showTimestamp, _showMemUsage
 *
 * [컨텍스트 계약]
 *  - Task 문맥 전용. ISR/timer 콜백에서 호출 금지.
 *  - 포터블한 확장이 필요하면 portENTER_CRITICAL_ISR 래퍼 도입 필요.
 *
 * [정책]
 *  - 로그 레벨별 판정/통계는 E10 도메인이 담당 (E10::_errMpuNan 등)
 *  - D10은 순수 "로그 출력" 책임만 보유
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <Stream.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifndef G_D10_THREAD_SAFE
#define G_D10_THREAD_SAFE 1
#endif

// ------------------------------------------------------
// ANSI 색상
// ------------------------------------------------------
#define G_D10_COLOR_RESET   "\033[0m"
#define G_D10_COLOR_RED     "\033[31m"
#define G_D10_COLOR_YELLOW  "\033[33m"
#define G_D10_COLOR_GREEN   "\033[32m"
#define G_D10_COLOR_CYAN    "\033[36m"
#define G_D10_COLOR_WHITE   "\033[37m"

// ------------------------------------------------------
// [v0415 rev2] 상수
//   - MSG_MAX: 128 → 96 (실측 최장 로그 ~58자 + 여유)
//   - SERIAL_LINE_MAX: 단일 write용 라인 버퍼 (2줄 출력 대응)
// ------------------------------------------------------
static constexpr uint16_t G_D10_MSG_MAX         = 96;
static constexpr uint16_t G_D10_SERIAL_LINE_MAX = 256;

// ------------------------------------------------------
// 로그 레벨
// ------------------------------------------------------
typedef enum : uint8_t {
    EN_D10_LOG_NONE  = 0,
    EN_D10_LOG_ERROR = 1,
    EN_D10_LOG_WARN  = 2,
    EN_D10_LOG_INFO  = 3,
    EN_D10_LOG_DEBUG = 4
} EN_D10_LogLevel_t;

// ------------------------------------------------------
// 로그 엔트리 (104 bytes, 4-byte align)
//   offset 0  : uint32_t  timestamp   (4B)
//   offset 4  : enum       level      (1B)
//   offset 5  : char[96]   message    (96B)
//   offset 101: padding               (3B)
// ------------------------------------------------------
typedef struct {
    uint32_t           timestamp;
    EN_D10_LogLevel_t  level;
    char               message[G_D10_MSG_MAX];
} ST_D10_LogEntry;

// ------------------------------------------------------
// Logger 클래스
// ------------------------------------------------------
class CL_D10_Logger {
  public:
    // [v0415 rev2] 256 → 64 (SRAM 34KB → 6.5KB)
    static constexpr uint16_t BUFFER_SIZE = 64;

    static void begin(Stream& p_serial = Serial);

    // [v0415 rev2] inline → .cpp 이동 (volatile 가시성)
    static void setLevel(EN_D10_LogLevel_t p_level);
    static EN_D10_LogLevel_t getLevel();

    // 간단 volatile write — inline 유지
    static void enableTimestamp(bool p_enable) { _showTimestamp = p_enable; }
    static void enableMemUsage(bool p_enable) { _showMemUsage = p_enable; }

    // 로그 출력 (Serial + RingBuffer)
    static void log(EN_D10_LogLevel_t p_level, const char* p_fmt, ...);

    // RingBuffer clear
    static void clear();

    static void printBanner();

    // [v0415 rev2] 관측용
    static uint32_t getOverwriteCount();
    static uint32_t getTruncCount();
    // [v0415 Phase 4-A] 관측용 (방어적)
    static uint32_t getCorruptCount();

  private:
    static void _pushToRing(EN_D10_LogLevel_t p_level, const char* p_msg, uint32_t p_nowMs);
    static void _printToSerial(EN_D10_LogLevel_t p_level, const char* p_msg, uint32_t p_nowMs);
    static const char* _getColor(EN_D10_LogLevel_t p_level);
    static const char* _getTag(EN_D10_LogLevel_t p_level);

  private:
    static Stream*                    _serial;

    // [v0415 rev2 / #9] volatile: 태스크 간 메모리 가시성
    static volatile EN_D10_LogLevel_t _logLevel;
    static volatile bool              _showTimestamp;
    static volatile bool              _showMemUsage;

#if G_D10_THREAD_SAFE
    static portMUX_TYPE               s_mux;
#endif

    static ST_D10_LogEntry            s_buffer[BUFFER_SIZE];
    static volatile uint16_t          s_head;
    static volatile uint16_t          s_count;
    // [v0415 rev2 / #7] 네이밍 정정: drop → overwrite
    static volatile uint32_t          s_overwriteCount;
    // [v0415 rev2 / 추가 B] 잘림 카운터
    static volatile uint32_t          s_truncCount;
    
    // [v0415 Phase 4-A] 슬롯 관측 카운터 (방어적)
    // -------------------------------------------------------
    //  ▸ 의미: Phase 2 후검증 실패 누적 = 실제 슬롯 손상
    //  ▸ 발생 조건:
    //      - RingBuffer(64) 가득 찬 상태 + 고빈도 로그 + 태스크 동시 진입
    //      - 예: 부팅 초기 진단 로그 폭주
    //  ▸ 영향: 로그 1~2건 손실 (시스템 동작 무영향)
    //  ▸ 정책: 값이 지속 증가하면 Phase 1 정책 재검토 필요
    //  ▸ 정상치: 0 (또는 부팅 초기 1~2)
    static volatile uint32_t          s_corruptCount;

};

// ------------------------------------------------------
// 매크로 (D10_ 접두사)
// ------------------------------------------------------
inline const char* _D10_callerOrUnknown(const char* p_caller) {
    return (p_caller && p_caller[0]) ? p_caller : "unknown";
}

#define D10_LOGE(_fmt, ...) CL_D10_Logger::log(EN_D10_LOG_ERROR, "[%s] " _fmt, __func__, ##__VA_ARGS__)
#define D10_LOGW(_fmt, ...) CL_D10_Logger::log(EN_D10_LOG_WARN,  "[%s] " _fmt, __func__, ##__VA_ARGS__)
#define D10_LOGI(_fmt, ...) CL_D10_Logger::log(EN_D10_LOG_INFO,  "[%s] " _fmt, __func__, ##__VA_ARGS__)
#define D10_LOGD(_fmt, ...) CL_D10_Logger::log(EN_D10_LOG_DEBUG, "[%s] " _fmt, __func__, ##__VA_ARGS__)

#define D10_LOGE_C(_caller, _fmt, ...) \
    CL_D10_Logger::log(EN_D10_LOG_ERROR, "[%s] " _fmt, _D10_callerOrUnknown((_caller)), ##__VA_ARGS__)
#define D10_LOGW_C(_caller, _fmt, ...) \
    CL_D10_Logger::log(EN_D10_LOG_WARN,  "[%s] " _fmt, _D10_callerOrUnknown((_caller)), ##__VA_ARGS__)
#define D10_LOGI_C(_caller, _fmt, ...) \
    CL_D10_Logger::log(EN_D10_LOG_INFO,  "[%s] " _fmt, _D10_callerOrUnknown((_caller)), ##__VA_ARGS__)
#define D10_LOGD_C(_caller, _fmt, ...) \
    CL_D10_Logger::log(EN_D10_LOG_DEBUG, "[%s] " _fmt, _D10_callerOrUnknown((_caller)), ##__VA_ARGS__)
