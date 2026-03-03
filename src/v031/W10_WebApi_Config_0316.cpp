// =======================================================
// File: W10_WebApi_Config_0316.cpp
// =======================================================

/*
 * ------------------------------------------------------
 * 소스명 : W10_WebApi_Config_0316.cpp
 * 모듈약어 : W10
 * 모듈명 : Web Config/Status/UI/OTA Server (Split: Config APIs)
 * ------------------------------------------------------
 * 기능 요약
 *  - (0316) /api/config, save/apply/export/import/rollback 분리
 * ------------------------------------------------------
 * [구현 규칙] ... (동일)
 * ------------------------------------------------------
 * [코드 네이밍 규칙] ... (동일)
 * ------------------------------------------------------
 */

#include "W10_Web_0315.h"


// 붙여넣기 대상:
// - _apiConfigSaveImportCommon
// - apiGetConfig
// - apiConfigSave
// - apiConfigApply
// - apiExport
// - apiImport
// - apiRollback



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

    // SafeMode Gate (허용 목록(_isApiAllowedInSafeMode) 기준으로 save/import 모두 공통 차단/허용)
    if (_gateSafeModeOrReply(req)) return;


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

    if (v_ok) {
        _markLastApply(true, (p_src ? p_src : "save"), "config_save");
        _sendOk(req, "config_save", "", &v_doc, 200);
    } else {
        _markLastApply(false, (p_src ? p_src : "save"), "config_save_failed");
        _sendErr(req, "config_save_failed", "Save/import failed.", &v_doc);
    }
}


// =====================================================
// /api/config (GET)  - raw/envelope 모두 Stream 적용
// =====================================================
void CL_W10_WebConfig::apiGetConfig(AsyncWebServerRequest* req) {
    const bool v_envelope = _wantsEnvelope(req);

    uint32_t v_etag = 0;
    size_t v_size = 0;
    bool v_hasEtag = (_cfg && _cfg->getConfigEtag(v_etag, &v_size));

    if (v_hasEtag && _ifNoneMatchHit(req, v_etag)) {
        _send304NoStoreEtag(req, v_etag);
        return;
    }


    String json;
    if (!_cfg || !_cfg->exportJson(json)) {
        _sendErr(req, "config_get_failed", "Failed to export config.");
        return;
    }

    if (v_envelope) {
        AsyncResponseStream* res = req->beginResponseStream("application/json");
        res->setCode(200);

        _addEtagHeadersNoStore(res, v_hasEtag, v_etag, v_size);

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

    _addEtagHeadersNoStore(res, v_hasEtag, v_etag, v_size);

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

    // [PATCH] SafeMode Gate (apply는 SafeMode에서 차단)
    if (_gateSafeModeOrReply(req)) return;

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

    if (v_hasEtag && _ifNoneMatchHit(req, v_etag)) {
        _send304NoStoreEtag(req, v_etag);
        return;
    }


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

        _addEtagHeadersNoStore(res, v_hasEtag, v_etag, v_size);

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

    _addEtagHeadersNoStore(res, v_hasEtag, v_etag, v_size);

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


