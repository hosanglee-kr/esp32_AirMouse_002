// File: src/v0415/C10_Config_Json_0415.cpp
// =======================================================
/*
 * ------------------------------------------------------
 * 소스명 : C10_Config_Json_0415.cpp
 * 모듈약어 : C10
 * 모듈명 : Config Manager (v0415) - JSON 직렬화/역직렬화
 * ------------------------------------------------------
 * 기능 요약
 *  - 프로파일 JSON 직렬화 (struct → JsonDocument)
 *    · _buildWiFiJson / _buildE10Json / _buildSlotsJson / _buildMacrosJson
 *    · buildProfileJson (엔트리 포인트)
 *  - 프로파일 JSON 역직렬화 (JsonDocument → struct)
 *    · _patchWiFiJson / _patchE10Json / _patchSlotsJson / _patchMacrosJson
 *    · patchProfileFromJson (엔트리 포인트)
 *  - Consumer mask 24-bit 상한 클램프
 *  - E10 JSON 대칭성 계약 (build ↔ patch 필드 파리티)
 *
 * [v0415 rev18.1 - Phase 5-A 분할]
 *  - 본 .cpp는 C10_Config_0415.cpp에서 JSON 파트 분리 (3-TU 분할)
 *    · C10_Config_0415.cpp           : lifecycle / CRUD / File IO / Hosts
 *    · C10_Config_Json_0415.cpp      : 본 파일 (JSON 직렬화/역직렬화)
 *    · C10_Config_Validate_0415.cpp  : validate* + G_E10_RULES[] (A-4)
 *  - 마이그레이션 함수 삭제에 따라 patchProfileFromJson은 순수 patch만 수행
 *    · ver<411 파일의 미지원 필드는 makeDefaultsE10 기본값으로 자동 fallback
 *  - 스키마 412: gesture.* 삭제, flick.* SSOT
 *    · _buildE10Json / _patchE10Json 모두 flick 블록 단일 참조
 *
 * [구현 규칙]
 *  - ArduinoJson v7.x.x 사용 (v6 이하 사용 금지)
 *  - JsonDocument 단일 타입만 사용
 *  - createNestedArray/Object/containsKey 사용 금지
 *  - memset + strlcpy 기반 안전 초기화
 *  - 주석/필드명은 JSON 구조와 동일하게 유지
 *  - 변수명은 가능한 해석 가능하게
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 *   - namespace 명         : 모듈약어_ 접두사
 *   - 전역 상수,매크로       : G_모듈약어_ 접두사
 *   - 전역 함수             : 모듈약어_ 접두사
 *   - enum 상수             : EN_모듈약어_ 접두사
 *   - 구조체                : ST_모듈약어_ 접두사
 *   - 클래스명              : CL_모듈약어_ 접두사 , 버전 제거
 *   - 클래스 private 멤버    : _ 접두사
 *   - 클래스 정적 멤버       : s_ 접두사
 *   - 함수 로컬 변수         : v_ 접두사
 *   - 함수 인자             : p_ 접두사
 * ------------------------------------------------------
 */

#include "C10_Config_0415.h"


// =======================================================
// JSON Build (struct → Json)
// =======================================================
void CL_C10_Config::_buildSlotJson(JsonObject p_obj, const ST_C20_ActionSlot_t& p_in) {
    p_obj["k"]   = p_in.kind;
    p_obj["h"]   = p_in.holdMode;
    p_obj["p16"] = p_in.param16;
    p_obj["p32"] = (uint32_t)p_in.param32;
}

void CL_C10_Config::_buildSlotArrayJson(JsonArray p_arr,
                                        const ST_C20_ActionSlot_t* p_arrSlots,
                                        uint8_t p_count) {
    for (uint8_t i = 0; i < p_count; i++) {
        JsonObject o = p_arr.add<JsonObject>();
        _buildSlotJson(o, p_arrSlots[i]);
    }
}

void CL_C10_Config::_buildWiFiJson(JsonObject p_parent, const ST_C10_WiFiConfig_t& p_in) {
    p_parent["mode"] = p_in.mode;

    JsonObject v_sta = p_parent["sta"].to<JsonObject>();
    v_sta["ssid"] = p_in.sta_ssid;
    v_sta["pass"] = p_in.sta_pass;

    JsonObject v_ap = p_parent["ap"].to<JsonObject>();
    v_ap["ssid"] = p_in.ap_ssid;
    v_ap["pass"] = p_in.ap_pass;

    JsonObject v_md = p_parent["mdns"].to<JsonObject>();
    v_md["host"] = p_in.mdns_host;
}

