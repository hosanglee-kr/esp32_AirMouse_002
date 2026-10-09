/*
 * ------------------------------------------------------
 * 소스명 : W10_WebApi_CtlPpt_0415.cpp
 * 모듈약어 : W10
 * 모듈명 : Web Config/Status/UI/OTA Server (Split: Control + PPT Test)
 * ------------------------------------------------------
 * 기능 요약
 *  - /api/control  : Quick Control (PPT/DPI/Precision/SafeMode/OTA Guard/I2C/Gyro)
 *  - /api/ppt/test : 단발 키 테스트 (page/mod/code)
 *
 * [v0412 변경]
 *  - /api/ppt GET/POST는 폐기됨 (라우팅 미등록). 프로파일 API(/api/profiles)로 통합.
 *  - /api/ppt/test는 기존 유지 (page/mod/code)
 * ------------------------------------------------------
 * [구현 규칙]
 *  - ArduinoJson v7.x.x 사용 (v6 이하 사용 금지)
 *  - JsonDocument 단일 타입만 사용
 *  - createNestedArray/Object/containsKey 사용 금지
 *  - memset + strlcpy 기반 안전 초기화
 * ------------------------------------------------------
 */

// =======================================================
#include "W10_Web_0415.h"

// =====================================================
// /api/control
// -----------------------------------------------------
// [v0415 변경] set_ppt cmd / ppt_mode legacy 필드 삭제
//   - E10 _isPptMode 필드 삭제 (Round G)
//   - PPT 판정은 active_mode == 2 로 일원화
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
        // [v0415] set_ppt cmd 삭제
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
            // [v0415] M10 Click-Lock 삭제. E10의 setHardClickLock은 no-op 스텁.
            bool v_en = false;
            if (!v_jsonDoc["enable"].isNull()) v_en = (bool)v_jsonDoc["enable"];
            ok = ok && (e10if && e10if->setHardClickLock ? e10if->setHardClickLock(e10if->ctx, v_en) : false);

        } else {
            ok = false;
        }

    } else if (ok) {
        // [v0415] legacy 필드: ppt_mode 삭제. dpi_level / precision_mode / safe_mode 유지
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
// /api/ppt/test (유지)
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

    // [v0415 L7f-A3-02] page 타입 검사 추가
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
    else    _sendErr(req, "ppt_test_failed", "Test failed.");
}
