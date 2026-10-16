// =======================================================
// File: src/v0415/W10_WebApi_Profile_0415.cpp
// =======================================================
/*
 * ------------------------------------------------------
 * 소스명 : W10_WebApi_Profile_0415.cpp
 * 모듈약어 : W10
 * 모듈명 : Profile / Triggers / Live Test API
 * ------------------------------------------------------
 * [v0415 Phase 2-1]
 *  - 7개 POST 핸들러에서 _parseBodyAndGate 사용
 *    · apiProfilesSwitch / Create / Delete / Rename
 *    · apiProfilesActivePost / apiActionTest / apiActionTestMacro
 * ------------------------------------------------------
 */

#include "W10_Web_0415.h"

// =====================================================
// GET /api/profiles
// =====================================================
void CL_W10_WebConfig::apiProfilesList(AsyncWebServerRequest* req) {
    if (!_cfg) {
        _sendErr(req, "no_config", "Config manager not ready.");
        return;
    }

    JsonDocument v_doc;
    v_doc["active"] = (uint8_t)_cfg->getActiveIndex();
    v_doc["count"]  = (uint8_t)_cfg->getProfileCount();

    JsonArray v_arr = v_doc["profiles"].to<JsonArray>();
    for (uint8_t i = 0; i < _cfg->getProfileCount(); i++) {
        ST_C10_ProfileConfig_t v_p;
        _cfg->makeDefaultsProfile(i, v_p);
        (void)_cfg->loadProfile(i, v_p);

        JsonObject o = v_arr.add<JsonObject>();
        o["idx"]  = i;
        o["name"] = v_p.name;
    }

    _sendOk(req, "profiles", "", &v_doc, 200);
}

// =====================================================
// POST /api/profiles/switch
// =====================================================
void CL_W10_WebConfig::apiProfilesSwitch(AsyncWebServerRequest* req,
                                         uint8_t* data, size_t len,
                                         size_t index, size_t total) {
    JsonDocument v_d;
    if (!_parseBodyAndGate(req, data, len, index, total, v_d)) return;

    if (v_d["idx"].isNull()) {
        _sendErr(req, "bad_json", "Missing idx.");
        return;
    }

    const uint8_t v_idx = (uint8_t)v_d["idx"];

    if (!_e10if || !_e10if->switchProfile) {
        _sendErr(req, "control_failed", "Switch not available.");
        return;
    }

    const bool v_ok = _e10if->switchProfile(_e10if->ctx, v_idx);
    if (!v_ok) {
        _sendErr(req, "profile_switch_failed",
                 "Switch failed (invalid idx / in progress).");
        return;
    }

    if (_cfg) {
        ST_C10_ProfileConfig_t v_p;
        _cfg->makeDefaultsProfile(_cfg->getActiveIndex(), v_p);
        if (_cfg->loadActiveProfile(v_p)) {
            _wifi = v_p.wifi;
        }
    }

    JsonDocument v_out;
    v_out["idx"] = v_idx;
    _sendOk(req, "profile_switch", "", &v_out, 200);
}

// =====================================================
// POST /api/profiles/create
// =====================================================
void CL_W10_WebConfig::apiProfilesCreate(AsyncWebServerRequest* req,
                                         uint8_t* data, size_t len,
                                         size_t index, size_t total) {
    if (!_cfg) {
        _sendErr(req, "no_config", "Config manager not ready.");
        return;
    }

    JsonDocument v_d;
    if (!_parseBodyAndGate(req, data, len, index, total, v_d)) return;

    const char* v_name = nullptr;
    if (!v_d["name"].isNull()) v_name = (const char*)v_d["name"];

    uint8_t v_outIdx = 0;
    if (!_cfg->createProfile(v_name, v_outIdx)) {
        _sendErr(req, "profile_create_failed", "Create failed (max reached?).");
        return;
    }

    JsonDocument v_out;
    v_out["idx"]  = v_outIdx;

    char v_nameBuf[C10_DEF::PROFILE_NAME_LEN] = {0};
    ST_C10_ProfileConfig_t v_p;
    _cfg->makeDefaultsProfile(v_outIdx, v_p);
    if (_cfg->loadProfile(v_outIdx, v_p)) {
        strlcpy(v_nameBuf, v_p.name, sizeof(v_nameBuf));
    }
    v_out["name"] = v_nameBuf;

    _sendOk(req, "profile_create", "", &v_out, 200);
}

