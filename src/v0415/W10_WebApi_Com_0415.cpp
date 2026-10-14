// =======================================================
// File: src/v0415/W10_WebApi_Com_0415.cpp
// =======================================================
/*
 * ------------------------------------------------------
 * 소스명 : W10_WebApi_Com_0415.cpp
 * 모듈약어 : W10
 * 모듈명 : API 공통 유틸/정책
 * ------------------------------------------------------
 * 기능 요약
 *  - (0316) API 공통 유틸/정책 분리
 *  - JSON Stream helper, ETag/304(API), SafeMode Gate, reboot helper 등
 *
 * [v0415 Phase 2-1]
 *  - _parseBodyAndGate() 구현 추가
 *    · SafeMode 게이트 + Body 수집 + JSON 파싱을 단일 헬퍼로 통합
 *    · 15개 POST API 핸들러의 3단 보일러플레이트 제거
 *
 * [v0415 Phase 2-2]
 *  - _httpFromCode() if-else 40줄 → 이진 탐색 15줄
 *    · G_W10_ERR_MAP[] 테이블 (W10_Def_0415.h)
 *    · 반드시 사전순 정렬 유지
 * ------------------------------------------------------
 * [구현 규칙]
 *  - ArduinoJson v7.x.x 사용
 *  - JsonDocument 단일 타입만 사용
 * ------------------------------------------------------
 */

#include "W10_Web_0415.h"


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
// ETag 문자열 생성 (따옴표 포함)
// 예) "1A2B3C4D"
// =====================================================
void CL_W10_WebConfig::_formatEtagQuoted(uint32_t p_etag, char* p_out, size_t p_outSize) const {
    if (!p_out || p_outSize < 4) return;
    snprintf(p_out, p_outSize, "\"%08X\"", (unsigned int)p_etag);
}


// =====================================================
// If-None-Match 호환 체크
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

    v_inm.replace(" ", "");
    v_inm.replace("\t", "");
    v_inm.replace("\r", "");
    v_inm.replace("\n", "");

    int start = 0;
    while (start < v_inm.length()) {
        int comma = v_inm.indexOf(',', start);
        String tok = (comma < 0) ? v_inm.substring(start)
                                 : v_inm.substring(start, comma);
        start = (comma < 0) ? v_inm.length() : (comma + 1);

        if (tok.length() == 0) continue;

        if (tok.startsWith("W/") || tok.startsWith("w/")) tok = tok.substring(2);

        if (tok.startsWith("\"") && tok.endsWith("\"") && tok.length() >= 2) {
            tok = tok.substring(1, tok.length() - 1);
        }

        if (tok == "*") return true;
        if (tok.equalsIgnoreCase(v_tagRaw)) return true;
    }
    return false;
}


// =====================================================
// 304 공통 (API/public json): no-store + ETag
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
// Standard API response helpers
// =====================================================
void CL_W10_WebConfig::_sendOk(AsyncWebServerRequest* req, const char* p_code,
                               const char* p_msg, JsonDocument* p_data, int p_http) {
    JsonDocument d;
    d["ok"] = true;
    d["code"] = (p_code ? p_code : "ok");
    d["msg"] = (p_msg ? p_msg : "");
    if (p_data) {
        d["data"].set(p_data->as<JsonVariantConst>());
    }
    _sendJsonStream(req, d, p_http);
}

// =====================================================
// [v0415 Phase 2-2] 에러 코드 → HTTP 매핑
// -----------------------------------------------------
//   - G_W10_ERR_MAP[] (W10_Def_0415.h) 이진 탐색
//   - 사전순 정렬 필수
//   - 26개 코드
// =====================================================
int CL_W10_WebConfig::_httpFromCode(const char* p_code) {
    if (!p_code) return 500;

    size_t v_lo = 0;
    size_t v_hi = G_W10_ERR_MAP_COUNT;
    while (v_lo < v_hi) {
        size_t v_mid = (v_lo + v_hi) / 2;
        int v_cmp = strcmp(p_code, G_W10_ERR_MAP[v_mid].code);
        if (v_cmp < 0)      v_hi = v_mid;
        else if (v_cmp > 0) v_lo = v_mid + 1;
        else                return G_W10_ERR_MAP[v_mid].http;
    }
    return 500;
}

void CL_W10_WebConfig::_sendErr(AsyncWebServerRequest* req, const char* code,
                                const char* msg, JsonDocument* data) {
    int v_http = _httpFromCode(code);

    JsonDocument doc;
    doc["ok"] = false;
    doc["code"] = code ? code : "error";
    doc["msg"] = msg ? msg : "";

    if (data) {
        doc["data"].set(data->as<JsonVariantConst>());
    } else {
        doc["data"].to<JsonObject>();
    }
    _sendJsonStream(req, doc, v_http);
}


// =====================================================
// [v0415 Phase 2-1] POST API 공통 사전 처리 헬퍼
// -----------------------------------------------------
//  ① SafeMode 게이트
//  ② Body 수집
//  ③ JSON 파싱
//  실패 시 응답 자동 송출 후 false 반환
// =====================================================
bool CL_W10_WebConfig::_parseBodyAndGate(AsyncWebServerRequest* req,
                                         uint8_t* data, size_t len,
                                         size_t index, size_t total,
                                         JsonDocument& p_outDoc) {
    // ① SafeMode 게이트
    if (_gateSafeModeOrReply(req)) return false;

    // ② Body 수집
    String v_body;
    if (!_collectBodyOrReply(req, data, len, index, total, v_body)) return false;

    // ③ JSON 파싱
    DeserializationError v_err = deserializeJson(p_outDoc, v_body);
    if (v_err) {
        _cnt_json_bad++;
        _diagPush("bad_json");
        _sendErr(req, "bad_json", "Invalid JSON.");
        return false;
    }
    return true;
}


// -----------------------
// SafeMode API Gate Policy
// -----------------------
bool CL_W10_WebConfig::_isSafeMode() const {
    return (_cfg && _cfg->isSafeMode());
}

bool CL_W10_WebConfig::_isApiAllowedInSafeMode(const char* p_uri,
                                               WebRequestMethodComposite p_method) const {
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

    if (strcmp(p_uri, "/api/config/export") == 0) return true;
    if (strcmp(p_uri, "/api/config/rollback") == 0) return true;

    if (strcmp(p_uri, "/api/profiles/active") == 0) {
        return (p_method == HTTP_GET);
    }
    if (strcmp(p_uri, "/api/profiles") == 0) {
        return (p_method == HTTP_GET);
    }
    if (strcmp(p_uri, "/api/triggers") == 0) return true;

    return false;
}


// =====================================================
// SafeMode Gate (공통)
// =====================================================
bool CL_W10_WebConfig::_gateSafeModeOrReply(AsyncWebServerRequest* req) {
    if (!_isSafeMode()) return false;

    String v_uri_str;
    WebRequestMethodComposite v_method = HTTP_ANY;

    if (req) {
        v_uri_str = req->url();
        v_method  = req->method();
    }

    const char* v_uri = v_uri_str.c_str();

    if (_isApiAllowedInSafeMode(v_uri, v_method)) return false;

    _cnt_safe_blocked++;
    _diagPush("safe_mode_blocked");
    _sendErr(req, "safe_mode_blocked", "Blocked in safe mode.");
    return true;
}

// =====================================================
// reboot reason helpers
// =====================================================
uint32_t CL_W10_WebConfig::_wifiDiffMask(const ST_C10_WiFiConfig_t& a,
                                         const ST_C10_WiFiConfig_t& b) {
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
