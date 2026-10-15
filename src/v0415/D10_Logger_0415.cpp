// File: src/v0415/D10_Logger_0415.cpp
// =======================================================
#include "D10_Logger_0415.h"

// =======================================================
// [v0415 rev2] static 멤버 정의 (.cpp 일원화)
//   - C++17 inline 변수 이식성 이슈 회피
//   - ESP32 Arduino Core 2.x (gnu++11) / 3.x (gnu++17) 양쪽 대응
// =======================================================
Stream*                    CL_D10_Logger::_serial         = nullptr;
volatile EN_D10_LogLevel_t CL_D10_Logger::_logLevel       = EN_D10_LOG_INFO;
volatile bool              CL_D10_Logger::_showTimestamp  = true;
volatile bool              CL_D10_Logger::_showMemUsage   = false;

#if G_D10_THREAD_SAFE
portMUX_TYPE               CL_D10_Logger::s_mux = portMUX_INITIALIZER_UNLOCKED;
#endif

ST_D10_LogEntry            CL_D10_Logger::s_buffer[CL_D10_Logger::BUFFER_SIZE];
volatile uint16_t          CL_D10_Logger::s_head           = 0;
volatile uint16_t          CL_D10_Logger::s_count          = 0;
volatile uint32_t          CL_D10_Logger::s_overwriteCount = 0;
volatile uint32_t          CL_D10_Logger::s_truncCount     = 0;
volatile uint32_t          CL_D10_Logger::s_corruptCount   = 0;

// =======================================================
// begin
// -------------------------------------------------------
// [v0415 rev2]
//   - delay(30) 제거: USB CDC enumeration은 setup() 대기 위임
//     · Native USB(ESP32-S3): 수백 ms ~ 1s 소요 → delay(30) 무의미
//     · UART 브리지: 즉시 열거 완료 → delay 불필요
//   - 권장 호출 예: setup()에서
//       Serial.begin(115200);
//       while (!Serial && (millis() - t0 < 1500)) delay(10);
//       CL_D10_Logger::begin(Serial);
// =======================================================
void CL_D10_Logger::begin(Stream& p_serial) {
    _serial = &p_serial;

#if G_D10_THREAD_SAFE
    portENTER_CRITICAL(&s_mux);
#endif
    memset(s_buffer, 0, sizeof(s_buffer));
    s_head           = 0;
    s_count          = 0;
    s_overwriteCount = 0;
    s_truncCount     = 0;
    s_corruptCount   = 0;
#if G_D10_THREAD_SAFE
    portEXIT_CRITICAL(&s_mux);
#endif

    printBanner();
}

// =======================================================
// clear
// =======================================================
void CL_D10_Logger::clear() {
#if G_D10_THREAD_SAFE
    portENTER_CRITICAL(&s_mux);
#endif
    memset(s_buffer, 0, sizeof(s_buffer));
    s_head           = 0;
    s_count          = 0;
    s_overwriteCount = 0;
    s_truncCount     = 0;
    s_corruptCount   = 0;
#if G_D10_THREAD_SAFE
    portEXIT_CRITICAL(&s_mux);
#endif
}

// =======================================================
// setLevel / getLevel
// -------------------------------------------------------
// [v0415 rev2 / #9] volatile 접근을 .cpp로 이동
// =======================================================
void CL_D10_Logger::setLevel(EN_D10_LogLevel_t p_level) {
    _logLevel = p_level;
}

EN_D10_LogLevel_t CL_D10_Logger::getLevel() {
    return _logLevel;
}

// =======================================================
// 관측용 카운터 (신규)
// =======================================================
uint32_t CL_D10_Logger::getOverwriteCount() {
    return s_overwriteCount;
}

uint32_t CL_D10_Logger::getTruncCount() {
    return s_truncCount;
}

uint32_t CL_D10_Logger::getCorruptCount() {
    return s_corruptCount;
}

// =======================================================
// log
// -------------------------------------------------------
// [v0415 rev2 / #8] 타임스탬프 단일 취득
//   - 이전: _pushToRing 내 4회 millis() 호출 (불일치 위험)
//   - 이후: 진입 시 1회 취득 → 하위 함수에 전달
// =======================================================
void CL_D10_Logger::log(EN_D10_LogLevel_t p_level, const char* p_fmt, ...) {
    if (!_serial || p_level == EN_D10_LOG_NONE) return;
    if (p_level > _logLevel) return;

    // [v0415 rev2 / #8] 단일 타임스탬프 취득
    const uint32_t v_now = (uint32_t)millis();

    char v_msgBuf[G_D10_MSG_MAX];
    va_list v_args;
    va_start(v_args, p_fmt);
    vsnprintf(v_msgBuf, sizeof(v_msgBuf), p_fmt, v_args);
    va_end(v_args);

    _pushToRing(p_level, v_msgBuf, v_now);
    _printToSerial(p_level, v_msgBuf, v_now);
}

