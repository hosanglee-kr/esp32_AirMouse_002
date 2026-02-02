#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : E10_PPT_Keymap_024.h
 * 모듈약어 : E10
 * 모듈명 : PPT Keymap Engine (AirMouse Full, HID 0x07 + Consumer 0x0C, Save/Load/Test Overlay)
 * ------------------------------------------------------
 * 기능 요약
 *  - PPT Keymap 저장/로드(LittleFS) + .bak 복구(A40_IO)
 *  - UI 편집을 위한 키 목록(Usage Page 0x07/0x0C) + 검색 필터
 *  - Apply without Save: test overlay(임시 적용) 지원
 *  - JSON 응답은 nested 생성 없이 "JSON Text"로 export (정책/메모리 안전)
 *  - (현장용) 잘못된 키맵/파싱 실패 시 D10 markFail/markSpike 연동 포인트 제공
 * ------------------------------------------------------
 * [구현 규칙]
 *  - 항상 소스 시작 주석 부분 체계 유지 및 내용 업데이트
 *  - 소스 시작 주석 부분 구현규칙, 코드네이밍규칙 내용 그대로 유지, 수정금지
 *  - ArduinoJson v7.x.x 사용 (v6 이하 사용 금지)
 *  - JsonDocument 단일 타입만 사용
 *  - createNestedArray/Object/containsKey 사용 금지
 *  - memset + strlcpy 기반 안전 초기화
 *  - 주석/필드명은 JSON 구조와 동일하게 유지
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 * - 전역 상수,매크로      : G_모듈약어_ 접두사
 * - 전역 변수             : g_모듈약어_ 접두사
 * - 전역 함수             : 모듈약어_ 접두사
 * - type                  : T_모듈약어_ 접두사
 * - typedef               : _t  접미사
 * - enum 상수             : EN_모듈약어_ 접두사
 * - 구조체                : ST_모듈약어_ 접두사
 * - 클래스명              : CL_모듈약어_ 접두사
 * - 클래스 private 멤버   : _ 접두사
 * - 클래스 멤버(함수/변수) : 모듈약어 접두사 미사용
 * - 클래스 정적 멤버      : s_ 접두사
 * - 함수 로컬 변수        : v_ 접두사
 * - 함수 인자             : p_접두사
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <string.h>
#include <ArduinoJson.h>

#include "A40_ComFunc_070.h"
#include "D10_Logger_061.h"

// ------------------------------------------------------
// [E10] 기본 설정
// ------------------------------------------------------
#ifndef G_E10_KEYMAP_FILE_PATH
#define G_E10_KEYMAP_FILE_PATH "/cfg/ppt_keymap_024.json"
#endif

#ifndef G_E10_KEY_LABEL_LEN
#define G_E10_KEY_LABEL_LEN 32
#endif

#ifndef G_E10_MAX_BIND
// 모드별 바인딩 개수(필요시 확장)
#define G_E10_MAX_BIND 24
#endif

#ifndef G_E10_MAX_MODE
#define G_E10_MAX_MODE 6
#endif

// HID Usage Page
#define G_E10_PAGE_KBD  0x0007
#define G_E10_PAGE_CONS 0x000C

// ------------------------------------------------------
// Keymap 엔트리
// ------------------------------------------------------
typedef struct {
    uint8_t  mode;     // 모드 인덱스 (0..G_E10_MAX_MODE-1)
    uint16_t page;     // 0x07 or 0x0C
    uint16_t usage;    // usage id
    char     action[G_E10_KEY_LABEL_LEN]; // UI에서 보이는 액션명(예: "NextSlide")
} ST_E10_Bind_t;

// ------------------------------------------------------
// (UI 키 목록용) 키 정의
// ------------------------------------------------------
typedef struct {
    uint16_t page;
    uint16_t usage;
    const char* name;     // 사람이 읽는 이름(검색 대상)
} ST_E10_KeyDef_t;

