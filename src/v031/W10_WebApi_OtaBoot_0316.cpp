// =======================================================
// File: W10_WebApi_OtaBoot_0316.cpp
// =======================================================

/*
 * ------------------------------------------------------
 * 소스명 : W10_WebApi_OtaBoot_0316.cpp
 * 모듈약어 : W10
 * 모듈명 : Web Config/Status/UI/OTA Server (Split: OTA + SafeBoot/FactoryReset/Reboot)
 * ------------------------------------------------------
 * 기능 요약
 *  - (0316) OTA 업로드/상태 + SafeBoot/FactoryReset/Reboot 분리
 * ------------------------------------------------------
 * [구현 규칙]
 *  - 항상 소스 시작 주석 부분 체계 유지 및 내용 업데이트
 *  - 소스 시작 주석 부분 구현규칙, 코드네이밍규칙 내용 그대로 유지, 수정금지
 *  - ArduinoJson v7.x.x 사용 (v6 이하 사용 금지)
 *  - JsonDocument 단일 타입만 사용
 *  - createNestedArray/Object/containsKey 사용 금지
 *  - memset + strlcpy 기반 안전 초기화
 *  - 주석/필드명은 JSON 구조와 동일하게 유지
 *  - 변수명은 가능한 해석 가능하게
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

#include "W10_Web_0315.h"


// - apiOtaUpload
// - apiOtaStatus
// - apiSafeBootGet / apiSafeBootPost
// - apiFactoryReset
// - _taskReboot
// - apiRebootPost
// - apiRebootCheck

// =====================================================
// OTA
// =====================================================
void CL_W10_WebConfig::apiOtaUpload(AsyncWebServerRequest* req, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
    if (!_isSafeMode()) {
        ST_W10_E10If_t* e10if = _e10if;
        if (e10if && e10if->getStatus) {
            ST_E10_Status_t s;
            memset(&s, 0, sizeof(s));
            if (e10if->getStatus(e10if->ctx, &s)) {
                if (s.ota_guard) {
                    _cnt_ota_blocked++;
                    _diagPush("ota_guard_blocked");

                    // 업로드 콜백에서는 send 금지. 상태만 기록.
                    if (index == 0) {
                        _otaInProgress = false;
                        _otaOk = false;
                        _otaWritten = 0;
                        _otaTotal = (uint32_t)req->contentLength();
                        strlcpy(_otaErr, "ota_guard", sizeof(_otaErr));
                    }
                    return;
                }
            }
        }
    }

    (void)filename;

    if (index == 0) {
        if (_otaInProgress) {
            _otaOk = false;
            strlcpy(_otaErr, "busy", sizeof(_otaErr));
            return;
        }

        _otaInProgress = true;
        _otaWritten = 0;
        _otaTotal = (uint32_t)req->contentLength();
        _otaOk = false;
        strlcpy(_otaErr, "in_progress", sizeof(_otaErr));

        size_t sketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (_otaTotal == 0 || _otaTotal > (uint32_t)sketchSpace) {
            _otaOk = false;
            strlcpy(_otaErr, "size_invalid", sizeof(_otaErr));
            _otaInProgress = false;
            return;
        }

        if (!Update.begin(sketchSpace)) {
            _otaOk = false;
            strlcpy(_otaErr, "Update.begin failed", sizeof(_otaErr));
            _otaInProgress = false;
            return;
        }
    }

    if (len) {
        size_t w = Update.write(data, len);
        _otaWritten += (uint32_t)w;
        if (w != len) strlcpy(_otaErr, "Update.write mismatch", sizeof(_otaErr));
    }

    if (final) {
        if (!Update.end(true)) {
            strlcpy(_otaErr, "Update.end failed", sizeof(_otaErr));
            _otaOk = false;
        } else if (Update.hasError()) {
            strlcpy(_otaErr, "Update.hasError", sizeof(_otaErr));
            _otaOk = false;
        } else {
            _otaOk = true;
            strlcpy(_otaErr, "ok", sizeof(_otaErr));
        }
        _otaInProgress = false;
    }
}

void CL_W10_WebConfig::apiOtaStatus(AsyncWebServerRequest* req) {
    JsonDocument v_doc;
    v_doc["in_progress"] = _otaInProgress;
    v_doc["total"] = (uint32_t)_otaTotal;
    v_doc["written"] = (uint32_t)_otaWritten;
    v_doc["ok"] = _otaOk;
    v_doc["err"] = _otaErr;

    _sendOk(req, "ota_status", "", &v_doc, 200);
}


// =====================================================
// SafeBoot / FactoryReset
// =====================================================
void CL_W10_WebConfig::apiSafeBootGet(AsyncWebServerRequest* req) {
    if (_cfg) {
        ST_C10_BootState_t bs;
        _cfg->getBootState(bs);
        JsonDocument v_doc;
        v_doc["safe_mode"] = bs.safe_mode;
        v_doc["fail_count"] = bs.fail_count;
        v_doc["pending"] = bs.pending;
        _sendOk(req, "safeboot", "", &v_doc, 200);
        return;
    }
    _sendErr(req, "no_config", "Config manager not ready.");
}

void CL_W10_WebConfig::apiSafeBootPost(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
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

    bool v_exit = false;
    if (!d["exit"].isNull()) v_exit = (bool)d["exit"];

    bool ok = false;
    if (_cfg && v_exit) {
        ok = _cfg->clearSafeMode();
    }

    JsonDocument v_doc;
    v_doc["exit"] = v_exit;
    v_doc["note"] = "exit=true -> clears safe_mode and reboots.";
    if (ok) {
        _markLastApply(true, "safeboot", "safeboot_exit");
        _sendOk(req, "safeboot_exit", "", &v_doc, 200);
    } else {
        _markLastApply(false, "safeboot", "safeboot_exit_failed");
        _sendErr(req, "safeboot_exit_failed", "Failed.", &v_doc);
    }

    if (ok && v_exit) {
        delay(150);
        ESP.restart();
    }
}



void CL_W10_WebConfig::apiFactoryReset(AsyncWebServerRequest* req) {
    bool ok = (_cfg && _cfg->factoryReset(true));

    JsonDocument v_doc;
    v_doc["note"] = "Factory reset done. Rebooting...";
    if (ok) {
        _markLastApply(true, "factory", "factory_reset");
        _sendOk(req, "factory_reset", "", &v_doc, 200);
    } else {
        _markLastApply(false, "factory", "factory_reset_failed");
        _sendErr(req, "factory_reset_failed", "Failed.", &v_doc);
    }

    if (ok) {
        delay(200);
        ESP.restart();
    }
}


// =====================================================
// /api/reboot (POST)
// =====================================================
void CL_W10_WebConfig::_taskReboot(void* p_arg) {
    (void)p_arg;
    delay(200);
    ESP.restart();
}

void CL_W10_WebConfig::apiRebootPost(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
    String v_body;
    if (!_collectBodyOrReply(req, data, len, index, total, v_body)) return;

    bool v_force = false;
    uint32_t v_mask = 0;
    bool v_hasMask = false;

    if (v_body.length() > 0) {
        JsonDocument in;
        DeserializationError err = deserializeJson(in, v_body);
        if (err) {
            _cnt_json_bad++;
            _diagPush("bad_json");
            _sendErr(req, "bad_json", "Invalid JSON.");
            return;
        }

        JsonVariant v_rm = in["reason_mask"];
        if (!v_rm.isNull()) {
            v_mask = (uint32_t)v_rm.as<uint32_t>();
            v_hasMask = true;
        }

        JsonVariant v_f = in["force"];
        if (!v_f.isNull()) v_force = v_f.as<bool>();
    }

    if (!v_force && !v_hasMask) {
        if (!_needReboot) {
            JsonDocument v_doc;
            v_doc["need_reboot"] = _needReboot;
            v_doc["need_reboot_mask"] = (uint32_t)_needRebootMask;
            v_doc["reboot_reasons"] = _rebootReasonsString(_needRebootMask);
            _sendErr(req, "no_reboot_needed", "Reboot is not required.", &v_doc);
            return;
        }
    }

    if (!v_force && v_hasMask) {
        if (((_needRebootMask & v_mask) != v_mask)) {
            JsonDocument v_doc;
            v_doc["need_reboot"] = _needReboot;
            v_doc["need_reboot_mask"] = (uint32_t)_needRebootMask;
            _sendErr(req, "reason_mask_mismatch", "Reboot is not allowed for the given reason_mask.", &v_doc);
            return;
        }
    }

    JsonDocument v_doc;
    v_doc["need_reboot"] = _needReboot;
    v_doc["need_reboot_mask"] = (uint32_t)_needRebootMask;
    _sendOk(req, "reboot_scheduled", "Reboot scheduled.", &v_doc, 200);

    xTaskCreatePinnedToCore(_taskReboot, "w10_reboot", 2048, nullptr, 1, nullptr, 0);
}

void CL_W10_WebConfig::apiRebootCheck(AsyncWebServerRequest* req) {
    JsonDocument v_doc;
    v_doc["required"] = _needReboot;
    v_doc["mask"] = (uint32_t)_needRebootMask;
    v_doc["reasons"] = _rebootReasonsString(_needRebootMask);

    v_doc["allowed"] = _needReboot;
    v_doc["deny_code"] = _needReboot ? "" : "no_reboot_needed";

    _sendOk(req, "reboot_check", "", &v_doc, 200);
}

