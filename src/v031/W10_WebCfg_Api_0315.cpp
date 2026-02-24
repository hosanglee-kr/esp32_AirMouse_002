// =======================================================
// File: W10_WebCfg_Api_0315.cpp
// =======================================================

/*
 * ------------------------------------------------------
 * 소스명 : W10_WebCfg_Api_0315.cpp
 * 모듈약어 : W10
 * 모듈명 : Web Config/Status/UI/OTA Server (Split: APIs + JSON Stream)
 * ------------------------------------------------------
 * 기능 요약
 *  - (0314) API 전부 담당
 *  - AsyncResponseStream 모두 적용(모든 JSON 응답 스트리밍)
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
    // 표준 ETag는 따옴표 포함이 일반적
    snprintf(p_out, p_outSize, "\"%08X\"", (unsigned int)p_etag);
}

// =====================================================
// If-None-Match 호환 체크 (최소 대응)
// - "abcd", abcd, W/"abcd", W/abcd, 콤마 리스트 모두 대응
// =====================================================
bool CL_W10_WebConfig::_ifNoneMatchHit(AsyncWebServerRequest* req, uint32_t p_etag) const {
    if (!req) return false;
    if (!req->hasHeader("If-None-Match")) return false;

    const AsyncWebHeader* h = req->getHeader("If-None-Match");
    if (!h) return false;

    char v_tagQuoted[16];
    char v_tagRaw[12];
    memset(v_tagQuoted, 0, sizeof(v_tagQuoted));
    memset(v_tagRaw, 0, sizeof(v_tagRaw));

    _formatEtagQuoted(p_etag, v_tagQuoted, sizeof(v_tagQuoted));
    snprintf(v_tagRaw, sizeof(v_tagRaw), "%08X", (unsigned int)p_etag);

    String v_inm = h->value();

    // 공백 제거(대략)
    v_inm.replace(" ", "");
    v_inm.replace("\t", "");

    // 여러 ETag가 콤마로 올 수 있음: "a","b",W/"c"
    int start = 0;
    while (start < v_inm.length()) {
        int comma = v_inm.indexOf(',', start);
        String tok = (comma < 0) ? v_inm.substring(start) : v_inm.substring(start, comma);
        start = (comma < 0) ? v_inm.length() : (comma + 1);

        if (tok.length() == 0) continue;

        // Weak ETag 접두 제거: W/
        if (tok.startsWith("W/")) tok = tok.substring(2);

        // 따옴표 제거: "ABCD" -> ABCD
        if (tok.startsWith("\"") && tok.endsWith("\"") && tok.length() >= 2) {
            tok = tok.substring(1, tok.length() - 1);
        }

        // 이제 tok는 보통 ABCDEF01 형태(따옴표/weak 제거됨)
        if (tok.equalsIgnoreCase(v_tagRaw)) return true;

    }
    return false;
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
    if (strcmp(p_uri, "/api/config/save") == 0) return true;
    if (strcmp(p_uri, "/api/config/apply") == 0) return true;
    if (strcmp(p_uri, "/api/config/export") == 0) return true;
    if (strcmp(p_uri, "/api/export") == 0) return true;
    if (strcmp(p_uri, "/api/config/import") == 0) return true;
    if (strcmp(p_uri, "/api/config/rollback") == 0) return true;
    
    return false;
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
// /api/config/save, /api/import 공통 처리
// =====================================================
void CL_W10_WebConfig::_apiConfigSaveImportCommon(
    AsyncWebServerRequest* req,
    uint8_t* data, size_t len,
    size_t index, size_t total,
    const char* p_src,
    const char* p_note,
    bool p_applyAfterSave) {

    String v_body;
    if (!_collectBodyOrReply(req, data, len, index, total, v_body)) return;

    ST_C10_WiFiConfig_t v_prevWiFi = _wifi;

    bool v_saved = false;
    
    // importJson() 내부 applied 결과는 "import 내부 처리"로만 따로 받음(표기용/참고용)
    bool v_importApplied = false;
    
    // API가 말하는 applied는 "우리가 applyFn 호출했는지"로만 결정
    bool v_applied = false;
    
    bool v_ok = (_cfg && _cfg->importJson(v_body, v_saved, v_importApplied));
    
    if (v_ok && v_saved && _cfg) {
        (void)_cfg->loadAll(_wifi, _e10);
        uint32_t v_m = _wifiDiffMask(v_prevWiFi, _wifi);
        if (v_m != 0) _markNeedReboot(v_m);
    }
    
    // 실제 applied는 p_applyAfterSave에 의해 결정
    if (v_ok && v_saved && p_applyAfterSave && _applyFn) {
        v_applied = _applyFn(_applyCtx);
    }
    
    JsonDocument v_doc;
    v_doc["saved"] = v_saved;
    v_doc["applied"] = v_applied;
    v_doc["import_applied"] = v_importApplied; // (선택) 디버깅/호환에 도움
    v_doc["note"] = (p_note ? p_note : "");

    /*
    bool v_saved = false;
    bool v_applied = false;

    bool v_ok = (_cfg && _cfg->importJson(v_body, v_saved, v_applied));

    if (v_ok && v_saved && _cfg) {
        (void)_cfg->loadAll(_wifi, _e10);
        uint32_t v_m = _wifiDiffMask(v_prevWiFi, _wifi);
        if (v_m != 0) _markNeedReboot(v_m);
    }

    if (v_ok && v_saved && p_applyAfterSave && _applyFn) {
        v_applied = _applyFn(_applyCtx);
    }

    JsonDocument v_doc;
    v_doc["saved"] = v_saved;
    v_doc["applied"] = v_applied;
    v_doc["note"] = (p_note ? p_note : "");
    */

    if (v_ok) {
        _markLastApply(true, (p_src ? p_src : "save"), "config_save");
        _sendOk(req, "config_save", "", &v_doc, 200);
    } else {
        _markLastApply(false, (p_src ? p_src : "save"), "config_save_failed");
        _sendErr(req, "config_save_failed", "Save/import failed.", &v_doc);
    }
}