// ------------------------------------------------------
// E10 Keymap 엔진
// ------------------------------------------------------
class CL_E10_PPT_Keymap {
  public:
    // --------------------------------------------------
    // 초기화/기본값
    // --------------------------------------------------
    static void begin(const char* p_filePath = G_E10_KEYMAP_FILE_PATH) {
        memset(s_filePath, 0, sizeof(s_filePath));
        A40_ComFunc::copyStr2Buffer_safe(s_filePath, (p_filePath ? p_filePath : G_E10_KEYMAP_FILE_PATH),
                                        sizeof(s_filePath), __func__);

        _clearAll(false /*keepTest*/);
        _setDefaults();

        // FS 로드(실패해도 기본값으로 기동)
        (void)loadFromFile(true /*useBackup*/);
    }

    // --------------------------------------------------
    // 저장/로드
    // --------------------------------------------------
    static bool loadFromFile(bool p_useBackup = true) {
        JsonDocument v_doc; // 단일 doc
        if (!A40_IO::Load_File2JsonDoc_V22(s_filePath, v_doc, p_useBackup, __func__)) {
            D10_LOGW("[E10] loadFromFile: fallback to defaults (%s)", s_filePath);
            D10_LOGW("[E10] loadFromFile: file missing or parse fail");
            D10_LOGW("[E10] loadFromFile: keep defaults");
            // parse 실패는 현장 spike로 볼 수도 있음
            CL_D10_Logger::markSpike("E10_load_fail", 1);
            return false;
        }
        return _applyFromJsonDoc(v_doc, false /*toTest*/);
    }

    static bool saveToFile(bool p_pretty = true, bool p_useBackup = true) {
        JsonDocument v_doc; // 단일 doc
        _buildJsonDoc(v_doc, false /*fromTest*/);
        const bool ok = A40_IO::Save_JsonDoc2File_V22(s_filePath, v_doc, p_useBackup, p_pretty, __func__);
        if (!ok) {
            CL_D10_Logger::markFail("[E10] saveToFile failed: %s", s_filePath);
        }
        return ok;
    }

    // --------------------------------------------------
    // Apply without Save (test overlay)
    //  - UI에서 즉시 테스트 → 재부팅/저장 없이 적용 가능
    // --------------------------------------------------
    static bool applyTestFromJson(const char* p_jsonText) {
        if (!p_jsonText) return false;

        JsonDocument v_doc; // 단일 doc
        DeserializationError err = deserializeJson(v_doc, p_jsonText);
        if (err) {
            CL_D10_Logger::markFail("[E10] applyTestFromJson parse error: %s", err.c_str());
            return false;
        }
        return _applyFromJsonDoc(v_doc, true /*toTest*/);
    }

    static void clearTestOverlay() {
        _clearAll(true /*keepTest*/);
        // test overlay만 제거
        memset(s_testBinds, 0, sizeof(s_testBinds));
        s_testCount = 0;
        s_testEnabled = false;
        D10_LOGI("[E10] test overlay cleared");
    }

    static bool isTestEnabled() { return s_testEnabled; }

    // --------------------------------------------------
    // 현재 바인딩 조회(실제 적용 기준: test 우선, 없으면 saved)
    // --------------------------------------------------
    static uint16_t getEffectiveCount() {
        return (s_testEnabled ? s_testCount : s_savedCount);
    }

    static bool getEffectiveBind(uint16_t p_index, ST_E10_Bind_t& p_out) {
        memset(&p_out, 0, sizeof(p_out));
        if (s_testEnabled) {
            if (p_index >= s_testCount) return false;
            p_out = s_testBinds[p_index];
            return true;
        }
        if (p_index >= s_savedCount) return false;
        p_out = s_savedBinds[p_index];
        return true;
    }

    // (선택) mode+action 기준으로 바인딩 검색
    static bool findBindByAction(uint8_t p_mode, const char* p_action, ST_E10_Bind_t& p_out) {
        memset(&p_out, 0, sizeof(p_out));
        if (!p_action || !p_action[0]) return false;

        const ST_E10_Bind_t* v_arr = s_testEnabled ? s_testBinds : s_savedBinds;
        const uint16_t v_cnt       = s_testEnabled ? s_testCount : s_savedCount;

        for (uint16_t i = 0; i < v_cnt; i++) {
            if (v_arr[i].mode != p_mode) continue;
            if (strcasecmp(v_arr[i].action, p_action) == 0) {
                p_out = v_arr[i];
                return true;
            }
        }
        return false;
    }

