// =======================================================
// File: W10_Def_0314.h
// =======================================================
#pragma once

/*
 * ------------------------------------------------------
 * 소스명 : W10_Def_0314.h
 * 모듈약어 : W10
 * 모듈명 : WebConfig Definitions (Split Support, Static Helpers, Tables)
 * ------------------------------------------------------
 * 기능 요약
 *  - (0314) W10_WebConfig_0314 분할 구조에 맞춘 공용 정의/헬퍼
 *  - 정적서빙 보안 유틸(경로 안전성/확장자 화이트리스트)
 *  - Content-Type / Cache-Control 분류
 *  - Body Slot / Diag Event / Reboot Mask / Keycode Tables
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
 *   - 클래스 private 멤버 함수/변수   : _ 접두사
 *   - 클래스 멤버(함수/변수) : 모듈약어 접두사 미사용
 *   - 클래스 정적 멤버      : s_ 접두사
 *   - 함수 로컬 변수        : v_ 접두사
 *   - 함수 인자             : p_ 접두사
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <string.h>
#include <strings.h>

// E10 status/type interface (must match your E10_Def_0310.h)
#include "E10_Def_0310.h"

// =======================================================
// Version / API
// =======================================================
static const uint16_t G_W10_API_VER = 314;

// =======================================================
// Static Routes / Prefix
// =======================================================
static const char* G_W10_URI_WWW_PREFIX          = "/www/";
static const char* G_W10_URI_JSON_PUBLIC_PREFIX  = "/json/public/";
static const char* G_W10_DEFAULT_INDEX_PATH      = "/www/index.html";

// =======================================================
// Cache-Control presets
// =======================================================
static const char* G_W10_CACHE_NOSTORE   = "no-store";
static const char* G_W10_CACHE_NOCACHE   = "no-cache";
static const char* G_W10_CACHE_IMMUTABLE = "public, max-age=31536000, immutable";
static const char* G_W10_CACHE_SHORT     = "public, max-age=3600";

// =======================================================
// Reboot reason masks (policy)
// =======================================================
static const uint32_t G_W10_REBOOT_WIFI_MODE  = 0x00000001UL;
static const uint32_t G_W10_REBOOT_WIFI_STA   = 0x00000002UL;
static const uint32_t G_W10_REBOOT_WIFI_AP    = 0x00000004UL;
static const uint32_t G_W10_REBOOT_WIFI_MDNS  = 0x00000008UL;
static const uint32_t G_W10_REBOOT_OTHER      = 0x80000000UL;

// =======================================================
// Body Slot (POST collector)
// =======================================================
static const uint8_t  G_W10_BODY_SLOTS          = 3;          // 동시 POST 수용 슬롯
static const uint32_t G_W10_BODY_MAX            = 8192;       // 최대 바디(문자열) 크기
static const uint32_t G_W10_BODY_SLOT_STALE_MS  = 15000;      // 오래된 슬롯 강제 회수 기준(ms)

struct ST_W10_BodySlot {
    AsyncWebServerRequest* req;   // owner request
    size_t len;                   // bytes written
    uint32_t lastMs;              // last update time
    char buf[G_W10_BODY_MAX + 1]; // fixed buffer (+null)
};

// =======================================================
// Diagnostics event ring
// =======================================================
static const uint8_t G_W10_DIAG_EVT_MAX = 16;

struct ST_W10_DiagEvt_t {
    uint32_t ms;
    char code[24];
};

// =======================================================
// Keycode tables
// =======================================================
struct ST_W10_ModRow_t {
    const char* name;
    uint8_t mask;
};

struct ST_W10_ConsumerRow_t {
    const char* name;
    uint32_t mask;
};

// HID modifier masks (USB HID keyboard modifier byte)
static const ST_W10_ModRow_t G_W10_MODS[] = {
    { "LCTRL",  0x01 }, { "LSHIFT", 0x02 }, { "LALT",   0x04 }, { "LGUI",  0x08 },
    { "RCTRL",  0x10 }, { "RSHIFT", 0x20 }, { "RALT",   0x40 }, { "RGUI",  0x80 },
};

// Consumer page masks (common subset)
static const ST_W10_ConsumerRow_t G_W10_CONSUMER[] = {
    { "MUTE",        0x0001 },
    { "VOL_UP",      0x0002 },
    { "VOL_DOWN",    0x0004 },
    { "PLAY_PAUSE",  0x0008 },
    { "SCAN_NEXT",   0x0010 },
    { "SCAN_PREV",   0x0020 },
    { "STOP",        0x0040 },
    { "WWW_HOME",    0x0080 },
};

// =======================================================
// E10 Interface (W10 -> E10)
// - ctx + function pointers
// - must match W10_WebConfig_0314 usage
// =======================================================
struct ST_W10_E10If_t {
    void* ctx;

    // status snapshot
    bool (*getStatus)(void* ctx, ST_E10_Status_t* out);

    // runtime apply (E10 config only)
    bool (*applyRuntimeE10)(void* ctx, const ST_C10_E10Config_t* e10cfg);

    // controls
    bool (*setPptMode)(void* ctx, bool en);
    bool (*setDpiLevel)(void* ctx, uint8_t level);
    bool (*setPrecisionMode)(void* ctx, uint8_t mode);

    bool (*forceReleaseButtons)(void* ctx);
    bool (*requestGyroCalibration)(void* ctx);
    bool (*requestI2CRecover)(void* ctx);
    bool (*clearDiagnostics)(void* ctx);

    bool (*setSafeMode)(void* ctx, bool en);
    bool (*setOtaGuard)(void* ctx, bool en);

    // ppt test
    bool (*testPptKey2)(void* ctx, uint8_t page, uint8_t mod, uint32_t code);
};

// =======================================================
// Helper: safe path (reject traversal / invalid)
// - allow only '/', '.', '-', '_', digits/letters and minimal symbols
// - block: "..", backslash, control chars, "//", "/./", "/../"
// =======================================================
static inline bool W10_isPathSafe(const char* p_path) {
    if (!p_path) return false;

    // must start with '/'
    if (p_path[0] != '/') return false;

    // control chars and backslash block
    for (const char* c = p_path; *c; c++) {
        const uint8_t ch = (uint8_t)(*c);
        if (ch < 0x20 || ch == 0x7F) return false;
        if (ch == '\\') return false;
    }

    // traversal block
    if (strstr(p_path, "..") != nullptr) return false;

    // normalize-ish block: double slash, /./, /../
    if (strstr(p_path, "//") != nullptr) return false;
    if (strstr(p_path, "/./") != nullptr) return false;
    if (strstr(p_path, "/../") != nullptr) return false;

    // reject query or fragment in filesystem path
    if (strchr(p_path, '?') != nullptr) return false;
    if (strchr(p_path, '#') != nullptr) return false;

    return true;
}

// =======================================================
// Helper: lower extension extractor
// - returns pointer to v_outLowerExt buffer (or nullptr if none)
// =======================================================
static inline const char* W10_getLowerExt(const char* p_path, char* p_outLowerExt, size_t p_outSz) {
    if (!p_path || !p_outLowerExt || p_outSz == 0) return nullptr;

    const char* dot = strrchr(p_path, '.');
    if (!dot || dot == p_path) return nullptr;

    dot++; // skip '.'
    if (*dot == '\0') return nullptr;

    size_t n = strlcpy(p_outLowerExt, dot, p_outSz);
    if (n == 0 || n >= p_outSz) return nullptr;

    for (size_t i = 0; p_outLowerExt[i]; i++) {
        char ch = p_outLowerExt[i];
        if (ch >= 'A' && ch <= 'Z') p_outLowerExt[i] = (char)(ch - 'A' + 'a');
    }
    return p_outLowerExt;
}

// =======================================================
// Allowed extensions
// - /www/*
// =======================================================
static inline bool W10_isAllowedWwwExt(const char* p_extLower) {
    if (!p_extLower) return false;

    // web assets
    if (strcmp(p_extLower, "html") == 0) return true;
    if (strcmp(p_extLower, "css")  == 0) return true;
    if (strcmp(p_extLower, "js")   == 0) return true;

    // images / icons
    if (strcmp(p_extLower, "svg")  == 0) return true;
    if (strcmp(p_extLower, "png")  == 0) return true;
    if (strcmp(p_extLower, "webp") == 0) return true;
    if (strcmp(p_extLower, "ico")  == 0) return true;

    // misc
    if (strcmp(p_extLower, "txt")  == 0) return true;
    if (strcmp(p_extLower, "map")  == 0) return true;

    return false;
}

// =======================================================
// Allowed extensions
// - /json/public/*
// =======================================================
static inline bool W10_isAllowedPublicJsonExt(const char* p_extLower) {
    if (!p_extLower) return false;
    return (strcmp(p_extLower, "json") == 0);
}

// =======================================================
// gzip target extensions (only these can be served as .gz)
// =======================================================
static inline bool W10_isGzipTargetExt(const char* p_extLower) {
    if (!p_extLower) return false;
    if (strcmp(p_extLower, "html") == 0) return true;
    if (strcmp(p_extLower, "css")  == 0) return true;
    if (strcmp(p_extLower, "js")   == 0) return true;
    return false;
}

// =======================================================
// Content-Type by extension
// =======================================================
static inline const char* W10_contentTypeFromExt(const char* p_extLower) {
    if (!p_extLower) return "application/octet-stream";

    if (strcmp(p_extLower, "html") == 0) return "text/html";
    if (strcmp(p_extLower, "css")  == 0) return "text/css";
    if (strcmp(p_extLower, "js")   == 0) return "application/javascript";

    if (strcmp(p_extLower, "svg")  == 0) return "image/svg+xml";
    if (strcmp(p_extLower, "png")  == 0) return "image/png";
    if (strcmp(p_extLower, "webp") == 0) return "image/webp";
    if (strcmp(p_extLower, "ico")  == 0) return "image/x-icon";

    if (strcmp(p_extLower, "txt")  == 0) return "text/plain";
    if (strcmp(p_extLower, "map")  == 0) return "application/json";
    if (strcmp(p_extLower, "json") == 0) return "application/json";

    return "application/octet-stream";
}

// =======================================================
// Version token detection (immutable heuristic)
// - ex) app.8f3a1c2d.js , main-v20260216.css , vendor.v0123.js
// =======================================================
static inline bool W10_hasVersionToken(const char* p_path) {
    if (!p_path) return false;

    // quick checks for common patterns
    if (strstr(p_path, "-v") != nullptr) return true;
    if (strstr(p_path, ".v") != nullptr) return true;

    // detect ".<8 hex>." pattern (simple)
    const char* s = p_path;
    while ((s = strchr(s, '.')) != nullptr) {
        s++; // after '.'
        int hexCount = 0;
        const char* t = s;
        while (*t) {
            const char c = *t;
            const bool isHex = ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
            if (!isHex) break;
            hexCount++;
            t++;
            if (hexCount > 12) break;
        }
        if (hexCount >= 8 && *t == '.') return true;
    }
    return false;
}

// =======================================================
// Cache-Control classifier for static
// - html : no-store
// - json (public) : no-store
// - versioned assets : immutable
// - else : short
// =======================================================
static inline const char* W10_cacheControlForStatic(const char* p_path, const char* p_extLower, bool p_isPublicJson) {
    (void)p_isPublicJson;

    if (!p_extLower) return G_W10_CACHE_SHORT;

    // html / json: no-store (policy)
    if (strcmp(p_extLower, "html") == 0) return G_W10_CACHE_NOSTORE;
    if (strcmp(p_extLower, "json") == 0) return G_W10_CACHE_NOSTORE;

    // for /www assets: if versioned => immutable for common static types
    if (p_path && W10_hasVersionToken(p_path)) {
        if (strcmp(p_extLower, "css")  == 0) return G_W10_CACHE_IMMUTABLE;
        if (strcmp(p_extLower, "js")   == 0) return G_W10_CACHE_IMMUTABLE;
        if (strcmp(p_extLower, "svg")  == 0) return G_W10_CACHE_IMMUTABLE;
        if (strcmp(p_extLower, "png")  == 0) return G_W10_CACHE_IMMUTABLE;
        if (strcmp(p_extLower, "webp") == 0) return G_W10_CACHE_IMMUTABLE;
        if (strcmp(p_extLower, "ico")  == 0) return G_W10_CACHE_IMMUTABLE;
    }

    return G_W10_CACHE_SHORT;
}
