// =======================================================
// File: src/v0415/W10_WebApi_OtaBoot_0415.cpp
// =======================================================
/*
 * ------------------------------------------------------
 * 소스명 : W10_WebApi_OtaBoot_0415.cpp
 * 모듈약어 : W10
 * 모듈명 : OTA + SafeBoot/FactoryReset/Reboot API
 * ------------------------------------------------------
 * [v0415 Phase 2-1]
 *  - apiSafeBootPost / apiRebootPost: _parseBodyAndGate 사용
 *  - apiRebootPost: Body 없어도 동작 가능 (빈 JSON 허용)
 * ------------------------------------------------------
 */

#include "W10_Web_0415.h"
#include "D10_Logger_0415.h"

// =====================================================
// OTA
// =====================================================
void CL_W10_WebConfig::apiOtaUpload(AsyncWebServerRequest* req,
                                    const String& filename,
                                    size_t index, uint8_t* data,
                                    size_t len, bool final) {
    (void)filename;

    if (index == 0) {
        if (!_isSafeMode()) {
            ST_W10_E10If_t* e10if = _e10if;
            if (e10if && e10if->getStatus) {
                ST_E10_Status_t s;
                memset(&s, 0, sizeof(s));
                if (e10if->getStatus(e10if->ctx, &s) && s.ota_guard) {
                    _cnt_ota_blocked++;
                    _diagPush("ota_guard_blocked");

                    _otaInProgress = false;
                    _otaOk = false;
                    _otaWritten = 0;
                    _otaTotal = (uint32_t)req->contentLength();
                    strlcpy(_otaErr, "ota_guard", sizeof(_otaErr));
                    return;
                }
            }
        }

        const uint32_t v_now = (uint32_t)millis();

        if (_otaInProgress) {
            if ((v_now - _otaStartedMs) < 30000u) {
                _otaOk = false;
                strlcpy(_otaErr, "busy", sizeof(_otaErr));
                return;
            }
            Update.abort();
            _otaInProgress = false;
            _otaWritten = 0;
        }

        _otaStartedMs = v_now;
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
// SafeBoot
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

void CL_W10_WebConfig::apiSafeBootPost(AsyncWebServerRequest* req,
                                       uint8_t* data, size_t len,
                                       size_t index, size_t total) {
    JsonDocument d;
    if (!_parseBodyAndGate(req, data, len, index, total, d)) return;

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
        D10_LOGI("[W10] SafeBoot exit requested. Rebooting...");
        delay(150);
        ESP.restart();
    }
}

// =====================================================
// Factory Reset
// =====================================================
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
        D10_LOGW("[W10] Factory reset requested. Rebooting...");
        delay(200);
        ESP.restart();
    }
}

// =====================================================
// POST /api/reboot
// =====================================================
void CL_W10_WebConfig::_taskReboot(void* p_arg) {
    (void)p_arg;
    delay(200);
    ESP.restart();
}

void CL_W10_WebConfig::apiRebootPost(AsyncWebServerRequest* req,
                                     uint8_t* data, size_t len,
                                     size_t index, size_t total) {
    // [Phase 2-1] SafeMode gate + Body + JSON parse 통합
    //   ※ Body가 빈 경우(force=false + mask 미지정)에도 동작해야 함
    //     → _parseBodyAndGate는 빈 body에 대해 bad_json 실패를 반환하므로
    //       여기서는 개별 처리 유지
    if (_gateSafeModeOrReply(req)) return;

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
            _sendErr(req, "reason_mask_mismatch",
                     "Reboot is not allowed for the given reason_mask.", &v_doc);
            return;
        }
    }

    JsonDocument v_doc;
    v_doc["need_reboot"] = _needReboot;
    v_doc["need_reboot_mask"] = (uint32_t)_needRebootMask;
    _sendOk(req, "reboot_scheduled", "Reboot scheduled.", &v_doc, 200);

    BaseType_t v_rc = xTaskCreatePinnedToCore(_taskReboot, "w10_reboot",
                                              2048, nullptr, 1, nullptr, 0);
    if (v_rc == pdPASS) {
        D10_LOGI("[W10] Reboot task scheduled successfully");
    } else {
        D10_LOGE("[W10] Failed to create reboot task (rc=%d), executing sync restart",
                 (int)v_rc);
        delay(200);
        ESP.restart();
    }
}