// =====================================================
// [v0415 Phase 3-D] E10 JSON 대칭성 계약 (Field Parity Contract)
// -----------------------------------------------------
//  ▸ 원칙: 이 목록의 모든 필드는
//      - _buildE10Json  (직렬화)에 반드시 존재
//      - _patchE10Json  (역직렬화)에 반드시 존재
//      - makeDefaultsE10(기본값)에 반드시 존재
//    3자 중 하나라도 누락 시 저장/로드 불일치 발생.
//
//  ▸ 신규 필드 추가 절차:
//      1) 이 목록에 추가
//      2) 3개 함수 동시 수정
//      3) check_schema_0415.py::check_e10_json_parity 실행
//
//  ▸ 필드 목록 (JSON 경로 기준):
//      dpi_level, hard_click_lock,
//      scale_base[3], accel_gain[3], accel_threshold,
//      wheel.threshold_deg, wheel.step_max,
//      scroll_cursor_damp,
//      precision.mode, precision.deadzone, precision.gain,
//      precision.accel, precision.max_step, precision.smooth,
//      precision.entry_ms, precision.exit_ms,
//      precision.entry_still_deg, precision.exit_move_deg,
//      precision.profile,
//      led_brightness, battery_adc_enabled,
//      gyro_bias.still_th, gyro_bias.still_win_ms, gyro_bias.alpha,
//      linear.th, linear.impulse_th, linear.window_ms,
//      flick.p2p_th, flick.window_ms, flick.cooldown_ms,
//      tilt_hold.angle_deg, tilt_hold.hold_ms, tilt_hold.repeat_hz,
//      sleep_idle_timeout_ms, active_mode, active_peer_index,
//      motion_adv.click_freeze.{enable,gyro_th,max_ms,hold_ms,
//                               fadeout_ms,move_th,freeze_move_th},
//      motion_adv.ema.{alpha_min,alpha_max,deadzone_th,fast_th,
//                      reversal_th,reversal_reset},
//      motion_adv.snap.{enable,mode_mask,axis_mode,confirm_frames,
//                       ratio_enter,strength},
//      power.idle_timeout_ms[3], power.idle_timeout_ble_ms,
//      power.pairing_idle_timeout_ms, power.deep_idle_timeout_ms,
//      power.wake_min_active_ms, power.wom_threshold,
//      power.wom_duration, power.fast_recalib_ms,
//      power.led_fadeout_ms, power.led_fadein_ms,
//      button.debounce_press_ms, button.debounce_release_ms,
//      button.long_delay_ms, button.double_delay_ms,
//      button.hold_2s_ms, button.hold_3s_ms,
//      button.min_click_ms, button.debounce_min_ticks
// =====================================================
void CL_C10_Config::_buildE10Json(JsonObject p_parent, const ST_C10_E10Config_t& p_in) {
    p_parent["dpi_level"]       = p_in.dpi_level;
    p_parent["hard_click_lock"] = p_in.hard_click_lock;

    JsonArray v_sb = p_parent["scale_base"].to<JsonArray>();
    v_sb.add(p_in.scale_base[0]); v_sb.add(p_in.scale_base[1]); v_sb.add(p_in.scale_base[2]);

    JsonArray v_ag = p_parent["accel_gain"].to<JsonArray>();
    v_ag.add(p_in.accel_gain[0]); v_ag.add(p_in.accel_gain[1]); v_ag.add(p_in.accel_gain[2]);

    p_parent["accel_threshold"]    = p_in.accel_threshold;
    p_parent["scroll_cursor_damp"] = p_in.scroll_cursor_damp;

    JsonObject v_w = p_parent["wheel"].to<JsonObject>();
    v_w["threshold_deg"] = p_in.wheel_threshold_deg;
    v_w["step_max"]      = p_in.wheel_step_max;

    // [v0415 rev15 / 스키마 412] gesture JSON 블록 삭제
    //   - flick 블록이 SSOT (하단 v_fl 참조)

    JsonObject v_p = p_parent["precision"].to<JsonObject>();
    v_p["mode"]             = p_in.precision_mode;
    v_p["deadzone"]         = p_in.precision_deadzone;
    v_p["gain"]             = p_in.precision_gain;
    v_p["accel"]            = p_in.precision_accel;
    v_p["max_step"]         = p_in.precision_max_step;
    v_p["smooth"]           = p_in.precision_smooth;
    v_p["entry_ms"]         = p_in.prec_entry_ms;
    v_p["exit_ms"]          = p_in.prec_exit_ms;
    v_p["entry_still_deg"]  = p_in.prec_entry_still_deg;
    v_p["exit_move_deg"]    = p_in.prec_exit_move_deg;
    v_p["profile"]          = p_in.prec_profile;

    p_parent["led_brightness"]      = p_in.led_brightness;
    p_parent["battery_adc_enabled"] = p_in.battery_adc_enabled;

    JsonObject v_gb = p_parent["gyro_bias"].to<JsonObject>();
    v_gb["still_th"]     = p_in.gyro_bias.still_th;
    v_gb["still_win_ms"] = p_in.gyro_bias.still_win_ms;
    v_gb["alpha"]        = p_in.gyro_bias.alpha;

    JsonObject v_ln = p_parent["linear"].to<JsonObject>();
    v_ln["th"]         = p_in.linear.th;
    v_ln["impulse_th"] = p_in.linear.impulse_th;
    v_ln["window_ms"]  = p_in.linear.window_ms;

    JsonObject v_fl = p_parent["flick"].to<JsonObject>();
    v_fl["p2p_th"]      = p_in.flick.p2p_th;
    v_fl["window_ms"]   = p_in.flick.window_ms;
    v_fl["cooldown_ms"] = p_in.flick.cooldown_ms;

    JsonObject v_th = p_parent["tilt_hold"].to<JsonObject>();
    v_th["angle_deg"] = p_in.tilt_hold.angle_deg;
    v_th["hold_ms"]   = p_in.tilt_hold.hold_ms;
    v_th["repeat_hz"] = p_in.tilt_hold.repeat_hz;

    p_parent["sleep_idle_timeout_ms"] = p_in.sleep_idle_timeout_ms;
    p_parent["active_mode"]           = p_in.active_mode;
    p_parent["active_peer_index"]     = p_in.active_peer_index;

    // ---- Motion Advanced ----
    JsonObject v_ma = p_parent["motion_adv"].to<JsonObject>();

    JsonObject v_cf = v_ma["click_freeze"].to<JsonObject>();
    v_cf["enable"]         = p_in.motion_adv.click_freeze.enable;
    v_cf["gyro_th"]        = p_in.motion_adv.click_freeze.gyro_th;
    v_cf["max_ms"]         = p_in.motion_adv.click_freeze.max_ms;
    v_cf["hold_ms"]        = p_in.motion_adv.click_freeze.hold_ms;
    v_cf["fadeout_ms"]     = p_in.motion_adv.click_freeze.fadeout_ms;
    v_cf["move_th"]        = p_in.motion_adv.click_freeze.move_th;
    v_cf["freeze_move_th"] = p_in.motion_adv.click_freeze.freeze_move_th;

    JsonObject v_ema = v_ma["ema"].to<JsonObject>();
    v_ema["alpha_min"]      = p_in.motion_adv.ema.alpha_min;
    v_ema["alpha_max"]      = p_in.motion_adv.ema.alpha_max;
    v_ema["deadzone_th"]    = p_in.motion_adv.ema.deadzone_th;
    v_ema["fast_th"]        = p_in.motion_adv.ema.fast_th;
    v_ema["reversal_th"]    = p_in.motion_adv.ema.reversal_th;
    v_ema["reversal_reset"] = p_in.motion_adv.ema.reversal_reset;

    JsonObject v_snap = v_ma["snap"].to<JsonObject>();
    v_snap["enable"]         = p_in.motion_adv.snap.enable;
    v_snap["mode_mask"]      = p_in.motion_adv.snap.mode_mask;
    v_snap["axis_mode"]      = p_in.motion_adv.snap.axis_mode;
    v_snap["confirm_frames"] = p_in.motion_adv.snap.confirm_frames;
    v_snap["ratio_enter"]    = p_in.motion_adv.snap.ratio_enter;
    v_snap["strength"]       = p_in.motion_adv.snap.strength;

    // ---- Power ----
    JsonObject v_pw = p_parent["power"].to<JsonObject>();
    JsonArray v_ito = v_pw["idle_timeout_ms"].to<JsonArray>();
    v_ito.add(p_in.power.idle_timeout_ms[0]);
    v_ito.add(p_in.power.idle_timeout_ms[1]);
    v_ito.add(p_in.power.idle_timeout_ms[2]);
    v_pw["idle_timeout_ble_ms"]     = p_in.power.idle_timeout_ble_ms;
    v_pw["pairing_idle_timeout_ms"] = p_in.power.pairing_idle_timeout_ms;
    v_pw["deep_idle_timeout_ms"]    = p_in.power.deep_idle_timeout_ms;
    v_pw["wake_min_active_ms"]      = p_in.power.wake_min_active_ms;
    v_pw["wom_threshold"]           = p_in.power.wom_threshold;
    v_pw["wom_duration"]            = p_in.power.wom_duration;
    v_pw["fast_recalib_ms"]         = p_in.power.fast_recalib_ms;
    v_pw["led_fadeout_ms"]          = p_in.power.led_fadeout_ms;
    v_pw["led_fadein_ms"]           = p_in.power.led_fadein_ms;

    // ---- Button ----
    JsonObject v_btn = p_parent["button"].to<JsonObject>();
    v_btn["debounce_press_ms"]   = p_in.button.debounce_press_ms;
    v_btn["debounce_release_ms"] = p_in.button.debounce_release_ms;
    v_btn["long_delay_ms"]       = p_in.button.long_delay_ms;
    v_btn["double_delay_ms"]     = p_in.button.double_delay_ms;
    v_btn["hold_2s_ms"]          = p_in.button.hold_2s_ms;
    v_btn["hold_3s_ms"]          = p_in.button.hold_3s_ms;
    v_btn["min_click_ms"]        = p_in.button.min_click_ms;
    v_btn["debounce_min_ticks"]  = p_in.button.debounce_min_ticks;
}

