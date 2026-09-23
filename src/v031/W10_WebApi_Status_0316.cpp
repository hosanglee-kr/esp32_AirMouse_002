// =======================================================
// File: W10_WebApi_Status_0316.cpp
// =======================================================

/*
 * ------------------------------------------------------
 * 소스명 : W10_WebApi_Status_0316.cpp
 * 모듈약어 : W10
 * 모듈명 : Web Config/Status/UI/OTA Server (Split: Status/Diag/Keycodes)
 * ------------------------------------------------------
 * 기능 요약
 *  - (0316) /api/status, /api/diag, /api/keycodes 및 E10 status fill 분리
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

#include "W10_Web_0315.h"

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
