// =======================================================
// File: W10_WebApi_Status_0412.cpp
// =======================================================

/*
 * ------------------------------------------------------
 * 소스명 : W10_WebApi_Status_0412.cpp
 * 모듈약어 : W10
 * 모듈명 : Web Config/Status/UI/OTA Server (Split: Status/Diag/Keycodes)
 * ------------------------------------------------------
 * 기능 요약
 *  - (0316) /api/status, /api/diag, /api/keycodes 및 E10 status fill 분리
 *  - [C-02] /api/status config에 profile_idx/name/count 추가 (프론트엔드 #stProfile)
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

#include "W10_Web_0412.h"

// 붙여넣기 대상:
// - _fillE10StatusFromSnapshot
// - _fillE10Status
// - _apiStatus
// - _apiDiag / _apiDiagClear
// - _kbName / _precModeName / apiKeycodes



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
        char v_profPath[64];
        C10_DEF::makeProfilePath(v_profPath, sizeof(v_profPath), _cfg->getActiveIndex());
        bool v_etagOk = _calcFileEtag32(v_profPath, v_etag, &v_cfgSize);

        // [C-02] profile 메타데이터 (프론트엔드 #stProfile 표시용)
        char v_profName[C10_DEF::PROFILE_NAME_LEN] = {0};
        {
            ST_C10_ProfileConfig_t v_p;
            _cfg->makeDefaultsProfile(_cfg->getActiveIndex(), v_p);
            if (_cfg->loadActiveProfile(v_p)) {
                strlcpy(v_profName, v_p.name, sizeof(v_profName));
            }
        }

        JsonObject cfg = v_doc["config"].to<JsonObject>();
        cfg["ver"] = (uint16_t)G_C10_CFG_VER;
        cfg["etag_ok"] = v_etagOk;
        cfg["etag"] = (uint32_t)v_etag;
        cfg["size"] = (uint32_t)v_cfgSize;

        // [C-02] profile 필드 (신규)
        cfg["profile_idx"]   = (uint8_t)_cfg->getActiveIndex();
        cfg["profile_count"] = (uint8_t)_cfg->getProfileCount();
        cfg["profile_name"]  = v_profName;

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
            // [C-02] profile 필드 (신규)
            gCfg["profile_idx"]   = cfg["profile_idx"];
            gCfg["profile_count"] = cfg["profile_count"];
            gCfg["profile_name"]  = cfg["profile_name"];
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

        // groups.e10 = top-level e10 전체 복사 (compact=1이 top-level을 지우므로 필수)
        JsonVariant e10v = v_doc["e10"];
        if (!e10v.isNull()) {
            JsonObject gE10 = groups["e10"].to<JsonObject>();
            JsonObject e10 = e10v.as<JsonObject>();
            for (JsonPair kv : e10) {
                gE10[kv.key()] = kv.value();
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

    // ---- mods ----
    JsonArray mods = d["mods"].to<JsonArray>();
    for (size_t i = 0; i < sizeof(G_W10_MODS) / sizeof(G_W10_MODS[0]); i++) {
        JsonObject o = mods.add<JsonObject>();
        o["name"] = G_W10_MODS[i].name;
        o["mask"] = G_W10_MODS[i].mask;
    }

    // ---- kb usages ----
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

    // ---- consumer ----
    JsonArray con = d["consumer"].to<JsonArray>();
    for (size_t i = 0; i < sizeof(G_W10_CONSUMER) / sizeof(G_W10_CONSUMER[0]); i++) {
        JsonObject o = con.add<JsonObject>();
        o["name"] = G_W10_CONSUMER[i].name;
        o["mask"] = (uint32_t)G_W10_CONSUMER[i].mask;
    }

    // ---- precision modes ----
    JsonArray pm = d["precision_modes"].to<JsonArray>();
    for (uint8_t m = 0; m < (uint8_t)EN_C10_E10_PREC_MAX; m++) {
        JsonObject o = pm.add<JsonObject>();
        const char* n = _precModeName(m);
        if (!n) n = "unknown";
        o["name"] = n;
        o["value"] = m;
    }

    // ==================================================
    // [D1] v0412 확장
    // ==================================================

    // ---- action_kinds ----
    //   hasP16/hasP32: 어느 파라미터를 쓰는지 (UI 힌트)
    //   p16hint/p32hint: 파라미터 의미
    JsonArray kinds = d["action_kinds"].to<JsonArray>();
    {
        struct ST_K { uint8_t v; const char* n; bool p16; bool p32; const char* h16; const char* h32; };
        static const ST_K G_KINDS[] = {
            { 0, "NONE",             false, false, nullptr,  nullptr },
            { 1, "MOUSE_CLICK",      true,  false, "mask",   nullptr },
            { 2, "MOUSE_HOLD",       true,  false, "mask",   nullptr },
            { 3, "MOUSE_WHEEL",      true,  false, "axis|dir", nullptr },
            { 4, "KB_TAP",           true,  true,  "usage",  "mod" },
            { 5, "KB_COMBO",         false, true,  nullptr,  "mod|u1|u2|u3" },
            { 6, "KB_REPEAT",        true,  true,  "usage",  "mod" },
            { 7, "CONSUMER_TAP",     false, true,  nullptr,  "mask32" },
            { 8, "CONSUMER_REPEAT",  false, true,  nullptr,  "mask32" },
            { 9, "SPECIAL",          true,  false, "special", nullptr },
            { 10, "MACRO",           false, true,  nullptr,  "macroIdx" },
        };
        for (size_t i = 0; i < sizeof(G_KINDS)/sizeof(G_KINDS[0]); i++) {
            JsonObject o = kinds.add<JsonObject>();
            o["value"]   = G_KINDS[i].v;
            o["name"]    = G_KINDS[i].n;
            o["hasP16"]  = G_KINDS[i].p16;
            o["hasP32"]  = G_KINDS[i].p32;
            if (G_KINDS[i].h16) o["p16hint"] = G_KINDS[i].h16;
            if (G_KINDS[i].h32) o["p32hint"] = G_KINDS[i].h32;
        }
    }

    // ---- specials ----
    JsonArray sp = d["specials"].to<JsonArray>();
    {
        struct ST_S { uint8_t v; const char* n; };
        static const ST_S G_SP[] = {
            { 0, "NONE" },
            { 1, "GYRO_RECALIB" },
            { 2, "SLEEP_NOW" },
            { 3, "MODE_CYCLE" },
            { 4, "PAIRING" },
            { 5, "HOST_CYCLE" },
        };
        for (size_t i = 0; i < sizeof(G_SP)/sizeof(G_SP[0]); i++) {
            JsonObject o = sp.add<JsonObject>();
            o["value"] = G_SP[i].v;
            o["name"]  = G_SP[i].n;
        }
    }

    // ---- directions ----
    JsonArray dirs = d["directions"].to<JsonArray>();
    { const char* G_D[] = { "LEFT", "RIGHT", "UP", "DOWN" };
      for (auto n : G_D) { JsonObject o = dirs.add<JsonObject>(); o["name"] = n; } }

    // ---- groups (Mode 슬롯 4그룹) ----
    JsonArray groups = d["groups"].to<JsonArray>();
    {
        struct ST_G { const char* n; uint8_t count; };
        static const ST_G G_GR[] = {
            { "slots",  15 },
            { "flick",  4 },
            { "linear", 4 },
            { "tilt",   4 },
        };
        for (size_t i = 0; i < sizeof(G_GR)/sizeof(G_GR[0]); i++) {
            JsonObject o = groups.add<JsonObject>();
            o["name"]  = G_GR[i].n;
            o["count"] = G_GR[i].count;
        }
    }

    // ---- slots_meta (S1..S15 트리거 라벨) ----
    JsonArray slotMeta = d["slots_meta"].to<JsonArray>();
    {
        struct ST_SM { const char* id; const char* label; const char* btn; const char* evt; };
        static const ST_SM G_SM[] = {
            { "S1",  "Top L Click",      "TOP_L",  "CLICK"  },
            { "S2",  "Top L Double",     "TOP_L",  "DOUBLE" },
            { "S3",  "Top L Long",       "TOP_L",  "LONG"   },
            { "S4",  "Top M Click",      "TOP_M",  "CLICK"  },
            { "S5",  "Top M Hold",       "TOP_M",  "HOLD"   },
            { "S6",  "Top R Click",      "TOP_R",  "CLICK"  },
            { "S7",  "Top R Double",     "TOP_R",  "DOUBLE" },
            { "S8",  "Top R Long",       "TOP_R",  "LONG"   },
            { "S9",  "Side F Click",     "SIDE_F", "CLICK"  },
            { "S10", "Side F Long",      "SIDE_F", "LONG"   },
            { "S11", "Side C Click",     "SIDE_C", "CLICK"  },
            { "S12", "Side C Double",    "SIDE_C", "DOUBLE" },
            { "S13", "Side C 2s Hold",   "SIDE_C", "HOLD_2S"},
            { "S14", "Side R Click",     "SIDE_R", "CLICK"  },
            { "S15", "Side R Long",      "SIDE_R", "LONG"   },
        };
        for (size_t i = 0; i < sizeof(G_SM)/sizeof(G_SM[0]); i++) {
            JsonObject o = slotMeta.add<JsonObject>();
            o["id"]    = G_SM[i].id;
            o["label"] = G_SM[i].label;
            o["btn"]   = G_SM[i].btn;
            o["evt"]   = G_SM[i].evt;
        }
    }

    // ==================================================
    // [v0412] triggers (27개)
    // ==================================================
    JsonArray trigs = d["triggers"].to<JsonArray>();
    for (uint8_t i = 0; i < (uint8_t)EN_C10_TRIG_MAX; i++) {
        JsonObject o = trigs.add<JsonObject>();
        o["idx"]    = i;
        o["name"]   = C10_TriggerName(i);
        o["group"]  = C10_TriggerGroup(i);
        o["locked"] = G_C10_TRIG_LOCKED[i];
    }
    d["trigger_count"] = (uint8_t)EN_C10_TRIG_MAX;

    d["api_ver"] = G_W10_API_VER;
    d["note"] = "v0412: profile-based slots (Global + Mode Override). "
                "action_kinds + specials + triggers for UI. "
                "mods mask == HID modifier byte. "
                "kb=usage-id(0x07), consumer=32-bit mask. "
                "MACRO kind(10) references macro index via p32.";

    _sendOk(req, "keycodes", "", &d, 200);
}