// =====================================================
// E10 status fill
// =====================================================
void CL_W10_WebConfig::_fillE10StatusFromSnapshot(JsonObject e, const ST_E10_Status_t& s) {
    e["ble_connected"] = s.ble_connected;
    e["ppt_mode"] = s.ppt_mode;
    e["dpi_level"] = s.dpi_level;
    e["precision_mode"] = s.precision_mode;

    e["fsm_state"] = s.fsm_state;
    e["fsm_sub"] = s.fsm_sub;
    e["btn_mask"] = s.btn_mask;

    e["safe_mode"] = s.safe_mode;

    JsonObject gate = e["gate"].to<JsonObject>();
    gate["ota_guard"] = s.ota_guard;
    gate["ota_guard_count"] = (uint32_t)s.ota_guard_count;
    gate["ota_guard_uptime_ms"] = (uint32_t)s.ota_guard_uptime_ms;

    JsonObject h = e["health"].to<JsonObject>();
    h["state"] = s.health;
    h["score"] = s.health_score;

    JsonObject gyro = e["gyro"].to<JsonObject>();
    gyro["bias_x"] = s.gyro_bias_x;
    gyro["bias_y"] = s.gyro_bias_y;
    gyro["bias_z"] = s.gyro_bias_z;
    gyro["rms"] = s.gyro_rms;

    e["cursor_rms"] = s.cursor_rms;
    e["temp_c"] = s.temp_c;

    JsonObject samp = e["sampling"].to<JsonObject>();
    samp["ms_target"] = (uint32_t)s.sampling_ms_target;
    samp["ms_avg"] = s.sampling_ms_avg;

    JsonObject i2c = e["i2c"].to<JsonObject>();
    i2c["recover_count"] = (uint32_t)s.i2c_recover_count;
    i2c["recover_last_ok"] = s.i2c_recover_last_ok;

    JsonObject err = e["err"].to<JsonObject>();
    err["mpu_nan"] = (uint32_t)s.err_mpu_nan;
    err["mutex_miss"] = (uint32_t)s.err_mutex_miss;
    err["task_overrun"] = (uint32_t)s.err_task_overrun;

    JsonObject an = e["anomaly"].to<JsonObject>();
    an["spike_count_10s"] = (uint32_t)s.spike_count_10s;
    an["consecutive_fail"] = (uint32_t)s.consecutive_fail;
    an["consecutive_recover_fail"] = (uint32_t)s.consecutive_recover_fail;

    JsonObject obs = e["obs"].to<JsonObject>();
    obs["task_stack_sensor_min_words"] = (uint32_t)s.task_stack_sensor_min_words;
    obs["task_stack_comm_min_words"] = (uint32_t)s.task_stack_comm_min_words;
    obs["sensor_dt_max_ms"] = s.sensor_dt_max_ms;
    obs["sensor_overrun_count"] = (uint32_t)s.sensor_overrun_count;
    obs["comm_dt_avg_ms"] = s.comm_dt_avg_ms;
    obs["comm_dt_max_ms"] = s.comm_dt_max_ms;
    obs["comm_overrun_count"] = (uint32_t)s.comm_overrun_count;
    obs["failsafe_release_count"] = (uint32_t)s.failsafe_release_count;

    JsonArray hist = e["err_hist"].to<JsonArray>();
    for (uint8_t i = 0; i < s.err_hist_n; i++) {
        JsonObject o = hist.add<JsonObject>();
        o["ts_ms"] = (uint32_t)s.err_hist[i].ts_ms;
        o["code"] = (uint32_t)s.err_hist[i].code;
        o["value"] = (uint32_t)s.err_hist[i].value;
    }
}

void CL_W10_WebConfig::_fillE10Status(JsonObject e, ST_W10_E10If_t* e10if) {
    if (!e10if) return;
    ST_E10_Status_t s;
    if (!e10if->getStatus || !e10if->getStatus(e10if->ctx, &s)) return;
    _fillE10StatusFromSnapshot(e, s);
}

