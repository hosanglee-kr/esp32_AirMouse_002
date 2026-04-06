// =======================================================
// File: W10_WebApi_Com_0316.cpp
// =======================================================

/*
 * ------------------------------------------------------
 * 소스명 : W10_WebApi_Com_0316.cpp
 * 모듈약어 : W10
 * 모듈명 : Web Config/Status/UI/OTA Server (Split: API Common Helpers/Policies)
 * ------------------------------------------------------
 * 기능 요약
 *  - (0316) API 공통 유틸/정책 분리
 *  - JSON Stream helper, ETag/304(API), SafeMode Gate, reboot helper 등
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

 // 여기에 아래 함수들을 "그대로 잘라서" 붙여넣기:
// - _sendJsonStream
// - _resPrintJsonString(2)
// - _formatEtagQuoted
// - _ifNoneMatchHit
// - _send304NoStoreEtag
// - _sendOk / _httpFromCode / _sendErr
// - _isSafeMode / _isApiAllowedInSafeMode / _gateSafeModeOrReply
// - _wantsEnvelope
// - _wifiDiffMask / _markNeedReboot / _markLastApply / _rebootReasonsString
// - _addEtagHeadersNoStore


#include "W10_Web_0315.h"




// =====================================================
// Common JSON response helper (API는 무조건 no-store)
// =====================================================
void CL_W10_WebConfig::_sendJsonStream(AsyncWebServerRequest* req, JsonDocument& d, int p_code) {
    AsyncResponseStream* res = req->beginResponseStream("application/json");
    res->setCode(p_code);
    res->addHeader("Cache-Control", G_W10_CACHE_NOSTORE);
    serializeJson(d, *res);
    req->send(res);
}


// =====================================================
// Envelope streaming safe string writer (Option-2)
// - prints a JSON string literal with proper escaping
// - output example: "abc\"def\n"
// =====================================================
void CL_W10_WebConfig::_resPrintJsonString(AsyncResponseStream* res, const String& v) {
    if (!res) return;
    JsonDocument d;
    d.set(v);
    serializeJson(d, *res);
}

void CL_W10_WebConfig::_resPrintJsonString(AsyncResponseStream* res, const char* v) {
    if (!res) return;
    JsonDocument d;
    d.set(v ? v : "");
    serializeJson(d, *res);
}


// =====================================================
// ETag 문자열 생성 (따옴표 포함)
// 예) "1A2B3C4D"
// =====================================================
void CL_W10_WebConfig::_formatEtagQuoted(uint32_t p_etag, char* p_out, size_t p_outSize) const {
    if (!p_out || p_outSize < 4) return;
    snprintf(p_out, p_outSize, "\"%08X\"", (unsigned int)p_etag);
}


// =====================================================
// If-None-Match 호환 체크 (최소 대응)
// - "abcd", abcd, W/"abcd", W/abcd, 콤마 리스트 모두 대응
// - 공백/CRLF 제거
// - "*" 지원
// =====================================================
bool CL_W10_WebConfig::_ifNoneMatchHit(AsyncWebServerRequest* req, uint32_t p_etag) const {
    if (!req) return false;
    if (!req->hasHeader("If-None-Match")) return false;

    const AsyncWebHeader* h = req->getHeader("If-None-Match");
    if (!h) return false;

    char v_tagRaw[12];
    memset(v_tagRaw, 0, sizeof(v_tagRaw));
    snprintf(v_tagRaw, sizeof(v_tagRaw), "%08X", (unsigned int)p_etag);

    String v_inm = h->value();

    // 공백/개행 제거(대략)
    v_inm.replace(" ", "");
    v_inm.replace("\t", "");
    v_inm.replace("\r", "");
    v_inm.replace("\n", "");

    // 여러 ETag가 콤마로 올 수 있음: "a","b",W/"c"
    int start = 0;
    while (start < v_inm.length()) {
        int comma = v_inm.indexOf(',', start);
        String tok = (comma < 0) ? v_inm.substring(start) : v_inm.substring(start, comma);
        start = (comma < 0) ? v_inm.length() : (comma + 1);

        if (tok.length() == 0) continue;

        // Weak ETag 접두 제거: W/ 또는 w/
        if (tok.startsWith("W/") || tok.startsWith("w/")) tok = tok.substring(2);

        // 따옴표 제거: "ABCD" -> ABCD
        if (tok.startsWith("\"") && tok.endsWith("\"") && tok.length() >= 2) {
            tok = tok.substring(1, tok.length() - 1);
        }

        // If-None-Match: * (리소스가 존재하면 매치로 간주)
        if (tok == "*") return true;

        // tok는 보통 ABCDEF01 형태(따옴표/weak 제거됨)
        if (tok.equalsIgnoreCase(v_tagRaw)) return true;
    }
    return false;
}



// =====================================================
// 304 공통 (API/public json): no-store + ETag
// - Vary는 여기서 넣지 않음(정책: Vary는 정적(gzip)에서만)
// =====================================================
void CL_W10_WebConfig::_send304NoStoreEtag(AsyncWebServerRequest* req, uint32_t p_etag) {
    if (!req) return;

    AsyncWebServerResponse* res304 = req->beginResponse(304);

    char v_tag[16];
    memset(v_tag, 0, sizeof(v_tag));
    _formatEtagQuoted(p_etag, v_tag, sizeof(v_tag));

    res304->addHeader("Cache-Control", G_W10_CACHE_NOSTORE);
    res304->addHeader("ETag", v_tag);
    req->send(res304);
}



// =====================================================
// (C) Standard API response helpers
// - { ok, code, msg, data }
// =====================================================
void CL_W10_WebConfig::_sendOk(AsyncWebServerRequest* req, const char* p_code, const char* p_msg, JsonDocument* p_data, int p_http) {
    JsonDocument d;
    d["ok"] = true;
    d["code"] = (p_code ? p_code : "ok");
    d["msg"] = (p_msg ? p_msg : "");
    if (p_data) {
        d["data"].set(p_data->as<JsonVariantConst>());
    }
    _sendJsonStream(req, d, p_http);
}

int CL_W10_WebConfig::_httpFromCode(const char* p_code) {
    if (!p_code) return 500;

    if (!strcmp(p_code, "bad_json")) return 400;
    if (!strcmp(p_code, "validation_failed")) return 400;
    if (!strcmp(p_code, "no_map")) return 400;

    if (!strcmp(p_code, "safe_mode_blocked")) return 403;
    if (!strcmp(p_code, "ota_guard_blocked")) return 403;
    if (!strcmp(p_code, "static_forbidden")) return 403;

    if (!strcmp(p_code, "api_not_found")) return 404;
    if (!strcmp(p_code, "static_not_found")) return 404;

    if (!strcmp(p_code, "no_reboot_needed")) return 409;
    if (!strcmp(p_code, "reason_mask_mismatch")) return 409;

    if (!strcmp(p_code, "body_too_large")) return 413;

    if (!strcmp(p_code, "no_body_slot")) return 503;

    if (!strcmp(p_code, "no_config")) return 503;

    if (!strcmp(p_code, "config_get_failed")) return 500;
    if (!strcmp(p_code, "config_save_failed")) return 500;
    if (!strcmp(p_code, "config_apply_failed")) return 500;
    if (!strcmp(p_code, "config_export_failed")) return 500;
    if (!strcmp(p_code, "config_import_failed")) return 500;
    if (!strcmp(p_code, "config_rollback_failed")) return 500;

    if (!strcmp(p_code, "control_failed")) return 500;
    if (!strcmp(p_code, "ppt_set_failed")) return 500;
    if (!strcmp(p_code, "ppt_test_failed")) return 500;
    if (!strcmp(p_code, "ota_failed")) return 500;
    if (!strcmp(p_code, "factory_reset_failed")) return 500;
    if (!strcmp(p_code, "safeboot_exit_failed")) return 500;

    return 500;
}

void CL_W10_WebConfig::_sendErr(AsyncWebServerRequest* req, const char* code, const char* msg, JsonDocument* data) {
    int v_http = _httpFromCode(code);

    JsonDocument doc;
    doc["ok"] = false;
    doc["code"] = code ? code : "error";
    doc["msg"] = msg ? msg : "";

    if (data) {
        doc["data"].set(data->as<JsonVariantConst>());
    }
    _sendJsonStream(req, doc, v_http);
}


// -----------------------
// SafeMode API Gate Policy
// -----------------------
bool CL_W10_WebConfig::_isSafeMode() const {
    return (_cfg && _cfg->isSafeMode());
}



bool CL_W10_WebConfig::_isApiAllowedInSafeMode(const char* p_uri) const {
    if (!p_uri) return false;

    if (strcmp(p_uri, "/api/status") == 0) return true;
    if (strcmp(p_uri, "/api/diag") == 0) return true;
    if (strcmp(p_uri, "/api/diag/clear") == 0) return true;
    if (strcmp(p_uri, "/api/keycodes") == 0) return true;
    if (strcmp(p_uri, "/api/safeboot") == 0) return true;

    if (strcmp(p_uri, "/api/ota/status") == 0) return true;
    if (strcmp(p_uri, "/api/ota") == 0) return true;
    if (strcmp(p_uri, "/api/factory_reset") == 0) return true;
    if (strcmp(p_uri, "/api/reboot") == 0) return true;
    if (strcmp(p_uri, "/api/reboot/check") == 0) return true;

    if (strcmp(p_uri, "/api/config") == 0) return true;
    // save/apply/control/ppt 등은 SafeMode에선 차단(최소 정책)
    // if (strcmp(p_uri, "/api/config/save") == 0) return true;
    // if (strcmp(p_uri, "/api/config/apply") == 0) return true;
    if (strcmp(p_uri, "/api/config/export") == 0) return true;
    if (strcmp(p_uri, "/api/export") == 0) return true;
    if (strcmp(p_uri, "/api/config/import") == 0) return true;
    if (strcmp(p_uri, "/api/config/rollback") == 0) return true;

    return false;
}

// =====================================================
// SafeMode Gate (공통)
//  - SafeMode + 비허용 API면 표준 에러 응답 후 true 반환
// - SafeMode이고 허용 목록이 아니면:
//   * 카운터/진단 기록
//   * 표준 에러 응답
//   * true 반환(호출부에서 return 처리)
// =====================================================
bool CL_W10_WebConfig::_gateSafeModeOrReply(AsyncWebServerRequest* req) {
    if (!_isSafeMode()) return false;

    const char* v_uri = nullptr;
    if (req) v_uri = req->url().c_str();

    if (_isApiAllowedInSafeMode(v_uri)) return false;

    _cnt_safe_blocked++;
    _diagPush("safe_mode_blocked");
    _sendErr(req, "safe_mode_blocked", "Blocked in safe mode.");
    return true;
}


// =====================================================
// (STEP12) Envelope selector helper
// =====================================================
bool CL_W10_WebConfig::_wantsEnvelope(AsyncWebServerRequest* req) {
    if (!req) return false;

    if (req->hasParam("envelope")) {
        const AsyncWebParameter* p = req->getParam("envelope");
        if (p) {
            const String v = p->value();
            if (v == "1") return true;
            if (v == "0") return false;
            if (v == "auto") {
                // fallthrough
            } else {
                return false;
            }
        }
    }

    if (req->hasHeader("Accept")) {
        const AsyncWebHeader* h = req->getHeader("Accept");
        if (h) {
            const String a = h->value();
            if (a.indexOf("application/vnd.snw.envelope+json") >= 0) return true;
        }
    }
    return false;
}


// =====================================================
// reboot reason helpers
// =====================================================
uint32_t CL_W10_WebConfig::_wifiDiffMask(const ST_C10_WiFiConfig_t& a, const ST_C10_WiFiConfig_t& b) {
    uint32_t m = 0;
    if (a.mode != b.mode) m |= G_W10_REBOOT_WIFI_MODE;
    if (strncmp(a.sta_ssid, b.sta_ssid, sizeof(a.sta_ssid)) != 0 ||
        strncmp(a.sta_pass, b.sta_pass, sizeof(a.sta_pass)) != 0) {
        m |= G_W10_REBOOT_WIFI_STA;
    }
    if (strncmp(a.ap_ssid, b.ap_ssid, sizeof(a.ap_ssid)) != 0 ||
        strncmp(a.ap_pass, b.ap_pass, sizeof(a.ap_pass)) != 0) {
        m |= G_W10_REBOOT_WIFI_AP;
    }
    if (strncmp(a.mdns_host, b.mdns_host, sizeof(a.mdns_host)) != 0) {
        m |= G_W10_REBOOT_WIFI_MDNS;
    }
    return m;
}

void CL_W10_WebConfig::_markNeedReboot(uint32_t reasonMask) {
    _needReboot = true;
    _needRebootMask |= (reasonMask ? reasonMask : G_W10_REBOOT_OTHER);
}

void CL_W10_WebConfig::_markLastApply(bool ok, const char* src, const char* code) {
    _lastApplyOk = ok;
    _lastApplyMs = (uint32_t)millis();
    if (src) strlcpy(_lastApplySrc, src, sizeof(_lastApplySrc));
    else _lastApplySrc[0] = '\0';

    if (code) strlcpy(_lastApplyCode, code, sizeof(_lastApplyCode));
    else _lastApplyCode[0] = '\0';
}

String CL_W10_WebConfig::_rebootReasonsString(uint32_t m) {
    String s;
    auto add = [&](const char* t) {
        if (!s.isEmpty()) s += ",";
        s += t;
    };
    if (m & G_W10_REBOOT_WIFI_MODE) add("wifi_mode");
    if (m & G_W10_REBOOT_WIFI_STA) add("wifi_sta");
    if (m & G_W10_REBOOT_WIFI_AP) add("wifi_ap");
    if (m & G_W10_REBOOT_WIFI_MDNS) add("mdns");
    if (m & G_W10_REBOOT_OTHER) add("other");
    return s;
}




// =====================================================
// 200 공통 (API/config/export): no-store + ETag + X-Config-Size
// - 정적과 달리 gzip variant가 없으므로 Vary 필요 없음(정적만 Vary 표준화)
// =====================================================
void CL_W10_WebConfig::_addEtagHeadersNoStore(AsyncWebServerResponse* res,
                                             bool p_hasEtag,
                                             uint32_t p_etag,
                                             size_t p_size) {
    if (!res) return;

    res->addHeader("Cache-Control", G_W10_CACHE_NOSTORE);

    if (p_hasEtag) {
        char v_tag[16];
        memset(v_tag, 0, sizeof(v_tag));
        _formatEtagQuoted(p_etag, v_tag, sizeof(v_tag));
        res->addHeader("ETag", v_tag);
        res->addHeader("X-Config-Size", String((unsigned int)p_size));
    }
}



