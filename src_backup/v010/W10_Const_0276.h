// =======================================================
// File: src/v010/W10_Const_0275.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : W10_Const_0275.h
 * 모듈약어 : W10
 * 모듈명 : Web Config Const (Types/Assets/Keymaps/Cache Policy Constants)
 * ------------------------------------------------------
 * 기능 요약
 *  - W10에서 사용하는 공용 타입/상수/키코드 맵/자산 경로 정의를 분리
 *  - 헤더-온리 유지(ODR 회피: inline/static constexpr 사용)
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

// ---- Version token(캐시 판정/자산 파일명 규칙에 사용) ----
static constexpr uint16_t G_W10_UI_VER = 275;

// ---- Asset route/info ----
struct ST_W10_Asset_t {
    const char* uri;   // HTTP URI
    const char* plain; // LittleFS path (plain)
    const char* gz;    // LittleFS path (gzip) - 없으면 nullptr 가능
    const char* type;  // mime
};

// ---- modifier presets (HID modifier byte 1:1) ----
struct ST_W10_Mod_t {
    const char* name;
    uint8_t mask;
};

inline static constexpr ST_W10_Mod_t G_W10_MODS[] = {
    {"None",0x00},{"LCtrl",0x01},{"LShift",0x02},{"LAlt",0x04},{"LMeta",0x08},
    {"RCtrl",0x10},{"RShift",0x20},{"RAlt",0x40},{"RMeta",0x80},
};

// ---- consumer presets (32-bit mask) ----
struct ST_W10_Consumer_t {
    const char* name;
    uint32_t mask;
};

inline static constexpr ST_W10_Consumer_t G_W10_CONSUMER[] = {
    {"None", 0x00000000},
    {"Play",0x00000001},{"Pause",0x00000002},{"Record",0x00000004},
    {"FastForward",0x00000008},{"Rewind",0x00000010},
    {"NextTrack",0x00000020},{"PrevTrack",0x00000040},
    {"Stop",0x00000080},{"Eject",0x00000100},{"RandomPlay",0x00000200},
    {"Repeat",0x00000400},{"PlayPause",0x00000800},
    {"Mute",0x00001000},{"VolumeUp",0x00002000},{"VolumeDown",0x00004000},
    {"WWWHome",0x00008000},{"MyComputer",0x00010000},{"Calculator",0x00020000},
    {"WWWFavorites",0x00040000},{"WWWSearch",0x00080000},{"WWWStop",0x00100000},
    {"WWWBack",0x00200000},{"MediaSelect",0x00400000},{"Mail",0x00800000},
};

// ---- Cache-Control strings ----
inline static constexpr const char* G_W10_CC_NOSTORE   = "no-store";
inline static constexpr const char* G_W10_CC_IMMUTABLE = "public, max-age=31536000, immutable";
inline static constexpr const char* G_W10_CC_SHORT     = "public, max-age=3600";

// ---- UI file paths (LittleFS) ----
inline static constexpr const char* G_W10_PATH_INDEX = "/www/index_0274.html";
inline static constexpr const char* G_W10_PATH_STYLE = "/www/style_0274.css";
inline static constexpr const char* G_W10_PATH_APP   = "/www/app_0274.js";

// ---- Asset table (URI -> path) ----
// NOTE: cache-control은 W10에서 "자동 분류"로 결정 (여기서는 목록만)
inline static constexpr ST_W10_Asset_t G_W10_ASSETS[] = {
    { "/",                 G_W10_PATH_INDEX, "/www/index_0274.html.gz", "text/html" },
    { "/www/",             G_W10_PATH_INDEX, "/www/index_0274.html.gz", "text/html" },
    { "/www/style_0274.css",G_W10_PATH_STYLE, "/www/style_0274.css.gz", "text/css" },
    { "/www/app_0274.js",  G_W10_PATH_APP,   "/www/app_0274.js.gz",     "application/javascript" },
};
