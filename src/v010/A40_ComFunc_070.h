#pragma once

/*
 * ------------------------------------------------------
 * 소스명 : A40_ComFunc_070.h
 * 모듈약어 : A40
 * 모듈명 : Common Utilities (AirMouse Full, JSON/FS/Mutex)
 * ------------------------------------------------------
 * 기능 요약
 *  - 문자열/클램프/유틸 (strlcpy 기반 안전 복사)
 *  - ArduinoJson v7 Helper (containsKey 금지 정책 준수)
 *  - Mutex/critical RAII Guard (FreeRTOS + portMUX)
 *  - LittleFS IO: Atomic 저장(.tmp) + .bak 백업 + 복구(로드 실패 시)
 *  - AirMouse(C10/W10/E10)에서 공용으로 사용 가능한 “브릭 방지” IO 유틸 제공
 * ------------------------------------------------------
 * [구현 규칙]
 *  - 항상 소스 시작 주석 부분 체계 유지 및 내용 업데이트
 *  - 소스 시작 주석 부분 구현규칙, 코드네이밍규칙 내용 그대로 유지, 수정금지
 *  - ArduinoJson v7.x.x 사용 (v6 이하 사용 금지)
 *  - JsonDocument 단일 타입만 사용
 *  - createNestedArray/Object/containsKey 사용 금지
 *  - memset + strlcpy 기반 안전 초기화
 *  - 주석/필드명은 JSON 구조와 동일하게 유지
 *  - 변수명은 가능한 해석 가능하게
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 *   - namespace 명        : 모듈약어_ 접두사
 *   - namespace 내 상수    : 모둘약어 접두시 미사용
 *   - 전역 상수,매크로      : G_모듈약어_ 접두사
 *   - 전역 변수             : g_모듈약어_ 접두사
 *   - 전역 함수             : 모듈약어_ 접두사
 *   - type                  : T_모듈약어_ 접두사
 *   - typedef               : _t  접미사
 *   - enum 상수             : EN_모듈약어_ 접두사
 *   - 구조체                : ST_모듈약어_ 접두사
 *   - 클래스명              : CL_모듈약어_ 접두사 , 버전 제거
 *   - 클래스 private 멤버   : _ 접두사
 *   - 클래스 멤버(함수/변수) : 모듈약어 접두사 미사용
 *   - 클래스 정적 멤버      : s_ 접두사
 *   - 함수 로컬 변수        : v_ 접두사
 *   - 함수 인자             : p_ 접두사
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <string.h>
#include <strings.h>

#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <memory>
#include <new>

#include "D10_Logger_061.h"

// ------------------------------------------------------
// 기본 정책/상수
// ------------------------------------------------------
#ifndef G_A40_LEN_PATH
#define G_A40_LEN_PATH  192
#endif

#ifndef G_A40_MUTEX_TIMEOUT_100
#define G_A40_MUTEX_TIMEOUT_100 pdMS_TO_TICKS(100)
#endif

#ifndef G_A40_IO_COPY_CHUNK
#define G_A40_IO_COPY_CHUNK 512
#endif

// [A40] caller 문자열이 비정상이면 "?"로 대체
static inline const char* _A40__callerOrUnknown(const char* p_callerFunc) {
    return (p_callerFunc && p_callerFunc[0]) ? p_callerFunc : "?";
}

// ======================================================
// 1) 공용 유틸 / JSON Helper
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
        D10_LOGW_C(v_caller, "[A40] copyStr2Buffer_safe: invalid dst/size");
        return 0;
    }

    if (!p_src) {
        p_dst[0] = '\0';
        D10_LOGW_C(v_caller, "[A40] copyStr2Buffer_safe: src is null");
        return 0;
    }

    return strlcpy(p_dst, p_src, p_n);
}

// [A40] C-string을 shared_ptr<char[]>로 복제(할당 실패/NULL 방어)
inline std::shared_ptr<char[]> cloneStr2SharedStr_safe(const char* p_src, const char* p_callerFunc = nullptr) {
    const char* v_caller = _A40__callerOrUnknown(p_callerFunc);

    if (!p_src) {
        D10_LOGW_C(v_caller, "[A40] cloneStr2SharedStr_safe: src is null");
        return nullptr;
    }

    const size_t v_len = strlen(p_src) + 1;
    std::shared_ptr<char[]> v_buf(new (std::nothrow) char[v_len], std::default_delete<char[]>());

    if (!v_buf) {
        D10_LOGE_C(v_caller, "[A40] cloneStr2SharedStr_safe: alloc failed (%u bytes)", (unsigned)v_len);
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
    if (v.isNull()) return JsonArrayConst();
    JsonArrayConst a = v.as<JsonArrayConst>();
    return a.isNull() ? JsonArrayConst() : a;
}

// [A40] doc에서 wrapKey가 있으면 그 오브젝트, 없으면 doc root를 반환
// - doc operator[]는 key를 만들 수 있으므로, "root object"에서만 조회
static inline JsonObjectConst Json_pickRootObject(const JsonDocument& p_doc, const char* p_wrapKey) {
    JsonObjectConst root = p_doc.as<JsonObjectConst>();
    if (root.isNull() || !p_wrapKey || !p_wrapKey[0]) return root;

    JsonVariantConst w = root[p_wrapKey];
    JsonObjectConst wo = w.as<JsonObjectConst>();
    return wo.isNull() ? root : wo;
}

// [A40] JSON 문자열을 dst에 복사(키 없음/불일치 => default로 채움, dst는 항상 memset(0))
static inline bool Json_copyStr(JsonObjectConst p_obj,
                               const char*     p_key,
                               char*           p_dst,
                               size_t          p_dstSize,
                               const char*     p_defaultVal = "",
                               const char*     p_callerFunc = nullptr) {
    const char* v_caller = _A40__callerOrUnknown(p_callerFunc);

    if (!p_dst || p_dstSize == 0) {
        D10_LOGW_C(v_caller, "[A40] Json_copyStr: invalid dst/size");
        return false;
    }
    memset(p_dst, 0, p_dstSize);

    if (!p_key || !p_key[0]) {
        D10_LOGW_C(v_caller, "[A40] Json_copyStr: invalid key");
        const char* v_def = (p_defaultVal ? p_defaultVal : "");
        strlcpy(p_dst, v_def, p_dstSize);
        return false;
    }

    const char* v_src = A40_ComFunc::Json_getStr(p_obj, p_key, p_defaultVal ? p_defaultVal : "");
    strlcpy(p_dst, (v_src ? v_src : ""), p_dstSize);
    return true;
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
        D10_LOGW_C(v_caller, "[A40] Json_copyStrReq: invalid dst/size");
        return false;
    }
    memset(p_dst, 0, p_dstSize);

    if (!p_key || !p_key[0]) {
        D10_LOGE_C(v_caller, "[A40] Json_copyStrReq: invalid key");
        A40_ComFunc::copyStr2Buffer_safe(p_dst, (p_defaultVal ? p_defaultVal : ""), p_dstSize, v_caller);
        return false;
    }

    if (p_obj.isNull()) {
        D10_LOGW_C(v_caller, "[A40] Json_copyStrReq: obj is null (key=%s)", p_key);
        A40_ComFunc::copyStr2Buffer_safe(p_dst, (p_defaultVal ? p_defaultVal : ""), p_dstSize, v_caller);
        return false;
    }

    JsonVariantConst v = p_obj[p_key];
    if (v.isNull()) {
        D10_LOGW_C(v_caller, "[A40] Json_copyStrReq: missing key=%s", p_key);
        A40_ComFunc::copyStr2Buffer_safe(p_dst, (p_defaultVal ? p_defaultVal : ""), p_dstSize, v_caller);
        return false;
    }
    if (!v.is<const char*>()) {
        D10_LOGW_C(v_caller, "[A40] Json_copyStrReq: type mismatch (key=%s)", p_key);
        A40_ComFunc::copyStr2Buffer_safe(p_dst, (p_defaultVal ? p_defaultVal : ""), p_dstSize, v_caller);
        return false;
    }

    const char* v_src = v.as<const char*>();
    if (!v_src || !v_src[0]) {
        D10_LOGW_C(v_caller, "[A40] Json_copyStrReq: empty string (key=%s)", p_key);
        A40_ComFunc::copyStr2Buffer_safe(p_dst, (p_defaultVal ? p_defaultVal : ""), p_dstSize, v_caller);
        return false;
    }

    A40_ComFunc::copyStr2Buffer_safe(p_dst, v_src, p_dstSize, v_caller);
    return true;
}

} // namespace A40_ComFunc

// ======================================================
// 2) Mutex Guard (Recursive Mutex, RAII, Lazy Init)
// ======================================================
class CL_A40_MutexGuard_Semaphore {
  public:
    explicit CL_A40_MutexGuard_Semaphore(SemaphoreHandle_t& p_mutex, TickType_t p_timeout, const char* p_caller)
        : _mutexPtr(&p_mutex), _caller(_A40__callerOrUnknown(p_caller)) {
        _internalInitAndTake(p_timeout);
    }
    ~CL_A40_MutexGuard_Semaphore() { unlock(); }

    bool acquireTicks(TickType_t p_timeoutTicks) {
        if (_acquired) return true;
        if (!_mutexPtr || !*_mutexPtr) return false;

        if (xSemaphoreTakeRecursive(*_mutexPtr, p_timeoutTicks) == pdTRUE) {
            _acquired = true;
            return true;
        }
        D10_LOGW_C(_caller, "[A40] Mutex acquire timeout (ticks=%u)", (unsigned)p_timeoutTicks);
        return false;
    }

    bool acquireMs(uint32_t p_timeoutMs = UINT32_MAX) {
        if (_acquired || !_mutexPtr || !*_mutexPtr) return _acquired;
        TickType_t v_ticks = (p_timeoutMs == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(p_timeoutMs);

        if (xSemaphoreTakeRecursive(*_mutexPtr, v_ticks) == pdTRUE) {
            _acquired = true;
            return true;
        }
        D10_LOGW_C(_caller, "[A40] Mutex acquire timeout");
        return false;
    }

    void unlock() {
        if (!_acquired || !_mutexPtr || !*_mutexPtr) return;
        if (xSemaphoreGiveRecursive(*_mutexPtr) == pdTRUE) {
            _acquired = false;
        } else {
            D10_LOGE_C(_caller, "[A40] Mutex unlock failed");
        }
    }

    bool isAcquired() const { return _acquired; }

  private:
    void _internalInitAndTake(TickType_t p_timeout) {
        if (!_mutexPtr) return;

        if (*_mutexPtr == nullptr) {
            static portMUX_TYPE s_initMux = portMUX_INITIALIZER_UNLOCKED;
            portENTER_CRITICAL(&s_initMux);
            if (*_mutexPtr == nullptr) {
                *_mutexPtr = xSemaphoreCreateRecursiveMutex();
                if (*_mutexPtr == nullptr) {
                    D10_LOGE_C(_caller, "[A40] CreateRecursiveMutex failed");
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
static inline void Dirty_setAtomic(bool& p_flag, portMUX_TYPE& p_flagSpinlock) {
    CL_A40_muxGuard_Critical g(&p_flagSpinlock);
    p_flag = true;
}
static inline void Dirty_clearAtomic(bool& p_flag, portMUX_TYPE& p_flagSpinlock) {
    CL_A40_muxGuard_Critical g(&p_flagSpinlock);
    p_flag = false;
}
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
        D10_LOGE_C(v, "[A40][IO] invalid path buffer");
        return false;
    }

    int n = snprintf(p_dst, p_dstSize, "%s%s", p_path, p_suffix ? p_suffix : "");
    if (n < 0 || (size_t)n >= p_dstSize) {
        D10_LOGE_C(v, "[A40][IO] path overflow: %s", p_path);
        return false;
    }
    return true;
}

// [IO] 파일 복사(rename 실패 fallback용)
static inline bool Copy_File_V22(const char* p_src, const char* p_dst, const char* p_caller = nullptr) {
    const char* v = _A40__callerOrUnknown(p_caller);
    if (!p_src || !p_src[0] || !p_dst || !p_dst[0]) return false;

    File fs = LittleFS.open(p_src, "r");
    if (!fs) { D10_LOGW_C(v, "[A40][IO] copy open src fail: %s", p_src); return false; }

    File fd = LittleFS.open(p_dst, "w");
    if (!fd) { fs.close(); D10_LOGW_C(v, "[A40][IO] copy open dst fail: %s", p_dst); return false; }

    uint8_t buf[G_A40_IO_COPY_CHUNK];
    while (true) {
        int r = fs.read(buf, sizeof(buf));
        if (r <= 0) break;

        int w = fd.write(buf, (size_t)r);
        if (w != r) {
            fd.close(); fs.close();
            D10_LOGE_C(v, "[A40][IO] copy write fail: %s", p_dst);
            return false;
        }
        // 긴 파일 복사 시 WDT/스케줄링 배려
        delay(0);
    }

    fd.flush();
    fd.close();
    fs.close();
    return true;
}

// [IO] JSON 파일을 doc으로 로드/파싱
static inline bool _parseJsonFileToDoc(const char* p_path, JsonDocument& p_doc, bool p_isBackup, const char* p_caller) {
    const char* v = _A40__callerOrUnknown(p_caller);

    File f = LittleFS.open(p_path, "r");
    if (!f) {
        D10_LOGW_C(v, "[A40][IO] open failed: %s", p_path);
        return false;
    }

    DeserializationError err = deserializeJson(p_doc, f);
    f.close();

    if (err) {
        D10_LOGE_C(v, "[A40][IO] %s parse error: %s (%s)", p_isBackup ? "bak" : "main", err.c_str(), p_path);
        return false;
    }
    return true;
}

// [IO] 파일 재파싱 검증(부분쓰기/손상 방지)
static inline bool _verifyJsonFile(const char* p_path, const char* p_caller) {
    const char* v = _A40__callerOrUnknown(p_caller);
    JsonDocument v_doc; // 단일 doc
    return _parseJsonFileToDoc(p_path, v_doc, false, v);
}

// [IO] 로드(메인 실패 시 .bak 복구 옵션)
// - 복구 시 bak를 가능한 "유지"하도록 copy 우선
inline bool Load_File2JsonDoc_V22(const char* p_path, JsonDocument& p_doc, bool p_useBackup, const char* p_caller = nullptr) {
    const char* v_caller = _A40__callerOrUnknown(p_caller);

    if (!p_path || !p_path[0]) {
        D10_LOGE_C(v_caller, "[A40][IO] invalid path");
        return false;
    }

    char v_bak[G_A40_LEN_PATH + 8];
    if (!_buildPathWithSuffix(v_bak, sizeof(v_bak), p_path, ".bak", v_caller)) return false;

    // main 없음 -> bak로 기동 시도
    if (!LittleFS.exists(p_path)) {
        if (p_useBackup && LittleFS.exists(v_bak)) {
            p_doc.clear();
            if (_parseJsonFileToDoc(v_bak, p_doc, true, v_caller)) {
                // bak를 유지하고 main을 새로 생성(복구 흔적 유지)
                (void)LittleFS.remove(p_path);
                if (!Copy_File_V22(v_bak, p_path, v_caller)) {
                    D10_LOGE_C(v_caller, "[A40][IO] restore copy failed: %s", p_path);
                    return false;
                }
                D10_LOGW_C(v_caller, "[A40][IO] restored main from bak(copy): %s", p_path);
                return true;
            }
        }
        D10_LOGI_C(v_caller, "[A40][IO] file not found: %s", p_path);
        return false;
    }

    // main parse
    p_doc.clear();
    if (_parseJsonFileToDoc(p_path, p_doc, false, v_caller)) return true;

    // main parse 실패 -> bak 복구 시도
    if (p_useBackup && LittleFS.exists(v_bak)) {
        p_doc.clear();
        if (_parseJsonFileToDoc(v_bak, p_doc, true, v_caller)) {
            // main을 교체하되 bak는 유지(현장 롤백 여지 유지)
            (void)LittleFS.remove(p_path);
            if (!Copy_File_V22(v_bak, p_path, v_caller)) {
                D10_LOGE_C(v_caller, "[A40][IO] recover copy failed: %s", p_path);
                return false;
            }
            D10_LOGW_C(v_caller, "[A40][IO] recovered main from bak(copy): %s", p_path);
            return true;
        }
    }

    D10_LOGE_C(v_caller, "[A40][IO] load failed: %s", p_path);
    return false;
}

// [IO] 저장(.tmp atomic + .bak/.old)  (브릭 방지 강화 리팩토링)
//
// 정책 요약:
//  1) tmp write + verify 성공 전까지 main 손대지 않음
//  2) p_useBackup=true  -> main을 .bak로 보존 후 교체(rollback 가능)
//  3) p_useBackup=false -> main을 .old로 임시 보존 후 교체(실패 시 복구), 성공 시 .old 제거
inline bool Save_JsonDoc2File_V22(const char*          p_path,
                                 const JsonDocument&  p_doc,
                                 bool                 p_useBackup,
                                 bool                 p_pretty = true,
                                 const char*          p_caller = nullptr) {
    const char* v_caller = _A40__callerOrUnknown(p_caller);

    if (!p_path || !p_path[0]) {
        D10_LOGE_C(v_caller, "[A40][IO] invalid path");
        return false;
    }

    char v_bak[G_A40_LEN_PATH + 8];
    char v_old[G_A40_LEN_PATH + 8];
    char v_tmp[G_A40_LEN_PATH + 8];
    if (!_buildPathWithSuffix(v_bak, sizeof(v_bak), p_path, ".bak", v_caller)) return false;
    if (!_buildPathWithSuffix(v_old, sizeof(v_old), p_path, ".old", v_caller)) return false;
    if (!_buildPathWithSuffix(v_tmp, sizeof(v_tmp), p_path, ".tmp", v_caller)) return false;

    // cleanup tmp
    if (LittleFS.exists(v_tmp)) (void)LittleFS.remove(v_tmp);

    // 1) tmp write
    File f = LittleFS.open(v_tmp, "w");
    if (!f) {
        D10_LOGE_C(v_caller, "[A40][IO] tmp open failed: %s", v_tmp);
        return false;
    }

    size_t bytes = p_pretty ? serializeJsonPretty(p_doc, f) : serializeJson(p_doc, f);
    f.flush();
    f.close();

    if (bytes == 0) {
        D10_LOGE_C(v_caller, "[A40][IO] write failed: %s", v_tmp);
        (void)LittleFS.remove(v_tmp);
        return false;
    }

    // 2) tmp verify (재파싱)
    if (!_verifyJsonFile(v_tmp, v_caller)) {
        D10_LOGE_C(v_caller, "[A40][IO] tmp verify parse failed: %s", v_tmp);
        (void)LittleFS.remove(v_tmp);
        return false;
    }

    // 3) main 보존(.bak or .old)
    const bool v_hasMain = LittleFS.exists(p_path);

    auto v_restoreFromBackupOrOld = [&](void) {
        if (p_useBackup) {
            if (LittleFS.exists(v_bak)) {
                (void)LittleFS.remove(p_path);
                if (!LittleFS.rename(v_bak, p_path)) (void)Copy_File_V22(v_bak, p_path, v_caller);
                D10_LOGW_C(v_caller, "[A40][IO] rollback from bak: %s", p_path);
            }
        } else {
            if (LittleFS.exists(v_old)) {
                (void)LittleFS.remove(p_path);
                if (!LittleFS.rename(v_old, p_path)) (void)Copy_File_V22(v_old, p_path, v_caller);
                D10_LOGW_C(v_caller, "[A40][IO] rollback from old: %s", p_path);
            }
        }
    };

    if (v_hasMain) {
        if (p_useBackup) {
            // bak는 항상 "가장 최근 이전본"이 되도록 갱신
            (void)LittleFS.remove(v_bak);
            if (!LittleFS.rename(p_path, v_bak)) {
                // rename 실패 -> copy fallback(가능하면 main 유지)
                D10_LOGW_C(v_caller, "[A40][IO] main->bak rename failed, try copy: %s", v_bak);
                if (!Copy_File_V22(p_path, v_bak, v_caller)) {
                    D10_LOGE_C(v_caller, "[A40][IO] main->bak copy failed");
                    (void)LittleFS.remove(v_tmp);
                    return false;
                }
                // copy 성공했으면 교체를 위해 main 제거(rename(tmp->main) 가능하게)
                (void)LittleFS.remove(p_path);
            }
        } else {
            (void)LittleFS.remove(v_old);
            if (!LittleFS.rename(p_path, v_old)) {
                D10_LOGW_C(v_caller, "[A40][IO] main->old rename failed, try copy: %s", v_old);
                if (!Copy_File_V22(p_path, v_old, v_caller)) {
                    D10_LOGE_C(v_caller, "[A40][IO] main->old copy failed");
                    (void)LittleFS.remove(v_tmp);
                    return false;
                }
                (void)LittleFS.remove(p_path);
            }
        }
    }

    // 4) tmp -> main
    if (!LittleFS.rename(v_tmp, p_path)) {
        D10_LOGE_C(v_caller, "[A40][IO] tmp->main rename failed: %s", p_path);
        (void)LittleFS.remove(v_tmp);
        v_restoreFromBackupOrOld();
        return false;
    }

    // 5) 성공 후 정리
    if (!p_useBackup && LittleFS.exists(v_old)) (void)LittleFS.remove(v_old);
    // backup 모드에서는 .bak 유지(현장 롤백을 위해)

    return true;
}

} // namespace A40_IO


