// ======================================================
// 소스명 : A40_ComFunc_060.h
// 모듈명 : Common Utilities (Full)
// ======================================================
#pragma once

#include <Arduino.h>
#include <string.h>
#include <strings.h>

#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <memory> // std::shared_ptr
#include <new>    // std::nothrow

// #include "A20_Const_Const_050.h"
#include "D10_Logger_050.h"

// ======================================================
// [A40 공통 로그 정책]
// ======================================================
// - 공통함수 내부 로그에 "호출자 함수명" 출력
// - 호출자가 __func__ 를 전달하도록 매크로 제공
// - caller 미전달 시 "?" 표시
// ======================================================

// [A40] caller 문자열이 비정상이면 "?"로 대체
static inline const char* _A40__callerOrUnknown(const char* p_callerFunc) {
    return (p_callerFunc && p_callerFunc[0]) ? p_callerFunc : "?";
}


// ======================================================
// 1) 공용 헬퍼
// ======================================================
namespace A40_ComFunc {

// [A40] 값 범위를 [low, high]로 클램프
template <typename T>
inline constexpr T clampVal(T p_value, T p_lowValue, T p_highValue) {
    return (p_value < p_lowValue) ? p_lowValue : (p_value > p_highValue) ? p_highValue : p_value;
}

// -------------------------
// 문자열 유틸
// -------------------------

// [A40] 안전 문자열 복사(strlcpy) + dst/src 유효성 로그
inline size_t copyStr2Buffer_safe(char* p_dst, const char* p_src, size_t p_n, const char* p_callerFunc = nullptr) {
    const char* v_caller = _A40__callerOrUnknown(p_callerFunc);

    if (!p_dst || p_n == 0) {
        CL_D10_Logger::log(EN_L10_LOG_WARN, "[A40][%s] copyStr2Buffer_safe: invalid dst/size", v_caller);
        return 0;
    }

    if (!p_src) {
        p_dst[0] = '\0';
        CL_D10_Logger::log(EN_L10_LOG_WARN, "[A40][%s] copyStr2Buffer_safe: src is null", v_caller);
        return 0;
    }

    return strlcpy(p_dst, p_src, p_n);
}

// [A40] C-string을 shared_ptr<char[]>로 복제(할당 실패/NULL 방어)
inline std::shared_ptr<char[]> cloneStr2SharedStr_safe(const char* p_src, const char* p_callerFunc = nullptr) {
    const char* v_caller = _A40__callerOrUnknown(p_callerFunc);

    if (!p_src) {
        CL_D10_Logger::log(EN_L10_LOG_WARN, "[A40][%s] cloneStr2SharedStr_safe: src is null", v_caller);
        return nullptr;
    }

    const size_t            v_len = strlen(p_src) + 1;
    std::shared_ptr<char[]> v_buf(new (std::nothrow) char[v_len], std::default_delete<char[]>());

    if (!v_buf) {
        CL_D10_Logger::log(EN_L10_LOG_ERROR,
                           "[A40][%s] cloneStr2SharedStr_safe: alloc failed (%u bytes)",
                           v_caller,
                           (unsigned)v_len);
        return nullptr;
    }

    strlcpy(v_buf.get(), p_src, v_len);
    return v_buf;
}


// ======================================================
// JSON Helper (containsKey 금지 정책)
// ======================================================

// [A40] 문자열 필드 읽기(없음/타입불일치/빈문자열 => default)
static inline const char* Json_getStr(JsonObjectConst p_obj, const char* p_key, const char* p_defaultVal) {
    if (p_obj.isNull()) return p_defaultVal;
    JsonVariantConst v = p_obj[p_key];
    if (v.isNull() || !v.is<const char*>()) return p_defaultVal;
    const char* s = v.as<const char*>();
    return (s && s[0]) ? s : p_defaultVal;
}

// [A40] 숫자 필드 읽기(없음/타입불일치 => default)
template <typename T>
static inline T Json_getNum(JsonObjectConst p_obj, const char* p_key, T p_defaultVal) {
    if (p_obj.isNull()) return p_defaultVal;
    JsonVariantConst v = p_obj[p_key];
    if (v.isNull() || !v.is<T>()) return p_defaultVal;
    return v.as<T>();
}

// [A40] bool 필드 읽기(없음/타입불일치 => default)
static inline bool Json_getBool(JsonObjectConst p_obj, const char* p_key, bool p_defaultVal) {
    if (p_obj.isNull()) return p_defaultVal;
    JsonVariantConst v = p_obj[p_key];
    if (v.isNull() || !v.is<bool>()) return p_defaultVal;
    return v.as<bool>();
}

// [A40] 배열 필드 읽기(없음/타입불일치 => null array)
static inline JsonArrayConst Json_getArr(JsonObjectConst p_obj, const char* p_key) {
    if (p_obj.isNull()) return JsonArrayConst();
    JsonVariantConst v = p_obj[p_key];
    if (v.isNull() || !v.is<JsonArrayConst>()) return JsonArrayConst();
    return v.as<JsonArrayConst>();
}

// [A40] doc에서 wrapKey가 있으면 그 오브젝트, 없으면 doc root를 반환
static inline JsonObjectConst Json_pickRootObject(JsonDocument& p_doc, const char* p_wrapKey) {
    JsonObjectConst v = p_doc[p_wrapKey].as<JsonObjectConst>();
    return v.isNull() ? p_doc.as<JsonObjectConst>() : v;
}

// [A40] const doc에서 wrapKey가 있으면 그 오브젝트, 없으면 doc root를 반환
static inline JsonObjectConst Json_pickRootObject(const JsonDocument& p_doc, const char* p_wrapKey) {
    JsonObjectConst v = p_doc[p_wrapKey].as<JsonObjectConst>();
    return v.isNull() ? p_doc.as<JsonObjectConst>() : v;
}


// ------------------------------------------------------
// Json_ensureObject
//  - p_v 위치가 JsonObject임을 보장 (방어적 생성)
//  - 이미 객체면 그대로 반환, 아니면 해당 위치를 빈 객체{}로 초기화
// ------------------------------------------------------
/**
 * @brief JsonVariant 내에 JsonObject가 존재함을 보장하는 헬퍼 함수
 * @details 
 * - 지정된 위치(p_v)가 이미 객체인지 확인합니다.
 * - 만약 객체가 아니거나(Null) 다른 타입(배열, 숫자 등)인 경우 해당 위치를 
 * 빈 JsonObject로 초기화(to<JsonObject>)하여 반환합니다.
 * - 복잡한 계층 구조를 생성할 때 중간 경로를 안전하게 확보하는 용도로 사용합니다.
 * * @param p_v 검사 및 변환할 대상 JsonVariant (예: p_doc["control"])
 * @return JsonObject 유효한 객체 참조 (실패 시에도 빈 객체 보장)
 */
static inline JsonObject Json_ensureObject(JsonVariant p_v) {
    // 1. 이미 JsonObject 타입인지 확인 (기존 데이터 유지)
    if (p_v.is<JsonObject>()) {
        return p_v.as<JsonObject>();
    }
    
    // 2. 객체가 아니거나 다른 타입이면 강제 변환 (기존 값은 유실될 수 있음)
    return p_v.to<JsonObject>();
}


// ------------------------------------------------------
// Json_copyStr (Silent-safe)
// ------------------------------------------------------

// [A40] JSON 문자열을 dst에 복사(키 없음/불일치 => default로 채움, dst는 항상 memset(0))
static inline bool Json_copyStr(JsonObjectConst p_obj,
                                const char*     p_key,
                                char*           p_dst,
                                size_t          p_dstSize,
                                const char*     p_defaultVal = "",
                                const char*     p_callerFunc = nullptr) {
    const char* v_caller = _A40__callerOrUnknown(p_callerFunc);

    if (!p_dst || p_dstSize == 0) {
        CL_D10_Logger::log(EN_L10_LOG_WARN, "[A40][%s] Json_copyStr: invalid dst/size", v_caller);
        return false;
    }

    memset(p_dst, 0, p_dstSize);

    if (!p_key || !p_key[0]) {
        CL_D10_Logger::log(EN_L10_LOG_WARN, "[A40][%s] Json_copyStr: invalid key", v_caller);
        const char* v_def = (p_defaultVal ? p_defaultVal : "");
        strlcpy(p_dst, v_def, p_dstSize);
        return false;
    }

    const char* v_src = A40_ComFunc::Json_getStr(p_obj, p_key, p_defaultVal ? p_defaultVal : "");
    strlcpy(p_dst, (v_src ? v_src : ""), p_dstSize);
    return true;
}

// [A40] JSON 문자열을 dst에 복사(로그 정책은 copyStr2Buffer_safe 방식으로)
static inline bool Json_copyStr_LogPolicy(JsonObjectConst p_obj,
                                          const char*     p_key,
                                          char*           p_dst,
                                          size_t          p_dstSize,
                                          const char*     p_defaultVal = "",
                                          const char*     p_callerFunc = nullptr) {
    const char* v_caller = _A40__callerOrUnknown(p_callerFunc);

    if (!p_dst || p_dstSize == 0) {
        CL_D10_Logger::log(EN_L10_LOG_WARN, "[A40][%s] Json_copyStr_LogPolicy: invalid dst/size", v_caller);
        return false;
    }

    memset(p_dst, 0, p_dstSize);

    if (!p_key || !p_key[0]) {
        CL_D10_Logger::log(EN_L10_LOG_WARN, "[A40][%s] Json_copyStr_LogPolicy: invalid key", v_caller);
        A40_ComFunc::copyStr2Buffer_safe(p_dst, (p_defaultVal ? p_defaultVal : ""), p_dstSize, v_caller);
        return false;
    }

    const char* v_src = A40_ComFunc::Json_getStr(p_obj, p_key, p_defaultVal ? p_defaultVal : "");
    size_t v_copied = A40_ComFunc::copyStr2Buffer_safe(p_dst, (v_src ? v_src : ""), p_dstSize, v_caller);
    return (v_copied > 0);
}

// [A40] 필수 문자열 키 정책: 누락/타입불일치/빈문자열이면 경고 + default 채움 + false 반환
static inline bool Json_copyStrReq(JsonObjectConst p_obj,
                                   const char*     p_key,
                                   char*           p_dst,
                                   size_t          p_dstSize,
                                   const char*     p_defaultVal = "",
                                   const char*     p_callerFunc = nullptr) {
    const char* v_caller = _A40__callerOrUnknown(p_callerFunc);

    if (!p_dst || p_dstSize == 0) {
        CL_D10_Logger::log(EN_L10_LOG_WARN, "[A40][%s] Json_copyStrReq: invalid dst/size", v_caller);
        return false;
    }

    memset(p_dst, 0, p_dstSize);

    if (!p_key || !p_key[0]) {
        CL_D10_Logger::log(EN_L10_LOG_ERROR, "[A40][%s] Json_copyStrReq: invalid key", v_caller);
        A40_ComFunc::copyStr2Buffer_safe(p_dst, (p_defaultVal ? p_defaultVal : ""), p_dstSize, v_caller);
        return false;
    }

    if (p_obj.isNull()) {
        CL_D10_Logger::log(EN_L10_LOG_WARN, "[A40][%s] Json_copyStrReq: obj is null (key=%s)", v_caller, p_key);
        A40_ComFunc::copyStr2Buffer_safe(p_dst, (p_defaultVal ? p_defaultVal : ""), p_dstSize, v_caller);
        return false;
    }

    JsonVariantConst v = p_obj[p_key];
    if (v.isNull()) {
        CL_D10_Logger::log(EN_L10_LOG_WARN, "[A40][%s] Json_copyStrReq: missing key=%s", v_caller, p_key);
        A40_ComFunc::copyStr2Buffer_safe(p_dst, (p_defaultVal ? p_defaultVal : ""), p_dstSize, v_caller);
        return false;
    }

    if (!v.is<const char*>()) {
        CL_D10_Logger::log(EN_L10_LOG_WARN, "[A40][%s] Json_copyStrReq: type mismatch (key=%s)", v_caller, p_key);
        A40_ComFunc::copyStr2Buffer_safe(p_dst, (p_defaultVal ? p_defaultVal : ""), p_dstSize, v_caller);
        return false;
    }

    const char* v_src = v.as<const char*>();
    if (!v_src || !v_src[0]) {
        CL_D10_Logger::log(EN_L10_LOG_WARN, "[A40][%s] Json_copyStrReq: empty string (key=%s)", v_caller, p_key);
        A40_ComFunc::copyStr2Buffer_safe(p_dst, (p_defaultVal ? p_defaultVal : ""), p_dstSize, v_caller);
        return false;
    }

    A40_ComFunc::copyStr2Buffer_safe(p_dst, v_src, p_dstSize, v_caller);
    return true;
}


// ------------------------------------------------------
// Json_removeKey
//  - doc의 root를 JsonObject로 보고 특정 key를 제거(remove)
//  - key가 없거나 root가 object가 아니면 아무것도 하지 않음
// ------------------------------------------------------
static inline void Json_removeKey(JsonDocument& p_doc, const char* p_key) {
    if (!p_key || p_key[0] == '\0') return;
    JsonObject v_root = p_doc.as<JsonObject>();
    if (v_root.isNull()) return;
    v_root.remove(p_key);
}


// ------------------------------------------------------
// Json_writeAutoOff
//  - autoOff 구조체를 JSON object에 기록
// ------------------------------------------------------
static inline void Json_writeAutoOff(JsonObject p_ao, const ST_A20_AutoOff_t& p_src) {
    p_ao["timer"]["enabled"]   = p_src.timer.enabled;
    p_ao["timer"]["minutes"]   = p_src.timer.minutes;

    p_ao["offTime"]["enabled"] = p_src.offTime.enabled;
    p_ao["offTime"]["time"]    = p_src.offTime.time;

    p_ao["offTemp"]["enabled"] = p_src.offTemp.enabled;
    p_ao["offTemp"]["temp"]    = p_src.offTemp.temp;
}


// ------------------------------------------------------
// Json_writeMotion
//  - motion 구조체를 JSON object(상위 root)에 기록
//  - p_root는 schedule/profile JSON object (js/jp 등)
// ------------------------------------------------------
static inline void Json_writeMotion(JsonObject p_root, const ST_A20_Motion_t& p_m) {
    p_root["motion"]["pir"]["enabled"]       = p_m.pir.enabled;
    p_root["motion"]["pir"]["holdSec"]       = p_m.pir.holdSec;

    p_root["motion"]["ble"]["enabled"]       = p_m.ble.enabled;
    p_root["motion"]["ble"]["rssiThreshold"] = p_m.ble.rssiThreshold;
    p_root["motion"]["ble"]["holdSec"]       = p_m.ble.holdSec;
}




// ======================================================
// Ptr Utility (delete + nullptr, Silent Policy)
// ======================================================

// ------------------------------------------------------
// freeObject
//  - 단일 객체 포인터용 delete 헬퍼
//  - nullptr 체크 후 delete 수행
//  - delete 이후 반드시 nullptr로 초기화
//  - 정상 흐름이므로 로그 출력 없음 (Silent 정책)
//  - 메모리 해제 누락 / 이중 delete 방지 목적
// ------------------------------------------------------
template <typename T>
static inline void freeObject(T*& p_ptr, const char* p_callerFunc = nullptr) {
    (void)p_callerFunc; // Silent 정책: 미사용 (확장 대비 유지)
    if (p_ptr) {
        delete p_ptr;
        p_ptr = nullptr;
    }
}

// ------------------------------------------------------
// Ptr_deleteArrayNull
//  - 배열(new[]) 포인터용 delete 헬퍼
//  - nullptr 체크 후 delete[] 수행
//  - delete[] 이후 반드시 nullptr로 초기화
//  - Silent 정책 (로그 없음)
// ------------------------------------------------------
template <typename T>
static inline void freeArray(T*& p_ptr, const char* p_callerFunc = nullptr) {
    (void)p_callerFunc; // Silent 정책
    if (p_ptr) {
        delete[] p_ptr;
        p_ptr = nullptr;
    }
}



} // namespace A40_ComFunc

