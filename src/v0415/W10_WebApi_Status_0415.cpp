// =======================================================
// File: src/v0415/W10_Web_Static_0415.cpp
// =======================================================
/*
 * ------------------------------------------------------
 * 소스명 : W10_Web_Static_0415.cpp
 * 모듈약어 : W10
 * 모듈명 : Web Config/Status/UI/OTA Server (Split: static/body/diag)
 * ------------------------------------------------------
 * 기능 요약
 *  - 정적서빙 + body slot + collectBody + staticErr + diagPush
 *
 * [v0415 주요 변경]
 *  - 파일명/심볼 _0415
 *  - include W10_Web_0415.h
 *  - 본문 로직 유지 (버전업만)
 *  - G_W10_DEFAULT_INDEX_PATH는 W10_Def_0415.h에서 index_0415.html
 * ------------------------------------------------------
 */

#include "W10_Web_0415.h"

// =====================================================
// internal: CRC32 (IEEE 802.3) - small tableless
// =====================================================
static uint32_t W10_crc32_update(uint32_t crc, const uint8_t* data, size_t len) {
    uint32_t c = crc;
    for (size_t i = 0; i < len; i++) {
        c ^= (uint32_t)data[i];
        for (uint8_t k = 0; k < 8; k++) {
            if (c & 1U) c = (c >> 1) ^ 0xEDB88320UL;
            else        c = (c >> 1);
        }
    }
    return c;
}

static uint32_t W10_fnv1a32(const char* s) {
    uint32_t h = 2166136261UL;
    if (!s) return h;
    while (*s) {
        h ^= (uint8_t)(*s++);
        h *= 16777619UL;
    }
    return h;
}

static uint32_t W10_rotl32(uint32_t x, uint8_t r) {
    return (x << r) | (x >> (32 - r));
}

// =====================================================
// Static용: 파일 ETag 계산
// =====================================================
bool CL_W10_WebConfig::_calcFileEtag32(const char* p_path, uint32_t& p_outEtag, size_t* p_outSize) {
    p_outEtag = 0;
    if (p_outSize) *p_outSize = 0;
    if (!p_path) return false;

    File f = LittleFS.open(p_path, "r");
    if (!f) return false;

    const size_t v_size = (size_t)f.size();
    if (p_outSize) *p_outSize = v_size;

    uint32_t v_mtime = 0;

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    v_mtime = (uint32_t)f.getLastWrite();
#endif

    uint32_t v_crc = 0xFFFFFFFFUL;

    uint8_t buf[256];
    memset(buf, 0, sizeof(buf));

    auto readAt = [&](size_t off, size_t want) {
        if (want == 0) return;
        if (off > v_size) return;
        if (off + want > v_size) want = (v_size - off);
        if (want == 0) return;

        f.seek(off, SeekSet);
        size_t got = f.read(buf, want);
        if (got > 0) {
            v_crc = W10_crc32_update(v_crc, buf, got);
        }
    };

    const size_t S = 256;
    readAt(0, (v_size >= S ? S : v_size));
    if (v_size > S) {
        size_t mid = (v_size / 2);
        if (mid >= (S / 2)) mid -= (S / 2);
        readAt(mid, (v_size >= S ? S : v_size));
    }
    if (v_size > (2 * S)) {
        readAt(v_size - S, S);
    }

    v_crc ^= 0xFFFFFFFFUL;

    uint32_t v_ph = W10_fnv1a32(p_path);

    uint32_t h = 0;
    h ^= (uint32_t)(v_size & 0xFFFFFFFFUL);
    h = W10_rotl32(h, 5) ^ v_crc;
    h = W10_rotl32(h, 7) ^ v_ph;
    h = W10_rotl32(h, 11) ^ v_mtime;

    h ^= (h >> 16);
    h *= 0x7FEB352DUL;
    h ^= (h >> 15);
    h *= 0x846CA68BUL;
    h ^= (h >> 16);

    p_outEtag = h;
    f.close();
    return true;
}

// =====================================================
// 304 공통 (정적)
// =====================================================
void CL_W10_WebConfig::_send304StaticWithCacheControl(AsyncWebServerRequest* req,
                                                     uint32_t p_etag,
                                                     const char* p_cacheControl,
                                                     bool p_varyAcceptEncoding) {
    if (!req) return;

    AsyncWebServerResponse* res304 = req->beginResponse(304);

    char v_tag[16];
    memset(v_tag, 0, sizeof(v_tag));
    _formatEtagQuoted(p_etag, v_tag, sizeof(v_tag));

    res304->addHeader("Cache-Control", (p_cacheControl ? p_cacheControl : G_W10_CACHE_NOSTORE));
    res304->addHeader("ETag", v_tag);

    if (p_varyAcceptEncoding) {
        res304->addHeader("Vary", "Accept-Encoding");
    }
    req->send(res304);
}

// =====================================================
// 200 공통 (정적)
// =====================================================
void CL_W10_WebConfig::_sendStaticWithCacheControlEtag(AsyncWebServerRequest* req,
                                                      const char* p_sendPath,
                                                      const char* p_contentType,
                                                      const char* p_cacheControl,
                                                      bool p_useGz,
                                                      bool p_varyAcceptEncoding,
                                                      bool p_hasEtag,
                                                      uint32_t p_etag,
                                                      size_t p_size) {
    if (!req || !p_sendPath || !p_contentType) {
        _sendStaticErr(req, 404, "static_not_found", "not found");
        return;
    }

    AsyncWebServerResponse* res = req->beginResponse(LittleFS, p_sendPath, p_contentType);

    if (p_useGz) {
        res->addHeader("Content-Encoding", "gzip");
        if (p_varyAcceptEncoding) res->addHeader("Vary", "Accept-Encoding");
    }

    res->addHeader("Cache-Control", (p_cacheControl ? p_cacheControl : G_W10_CACHE_NOSTORE));

    if (p_hasEtag) {
        char v_tag[16];
        memset(v_tag, 0, sizeof(v_tag));
        _formatEtagQuoted(p_etag, v_tag, sizeof(v_tag));
        res->addHeader("ETag", v_tag);
        res->addHeader("X-File-Size", String((unsigned int)p_size));
    }

    req->send(res);
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
// Static error helper
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
// Dynamic static serving
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
    if (!req || !p_path) {
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

    const char* v_cc = W10_cacheControlForStatic(p_path, v_extLower, false);

    uint32_t v_etag = 0;
    size_t v_size = 0;
    bool v_etagOk = _calcFileEtag32(v_sendPath, v_etag, &v_size);

    if (v_etagOk && _ifNoneMatchHit(req, v_etag)) {
        _send304StaticWithCacheControl(req, v_etag, v_cc, v_useGz);
        return;
    }

    const char* v_ct = W10_contentTypeFromExt(v_extLower);
    _sendStaticWithCacheControlEtag(req,
                                    v_sendPath,
                                    v_ct,
                                    v_cc,
                                    v_useGz,
                                    v_useGz,
                                    v_etagOk,
                                    v_etag,
                                    v_size);
}

void CL_W10_WebConfig::_servePublicJson(AsyncWebServerRequest* req, const char* p_path) {
    if (!req || !p_path) {
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

    uint32_t v_etag = 0;
    size_t v_size = 0;
    bool v_etagOk = _calcFileEtag32(p_path, v_etag, &v_size);

    if (v_etagOk && _ifNoneMatchHit(req, v_etag)) {
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
        res->addHeader("X-File-Size", String((unsigned int)v_size));
    }
    req->send(res);
}
