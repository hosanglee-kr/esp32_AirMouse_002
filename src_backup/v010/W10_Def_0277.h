// =======================================================
// File: src/v010/W10_Def_0277.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : W10_Def_0277.h
 * 모듈약어 : W10
 * 모듈명 : Web Defs (Types/Consts/Keycodes/Cache Rules)
 * ------------------------------------------------------
 * 기능 요약
 *  - W10 공용 타입/상수/고정 문자열/키코드 프리셋 분리
 *  - 정적파일 캐시 정책 자동 분류 규칙(immutable/no-store/short)
 *  - 동적 서빙 허용 확장자 목록(보안) 정의
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

static constexpr const char* G_W10_URI_WWW_PREFIX = "/www/";
static constexpr const char* G_W10_URI_JSON_PUBLIC_PREFIX = "/json/public/";

static constexpr const char* G_W10_PATH_WWW_PREFIX = "/www/";
static constexpr const char* G_W10_PATH_JSON_PUBLIC_PREFIX = "/json/public/";

static constexpr const char* G_W10_CACHE_NOSTORE   = "no-store";
static constexpr const char* G_W10_CACHE_IMMUTABLE = "public, max-age=31536000, immutable";
// 개발/운영 중 “버전 토큰 없는 파일”은 짧게 캐시(필요하면 조정)
static constexpr const char* G_W10_CACHE_SHORT     = "public, max-age=3600";

// 버전 토큰 판별 규칙(파일명에 이 토큰이 있으면 immutable 후보)
// - 예: app_0275.js, style_0275.css, logo_v0275.webp 등
static constexpr const char* G_W10_VER_TOKEN_A = "_027";
static constexpr const char* G_W10_VER_TOKEN_B = "-027";
static constexpr const char* G_W10_VER_TOKEN_C = "v027";

struct ST_W10_Mod_t { const char* name; uint8_t mask; };
static constexpr ST_W10_Mod_t G_W10_MODS[] = {
    {"None",0x00},{"LCtrl",0x01},{"LShift",0x02},{"LAlt",0x04},{"LMeta",0x08},
    {"RCtrl",0x10},{"RShift",0x20},{"RAlt",0x40},{"RMeta",0x80},
};

struct ST_W10_Consumer_t { const char* name; uint32_t mask; };
static constexpr ST_W10_Consumer_t G_W10_CONSUMER[] = {
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

// 허용 확장자(동적 서빙)
// - /www/* : html/css/js/svg/png/webp/ico 만 허용 (gzip는 html/css/js만)
// - /json/public/* : json만 허용
static inline bool W10_isAllowedWwwExt(const char* p_extLower){
    if(!p_extLower) return false;
    return (strcmp(p_extLower,"html")==0) ||
           (strcmp(p_extLower,"css")==0)  ||
           (strcmp(p_extLower,"js")==0)   ||
           (strcmp(p_extLower,"svg")==0)  ||
           (strcmp(p_extLower,"png")==0)  ||
           (strcmp(p_extLower,"webp")==0) ||
           (strcmp(p_extLower,"ico")==0);
}
static inline bool W10_isAllowedPublicJsonExt(const char* p_extLower){
    if(!p_extLower) return false;
    return (strcmp(p_extLower,"json")==0);
}

// 파일명 버전 토큰이 있으면 immutable 후보로 분류
static inline bool W10_hasVersionToken(const char* p_pathOrName){
    if(!p_pathOrName) return false;
    return (strstr(p_pathOrName, G_W10_VER_TOKEN_A) != nullptr) ||
           (strstr(p_pathOrName, G_W10_VER_TOKEN_B) != nullptr) ||
           (strstr(p_pathOrName, G_W10_VER_TOKEN_C) != nullptr);
}