// =====================================================
// POST /api/profiles/delete
// =====================================================
void CL_W10_WebConfig::apiProfilesDelete(AsyncWebServerRequest* req,
                                         uint8_t* data, size_t len,
                                         size_t index, size_t total) {
    if (!_cfg) {
        _sendErr(req, "no_config", "Config manager not ready.");
        return;
    }

    JsonDocument v_d;
    if (!_parseBodyAndGate(req, data, len, index, total, v_d)) return;

    if (v_d["idx"].isNull()) {
        _sendErr(req, "bad_json", "Missing idx.");
        return;
    }

    const uint8_t v_idx = (uint8_t)v_d["idx"];

    const uint8_t v_prevCount = _cfg->getProfileCount();
    const bool v_wasSwap = (v_idx < (v_prevCount - 1)) && (v_prevCount > 1);
    const uint8_t v_swappedFrom = (uint8_t)(v_prevCount - 1);

    if (!_cfg->deleteProfile(v_idx)) {
        _sendErr(req, "profile_delete_failed", "Delete failed (min 1 profile).");
        return;
    }

    bool v_reloaded = false;
    if (_e10if && _e10if->reloadProfile) {
        v_reloaded = _e10if->reloadProfile(_e10if->ctx);
    }

    if (_cfg) {
        ST_C10_ProfileConfig_t v_p;
        _cfg->makeDefaultsProfile(_cfg->getActiveIndex(), v_p);
        if (_cfg->loadActiveProfile(v_p)) {
            _wifi = v_p.wifi;
        }
    }

    JsonDocument v_out;
    v_out["count"]       = (uint8_t)_cfg->getProfileCount();
    v_out["active"]      = (uint8_t)_cfg->getActiveIndex();
    v_out["reloaded"]    = v_reloaded;
    v_out["swap"]        = v_wasSwap;
    if (v_wasSwap) {
        v_out["swapped_from"] = v_swappedFrom;
        v_out["swapped_to"]   = v_idx;
    }

    _sendOk(req, "profile_delete", "", &v_out, 200);
}

// =====================================================
// POST /api/profiles/rename
// =====================================================
void CL_W10_WebConfig::apiProfilesRename(AsyncWebServerRequest* req,
                                         uint8_t* data, size_t len,
                                         size_t index, size_t total) {
    if (!_cfg) {
        _sendErr(req, "no_config", "Config manager not ready.");
        return;
    }

    JsonDocument v_d;
    if (!_parseBodyAndGate(req, data, len, index, total, v_d)) return;

    if (v_d["idx"].isNull() || v_d["name"].isNull()) {
        _sendErr(req, "bad_json", "Missing idx or name.");
        return;
    }

    const uint8_t v_idx  = (uint8_t)v_d["idx"];
    const char*   v_name = (const char*)v_d["name"];

    if (!v_name || !v_name[0]) {
        _sendErr(req, "bad_json", "Empty name.");
        return;
    }

    if (!_cfg->renameProfile(v_idx, v_name)) {
        _sendErr(req, "profile_rename_failed", "Rename failed.");
        return;
    }

    bool v_reloaded = false;
    if (v_idx == _cfg->getActiveIndex() && _e10if && _e10if->reloadProfile) {
        v_reloaded = _e10if->reloadProfile(_e10if->ctx);
    }

    JsonDocument v_out;
    v_out["idx"]      = v_idx;
    v_out["name"]     = v_name;
    v_out["reloaded"] = v_reloaded;

    _sendOk(req, "profile_rename", "", &v_out, 200);
}