void CL_C10_Config::_buildSlotsJson(JsonObject p_parent, const ST_C10_ProfileSlots_t& p_in) {
    JsonArray v_glob = p_parent["global"].to<JsonArray>();
    _buildSlotArrayJson(v_glob, p_in.global, EN_C10_TRIG_MAX);

    JsonArray v_modes = p_parent["modes"].to<JsonArray>();
    for (uint8_t m = 0; m < C10_DEF::MODE_COUNT; m++) {
        JsonObject v_m = v_modes.add<JsonObject>();
        v_m["mask"] = (uint32_t)p_in.overrideMask[m];
        JsonArray v_arr = v_m["slots"].to<JsonArray>();
        _buildSlotArrayJson(v_arr, p_in.modes[m], EN_C10_TRIG_MAX);
    }
}

void CL_C10_Config::_buildMacroStepJson(JsonObject p_obj, const ST_C10_MacroStep_t& p_in) {
    p_obj["k"]   = p_in.kind;
    p_obj["h"]   = p_in.holdMode;
    p_obj["d"]   = p_in.delayMs;
    p_obj["p16"] = p_in.param16;
    p_obj["p32"] = (uint32_t)p_in.param32;
}

void CL_C10_Config::_buildMacroJson(JsonObject p_obj, const ST_C10_Macro_t& p_in) {
    p_obj["name"] = p_in.name;

    JsonArray v_steps = p_obj["steps"].to<JsonArray>();
    for (uint8_t i = 0; i < p_in.stepCount; i++) {
        JsonObject o = v_steps.add<JsonObject>();
        _buildMacroStepJson(o, p_in.steps[i]);
    }
}