// ======================================================
// 2) Mutex Guard (Recursive Mutex, RAII, Lazy Init)
// ======================================================
#ifndef G_A40_MUTEX_TIMEOUT_100
# define G_A40_MUTEX_TIMEOUT_100 pdMS_TO_TICKS(100)
#endif

class CL_A40_MutexGuard_Semaphore {
  public:
    // [A40] Lazy-init recursive mutex 후 acquire(타임아웃 적용)
    explicit CL_A40_MutexGuard_Semaphore(SemaphoreHandle_t& p_mutex, TickType_t p_timeout, const char* p_caller)
        : _mutexPtr(&p_mutex), _caller(_A40__callerOrUnknown(p_caller)) {
        _internalInitAndTake(p_timeout);
    }

    // [A40] 소멸 시 자동 unlock(RAII)
    ~CL_A40_MutexGuard_Semaphore() { unlock(); }

    // [A40] ticks 단위로 mutex 재획득(이미 acquired면 true)
    bool acquireTicks(TickType_t p_timeoutTicks) {
        if (_acquired) return true;
        if (!_mutexPtr || !*_mutexPtr) return false;

        if (xSemaphoreTakeRecursive(*_mutexPtr, p_timeoutTicks) == pdTRUE) {
            _acquired = true;
            return true;
        }

        CL_D10_Logger::log(EN_L10_LOG_WARN, "[A40][%s] Mutex acquire timeout (ticks=%u)", _caller, (unsigned)p_timeoutTicks);
        return false;
    }

