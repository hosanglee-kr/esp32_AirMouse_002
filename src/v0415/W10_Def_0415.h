// File: src/v0415/W10_Def_0415.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : W10_Def_0415.h
 * 모듈약어 : W10
 * 모듈명 : Web Defs (Types/Consts/Keycodes/Cache Rules)
 * ------------------------------------------------------
 * 기능 요약
 *  - W10 공용 타입/상수/고정 문자열/키코드 프리셋
 *  - 정적파일 캐시 정책 자동 분류
 *  - 동적 서빙 허용 확장자 목록
 *  - Content-Type 매핑 + gzip 후보 확장자
 *
 * [v0415 Phase 1-3]
 *  - `G_W10_KB_NAMES[]` 테이블 도입 (KB Usage 이름)
 *    · `_kbName()` switch 200줄 → 이진 탐색 15줄
 *
 * [v0415 Phase 2-2]
 *  - `G_W10_ERR_MAP[]` 테이블 도입 (에러 코드 → HTTP)
 *    · `_httpFromCode` if-else 40줄 → 이진 탐색 15줄
 *    · 반드시 사전순 정렬 유지 (이진 탐색 전제)
 *
 * [v0415 rev15 이전]
 *  - G_W10_API_VER: 410 → 411
 *  - G_W10_DEFAULT_INDEX_PATH: index_0415.html
 *  - setPptMode 콜백 삭제
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <string.h>
#include <strings.h>

class AsyncWebServerRequest;

#include "E10_Def_0415.h"
#include "C10_Config_0415.h"

// -------------------------------------------------------
// URI/Path Prefix
// -------------------------------------------------------
static constexpr const char* G_W10_URI_WWW_PREFIX         = "/www/";
static constexpr const char* G_W10_URI_JSON_PUBLIC_PREFIX = "/json/public/";

static constexpr const char* G_W10_PATH_WWW_PREFIX         = "/www/";
static constexpr const char* G_W10_PATH_JSON_PUBLIC_PREFIX = "/json/public/";

static constexpr const char* G_W10_DEFAULT_INDEX_PATH = "/www/index_0415.html";

// -------------------------------------------------------
// Cache-Control presets
// -------------------------------------------------------
static constexpr const char* G_W10_CACHE_NOSTORE   = "no-store";
static constexpr const char* G_W10_CACHE_IMMUTABLE = "public, max-age=31536000, immutable";
static constexpr const char* G_W10_CACHE_SHORT     = "public, max-age=3600";

static constexpr size_t G_W10_BODY_MAX = 8192;

// -------------------------------------------------------
// Reboot reason bits
// -------------------------------------------------------
static constexpr uint32_t G_W10_REBOOT_WIFI_MODE = 0x00000001;
static constexpr uint32_t G_W10_REBOOT_WIFI_STA  = 0x00000002;
static constexpr uint32_t G_W10_REBOOT_WIFI_AP   = 0x00000004;
static constexpr uint32_t G_W10_REBOOT_WIFI_MDNS = 0x00000008;
static constexpr uint32_t G_W10_REBOOT_OTHER     = 0x80000000;

static constexpr uint16_t G_W10_API_VER = 411;

// =======================================================
// [v0415 Wave H] 펌웨어 버전 문자열
// =======================================================
static constexpr const char* G_APP_FW_VER = "v0415";

static constexpr uint8_t  G_W10_BODY_SLOTS = 4;
static constexpr uint32_t G_W10_BODY_SLOT_STALE_MS = 1500;

static constexpr uint8_t G_W10_DIAG_EVT_MAX = 16;

// -------------------------------------------------------
// Body Slot / Diag Event
// -------------------------------------------------------
struct ST_W10_BodySlot {
    AsyncWebServerRequest* req;
    size_t len;
    uint32_t lastMs;
    char buf[G_W10_BODY_MAX + 1];
};

struct ST_W10_DiagEvt_t {
    uint32_t ms;
    char code[24];
};

// -------------------------------------------------------
// Mods (HID modifier byte 1:1)
// -------------------------------------------------------
struct ST_W10_Mod_t {
    const char* name;
    uint8_t     mask;
};
static constexpr ST_W10_Mod_t G_W10_MODS[] = {
    {  "None", 0x00}, { "LCtrl", 0x01}, {"LShift", 0x02}, {  "LAlt", 0x04},
    { "LMeta", 0x08}, { "RCtrl", 0x10}, {"RShift", 0x20}, {  "RAlt", 0x40},
    { "RMeta", 0x80},
};