// =====================================================
// /api/status
// =====================================================
void CL_W10_WebConfig::_apiStatus(AsyncWebServerRequest* req) {
    JsonDocument v_doc;
    const uint32_t v_uptime = (uint32_t)millis();
    const uint32_t v_heapFree = (uint32_t)ESP.getFreeHeap();
    const uint32_t v_heapMin = (uint32_t)ESP.getMinFreeHeap();
    const uint32_t v_heapMaxA = (uint32_t)ESP.getMaxAllocHeap();

    bool v_flat = true;
    bool v_compact = false;
    if (req && req->hasParam("flat")) {
        const String v = req->getParam("flat")->value();
        if (v == "0" || v == "false") v_flat = false;
    }
    if (req && req->hasParam("compact")) {
        const String v = req->getParam("compact")->value();
        if (v == "1" || v == "true") v_compact = true;
    }

    v_doc["uptime_ms"] = v_uptime;
    v_doc["heap_free"] = v_heapFree;
    v_doc["heap_min_free"] = v_heapMin;
    v_doc["heap_max_alloc"] = v_heapMaxA;
    v_doc["api_ver"] = (uint16_t)G_W10_API_VER;

    JsonObject sys = v_doc["sys"].to<JsonObject>();
    sys["uptime_ms"] = v_uptime;
    sys["api_ver"] = (uint16_t)G_W10_API_VER;

    JsonObject mem = v_doc["mem"].to<JsonObject>();
    mem["heap_free"] = v_heapFree;
    mem["heap_min_free"] = v_heapMin;
    mem["heap_max_alloc"] = v_heapMaxA;

    JsonObject feat = v_doc["features"].to<JsonObject>();
    feat["etag_config"] = true;
    feat["reboot_api"] = true;
    feat["safe_mode_policy"] = true;
    feat["ota_guard"] = true;
    feat["e10_observability"] = true;

    JsonObject diag = v_doc["diag"].to<JsonObject>();
    diag["body_too_large"] = (uint32_t)_cnt_body_too_large;
    diag["body_no_slot"] = (uint32_t)_cnt_body_no_slot;
    diag["json_bad"] = (uint32_t)_cnt_json_bad;
    diag["safe_blocked"] = (uint32_t)_cnt_safe_blocked;
    diag["ota_blocked"] = (uint32_t)_cnt_ota_blocked;

    JsonObject net = v_doc["net"].to<JsonObject>();
    net["mode"] = (WiFi.getMode() == WIFI_AP) ? "AP" : "STA";
    net["ip"] = (WiFi.getMode() == WIFI_AP) ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
    net["ssid"] = (WiFi.getMode() == WIFI_AP) ? String(_wifi.ap_ssid) : WiFi.SSID();
    net["mdns"] = String(_wifi.mdns_host) + ".local";

    bool v_otaGuard = false;
    ST_W10_E10If_t* e10if = _e10if;
    if (e10if) {
        ST_E10_Status_t s;
        if (e10if->getStatus && e10if->getStatus(e10if->ctx, &s)) {
            JsonObject e = v_doc["e10"].to<JsonObject>();
            _fillE10StatusFromSnapshot(e, s);
            v_otaGuard = s.ota_guard;
        }
    }

    JsonObject ota = v_doc["ota"].to<JsonObject>();
    ota["in_progress"] = _otaInProgress;
    ota["total"] = (uint32_t)_otaTotal;
    ota["written"] = (uint32_t)_otaWritten;
    ota["ok"] = _otaOk;
    ota["err"] = _otaErr;

    if (_cfg) {
        ST_C10_BootState_t bs;
        _cfg->getBootState(bs);
        JsonObject b = v_doc["boot"].to<JsonObject>();
        b["safe_mode"] = bs.safe_mode;
        b["fail_count"] = bs.fail_count;
        b["pending"] = bs.pending;
        b["last_reset_reason"] = bs.last_reset_reason;

        JsonObject pol = v_doc["policy"].to<JsonObject>();
        pol["reboot_required"] = _needReboot;
        pol["reboot_reason_mask"] = (uint32_t)_needRebootMask;
        pol["reboot_reasons"] = _rebootReasonsString(_needRebootMask);
        pol["safe_mode_api_limited"] = bs.safe_mode;
        pol["ota_upload_blocked"] = v_otaGuard;

        uint32_t v_etag = 0;
        size_t v_cfgSize = 0;
        bool v_etagOk = _cfg->getConfigEtag(v_etag, &v_cfgSize);
        JsonObject cfg = v_doc["config"].to<JsonObject>();
        cfg["ver"] = (uint16_t)G_C10_CFG_VER;
        cfg["etag_ok"] = v_etagOk;
        cfg["etag"] = (uint32_t)v_etag;
        cfg["size"] = (uint32_t)v_cfgSize;

        cfg["last_apply_ok"] = _lastApplyOk;
        cfg["last_apply_ms"] = _lastApplyMs;
        cfg["last_apply_age_ms"] = (_lastApplyMs == 0) ? 0 : (uint32_t)(v_uptime - _lastApplyMs);
        cfg["last_apply_code"] = _lastApplyCode;
        cfg["last_apply_src"] = _lastApplySrc;
    }

    JsonObject groups = v_doc["groups"].to<JsonObject>();
    {
        JsonObject gSys = groups["sys"].to<JsonObject>();
        gSys["uptime_ms"] = v_uptime;
        gSys["api_ver"] = (uint16_t)G_W10_API_VER;

        JsonObject gMem = groups["mem"].to<JsonObject>();
        gMem["heap_free"] = v_heapFree;
        gMem["heap_min_free"] = v_heapMin;
        gMem["heap_max_alloc"] = v_heapMaxA;

        JsonObject gNet = groups["net"].to<JsonObject>();
        gNet["mode"] = net["mode"];
        gNet["ip"] = net["ip"];
        gNet["ssid"] = net["ssid"];
        gNet["mdns"] = net["mdns"];

        JsonObject gDiag = groups["diag"].to<JsonObject>();
        gDiag["body_too_large"] = diag["body_too_large"];
        gDiag["body_no_slot"] = diag["body_no_slot"];
        gDiag["json_bad"] = diag["json_bad"];
        gDiag["safe_blocked"] = diag["safe_blocked"];
        gDiag["ota_blocked"] = diag["ota_blocked"];

        JsonObject gFeat = groups["features"].to<JsonObject>();
        gFeat["etag_config"] = feat["etag_config"];
        gFeat["reboot_api"] = feat["reboot_api"];
        gFeat["safe_mode_policy"] = feat["safe_mode_policy"];
        gFeat["ota_guard"] = feat["ota_guard"];
        gFeat["e10_observability"] = feat["e10_observability"];

        if (_cfg) {
            groups["policy_ref"] = "/policy";

            JsonObject gBoot = groups["boot"].to<JsonObject>();
            JsonObject boot = v_doc["boot"].as<JsonObject>();
            gBoot["safe_mode"] = boot["safe_mode"];
            gBoot["fail_count"] = boot["fail_count"];
            gBoot["pending"] = boot["pending"];
            gBoot["last_reset_reason"] = boot["last_reset_reason"];

            JsonObject gCfg = groups["config"].to<JsonObject>();
            JsonObject cfg = v_doc["config"].as<JsonObject>();
            gCfg["ver"] = cfg["ver"];
            gCfg["etag_ok"] = cfg["etag_ok"];
            gCfg["etag"] = cfg["etag"];
            gCfg["size"] = cfg["size"];
            gCfg["last_apply_ok"] = cfg["last_apply_ok"];
            gCfg["last_apply_ms"] = cfg["last_apply_ms"];
            gCfg["last_apply_age_ms"] = cfg["last_apply_age_ms"];
            gCfg["last_apply_code"] = cfg["last_apply_code"];
            gCfg["last_apply_src"] = cfg["last_apply_src"];
        }

        JsonObject gOta = groups["ota"].to<JsonObject>();
        gOta["in_progress"] = ota["in_progress"];
        gOta["total"] = ota["total"];
        gOta["written"] = ota["written"];
        gOta["ok"] = ota["ok"];
        gOta["err"] = ota["err"];

        JsonVariant e10v = v_doc["e10"];
        if (!e10v.isNull()) {
            JsonObject gE10 = groups["e10"].to<JsonObject>();
            JsonObject e10 = e10v.as<JsonObject>();

            JsonVariant v;
            v = e10["ble_connected"];   if (!v.isNull()) gE10["ble_connected"] = v;
            v = e10["ppt_mode"];        if (!v.isNull()) gE10["ppt_mode"] = v;
            v = e10["dpi_level"];       if (!v.isNull()) gE10["dpi_level"] = v;
            v = e10["precision_mode"];  if (!v.isNull()) gE10["precision_mode"] = v;
            v = e10["fsm_state"];       if (!v.isNull()) gE10["fsm_state"] = v;
            v = e10["btn_mask"];        if (!v.isNull()) gE10["btn_mask"] = v;

            v = e10["safe_mode"];       if (!v.isNull()) gE10["safe_mode"] = v;

            JsonVariant gate = e10["gate"];
            if (!gate.isNull()) {
                JsonObject gg = gE10["gate"].to<JsonObject>();
                JsonObject gsrc = gate.as<JsonObject>();
                JsonVariant gv;
                gv = gsrc["ota_guard"];           if (!gv.isNull()) gg["ota_guard"] = gv;
                gv = gsrc["ota_guard_count"];     if (!gv.isNull()) gg["ota_guard_count"] = gv;
                gv = gsrc["ota_guard_uptime_ms"]; if (!gv.isNull()) gg["ota_guard_uptime_ms"] = gv;
            }

            JsonVariant obs = e10["obs"];
            if (!obs.isNull()) {
                JsonObject go = gE10["obs"].to<JsonObject>();
                JsonObject os = obs.as<JsonObject>();
                JsonVariant ov;
                ov = os["task_stack_sensor_min_words"]; if (!ov.isNull()) go["task_stack_sensor_min_words"] = ov;
                ov = os["task_stack_comm_min_words"];   if (!ov.isNull()) go["task_stack_comm_min_words"] = ov;
                ov = os["sensor_dt_max_ms"];            if (!ov.isNull()) go["sensor_dt_max_ms"] = ov;
                ov = os["sensor_overrun_count"];        if (!ov.isNull()) go["sensor_overrun_count"] = ov;
                ov = os["comm_dt_avg_ms"];              if (!ov.isNull()) go["comm_dt_avg_ms"] = ov;
                ov = os["comm_dt_max_ms"];              if (!ov.isNull()) go["comm_dt_max_ms"] = ov;
                ov = os["comm_overrun_count"];          if (!ov.isNull()) go["comm_overrun_count"] = ov;
                ov = os["failsafe_release_count"];      if (!ov.isNull()) go["failsafe_release_count"] = ov;
            }
        }
    }

    if (!v_flat || v_compact) {
        v_doc.remove("uptime_ms");
        v_doc.remove("heap_free");
        v_doc.remove("heap_min_free");
        v_doc.remove("heap_max_alloc");
        v_doc.remove("api_ver");
    }
    if (v_compact) {
        v_doc.remove("sys");
        v_doc.remove("mem");
        v_doc.remove("net");
        v_doc.remove("diag");
        v_doc.remove("ota");
        v_doc.remove("boot");
        v_doc.remove("config");
        v_doc.remove("e10");
        v_doc.remove("features");
    }

    _sendOk(req, "status", "", &v_doc, 200);
}