void CL_C10_Config::_buildMacrosJson(JsonArray p_arr, const ST_C10_MacroLib_t& p_in) {
    for (uint8_t i = 0; i < p_in.count; i++) {
        JsonObject o = p_arr.add<JsonObject>();
        _buildMacroJson(o, p_in.macros[i]);
    }
}

bool CL_C10_Config::buildProfileJson(const ST_C10_ProfileConfig_t& p_in, JsonDocument& p_out) {
    p_out.clear();
    // [v0415] 저장 시 항상 현재 스키마 버전 기록
    p_out["ver"]  = (uint16_t)G_C10_CFG_VER;
    p_out["name"] = p_in.name;

    JsonObject v_w = p_out["wifi"].to<JsonObject>();
    _buildWiFiJson(v_w, p_in.wifi);

    JsonObject v_e = p_out["e10"].to<JsonObject>();
    _buildE10Json(v_e, p_in.e10);

    JsonObject v_s = p_out["slots"].to<JsonObject>();
    _buildSlotsJson(v_s, p_in.slots);

    JsonArray v_m = p_out["macros"].to<JsonArray>();
    _buildMacrosJson(v_m, p_in.macros);

    return true;
}


// =======================================================
// JSON Patch (Json → struct)
// =======================================================
bool CL_C10_Config::_patchSlotJson(JsonVariantConst p_v, ST_C20_ActionSlot_t& p_io) {
    if (p_v.isNull()) return false;
    if (!p_v["k"].isNull())   p_io.kind     = (uint8_t)p_v["k"];
    if (!p_v["h"].isNull())   p_io.holdMode = (uint8_t)p_v["h"];
    if (!p_v["p16"].isNull()) p_io.param16  = (uint16_t)p_v["p16"];
    if (!p_v["p32"].isNull()) p_io.param32  = (uint32_t)p_v["p32"];

    // [v0415] Consumer kind의 경우 24-bit 상한 클램프
    if (p_io.kind == (uint8_t)EN_C20_ACT_CONSUMER_TAP ||
        p_io.kind == (uint8_t)EN_C20_ACT_CONSUMER_REPEAT) {
        p_io.param32 &= G_C20_CONSUMER_MASK_MAX;
    }
    return true;
}

bool CL_C10_Config::_patchSlotArrayJson(JsonVariantConst p_v,
                                        ST_C20_ActionSlot_t* p_arr,
                                        uint8_t p_count) {
    if (p_v.isNull()) return false;

    JsonArrayConst arr = p_v.as<JsonArrayConst>();
    if (arr.isNull()) return false;

    const uint8_t n = (arr.size() < p_count) ? (uint8_t)arr.size() : p_count;
    for (uint8_t i = 0; i < n; i++) {
        (void)_patchSlotJson(arr[i], p_arr[i]);
    }
    return true;
}

