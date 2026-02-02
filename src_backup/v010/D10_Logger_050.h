#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : D10_Logger_050.h
 * 모듈약어 : D10
 * 모듈명 : Logger (RingBuffer Only)
 * ------------------------------------------------------
 * 기능 요약
 * - 전역 통합 로깅 시스템 (Serial 실시간 출력)
 * - INFO / WARN / ERROR / DEBUG 레벨 지원
 * - RingBuffer(256개) 순환 로그 저장 및 JSON 조회(getLogsAsJson)
 * - ANSI 컬러 포맷 지원 (시리얼 콘솔용)
 * - 로그 파일 저장(saveToFile) 및 메모리 기반 진단
 * ------------------------------------------------------
 * [구현 규칙]
 * - 항상 소스 시작 주석 부분 체계 유지 및 내용 업데이트
 * - 소스 시작 주석 부분 구현규칙, 코드네이밍규칙 내용 그대로 유지, 수정금지
 * - ArduinoJson v7.x.x 사용 (v6 이하 사용 금지)
 * - JsonDocument 단일 타입만 사용
 * - createNestedArray/Object/containsKey 사용 금지
 * - memset + strlcpy 기반 안전 초기화
 * - 주석/필드명은 JSON 구조와 동일하게 유지
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
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Stream.h>
#include <stdarg.h>
#include <stdio.h>

// ------------------------------------------------------
// 로그 레벨 정의
// ------------------------------------------------------
typedef enum : uint8_t {
	EN_D10_LOG_NONE	 = 0,
	EN_D10_LOG_ERROR = 1,
	EN_D10_LOG_WARN	 = 2,
	EN_D10_LOG_INFO	 = 3,
	EN_D10_LOG_DEBUG = 4
} EN_D10_LogLevel_t;

// ------------------------------------------------------
// ANSI 색상 코드 상수
// ------------------------------------------------------
#define G_D10_COLOR_RESET  "\033[0m"
#define G_D10_COLOR_RED	   "\033[31m"
#define G_D10_COLOR_YELLOW "\033[33m"
#define G_D10_COLOR_GREEN  "\033[32m"
#define G_D10_COLOR_CYAN   "\033[36m"
#define G_D10_COLOR_WHITE  "\033[37m"

// ------------------------------------------------------
// 로그 엔트리 구조체
// ------------------------------------------------------
typedef struct {
	uint32_t		  timestamp;
	EN_D10_LogLevel_t level;
	char			  message[128];
} ST_D10_LogEntry;

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
		// 버퍼 안전 초기화
		memset(s_buffer, 0, sizeof(s_buffer));
		delay(100);
		printBanner();
	}

	// --------------------------------------------------
	// 로그 레벨 및 설정 제어
	// --------------------------------------------------
	static void setLevel(EN_D10_LogLevel_t p_level) {
		_logLevel = p_level;
	}
	static EN_D10_LogLevel_t getLevel() {
		return _logLevel;
	}

	static void enableTimestamp(bool p_enable) {
		_showTimestamp = p_enable;
	}
	static void enableMemUsage(bool p_enable) {
		_showMemUsage = p_enable;
	}

	// --------------------------------------------------
	// 로그 출력 (Serial + RingBuffer 저장)
	// --------------------------------------------------
	static void log(EN_D10_LogLevel_t p_level, const char* p_fmt, ...) {
		if (!_serial || p_level > _logLevel || p_level == EN_D10_LOG_NONE)
			return;

		char	v_msgBuf[128];
		va_list v_args;
		va_start(v_args, p_fmt);
		vsnprintf(v_msgBuf, sizeof(v_msgBuf), p_fmt, v_args);
		va_end(v_args);

		// 1. 순환 버퍼(Ring Buffer) 저장
		uint16_t v_idx			  = (s_head + s_count) % BUFFER_SIZE;
		s_buffer[v_idx].timestamp = millis();
		s_buffer[v_idx].level	  = p_level;
		strlcpy(s_buffer[v_idx].message, v_msgBuf, sizeof(s_buffer[v_idx].message));

		if (s_count < BUFFER_SIZE) {
			s_count++;
		} else {
			s_head = (s_head + 1) % BUFFER_SIZE;
		}

		// 2. 시리얼 출력 처리
		const char* v_color = _getColor(p_level);
		const char* v_tag	= _getTag(p_level);

		if (_showTimestamp) {
			unsigned long v_ms = millis();
			_serial->printf("[%lu.%03u] ", v_ms / 1000, (uint16_t)(v_ms % 1000));
		}

		_serial->printf("%s[%s]%s %s\r\n", v_color, v_tag, G_D10_COLOR_RESET, v_msgBuf);

		if (_showMemUsage) {
			_serial->printf("   %s(Free:%luB)%s\r\n", G_D10_COLOR_CYAN, (unsigned long)ESP.getFreeHeap(), G_D10_COLOR_RESET);
		}
	}

	// --------------------------------------------------
	// JSON 변환 및 파일 저장
	// --------------------------------------------------
	static void getLogsAsJson(JsonDocument& p_doc) {
		// ArduinoJson v7: to<JsonArray>() 사용
		JsonArray v_arr = p_doc["logs"].to<JsonArray>();

		for (uint16_t v_i = 0; v_i < s_count; v_i++) {
			uint16_t   v_idx = (s_head + v_i) % BUFFER_SIZE;
			// ArduinoJson v7: add<JsonObject>() 사용
			JsonObject v_obj = v_arr.add<JsonObject>();

			v_obj["ts"]	 = s_buffer[v_idx].timestamp;
			v_obj["lv"]	 = (int)s_buffer[v_idx].level;
			v_obj["msg"] = s_buffer[v_idx].message;
		}
	}

	static bool saveToFile(const char* p_path = "/json/debug.json") {
		File v_f = LittleFS.open(p_path, "w");
		if (!v_f)
			return false;

		JsonDocument v_doc;
		getLogsAsJson(v_doc);
		serializeJsonPretty(v_doc, v_f);
		v_f.close();
		return true;
	}

	static void printBanner() {
		if (!_serial)
			return;
		_serial->println(F("\r\n------------------------------------------------------"));
		_serial->println(F(" Smart Nature Wind Logger (v017, RingBuffer Only)"));
		_serial->println(F("------------------------------------------------------"));
	}

  private:
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

  private:
	static Stream*			 _serial;
	static EN_D10_LogLevel_t _logLevel;
	static bool				 _showTimestamp;
	static bool				 _showMemUsage;

	static ST_D10_LogEntry	 s_buffer[BUFFER_SIZE];
	static uint16_t			 s_head;
	static uint16_t			 s_count;
};