// =====================================================
// /api/diag
// =====================================================
void CL_W10_WebConfig::_apiDiag(AsyncWebServerRequest* req) {
    JsonDocument v_doc;

    JsonObject diag = v_doc["diag"].to<JsonObject>();
    diag["body_too_large"] = (uint32_t)_cnt_body_too_large;
    diag["body_no_slot"] = (uint32_t)_cnt_body_no_slot;
    diag["json_bad"] = (uint32_t)_cnt_json_bad;
    diag["safe_blocked"] = (uint32_t)_cnt_safe_blocked;
    diag["ota_blocked"] = (uint32_t)_cnt_ota_blocked;

    JsonObject apply = v_doc["last_apply"].to<JsonObject>();
    const uint32_t v_uptime = (uint32_t)millis();
    apply["ok"] = _lastApplyOk;
    apply["ms"] = (uint32_t)_lastApplyMs;
    apply["age_ms"] = (_lastApplyMs == 0) ? (uint32_t)0 : (uint32_t)((uint32_t)v_uptime - (uint32_t)_lastApplyMs);
    apply["code"] = _lastApplyCode;
    apply["src"] = _lastApplySrc;

    JsonArray ev = v_doc["events"].to<JsonArray>();
    if (_diagEvtCount > 0) {
        const uint8_t start = (uint8_t)((_diagEvtHead + G_W10_DIAG_EVT_MAX - _diagEvtCount) % G_W10_DIAG_EVT_MAX);
        for (uint8_t i = 0; i < _diagEvtCount; i++) {
            const uint8_t pos = (uint8_t)((start + i) % G_W10_DIAG_EVT_MAX);
            JsonObject e = ev.add<JsonObject>();
            e["ms"] = (uint32_t)_diagEvt[pos].ms;
            e["code"] = _diagEvt[pos].code;
        }
    }
    _sendOk(req, "diag", "", &v_doc, 200);
}

void CL_W10_WebConfig::_apiDiagClear(AsyncWebServerRequest* req) {
    (void)req;

    _cnt_body_too_large = 0;
    _cnt_body_no_slot = 0;
    _cnt_json_bad = 0;
    _cnt_safe_blocked = 0;
    _cnt_ota_blocked = 0;

    _diagEvtHead = 0;
    _diagEvtCount = 0;
    memset(_diagEvt, 0, sizeof(_diagEvt));

    JsonDocument v_doc;
    v_doc["cleared"] = true;
    _sendOk(req, "diag_clear", "", &v_doc, 200);
}

// =====================================================
// /api/keycodes
// =====================================================
const char* CL_W10_WebConfig::_kbName(uint16_t p_code) {
    switch (p_code) {
        case 0x00: return "None";
        case 0x04: return "A";
        case 0x05: return "B";
        case 0x06: return "C";
        case 0x07: return "D";
        case 0x08: return "E";
        case 0x09: return "F";
        case 0x0A: return "G";
        case 0x0B: return "H";
        case 0x0C: return "I";
        case 0x0D: return "J";
        case 0x0E: return "K";
        case 0x0F: return "L";
        case 0x10: return "M";
        case 0x11: return "N";
        case 0x12: return "O";
        case 0x13: return "P";
        case 0x14: return "Q";
        case 0x15: return "R";
        case 0x16: return "S";
        case 0x17: return "T";
        case 0x18: return "U";
        case 0x19: return "V";
        case 0x1A: return "W";
        case 0x1B: return "X";
        case 0x1C: return "Y";
        case 0x1D: return "Z";
        case 0x1E: return "1";
        case 0x1F: return "2";
        case 0x20: return "3";
        case 0x21: return "4";
        case 0x22: return "5";
        case 0x23: return "6";
        case 0x24: return "7";
        case 0x25: return "8";
        case 0x26: return "9";
        case 0x27: return "0";
        case 0x28: return "Enter";
        case 0x29: return "Esc";
        case 0x2A: return "Backspace";
        case 0x2B: return "Tab";
        case 0x2C: return "Space";
        case 0x3A: return "F1";
        case 0x3B: return "F2";
        case 0x3C: return "F3";
        case 0x3D: return "F4";
        case 0x3E: return "F5";
        case 0x3F: return "F6";
        case 0x40: return "F7";
        case 0x41: return "F8";
        case 0x42: return "F9";
        case 0x43: return "F10";
        case 0x44: return "F11";
        case 0x45: return "F12";
        case 0x4B: return "PageUp";
        case 0x4E: return "PageDown";
        case 0x4F: return "Right";
        case 0x50: return "Left";
        case 0x51: return "Down";
        case 0x52: return "Up";
        default: return nullptr;
    }
}

const char* CL_W10_WebConfig::_precModeName(uint8_t p_mode) {
    switch (p_mode) {
        case (uint8_t)EN_C10_E10_PREC_OFF:  return "OFF";
        case (uint8_t)EN_C10_E10_PREC_LOW:  return "LOW";
        case (uint8_t)EN_C10_E10_PREC_MED:  return "MED";
        case (uint8_t)EN_C10_E10_PREC_HIGH: return "HIGH";
        case (uint8_t)EN_C10_E10_PREC_PPT:  return "PPT";
        default: return "UNKNOWN";
    }
}

void CL_W10_WebConfig::apiKeycodes(AsyncWebServerRequest* req) {
    JsonDocument d;

    JsonArray mods = d["mods"].to<JsonArray>();
    for (size_t i = 0; i < sizeof(G_W10_MODS) / sizeof(G_W10_MODS[0]); i++) {
        JsonObject o = mods.add<JsonObject>();
        o["name"] = G_W10_MODS[i].name;
        o["mask"] = G_W10_MODS[i].mask;
    }

    JsonArray kb = d["kb"].to<JsonArray>();
    char nameBuf[8];
    for (uint16_t code = 0; code <= 0xE7; code++) {
        const char* n = _kbName(code);
        if (!n) {
            snprintf(nameBuf, sizeof(nameBuf), "0x%02X", (unsigned)code);
            n = nameBuf;
        }
        JsonObject o = kb.add<JsonObject>();
        o["name"] = n;
        o["code"] = code;
    }

    JsonArray con = d["consumer"].to<JsonArray>();
    for (size_t i = 0; i < sizeof(G_W10_CONSUMER) / sizeof(G_W10_CONSUMER[0]); i++) {
        JsonObject o = con.add<JsonObject>();
        o["name"] = G_W10_CONSUMER[i].name;
        o["mask"] = (uint32_t)G_W10_CONSUMER[i].mask;
    }

    JsonArray pm = d["precision_modes"].to<JsonArray>();
    for (uint8_t m = 0; m < (uint8_t)EN_C10_E10_PREC_MAX; m++) {
        JsonObject o = pm.add<JsonObject>();
        const char* n = _precModeName(m);
        if (!n) n = "unknown";
        o["name"] = n;
        o["value"] = m;
    }

    d["note"] = "mods mask == HID modifier byte. kb=usage-id(0x07), consumer=32-bit mask. precision_modes owned by C10.";
    _sendOk(req, "keycodes", "", &d, 200);
}