    // --------------------------------------------------
    // UI용 JSON Text Export
    //  - /api/ppt/keymap
    // --------------------------------------------------
    static void getKeymapAsJsonText(String& p_out, bool p_includeTestState = true) {
        p_out.reserve(8192);
        p_out = "{";

        // file
        p_out += "\"file\":\"";
        _appendJsonEscaped(p_out, s_filePath);
        p_out += "\"";

        // test flag
        if (p_includeTestState) {
            p_out += ",\"testEnabled\":";
            p_out += (s_testEnabled ? "true" : "false");
        }

        // binds
        p_out += ",\"binds\":[";
        const uint16_t v_cnt = getEffectiveCount();
        for (uint16_t i = 0; i < v_cnt; i++) {
            ST_E10_Bind_t b;
            if (!getEffectiveBind(i, b)) continue;

            if (i) p_out += ",";
            p_out += "{";
            p_out += "\"mode\":";  p_out += String((int)b.mode);
            p_out += ",\"page\":"; p_out += String((unsigned)b.page);
            p_out += ",\"usage\":";p_out += String((unsigned)b.usage);
            p_out += ",\"action\":\""; _appendJsonEscaped(p_out, b.action); p_out += "\"";
            p_out += "}";
        }
        p_out += "]";

        p_out += "}";
    }

    // --------------------------------------------------
    // UI용 키 목록 JSON Text Export
    //  - /api/ppt/keys?page=7|12&q=xxx
    // --------------------------------------------------
    static void getKeyListAsJsonText(String& p_out,
                                    uint16_t p_page,
                                    const char* p_q,
                                    uint16_t p_limit = 300) {
        p_out.reserve(12288);
        p_out = "{";
        p_out += "\"page\":"; p_out += String((unsigned)p_page);
        p_out += ",\"q\":\""; _appendJsonEscaped(p_out, (p_q ? p_q : "")); p_out += "\"";
        p_out += ",\"keys\":[";

        uint16_t v_added = 0;
        for (uint16_t i = 0; ; i++) {
            const ST_E10_KeyDef_t* kd = _getKeyDefByIndex(i);
            if (!kd) break;
            if (kd->page != p_page) continue;

            if (p_q && p_q[0]) {
                if (!_containsIgnoreCase(kd->name, p_q)) continue;
            }

            if (v_added) p_out += ",";
            p_out += "{";
            p_out += "\"usage\":"; p_out += String((unsigned)kd->usage);
            p_out += ",\"name\":\""; _appendJsonEscaped(p_out, kd->name); p_out += "\"";
            p_out += "}";
            v_added++;
            if (v_added >= p_limit) break;
        }

        p_out += "],\"count\":"; p_out += String((unsigned)v_added);
        p_out += "}";
    }

    // --------------------------------------------------
    // UI에서 저장 요청(JSON body) 처리용
    //  - /api/ppt/keymap (POST)
    // --------------------------------------------------
    static bool applySavedFromJson(const char* p_jsonText) {
        if (!p_jsonText) return false;

        JsonDocument v_doc; // 단일 doc
        DeserializationError err = deserializeJson(v_doc, p_jsonText);
        if (err) {
            CL_D10_Logger::markFail("[E10] applySavedFromJson parse error: %s", err.c_str());
            return false;
        }
        const bool ok = _applyFromJsonDoc(v_doc, false /*toTest*/);
        if (!ok) return false;

        // 저장까지 수행(원하면 W10에서 분리 가능)
        return saveToFile(true /*pretty*/, true /*bak*/);
    }

  private:
    // --------------------------------------------------
    // 내부 상태
    // --------------------------------------------------
    static void _clearAll(bool p_keepTest) {
        memset(s_savedBinds, 0, sizeof(s_savedBinds));
        s_savedCount = 0;

        if (!p_keepTest) {
            memset(s_testBinds, 0, sizeof(s_testBinds));
            s_testCount = 0;
            s_testEnabled = false;
        }
    }

