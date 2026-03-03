// =======================================================
// File: W10_WebApi_CtlPpt_0316.cpp
// =======================================================

/*
* ------------------------------------------------------
* 소스명 : W10_WebApi_CtlPpt_0316.cpp
* 모듈약어 : W10
* 모듈명 : Web Config/Status/UI/OTA Server (Split: Control + PPT)
* ------------------------------------------------------
* 기능 요약
* - (0316) /api/control, /api/ppt 분리
* ------------------------------------------------------
* [구현 규칙] ... (동일)
* ------------------------------------------------------
* [코드 네이밍 규칙] ... (동일)
* ------------------------------------------------------
*/

#include "W10_Web_0315.h"

// 붙여넣기 대상:
// - apiControl
// - apiGetPpt
// - apiPostPpt
// - apiPptTest



// =====================================================
// /api/control
// =====================================================
void CL_W10_WebConfig::apiControl(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {

    if (_gateSafeModeOrReply(req)) return;

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
    // SafeMode Gate (ppt 조회도 SafeMode에서 차단: 최소 정책)
    if (_gateSafeModeOrReply(req)) return;

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

    if (_gateSafeModeOrReply(req)) return;

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

    if (_gateSafeModeOrReply(req)) return;

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