// =====================================================
// /api/config (GET)  - raw/envelope 모두 Stream 적용
// =====================================================
void CL_W10_WebConfig::apiGetConfig(AsyncWebServerRequest* req) {
    const bool v_envelope = _wantsEnvelope(req);

    uint32_t v_etag = 0;
    size_t v_size = 0;
    bool v_hasEtag = (_cfg && _cfg->getConfigEtag(v_etag, &v_size));

    if (v_hasEtag) {
        if (_ifNoneMatchHit(req, v_etag)) {
            AsyncWebServerResponse* res304 = req->beginResponse(304);

            char v_tag[16];
            memset(v_tag, 0, sizeof(v_tag));
            _formatEtagQuoted(v_etag, v_tag, sizeof(v_tag));

            // 304에도 no-store 적용(요구사항)
            res304->addHeader("Cache-Control", G_W10_CACHE_NOSTORE);
            res304->addHeader("ETag", v_tag);
            req->send(res304);
            return;
        }
    }
    /*
    if (v_hasEtag) {
        if (req->hasHeader("If-None-Match")) {
            const AsyncWebHeader* h = req->getHeader("If-None-Match");
            if (h) {
                String v_inm = h->value();
                char v_tag[16];
                snprintf(v_tag, sizeof(v_tag), "%08X", (unsigned int)v_etag);
                if (v_inm == String(v_tag)) {
                    AsyncWebServerResponse* res304 = req->beginResponse(304);
                    res304->addHeader("ETag", v_tag);
                    res304->addHeader("Cache-Control", G_W10_CACHE_NOCACHE);
                    req->send(res304);
                    return;
                }
            }
        }
    }
    */

    String json;
    if (!_cfg || !_cfg->exportJson(json)) {
        _sendErr(req, "config_get_failed", "Failed to export config.");
        return;
    }

    if (v_envelope) {
        AsyncResponseStream* res = req->beginResponseStream("application/json");
        res->setCode(200);
        res->addHeader("Cache-Control", G_W10_CACHE_NOSTORE);
        
        
        if (v_hasEtag) {
            char v_tag[16];
            memset(v_tag, 0, sizeof(v_tag));
            _formatEtagQuoted(v_etag, v_tag, sizeof(v_tag));
            res->addHeader("ETag", v_tag);
            res->addHeader("X-Config-Size", String((unsigned int)v_size));
        }
        /*
        if (v_hasEtag) {
            char v_tag[16];
            snprintf(v_tag, sizeof(v_tag), "%08X", (unsigned int)v_etag);
            res->addHeader("ETag", v_tag);
            res->addHeader("X-Config-Size", String((unsigned int)v_size));
        }
        */

        res->print("{\"ok\":true,\"code\":\"config_get\",\"msg\":\"\",\"data\":{");
        if (v_hasEtag) {
            char v_tag2[16];
            snprintf(v_tag2, sizeof(v_tag2), "%08X", (unsigned int)v_etag);
            res->print("\"etag\":");
            _resPrintJsonString(res, v_tag2);   // 따옴표 포함 문자열 리터럴이 출력됨
            res->print(",");
            
            res->print("\"size\":");
            res->print((unsigned int)v_size);
            res->print(",");
        }
        res->print("\"config\":");
        res->print(json);
        res->print("}}");
        req->send(res);
        return;
    }

    // raw JSON도 stream
    AsyncResponseStream* res = req->beginResponseStream("application/json");
    res->setCode(200);
    res->addHeader("Cache-Control", G_W10_CACHE_NOSTORE);
    if (v_hasEtag) {
        char v_tag[16];
        memset(v_tag, 0, sizeof(v_tag));
        _formatEtagQuoted(v_etag, v_tag, sizeof(v_tag));
        res->addHeader("ETag", v_tag);
        res->addHeader("X-Config-Size", String((unsigned int)v_size));
    }
    /*
    if (v_hasEtag) {
        char v_tag[16];
        snprintf(v_tag, sizeof(v_tag), "%08X", (unsigned int)v_etag);
        res->addHeader("ETag", v_tag);
        res->addHeader("X-Config-Size", String((unsigned int)v_size));
    }
    */
    res->print(json);
    req->send(res);
}

void CL_W10_WebConfig::apiConfigSave(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    _apiConfigSaveImportCommon(req, data, len, index, total,
        "save",
        "WiFi changes require reboot.",
        true);
}

void CL_W10_WebConfig::apiConfigApply(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    String v_body;
    if (!_collectBodyOrReply(req, data, len, index, total, v_body)) return;

    bool v_ok = true;
    bool v_applied = false;

    if (!_cfg) {
        v_ok = false;
    } else {
        ST_C10_E10Config_t v_e;
        v_ok = _cfg->buildPatchedE10(v_body, v_e, true);

        ST_W10_E10If_t* v_e10if = _e10if;
        if (v_ok && v_e10if && v_e10if->applyRuntimeE10) {
            v_applied = v_e10if->applyRuntimeE10(v_e10if->ctx, &v_e);
        }
    }

    JsonDocument v_doc;
    v_doc["applied"] = v_applied;
    v_doc["note"] = "apply-only: not saved. WiFi fields are ignored (E10 only).";

    if (v_ok) {
        _markLastApply(true, "apply", "config_apply");
        _sendOk(req, "config_apply", "", &v_doc, 200);
    } else {
        _markLastApply(false, "apply", "config_apply_failed");
        _sendErr(req, "config_apply_failed", "Apply failed.", &v_doc);
    }
}

void CL_W10_WebConfig::apiExport(AsyncWebServerRequest* req) {
    const bool v_envelope = _wantsEnvelope(req);

    bool v_pretty = false;
    if (req && req->hasParam("pretty")) {
        const AsyncWebParameter* p = req->getParam("pretty");
        if (p && p->value() == "1") v_pretty = true;
    }
    if (req && req->hasParam("format")) {
        const AsyncWebParameter* p = req->getParam("format");
        if (p) {
            const String v = p->value();
            if (v == "pretty") v_pretty = true;
            else if (v == "minified") v_pretty = false;
        }
    }

    bool v_attach = true;
    if (req && req->hasParam("attachment")) {
        const AsyncWebParameter* p = req->getParam("attachment");
        if (p && p->value() == "0") v_attach = false;
    }
    if (req && req->hasParam("download")) {
        const AsyncWebParameter* p = req->getParam("download");
        if (p && p->value() == "0") v_attach = false;
    }

    String v_filename;
    if (req && req->hasParam("filename")) {
        const AsyncWebParameter* p = req->getParam("filename");
        if (p) v_filename = p->value();
    }
    if (v_filename.length() == 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "config_%u.json", (unsigned int)G_C10_CFG_VER);
        v_filename = String(buf);
    }

    uint32_t v_etag = 0;
    size_t v_size = 0;
    bool v_hasEtag = (_cfg && _cfg->getConfigEtag(v_etag, &v_size));

    if (v_hasEtag) {
        if (_ifNoneMatchHit(req, v_etag)) {
            AsyncWebServerResponse* res304 = req->beginResponse(304);

            char v_tag[16];
            memset(v_tag, 0, sizeof(v_tag));
            _formatEtagQuoted(v_etag, v_tag, sizeof(v_tag));

            // 304에도 no-store 적용(요구사항)
            res304->addHeader("Cache-Control", G_W10_CACHE_NOSTORE);
            res304->addHeader("ETag", v_tag);
            req->send(res304);
            return;
        }
    }
    
    /*
    if (v_hasEtag && req && req->hasHeader("If-None-Match")) {
        const AsyncWebHeader* h = req->getHeader("If-None-Match");
        if (h) {
            String v_inm = h->value();
            char v_tag[16];
            snprintf(v_tag, sizeof(v_tag), "%08X", (unsigned int)v_etag);
            if (v_inm == String(v_tag)) {
                AsyncWebServerResponse* res304 = req->beginResponse(304);
                res304->addHeader("ETag", v_tag);
                res304->addHeader("Cache-Control", G_W10_CACHE_NOCACHE);
                req->send(res304);
                return;
            }
        }
    }
    */

    String json;
    if (!_cfg || !_cfg->exportJson(json)) {
        _sendErr(req, "config_export_failed", "Failed to export config.");
        return;
    }

    if (v_pretty) {
        JsonDocument vd;
        DeserializationError verr = deserializeJson(vd, json);
        if (!verr) {
            String v_out;
            serializeJsonPretty(vd, v_out);
            json = v_out;
        }
    }

    if (v_envelope) {
        AsyncResponseStream* res = req->beginResponseStream("application/json");
        res->setCode(200);
        res->addHeader("Cache-Control", G_W10_CACHE_NOSTORE);

        if (v_hasEtag) {
            char v_tag[16];
            memset(v_tag, 0, sizeof(v_tag));
            _formatEtagQuoted(v_etag, v_tag, sizeof(v_tag));
            res->addHeader("ETag", v_tag);
            res->addHeader("X-Config-Size", String((unsigned int)v_size));
        }
    
        /*
        if (v_hasEtag) {
            char v_tag[16];
            snprintf(v_tag, sizeof(v_tag), "%08X", (unsigned int)v_etag);
            res->addHeader("ETag", v_tag);
            res->addHeader("X-Config-Size", String((unsigned int)v_size));
        }
        */
        if (v_attach) {
            res->addHeader("Content-Disposition", String("attachment; filename=\"") + v_filename + "\"");
        }

        res->print("{\"ok\":true,\"code\":\"config_export\",\"msg\":\"\",\"data\":{");
        res->print("\"filename\":");
        _resPrintJsonString(res, v_filename);
        res->print(",");

        if (v_hasEtag) {
            char v_tag2[16];
            snprintf(v_tag2, sizeof(v_tag2), "%08X", (unsigned int)v_etag);
            
            res->print("\"etag\":");
            _resPrintJsonString(res, v_tag2);
            res->print(",");
            
            res->print("\"size\":");
            res->print((unsigned int)v_size);
            res->print(",");
        }

        res->print("\"config\":");
        res->print(json);
        res->print("}}");
        req->send(res);
        return;
    }

    // raw JSON도 stream + attachment optional
    AsyncResponseStream* res = req->beginResponseStream("application/json");
    res->setCode(200);
    if (v_attach) {
        res->addHeader("Content-Disposition", String("attachment; filename=\"") + v_filename + "\"");
    }
    res->addHeader("Cache-Control", G_W10_CACHE_NOSTORE);
    
    if (v_hasEtag) {
        char v_tag[16];
        memset(v_tag, 0, sizeof(v_tag));
        _formatEtagQuoted(v_etag, v_tag, sizeof(v_tag));
        res->addHeader("ETag", v_tag);
        res->addHeader("X-Config-Size", String((unsigned int)v_size));
    }
    /*
    if (v_hasEtag) {
        char v_tag[16];
        snprintf(v_tag, sizeof(v_tag), "%08X", (unsigned int)v_etag);
        res->addHeader("ETag", v_tag);
        res->addHeader("X-Config-Size", String((unsigned int)v_size));
    }
    */

    res->print(json);
    req->send(res);
}

