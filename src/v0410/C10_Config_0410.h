// File: src/v0410/C10_Config_0410.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : C10_Config_0410.h
 * 모듈약어 : C10
 * 모듈명 : Config Manager (v0410 프로파일 + 매크로)
 * ------------------------------------------------------
 * 기능 요약
 *  - 멀티 프로파일 CRUD 및 활성 프로파일 영구 전환
 *  - Global + Override 슬롯 관리 및 유효성 검증
 *  - 매크로 라이브러리 CRUD 및 Live Test 지원
 *  - 레거시 단일 config 파일 무시 삭제 및 클린 초기화
 *  - SafeBoot 및 원자적(Atomic) 파일 쓰기 보장
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <string.h>
#include <esp_system.h>

#include "C10_Def_0410.h"
#include "D10_Logger_0410.h"

class CL_C10_Config {
  private:
    ST_C10_BootState_t     _boot;
    ST_C10_ProfileIndex_t  _index;
    ST_C10_ProfileConfig_t _activeProfile;   // 런타임 캐시

    uint32_t _bootOkRetryAtMs = 0;
    volatile bool _profileSwitchInProgress = false;

  public:
    CL_C10_Config();
    ~CL_C10_Config() = default;

    // ---------- Lifecycle ----------
    void begin(bool p_formatOnFail = true);

    // ---------- Profile Index ----------
    uint8_t getProfileCount() const { return _index.profileCount; }
    uint8_t getActiveIndex()  const { return _index.activeIndex;  }
    bool    setActiveIndex(uint8_t p_idx);

    // ---------- Profile CRUD ----------
    bool loadProfile(uint8_t p_idx, ST_C10_ProfileConfig_t& p_out);
    bool saveProfile(uint8_t p_idx, const ST_C10_ProfileConfig_t& p_in);
    bool createProfile(const char* p_newName, uint8_t& p_outIdx);
    bool duplicateProfile(uint8_t p_srcIdx, const char* p_newName, uint8_t& p_outIdx);
    bool deleteProfile(uint8_t p_idx);
    bool renameProfile(uint8_t p_idx, const char* p_newName);

    bool loadActiveProfile(ST_C10_ProfileConfig_t& p_out);
    bool saveActiveProfile(const ST_C10_ProfileConfig_t& p_in);
    const ST_C10_ProfileConfig_t& getActiveProfile() const { return _activeProfile; }

    // ---------- Defaults Generator ----------
    void makeDefaultsWiFi  (ST_C10_WiFiConfig_t&   p_out);
    void makeDefaultsE10   (ST_C10_E10Config_t&    p_out);
    void makeDefaultsSlots (ST_C10_ProfileSlots_t& p_out);
    void makeDefaultsMacros(ST_C10_MacroLib_t&     p_out);
    void makeDefaultsProfile(uint8_t p_idx, ST_C10_ProfileConfig_t& p_out);

    // ---------- Validation ----------
    bool validateWiFi     (const ST_C10_WiFiConfig_t&    p_w) const;
    bool validateE10      (const ST_C10_E10Config_t&     p_e) const;
    bool validateSlot     (const ST_C20_ActionSlot_t&    p_s, uint8_t p_macroCount = 0) const;
    bool validateSlots    (const ST_C10_ProfileSlots_t&  p_s, uint8_t p_macroCount = 0) const;
    bool validateMacroStep(const ST_C10_MacroStep_t&     p_s) const;
    bool validateMacro    (const ST_C10_Macro_t&         p_m) const;
    bool validateMacros   (const ST_C10_MacroLib_t&      p_m) const;
    bool validateProfile  (const ST_C10_ProfileConfig_t& p_p) const;

    // ---------- SafeBoot ----------
    bool isSafeMode() const { return _boot.safe_mode; }
    void getBootState(ST_C10_BootState_t& p_out) const { p_out = _boot; }
    bool clearSafeMode();
    bool bootMarkOkIfGracePassed(uint32_t p_graceMs);

    // ---------- JSON Patcher / Builder ----------
    bool patchProfileFromJson(const String& p_json, ST_C10_ProfileConfig_t& p_io);
    bool buildProfileJson(const ST_C10_ProfileConfig_t& p_in, JsonDocument& p_out);

    // ---------- State Guard ----------
    bool isProfileSwitchInProgress() const { return _profileSwitchInProgress; }
    void setProfileSwitchInProgress(bool p_flag) { _profileSwitchInProgress = p_flag; }

    // ---------- Factory Reset ----------
    bool factoryReset(bool p_recreateDefault = true);

  private:
    bool _loadBootState(ST_C10_BootState_t& p_out);
    bool _saveBootState(const ST_C10_BootState_t& p_in);
    bool _loadIndex();
    bool _saveIndex();

    bool _verifyJsonFile(const char* p_path);
    bool _copyFile(const char* p_src, const char* p_dst);
    bool _atomicWriteJson(const char* p_path, const char* p_tmpPath, JsonDocument& p_doc);

    bool    _isBadResetReason(esp_reset_reason_t p_r);
    uint8_t _getResetReasonU8();


    void _buildWiFiJson    (JsonObject p_parent, const ST_C10_WiFiConfig_t& p_in);
    void _buildE10Json     (JsonObject p_parent, const ST_C10_E10Config_t&  p_in);
    void _buildSlotsJson   (JsonObject p_parent, const ST_C10_ProfileSlots_t& p_in);
    void _buildSlotArrayJson(JsonArray p_arr, const ST_C20_ActionSlot_t* p_arrSlots, uint8_t p_count);
    void _buildSlotJson    (JsonObject p_obj, const ST_C20_ActionSlot_t& p_in);
    void _buildMacrosJson  (JsonArray p_arr, const ST_C10_MacroLib_t& p_in);
    void _buildMacroJson   (JsonObject p_obj, const ST_C10_Macro_t& p_in);
    void _buildMacroStepJson(JsonObject p_obj, const ST_C10_MacroStep_t& p_in);

    bool _patchWiFiJson    (JsonVariantConst p_v, ST_C10_WiFiConfig_t&   p_io);
    bool _patchE10Json     (JsonVariantConst p_v, ST_C10_E10Config_t&    p_io);
    bool _patchSlotsJson   (JsonVariantConst p_v, ST_C10_ProfileSlots_t& p_io);
    bool _patchSlotArrayJson(JsonVariantConst p_v, ST_C20_ActionSlot_t* p_arr, uint8_t p_count);
    bool _patchSlotJson    (JsonVariantConst p_v, ST_C20_ActionSlot_t&   p_io);
    bool _patchMacrosJson  (JsonVariantConst p_v, ST_C10_MacroLib_t&     p_io);
    bool _patchMacroJson   (JsonVariantConst p_v, ST_C10_Macro_t&        p_io);
    bool _patchMacroStepJson(JsonVariantConst p_v, ST_C10_MacroStep_t&   p_io);
};