    static void _setDefaults() {
        // 기본 PPT 액션 예시(필요시 현 프로젝트 액션명에 맞춰 조정)
        // mode 0: PPT
        _setBindSaved(0, G_E10_PAGE_KBD, 0x4E /*Keyboard PageDown*/, "NextSlide");
        _setBindSaved(0, G_E10_PAGE_KBD, 0x4B /*Keyboard PageUp*/,   "PrevSlide");
        _setBindSaved(0, G_E10_PAGE_KBD, 0x29 /*ESC*/,              "Escape");
        _setBindSaved(0, G_E10_PAGE_CONS,0x00E9 /*VolUp*/,          "VolUp");
        _setBindSaved(0, G_E10_PAGE_CONS,0x00EA /*VolDown*/,        "VolDown");
        _setBindSaved(0, G_E10_PAGE_CONS,0x00E2 /*Mute*/,           "Mute");

        // mode 1: Laser/Focus(예시)
        _setBindSaved(1, G_E10_PAGE_KBD, 0x2C /*Space*/,            "BlankToggle");
    }

    static void _setBindSaved(uint8_t p_mode, uint16_t p_page, uint16_t p_usage, const char* p_action) {
        if (s_savedCount >= G_E10_MAX_BIND * G_E10_MAX_MODE) return;
        ST_E10_Bind_t& b = s_savedBinds[s_savedCount++];
        memset(&b, 0, sizeof(b));
        b.mode = p_mode;
        b.page = p_page;
        b.usage = p_usage;
        A40_ComFunc::copyStr2Buffer_safe(b.action, (p_action ? p_action : ""), sizeof(b.action), __func__);
    }

    // --------------------------------------------------
    // JSON Doc build/apply
    // --------------------------------------------------
    static void _buildJsonDoc(JsonDocument& p_doc, bool p_fromTest) {
        p_doc.clear();
        // 정책상 nested 생성 금지 → 여기서는 "파싱/저장용 doc" 이므로
        // 최소 구조만 operator[]로 세팅(주의: ArduinoJson은 내부적으로 nested를 만들 수 있으나,
        // 정책은 createNestedArray/Object 금지이므로 operator[] 기반 + serialize만 사용)
        //
        // 형태:
        // {
        //   "ver": 24,
        //   "binds": [
        //     {"mode":0,"page":7,"usage":78,"action":"NextSlide"},
        //     ...
        //   ]
        // }

        JsonObject root = p_doc.to<JsonObject>();
        root["ver"] = 24;

        // binds는 Array가 필요하지만 createNestedArray 금지 → operator[]로 접근 후 as<JsonArray>()
        JsonVariant vBinds = root["binds"];
        JsonArray a = vBinds.to<JsonArray>();

        const ST_E10_Bind_t* v_arr = p_fromTest ? s_testBinds : s_savedBinds;
        const uint16_t v_cnt       = p_fromTest ? s_testCount : s_savedCount;

        for (uint16_t i = 0; i < v_cnt; i++) {
            JsonObject o = a.add<JsonObject>();
            o["mode"] = v_arr[i].mode;
            o["page"] = v_arr[i].page;
            o["usage"]= v_arr[i].usage;
            o["action"]= v_arr[i].action;
        }
    }

