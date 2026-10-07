// =======================================================
// File: src/v0415/W10_WebApi_Profile_0415.cpp
// =======================================================
/*
 * ------------------------------------------------------
 * 소스명 : W10_WebApi_Profile_0415.cpp
 * 모듈약어 : W10
 * 모듈명 : Profile / Triggers / Live Test API (v0415)
 * ------------------------------------------------------
 * 기능 요약
 *  - /api/profiles                (GET)  목록 + active
 *  - /api/profiles/switch         (POST) 활성 전환
 *  - /api/profiles/create         (POST) 새 프로파일
 *  - /api/profiles/delete         (POST) 삭제
 *  - /api/profiles/rename         (POST) 이름 변경
 *  - /api/profiles/active         (GET)  활성 프로파일 전체 config
 *  - /api/profiles/active         (POST) 활성 프로파일 patch + 저장
 *  - /api/triggers                (GET)  트리거 라이브러리 (27)
 *  - /api/action/test             (POST) Live Test (단일 액션)
 *
 * [설계]
 *  - Profile CRUD는 _cfg 직접 사용 (C10_Config_0415)
 *  - 실제 전환/재로드는 _e10if 경유 (E10 내부 상태 안전 처리)
 *  - Live Test: SPECIAL → sensorTask 동기, 그 외 → 큐 비동기
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 *  - 함수 인자 p_, 로컬 v_
 * ------------------------------------------------------
 */

#include "W10_Web_0415.h"

// =====================================================
// GET /api/profiles
//   → { ok, data:{ active, count, profiles:[{idx, name}] } }
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
// POST /api/profiles/switch  { idx }
// =====================================================
void CL_W10_WebConfig::apiProfilesSwitch(AsyncWebServerRequest* req,
                                         uint8_t* data, size_t len,
                                         size_t index, size_t total) {
    if (_gateSafeModeOrReply(req)) return;

    String v_body;
    if (!_collectBodyOrReply(req, data, len, index, total, v_body)) return;

    JsonDocument v_d;
    DeserializationError v_err = deserializeJson(v_d, v_body);
    if (v_err) {
        _cnt_json_bad++;
        _diagPush("bad_json");
        _sendErr(req, "bad_json", "Invalid JSON.");
        return;
    }

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
        _sendErr(req, "profile_switch_failed", "Switch failed (invalid idx / in progress).");
        return;
    }

    // 로컬 _wifi/_e10 캐시 재로드
    if (_cfg) {
        ST_C10_ProfileConfig_t v_p;
        _cfg->makeDefaultsProfile(_cfg->getActiveIndex(), v_p);
        if (_cfg->loadActiveProfile(v_p)) {
            _wifi = v_p.wifi;
            _e10  = v_p.e10;
        }
    }

    JsonDocument v_out;
    v_out["idx"] = v_idx;
    _sendOk(req, "profile_switch", "", &v_out, 200);
}