// =====================================================
// GET /api/profiles/active
// =====================================================
void CL_W10_WebConfig::apiProfilesActiveGet(AsyncWebServerRequest* req) {
    if (!_cfg) {
        _sendErr(req, "no_config", "Config manager not ready.");
        return;
    }

    ST_C10_ProfileConfig_t v_p;
    _cfg->makeDefaultsProfile(_cfg->getActiveIndex(), v_p);

    if (!_cfg->loadActiveProfile(v_p)) {
        _sendErr(req, "profile_load_failed", "Failed to load active profile.");
        return;
    }

    JsonDocument v_inner;
    if (!_cfg->buildProfileJson(v_p, v_inner)) {
        _sendErr(req, "profile_build_failed", "Failed to serialize profile.");
        return;
    }

    // envelope 수동 스트리밍 (triple-copy 회피)
    AsyncResponseStream* res = req->beginResponseStream("application/json");
    res->setCode(200);
    res->addHeader("Cache-Control", G_W10_CACHE_NOSTORE);

    res->print(F("{\"ok\":true,\"code\":\"profile_get\",\"msg\":\"\",\"data\":{\"idx\":"));
    res->print((unsigned)_cfg->getActiveIndex());
    res->print(F(",\"count\":"));
    res->print((unsigned)_cfg->getProfileCount());
    res->print(F(",\"config\":"));
    serializeJson(v_inner, *res);
    res->print(F("}}"));

    req->send(res);
}

// =====================================================
// POST /api/profiles/active
// =====================================================
void CL_W10_WebConfig::apiProfilesActivePost(AsyncWebServerRequest* req,
                                             uint8_t* data, size_t len,
                                             size_t index, size_t total) {
    if (!_cfg) {
        _sendErr(req, "no_config", "Config manager not ready.");
        return;
    }

    // [Phase 2-1] SafeMode gate + Body + JSON parse 통합
    //   ※ patchProfileFromJson이 v_body(String)을 필요로 하므로
    //     _parseBodyAndGate 대신 개별 처리 유지
    if (_gateSafeModeOrReply(req)) return;

    String v_body;
    if (!_collectBodyOrReply(req, data, len, index, total, v_body)) return;

    ST_C10_ProfileConfig_t v_p;
    _cfg->makeDefaultsProfile(_cfg->getActiveIndex(), v_p);
    (void)_cfg->loadActiveProfile(v_p);

    if (!_cfg->patchProfileFromJson(v_body, v_p)) {
        _cnt_json_bad++;
        _diagPush("bad_json");
        _sendErr(req, "bad_json", "Invalid profile JSON.");
        return;
    }

    if (!_cfg->validateProfile(v_p)) {
        _sendErr(req, "validation_failed", "Profile validation failed.");
        return;
    }

    const uint8_t v_idx = _cfg->getActiveIndex();
    if (!_cfg->saveProfile(v_idx, v_p)) {
        _sendErr(req, "profile_save_failed", "Failed to save profile.");
        return;
    }

    _wifi = v_p.wifi;
    _bumpRebootMaskFromBoot();

    bool v_reloaded = false;
    if (_e10if && _e10if->reloadProfile) {
        v_reloaded = _e10if->reloadProfile(_e10if->ctx);
    }

    JsonDocument v_out;
    v_out["idx"]             = v_idx;
    v_out["reloaded"]        = v_reloaded;
    v_out["reboot_required"] = _needReboot;

    _markLastApply(true, "profile", "profile_save");
    _sendOk(req, "profile_set", "", &v_out, 200);
}

// =====================================================
// GET /api/triggers
// =====================================================
void CL_W10_WebConfig::apiTriggers(AsyncWebServerRequest* req) {
    JsonDocument v_doc;
    v_doc["count"] = (uint8_t)EN_C10_TRIG_MAX;

    JsonArray v_arr = v_doc["triggers"].to<JsonArray>();
    for (uint8_t i = 0; i < (uint8_t)EN_C10_TRIG_MAX; i++) {
        JsonObject o = v_arr.add<JsonObject>();
        o["idx"]    = i;
        o["name"]   = C10_TriggerName(i);
        o["group"]  = C10_TriggerGroup(i);
        o["locked"] = G_C10_TRIG_LOCKED[i];
    }

    _sendOk(req, "triggers", "", &v_doc, 200);
}