bool CL_C10_Config::_patchWiFiJson(JsonVariantConst p_v, ST_C10_WiFiConfig_t& p_io) {
    if (p_v.isNull()) return false;

    if (!p_v["mode"].isNull()) p_io.mode = (uint8_t)p_v["mode"];

    JsonVariantConst v_sta = p_v["sta"];
    if (!v_sta.isNull()) {
        if (!v_sta["ssid"].isNull()) strlcpy(p_io.sta_ssid, (const char*)v_sta["ssid"], sizeof(p_io.sta_ssid));
        if (!v_sta["pass"].isNull()) strlcpy(p_io.sta_pass, (const char*)v_sta["pass"], sizeof(p_io.sta_pass));
    }

    JsonVariantConst v_ap = p_v["ap"];
    if (!v_ap.isNull()) {
        if (!v_ap["ssid"].isNull()) strlcpy(p_io.ap_ssid, (const char*)v_ap["ssid"], sizeof(p_io.ap_ssid));
        if (!v_ap["pass"].isNull()) strlcpy(p_io.ap_pass, (const char*)v_ap["pass"], sizeof(p_io.ap_pass));
    }

    JsonVariantConst v_md = p_v["mdns"];
    if (!v_md.isNull()) {
        if (!v_md["host"].isNull()) strlcpy(p_io.mdns_host, (const char*)v_md["host"], sizeof(p_io.mdns_host));
    }
    return true;
}