void CL_W10_WebConfig::apiImport(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    _apiConfigSaveImportCommon(req, data, len, index, total,
        "import",
        "import: saved and applied (runtime). WiFi changes require reboot.",
        true);
}

void CL_W10_WebConfig::apiRollback(AsyncWebServerRequest* req) {
    bool ok = (_cfg && _cfg->rollbackFromBak());
    bool applied = false;
    if (ok && _applyFn) applied = _applyFn(_applyCtx);

    JsonDocument v_doc;
    v_doc["applied"] = applied;
    v_doc["note"] = "rollback: restored from .bak and applied.";

    if (ok) {
        _markLastApply(true, "rollback", "config_rollback");
        _sendOk(req, "config_rollback", "", &v_doc, 200);
    } else {
        _markLastApply(false, "rollback", "config_rollback_failed");
        _sendErr(req, "config_rollback_failed", "Rollback failed.", &v_doc);
    }
}

// =====================================================
// /api/control
// =====================================================
void CL_W10_WebConfig::apiControl(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    if (_isSafeMode() && !_isApiAllowedInSafeMode(req->url().c_str())) {
        _cnt_safe_blocked++;
        _diagPush("safe_mode_blocked");
        _sendErr(req, "safe_mode_blocked", "Blocked in safe mode.");
        return;
    }

    String v_body;
    if (!_collectBodyOrReply(req, data, len, index, total, v_body)) return;

    JsonDocument d;
    DeserializationError err = deserializeJson(d, v_body);
    if (err) {
        _cnt_json_bad++;
        _diagPush("bad_json");
        _sendErr(req, "bad_json", "Invalid JSON.");
        return;
    }

    ST_W10_E10If_t* e10if = _e10if;
    bool ok = true;

    const bool v_snapshot = (!d["snapshot"].isNull()) ? (bool)d["snapshot"] : false;

    const char* v_cmd = nullptr;
    if (!d["cmd"].isNull()) v_cmd = (const char*)d["cmd"];

    if (!e10if) ok = false;

    if (ok && v_cmd && v_cmd[0] != '\0') {
        if (strcmp(v_cmd, "set_ppt") == 0) {
            bool v_en = false;
            if (!d["enable"].isNull()) v_en = (bool)d["enable"];
            ok = ok && (e10if && e10if->setPptMode ? e10if->setPptMode(e10if->ctx, v_en) : false);

        } else if (strcmp(v_cmd, "set_dpi") == 0) {
            uint8_t v_lv = 2;
            if (!d["level"].isNull()) v_lv = (uint8_t)d["level"];
            ok = ok && (e10if && e10if->setDpiLevel ? e10if->setDpiLevel(e10if->ctx, v_lv) : false);

        } else if (strcmp(v_cmd, "set_precision") == 0) {
            uint8_t v_mode = (uint8_t)EN_C10_E10_PREC_OFF;
            if (!d["mode"].isNull()) v_mode = (uint8_t)d["mode"];

            if (v_mode >= (uint8_t)EN_C10_E10_PREC_MAX) {
                ok = false;
            } else {
                ok = ok && (e10if && e10if->setPrecisionMode ? e10if->setPrecisionMode(e10if->ctx, v_mode) : false);
            }

        } else if (strcmp(v_cmd, "force_release") == 0) {
            ok = ok && (e10if && e10if->forceReleaseButtons ? e10if->forceReleaseButtons(e10if->ctx) : false);

        } else if (strcmp(v_cmd, "gyro_calib") == 0) {
            ok = ok && (e10if && e10if->requestGyroCalibration ? e10if->requestGyroCalibration(e10if->ctx) : false);

        } else if (strcmp(v_cmd, "i2c_recover") == 0) {
            ok = ok && (e10if && e10if->requestI2CRecover ? e10if->requestI2CRecover(e10if->ctx) : false);

        } else if (strcmp(v_cmd, "clear_diag") == 0) {
            ok = ok && (e10if && e10if->clearDiagnostics ? e10if->clearDiagnostics(e10if->ctx) : false);

        } else if (strcmp(v_cmd, "set_safe_mode") == 0) {
            bool v_en = false;
            if (!d["enable"].isNull()) v_en = (bool)d["enable"];
            ok = ok && (e10if && e10if->setSafeMode ? e10if->setSafeMode(e10if->ctx, v_en) : false);

        } else if (strcmp(v_cmd, "set_ota_guard") == 0) {
            bool v_en = false;
            if (!d["enable"].isNull()) v_en = (bool)d["enable"];
            ok = ok && (e10if && e10if->setOtaGuard ? e10if->setOtaGuard(e10if->ctx, v_en) : false);

        } else {
            ok = false;
        }

    } else if (ok) {
        if (!d["ppt_mode"].isNull()) ok = ok && (e10if && e10if->setPptMode ? e10if->setPptMode(e10if->ctx, (bool)d["ppt_mode"]) : false);
        if (!d["dpi_level"].isNull()) ok = ok && (e10if && e10if->setDpiLevel ? e10if->setDpiLevel(e10if->ctx, (uint8_t)d["dpi_level"]) : false);

        if (!d["precision_mode"].isNull()) {
            uint8_t v_mode = (uint8_t)d["precision_mode"];
            if (v_mode >= (uint8_t)EN_C10_E10_PREC_MAX) ok = false;
            else ok = ok && (e10if && e10if->setPrecisionMode ? e10if->setPrecisionMode(e10if->ctx, v_mode) : false);
        }
        if (!d["safe_mode"].isNull()) ok = ok && (e10if && e10if->setSafeMode ? e10if->setSafeMode(e10if->ctx, (bool)d["safe_mode"]) : false);
    }

    JsonDocument v_doc;
    v_doc["cmd"] = (v_cmd ? v_cmd : "");
    if (v_snapshot && e10if) {
        JsonObject e = v_doc["e10"].to<JsonObject>();
        _fillE10Status(e, e10if);
    }
    if (ok) _sendOk(req, "control", "", &v_doc, 200);
    else _sendErr(req, "control_failed", "Control failed.", &v_doc);
}