// ------------------------------------------------------
// 정적 멤버 변수 초기화 (inline 선언으로 중복 정의 방지)
// ------------------------------------------------------
inline Stream*			 CL_D10_Logger::_serial		   = nullptr;
inline EN_D10_LogLevel_t CL_D10_Logger::_logLevel	   = EN_D10_LOG_INFO;
inline bool				 CL_D10_Logger::_showTimestamp = true;
inline bool				 CL_D10_Logger::_showMemUsage  = false;

inline ST_D10_LogEntry	 CL_D10_Logger::s_buffer[CL_D10_Logger::BUFFER_SIZE];
inline uint16_t			 CL_D10_Logger::s_head	= 0;
inline uint16_t			 CL_D10_Logger::s_count = 0;

// ------------------------------------------------------
// 전역 로깅 매크로 (D10 접두사로 통일)
// ------------------------------------------------------

// 호출자 함수 자동 추적 매크로
#define D10_LOGE(_fmt, ...) CL_D10_Logger::log(EN_D10_LOG_ERROR, "[D10][%s] " _fmt, __func__, ##__VA_ARGS__)
#define D10_LOGW(_fmt, ...) CL_D10_Logger::log(EN_D10_LOG_WARN,  "[D10][%s] " _fmt, __func__, ##__VA_ARGS__)
#define D10_LOGI(_fmt, ...) CL_D10_Logger::log(EN_D10_LOG_INFO,  "[D10][%s] " _fmt, __func__, ##__VA_ARGS__)
#define D10_LOGD(_fmt, ...) CL_D10_Logger::log(EN_D10_LOG_DEBUG, "[D10][%s] " _fmt, __func__, ##__VA_ARGS__)

// 호출자 이름을 수동으로 지정하는 매크로용 헬퍼
inline const char* _D10_callerOrUnknown(const char* p_caller) { return p_caller ? p_caller : "unknown"; }

#define D10_LOGE_C(_caller, _fmt, ...) \
    CL_D10_Logger::log(EN_D10_LOG_ERROR, "[D10][%s] " _fmt, _D10_callerOrUnknown((_caller)), ##__VA_ARGS__)
#define D10_LOGW_C(_caller, _fmt, ...) \
    CL_D10_Logger::log(EN_D10_LOG_WARN,  "[D10][%s] " _fmt, _D10_callerOrUnknown((_caller)), ##__VA_ARGS__)
#define D10_LOGI_C(_caller, _fmt, ...) \
    CL_D10_Logger::log(EN_D10_LOG_INFO,  "[D10][%s] " _fmt, _D10_callerOrUnknown((_caller)), ##__VA_ARGS__)
#define D10_LOGD_C(_caller, _fmt, ...) \
    CL_D10_Logger::log(EN_D10_LOG_DEBUG, "[D10][%s] " _fmt, _D10_callerOrUnknown((_caller)), ##__VA_ARGS__)