// =====================================================
// POST /api/action/test
// =====================================================
void CL_W10_WebConfig::apiActionTest(AsyncWebServerRequest* req,
                                     uint8_t* data, size_t len,
                                     size_t index, size_t total) {
    JsonDocument v_d;
    if (!_parseBodyAndGate(req, data, len, index, total, v_d)) return;

    const uint8_t  v_k   = (uint8_t) (v_d["k"].isNull()   ? 0 : (int)v_d["k"]);
    const uint8_t  v_h   = (uint8_t) (v_d["h"].isNull()   ? 0 : (int)v_d["h"]);
    const uint16_t v_p16 = (uint16_t)(v_d["p16"].isNull() ? 0 : (int)v_d["p16"]);
    const uint32_t v_p32 = (uint32_t)(v_d["p32"].isNull() ? 0 : (unsigned)v_d["p32"]);

    if (!_e10if || !_e10if->execLiveTest) {
        _sendErr(req, "control_failed", "Live test not available.");
        return;
    }

    const bool v_ok = _e10if->execLiveTest(_e10if->ctx, v_k, v_h, v_p16, v_p32);
    if (!v_ok) {
        bool v_otaGuard = false;
        if (_e10if->getStatus) {
            ST_E10_Status_t s;
            memset(&s, 0, sizeof(s));
            if (_e10if->getStatus(_e10if->ctx, &s)) v_otaGuard = s.ota_guard;
        }
        if (_isSafeMode() || v_otaGuard) {
            _sendErr(req, "ota_guard_blocked", "Live test blocked by gate.");
        } else {
            _sendErr(req, "ble_disconnected",
                     "Live test failed (BLE disconnected to host PC).");
        }
        return;
    }

    JsonDocument v_out;
    v_out["k"]   = v_k;
    v_out["h"]   = v_h;
    v_out["p16"] = v_p16;
    v_out["p32"] = v_p32;

    _sendOk(req, "action_test", "", &v_out, 200);
}

// =====================================================
// POST /api/action/test_macro
// =====================================================
void CL_W10_WebConfig::apiActionTestMacro(AsyncWebServerRequest* req,
                                          uint8_t* data, size_t len,
                                          size_t index, size_t total) {
    JsonDocument v_d;
    if (!_parseBodyAndGate(req, data, len, index, total, v_d)) return;

    if (v_d["idx"].isNull()) {
        _sendErr(req, "bad_json", "Missing idx.");
        return;
    }

    const uint8_t v_idx = (uint8_t)v_d["idx"];

    if (!_e10if || !_e10if->execLiveTest) {
        _sendErr(req, "control_failed", "Live test not available.");
        return;
    }

    const bool v_ok = _e10if->execLiveTest(_e10if->ctx,
                                           (uint8_t)EN_C20_ACT_MACRO,
                                           (uint8_t)EN_C20_HOLD_NONE,
                                           0,
                                           (uint32_t)v_idx);
    if (!v_ok) {
        bool v_otaGuard = false;
        if (_e10if->getStatus) {
            ST_E10_Status_t s;
            memset(&s, 0, sizeof(s));
            if (_e10if->getStatus(_e10if->ctx, &s)) v_otaGuard = s.ota_guard;
        }
        if (_isSafeMode() || v_otaGuard) {
            _sendErr(req, "ota_guard_blocked", "Macro test blocked by gate.");
        } else {
            _sendErr(req, "ble_disconnected",
                     "Macro test failed (BLE disconnected to host PC).");
        }
        return;
    }

    JsonDocument v_out;
    v_out["idx"] = v_idx;

    _sendOk(req, "action_test_macro", "", &v_out, 200);
}