// =====================================================
// /api/ppt
// =====================================================
void CL_W10_WebConfig::apiGetPpt(AsyncWebServerRequest* req) {
    if (_cfg) (void)_cfg->loadAll(_wifi, _e10);

    JsonDocument d;
    JsonObject map = d["map"].to<JsonObject>();

    auto put = [&](const char* n, const ST_C10_PptKey2_t& k) {
        JsonObject o = map[n].to<JsonObject>();
        o["page"] = (k.page == (uint8_t)EN_C10_KEYPAGE_CONSUMER) ? "consumer" : "kb";
        o["mod"] = k.mod;
        o["code"] = k.code;
    };

    put("start", _e10.ppt2_start);
    put("exit", _e10.ppt2_exit);
    put("next", _e10.ppt2_next);
    put("prev", _e10.ppt2_prev);
    put("black", _e10.ppt2_black);
    put("laser", _e10.ppt2_laser);

    _sendOk(req, "ppt", "", &d, 200);
}

void CL_W10_WebConfig::apiPostPpt(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    if (_isSafeMode() && !_isApiAllowedInSafeMode(req->url().c_str())) {
        _cnt_safe_blocked++;
        _diagPush("safe_mode_blocked");
        _sendErr(req, "safe_mode_blocked", "Blocked in safe mode.");
        return;
    }

    String v_body;
    if (!_collectBodyOrReply(req, data, len, index, total, v_body)) return;

    JsonDocument d;
    DeserializationError err = deserializeJson(d, v_body);
    if (err) {
        _cnt_json_bad++;
        _diagPush("bad_json");
        _sendErr(req, "bad_json", "Invalid JSON.");
        return;
    }

    bool save = true;
    if (!d["save"].isNull()) save = (bool)d["save"];

    JsonVariant map = d["map"];
    if (map.isNull()) {
        _sendErr(req, "no_map", "Missing map field.");
        return;
    }

    bool ok = true;
    bool saved = false;
    bool applied = false;

    if (!_cfg) {
        ok = false;
    } else {
        ST_C10_WiFiConfig_t w;
        ST_C10_E10Config_t e;
        _cfg->makeDefaultsWiFi(w);
        _cfg->makeDefaultsE10(e);
        (void)_cfg->loadAll(w, e);

        auto loadK = [&](const char* n, ST_C10_PptKey2_t& k) {
            JsonVariant o = map[n];
            if (o.isNull()) return;

            if (!o["page"].isNull()) {
                const char* s = (const char*)o["page"];
                if (s && strcasecmp(s, "consumer") == 0)
                    k.page = (uint8_t)EN_C10_KEYPAGE_CONSUMER;
                else
                    k.page = (uint8_t)EN_C10_KEYPAGE_KB;
            }
            if (!o["mod"].isNull()) k.mod = (uint8_t)o["mod"];
            if (!o["code"].isNull()) k.code = (uint32_t)o["code"];
        };

        loadK("start", e.ppt2_start);
        loadK("exit", e.ppt2_exit);
        loadK("next", e.ppt2_next);
        loadK("prev", e.ppt2_prev);
        loadK("black", e.ppt2_black);
        loadK("laser", e.ppt2_laser);

        ok = ok && _cfg->validateE10(e);

        if (ok && save) {
            ok = _cfg->saveAll(w, e);
            saved = ok;
        }

        ST_W10_E10If_t* e10if = _e10if;
        if (ok && e10if && e10if->applyRuntimeE10) {
            applied = e10if->applyRuntimeE10(e10if->ctx, &e);
        }
    }

    JsonDocument v_doc;
    v_doc["saved"] = saved;
    v_doc["applied"] = applied;
    if (ok) _sendOk(req, "ppt_set", "", &v_doc, 200);
    else _sendErr(req, "ppt_set_failed", "Failed to update mapping.", &v_doc);
}

void CL_W10_WebConfig::apiPptTest(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    if (_isSafeMode() && !_isApiAllowedInSafeMode(req->url().c_str())) {
        _cnt_safe_blocked++;
        _diagPush("safe_mode_blocked");
        _sendErr(req, "safe_mode_blocked", "Blocked in safe mode.");
        return;
    }

    String v_body;
    if (!_collectBodyOrReply(req, data, len, index, total, v_body)) return;

    JsonDocument d;
    DeserializationError err = deserializeJson(d, v_body);
    if (err) {
        _cnt_json_bad++;
        _diagPush("bad_json");
        _sendErr(req, "bad_json", "Invalid JSON.");
        return;
    }

    uint8_t page = (uint8_t)EN_C10_KEYPAGE_KB;
    uint8_t mod = 0;
    uint32_t code = 0;

    if (!d["page"].isNull()) {
        const char* s = (const char*)d["page"];
        if (s && strcasecmp(s, "consumer") == 0) page = (uint8_t)EN_C10_KEYPAGE_CONSUMER;
    }
    if (!d["mod"].isNull()) mod = (uint8_t)d["mod"];
    if (!d["code"].isNull()) code = (uint32_t)d["code"];

    ST_W10_E10If_t* e10if = _e10if;
    bool ok = (e10if && e10if->testPptKey2 ? e10if->testPptKey2(e10if->ctx, page, mod, code) : false);

    if (ok) _sendOk(req, "ppt_test", "", nullptr, 200);
    else _sendErr(req, "ppt_test_failed", "Test failed.");
}

