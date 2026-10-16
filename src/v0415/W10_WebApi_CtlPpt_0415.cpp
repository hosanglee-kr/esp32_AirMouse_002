/*
 * ------------------------------------------------------
 * 소스명 : W10_WebApi_CtlPpt_0415.cpp
 * 모듈약어 : W10
 * 모듈명 : Control + PPT Test API
 * ------------------------------------------------------
 * [v0415 Phase 2-1]
 *  - apiControl / apiPptTest: _parseBodyAndGate 사용
 * ------------------------------------------------------
 */

#include "W10_Web_0415.h"

// =====================================================
// /api/control
// =====================================================
void CL_W10_WebConfig::apiControl(AsyncWebServerRequest* req,
                                  uint8_t* data, size_t len,
                                  size_t index, size_t total) {
    // [Phase 2-1] SafeMode gate + Body + JSON parse 통합
    JsonDocument v_jsonDoc;
    if (!_parseBodyAndGate(req, data, len, index, total, v_jsonDoc)) return;

    ST_W10_E10If_t* e10if = _e10if;
    bool ok = true;

    const bool v_snapshot = (!v_jsonDoc["snapshot"].isNull()) ? (bool)v_jsonDoc["snapshot"] : false;

    const char* v_cmd = nullptr;
    if (!v_jsonDoc["cmd"].isNull()) v_cmd = (const char*)v_jsonDoc["cmd"];

    if (!e10if) ok = false;

    if (ok && v_cmd && v_cmd[0] != '\0') {
        if (strcmp(v_cmd, "set_dpi") == 0) {
            uint8_t v_lv = 2;
            if (!v_jsonDoc["level"].isNull()) v_lv = (uint8_t)v_jsonDoc["level"];
            ok = ok && (e10if && e10if->setDpiLevel ? e10if->setDpiLevel(e10if->ctx, v_lv) : false);

        } else if (strcmp(v_cmd, "set_precision") == 0) {
            uint8_t v_mode = (uint8_t)EN_C10_E10_PREC_OFF;
            if (!v_jsonDoc["mode"].isNull()) v_mode = (uint8_t)v_jsonDoc["mode"];

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
            if (!v_jsonDoc["enable"].isNull()) v_en = (bool)v_jsonDoc["enable"];
            ok = ok && (e10if && e10if->setSafeMode ? e10if->setSafeMode(e10if->ctx, v_en) : false);

        } else if (strcmp(v_cmd, "set_ota_guard") == 0) {
            bool v_en = false;
            if (!v_jsonDoc["enable"].isNull()) v_en = (bool)v_jsonDoc["enable"];
            ok = ok && (e10if && e10if->setOtaGuard ? e10if->setOtaGuard(e10if->ctx, v_en) : false);

        } else if (strcmp(v_cmd, "set_hard_click_lock") == 0) {
            bool v_en = false;
            if (!v_jsonDoc["enable"].isNull()) v_en = (bool)v_jsonDoc["enable"];
            ok = ok && (e10if && e10if->setHardClickLock ? e10if->setHardClickLock(e10if->ctx, v_en) : false);

        } else if (strcmp(v_cmd, "set_mode") == 0) {
            uint8_t v_mode = 1;
            if (!v_jsonDoc["mode"].isNull()) v_mode = (uint8_t)v_jsonDoc["mode"];

            if (v_mode < 1 || v_mode > 3) {
                ok = false;
            } else {
                ok = ok && (e10if && e10if->setActiveMode ? e10if->setActiveMode(e10if->ctx, v_mode) : false);
            }

        } else {
            ok = false;
        }

    } else if (ok) {
        if (!v_jsonDoc["dpi_level"].isNull()) {
            ok = ok && (e10if && e10if->setDpiLevel ? e10if->setDpiLevel(e10if->ctx, (uint8_t)v_jsonDoc["dpi_level"]) : false);
        }
        if (!v_jsonDoc["precision_mode"].isNull()) {
            uint8_t v_mode = (uint8_t)v_jsonDoc["precision_mode"];
            if (v_mode >= (uint8_t)EN_C10_E10_PREC_MAX) ok = false;
            else ok = ok && (e10if && e10if->setPrecisionMode ? e10if->setPrecisionMode(e10if->ctx, v_mode) : false);
        }
        if (!v_jsonDoc["safe_mode"].isNull()) {
            ok = ok && (e10if && e10if->setSafeMode ? e10if->setSafeMode(e10if->ctx, (bool)v_jsonDoc["safe_mode"]) : false);
        }
    }

    JsonDocument v_doc;
    v_doc["cmd"] = (v_cmd ? v_cmd : "");
    if (v_snapshot && e10if) {
        JsonObject e = v_doc["e10"].to<JsonObject>();
        _fillE10Status(e, e10if);
    }
    if (ok) _sendOk(req, "control", "", &v_doc, 200);
    else    _sendErr(req, "control_failed", "Control failed.", &v_doc);
}

// =====================================================
// /api/ppt/test
// =====================================================
void CL_W10_WebConfig::apiPptTest(AsyncWebServerRequest* req,
                                  uint8_t* data, size_t len,
                                  size_t index, size_t total) {
    D10_LOGI("[W10] POST /api/ppt/test: len=%u, tot=%u",
             (unsigned)len, (unsigned)total);

    // [Phase 2-1] SafeMode gate + Body + JSON parse 통합
    JsonDocument v_jsonDoc;
    if (!_parseBodyAndGate(req, data, len, index, total, v_jsonDoc)) return;

    uint8_t  page = (uint8_t)EN_C10_KEYPAGE_KB;
    uint8_t  mod  = 0;
    uint32_t code = 0;

    JsonVariant v_page = v_jsonDoc["page"];
    if (!v_page.isNull() && v_page.is<const char*>()) {
        const char* s = v_page.as<const char*>();
        if (s && strcasecmp(s, "consumer") == 0) page = (uint8_t)EN_C10_KEYPAGE_CONSUMER;
    }
    if (!v_jsonDoc["mod"].isNull())  mod  = (uint8_t)v_jsonDoc["mod"];
    if (!v_jsonDoc["code"].isNull()) code = (uint32_t)v_jsonDoc["code"];

    ST_W10_E10If_t* e10if = _e10if;
    bool ok = (e10if && e10if->testPptKey2 ? e10if->testPptKey2(e10if->ctx, page, mod, code) : false);

    if (ok) _sendOk(req, "ppt_test", "", nullptr, 200);
    else    _sendErr(req, "ble_disconnected", "Key test failed (BLE disconnected to host PC).");
}
