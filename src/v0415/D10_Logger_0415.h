#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : D10_Logger_0415.h
 * 모듈약어 : D10
 * 모듈명 : Logger (RingBuffer + Serial only)
 * ------------------------------------------------------
 * 기능 요약
 *  - INFO / WARN / ERROR / DEBUG 레벨
 *  - RingBuffer(256) 순환 로그 + drop 카운터
 *  - Serial 실시간 출력 (ANSI 색상, 타임스탬프)
 *  - Thread-safe (portMUX)
 *
 * [v0415 대폭 축소 — Phase 2 grep 결과 반영]
 *  - 삭제: getStats / getCountByLevel / markOk / markFail / markSpike
 *          getConsecFail / getSpike / getLastFailMs / getDropCount
 *          getLastErrMs / getLastWarnMs / getLogsAsJsonText / saveToFile
 *  - 이유: 전부 Dead code (grep 결과 어디에서도 호출 없음)
 *  - 정책: 로그 레벨별 판정/통계는 E10 도메인이 담당 (E10::_errMpuNan 등)
 *          D10은 순수 "로그 출력" 책임만 보유 (Phase 2 Q2-c 결정)
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
// ANSI 색상
// ------------------------------------------------------
#define G_D10_COLOR_RESET   "\033[0m"
#define G_D10_COLOR_RED     "\033[31m"
#define G_D10_COLOR_YELLOW  "\033[33m"
#define G_D10_COLOR_GREEN   "\033[32m"
#define G_D10_COLOR_CYAN    "\033[36m"
#define G_D10_COLOR_WHITE   "\033[37m"

// ------------------------------------------------------
// [v0415] 상수화
// ------------------------------------------------------
static constexpr uint16_t G_D10_MSG_MAX    = 128;
static constexpr uint8_t  G_D10_LEVEL_MAX  = 4;   // EN_D10_LOG_DEBUG

// ------------------------------------------------------
// 로그 엔트리
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
    static const uint16_t BUFFER_SIZE = 256;

    static void begin(Stream& p_serial = Serial);

    static void setLevel(EN_D10_LogLevel_t p_level) { _logLevel = p_level; }
    static EN_D10_LogLevel_t getLevel() { return _logLevel; }

    static void enableTimestamp(bool p_enable) { _showTimestamp = p_enable; }
    static void enableMemUsage(bool p_enable) { _showMemUsage = p_enable; }

    // 로그 출력 (Serial + RingBuffer)
    static void log(EN_D10_LogLevel_t p_level, const char* p_fmt, ...);

    // RingBuffer clear
    static void clear();

    static void printBanner();

  private:
    static void _pushToRing(EN_D10_LogLevel_t p_level, const char* p_msg);
    static void _printToSerial(EN_D10_LogLevel_t p_level, const char* p_msg);
    static const char* _getColor(EN_D10_LogLevel_t p_level);
    static const char* _getTag(EN_D10_LogLevel_t p_level);

  private:
    static Stream*              _serial;
    static EN_D10_LogLevel_t    _logLevel;
    static bool                 _showTimestamp;
    static bool                 _showMemUsage;

#if G_D10_THREAD_SAFE
    static portMUX_TYPE         s_mux;
#endif

    static ST_D10_LogEntry      s_buffer[BUFFER_SIZE];
    static uint16_t             s_head;
    static uint16_t             s_count;
    static uint32_t             s_dropCount;
    static uint32_t             s_lvCount[G_D10_LEVEL_MAX + 1];
    static uint32_t             s_lastErrMs;
    static uint32_t             s_lastWarnMs;
};

// 정적 멤버 초기화 (inline)
inline Stream*              CL_D10_Logger::_serial         = nullptr;
inline EN_D10_LogLevel_t    CL_D10_Logger::_logLevel       = EN_D10_LOG_INFO;
inline bool                 CL_D10_Logger::_showTimestamp  = true;
inline bool                 CL_D10_Logger::_showMemUsage   = false;

#if G_D10_THREAD_SAFE
inline portMUX_TYPE         CL_D10_Logger::s_mux = portMUX_INITIALIZER_UNLOCKED;
#endif

inline ST_D10_LogEntry      CL_D10_Logger::s_buffer[CL_D10_Logger::BUFFER_SIZE];
inline uint16_t             CL_D10_Logger::s_head      = 0;
inline uint16_t             CL_D10_Logger::s_count     = 0;
inline uint32_t             CL_D10_Logger::s_dropCount = 0;
inline uint32_t             CL_D10_Logger::s_lvCount[G_D10_LEVEL_MAX + 1] = {0};
inline uint32_t             CL_D10_Logger::s_lastErrMs = 0;
inline uint32_t             CL_D10_Logger::s_lastWarnMs = 0;

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

// ------------------------------------------------------
// [v0415 fix] 호출자 명시 변형 (_C)
// ------------------------------------------------------
//  - A40 삭제 시 우발적으로 함께 제거됨 (Round L 오판정)
//  - C10_Config_0415 / B20_Ble_0415 등 모듈에서 사용 중
//  - __func__ 대신 명시적 caller 문자열을 로그 prefix로 사용
//  - 예: D10_LOGW_C("C10::loadProfile", "patch failed: idx=%u", idx)
// ------------------------------------------------------
#define D10_LOGE_C(_caller, _fmt, ...) \
    CL_D10_Logger::log(EN_D10_LOG_ERROR, "[%s] " _fmt, _D10_callerOrUnknown((_caller)), ##__VA_ARGS__)
#define D10_LOGW_C(_caller, _fmt, ...) \
    CL_D10_Logger::log(EN_D10_LOG_WARN,  "[%s] " _fmt, _D10_callerOrUnknown((_caller)), ##__VA_ARGS__)
#define D10_LOGI_C(_caller, _fmt, ...) \
    CL_D10_Logger::log(EN_D10_LOG_INFO,  "[%s] " _fmt, _D10_callerOrUnknown((_caller)), ##__VA_ARGS__)
#define D10_LOGD_C(_caller, _fmt, ...) \
    CL_D10_Logger::log(EN_D10_LOG_DEBUG, "[%s] " _fmt, _D10_callerOrUnknown((_caller)), ##__VA_ARGS__)
    