// -------------------------------------------------------
// Consumer (24-bit HID Consumer Page, Descriptor SSOT)
// -------------------------------------------------------
struct ST_W10_Consumer_t {
    const char* name;
    uint32_t    mask;
};
static constexpr ST_W10_Consumer_t G_W10_CONSUMER[] = {
    {        "None", 0x00000000},
    {        "PLAY", 0x00000001},
    {       "PAUSE", 0x00000002},
    {      "RECORD", 0x00000004},
    {          "FF", 0x00000008},
    {      "REWIND", 0x00000010},
    {  "NEXT_TRACK", 0x00000020},
    {  "PREV_TRACK", 0x00000040},
    {        "STOP", 0x00000080},
    {       "EJECT", 0x00000100},
    {      "RANDOM", 0x00000200},
    {      "REPEAT", 0x00000400},
    {  "PLAY_PAUSE", 0x00000800},
    {        "MUTE", 0x00001000},
    {      "VOL_UP", 0x00002000},
    {    "VOL_DOWN", 0x00004000},
    {    "WWW_HOME", 0x00008000},
    {     "MY_COMP", 0x00010000},
    {        "CALC", 0x00020000},
    {     "WWW_FAV", 0x00040000},
    {  "WWW_SEARCH", 0x00080000},
    {    "WWW_STOP", 0x00100000},
    {    "WWW_BACK", 0x00200000},
    {   "MEDIA_SEL", 0x00400000},
    {        "MAIL", 0x00800000},
};

// =====================================================
// [v0415 Phase 1-3] KB Usage 이름 테이블
// -----------------------------------------------------
//  - code 오름차순 정렬 필수 (이진 탐색)
//  - 106개 항목
//  - check_schema_0415.py::check_w10_kb_names_sorted 로 검증
//  - _kbName() 이진 탐색으로 O(log n) 조회
// =====================================================
struct ST_W10_KbName_t {
    uint16_t    code;
    const char* name;
};

static constexpr ST_W10_KbName_t G_W10_KB_NAMES[] = {
    {0x00, "None"},

    // 영문 알파벳
    {0x04, "A"}, {0x05, "B"}, {0x06, "C"}, {0x07, "D"},
    {0x08, "E"}, {0x09, "F"}, {0x0A, "G"}, {0x0B, "H"},
    {0x0C, "I"}, {0x0D, "J"}, {0x0E, "K"}, {0x0F, "L"},
    {0x10, "M"}, {0x11, "N"}, {0x12, "O"}, {0x13, "P"},
    {0x14, "Q"}, {0x15, "R"}, {0x16, "S"}, {0x17, "T"},
    {0x18, "U"}, {0x19, "V"}, {0x1A, "W"}, {0x1B, "X"},
    {0x1C, "Y"}, {0x1D, "Z"},

    // 숫자
    {0x1E, "1"}, {0x1F, "2"}, {0x20, "3"}, {0x21, "4"},
    {0x22, "5"}, {0x23, "6"}, {0x24, "7"}, {0x25, "8"},
    {0x26, "9"}, {0x27, "0"},

    // 기본 편집 키
    {0x28, "Enter"}, {0x29, "Esc"}, {0x2A, "Backspace"},
    {0x2B, "Tab"},   {0x2C, "Space"},

    // 기호 (0x2D ~ 0x38)
    {0x2D, "Minus"},     {0x2E, "Equal"},     {0x2F, "LBracket"},
    {0x30, "RBracket"},  {0x31, "Backslash"}, {0x32, "NonUSHash"},
    {0x33, "Semicolon"}, {0x34, "Quote"},     {0x35, "Grave"},
    {0x36, "Comma"},     {0x37, "Period"},    {0x38, "Slash"},

    // 편집 / 탐색
    {0x39, "CapsLock"},
    {0x3A, "F1"},  {0x3B, "F2"},  {0x3C, "F3"},  {0x3D, "F4"},
    {0x3E, "F5"},  {0x3F, "F6"},  {0x40, "F7"},  {0x41, "F8"},
    {0x42, "F9"},  {0x43, "F10"}, {0x44, "F11"}, {0x45, "F12"},
    {0x46, "PrtSc"},      {0x47, "ScrollLock"}, {0x48, "Pause"},
    {0x49, "Insert"},     {0x4A, "Home"},
    {0x4B, "PageUp"},     {0x4C, "Delete"},     {0x4D, "End"},
    {0x4E, "PageDown"},
    {0x4F, "Right"}, {0x50, "Left"}, {0x51, "Down"}, {0x52, "Up"},

    // 숫자 키패드 (0x53 ~ 0x63)
    {0x53, "NumLock"}, {0x54, "KP_Slash"}, {0x55, "KP_Star"},
    {0x56, "KP_Minus"}, {0x57, "KP_Plus"}, {0x58, "KP_Enter"},
    {0x59, "KP_1"}, {0x5A, "KP_2"}, {0x5B, "KP_3"}, {0x5C, "KP_4"},
    {0x5D, "KP_5"}, {0x5E, "KP_6"}, {0x5F, "KP_7"}, {0x60, "KP_8"},
    {0x61, "KP_9"}, {0x62, "KP_0"}, {0x63, "KP_Dot"},

    // Power (v0415 신규)
    {0x66, "Power"},

    // 수정자 (0xE0 ~ 0xE7)
    {0xE0, "LCtrl"},  {0xE1, "LShift"}, {0xE2, "LAlt"},  {0xE3, "LGui"},
    {0xE4, "RCtrl"},  {0xE5, "RShift"}, {0xE6, "RAlt"},  {0xE7, "RGui"},
};