bool CL_C10_Config::_patchE10Json(JsonVariantConst p_v, ST_C10_E10Config_t& p_io) {
    if (p_v.isNull()) return false;

    if (!p_v["dpi_level"].isNull())       p_io.dpi_level = (uint8_t)p_v["dpi_level"];
    if (!p_v["hard_click_lock"].isNull()) p_io.hard_click_lock = (bool)p_v["hard_click_lock"];

    JsonVariantConst v_sb = p_v["scale_base"];
    if (v_sb.is<JsonArrayConst>()) {
        JsonArrayConst a = v_sb.as<JsonArrayConst>();
        if (a.size() >= 3) { p_io.scale_base[0]=(float)a[0]; p_io.scale_base[1]=(float)a[1]; p_io.scale_base[2]=(float)a[2]; }
    }
    JsonVariantConst v_ag = p_v["accel_gain"];
    if (v_ag.is<JsonArrayConst>()) {
        JsonArrayConst a = v_ag.as<JsonArrayConst>();
        if (a.size() >= 3) { p_io.accel_gain[0]=(float)a[0]; p_io.accel_gain[1]=(float)a[1]; p_io.accel_gain[2]=(float)a[2]; }
    }

    if (!p_v["accel_threshold"].isNull())    p_io.accel_threshold = (float)p_v["accel_threshold"];
    if (!p_v["scroll_cursor_damp"].isNull()) p_io.scroll_cursor_damp = (float)p_v["scroll_cursor_damp"];

    JsonVariantConst v_w = p_v["wheel"];
    if (!v_w.isNull()) {
        if (!v_w["threshold_deg"].isNull()) p_io.wheel_threshold_deg = (float)v_w["threshold_deg"];
        if (!v_w["step_max"].isNull())      p_io.wheel_step_max = (uint8_t)v_w["step_max"];
    }

    // [v0415 rev15 / 스키마 412] gesture.* 필드 삭제
    //   - legacy 411 파일의 gesture 블록은 무시됨 (makeDefaults의 flick 기본값 사용)
    //   - flick.* 는 하단에서 별도 파싱 (SSOT)

    JsonVariantConst v_p = p_v["precision"];
    if (!v_p.isNull()) {
        if (!v_p["mode"].isNull())            p_io.precision_mode = (uint8_t)v_p["mode"];
        if (!v_p["deadzone"].isNull())        p_io.precision_deadzone = (float)v_p["deadzone"];
        if (!v_p["gain"].isNull())            p_io.precision_gain = (float)v_p["gain"];
        if (!v_p["accel"].isNull())           p_io.precision_accel = (float)v_p["accel"];
        if (!v_p["max_step"].isNull())        p_io.precision_max_step = (uint8_t)v_p["max_step"];
        if (!v_p["smooth"].isNull())          p_io.precision_smooth = (float)v_p["smooth"];
        if (!v_p["entry_ms"].isNull())        p_io.prec_entry_ms = (uint16_t)v_p["entry_ms"];
        if (!v_p["exit_ms"].isNull())         p_io.prec_exit_ms = (uint16_t)v_p["exit_ms"];
        if (!v_p["entry_still_deg"].isNull()) p_io.prec_entry_still_deg = (float)v_p["entry_still_deg"];
        if (!v_p["exit_move_deg"].isNull())   p_io.prec_exit_move_deg = (float)v_p["exit_move_deg"];
        if (!v_p["profile"].isNull())         p_io.prec_profile = (uint8_t)v_p["profile"];
    }

    if (!p_v["led_brightness"].isNull())      p_io.led_brightness = (uint8_t)p_v["led_brightness"];
    if (!p_v["battery_adc_enabled"].isNull()) p_io.battery_adc_enabled = (bool)p_v["battery_adc_enabled"];

    JsonVariantConst v_gb = p_v["gyro_bias"];
    if (!v_gb.isNull()) {
        if (!v_gb["still_th"].isNull())     p_io.gyro_bias.still_th = (float)v_gb["still_th"];
        if (!v_gb["still_win_ms"].isNull()) p_io.gyro_bias.still_win_ms = (uint16_t)v_gb["still_win_ms"];
        if (!v_gb["alpha"].isNull())        p_io.gyro_bias.alpha = (float)v_gb["alpha"];
    }

    JsonVariantConst v_ln = p_v["linear"];
    if (!v_ln.isNull()) {
        if (!v_ln["th"].isNull())         p_io.linear.th = (float)v_ln["th"];
        if (!v_ln["impulse_th"].isNull()) p_io.linear.impulse_th = (float)v_ln["impulse_th"];
        if (!v_ln["window_ms"].isNull())  p_io.linear.window_ms = (uint16_t)v_ln["window_ms"];
    }

    JsonVariantConst v_fl = p_v["flick"];
    if (!v_fl.isNull()) {
        if (!v_fl["p2p_th"].isNull())      p_io.flick.p2p_th = (float)v_fl["p2p_th"];
        if (!v_fl["window_ms"].isNull())   p_io.flick.window_ms = (uint16_t)v_fl["window_ms"];
        if (!v_fl["cooldown_ms"].isNull()) p_io.flick.cooldown_ms = (uint16_t)v_fl["cooldown_ms"];
    }

    JsonVariantConst v_th = p_v["tilt_hold"];
    if (!v_th.isNull()) {
        if (!v_th["angle_deg"].isNull()) p_io.tilt_hold.angle_deg = (float)v_th["angle_deg"];
        if (!v_th["hold_ms"].isNull())   p_io.tilt_hold.hold_ms = (uint16_t)v_th["hold_ms"];
        if (!v_th["repeat_hz"].isNull()) p_io.tilt_hold.repeat_hz = (uint8_t)v_th["repeat_hz"];
    }

    if (!p_v["sleep_idle_timeout_ms"].isNull()) p_io.sleep_idle_timeout_ms = (uint32_t)p_v["sleep_idle_timeout_ms"];
    if (!p_v["active_mode"].isNull())           p_io.active_mode = (uint8_t)p_v["active_mode"];
    if (!p_v["active_peer_index"].isNull())     p_io.active_peer_index = (uint8_t)p_v["active_peer_index"];

    // ---- Motion Advanced (부분 패치) ----
    JsonVariantConst v_ma = p_v["motion_adv"];
    if (!v_ma.isNull()) {
        JsonVariantConst v_cf = v_ma["click_freeze"];
        if (!v_cf.isNull()) {
            auto& d = p_io.motion_adv.click_freeze;
            if (!v_cf["enable"].isNull())         d.enable         = (bool)v_cf["enable"];
            if (!v_cf["gyro_th"].isNull())        d.gyro_th        = (float)v_cf["gyro_th"];
            if (!v_cf["max_ms"].isNull())         d.max_ms         = (uint16_t)v_cf["max_ms"];
            if (!v_cf["hold_ms"].isNull())        d.hold_ms        = (uint16_t)v_cf["hold_ms"];
            if (!v_cf["fadeout_ms"].isNull())     d.fadeout_ms     = (uint16_t)v_cf["fadeout_ms"];
            if (!v_cf["move_th"].isNull())        d.move_th        = (float)v_cf["move_th"];
            if (!v_cf["freeze_move_th"].isNull()) d.freeze_move_th = (float)v_cf["freeze_move_th"];
        }

        JsonVariantConst v_ema = v_ma["ema"];
        if (!v_ema.isNull()) {
            auto& e = p_io.motion_adv.ema;
            if (!v_ema["alpha_min"].isNull())      e.alpha_min      = (float)v_ema["alpha_min"];
            if (!v_ema["alpha_max"].isNull())      e.alpha_max      = (float)v_ema["alpha_max"];
            if (!v_ema["deadzone_th"].isNull())    e.deadzone_th    = (float)v_ema["deadzone_th"];
            if (!v_ema["fast_th"].isNull())        e.fast_th        = (float)v_ema["fast_th"];
            if (!v_ema["reversal_th"].isNull())    e.reversal_th    = (float)v_ema["reversal_th"];
            if (!v_ema["reversal_reset"].isNull()) e.reversal_reset = (bool)v_ema["reversal_reset"];
        }

        JsonVariantConst v_snap = v_ma["snap"];
        if (!v_snap.isNull()) {
            auto& s = p_io.motion_adv.snap;
            if (!v_snap["enable"].isNull())         s.enable         = (bool)v_snap["enable"];
            if (!v_snap["mode_mask"].isNull())      s.mode_mask      = (uint8_t)v_snap["mode_mask"];
            if (!v_snap["axis_mode"].isNull())      s.axis_mode      = (uint8_t)v_snap["axis_mode"];
            if (!v_snap["confirm_frames"].isNull()) s.confirm_frames = (uint8_t)v_snap["confirm_frames"];
            if (!v_snap["ratio_enter"].isNull())    s.ratio_enter    = (float)v_snap["ratio_enter"];
            if (!v_snap["strength"].isNull())       s.strength       = (float)v_snap["strength"];
        }
    }

    // ---- Power ----
    JsonVariantConst v_pw = p_v["power"];
    if (!v_pw.isNull()) {
        auto& d = p_io.power;
        JsonVariantConst v_ito = v_pw["idle_timeout_ms"];
        if (v_ito.is<JsonArrayConst>()) {
            JsonArrayConst a = v_ito.as<JsonArrayConst>();
            if (a.size() >= 3) {
                d.idle_timeout_ms[0] = (uint32_t)a[0];
                d.idle_timeout_ms[1] = (uint32_t)a[1];
                d.idle_timeout_ms[2] = (uint32_t)a[2];
            }
        }
        if (!v_pw["idle_timeout_ble_ms"].isNull())     d.idle_timeout_ble_ms     = (uint32_t)v_pw["idle_timeout_ble_ms"];
        if (!v_pw["pairing_idle_timeout_ms"].isNull()) d.pairing_idle_timeout_ms = (uint32_t)v_pw["pairing_idle_timeout_ms"];
        if (!v_pw["deep_idle_timeout_ms"].isNull())    d.deep_idle_timeout_ms    = (uint32_t)v_pw["deep_idle_timeout_ms"];
        if (!v_pw["wake_min_active_ms"].isNull())      d.wake_min_active_ms      = (uint16_t)v_pw["wake_min_active_ms"];
        if (!v_pw["wom_threshold"].isNull())           d.wom_threshold           = (uint8_t)v_pw["wom_threshold"];
        if (!v_pw["wom_duration"].isNull())            d.wom_duration            = (uint8_t)v_pw["wom_duration"];
        if (!v_pw["fast_recalib_ms"].isNull())         d.fast_recalib_ms         = (uint16_t)v_pw["fast_recalib_ms"];
        if (!v_pw["led_fadeout_ms"].isNull())          d.led_fadeout_ms          = (uint16_t)v_pw["led_fadeout_ms"];
        if (!v_pw["led_fadein_ms"].isNull())           d.led_fadein_ms           = (uint16_t)v_pw["led_fadein_ms"];
    }

    // ---- Button ----
    JsonVariantConst v_btn = p_v["button"];
    if (!v_btn.isNull()) {
        auto& b = p_io.button;
        if (!v_btn["debounce_press_ms"].isNull())   b.debounce_press_ms   = (uint16_t)v_btn["debounce_press_ms"];
        if (!v_btn["debounce_release_ms"].isNull()) b.debounce_release_ms = (uint16_t)v_btn["debounce_release_ms"];
        if (!v_btn["long_delay_ms"].isNull())       b.long_delay_ms       = (uint16_t)v_btn["long_delay_ms"];
        if (!v_btn["double_delay_ms"].isNull())     b.double_delay_ms     = (uint16_t)v_btn["double_delay_ms"];
        if (!v_btn["hold_2s_ms"].isNull())          b.hold_2s_ms          = (uint16_t)v_btn["hold_2s_ms"];
        if (!v_btn["hold_3s_ms"].isNull())          b.hold_3s_ms          = (uint16_t)v_btn["hold_3s_ms"];
        if (!v_btn["min_click_ms"].isNull())        b.min_click_ms        = (uint16_t)v_btn["min_click_ms"];
        if (!v_btn["debounce_min_ticks"].isNull())  b.debounce_min_ticks  = (uint8_t)v_btn["debounce_min_ticks"];
    }

    return true;
}