    // [A40] ms 단위로 mutex 재획득(기본: 무한대기)
    bool acquireMs(uint32_t p_timeoutMs = UINT32_MAX) {
        if (_acquired || !_mutexPtr || !*_mutexPtr) return _acquired;
        TickType_t v_ticks = (p_timeoutMs == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(p_timeoutMs);

        if (xSemaphoreTakeRecursive(*_mutexPtr, v_ticks) == pdTRUE) {
            _acquired = true;
            return true;
        }

        CL_D10_Logger::log(EN_L10_LOG_WARN, "[A40][%s] Mutex acquire timeout", _caller);
        return false;
    }

    // [A40] unlock(미획득 상태면 no-op)
    void unlock() {
        if (!_acquired || !_mutexPtr || !*_mutexPtr) return;
        if (xSemaphoreGiveRecursive(*_mutexPtr) == pdTRUE) {
            _acquired = false;
        } else {
            CL_D10_Logger::log(EN_L10_LOG_ERROR, "[A40][%s] Mutex unlock failed", _caller);
        }
    }

    // [A40] 현재 lock을 잡고 있는지 여부
    bool isAcquired() const { return _acquired; }

  private:
    // [A40] mutex가 null이면 생성 후 take까지 처리
    void _internalInitAndTake(TickType_t p_timeout) {
        if (!_mutexPtr) return;

        if (*_mutexPtr == nullptr) {
            static portMUX_TYPE s_initMux = portMUX_INITIALIZER_UNLOCKED;
            portENTER_CRITICAL(&s_initMux);
            if (*_mutexPtr == nullptr) {
                *_mutexPtr = xSemaphoreCreateRecursiveMutex();
                if (*_mutexPtr == nullptr) {
                    CL_D10_Logger::log(EN_L10_LOG_ERROR, "[A40][%s] CreateRecursiveMutex failed", _caller);
                }
            }
            portEXIT_CRITICAL(&s_initMux);
        }

        if (*_mutexPtr && xSemaphoreTakeRecursive(*_mutexPtr, p_timeout) == pdTRUE) {
            _acquired = true;
        }
    }

    SemaphoreHandle_t* _mutexPtr = nullptr;
    const char*        _caller   = "?";
    bool               _acquired = false;
};

// ======================================================
// 3) Critical Section Guard
// ======================================================
class CL_A40_muxGuard_Critical {
  public:
    // [A40] portMUX critical section enter/exit RAII
    explicit CL_A40_muxGuard_Critical(portMUX_TYPE* p_flagSpinlock) : _flagSpinlock(p_flagSpinlock) {
        if (_flagSpinlock) portENTER_CRITICAL(_flagSpinlock);
    }
    ~CL_A40_muxGuard_Critical() {
        if (_flagSpinlock) portEXIT_CRITICAL(_flagSpinlock);
    }