static constexpr size_t G_W10_KB_NAMES_COUNT =
    sizeof(G_W10_KB_NAMES) / sizeof(G_W10_KB_NAMES[0]);

// =====================================================
// [v0415 Phase 2-2] 에러 코드 → HTTP 매핑 테이블
// -----------------------------------------------------
//  - code 사전순 정렬 필수 (이진 탐색)
//  - 26개 항목
//  - check_schema_0415.py::check_w10_err_map_sorted 로 검증
//  - _httpFromCode() 이진 탐색으로 O(log n) 조회
// =====================================================
struct ST_W10_ErrMap_t {
    const char* code;
    int         http;
};

static constexpr ST_W10_ErrMap_t G_W10_ERR_MAP[] = {
    {"api_not_found",          404},
    {"bad_json",               400},
    {"ble_disconnected",       400},
    {"body_too_large",         413},
    {"config_apply_failed",    500},
    {"config_export_failed",   500},
    {"config_get_failed",      500},
    {"config_import_failed",   500},
    {"config_rollback_failed", 500},
    {"config_save_failed",     500},
    {"control_failed",         500},
    {"factory_reset_failed",   500},
    {"no_body_slot",           503},
    {"no_config",              503},
    {"no_map",                 400},
    {"no_reboot_needed",       409},
    {"ota_failed",             500},
    {"ota_guard_blocked",      403},
    {"ppt_set_failed",         500},
    {"ppt_test_failed",        500},
    {"reason_mask_mismatch",   409},
    {"safe_mode_blocked",      403},
    {"safeboot_exit_failed",   500},
    {"static_forbidden",       403},
    {"static_not_found",       404},
    {"validation_failed",      400},
};

static constexpr size_t G_W10_ERR_MAP_COUNT =
    sizeof(G_W10_ERR_MAP) / sizeof(G_W10_ERR_MAP[0]);

// =======================================================
// [W10-E10 Interface]
// =======================================================
struct ST_W10_E10If_t {
    void* ctx;

    // Status snapshot
    bool (*getStatus)(void* ctx, ST_E10_Status_t* out);

    // Runtime apply-only
    bool (*applyRuntimeE10)(void* ctx, const ST_C10_E10Config_t* e10);

    // Runtime controls
    bool (*setDpiLevel)(void* ctx, uint8_t level);
    bool (*setPrecisionMode)(void* ctx, uint8_t mode);
    bool (*setHardClickLock)(void* ctx, bool en);

    // 모드 직접 지정
    bool (*setActiveMode)(void* ctx, uint8_t mode);

    bool (*setSafeMode)(void* ctx, bool en);
    bool (*setOtaGuard)(void* ctx, bool en);

    bool (*forceReleaseButtons)(void* ctx);
    bool (*requestGyroCalibration)(void* ctx);
    bool (*requestI2CRecover)(void* ctx);
    bool (*clearDiagnostics)(void* ctx);

    // PPT test
    bool (*testPptKey2)(void* ctx, uint8_t page, uint8_t mod, uint32_t code);

    // Profile 관리
    bool (*reloadProfile)(void* ctx);
    bool (*saveProfile)(void* ctx);
    bool (*getProfileInfo)(void* ctx,
                           uint8_t* outIdx, uint8_t* outCount,
                           char* outName, size_t outNameSize);
    bool (*switchProfile)(void* ctx, uint8_t p_idx);

    // Live Test
    bool (*execLiveTest)(void* ctx,
                         uint8_t p_kind, uint8_t p_hMode,
                         uint16_t p_p16, uint32_t p_p32);

    // Multi-Host
    bool (*requestHostOp)(void* ctx, uint8_t op);
    bool (*getHostState)(void* ctx,
                         uint8_t* outBondCount,
                         uint8_t* outActiveIdx,
                         uint8_t* outTargetIdx,
                         bool* outSelecting,
                         char* outActivePeerName,
                         size_t nameBufSize);
};

