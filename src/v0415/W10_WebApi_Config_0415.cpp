// =======================================================
// File: src/v0415/W10_WebApi_Config_0415.cpp
// =======================================================
/*
 * ------------------------------------------------------
 * 소스명 : W10_WebApi_Config_0415.cpp
 * 모듈약어 : W10
 * 모듈명 : Web Config API (v0415: 프로파일 기반)
 * ------------------------------------------------------
 * 기능 요약
 *  - /api/config          (GET)  활성 프로파일 raw JSON
 *  - /api/config/save     (POST) 활성 프로파일 patch + 저장 + E10 재로드
 *  - /api/config/apply    (POST) save 별칭 (v0412: no-save 폐기)
 *  - /api/config/export   (GET)  활성 프로파일 다운로드
 *  - /api/export          (GET)  별칭
 *  - /api/config/import   (POST) JSON → 새 프로파일 생성
 *  - /api/config/rollback (POST) v0412 폐기 (안내 응답)
 *
 * [v0400 → v0412 주요 변경]
 *  - C10_Config의 loadAll/saveAll/exportJson/importJson → 프로파일 메서드로 대체
 *  - ETag/Envelope 폐기 (클라이언트 단순화)
 *  - apply = save (프로파일 시스템에선 apply-only 의미 없음)
 *  - import = 새 프로파일 생성 (덮어쓰기 아님)
 *  - rollback 폐기 (프로파일 switch로 대체)
 * ------------------------------------------------------
 * [구현 규칙]
 *  - 항상 소스 시작 주석 부분 체계 유지 및 내용 업데이트
 *  - 소스 시작 주석 부분 구현규칙, 코드네이밍규칙 내용 그대로 유지, 수정금지
 *  - ArduinoJson v7.x.x 사용 (v6 이하 사용 금지)
 *  - JsonDocument 단일 타입만 사용
 *  - createNestedArray/Object/containsKey 사용 금지
 *  - memset + strlcpy 기반 안전 초기화
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

#include "W10_Web_0415.h"


void CL_W10_WebConfig::_bumpRebootMaskFromBoot() {
    const uint32_t v_m = _wifiDiffMask(_bootWifi, _wifi);
    _needRebootMask = v_m;
    _needReboot     = (v_m != 0);
}

// =====================================================
// GET /api/config — 활성 프로파일 조회 (raw JSON)
//   반환: { ver, name, wifi, e10, slots, macros }
// =====================================================
void CL_W10_WebConfig::apiGetConfig(AsyncWebServerRequest* req) {
    if (!_cfg) {
        _sendErr(req, "no_config", "Config manager not ready.");
        return;
    }

    ST_C10_ProfileConfig_t v_p;
    _cfg->makeDefaultsProfile(_cfg->getActiveIndex(), v_p);
    if (!_cfg->loadActiveProfile(v_p)) {
        _sendErr(req, "config_get_failed", "Failed to load active profile.");
        return;
    }

    JsonDocument v_doc;
    if (!_cfg->buildProfileJson(v_p, v_doc)) {
        _sendErr(req, "config_get_failed", "Failed to serialize profile.");
        return;
    }

    AsyncResponseStream* res = req->beginResponseStream("application/json");
    res->setCode(200);
    res->addHeader("Cache-Control", G_W10_CACHE_NOSTORE);
    serializeJson(v_doc, *res);
    req->send(res);
}

// =====================================================
// POST /api/config/save — 활성 프로파일 patch + 저장 + E10 재로드
//   Body: partial profile JSON
// =====================================================
void CL_W10_WebConfig::apiConfigSave(AsyncWebServerRequest* req,
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

    const ST_C10_WiFiConfig_t v_prevWifi = v_p.wifi;

    // ---- patch ----
    if (!_cfg->patchProfileFromJson(v_body, v_p)) {
        _cnt_json_bad++;
        _diagPush("bad_json");
        _sendErr(req, "bad_json", "Invalid profile JSON.");
        return;
    }

    // ---- validate ----
    if (!_cfg->validateProfile(v_p)) {
        _sendErr(req, "validation_failed", "Validation failed.");
        return;
    }

    // ---- save ----
    const uint8_t v_idx = _cfg->getActiveIndex();
    if (!_cfg->saveProfile(v_idx, v_p)) {
        _markLastApply(false, "save", "config_save_failed");
        _sendErr(req, "config_save_failed", "Save failed.");
        return;
    }

    // ---- W10 로컬 캐시 갱신 ----
    _wifi = v_p.wifi;
    _e10  = v_p.e10;

    // ---- WiFi diff → reboot 필요 여부 ----
    // 매 저장 시 boot 스냅샷과 비교 → 되돌림 시 마스크 자동 클리어
    (void)v_prevWifi;  // 참조는 유지 (디버그 목적)
    _bumpRebootMaskFromBoot();

    // ---- E10 재로드 (런타임 반영) ----
    bool v_reloaded = false;
    if (_e10if && _e10if->reloadProfile) {
        v_reloaded = _e10if->reloadProfile(_e10if->ctx);
    }

    JsonDocument v_out;
    v_out["saved"]           = true;
    v_out["reloaded"]        = v_reloaded;
    v_out["idx"]             = v_idx;
    v_out["reboot_required"] = _needReboot;

    _markLastApply(true, "save", "config_save");
    _sendOk(req, "config_save", "", &v_out, 200);
}

// =====================================================
// POST /api/config/apply — v0412: save 별칭
// =====================================================
void CL_W10_WebConfig::apiConfigApply(AsyncWebServerRequest* req,
                                      uint8_t* data, size_t len,
                                      size_t index, size_t total) {
    apiConfigSave(req, data, len, index, total);
}

// =====================================================
// GET /api/config/export, /api/export — 활성 프로파일 다운로드
//   Query: filename=<optional>
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
// POST /api/config/import — JSON으로 새 프로파일 생성
//   Body: 완전한 프로파일 JSON (ver, name, wifi, e10, slots, macros)
//   실패 시 생성했던 프로파일 자동 롤백
// =====================================================
void CL_W10_WebConfig::apiImport(AsyncWebServerRequest* req,
                                 uint8_t* data, size_t len,
                                 size_t index, size_t total) {
    if (_gateSafeModeOrReply(req)) return;

    if (!_cfg) {
        _sendErr(req, "no_config", "Config manager not ready.");
        return;
    }

    if (_cfg->getProfileCount() >= C10_DEF::PROFILE_MAX) {
        _sendErr(req, "config_import_failed", "Max profiles reached.");
        return;
    }

    String v_body;
    if (!_collectBodyOrReply(req, data, len, index, total, v_body)) return;

    // ---- JSON parse ----
    JsonDocument v_doc;
    DeserializationError v_err = deserializeJson(v_doc, v_body);
    if (v_err) {
        _cnt_json_bad++;
        _diagPush("bad_json");
        _sendErr(req, "bad_json", "Invalid JSON.");
        return;
    }

    // ---- 새 프로파일 생성 (name은 JSON에서) ----
    const char* v_name = nullptr;
    if (!v_doc["name"].isNull()) v_name = (const char*)v_doc["name"];

    uint8_t v_newIdx = 0;
    if (!_cfg->createProfile(v_name, v_newIdx)) {
        _sendErr(req, "config_import_failed", "Create failed.");
        return;
    }

    // ---- 생성된 프로파일 로드 ----
    ST_C10_ProfileConfig_t v_p;
    _cfg->makeDefaultsProfile(v_newIdx, v_p);
    if (!_cfg->loadProfile(v_newIdx, v_p)) {
        (void)_cfg->deleteProfile(v_newIdx);
        _sendErr(req, "config_import_failed", "Load failed.");
        return;
    }

    // ---- patch with imported JSON ----
    if (!_cfg->patchProfileFromJson(v_body, v_p)) {
        (void)_cfg->deleteProfile(v_newIdx);
        _sendErr(req, "bad_json", "Patch failed.");
        return;
    }

    // ---- validate ----
    if (!_cfg->validateProfile(v_p)) {
        (void)_cfg->deleteProfile(v_newIdx);
        _sendErr(req, "validation_failed", "Validation failed.");
        return;
    }

    // ---- save ----
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
// POST /api/config/rollback — v0412 폐기
// =====================================================
void CL_W10_WebConfig::apiRollback(AsyncWebServerRequest* req) {
    (void)req;
    _sendErr(req, "config_rollback_failed",
             "Rollback not supported in v0412. Use profile switch or factory_reset.");
}