bool CL_C10_Config::_patchSlotsJson(JsonVariantConst p_v, ST_C10_ProfileSlots_t& p_io) {
    if (p_v.isNull()) return false;

    (void)_patchSlotArrayJson(p_v["global"], p_io.global, EN_C10_TRIG_MAX);

    JsonVariantConst v_modes = p_v["modes"];
    if (!v_modes.is<JsonArrayConst>()) return true;

    JsonArrayConst arr = v_modes.as<JsonArrayConst>();
    const uint8_t n = (arr.size() < C10_DEF::MODE_COUNT)
                    ? (uint8_t)arr.size() : C10_DEF::MODE_COUNT;

    for (uint8_t m = 0; m < n; m++) {
        JsonVariantConst v_m = arr[m];
        if (v_m.isNull()) continue;
        if (!v_m["mask"].isNull()) p_io.overrideMask[m] = (uint32_t)v_m["mask"];
        (void)_patchSlotArrayJson(v_m["slots"], p_io.modes[m], EN_C10_TRIG_MAX);
    }
    return true;
}

bool CL_C10_Config::_patchMacroStepJson(JsonVariantConst p_v, ST_C10_MacroStep_t& p_io) {
    if (p_v.isNull()) return false;
    if (!p_v["k"].isNull()) {
        const uint8_t v_k = (uint8_t)p_v["k"];
        if (v_k != (uint8_t)EN_C20_ACT_MACRO &&
            v_k != (uint8_t)EN_C20_ACT_SPECIAL &&
            v_k < (uint8_t)EN_C20_ACT_MAX) {
            p_io.kind = v_k;
        }
    }
    // [v0412] 매크로 step은 holdMode == NONE(0)만 허용
    p_io.holdMode = (uint8_t)EN_C20_HOLD_NONE;
    if (!p_v["d"].isNull()) {
        uint16_t v_d = (uint16_t)p_v["d"];
        if (v_d > C10_DEF::MACRO_DELAY_MAX_MS) v_d = C10_DEF::MACRO_DELAY_MAX_MS;
        p_io.delayMs = v_d;
    }
    if (!p_v["p16"].isNull()) p_io.param16 = (uint16_t)p_v["p16"];
    if (!p_v["p32"].isNull()) p_io.param32 = (uint32_t)p_v["p32"];

    // [v0415] Consumer kind의 경우 24-bit 상한 클램프
    if (p_io.kind == (uint8_t)EN_C20_ACT_CONSUMER_TAP ||
        p_io.kind == (uint8_t)EN_C20_ACT_CONSUMER_REPEAT) {
        p_io.param32 &= G_C20_CONSUMER_MASK_MAX;
    }
    return true;
}

