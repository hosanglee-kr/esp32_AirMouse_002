// =======================================================
// File: src/v0410/C10_Config_0410.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : C10_Config_0410.h
 * 모듈약어 : C10
 * 모듈명 : Config Manager (v0410 3-Mode)
 * ------------------------------------------------------
 * 기능 요약
 *  - Config 로드/저장/검증 (Atomic + .bak rollback)
 *  - Boot state (SafeBoot / fail_count / pending)
 *  - Mode별 슬롯 매트릭스 JSON 직렬화/역직렬화
 *  - ETag (FNV-1a 32bit) / Export / Import
 * ------------------------------------------------------
 * [구현 규칙]
 *  - ArduinoJson v7.x.x 사용 (v6 이하 사용 금지)
 *  - JsonDocument 단일 타입만 사용
 *  - createNestedArray/Object/containsKey 사용 금지
 *  - memset + strlcpy 기반 안전 초기화
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 *   - namespace 명        : 모듈약어_ 접두사
 *   - 전역 상수,매크로      : G_모듈약어_ 접두사
 *   - enum 상수             : EN_모듈약어_ 접두사
 *   - 구조체                : ST_모듈약어_ 접두사
 *   - 클래스명              : CL_모듈약어_ 접두사
 *   - 클래스 private 멤버   : _ 접두사
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <string.h>
#include <strings.h>
#include <esp_system.h>

#include "C10_Def_0410.h"
#include "D10_Logger_0410.h"   // 로거 재사용

class CL_C10_Config {
  private:
    ST_C10_BootState_t _boot;

    // bootMarkOkIfGracePassed 실패 시 재시도 백오프용
    uint32_t _bootOkRetryAtMs = 0;

  public:
    CL_C10_Config();
    ~CL_C10_Config() = default;

    // ---------- lifecycle ----------
    void begin(bool p_formatOnFail = true);

    // ---------- Defaults ----------
    void makeDefaultsWiFi(ST_C10_WiFiConfig_t& p_out);
    void makeDefaultsE10 (ST_C10_E10Config_t&  p_out);
    void makeDefaultsMode(uint8_t p_modeIdx, ST_C10_ModeConfig_t& p_out);

    // ---------- Validation ----------
    bool validateWiFi(const ST_C10_WiFiConfig_t& p_w) const;
    bool validateE10 (const ST_C10_E10Config_t&  p_e) const;
    bool validateMode(const ST_C10_ModeConfig_t& p_m, uint8_t p_modeIdx) const;
    bool validateSlot(const ST_C20_ActionSlot_t& p_s) const;

    // ---------- SafeBoot APIs ----------
    bool isSafeMode() const { return _boot.safe_mode; }
    void getBootState(ST_C10_BootState_t& p_out) const { p_out = _boot; }
    bool clearSafeMode();
    bool bootMarkOkIfGracePassed(uint32_t p_graceMs);

    // ---------- Build Patched ----------
    bool buildPatchedAll(const String& p_patchJson,
                         ST_C10_WiFiConfig_t& p_wifi,
                         ST_C10_E10Config_t&  p_e10,
                         bool p_loadCurrent = true);

    bool buildPatchedE10(const String& p_patchJson,
                         ST_C10_E10Config_t&  p_e10,
                         bool p_loadCurrent = true);

    // ---------- Load/Save ----------
    bool loadAll(ST_C10_WiFiConfig_t& p_wifi, ST_C10_E10Config_t& p_e10);
    bool saveAll(const ST_C10_WiFiConfig_t& p_wifi,
                 const ST_C10_E10Config_t&  p_e10);
    bool rollbackFromBak();

    // ---------- ETag / Export / Import ----------
    bool getConfigEtag(uint32_t& p_outHash, size_t* p_outSize = nullptr);
    bool exportJson(String& p_out);
    bool importJson(const String& p_json, bool& p_saved, bool& p_applied);

    // ---------- Patchers ----------
    bool patchFromJsonWiFi(const String& p_json, ST_C10_WiFiConfig_t& p_wifi);
    bool patchFromJsonE10 (const String& p_json, ST_C10_E10Config_t&  p_e10);

    // ---------- Factory Reset ----------
    bool factoryReset(bool p_recreateDefault);

  private:
    // =====================================================
    // JSON build (struct -> JsonDocument)
    // =====================================================
    void _buildJson(const ST_C10_WiFiConfig_t& p_w,
                    const ST_C10_E10Config_t&  p_e,
                    JsonDocument& p_doc);

    void _buildModeJson(JsonObject p_parent,
                        const ST_C10_ModeConfig_t& p_m);

    void _buildSlotArray(JsonObject p_parent,
                         const char* p_key,
                         const ST_C20_ActionSlot_t* p_slots,
                         uint8_t p_count);

    void _buildSlotJson(JsonObject p_obj,
                        const ST_C20_ActionSlot_t& p_s);

    // =====================================================
    // JSON patch (JsonVariant -> struct)
    // =====================================================
    bool _patchModeJson(JsonVariantConst p_v, ST_C10_ModeConfig_t& p_m);

    bool _patchSlotArray(JsonVariantConst p_v,
                         ST_C20_ActionSlot_t* p_slots,
                         uint8_t p_count);

    bool _patchSlotJson(JsonVariantConst p_v, ST_C20_ActionSlot_t& p_s);

    // =====================================================
    // Boot state
    // =====================================================
    bool _loadBootState(ST_C10_BootState_t& p_out);
    bool _saveBootState(const ST_C10_BootState_t& p_in);

    // =====================================================
    // File utils
    // =====================================================
    bool _verifyJsonFile(const char* p_path);
    bool _copyFile(const char* p_src, const char* p_dst);

    // =====================================================
    // Reset reason
    // =====================================================
    bool    _isBadResetReason(esp_reset_reason_t p_r);
    uint8_t _getResetReasonU8();
};