// =======================================================
// _pushToRing — Two-Phase Locking
// -------------------------------------------------------
// [v0415 rev2 / #3]
//   Phase 1 (critical): 인덱스 확보 + 카운터 갱신 (ISR 안전, 최소 시간)
//   Phase 2 (일반):     메시지 복사 (96B, critical 외부)
//
//   각 태스크는 critical 하에 고유 슬롯 인덱스를 확보하므로
//   critical 외부의 슬롯 write는 서로 다른 슬롯에 대해 무경합.
// =======================================================
void CL_D10_Logger::_pushToRing(EN_D10_LogLevel_t p_level, const char* p_msg, uint32_t p_nowMs) {
    uint16_t v_idx;

    // ---- Phase 1: 인덱스 확보 (critical, ISR-safe) ----
#if G_D10_THREAD_SAFE
    portENTER_CRITICAL(&s_mux);
#endif
    v_idx = (uint16_t)((s_head + s_count) % BUFFER_SIZE);
    if (s_count < BUFFER_SIZE) {
        s_count++;
    } else {
        // [v0415 rev2 / #7] 네이밍 정정: drop → overwrite
        s_head = (uint16_t)((s_head + 1) % BUFFER_SIZE);
        s_overwriteCount++;
    }
#if G_D10_THREAD_SAFE
    portEXIT_CRITICAL(&s_mux);
#endif

    // ---- Phase 2: 슬롯 write (critical 외부, 96B 복사) ----
    s_buffer[v_idx].timestamp = p_nowMs;
    s_buffer[v_idx].level     = p_level;

    // [v0415 rev2 / 추가 B] truncation 방어 + 관측
    const size_t v_len = (p_msg ? strlen(p_msg) : 0);
    if (v_len >= G_D10_MSG_MAX) {
        // 앞부분 유지 + 마지막 3바이트 "..."
        const size_t v_keep = G_D10_MSG_MAX - 4;   // 3 for "..." + 1 for NUL
        memcpy(s_buffer[v_idx].message, p_msg, v_keep);
        s_buffer[v_idx].message[v_keep]              = '.';
        s_buffer[v_idx].message[v_keep + 1]          = '.';
        s_buffer[v_idx].message[v_keep + 2]          = '.';
        s_buffer[v_idx].message[G_D10_MSG_MAX - 1]   = '\0';
        s_truncCount++;
    } else {
        if (v_len > 0) memcpy(s_buffer[v_idx].message, p_msg, v_len);
        s_buffer[v_idx].message[v_len] = '\0';
    }

    // [Phase 4-A] 후검증: timestamp가 여전히 우리 것인가?
    //   - 이상 시: 다른 태스크가 같은 슬롯을 예약했거나 메모리 손상
    //   - 실사용 영향 없으나, 지속 발생 시 Phase 1 정책 재검토 필요
    if (s_buffer[v_idx].timestamp != p_nowMs) {
        s_corruptCount++;
    }
}

// =======================================================
// _printToSerial — 단일 버퍼 write
// -------------------------------------------------------
// [v0415 rev2 / #4]
//   - 이전: printf를 2회 호출 (본 메시지 + mem usage) → HWCDC 내부 분할 + 인터리빙
//   - 이후: 라인 전체를 단일 스택 버퍼에 조립 → _serial->write(buf, len) 1회
//   - USB CDC 백프레셔 시 태스크 블로킹 가능 (문서화 필요)
// =======================================================
void CL_D10_Logger::_printToSerial(EN_D10_LogLevel_t p_level, const char* p_msg, uint32_t p_nowMs) {
    if (!_serial) return;

    char   v_line[G_D10_SERIAL_LINE_MAX];
    size_t v_off = 0;

    // 안전 append 헬퍼: 남은 공간 계산 + vsnprintf
    auto _app = [&](const char* p_fmt, ...) {
        if (v_off >= sizeof(v_line) - 1) return;
        va_list ap;
        va_start(ap, p_fmt);
        const int v_r = vsnprintf(v_line + v_off, sizeof(v_line) - v_off, p_fmt, ap);
        va_end(ap);
        if (v_r > 0) {
            const size_t v_avail = sizeof(v_line) - v_off;
            v_off += ((size_t)v_r < v_avail) ? (size_t)v_r : (v_avail - 1);
        }
    };

    // ① 타임스탬프
    if (_showTimestamp) {
        _app("[%lu.%03u] ",
             (unsigned long)(p_nowMs / 1000),
             (unsigned)(p_nowMs % 1000));
    }

    // ② 컬러 + 태그 + 메시지
    _app("%s[%s]%s %s\r\n",
         _getColor(p_level),
         _getTag(p_level),
         G_D10_COLOR_RESET,
         (p_msg ? p_msg : ""));

    // ③ (옵션) mem usage
    if (_showMemUsage) {
        _app("   %s(Free:%luB)%s\r\n",
             G_D10_COLOR_CYAN,
             (unsigned long)ESP.getFreeHeap(),
             G_D10_COLOR_RESET);
    }

    // ④ 단일 write
    if (v_off > 0) {
        _serial->write(reinterpret_cast<const uint8_t*>(v_line), v_off);
    }
}

// =======================================================
// 색상 / 태그
// =======================================================
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

// =======================================================
// printBanner
// =======================================================
void CL_D10_Logger::printBanner() {
    if (!_serial) return;
    _serial->println(F("\r\n------------------------------------------------------"));
    _serial->println(F(" AirMouse Logger (v0415 rev2, RingBuffer=64, MSG_MAX=96)"));
    _serial->println(F("------------------------------------------------------"));
}
