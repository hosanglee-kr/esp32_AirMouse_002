// =======================================================
// File: src/v040/W10_WebApi_CtlPpt_0400.cpp
// =======================================================
/*
 * ------------------------------------------------------
 * 소스명 : W10_WebApi_CtlPpt_0400.cpp
 * 모듈약어 : W10
 * 모듈명 : Web Config/Status/UI/OTA Server (Split: Control + PPT)
 * ------------------------------------------------------
 * 기능 요약
 *  - (0400) /api/control, /api/ppt v0400 Mode 슬롯 방식 재설계
 *
 * [v0400 변경]
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

#include "W10_Web_0400.h"

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
// [v0400] /api/ppt GET — Mode 슬롯 매트릭스 조회
//   ?mode=N (1/2/3, 생략 시 active_mode)
// =====================================================
void CL_W10_WebConfig::apiGetPpt(AsyncWebServerRequest* req) {
    if (_gateSafeModeOrReply(req)) return;

    if (_cfg) (void)_cfg->loadAll(_wifi, _e10);

    // Mode 결정 (query param 또는 active_mode)
    uint8_t v_mode = _e10.active_mode;
    if (v_mode < 1 || v_mode > C10_DEF::MODE_COUNT) v_mode = 1;

    if (req->hasParam("mode")) {
        const AsyncWebParameter* p = req->getParam("mode");
        if (p) {
            const int v_m = atoi(p->value().c_str());
            if (v_m >= 1 && v_m <= (int)C10_DEF::MODE_COUNT) {
                v_mode = (uint8_t)v_m;
            }
        }
    }

    const ST_C10_ModeConfig_t& m = _e10.modes[v_mode - 1];

    JsonDocument v_doc;
    v_doc["mode"] = v_mode;

    // ---- slots[15] ----
    JsonArray v_slots = v_doc["slots"].to<JsonArray>();
    for (uint8_t i = 0; i < C10_DEF::SLOT_BTN_COUNT; i++) {
        JsonObject o = v_slots.add<JsonObject>();
        o["k"]   = m.slots[i].kind;
        o["h"]   = m.slots[i].holdMode;
        o["p16"] = m.slots[i].param16;
        o["p32"] = (uint32_t)m.slots[i].param32;
    }

    // ---- flick[4] ----
    JsonArray v_flick = v_doc["flick"].to<JsonArray>();
    for (uint8_t i = 0; i < C10_DEF::SLOT_FLICK_COUNT; i++) {
        JsonObject o = v_flick.add<JsonObject>();
        o["k"]   = m.flick[i].kind;
        o["h"]   = m.flick[i].holdMode;
        o["p16"] = m.flick[i].param16;
        o["p32"] = (uint32_t)m.flick[i].param32;
    }

    // ---- linear[4] ----
    JsonArray v_linear = v_doc["linear"].to<JsonArray>();
    for (uint8_t i = 0; i < C10_DEF::SLOT_LINEAR_COUNT; i++) {
        JsonObject o = v_linear.add<JsonObject>();
        o["k"]   = m.linear[i].kind;
        o["h"]   = m.linear[i].holdMode;
        o["p16"] = m.linear[i].param16;
        o["p32"] = (uint32_t)m.linear[i].param32;
    }

    // ---- tilt[4] ----
    JsonArray v_tilt = v_doc["tilt"].to<JsonArray>();
    for (uint8_t i = 0; i < C10_DEF::SLOT_TILT_COUNT; i++) {
        JsonObject o = v_tilt.add<JsonObject>();
        o["k"]   = m.tilt[i].kind;
        o["h"]   = m.tilt[i].holdMode;
        o["p16"] = m.tilt[i].param16;
        o["p32"] = (uint32_t)m.tilt[i].param32;
    }

    _sendOk(req, "ppt", "", &v_doc, 200);
}

// =====================================================
// [v0400] /api/ppt POST — Mode 슬롯 매트릭스 저장
//   ?mode=N (1/2/3, 생략 시 active_mode)
//   Body: {"slots":[...], "flick":[...], "linear":[...], "tilt":[...]}
//   각 배열은 부분 patch 가능 (배열 길이만큼만 반영)
// =====================================================
void CL_W10_WebConfig::apiPostPpt(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {

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

    if (!_cfg) {
        _sendErr(req, "no_config", "Config manager not ready.");
        return;
    }

    // Mode 결정
    uint8_t v_mode = _e10.active_mode;
    if (v_mode < 1 || v_mode > C10_DEF::MODE_COUNT) v_mode = 1;

    if (req->hasParam("mode")) {
        const AsyncWebParameter* p = req->getParam("mode");
        if (p) {
            const int v_m = atoi(p->value().c_str());
            if (v_m >= 1 && v_m <= (int)C10_DEF::MODE_COUNT) {
                v_mode = (uint8_t)v_m;
            }
        }
    }

    const bool v_save = (!v_jsonDoc["save"].isNull()) ? (bool)v_jsonDoc["save"] : true;

    // 현재 config 로드 (다른 Mode 보존)
    ST_C10_WiFiConfig_t w;
    ST_C10_E10Config_t  e;
    _cfg->makeDefaultsWiFi(w);
    _cfg->makeDefaultsE10(e);
    (void)_cfg->loadAll(w, e);

    // 대상 Mode의 슬롯 매트릭스 (부분 patch)
    ST_C10_ModeConfig_t& target = e.modes[v_mode - 1];

    auto loadSlots = [&](JsonVariantConst varr, ST_C20_ActionSlot_t* p_slots, uint8_t p_count) {
        if (varr.isNull()) return;
        JsonArrayConst arr = varr.as<JsonArrayConst>();
        if (arr.isNull()) return;
        const uint8_t n = (arr.size() < p_count) ? (uint8_t)arr.size() : p_count;
        for (uint8_t i = 0; i < n; i++) {
            JsonVariantConst o = arr[i];
            if (o.isNull()) continue;
            if (!o["k"].isNull())   p_slots[i].kind     = (uint8_t)o["k"];
            if (!o["h"].isNull())   p_slots[i].holdMode = (uint8_t)o["h"];
            if (!o["p16"].isNull()) p_slots[i].param16  = (uint16_t)o["p16"];
            if (!o["p32"].isNull()) p_slots[i].param32  = (uint32_t)o["p32"];
        }
    };

    loadSlots(v_jsonDoc["slots"],  target.slots,  C10_DEF::SLOT_BTN_COUNT);
    loadSlots(v_jsonDoc["flick"],  target.flick,  C10_DEF::SLOT_FLICK_COUNT);
    loadSlots(v_jsonDoc["linear"], target.linear, C10_DEF::SLOT_LINEAR_COUNT);
    loadSlots(v_jsonDoc["tilt"],   target.tilt,   C10_DEF::SLOT_TILT_COUNT);

    // 검증
    bool ok = _cfg->validateE10(e);
    if (!ok) {
        _sendErr(req, "validation_failed", "Mode slot validation failed.");
        return;
    }

    bool saved = false;
    if (v_save) {
        ok = _cfg->saveAll(w, e);
        saved = ok;
        if (ok) {
            (void)_cfg->loadAll(_wifi, _e10);
        }
    }

    // 런타임 반영
    bool applied = false;
    ST_W10_E10If_t* e10if = _e10if;
    if (ok && e10if && e10if->applyRuntimeE10) {
        applied = e10if->applyRuntimeE10(e10if->ctx, &e);
    }

    JsonDocument v_doc;
    v_doc["mode"]    = v_mode;
    v_doc["saved"]   = saved;
    v_doc["applied"] = applied;

    if (ok) _sendOk(req, "ppt_set", "", &v_doc, 200);
    else    _sendErr(req, "ppt_set_failed", "Failed to update mode slots.", &v_doc);
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