// -------------------------------------------------------
// 확장자/경로 검증 (기존 유지)
// -------------------------------------------------------
static inline bool W10_isAllowedWwwExt(const char* p_extLower) {
    if (!p_extLower) return false;
    return (strcmp(p_extLower, "html") == 0)
        || (strcmp(p_extLower, "css") == 0)
        || (strcmp(p_extLower, "js") == 0)
        || (strcmp(p_extLower, "svg") == 0)
        || (strcmp(p_extLower, "png") == 0)
        || (strcmp(p_extLower, "webp") == 0)
        || (strcmp(p_extLower, "ico") == 0)
        || (strcmp(p_extLower, "txt") == 0);
}
static inline bool W10_isAllowedPublicJsonExt(const char* p_extLower) {
    if (!p_extLower) return false;
    return (strcmp(p_extLower, "json") == 0);
}
static inline bool W10_isGzipTargetExt(const char* p_extLower) {
    if (!p_extLower) return false;
    return (strcmp(p_extLower, "html") == 0)
        || (strcmp(p_extLower, "css") == 0)
        || (strcmp(p_extLower, "js") == 0);
}

// -------------------------------------------------------
// 파일명 버전 토큰 판별 (_NNNN)
// -------------------------------------------------------
static inline bool W10_hasVersionToken(const char* p_pathOrName) {
    if (!p_pathOrName) return false;

    const char* v_dot = strrchr(p_pathOrName, '.');
    if (!v_dot) return false;

    if (strcasecmp(v_dot + 1, "gz") == 0) {
        size_t v_prefixLen = (size_t)(v_dot - p_pathOrName);
        if (v_prefixLen == 0) return false;

        char v_tmp[256];
        memset(v_tmp, 0, sizeof(v_tmp));
        if (v_prefixLen >= sizeof(v_tmp)) v_prefixLen = sizeof(v_tmp) - 1;
        memcpy(v_tmp, p_pathOrName, v_prefixLen);
        v_tmp[v_prefixLen] = '\0';

        const char* v_dot2 = strrchr(v_tmp, '.');
        if (!v_dot2) return false;

        ptrdiff_t off = (ptrdiff_t)(v_dot2 - v_tmp);
        v_dot = p_pathOrName + off;
    }

    if (v_dot <= (p_pathOrName + 4)) return false;

    const char* v_u = v_dot - 5;
    if (*v_u != '_') return false;

    const char c1 = v_u[1];
    const char c2 = v_u[2];
    const char c3 = v_u[3];
    const char c4 = v_u[4];

    if (c1 < '0' || c1 > '9') return false;
    if (c2 < '0' || c2 > '9') return false;
    if (c3 < '0' || c3 > '9') return false;
    if (c4 < '0' || c4 > '9') return false;

    return true;
}

static inline const char* W10_contentTypeFromExt(const char* p_extLower) {
    if (!p_extLower) return "application/octet-stream";
    if (strcmp(p_extLower, "html") == 0) return "text/html";
    if (strcmp(p_extLower, "css") == 0) return "text/css";
    if (strcmp(p_extLower, "js") == 0) return "application/javascript";
    if (strcmp(p_extLower, "svg") == 0) return "image/svg+xml";
    if (strcmp(p_extLower, "png") == 0) return "image/png";
    if (strcmp(p_extLower, "webp") == 0) return "image/webp";
    if (strcmp(p_extLower, "ico") == 0) return "image/x-icon";
    if (strcmp(p_extLower, "json") == 0) return "application/json";
    if (strcmp(p_extLower, "txt") == 0) return "text/plain";
    return "application/octet-stream";
}

static inline const char* W10_cacheControlForStatic(const char* p_path,
                                                    const char* p_extLower,
                                                    bool p_isPublicJson) {
    if (p_isPublicJson) return G_W10_CACHE_NOSTORE;
    if (p_extLower && strcmp(p_extLower, "html") == 0) return G_W10_CACHE_NOSTORE;
    if (W10_hasVersionToken(p_path)) return G_W10_CACHE_IMMUTABLE;
    return G_W10_CACHE_SHORT;
}

static inline bool W10_isPathSafe(const char* p_path) {
    if (!p_path) return false;
    if (strstr(p_path, "..")) return false;
    return true;
}

static inline const char* W10_getLowerExt(const char* p_path, char* p_outExt, size_t p_outSize) {
    if (!p_path || !p_outExt || p_outSize < 2) return nullptr;
    p_outExt[0] = '\0';

    const char* v_dot = strrchr(p_path, '.');
    if (!v_dot || v_dot == p_path) return nullptr;

    const char* v_ext = v_dot + 1;
    size_t      v_len = strlen(v_ext);
    if (v_len == 0 || v_len >= p_outSize) return nullptr;

    for (size_t i = 0; i < v_len; i++) {
        char c = v_ext[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        p_outExt[i] = c;
    }
    p_outExt[v_len] = '\0';
    return p_outExt;
}
