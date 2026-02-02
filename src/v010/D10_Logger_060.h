#pragma once

/*
 * ------------------------------------------------------
 * 소스명 : D10_Logger_060.h
 * 모듈약어 : D10
 * 모듈명 : Logger (AirMouse Full, RingBuffer + JSON Text Export)
 * ------------------------------------------------------
 * 기능 요약
 *  - 전역 통합 로깅 시스템 (Serial 실시간 출력)
 *  - INFO / WARN / ERROR / DEBUG 레벨 지원
 *  - RingBuffer(256개) 순환 로그 저장 + overwrite(drop) 카운터
 *  - Thread-safe 보호(portMUX) (Core0/1 task + AsyncWebServer 동시 호출 대비)
 *  - JSON은 ArduinoJson nested 생성 없이 "JSON Text"로 export (정책/메모리 안전)
 *  - (옵션) LittleFS 파일 저장(saveToFile) (FS 장애 시에도 동작 유지)
 * ------------------------------------------------------
 * [구현 규칙]
 *  - 항상 소스 시작 주석 부분 체계 유지 및 내용 업데이트
 *  - 소스 시작 주석 부분 구현규칙, 코드네이밍규칙 내용 그대로 유지, 수정금지
 *  - ArduinoJson v7.x.x 사용 (v6 이하 사용 금지)
 *  - JsonDocument 단일 타입만 사용
 *  - createNestedArray/Object/containsKey 사용 금지
 *  - memset + strlcpy 기반 안전 초기화
 *  - 주석/필드명은 JSON 구조와 동일하게 유지
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 * - 전역 상수,매크로      : G_모듈약어_ 접두사
 * - 전역 변수             : g_모듈약어_ 접두사
 * - 전역 함수             : 모듈약어_ 접두사
 * - type                  : T_모듈약어_ 접두사
 * - typedef               : _t  접미사
 * - enum 상수             : EN_모듈약어_ 접두사
 * - 구조체                : ST_모듈약어_ 접두사
 * - 클래스명              : CL_모듈약어_ 접두사
 * - 클래스 private 멤버   : _ 접두사
 * - 클래스 멤버(함수/변수) : 모듈약어 접두사 미사용
 * - 클래스 정적 멤버      : s_ 접두사
 * - 함수 로컬 변수        : v_ 접두사
 * - 함수 인자             : p_접두사
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <Stream.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <ArduinoJson.h>

#ifndef G_D10_THREAD_SAFE
#define G_D10_THREAD_SAFE 1
#endif

#ifndef G_D10_ENABLE_LFS
#define G_D10_ENABLE_LFS 1
#endif

#if G_D10_ENABLE_LFS
#include <LittleFS.h>
#endif

// ------------------------------------------------------
// 로그 레벨 정의
// ------------------------------------------------------
typedef enum : uint8_t {
    EN_D10_LOG_NONE  = 0,
    EN_D10_LOG_ERROR = 1,
    EN_D10_LOG_WARN  = 2,
    EN_D10_LOG_INFO  = 3,
    EN_D10_LOG_DEBUG = 4
} EN_D10_LogLevel_t;

// ------------------------------------------------------
// ANSI 색상 코드 상수
// ------------------------------------------------------
#define G_D10_COLOR_RESET  "\033[0m"
#define G_D10_COLOR_RED    "\033[31m"
#define G_D10_COLOR_YELLOW "\033[33m"
#define G_D10_COLOR_GREEN  "\033[32m"
#define G_D10_COLOR_CYAN   "\033[36m"
#define G_D10_COLOR_WHITE  "\033[37m"

// ------------------------------------------------------
// 로그 엔트리 구조체
// ------------------------------------------------------
typedef struct {
    uint32_t           timestamp;
    EN_D10_LogLevel_t  level;
    char               message[128];
} ST_D10_LogEntry;

// ------------------------------------------------------
// (현장 대시보드용) 상태 스냅샷
// ------------------------------------------------------
typedef struct {
    uint32_t drop;
    uint32_t lastErrMs;
    uint32_t lastWarnMs;
    uint32_t cntErr;
    uint32_t cntWarn;
    uint32_t cntInfo;
    uint32_t cntDbg;
    uint16_t buffered;   // 현재 저장 개수
    uint16_t capacity;   // 256
} ST_D10_Stats_t;

// ------------------------------------------------------
// Logger 클래스
// ------------------------------------------------------
class CL_D10_Logger {
  public:
    static const uint16_t BUFFER_SIZE = 256;

    // --------------------------------------------------
    // 초기화
    // --------------------------------------------------
    static void begin(Stream& p_serial = Serial) {
        _serial = &p_serial;

#if G_D10_THREAD_SAFE
        portENTER_CRITICAL(&s_mux);
#endif
        memset(s_buffer, 0, sizeof(s_buffer));
        s_head = 0;
        s_count = 0;
        s_dropCount = 0;
        memset(s_lvCount, 0, sizeof(s_lvCount));
        s_lastErrMs = 0;
        s_lastWarnMs = 0;
#if G_D10_THREAD_SAFE
        portEXIT_CRITICAL(&s_mux);
#endif

        delay(30);
        printBanner();
    }

    // --------------------------------------------------
    // 로그 레벨 및 설정 제어
    // --------------------------------------------------
    static void setLevel(EN_D10_LogLevel_t p_level) { _logLevel = p_level; }
    static EN_D10_LogLevel_t getLevel() { return _logLevel; }

    static void enableTimestamp(bool p_enable) { _showTimestamp = p_enable; }
    static void enableMemUsage(bool p_enable) { _showMemUsage = p_enable; }

    // --------------------------------------------------
    // 기본 상태 요약 (status/dashboard 용)
    // --------------------------------------------------
    static uint32_t getDropCount() { return s_dropCount; }
    static uint32_t getLastErrMs() { return s_lastErrMs; }
    static uint32_t getLastWarnMs() { return s_lastWarnMs; }

    static uint32_t getCountByLevel(EN_D10_LogLevel_t p_lv) {
        uint8_t v = (uint8_t)p_lv;
        if (v > 4) return 0;
        return s_lvCount[v];
    }

    static void getStats(ST_D10_Stats_t& p_out) {
        memset(&p_out, 0, sizeof(p_out));
#if G_D10_THREAD_SAFE
        portENTER_CRITICAL(&s_mux);
#endif
        p_out.drop      = s_dropCount;
        p_out.lastErrMs = s_lastErrMs;
        p_out.lastWarnMs= s_lastWarnMs;
        p_out.cntErr    = s_lvCount[(uint8_t)EN_D10_LOG_ERROR];
        p_out.cntWarn   = s_lvCount[(uint8_t)EN_D10_LOG_WARN];
        p_out.cntInfo   = s_lvCount[(uint8_t)EN_D10_LOG_INFO];
        p_out.cntDbg    = s_lvCount[(uint8_t)EN_D10_LOG_DEBUG];
        p_out.buffered  = s_count;
        p_out.capacity  = BUFFER_SIZE;
#if G_D10_THREAD_SAFE
        portEXIT_CRITICAL(&s_mux);
#endif
    }

    // --------------------------------------------------
    // 로그 출력 (Serial + RingBuffer 저장)
    // --------------------------------------------------
    static void log(EN_D10_LogLevel_t p_level, const char* p_fmt, ...) {
        if (!_serial || p_level == EN_D10_LOG_NONE) return;
        if (p_level > _logLevel) return;

        char v_msgBuf[128];
        va_list v_args;
        va_start(v_args, p_fmt);
        vsnprintf(v_msgBuf, sizeof(v_msgBuf), p_fmt, v_args);
        va_end(v_args);

        _pushToRing(p_level, v_msgBuf);
        _printToSerial(p_level, v_msgBuf);
    }

    // --------------------------------------------------
    // RingBuffer clear (현장 조치용)
    // --------------------------------------------------
    static void clear() {
#if G_D10_THREAD_SAFE
        portENTER_CRITICAL(&s_mux);
#endif
        memset(s_buffer, 0, sizeof(s_buffer));
        s_head = 0;
        s_count = 0;
        s_dropCount = 0;
        memset(s_lvCount, 0, sizeof(s_lvCount));
        s_lastErrMs = 0;
        s_lastWarnMs = 0;
#if G_D10_THREAD_SAFE
        portEXIT_CRITICAL(&s_mux);
#endif
    }

    // --------------------------------------------------
    // JSON Text Export (정책/메모리 안전)
    //  - createNestedArray/Object 없이 문자열로 JSON 생성
    //  - p_max: 최신 p_max개만 출력
    // --------------------------------------------------
    static void getLogsAsJsonText(String& p_out, uint16_t p_max = 160) {
        p_out.reserve(4096);
        p_out = "{\"logs\":[";

        // snapshot
        uint16_t v_total = 0;
        uint16_t v_n = 0;
        uint16_t v_start = 0;

        uint32_t v_drop = 0;
        uint32_t v_lastErr = 0;
        uint32_t v_lastWarn = 0;
        uint32_t v_errCnt = 0;
        uint32_t v_warnCnt = 0;
        uint32_t v_infoCnt = 0;
        uint32_t v_dbgCnt = 0;

#if G_D10_THREAD_SAFE
        portENTER_CRITICAL(&s_mux);
#endif
        v_total = s_count;
        v_n = v_total;
        if (p_max < v_n) v_n = p_max;

        v_start = s_head;
        if (v_total > v_n) {
            v_start = (s_head + (v_total - v_n)) % BUFFER_SIZE;
        }

        // logs
        for (uint16_t i = 0; i < v_n; i++) {
            uint16_t v_idx = (v_start + i) % BUFFER_SIZE;
            const ST_D10_LogEntry& e = s_buffer[v_idx];

            if (i) p_out += ",";
            p_out += "{\"ts\":";
            p_out += String(e.timestamp);
            p_out += ",\"lv\":";
            p_out += String((int)e.level);
            p_out += ",\"msg\":\"";
            _appendJsonEscaped(p_out, e.message);
            p_out += "\"}";
        }

        // stats
        v_drop = s_dropCount;
        v_lastErr = s_lastErrMs;
        v_lastWarn = s_lastWarnMs;
        v_errCnt = s_lvCount[(uint8_t)EN_D10_LOG_ERROR];
        v_warnCnt = s_lvCount[(uint8_t)EN_D10_LOG_WARN];
        v_infoCnt = s_lvCount[(uint8_t)EN_D10_LOG_INFO];
        v_dbgCnt = s_lvCount[(uint8_t)EN_D10_LOG_DEBUG];
#if G_D10_THREAD_SAFE
        portEXIT_CRITICAL(&s_mux);
#endif

        p_out += "],\"drop\":";
        p_out += String(v_drop);
        p_out += ",\"lvCount\":{\"err\":";
        p_out += String(v_errCnt);
        p_out += ",\"warn\":";
        p_out += String(v_warnCnt);
        p_out += ",\"info\":";
        p_out += String(v_infoCnt);
        p_out += ",\"dbg\":";
        p_out += String(v_dbgCnt);
        p_out += "},\"lastErrMs\":";
        p_out += String(v_lastErr);
        p_out += ",\"lastWarnMs\":";
        p_out += String(v_lastWarn);
        p_out += "}";
    }

#if G_D10_ENABLE_LFS
    // --------------------------------------------------
    // 파일 저장 (옵션)
    // --------------------------------------------------
    static bool saveToFile(const char* p_path = "/json/debug.json", uint16_t p_max = 200) {
        if (!p_path || !p_path[0]) return false;

        File v_f = LittleFS.open(p_path, "w");
        if (!v_f) return false;

        String v_json;
        getLogsAsJsonText(v_json, p_max);
        v_f.print(v_json);
        v_f.flush();
        v_f.close();
        return true;
    }
#endif

    static void printBanner() {
        if (!_serial) return;
        _serial->println(F("\r\n------------------------------------------------------"));
        _serial->println(F(" AirMouse Logger (v060, RingBuffer + JSON Text Export)"));
        _serial->println(F("------------------------------------------------------"));
    }

  private:
    static void _pushToRing(EN_D10_LogLevel_t p_level, const char* p_msg) {
#if G_D10_THREAD_SAFE
        portENTER_CRITICAL(&s_mux);
#endif
        if ((uint8_t)p_level <= 4) s_lvCount[(uint8_t)p_level]++;
        if (p_level == EN_D10_LOG_ERROR) s_lastErrMs = millis();
        if (p_level == EN_D10_LOG_WARN)  s_lastWarnMs = millis();

        uint16_t v_idx = (s_head + s_count) % BUFFER_SIZE;
        s_buffer[v_idx].timestamp = millis();
        s_buffer[v_idx].level = p_level;
        strlcpy(s_buffer[v_idx].message, (p_msg ? p_msg : ""), sizeof(s_buffer[v_idx].message));

        if (s_count < BUFFER_SIZE) {
            s_count++;
        } else {
            s_head = (s_head + 1) % BUFFER_SIZE;
            s_dropCount++;
        }
#if G_D10_THREAD_SAFE
        portEXIT_CRITICAL(&s_mux);
#endif
    }

    static void _printToSerial(EN_D10_LogLevel_t p_level, const char* p_msg) {
        if (!_serial) return;

        const char* v_color = _getColor(p_level);
        const char* v_tag   = _getTag(p_level);

        if (_showTimestamp) {
            unsigned long v_ms = millis();
            _serial->printf("[%lu.%03u] ", v_ms / 1000, (uint16_t)(v_ms % 1000));
        }

        _serial->printf("%s[%s]%s %s\r\n", v_color, v_tag, G_D10_COLOR_RESET, (p_msg ? p_msg : ""));

        if (_showMemUsage) {
            _serial->printf("   %s(Free:%luB)%s\r\n", G_D10_COLOR_CYAN, (unsigned long)ESP.getFreeHeap(), G_D10_COLOR_RESET);
        }
    }

    static const char* _getColor(EN_D10_LogLevel_t p_level) {
        switch (p_level) {
            case EN_D10_LOG_ERROR: return G_D10_COLOR_RED;
            case EN_D10_LOG_WARN:  return G_D10_COLOR_YELLOW;
            case EN_D10_LOG_INFO:  return G_D10_COLOR_GREEN;
            case EN_D10_LOG_DEBUG: return G_D10_COLOR_CYAN;
            default:               return G_D10_COLOR_WHITE;
        }
    }

    static const char* _getTag(EN_D10_LogLevel_t p_level) {
        switch (p_level) {
            case EN_D10_LOG_ERROR: return "ERR";
            case EN_D10_LOG_WARN:  return "WRN";
            case EN_D10_LOG_INFO:  return "INF";
            case EN_D10_LOG_DEBUG: return "DBG";
            default:               return "LOG";
        }
    }

    // JSON 문자열 이스케이프(최소)
    static void _appendJsonEscaped(String& p_out, const char* p_s) {
        if (!p_s) return;
        for (size_t i = 0; p_s[i] != '\0'; i++) {
            char c = p_s[i];
            if (c == '\"' || c == '\\') { p_out += '\\'; p_out += c; }
            else if (c == '\n') p_out += "\\n";
            else if (c == '\r') p_out += "\\r";
            else if (c == '\t') p_out += "\\t";
            else p_out += c;
        }
    }

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
    static uint32_t             s_lvCount[5];
    static uint32_t             s_lastErrMs;
    static uint32_t             s_lastWarnMs;
};

// ------------------------------------------------------
// 정적 멤버 변수 초기화 (inline 선언으로 중복 정의 방지)
// ------------------------------------------------------
inline Stream*              CL_D10_Logger::_serial         = nullptr;
inline EN_D10_LogLevel_t    CL_D10_Logger::_logLevel       = EN_D10_LOG_INFO;
inline bool                 CL_D10_Logger::_showTimestamp  = true;
inline bool                 CL_D10_Logger::_showMemUsage   = false;

#if G_D10_THREAD_SAFE
inline portMUX_TYPE         CL_D10_Logger::s_mux           = portMUX_INITIALIZER_UNLOCKED;
#endif

inline ST_D10_LogEntry      CL_D10_Logger::s_buffer[CL_D10_Logger::BUFFER_SIZE];
inline uint16_t             CL_D10_Logger::s_head          = 0;
inline uint16_t             CL_D10_Logger::s_count         = 0;

inline uint32_t             CL_D10_Logger::s_dropCount     = 0;
inline uint32_t             CL_D10_Logger::s_lvCount[5]    = {0,0,0,0,0};
inline uint32_t             CL_D10_Logger::s_lastErrMs     = 0;
inline uint32_t             CL_D10_Logger::s_lastWarnMs    = 0;

// ------------------------------------------------------
// 전역 로깅 매크로 (D10 접두사로 통일)
// ------------------------------------------------------
inline const char* _D10_callerOrUnknown(const char* p_caller) { return (p_caller && p_caller[0]) ? p_caller : "unknown"; }

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
    