    static bool _applyFromJsonDoc(const JsonDocument& p_doc, bool p_toTest) {
        JsonObjectConst root = p_doc.as<JsonObjectConst>();
        if (root.isNull()) {
            CL_D10_Logger::markFail("[E10] applyFromJsonDoc: root is null");
            return false;
        }

        // binds array
        JsonArrayConst a = A40_ComFunc::Json_getArr(root, "binds");
        if (a.isNull()) {
            CL_D10_Logger::markFail("[E10] applyFromJsonDoc: binds missing");
            return false;
        }

        ST_E10_Bind_t* v_dst = p_toTest ? s_testBinds : s_savedBinds;
        uint16_t*      v_cnt = p_toTest ? &s_testCount : &s_savedCount;

        memset(v_dst, 0, sizeof(s_savedBinds)); // 동일 크기
        *v_cnt = 0;

        uint16_t v_idx = 0;
        for (JsonVariantConst it : a) {
            if (v_idx >= (G_E10_MAX_BIND * G_E10_MAX_MODE)) break;

            JsonObjectConst o = it.as<JsonObjectConst>();
            if (o.isNull()) continue;

            const uint8_t  mode  = (uint8_t)A40_ComFunc::Json_getNum<uint16_t>(o, "mode", 0);
            const uint16_t page  = (uint16_t)A40_ComFunc::Json_getNum<uint16_t>(o, "page", G_E10_PAGE_KBD);
            const uint16_t usage = (uint16_t)A40_ComFunc::Json_getNum<uint16_t>(o, "usage", 0);
            const char*    act   = A40_ComFunc::Json_getStr(o, "action", "");

            // 유효성(필수)
            if (!act || !act[0]) {
                CL_D10_Logger::markSpike("E10_bad_action", (int32_t)v_idx);
                continue;
            }
            if (page != G_E10_PAGE_KBD && page != G_E10_PAGE_CONS) {
                CL_D10_Logger::markSpike("E10_bad_page", (int32_t)page);
                continue;
            }
            if (page == G_E10_PAGE_KBD) {
                // keyboard usage는 보통 0x00..0xE7 (modifier 포함)
                if (usage > 0x00FF) {
                    CL_D10_Logger::markSpike("E10_kbd_usage_big", (int32_t)usage);
                }
            }

            ST_E10_Bind_t& b = v_dst[v_idx];
            memset(&b, 0, sizeof(b));
            b.mode = (mode < G_E10_MAX_MODE) ? mode : 0;
            b.page = page;
            b.usage= usage;
            A40_ComFunc::copyStr2Buffer_safe(b.action, act, sizeof(b.action), __func__);

            v_idx++;
        }

        *v_cnt = v_idx;

        if (p_toTest) {
            s_testEnabled = true;
            D10_LOGI("[E10] test overlay applied (cnt=%u)", (unsigned)s_testCount);
        } else {
            s_testEnabled = false; // saved apply 시 test는 자동 종료(원하면 유지로 변경 가능)
            D10_LOGI("[E10] saved keymap applied (cnt=%u)", (unsigned)s_savedCount);
        }

        if (*v_cnt == 0) {
            CL_D10_Logger::markFail("[E10] applyFromJsonDoc: no valid binds");
            return false;
        }
        return true;
    }

    // --------------------------------------------------
    // 키 테이블 (최소/대표 + 확장 포인트)
    //  - 0x07 전체를 “완전한 이름”으로 다 넣으면 코드가 매우 길어지므로,
    //    현장 운영에 필요한 키는 이름 포함, 나머지는 "KBD_0xNN" 자동 표기로 처리
    //  - Consumer page는 대표 미디어키 이름 포함 + 나머지 자동 표기
    // --------------------------------------------------
    static const ST_E10_KeyDef_t* _getKeyDefByIndex(uint16_t p_idx) {
        // (1) 대표 키(이름 보장)
        static const ST_E10_KeyDef_t s_named[] = {
            {G_E10_PAGE_KBD, 0x04, "A"}, {G_E10_PAGE_KBD, 0x05, "B"}, {G_E10_PAGE_KBD, 0x06, "C"},
            {G_E10_PAGE_KBD, 0x28, "Enter"}, {G_E10_PAGE_KBD, 0x29, "Escape"},
            {G_E10_PAGE_KBD, 0x2C, "Space"},
            {G_E10_PAGE_KBD, 0x4B, "PageUp"}, {G_E10_PAGE_KBD, 0x4E, "PageDown"},
            {G_E10_PAGE_KBD, 0x50, "LeftArrow"}, {G_E10_PAGE_KBD, 0x4F, "RightArrow"},
            {G_E10_PAGE_CONS, 0x00CD, "PlayPause"},
            {G_E10_PAGE_CONS, 0x00B5, "ScanNextTrack"},
            {G_E10_PAGE_CONS, 0x00B6, "ScanPreviousTrack"},
            {G_E10_PAGE_CONS, 0x00E2, "Mute"},
            {G_E10_PAGE_CONS, 0x00E9, "VolumeUp"},
            {G_E10_PAGE_CONS, 0x00EA, "VolumeDown"},
        };

        const uint16_t namedCnt = (uint16_t)(sizeof(s_named) / sizeof(s_named[0]));
        if (p_idx < namedCnt) return &s_named[p_idx];

        // (2) 0x07 확장(자동 생성): 1..0xE7 (Modifier 포함 0xE0..0xE7)
        // (3) 0x0C 확장(자동 생성): 0x0000..0x02FF 범위(필요시)
        //
        // 자동 생성은 "정적 버퍼 1개"를 재사용하므로, 호출 후 즉시 사용(복사) 전제
        static ST_E10_KeyDef_t s_dyn;
        static char s_dynName[20];

        uint32_t v = (uint32_t)p_idx - (uint32_t)namedCnt;

        // keyboard auto: usage 0x01..0xE7
        const uint16_t kbdStart = 0x01;
        const uint16_t kbdEnd   = 0xE7;
        const uint16_t kbdCnt   = (uint16_t)(kbdEnd - kbdStart + 1);

        if (v < kbdCnt) {
            const uint16_t usage = (uint16_t)(kbdStart + v);
            s_dyn.page = G_E10_PAGE_KBD;
            s_dyn.usage = usage;
            snprintf(s_dynName, sizeof(s_dynName), "KBD_0x%02X", (unsigned)usage);
            s_dyn.name = s_dynName;
            return &s_dyn;
        }

        v -= kbdCnt;

        // consumer auto: usage 0x0000..0x02FF (768개)
        const uint16_t consStart = 0x0000;
        const uint16_t consEnd   = 0x02FF;
        const uint16_t consCnt   = (uint16_t)(consEnd - consStart + 1);

        if (v < consCnt) {
            const uint16_t usage = (uint16_t)(consStart + v);
            s_dyn.page = G_E10_PAGE_CONS;
            s_dyn.usage = usage;
            snprintf(s_dynName, sizeof(s_dynName), "CONS_0x%04X", (unsigned)usage);
            s_dyn.name = s_dynName;
            return &s_dyn;
        }

        return nullptr;
    }