  private:
    portMUX_TYPE* _flagSpinlock = nullptr;
};

// ======================================================
// 4) Dirty Flag Atomic Helper (mux 인자 주입)
// ======================================================
namespace A40_ComFunc {

// [A40] mux 보호 하에 dirty flag set
static inline void Dirty_setAtomic(bool& p_flag, portMUX_TYPE& p_flagSpinlock) {
    CL_A40_muxGuard_Critical g(&p_flagSpinlock);
    p_flag = true;
}

// [A40] mux 보호 하에 dirty flag clear
static inline void Dirty_clearAtomic(bool& p_flag, portMUX_TYPE& p_flagSpinlock) {
    CL_A40_muxGuard_Critical g(&p_flagSpinlock);
    p_flag = false;
}

// [A40] mux 보호 하에 dirty flag read
static inline bool Dirty_readAtomic(const bool& p_flag, portMUX_TYPE& p_flagSpinlock) {
    CL_A40_muxGuard_Critical g(&p_flagSpinlock);
    return p_flag;
}

} // namespace A40_ComFunc

// ======================================================
// 5) IO Utility (LittleFS + JSON + .bak Recovery)
// ======================================================
namespace A40_IO {

// [IO] path + suffix를 dst에 생성(오버플로 방지)
static inline bool _buildPathWithSuffix(
    char* p_dst, size_t p_dstSize, const char* p_path, const char* p_suffix, const char* p_caller) {
    const char* v = _A40__callerOrUnknown(p_caller);

    if (!p_dst || p_dstSize == 0 || !p_path || !p_path[0]) {
        CL_D10_Logger::log(EN_L10_LOG_ERROR, "[IO][%s] invalid path buffer", v);
        return false;
    }

    int n = snprintf(p_dst, p_dstSize, "%s%s", p_path, p_suffix ? p_suffix : "");
    if (n < 0 || (size_t)n >= p_dstSize) {
        CL_D10_Logger::log(EN_L10_LOG_ERROR, "[IO][%s] path overflow: %s", v, p_path);
        return false;
    }
    return true;
}

// [IO] JSON 파일을 doc으로 로드/파싱
static inline bool _parseJsonFileToDoc(const char* p_path, JsonDocument& p_doc, bool p_isBackup, const char* p_caller) {
    const char* v = _A40__callerOrUnknown(p_caller);
    File        f = LittleFS.open(p_path, "r");
    if (!f) {
        CL_D10_Logger::log(EN_L10_LOG_ERROR, "[IO][%s] open failed: %s", v, p_path);
        return false;
    }

    DeserializationError err = deserializeJson(p_doc, f);
    f.close();

    if (err) {
        CL_D10_Logger::log(EN_L10_LOG_ERROR, "[IO][%s] %s parse error: %s (%s)", v, p_isBackup ? "bak" : "main", err.c_str(), p_path);
        return false;
    }
    return true;
}

// [IO] 파일 -> JsonDocument 로드(메인 실패 시 .bak 복구 옵션)
inline bool Load_File2JsonDoc_V21(const char* p_path, JsonDocument& p_doc, bool p_useBackup, const char* p_caller = nullptr) {
    const char* v_caller = _A40__callerOrUnknown(p_caller);

    if (!p_path || !p_path[0]) {
        CL_D10_Logger::log(EN_L10_LOG_ERROR, "[IO][%s] invalid path", v_caller);
        return false;
    }

    char bak[A20_Const::LEN_PATH + 8];
    if (!_buildPathWithSuffix(bak, sizeof(bak), p_path, ".bak", v_caller)) return false;

    if (!LittleFS.exists(p_path)) {
        if (p_useBackup && LittleFS.exists(bak)) {
            p_doc.clear();
            if (_parseJsonFileToDoc(bak, p_doc, true, v_caller)) {
                if (LittleFS.rename(bak, p_path)) {
                    CL_D10_Logger::log(EN_L10_LOG_WARN, "[IO][%s] restored from bak: %s", v_caller, p_path);
                } else {
                    CL_D10_Logger::log(EN_L10_LOG_ERROR, "[IO][%s] restore rename failed: %s -> %s", v_caller, bak, p_path);
                }
                return true;
            }
        }
        CL_D10_Logger::log(EN_L10_LOG_INFO, "[IO][%s] file not found: %s", v_caller, p_path);
        return false;
    }

    p_doc.clear();
    if (_parseJsonFileToDoc(p_path, p_doc, false, v_caller)) {
        return true;
    }

    if (p_useBackup && LittleFS.exists(bak)) {
        p_doc.clear();
        if (_parseJsonFileToDoc(bak, p_doc, true, v_caller)) {
            if (!LittleFS.remove(p_path)) {
                CL_D10_Logger::log(EN_L10_LOG_WARN, "[IO][%s] main remove failed (continue): %s", v_caller, p_path);
            }
            if (LittleFS.rename(bak, p_path)) {
                CL_D10_Logger::log(EN_L10_LOG_WARN, "[IO][%s] recovered from bak: %s", v_caller, p_path);
            } else {
                CL_D10_Logger::log(EN_L10_LOG_ERROR, "[IO][%s] recover rename failed: %s -> %s", v_caller, bak, p_path);
            }
            return true;
        }
    }

    CL_D10_Logger::log(EN_L10_LOG_ERROR, "[IO][%s] load failed: %s", v_caller, p_path);
    return false;
}

// [IO] JsonDocument -> 파일 저장(.tmp atomic + .bak 옵션)
inline bool Save_JsonDoc2File_V21(const char*         p_path,
                                  const JsonDocument& p_doc,
                                  bool                p_useBackup,
                                  bool                p_pretty = true,
                                  const char*         p_caller = nullptr) {
    const char* v_caller = _A40__callerOrUnknown(p_caller);

    char bak[A20_Const::LEN_PATH + 8];
    char tmp[A20_Const::LEN_PATH + 8];
    if (!_buildPathWithSuffix(bak, sizeof(bak), p_path, ".bak", v_caller)) return false;
    if (!_buildPathWithSuffix(tmp, sizeof(tmp), p_path, ".tmp", v_caller)) return false;

    if (LittleFS.exists(tmp)) LittleFS.remove(tmp);

    File f = LittleFS.open(tmp, "w");
    if (!f) {
        CL_D10_Logger::log(EN_L10_LOG_ERROR, "[IO][%s] tmp open failed: %s", v_caller, tmp);
        return false;
    }

    size_t bytes = p_pretty ? serializeJsonPretty(p_doc, f) : serializeJson(p_doc, f);
    f.close();

    if (bytes == 0) {
        CL_D10_Logger::log(EN_L10_LOG_ERROR, "[IO][%s] write failed: %s", v_caller, tmp);
        LittleFS.remove(tmp);
        return false;
    }

    if (LittleFS.exists(p_path) && p_useBackup) {
        LittleFS.remove(bak);
        LittleFS.rename(p_path, bak);
    } else if (LittleFS.exists(p_path)) {
        LittleFS.remove(p_path);
    }

    if (!LittleFS.rename(tmp, p_path)) {
        CL_D10_Logger::log(EN_L10_LOG_ERROR, "[IO][%s] rename failed: %s", v_caller, p_path);
        if (p_useBackup && LittleFS.exists(bak)) {
            LittleFS.rename(bak, p_path);
        }
        return false;
    }

    return true;
}

} // namespace A40_IO



// =============================================================================
// [A40] 내부 Helper: "HH:MM" 문자열을 분(Minutes) 단위로 변환
// -----------------------------------------------------------------------------
// * 기능 개요:
//   - "HH:MM" 형식의 문자열을 파싱하여 0~1440 범위의 uint16_t 값을 반환합니다.
//   - 앞뒤 공백(Space, Tab, CR, LF)을 자동으로 제거(Trim)합니다.
// * 특이 사항 및 제약 조건:
//   - 24시간 표기법 지원: "24:00" 입력 시 1440분으로 처리하여 하루의 끝을 허용합니다.
//   - 엄격한 포맷: 반드시 콜론(:)이 포함되어야 하며, "HHMM" 형식은 오류(0)로 처리합니다.
//   - 안전 장치: 분(MM)이 2자리를 초과하거나, 24:01 이상의 잘못된 시간은 방어적으로 처리합니다.
// * 의존성: 타 모듈(CT10 등)과의 의존성을 배제하기 위해 로컬 static 함수로 유지합니다.
// =============================================================================
static uint16_t A40_parseHHMMtoMin_24h(const char* p_time) {
	if (!p_time || p_time[0] == '\0') return 0;

	// 1. 앞 공백 스킵
	while (*p_time == ' ' || *p_time == '\t' || *p_time == '\r' || *p_time == '\n') p_time++;
	if (p_time[0] == '\0') return 0;

	// 2. 콜론 위치 확인 (HH:MM 포맷 검증)
	const char* v_colon = strchr(p_time, ':');
	if (!v_colon) return 0;

	// 3. 시간(HH) 파싱
	char v_hhBuf[4];
	memset(v_hhBuf, 0, sizeof(v_hhBuf));
	size_t v_hhLen = (size_t)(v_colon - p_time);
	if (v_hhLen == 0 || v_hhLen >= sizeof(v_hhBuf)) return 0;
	memcpy(v_hhBuf, p_time, v_hhLen);

	// 4. 분(MM) 파싱 및 뒤 공백 제거
	const char* v_mmStr = v_colon + 1;
	if (!v_mmStr || v_mmStr[0] == '\0') return 0;

	char v_mmBuf[4];
	memset(v_mmBuf, 0, sizeof(v_mmBuf));

	size_t v_mmLen = strlen(v_mmStr);
	while (v_mmLen > 0) {
		char c = v_mmStr[v_mmLen - 1];
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n') v_mmLen--;
		else break;
	}
	// 분은 최대 2자리(00~59)만 허용하는 엄격한 규칙 적용
	if (v_mmLen == 0 || v_mmLen > 2) return 0;
	memcpy(v_mmBuf, v_mmStr, v_mmLen);

	int v_hh = atoi(v_hhBuf);
	int v_mm = atoi(v_mmBuf);

	// 5. 수치 범위 방어 및 특수 케이스 처리
	if (v_hh < 0) v_hh = 0;
	if (v_mm < 0) v_mm = 0;

	// [특수 처리] 24:00 허용 (종료 시간 설정 등에 사용)
	if (v_hh == 24) {
		if (v_mm == 0) return 1440;
		return 0; // 24:01 등은 무효 처리
	}

	// [일반 처리] 범위 초과 시 Clamp 처리
	if (v_hh > 23) v_hh = 23;
	if (v_mm > 59) v_mm = 59;

	return (uint16_t)(v_hh * 60 + v_mm);
}