// =====================================================
// OTA
// =====================================================
void CL_W10_WebConfig::apiOtaUpload(AsyncWebServerRequest* req, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
    if (!_isSafeMode()) {
        ST_W10_E10If_t* e10if = _e10if;
        if (e10if && e10if->getStatus) {
            ST_E10_Status_t s;
            memset(&s, 0, sizeof(s));
            if (e10if->getStatus(e10if->ctx, &s)) {
                if (s.ota_guard) {
                    _cnt_ota_blocked++;
                    _diagPush("ota_guard_blocked");
                
                    // 업로드 콜백에서는 send 금지. 상태만 기록.
                    if (index == 0) {
                        _otaInProgress = false;
                        _otaOk = false;
                        _otaWritten = 0;
                        _otaTotal = (uint32_t)req->contentLength();
                        strlcpy(_otaErr, "ota_guard", sizeof(_otaErr));
                    }
                    return;
                }

                /*
                if (s.ota_guard) {
                    _cnt_ota_blocked++;
                    _diagPush("ota_guard_blocked");
                    if (index == 0) {
                        JsonDocument v_doc;
                        v_doc["reason"] = "ota_guard";
                        _sendErr(req, "ota_guard_blocked", "OTA upload is blocked by guard.", &v_doc);
                    }
                    return;
                }
                */
            }
        }
    }

    (void)filename;

    if (index == 0) {
        if (_otaInProgress) {
            _otaOk = false;
            strlcpy(_otaErr, "busy", sizeof(_otaErr));
            return;
        }

        _otaInProgress = true;
        _otaWritten = 0;
        _otaTotal = (uint32_t)req->contentLength();
        _otaOk = false;
        strlcpy(_otaErr, "in_progress", sizeof(_otaErr));

        size_t sketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (_otaTotal == 0 || _otaTotal > (uint32_t)sketchSpace) {
            _otaOk = false;
            strlcpy(_otaErr, "size_invalid", sizeof(_otaErr));
            _otaInProgress = false;
            return;
        }

        if (!Update.begin(sketchSpace)) {
            _otaOk = false;
            strlcpy(_otaErr, "Update.begin failed", sizeof(_otaErr));
            _otaInProgress = false;
            return;
        }
    }

    if (len) {
        size_t w = Update.write(data, len);
        _otaWritten += (uint32_t)w;
        if (w != len) strlcpy(_otaErr, "Update.write mismatch", sizeof(_otaErr));
    }

    if (final) {
        if (!Update.end(true)) {
            strlcpy(_otaErr, "Update.end failed", sizeof(_otaErr));
            _otaOk = false;
        } else if (Update.hasError()) {
            strlcpy(_otaErr, "Update.hasError", sizeof(_otaErr));
            _otaOk = false;
        } else {
            _otaOk = true;
            strlcpy(_otaErr, "ok", sizeof(_otaErr));
        }
        _otaInProgress = false;
    }
}

void CL_W10_WebConfig::apiOtaStatus(AsyncWebServerRequest* req) {
    JsonDocument v_doc;
    v_doc["in_progress"] = _otaInProgress;
    v_doc["total"] = (uint32_t)_otaTotal;
    v_doc["written"] = (uint32_t)_otaWritten;
    v_doc["ok"] = _otaOk;
    v_doc["err"] = _otaErr;

    _sendOk(req, "ota_status", "", &v_doc, 200);
}

// =====================================================
// SafeBoot / FactoryReset
// =====================================================
void CL_W10_WebConfig::apiSafeBootGet(AsyncWebServerRequest* req) {
    if (_cfg) {
        ST_C10_BootState_t bs;
        _cfg->getBootState(bs);
        JsonDocument v_doc;
        v_doc["safe_mode"] = bs.safe_mode;
        v_doc["fail_count"] = bs.fail_count;
        v_doc["pending"] = bs.pending;
        _sendOk(req, "safeboot", "", &v_doc, 200);
        return;
    }
    _sendErr(req, "no_config", "Config manager not ready.");
}

void CL_W10_WebConfig::apiSafeBootPost(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    String v_body;
    if (!_collectBodyOrReply(req, data, len, index, total, v_body)) return;

    JsonDocument d;
    DeserializationError err = deserializeJson(d, v_body);
    if (err) {
        _cnt_json_bad++;
        _diagPush("bad_json");
        _sendErr(req, "bad_json", "Invalid JSON.");
        return;
    }

    bool v_exit = false;
    if (!d["exit"].isNull()) v_exit = (bool)d["exit"];

    bool ok = false;
    if (_cfg && v_exit) {
        ok = _cfg->clearSafeMode();
    }

    JsonDocument v_doc;
    v_doc["exit"] = v_exit;
    v_doc["note"] = "exit=true -> clears safe_mode and reboots.";
    if (ok) {
        _markLastApply(true, "safeboot", "safeboot_exit");
        _sendOk(req, "safeboot_exit", "", &v_doc, 200);
    } else {
        _markLastApply(false, "safeboot", "safeboot_exit_failed");
        _sendErr(req, "safeboot_exit_failed", "Failed.", &v_doc);
    }

    if (ok && v_exit) {
        delay(150);
        ESP.restart();
    }
}

void CL_W10_WebConfig::apiFactoryReset(AsyncWebServerRequest* req) {
    bool ok = (_cfg && _cfg->factoryReset(true));

    JsonDocument v_doc;
    v_doc["note"] = "Factory reset done. Rebooting...";
    if (ok) {
        _markLastApply(true, "factory", "factory_reset");
        _sendOk(req, "factory_reset", "", &v_doc, 200);
    } else {
        _markLastApply(false, "factory", "factory_reset_failed");
        _sendErr(req, "factory_reset_failed", "Failed.", &v_doc);
    }

    if (ok) {
        delay(200);
        ESP.restart();
    }
}

// =====================================================
// /api/reboot (POST)
// =====================================================
void CL_W10_WebConfig::_taskReboot(void* p_arg) {
    (void)p_arg;
    delay(200);
    ESP.restart();
}

void CL_W10_WebConfig::apiRebootPost(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    String v_body;
    if (!_collectBodyOrReply(req, data, len, index, total, v_body)) return;

    bool v_force = false;
    uint32_t v_mask = 0;
    bool v_hasMask = false;

    if (v_body.length() > 0) {
        JsonDocument in;
        DeserializationError err = deserializeJson(in, v_body);
        if (err) {
            _cnt_json_bad++;
            _diagPush("bad_json");
            _sendErr(req, "bad_json", "Invalid JSON.");
            return;
        }

        JsonVariant v_rm = in["reason_mask"];
        if (!v_rm.isNull()) {
            v_mask = (uint32_t)v_rm.as<uint32_t>();
            v_hasMask = true;
        }

        JsonVariant v_f = in["force"];
        if (!v_f.isNull()) v_force = v_f.as<bool>();
    }

    if (!v_force && !v_hasMask) {
        if (!_needReboot) {
            JsonDocument v_doc;
            v_doc["need_reboot"] = _needReboot;
            v_doc["need_reboot_mask"] = (uint32_t)_needRebootMask;
            v_doc["reboot_reasons"] = _rebootReasonsString(_needRebootMask);
            _sendErr(req, "no_reboot_needed", "Reboot is not required.", &v_doc);
            return;
        }
    }

    if (!v_force && v_hasMask) {
        if (((_needRebootMask & v_mask) != v_mask)) {
            JsonDocument v_doc;
            v_doc["need_reboot"] = _needReboot;
            v_doc["need_reboot_mask"] = (uint32_t)_needRebootMask;
            _sendErr(req, "reason_mask_mismatch", "Reboot is not allowed for the given reason_mask.", &v_doc);
            return;
        }
    }

    JsonDocument v_doc;
    v_doc["need_reboot"] = _needReboot;
    v_doc["need_reboot_mask"] = (uint32_t)_needRebootMask;
    _sendOk(req, "reboot_scheduled", "Reboot scheduled.", &v_doc, 200);

    xTaskCreatePinnedToCore(_taskReboot, "w10_reboot", 2048, nullptr, 1, nullptr, 0);
}

void CL_W10_WebConfig::apiRebootCheck(AsyncWebServerRequest* req) {
    JsonDocument v_doc;
    v_doc["required"] = _needReboot;
    v_doc["mask"] = (uint32_t)_needRebootMask;
    v_doc["reasons"] = _rebootReasonsString(_needRebootMask);

    v_doc["allowed"] = _needReboot;
    v_doc["deny_code"] = _needReboot ? "" : "no_reboot_needed";

    _sendOk(req, "reboot_check", "", &v_doc, 200);
}

