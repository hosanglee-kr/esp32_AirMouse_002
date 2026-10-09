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
 * [v0415 주요 변경]
 *  - G_W10_API_VER: 410 → 411 (스키마 v411 정합)
 *  - G_W10_DEFAULT_INDEX_PATH: index_0414.html → index_0415.html
 *  - ST_W10_E10If_t::setPptMode 콜백 삭제 (Round G/H 정합)
 *    · set_ppt 명령 & ppt_mode 필드는 W10Api_CtlPpt / _Status에서 제거됨
 *  - G_W10_CONSUMER[] 값은 v0412 그대로 유지 (Descriptor 100% 일치, Round A 확정)
 *    · W10이 Descriptor SSOT (C20이 이에 맞춰 수정됨)
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

// [v0415] index 갱신
static constexpr const char* G_W10_DEFAULT_INDEX_PATH = "/www/index_0415.html";

// -------------------------------------------------------
// Cache-Control presets
// -------------------------------------------------------
static constexpr const char* G_W10_CACHE_NOSTORE   = "no-store";
static constexpr const char* G_W10_CACHE_IMMUTABLE = "public, max-age=31536000, immutable";
static constexpr const char* G_W10_CACHE_SHORT     = "public, max-age=3600";

static constexpr size_t G_W10_BODY_MAX = 8192;

// ----------------------------------------------------
// Reboot reason bits
// ----------------------------------------------------
static constexpr uint32_t G_W10_REBOOT_WIFI_MODE = 0x00000001;
static constexpr uint32_t G_W10_REBOOT_WIFI_STA  = 0x00000002;
static constexpr uint32_t G_W10_REBOOT_WIFI_AP   = 0x00000004;
static constexpr uint32_t G_W10_REBOOT_WIFI_MDNS = 0x00000008;
static constexpr uint32_t G_W10_REBOOT_OTHER     = 0x80000000;

// [v0415] API version bump
static constexpr uint16_t G_W10_API_VER = 411;

static constexpr uint8_t  G_W10_BODY_SLOTS = 4;
static constexpr uint32_t G_W10_BODY_SLOT_STALE_MS = 1500;

static constexpr uint8_t G_W10_DIAG_EVT_MAX = 16;

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
//   [v0415] 값은 v0412와 동일 (Descriptor 100% 일치)
//   C20_Action_0415.h의 EN_C20_Consumer_t와 값 체계 동일 (Phase 1 방안 1-B)
// -------------------------------------------------------
struct ST_W10_Consumer_t {
    const char* name;
    uint32_t    mask;
};
static constexpr ST_W10_Consumer_t G_W10_CONSUMER[] = {
    {        "None", 0x00000000},
    {        "Play", 0x00000001},   // Bit 0 : 0xB0
    {       "Pause", 0x00000002},   // Bit 1 : 0xB1
    {      "Record", 0x00000004},   // Bit 2 : 0xB2
    { "FastForward", 0x00000008},   // Bit 3 : 0xB3
    {      "Rewind", 0x00000010},   // Bit 4 : 0xB4
    {   "NextTrack", 0x00000020},   // Bit 5 : 0xB5
    {   "PrevTrack", 0x00000040},   // Bit 6 : 0xB6
    {        "Stop", 0x00000080},   // Bit 7 : 0xB7
    {       "Eject", 0x00000100},   // Bit 8 : 0xB8
    {  "RandomPlay", 0x00000200},   // Bit 9 : 0xB9
    {      "Repeat", 0x00000400},   // Bit 10: 0xBC
    {   "PlayPause", 0x00000800},   // Bit 11: 0xCD
    {        "Mute", 0x00001000},   // Bit 12: 0xE2
    {    "VolumeUp", 0x00002000},   // Bit 13: 0xE9
    {  "VolumeDown", 0x00004000},   // Bit 14: 0xEA
    {     "WWWHome", 0x00008000},   // Bit 15: 0x0223 (AC Home)
    {  "MyComputer", 0x00010000},   // Bit 16: 0x0194
    {  "Calculator", 0x00020000},   // Bit 17: 0x0192
    {"WWWFavorites", 0x00040000},   // Bit 18: 0x022A
    {   "WWWSearch", 0x00080000},   // Bit 19: 0x0221 (AC Search)
    {     "WWWStop", 0x00100000},   // Bit 20: 0x0226
    {     "WWWBack", 0x00200000},   // Bit 21: 0x0224 (AC Back)
    { "MediaSelect", 0x00400000},   // Bit 22: 0x0183
    {        "Mail", 0x00800000},   // Bit 23: 0x018A
};

// =======================================================
// [W10-E10 Interface]
// -------------------------------------------------------
// [v0415] setPptMode 콜백 삭제 (Round G/H 정합)
//   - E10의 setPptMode는 no-op 스텁이지만, W10에서 PPT 제어 자체가
//     더 이상 필요 없음 (active_mode로 일원화)
// =======================================================
struct ST_W10_E10If_t {
    void* ctx;

    // Status snapshot
    bool (*getStatus)(void* ctx, ST_E10_Status_t* out);

    // Runtime apply-only (no persist)
    bool (*applyRuntimeE10)(void* ctx, const ST_C10_E10Config_t* e10);

    // Runtime controls
    bool (*setDpiLevel)(void* ctx, uint8_t level);
    bool (*setPrecisionMode)(void* ctx, uint8_t mode);
    bool (*setHardClickLock)(void* ctx, bool en);

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

    // [v0415 삭제] bool (*setPptMode)(void* ctx, bool en);
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

static inline const char* W10_cacheControlForStatic(const char* p_path, const char* p_extLower, bool p_isPublicJson) {
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
