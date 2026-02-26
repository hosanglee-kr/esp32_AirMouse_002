// =======================================================
// File: W10_WebCfg_Static_0314.cpp
// =======================================================

/*
 * ------------------------------------------------------
 * 소스명 : W10_WebCfg_Static_0314.cpp
 * 모듈약어 : W10
 * 모듈명 : Web Config/Status/UI/OTA Server (Split: static/body/diag helpers)
 * ------------------------------------------------------
 * 기능 요약
 *  - (0314) 0313 분할: 정적서빙 + body slot + collectBody + staticErr + diagPush
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

#include "W10_WebCfg_0314.h"

// =====================================================
// CRC32 (bitwise) - small payload only (samples)
// =====================================================
static uint32_t W10_crc32_update(uint32_t p_crc, const uint8_t* p_data, size_t p_len) {
    uint32_t v_crc = ~p_crc;
    for (size_t i = 0; i < p_len; i++) {
        v_crc ^= (uint32_t)p_data[i];
        for (uint8_t b = 0; b < 8; b++) {
            const uint32_t v_mask = (uint32_t)-(int32_t)(v_crc & 1u);
            v_crc = (v_crc >> 1) ^ (0xEDB88320u & v_mask);
        }
    }
    return ~v_crc;
}

static uint32_t W10_mix32(uint32_t x) {
    // lightweight mixing
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

// =====================================================
// Static용: 파일 ETag 계산(32-bit) + 파일 크기(optional)
// - mix: size + mtime + sample_crc
// - sample: head/mid/tail (each up to 64 bytes)
// =====================================================
bool CL_W10_WebConfig::_calcFileEtag32(const char* p_path, uint32_t& p_outEtag, size_t* p_outSize) {
    p_outEtag = 0;
    if (p_outSize) *p_outSize = 0;
    if (!p_path) return false;

    File v_f = LittleFS.open(p_path, "r");
    if (!v_f) return false;

    const size_t v_size = (size_t)v_f.size();
    if (p_outSize) *p_outSize = v_size;

    // ---- mtime (best-effort) ----
    uint32_t v_mtime = 0;
#if defined(ARDUINO_ARCH_ESP32)
    // ESP32 Arduino FS: depending on core, File may support getLastWrite()
    // If unavailable, this still compiles because it's inside try-like macro guards?
    // 현실적으로 컴파일 에러를 막기 위해 아래는 "존재할 때만" 사용 권장.
    // 사용 환경에서 getLastWrite()가 확실히 있으면 이 블록을 활성화하세요.
    // v_mtime = (uint32_t)v_f.getLastWrite();
#endif

    // ---- sample crc ----
    uint8_t v_buf[64];
    memset(v_buf, 0, sizeof(v_buf));

    uint32_t v_crc = 0;

    auto readAt = [&](size_t off) {
        if (v_size == 0) return;
        if (off >= v_size) off = 0;
        v_f.seek(off, SeekSet);
        const size_t want = (v_size - off < sizeof(v_buf)) ? (v_size - off) : sizeof(v_buf);
        if (want == 0) return;
        const size_t got = (size_t)v_f.read(v_buf, want);
        if (got > 0) v_crc = W10_crc32_update(v_crc, v_buf, got);
    };

    if (v_size <= 128) {
        // small file: read all (still cheap)
        v_f.seek(0, SeekSet);
        while (true) {
            const size_t got = (size_t)v_f.read(v_buf, sizeof(v_buf));
            if (got == 0) break;
            v_crc = W10_crc32_update(v_crc, v_buf, got);
        }
    } else {
        // head/mid/tail samples
        readAt(0);
        readAt(v_size / 2);
        readAt((v_size > 64) ? (v_size - 64) : 0);
    }

    v_f.close();

    // ---- mix ----
    uint32_t v_e = 0xA5C3F19Bu;
    v_e ^= W10_mix32((uint32_t)v_size);
    v_e ^= W10_mix32((uint32_t)v_mtime);
    v_e ^= W10_mix32(v_crc);

    p_outEtag = v_e;
    return true;
}


// =====================================================
// 304 공통 (정적): Cache-Control 지정 + (선택) Vary:Accept-Encoding
// - Cache-Control은 200과 "동일 값"으로 넣어 일관성 유지
// - Vary는 "실제로 gzip variant를 사용한 경우에만" 넣기 위한 옵션
// =====================================================
void CL_W10_WebConfig::_send304StaticWithCacheControl(
    AsyncWebServerRequest* req,
    uint32_t p_etag,
    const char* p_cacheControl,
    bool p_varyAcceptEncoding) {

    if (!req) return;

    AsyncWebServerResponse* v_res = req->beginResponse(304);

    // Cache-Control: 200과 동일 정책 적용(정적 캐시정책을 304에서도 유지)
    v_res->addHeader("Cache-Control", (p_cacheControl ? p_cacheControl : G_W10_CACHE_NOSTORE));

    // ETag
    char v_tag[16];
    memset(v_tag, 0, sizeof(v_tag));
    _formatEtagQuoted(p_etag, v_tag, sizeof(v_tag));
    v_res->addHeader("ETag", v_tag);

    // Vary (정책: 정적에서 gzip을 "실제로" 사용한 경우만)
    if (p_varyAcceptEncoding) {
        v_res->addHeader("Vary", "Accept-Encoding");
    }

    req->send(v_res);
}



// =====================================================
// POST body slots
// =====================================================
ST_W10_BodySlot* CL_W10_WebConfig::_bodySlotAlloc(AsyncWebServerRequest* req) {
    for (uint8_t i = 0; i < G_W10_BODY_SLOTS; i++) {
        if (s_bodySlots[i].req == req) return &s_bodySlots[i];
    }

    for (uint8_t i = 0; i < G_W10_BODY_SLOTS; i++) {
        if (s_bodySlots[i].req == nullptr) {
            s_bodySlots[i].req = req;
            s_bodySlots[i].len = 0;
            s_bodySlots[i].lastMs = millis();
            memset(s_bodySlots[i].buf, 0, sizeof(s_bodySlots[i].buf));
            return &s_bodySlots[i];
        }
    }

    uint8_t v_oldIdx = 0;
    uint32_t v_oldMs = s_bodySlots[0].lastMs;
    for (uint8_t i = 1; i < G_W10_BODY_SLOTS; i++) {
        if (s_bodySlots[i].lastMs < v_oldMs) {
            v_oldMs = s_bodySlots[i].lastMs;
            v_oldIdx = i;
        }
    }

    const uint32_t v_now = millis();
    if ((v_now - v_oldMs) < G_W10_BODY_SLOT_STALE_MS) {
        return nullptr;
    }

    s_bodySlots[v_oldIdx].req = req;
    s_bodySlots[v_oldIdx].len = 0;
    s_bodySlots[v_oldIdx].lastMs = v_now;
    memset(s_bodySlots[v_oldIdx].buf, 0, sizeof(s_bodySlots[v_oldIdx].buf));
    return &s_bodySlots[v_oldIdx];
}

ST_W10_BodySlot* CL_W10_WebConfig::_bodyGetSlot(AsyncWebServerRequest* req, size_t index) {
    if (index == 0) {
        return _bodySlotAlloc(req);
    }
    for (uint8_t i = 0; i < G_W10_BODY_SLOTS; i++) {
        if (s_bodySlots[i].req == req) return &s_bodySlots[i];
    }
    return _bodySlotAlloc(req);
}

void CL_W10_WebConfig::_bodyFree(AsyncWebServerRequest* req) {
    for (uint8_t i = 0; i < G_W10_BODY_SLOTS; i++) {
        if (s_bodySlots[i].req == req) {
            s_bodySlots[i].req = nullptr;
            s_bodySlots[i].len = 0;
            s_bodySlots[i].lastMs = 0;
            memset(s_bodySlots[i].buf, 0, sizeof(s_bodySlots[i].buf));
            return;
        }
    }
}

// =====================================================
// gzip Accept
// =====================================================
bool CL_W10_WebConfig::_acceptsGzip(AsyncWebServerRequest* req) {
    if (!req->hasHeader("Accept-Encoding")) return false;
    const String v_ae = req->header("Accept-Encoding");
    return (v_ae.indexOf("gzip") >= 0);
}

// =====================================================
// wants JSON?
// =====================================================
bool CL_W10_WebConfig::_wantsJson(AsyncWebServerRequest* req) const {
    if (!req) return false;
    if (!req->hasHeader("Accept")) return false;
    const AsyncWebHeader* h = req->getHeader("Accept");
    if (!h) return false;
    const String v = h->value();
    return (v.indexOf("application/json") >= 0);
}

// =====================================================
// Static error helper (JSON도 stream)
// =====================================================
void CL_W10_WebConfig::_sendStaticErr(AsyncWebServerRequest* req, int p_http, const char* p_code, const char* p_msg) {
    if (!req) return;

    if (_wantsJson(req)) {
        JsonDocument doc;
        doc["ok"] = false;
        doc["code"] = (p_code ? p_code : "error");
        doc["msg"] = (p_msg ? p_msg : "error");

        JsonObject data = doc["data"].to<JsonObject>();
        data["http"] = (int)p_http;
        data["uri"] = req->url();

        // static도 cache-control은 no-store로 통일(요구: stream 통일)
        AsyncResponseStream* res = req->beginResponseStream("application/json");
        res->setCode(p_http);
        res->addHeader("Cache-Control", G_W10_CACHE_NOSTORE);
        serializeJson(doc, *res);
        req->send(res);
        return;
    }

    req->send(p_http, "text/plain", (p_msg ? p_msg : "error"));
}

// =====================================================
// diag ring push
// =====================================================
void CL_W10_WebConfig::_diagPush(const char* p_code) {
    if (!p_code) return;

    const uint32_t v_now = (uint32_t)millis();

    _diagEvt[_diagEvtHead].ms = v_now;
    strlcpy(_diagEvt[_diagEvtHead].code, p_code, sizeof(_diagEvt[_diagEvtHead].code));

    _diagEvtHead = (uint8_t)((_diagEvtHead + 1) % G_W10_DIAG_EVT_MAX);
    if (_diagEvtCount < G_W10_DIAG_EVT_MAX) _diagEvtCount++;
}

// =====================================================
// Body Collector (fixed slots)
// =====================================================
bool CL_W10_WebConfig::_collectBodyOrReply(AsyncWebServerRequest* req,
    uint8_t* data, size_t len,
    size_t index, size_t total,
    String& p_outBody) {

    if (total > G_W10_BODY_MAX) {
        _cnt_body_too_large++;
        _diagPush("body_too_large");
        JsonDocument v_doc;
        v_doc["max"] = (uint32_t)G_W10_BODY_MAX;
        v_doc["total"] = (uint32_t)total;
        _sendErr(req, "body_too_large", "Request body too large.", &v_doc);
        return false;
    }

    ST_W10_BodySlot* v_slot = _bodyGetSlot(req, index);
    if (!v_slot) {
        _cnt_body_no_slot++;
        _diagPush("no_body_slot");

        JsonDocument v_doc;
        v_doc["slots"] = (uint8_t)G_W10_BODY_SLOTS;
        v_doc["stale_ms"] = (uint32_t)G_W10_BODY_SLOT_STALE_MS;
        v_doc["hint"] = "too many concurrent POST; retry";

        _sendErr(req, "no_body_slot", "Server is busy. Try again.", &v_doc);
        return false;
    }

    if ((v_slot->len + len) > G_W10_BODY_MAX) {
        _cnt_body_too_large++;
        _diagPush("body_too_large");
        JsonDocument v_doc;
        v_doc["max"] = (uint32_t)G_W10_BODY_MAX;
        v_doc["total"] = (uint32_t)total;
        _bodyFree(req);
        _sendErr(req, "body_too_large", "Request body too large.", &v_doc);
        return false;
    }

    memcpy((void*)(v_slot->buf + v_slot->len), (const void*)data, len);
    v_slot->len += len;
    v_slot->lastMs = millis();
    v_slot->buf[v_slot->len] = '\0';

    if (index + len < total) return false;

    p_outBody = String(v_slot->buf);
    _bodyFree(req);
    return true;
}

// =====================================================
// Dynamic static serving (화이트리스트 + 확장자 제한)
// =====================================================
void CL_W10_WebConfig::_handleDynamicStatic(AsyncWebServerRequest* req) {
    const String v_uri = req->url();

    if (v_uri.startsWith("/api/")) {
        _sendErr(req, "api_not_found", "API endpoint not found.");
        return;
    }

    if (v_uri == "/favicon.ico") {
        _serveWwwStatic(req, "/www/favicon.ico");
        return;
    }
    if (v_uri == "/robots.txt") {
        _serveWwwStatic(req, "/www/robots.txt");
        return;
    }

    if (v_uri.startsWith(G_W10_URI_WWW_PREFIX)) {
        if (!W10_isPathSafe(v_uri.c_str())) {
            _sendStaticErr(req, 403, "static_forbidden", "forbidden");
            return;
        }
        _serveWwwStatic(req, v_uri.c_str());
        return;
    }

    if (v_uri.startsWith(G_W10_URI_JSON_PUBLIC_PREFIX)) {
        if (!W10_isPathSafe(v_uri.c_str())) {
            _sendStaticErr(req, 403, "static_forbidden", "forbidden");
            return;
        }
        _servePublicJson(req, v_uri.c_str());
        return;
    }

    _sendStaticErr(req, 404, "static_not_found", "not found");
}

void CL_W10_WebConfig::_serveWwwStatic(AsyncWebServerRequest* req, const char* p_path) {
    if (!req) return;

    if (!p_path) {
        _sendStaticErr(req, 404, "static_not_found", "not found");
        return;
    }

    char v_ext[12];
    memset(v_ext, 0, sizeof(v_ext));
    const char* v_extLower = W10_getLowerExt(p_path, v_ext, sizeof(v_ext));

    if (!v_extLower || !W10_isAllowedWwwExt(v_extLower)) {
        _sendStaticErr(req, 403, "static_forbidden", "forbidden");
        return;
    }

    bool v_useGz = false;
    String v_gzPath;

    // gzip: html/css/js 만 (.gz 존재 + Accept-Encoding:gzip) 시 사용
    if (W10_isGzipTargetExt(v_extLower)) {
        v_gzPath = String(p_path) + ".gz";
        if (LittleFS.exists(v_gzPath) && _acceptsGzip(req)) {
            v_useGz = true;
        }
    }

    const char* v_sendPath = p_path;
    if (v_useGz) v_sendPath = v_gzPath.c_str();

    if (!LittleFS.exists(v_sendPath)) {
        _sendStaticErr(req, 404, "static_not_found", "not found");
        return;
    }

    const char* v_ct = W10_contentTypeFromExt(v_extLower);

    // Cache-Control: 200 정책을 먼저 확정
    const char* v_cc = W10_cacheControlForStatic(p_path, v_extLower, false);

    // ETag 계산은 "실제 보내는 파일"(gz면 gz) 기준
    uint32_t v_etag = 0;
    size_t v_size = 0;
    const bool v_etagOk = _calcFileEtag32(v_sendPath, v_etag, &v_size);

    // 304 (정적): Cache-Control=200과 동일 값, Vary는 "실제로 gzip 사용한 경우"만
    if (v_etagOk && _ifNoneMatchHit(req, v_etag)) {
        _send304StaticWithCacheControl(req, v_etag, v_cc, v_useGz);
        return;
    }

    AsyncWebServerResponse* res = req->beginResponse(LittleFS, v_sendPath, v_ct);

    if (v_useGz) {
        res->addHeader("Content-Encoding", "gzip");
        // Vary는 gzip 실제 사용 시에만
        res->addHeader("Vary", "Accept-Encoding");
    }

    res->addHeader("Cache-Control", v_cc);

    // ETag (200)
    if (v_etagOk) {
        char v_tag[16];
        memset(v_tag, 0, sizeof(v_tag));
        _formatEtagQuoted(v_etag, v_tag, sizeof(v_tag));
        res->addHeader("ETag", v_tag);
    }

    req->send(res);
}

void CL_W10_WebConfig::_servePublicJson(AsyncWebServerRequest* req, const char* p_path) {
    if (!req) return;

    if (!p_path) {
        _sendStaticErr(req, 404, "static_not_found", "not found");
        return;
    }

    char v_ext[12];
    memset(v_ext, 0, sizeof(v_ext));
    const char* v_extLower = W10_getLowerExt(p_path, v_ext, sizeof(v_ext));

    if (!v_extLower || !W10_isAllowedPublicJsonExt(v_extLower)) {
        _sendStaticErr(req, 403, "static_forbidden", "forbidden");
        return;
    }

    if (!LittleFS.exists(p_path)) {
        _sendStaticErr(req, 404, "static_not_found", "not found");
        return;
    }

    // public json: 정책상 no-store
    // ETag+304는 동일 패턴으로 추가, 단 Vary는 넣지 않음(정책: Vary는 정적 gzip만)
    uint32_t v_etag = 0;
    size_t v_size = 0;
    const bool v_etagOk = _calcFileEtag32(p_path, v_etag, &v_size);

    if (v_etagOk && _ifNoneMatchHit(req, v_etag)) {
        // public json 304는 no-store 유지 + ETag
        _send304NoStoreEtag(req, v_etag);
        return;
    }

    AsyncWebServerResponse* res = req->beginResponse(LittleFS, p_path, "application/json");
    res->addHeader("Cache-Control", G_W10_CACHE_NOSTORE);

    if (v_etagOk) {
        char v_tag[16];
        memset(v_tag, 0, sizeof(v_tag));
        _formatEtagQuoted(v_etag, v_tag, sizeof(v_tag));
        res->addHeader("ETag", v_tag);
    }

    // (옵션/요구) public json 200에서는 Vary 제거(더 엄격)
    req->send(res);
}


/*
void CL_W10_WebConfig::_serveWwwStatic(AsyncWebServerRequest* req, const char* p_path) {
    if (!p_path) {
        _sendStaticErr(req, 404, "static_not_found", "not found");
        return;
    }

    char v_ext[12];
    memset(v_ext, 0, sizeof(v_ext));
    const char* v_extLower = W10_getLowerExt(p_path, v_ext, sizeof(v_ext));

    if (!v_extLower || !W10_isAllowedWwwExt(v_extLower)) {
        _sendStaticErr(req, 403, "static_forbidden", "forbidden");
        return;
    }

    bool v_useGz = false;
    String v_gzPath;

    if (W10_isGzipTargetExt(v_extLower)) {
        v_gzPath = String(p_path) + ".gz";
        if (LittleFS.exists(v_gzPath) && _acceptsGzip(req)) {
            v_useGz = true;
        }
    }

    const char* v_sendPath = p_path;
    if (v_useGz) v_sendPath = v_gzPath.c_str();

    if (!LittleFS.exists(v_sendPath)) {
        _sendStaticErr(req, 404, "static_not_found", "not found");
        return;
    }

    const char* v_ct = W10_contentTypeFromExt(v_extLower);
    AsyncWebServerResponse* res = req->beginResponse(LittleFS, v_sendPath, v_ct);

    if (v_useGz) res->addHeader("Content-Encoding", "gzip");

    const char* v_cc = W10_cacheControlForStatic(p_path, v_extLower, false);
    res->addHeader("Cache-Control", v_cc);

    req->send(res);
}

void CL_W10_WebConfig::_servePublicJson(AsyncWebServerRequest* req, const char* p_path) {
    if (!p_path) {
        _sendStaticErr(req, 404, "static_not_found", "not found");
        return;
    }

    char v_ext[12];
    memset(v_ext, 0, sizeof(v_ext));
    const char* v_extLower = W10_getLowerExt(p_path, v_ext, sizeof(v_ext));

    if (!v_extLower || !W10_isAllowedPublicJsonExt(v_extLower)) {
        _sendStaticErr(req, 403, "static_forbidden", "forbidden");
        return;
    }

    if (!LittleFS.exists(p_path)) {
        _sendStaticErr(req, 404, "static_not_found", "not found");
        return;
    }

    AsyncWebServerResponse* res = req->beginResponse(LittleFS, p_path, "application/json");
    res->addHeader("Cache-Control", G_W10_CACHE_NOSTORE);
    req->send(res);
}
*/
