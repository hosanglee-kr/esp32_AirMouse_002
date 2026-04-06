// =======================================================
// File: src/v010/W10_Def_0278.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : W10_Def_0278.h
 * 모듈약어 : W10
 * 모듈명 : Web Defs (Types/Consts/Keycodes/Cache Rules)
 * ------------------------------------------------------
 * 기능 요약
 *  - W10 공용 타입/상수/고정 문자열/키코드 프리셋 분리
 *  - 정적파일 캐시 정책 자동 분류 규칙(immutable/no-store/short)
 *  - 동적 서빙 허용 확장자 목록(보안) 정의
 *  - Content-Type 매핑 + gzip 후보 확장자 정의
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

// -------------------------------------------------------
// URI/Path Prefix (W10 라우팅 화이트리스트 핵심)
// -------------------------------------------------------
static constexpr const char* G_W10_URI_WWW_PREFIX         = "/www/";
static constexpr const char* G_W10_URI_JSON_PUBLIC_PREFIX = "/json/public/";

static constexpr const char* G_W10_PATH_WWW_PREFIX         = "/www/";
static constexpr const char* G_W10_PATH_JSON_PUBLIC_PREFIX = "/json/public/";

// 루트 접속 시 기본 index (프로젝트 빌드/배포 규칙에 맞게 고정)
// - 예: /www/index_0276.html
static constexpr const char* G_W10_DEFAULT_INDEX_PATH = "/www/index_0276.html";

// -------------------------------------------------------
// Cache-Control presets
// -------------------------------------------------------
static constexpr const char* G_W10_CACHE_NOSTORE   = "no-store";
static constexpr const char* G_W10_CACHE_IMMUTABLE = "public, max-age=31536000, immutable";
static constexpr const char* G_W10_CACHE_SHORT     = "public, max-age=3600";

// -------------------------------------------------------
// 버전 토큰 규칙(파일명에 포함되면 immutable 후보)
// - 예: app_0279.js, style-0279.css, logo_v0279.webp ...
// -------------------------------------------------------
static constexpr const char* G_W10_VER_TOKEN_A = "_027";
static constexpr const char* G_W10_VER_TOKEN_B = "-027";
static constexpr const char* G_W10_VER_TOKEN_C = "v027";

// -------------------------------------------------------
// PPT keycodes presets (W10 /api/keycodes)
// - mod mask == HID modifier byte (E10 정책과 1:1)
// -------------------------------------------------------
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

// -------------------------------------------------------
// 허용 확장자(동적 서빙 보안 규칙)
// - /www/* : html/css/js/svg/png/webp/ico 만 허용
// - /json/public/* : json 만 허용
// - gzip 대상: html/css/js 만 (.gz 존재 + Accept-Encoding:gzip)
// -------------------------------------------------------
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
static inline bool W10_isGzipTargetExt(const char* p_extLower){
    if(!p_extLower) return false;
    return (strcmp(p_extLower,"html")==0) ||
           (strcmp(p_extLower,"css")==0)  ||
           (strcmp(p_extLower,"js")==0);
}

// -------------------------------------------------------
// 파일명 버전 토큰 판별(immutable 후보)
// -------------------------------------------------------
static inline bool W10_hasVersionToken(const char* p_pathOrName){
    if(!p_pathOrName) return false;
    return (strstr(p_pathOrName, G_W10_VER_TOKEN_A) != nullptr) ||
           (strstr(p_pathOrName, G_W10_VER_TOKEN_B) != nullptr) ||
           (strstr(p_pathOrName, G_W10_VER_TOKEN_C) != nullptr);
}

// -------------------------------------------------------
// Content-Type mapping (확장자 소문자 기준)
// -------------------------------------------------------
static inline const char* W10_contentTypeFromExt(const char* p_extLower){
    if(!p_extLower) return "application/octet-stream";
    if(strcmp(p_extLower,"html")==0) return "text/html";
    if(strcmp(p_extLower,"css")==0)  return "text/css";
    if(strcmp(p_extLower,"js")==0)   return "application/javascript";
    if(strcmp(p_extLower,"svg")==0)  return "image/svg+xml";
    if(strcmp(p_extLower,"png")==0)  return "image/png";
    if(strcmp(p_extLower,"webp")==0) return "image/webp";
    if(strcmp(p_extLower,"ico")==0)  return "image/x-icon";
    if(strcmp(p_extLower,"json")==0) return "application/json";
    if(strcmp(p_extLower,"txt")==0)  return "text/plain";
    return "application/octet-stream";
}

// -------------------------------------------------------
// Cache-Control auto rule
// - no-store: html, /json/public/*, /api/*
// - immutable: 버전 토큰 포함한 정적 리소스
// - short: 나머지 정적 리소스
// -------------------------------------------------------
static inline const char* W10_cacheControlForStatic(const char* p_path, const char* p_extLower, bool p_isPublicJson){
    if(p_isPublicJson) return G_W10_CACHE_NOSTORE;
    if(p_extLower && strcmp(p_extLower,"html")==0) return G_W10_CACHE_NOSTORE;
    if(W10_hasVersionToken(p_path)) return G_W10_CACHE_IMMUTABLE;
    return G_W10_CACHE_SHORT;
}

// -------------------------------------------------------
// Path safety (최소한의 디렉토리 트래버설 방지)
// -------------------------------------------------------
static inline bool W10_isPathSafe(const char* p_path){
    if(!p_path) return false;
    if(strstr(p_path, "..")) return false;
    if(strstr(p_path, "//")) return false;
    return true;
}

// -------------------------------------------------------
// 확장자 추출(소문자) : p_outExt 버퍼에 기록
// - 반환값: p_outExt (성공) / nullptr (실패)
// -------------------------------------------------------
static inline const char* W10_getLowerExt(const char* p_path, char* p_outExt, size_t p_outSize){
    if(!p_path || !p_outExt || p_outSize < 2) return nullptr;
    p_outExt[0] = '\0';

    const char* v_dot = strrchr(p_path, '.');
    if(!v_dot || v_dot == p_path) return nullptr;

    const char* v_ext = v_dot + 1;
    size_t v_len = strlen(v_ext);
    if(v_len == 0 || v_len >= p_outSize) return nullptr;

    for(size_t i=0;i<v_len;i++){
        char c = v_ext[i];
        if(c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        p_outExt[i] = c;
    }
    p_outExt[v_len] = '\0';
    return p_outExt;
}
