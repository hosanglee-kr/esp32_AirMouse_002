// =======================================================
// File: src/v0415/W10_WebApi_Config_0415.cpp
// =======================================================
/*
 * ------------------------------------------------------
 * 소스명 : W10_WebApi_Config_0415.cpp
 * 모듈약어 : W10
 * 모듈명 : Config export/import/rollback API
 * ------------------------------------------------------
 * [v0415 Phase 2-1]
 *  - apiImport: _parseBodyAndGate 사용
 *  - apiExport / apiRollback: Body 없음 (게이트만)
 * ------------------------------------------------------
 */

#include "W10_Web_0415.h"

// =====================================================
// 부팅 WiFi 스냅샷과 현재 상태 diff → 재부팅 마스크 재계산
// =====================================================
void CL_W10_WebConfig::_bumpRebootMaskFromBoot() {
    const uint32_t v_m = _wifiDiffMask(_bootWifi, _wifi);
    _needRebootMask = v_m;
    _needReboot     = (v_m != 0);
}

// =====================================================
// GET /api/config/export
// =====================================================
void CL_W10_WebConfig::apiExport(AsyncWebServerRequest* req) {
    if (!_cfg) {
        _sendErr(req, "no_config", "Config manager not ready.");
        return;
    }

    ST_C10_ProfileConfig_t v_p;
    _cfg->makeDefaultsProfile(_cfg->getActiveIndex(), v_p);
    if (!_cfg->loadActiveProfile(v_p)) {
        _sendErr(req, "config_export_failed", "Failed to load active profile.");
        return;
    }

    JsonDocument v_doc;
    if (!_cfg->buildProfileJson(v_p, v_doc)) {
        _sendErr(req, "config_export_failed", "Failed to serialize profile.");
        return;
    }

    // ---- filename (query or default) ----
    String v_filename;
    if (req->hasParam("filename")) {
        const AsyncWebParameter* p = req->getParam("filename");
        if (p) v_filename = p->value();
    }

    // 파일명 sanitize (CRLF/제어문자/경로조작 방어)
    String v_safe;
    v_safe.reserve(v_filename.length());
    for (size_t i = 0; i < v_filename.length() && v_safe.length() < 64; i++) {
        const char c = v_filename[i];
        if (c == '\r' || c == '\n' || c == '"' || c == '\\' || c == '/' ||
            (uint8_t)c < 0x20 || c == 0x7F) {
            v_safe += '_';
        } else {
            v_safe += c;
        }
    }
    v_filename = v_safe;

    if (v_filename.length() == 0) {
        char v_buf[40];
        snprintf(v_buf, sizeof(v_buf), "profile_%u.json",
                 (unsigned)_cfg->getActiveIndex());
        v_filename = String(v_buf);
    }

    AsyncResponseStream* res = req->beginResponseStream("application/json");
    res->setCode(200);
    res->addHeader("Cache-Control", G_W10_CACHE_NOSTORE);
    res->addHeader("Content-Disposition",
                   String("attachment; filename=\"") + v_filename + "\"");
    serializeJson(v_doc, *res);
    req->send(res);
}

// =====================================================
// POST /api/config/import
// =====================================================
void CL_W10_WebConfig::apiImport(AsyncWebServerRequest* req,
                                 uint8_t* data, size_t len,
                                 size_t index, size_t total) {
    if (!_cfg) {
        _sendErr(req, "no_config", "Config manager not ready.");
        return;
    }

    if (_cfg->getProfileCount() >= C10_DEF::PROFILE_MAX) {
        _sendErr(req, "config_import_failed", "Max profiles reached.");
        return;
    }

    // [Phase 2-1] SafeMode gate + Body + JSON parse 통합
    //   ※ patchProfileFromJson이 v_body(String)을 필요로 하므로
    //     _parseBodyAndGate 대신 개별 처리 유지
    if (_gateSafeModeOrReply(req)) return;

    String v_body;
    if (!_collectBodyOrReply(req, data, len, index, total, v_body)) return;

    JsonDocument v_doc;
    DeserializationError v_err = deserializeJson(v_doc, v_body);
    if (v_err) {
        _cnt_json_bad++;
        _diagPush("bad_json");
        _sendErr(req, "bad_json", "Invalid JSON.");
        return;
    }

    const char* v_name = nullptr;
    if (!v_doc["name"].isNull()) v_name = (const char*)v_doc["name"];

    uint8_t v_newIdx = 0;
    if (!_cfg->createProfile(v_name, v_newIdx)) {
        _sendErr(req, "config_import_failed", "Create failed.");
        return;
    }

    ST_C10_ProfileConfig_t v_p;
    _cfg->makeDefaultsProfile(v_newIdx, v_p);
    if (!_cfg->loadProfile(v_newIdx, v_p)) {
        (void)_cfg->deleteProfile(v_newIdx);
        _sendErr(req, "config_import_failed", "Load failed.");
        return;
    }

    if (!_cfg->patchProfileFromJson(v_body, v_p)) {
        (void)_cfg->deleteProfile(v_newIdx);
        _sendErr(req, "bad_json", "Patch failed.");
        return;
    }

    if (!_cfg->validateProfile(v_p)) {
        (void)_cfg->deleteProfile(v_newIdx);
        _sendErr(req, "validation_failed", "Validation failed.");
        return;
    }

    if (!_cfg->saveProfile(v_newIdx, v_p)) {
        (void)_cfg->deleteProfile(v_newIdx);
        _sendErr(req, "config_import_failed", "Save failed.");
        return;
    }

    JsonDocument v_out;
    v_out["idx"]   = v_newIdx;
    v_out["count"] = (uint8_t)_cfg->getProfileCount();
    v_out["name"]  = v_p.name;

    _markLastApply(true, "import", "config_import");
    _sendOk(req, "config_import", "", &v_out, 200);
}

// =====================================================
// POST /api/config/rollback (v0412 폐기)
// =====================================================
void CL_W10_WebConfig::apiRollback(AsyncWebServerRequest* req) {
    (void)req;
    _sendErr(req, "config_rollback_failed",
             "Rollback not supported in v0412. Use profile switch or factory_reset.");
}