    static bool _containsIgnoreCase(const char* p_hay, const char* p_needle) {
        if (!p_hay || !p_needle) return false;
        if (!p_needle[0]) return true;

        // 간단한 contains (case-insensitive)
        const size_t nLen = strlen(p_needle);
        if (nLen == 0) return true;

        for (size_t i = 0; p_hay[i] != '\0'; i++) {
            size_t k = 0;
            while (p_hay[i + k] != '\0' && k < nLen) {
                char a = p_hay[i + k];
                char b = p_needle[k];
                if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
                if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
                if (a != b) break;
                k++;
            }
            if (k == nLen) return true;
        }
        return false;
    }

    static void _appendJsonEscaped(String& p_out, const char* p_s) {
        if (!p_s) return;
        for (size_t i = 0; p_s[i] != '\0'; i++) {
            char c = p_s[i];
            if (c == '\"' || c == '\\') { p_out += '\\'; p_out += c; }
            else if (c == '\n') p_out += "\\n";
            else if (c == '\r') p_out += "\\r";
            else if (c == '\t') p_out += "\\t";
            else p_out += c;
        }
    }

  private:
    static char           s_filePath[96];

    static ST_E10_Bind_t  s_savedBinds[G_E10_MAX_BIND * G_E10_MAX_MODE];
    static uint16_t       s_savedCount;

    static ST_E10_Bind_t  s_testBinds[G_E10_MAX_BIND * G_E10_MAX_MODE];
    static uint16_t       s_testCount;
    static bool           s_testEnabled;
};

// ------------------------------------------------------
// static storage
// ------------------------------------------------------
inline char          CL_E10_PPT_Keymap::s_filePath[96] = {0};

inline ST_E10_Bind_t CL_E10_PPT_Keymap::s_savedBinds[G_E10_MAX_BIND * G_E10_MAX_MODE];
inline uint16_t      CL_E10_PPT_Keymap::s_savedCount = 0;

inline ST_E10_Bind_t CL_E10_PPT_Keymap::s_testBinds[G_E10_MAX_BIND * G_E10_MAX_MODE];
inline uint16_t      CL_E10_PPT_Keymap::s_testCount = 0;
inline bool          CL_E10_PPT_Keymap::s_testEnabled = false;