// =====================================================
// POST /api/profiles/create  { name }
// =====================================================
void CL_W10_WebConfig::apiProfilesCreate(AsyncWebServerRequest* req,
                                         uint8_t* data, size_t len,
                                         size_t index, size_t total) {
    if (_gateSafeModeOrReply(req)) return;

    if (!_cfg) {
        _sendErr(req, "no_config", "Config manager not ready.");
        return;
    }

    String v_body;
    if (!_collectBodyOrReply(req, data, len, index, total, v_body)) return;

    JsonDocument v_d;
    DeserializationError v_err = deserializeJson(v_d, v_body);
    if (v_err) {
        _cnt_json_bad++;
        _diagPush("bad_json");
        _sendErr(req, "bad_json", "Invalid JSON.");
        return;
    }

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
// POST /api/profiles/delete  { idx }
// =====================================================
void CL_W10_WebConfig::apiProfilesDelete(AsyncWebServerRequest* req,
                                         uint8_t* data, size_t len,
                                         size_t index, size_t total) {
    if (_gateSafeModeOrReply(req)) return;

    if (!_cfg) {
        _sendErr(req, "no_config", "Config manager not ready.");
        return;
    }

    String v_body;
    if (!_collectBodyOrReply(req, data, len, index, total, v_body)) return;

    JsonDocument v_d;
    DeserializationError v_err = deserializeJson(v_d, v_body);
    if (v_err) {
        _cnt_json_bad++;
        _diagPush("bad_json");
        _sendErr(req, "bad_json", "Invalid JSON.");
        return;
    }

    if (v_d["idx"].isNull()) {
        _sendErr(req, "bad_json", "Missing idx.");
        return;
    }

    const uint8_t v_idx = (uint8_t)v_d["idx"];

    if (!_cfg->deleteProfile(v_idx)) {
        _sendErr(req, "profile_delete_failed", "Delete failed (min 1 profile).");
        return;
    }

    // active index가 shift될 수 있으므로 E10 재로드
    bool v_reloaded = false;
    if (_e10if && _e10if->reloadProfile) {
        v_reloaded = _e10if->reloadProfile(_e10if->ctx);
    }

    // W10 로컬 캐시 재로드
    if (_cfg) {
        ST_C10_ProfileConfig_t v_p;
        _cfg->makeDefaultsProfile(_cfg->getActiveIndex(), v_p);
        if (_cfg->loadActiveProfile(v_p)) {
            _wifi = v_p.wifi;
            _e10  = v_p.e10;
        }
    }

    JsonDocument v_out;
    v_out["count"]    = (uint8_t)_cfg->getProfileCount();
    v_out["active"]   = (uint8_t)_cfg->getActiveIndex();
    v_out["reloaded"] = v_reloaded;

    _sendOk(req, "profile_delete", "", &v_out, 200);
}

// =====================================================
// POST /api/profiles/rename  { idx, name }
// =====================================================
void CL_W10_WebConfig::apiProfilesRename(AsyncWebServerRequest* req,
                                         uint8_t* data, size_t len,
                                         size_t index, size_t total) {
    if (_gateSafeModeOrReply(req)) return;

    if (!_cfg) {
        _sendErr(req, "no_config", "Config manager not ready.");
        return;
    }

    String v_body;
    if (!_collectBodyOrReply(req, data, len, index, total, v_body)) return;

    JsonDocument v_d;
    DeserializationError v_err = deserializeJson(v_d, v_body);
    if (v_err) {
        _cnt_json_bad++;
        _diagPush("bad_json");
        _sendErr(req, "bad_json", "Invalid JSON.");
        return;
    }

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

    // active 프로파일 rename이면 E10 반영
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
//   → { ok, data:{ idx, count, config:{ ...full profile... } } }
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

    // [R2-M-3] envelope 수동 스트리밍 (JSON triple-copy → single streaming)
    //   - 기존: v_inner → v_out(복사) → _sendOk 내부 d(복사) → serialize → 3회 복사
    //   - 변경: v_inner → 직접 serialize (1회 스트리밍)
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
// POST /api/profiles/active  { ...partial profile... }
//   - 현재 활성 프로파일 위에 patch
//   - 검증 → 저장 → E10 재로드
// =====================================================
void CL_W10_WebConfig::apiProfilesActivePost(AsyncWebServerRequest* req,
                                             uint8_t* data, size_t len,
                                             size_t index, size_t total) {
    if (_gateSafeModeOrReply(req)) return;

    if (!_cfg) {
        _sendErr(req, "no_config", "Config manager not ready.");
        return;
    }

    String v_body;
    if (!_collectBodyOrReply(req, data, len, index, total, v_body)) return;

    // ---- 현재 활성 프로파일 로드 ----
    ST_C10_ProfileConfig_t v_p;
    _cfg->makeDefaultsProfile(_cfg->getActiveIndex(), v_p);
    (void)_cfg->loadActiveProfile(v_p);

    // ---- patch ----
    if (!_cfg->patchProfileFromJson(v_body, v_p)) {
        _cnt_json_bad++;
        _diagPush("bad_json");
        _sendErr(req, "bad_json", "Invalid profile JSON.");
        return;
    }

    // ---- 검증 ----
    if (!_cfg->validateProfile(v_p)) {
        _sendErr(req, "validation_failed", "Profile validation failed.");
        return;
    }

    // ---- 저장 ----
    const uint8_t v_idx = _cfg->getActiveIndex();
    if (!_cfg->saveProfile(v_idx, v_p)) {
        _sendErr(req, "profile_save_failed", "Failed to save profile.");
        return;
    }

    // ---- W10 로컬 캐시 ----
    _wifi = v_p.wifi;
    _e10  = v_p.e10;

    // ---- E10 재로드 (런타임 반영) ----
    bool v_reloaded = false;
    if (_e10if && _e10if->reloadProfile) {
        v_reloaded = _e10if->reloadProfile(_e10if->ctx);
    }

    JsonDocument v_out;
    v_out["idx"]      = v_idx;
    v_out["reloaded"] = v_reloaded;

    _markLastApply(true, "profile", "profile_save");
    _sendOk(req, "profile_set", "", &v_out, 200);
}

// =====================================================
// GET /api/triggers
//   → { ok, data:{ count, triggers:[{idx, name, group, locked}] } }
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
// POST /api/action/test  { k, h, p16, p32 }
//   - Live Test (BLE 연결 시)
//   - SPECIAL: sensorTask 즉시
//   - MACRO / 기타: 큐 경유 (비동기)
// =====================================================
void CL_W10_WebConfig::apiActionTest(AsyncWebServerRequest* req,
                                     uint8_t* data, size_t len,
                                     size_t index, size_t total) {
    if (_gateSafeModeOrReply(req)) return;

    String v_body;
    if (!_collectBodyOrReply(req, data, len, index, total, v_body)) return;

    JsonDocument v_d;
    DeserializationError v_err = deserializeJson(v_d, v_body);
    if (v_err) {
        _cnt_json_bad++;
        _diagPush("bad_json");
        _sendErr(req, "bad_json", "Invalid JSON.");
        return;
    }

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
        _sendErr(req, "ppt_test_failed", "Live test failed (BLE disconnected?).");
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
// POST /api/action/test_macro  { idx }
//   - 매크로 인덱스 직접 실행
//   - 내부적으로 /api/action/test와 동일 경로 (kind=MACRO)
//   - 실제 실행은 commTask에서 (비동기). 응답은 즉시 200.
// =====================================================
void CL_W10_WebConfig::apiActionTestMacro(AsyncWebServerRequest* req,
                                          uint8_t* data, size_t len,
                                          size_t index, size_t total) {
    if (_gateSafeModeOrReply(req)) return;

    String v_body;
    if (!_collectBodyOrReply(req, data, len, index, total, v_body)) return;

    JsonDocument v_d;
    DeserializationError v_err = deserializeJson(v_d, v_body);
    if (v_err) {
        _cnt_json_bad++;
        _diagPush("bad_json");
        _sendErr(req, "bad_json", "Invalid JSON.");
        return;
    }

    if (v_d["idx"].isNull()) {
        _sendErr(req, "bad_json", "Missing idx.");
        return;
    }

    const uint8_t v_idx = (uint8_t)v_d["idx"];

    if (!_e10if || !_e10if->execLiveTest) {
        _sendErr(req, "control_failed", "Live test not available.");
        return;
    }

    // kind=MACRO(10), p32=macro idx
    const bool v_ok = _e10if->execLiveTest(_e10if->ctx,
                                           /*kind*/ 10,
                                           /*hMode*/ 0,
                                           /*p16*/ 0,
                                           /*p32*/ (uint32_t)v_idx);
    if (!v_ok) {
        _sendErr(req, "ppt_test_failed", "Macro test failed (BLE disconnected?).");
        return;
    }

    JsonDocument v_out;
    v_out["idx"] = v_idx;

    _sendOk(req, "action_test_macro", "", &v_out, 200);
}

