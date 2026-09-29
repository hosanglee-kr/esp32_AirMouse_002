// =======================================================
// File: src/v0410/W10_WebApi_CtlPpt_0410.cpp
// =======================================================
/*
 * ------------------------------------------------------
 * 소스명 : W10_WebApi_CtlPpt_0410.cpp
 * 모듈약어 : W10
 * 모듈명 : Web Config/Status/UI/OTA Server (Split: Control + PPT)
 * ------------------------------------------------------
 * 기능 요약
 *  - (0410) /api/control, /api/ppt v0410 Mode 슬롯 방식 재설계
 *
 * [v0410 변경]
 *  - /api/ppt GET/POST를 Mode 슬롯 매트릭스 방식으로 전환
 *  - ppt2_* 6슬롯 폐기 → Mode별 {slots[15], flick[4], linear[4], tilt[4]}
 *  - ?mode=N 파라미터 추가 (생략 시 active_mode)
 *  - /api/ppt/test는 기존 유지 (page/mod/code)
 * ------------------------------------------------------
 * [구현 규칙]
 *  - ArduinoJson v7.x.x 사용 (v6 이하 사용 금지)
 *  - JsonDocument 단일 타입만 사용
 *  - createNestedArray/Object/containsKey 사용 금지
 *  - memset + strlcpy 기반 안전 초기화
 * ------------------------------------------------------
 */

#include "W10_Web_0410.h"

// =====================================================
// /api/control
// =====================================================
void CL_W10_WebConfig::apiControl(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {

    if (_gateSafeModeOrReply(req)) return;

    String v_body;
    if (!_collectBodyOrReply(req, data, len, index, total, v_body)) return;

    JsonDocument v_jsonDoc;
    DeserializationError v_err = deserializeJson(v_jsonDoc, v_body);
    if (v_err) {
        _cnt_json_bad++;
        _diagPush("bad_json");
        _sendErr(req, "bad_json", "Invalid JSON.");
        return;
    }

    ST_W10_E10If_t* e10if = _e10if;
    bool ok = true;

    const bool v_snapshot = (!v_jsonDoc["snapshot"].isNull()) ? (bool)v_jsonDoc["snapshot"] : false;

    const char* v_cmd = nullptr;
    if (!v_jsonDoc["cmd"].isNull()) v_cmd = (const char*)v_jsonDoc["cmd"];

    if (!e10if) ok = false;

    if (ok && v_cmd && v_cmd[0] != '\0') {
        if (strcmp(v_cmd, "set_ppt") == 0) {
            bool v_en = false;
            if (!v_jsonDoc["enable"].isNull()) v_en = (bool)v_jsonDoc["enable"];
            ok = ok && (e10if && e10if->setPptMode ? e10if->setPptMode(e10if->ctx, v_en) : false);

        } else if (strcmp(v_cmd, "set_dpi") == 0) {
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

        } else {
            ok = false;
        }

    } else if (ok) {
        if (!v_jsonDoc["ppt_mode"].isNull()) ok = ok && (e10if && e10if->setPptMode ? e10if->setPptMode(e10if->ctx, (bool)v_jsonDoc["ppt_mode"]) : false);
        if (!v_jsonDoc["dpi_level"].isNull()) ok = ok && (e10if && e10if->setDpiLevel ? e10if->setDpiLevel(e10if->ctx, (uint8_t)v_jsonDoc["dpi_level"]) : false);

        if (!v_jsonDoc["precision_mode"].isNull()) {
            uint8_t v_mode = (uint8_t)v_jsonDoc["precision_mode"];
            if (v_mode >= (uint8_t)EN_C10_E10_PREC_MAX) ok = false;
            else ok = ok && (e10if && e10if->setPrecisionMode ? e10if->setPrecisionMode(e10if->ctx, v_mode) : false);
        }
        if (!v_jsonDoc["safe_mode"].isNull()) ok = ok && (e10if && e10if->setSafeMode ? e10if->setSafeMode(e10if->ctx, (bool)v_jsonDoc["safe_mode"]) : false);
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
// [v0410] /api/ppt GET/POST — 폐기 (R9: profile API로 통합)
// =====================================================
void CL_W10_WebConfig::apiGetPpt(AsyncWebServerRequest* req) {
    _sendErr(req, "deprecated", "PPT API deprecated in v0410. Use /api/profiles instead.");
}

void CL_W10_WebConfig::apiPostPpt(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    (void)data; (void)len; (void)index; (void)total;
    _sendErr(req, "deprecated", "PPT API deprecated in v0410. Use /api/profiles instead.");
}

// =====================================================
// /api/ppt/test — 기존 유지 (단일 키 테스트)
// =====================================================
void CL_W10_WebConfig::apiPptTest(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {

    if (_gateSafeModeOrReply(req)) return;

    String v_body;
    if (!_collectBodyOrReply(req, data, len, index, total, v_body)) return;

    JsonDocument v_jsonDoc;
    DeserializationError v_err = deserializeJson(v_jsonDoc, v_body);
    if (v_err) {
        _cnt_json_bad++;
        _diagPush("bad_json");
        _sendErr(req, "bad_json", "Invalid JSON.");
        return;
    }

    uint8_t  page = (uint8_t)EN_C10_KEYPAGE_KB;
    uint8_t  mod  = 0;
    uint32_t code = 0;

    if (!v_jsonDoc["page"].isNull()) {
        const char* s = (const char*)v_jsonDoc["page"];
        if (s && strcasecmp(s, "consumer") == 0) page = (uint8_t)EN_C10_KEYPAGE_CONSUMER;
    }
    if (!v_jsonDoc["mod"].isNull())  mod  = (uint8_t)v_jsonDoc["mod"];
    if (!v_jsonDoc["code"].isNull()) code = (uint32_t)v_jsonDoc["code"];

    ST_W10_E10If_t* e10if = _e10if;
    bool ok = (e10if && e10if->testPptKey2 ? e10if->testPptKey2(e10if->ctx, page, mod, code) : false);

    if (ok) _sendOk(req, "ppt_test", "", nullptr, 200);
    else    _sendErr(req, "ppt_test_failed", "Test failed.");
}
