// File: src/v0415/D10_Logger_0415.cpp
// =======================================================
#include "D10_Logger_0415.h"

void CL_D10_Logger::begin(Stream& p_serial) {
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

void CL_D10_Logger::clear() {
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

void CL_D10_Logger::log(EN_D10_LogLevel_t p_level, const char* p_fmt, ...) {
    if (!_serial || p_level == EN_D10_LOG_NONE) return;
    if (p_level > _logLevel) return;

    char v_msgBuf[G_D10_MSG_MAX];
    va_list v_args;
    va_start(v_args, p_fmt);
    vsnprintf(v_msgBuf, sizeof(v_msgBuf), p_fmt, v_args);
    va_end(v_args);

    _pushToRing(p_level, v_msgBuf);
    _printToSerial(p_level, v_msgBuf);
}

void CL_D10_Logger::_pushToRing(EN_D10_LogLevel_t p_level, const char* p_msg) {
#if G_D10_THREAD_SAFE
    portENTER_CRITICAL(&s_mux);
#endif
    if ((uint8_t)p_level <= G_D10_LEVEL_MAX) s_lvCount[(uint8_t)p_level]++;
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

void CL_D10_Logger::_printToSerial(EN_D10_LogLevel_t p_level, const char* p_msg) {
    if (!_serial) return;

    const char* v_color = _getColor(p_level);
    const char* v_tag   = _getTag(p_level);

    if (_showTimestamp) {
        unsigned long v_ms = millis();
        _serial->printf("[%lu.%03u] ", v_ms / 1000, (uint16_t)(v_ms % 1000));
    }

    _serial->printf("%s[%s]%s %s\r\n", v_color, v_tag, G_D10_COLOR_RESET, (p_msg ? p_msg : ""));

    if (_showMemUsage) {
        _serial->printf("   %s(Free:%luB)%s\r\n", G_D10_COLOR_CYAN,
                        (unsigned long)ESP.getFreeHeap(), G_D10_COLOR_RESET);
    }
}

const char* CL_D10_Logger::_getColor(EN_D10_LogLevel_t p_level) {
    switch (p_level) {
        case EN_D10_LOG_ERROR: return G_D10_COLOR_RED;
        case EN_D10_LOG_WARN:  return G_D10_COLOR_YELLOW;
        case EN_D10_LOG_INFO:  return G_D10_COLOR_GREEN;
        case EN_D10_LOG_DEBUG: return G_D10_COLOR_CYAN;
        default:               return G_D10_COLOR_WHITE;
    }
}

const char* CL_D10_Logger::_getTag(EN_D10_LogLevel_t p_level) {
    switch (p_level) {
        case EN_D10_LOG_ERROR: return "ERR";
        case EN_D10_LOG_WARN:  return "WRN";
        case EN_D10_LOG_INFO:  return "INF";
        case EN_D10_LOG_DEBUG: return "DBG";
        default:               return "LOG";
    }
}

void CL_D10_Logger::printBanner() {
    if (!_serial) return;
    _serial->println(F("\r\n------------------------------------------------------"));
    _serial->println(F(" AirMouse Logger (v0415, RingBuffer + Serial only)"));
    _serial->println(F("------------------------------------------------------"));
}