bool CL_C10_Config::_patchMacroJson(JsonVariantConst p_v, ST_C10_Macro_t& p_io) {
    if (p_v.isNull()) return false;

    if (!p_v["name"].isNull()) strlcpy(p_io.name, (const char*)p_v["name"], sizeof(p_io.name));

    JsonVariantConst v_steps = p_v["steps"];
    if (v_steps.is<JsonArrayConst>()) {
        JsonArrayConst arr = v_steps.as<JsonArrayConst>();
        const uint8_t n = (arr.size() < C10_DEF::MACRO_STEP_MAX)
                        ? (uint8_t)arr.size() : C10_DEF::MACRO_STEP_MAX;
        for (uint8_t i = 0; i < n; i++) {
            (void)_patchMacroStepJson(arr[i], p_io.steps[i]);
        }
        p_io.stepCount = n;
    }
    return true;
}

bool CL_C10_Config::_patchMacrosJson(JsonVariantConst p_v, ST_C10_MacroLib_t& p_io) {
    if (p_v.isNull()) return false;

    JsonArrayConst arr = p_v.as<JsonArrayConst>();
    if (arr.isNull()) return false;

    const uint8_t n = (arr.size() < C10_DEF::MACRO_MAX)
                    ? (uint8_t)arr.size() : C10_DEF::MACRO_MAX;
    for (uint8_t i = 0; i < n; i++) {
        (void)_patchMacroJson(arr[i], p_io.macros[i]);
    }
    p_io.count = n;
    return true;
}

bool CL_C10_Config::patchProfileFromJson(const String& p_json, ST_C10_ProfileConfig_t& p_io) {
    JsonDocument v_doc;
    if (deserializeJson(v_doc, p_json)) return false;

    // [v0415] ver 파싱 (마이그레이션 감지용)
    if (!v_doc["ver"].isNull()) p_io.ver = (uint16_t)v_doc["ver"];

    if (!v_doc["name"].isNull()) strlcpy(p_io.name, (const char*)v_doc["name"], sizeof(p_io.name));

    (void)_patchWiFiJson  (v_doc["wifi"],   p_io.wifi);
    (void)_patchE10Json   (v_doc["e10"],    p_io.e10);
    (void)_patchSlotsJson (v_doc["slots"],  p_io.slots);
    (void)_patchMacrosJson(v_doc["macros"], p_io.macros);

    return true;
}
