// =======================================================
// File: src/v0410/C10_Config_0410.cpp
// =======================================================
#include "C10_Config_0410.h"

// =======================================================
// ctor / begin
// =======================================================
CL_C10_Config::CL_C10_Config() {
    memset(&_boot, 0, sizeof(_boot));
    _bootOkRetryAtMs = 0;
}

void CL_C10_Config::begin(bool p_formatOnFail) {
    (void)LittleFS.begin(p_formatOnFail);
    if (!LittleFS.exists("/json")) (void)LittleFS.mkdir("/json");

    // 1) boot state load + mark start (pending)
    _loadBootState(_boot);

    // reset reason 기록
    _boot.last_reset_reason = _getResetReasonU8();

    // 이전 부팅이 pending이었다면 비정상 리셋일 때만 fail_count++
    if (_boot.pending) {
        esp_reset_reason_t v_r = (esp_reset_reason_t)_boot.last_reset_reason;
        if (_isBadResetReason(v_r)) {
            if (_boot.fail_count < 250) _boot.fail_count++;
        }
    }

    if (_boot.fail_count >= C10_DEF::SAFE_FAIL_THRESHOLD) {
        _boot.safe_mode = true;
    }

    _boot.pending = true;
    _boot.boot_ms = (uint32_t)millis();
    (void)_saveBootState(_boot);

    // 2) config 존재 보장
    if (!LittleFS.exists(C10_DEF::CFG_PATH)) {
        ST_C10_WiFiConfig_t v_w;
        ST_C10_E10Config_t  v_e;
        makeDefaultsWiFi(v_w);
        makeDefaultsE10(v_e);
        (void)saveAll(v_w, v_e);
    }
}

// =======================================================
// Defaults
// =======================================================
void CL_C10_Config::makeDefaultsWiFi(ST_C10_WiFiConfig_t& p_out) {
    memset(&p_out, 0, sizeof(p_out));
    p_out.mode = (uint8_t)EN_C10_WIFI_AUTO;
    strlcpy(p_out.ap_ssid, "EliteAirMouse", sizeof(p_out.ap_ssid));
    strlcpy(p_out.ap_pass, "12345678", sizeof(p_out.ap_pass));
    strlcpy(p_out.mdns_host, "elite-airmouse", sizeof(p_out.mdns_host));
}

void CL_C10_Config::makeDefaultsMode(uint8_t p_modeIdx, ST_C10_ModeConfig_t& p_out) {
    memset(&p_out, 0, sizeof(p_out));

    for (uint8_t i = 0; i < C10_DEF::SLOT_BTN_COUNT; i++) p_out.slots[i] = C20_MakeNone();
    for (uint8_t i = 0; i < C10_DEF::SLOT_FLICK_COUNT; i++) p_out.flick[i] = C20_MakeNone();
    for (uint8_t i = 0; i < C10_DEF::SLOT_LINEAR_COUNT; i++) p_out.linear[i] = C20_MakeNone();
    for (uint8_t i = 0; i < C10_DEF::SLOT_TILT_COUNT; i++) p_out.tilt[i] = C20_MakeNone();

    if (p_modeIdx == 0) {
        // ---------------- Mode 1: PC Air Mouse ----------------
        p_out.slots[0]  = C20_MakeMouseClick(EN_C20_M_L);                // S1 🔒
        p_out.slots[1]  = C20_MakeMouseClick(EN_C20_M_L);                // S2 L Double
        p_out.slots[2]  = C20_MakeKbTap(EN_C20_MOD_LALT, EN_C20_KB_TAB); // S3 Alt+Tab
        p_out.slots[3]  = C20_MakeMouseClick(EN_C20_M_M);                // S4
        // S5 = None (Move Gate, 코드가 강제)
        p_out.slots[5]  = C20_MakeMouseClick(EN_C20_M_R);                                  // S6 🔒
        p_out.slots[6]  = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_ESC);                   // S7 Esc
        p_out.slots[7]  = C20_MakeKbTap(EN_C20_MOD_LGUI | EN_C20_MOD_LSHIFT, EN_C20_KB_S); // S8 Win+Shift+S
        p_out.slots[8]  = C20_MakeConsumer(EN_C20_CON_VOL_UP);                             // S9
        p_out.slots[9]  = C20_MakeConsumer(EN_C20_CON_NEXT_TRACK);                         // S10
        p_out.slots[10] = C20_MakeConsumer(EN_C20_CON_PLAY_PAUSE);                         // S11
        p_out.slots[11] = C20_MakeSpecial(EN_C20_SP_MODE_CYCLE);                           // S12 🔒
        p_out.slots[12] = C20_MakeSpecial(EN_C20_SP_PAIRING);                              // S13 🔒
        p_out.slots[13] = C20_MakeConsumer(EN_C20_CON_VOL_DOWN);                           // S14
        p_out.slots[14] = C20_MakeConsumer(EN_C20_CON_PREV_TRACK);                         // S15

        {
            ST_C20_ActionSlot_t v = {EN_C20_ACT_KB_COMBO,
                                     EN_C20_HOLD_NONE,
                                     0,
                                     C20_EncodeCombo(EN_C20_MOD_LCTRL | EN_C20_MOD_LGUI, EN_C20_KB_LEFT)};
            p_out.flick[0]        = v; // G1 L: Ctrl+Win+Left
        }
        {
            ST_C20_ActionSlot_t v = {EN_C20_ACT_KB_COMBO,
                                     EN_C20_HOLD_NONE,
                                     0,
                                     C20_EncodeCombo(EN_C20_MOD_LCTRL | EN_C20_MOD_LGUI, EN_C20_KB_RIGHT)};
            p_out.flick[1]        = v; // G2 R: Ctrl+Win+Right
        }
        p_out.flick[2] = C20_MakeKbTap(EN_C20_MOD_LGUI, EN_C20_KB_D);   // G3 U: Win+D
        p_out.flick[3] = C20_MakeKbTap(EN_C20_MOD_LALT, EN_C20_KB_TAB); // G4 D: Alt+Tab
        // Linear/Tilt = None

    } else if (p_modeIdx == 1) {
        // ---------------- Mode 2: Presentation ----------------
        p_out.slots[0]  = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_PAGEDOWN); // S1 🔒
        p_out.slots[1]  = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_PAGEUP);   // S2
        p_out.slots[2]  = C20_MakeKbTap(EN_C20_MOD_LSHIFT, EN_C20_KB_F5);     // S3
        p_out.slots[3]  = C20_MakeKbTap(EN_C20_MOD_LCTRL, EN_C20_KB_L);       // S4 Ctrl+L
        // S5 = None (Move Gate)
        p_out.slots[5]  = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_ESC);      // S6
        p_out.slots[6]  = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_B);        // S7 B
        p_out.slots[7]  = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_W);        // S8 W
        p_out.slots[8]  = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_PAGEDOWN); // S9
        p_out.slots[9]  = C20_MakeKbTap(EN_C20_MOD_LSHIFT, EN_C20_KB_F5);     // S10
        p_out.slots[10] = C20_MakeKbTap(EN_C20_MOD_LCTRL, EN_C20_KB_P);       // S11 Ctrl+P
        p_out.slots[11] = C20_MakeSpecial(EN_C20_SP_MODE_CYCLE);              // S12 🔒
        p_out.slots[12] = C20_MakeSpecial(EN_C20_SP_PAIRING);                 // S13 🔒
        p_out.slots[13] = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_PAGEUP);   // S14
        p_out.slots[14] = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_ESC);      // S15

        p_out.flick[0] = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_PAGEDOWN); // G1 L
        p_out.flick[1] = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_PAGEUP);   // G2 R
        p_out.flick[2] = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_B);        // G3 U
        // G4 D + Linear/Tilt = None

    } else {
        // ---------------- Mode 3: Smart TV ----------------
        p_out.slots[0]  = C20_MakeConsumer(EN_C20_CON_AC_BACK);            // S1 🔒
        p_out.slots[1]  = C20_MakeConsumer(EN_C20_CON_AC_HOME);            // S2
        p_out.slots[2]  = C20_MakeConsumer(EN_C20_CON_POWER);              // S3
        p_out.slots[3]  = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_ENTER); // S4 🔒
        // S5 = None (커서 활성)
        p_out.slots[5]  = C20_MakeConsumer(EN_C20_CON_AC_HOME);    // S6
        p_out.slots[6]  = C20_MakeConsumer(EN_C20_CON_TV_INPUT);   // S7
        p_out.slots[7]  = C20_MakeConsumer(EN_C20_CON_POWER);      // S8
        p_out.slots[8]  = C20_MakeConsumer(EN_C20_CON_VOL_UP);     // S9
        p_out.slots[9]  = C20_MakeConsumer(EN_C20_CON_CH_UP);      // S10
        p_out.slots[10] = C20_MakeConsumer(EN_C20_CON_PLAY_PAUSE); // S11
        p_out.slots[11] = C20_MakeSpecial(EN_C20_SP_MODE_CYCLE);   // S12 🔒
        p_out.slots[12] = C20_MakeSpecial(EN_C20_SP_PAIRING);      // S13 🔒
        p_out.slots[13] = C20_MakeConsumer(EN_C20_CON_VOL_DOWN);   // S14
        p_out.slots[14] = C20_MakeConsumer(EN_C20_CON_CH_DOWN);    // S15

        p_out.flick[0] = C20_MakeConsumer(EN_C20_CON_REWIND); // G1 L
        p_out.flick[1] = C20_MakeConsumer(EN_C20_CON_FF);     // G2 R
        p_out.flick[2] = C20_MakeConsumer(EN_C20_CON_MUTE);   // G3 U
        // G4 D = None

        p_out.tilt[0] = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_UP);    // T1 Up
        p_out.tilt[1] = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_DOWN);  // T2 Down
        p_out.tilt[2] = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_LEFT);  // T3 Left
        p_out.tilt[3] = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_RIGHT); // T4 Right
    }
}

void CL_C10_Config::makeDefaultsE10(ST_C10_E10Config_t& p_out) {
    memset(&p_out, 0, sizeof(p_out));

    // ---- 기존 물리 엔진 파라미터 ----
    p_out.dpi_level       = 2;
    p_out.hard_click_lock = true;

    p_out.scale_base[0]   = 0.55f;
    p_out.scale_base[1]   = 0.75f;
    p_out.scale_base[2]   = 1.00f;
    p_out.accel_gain[0]   = 0.35f;
    p_out.accel_gain[1]   = 0.55f;
    p_out.accel_gain[2]   = 0.85f;
    p_out.accel_threshold = 8.0f;

    p_out.wheel_threshold_deg = 90.0f;
    p_out.wheel_step_max      = 6;

    p_out.gesture_flick_deg   = 200.0f;
    p_out.gesture_cooldown_ms = 600;

    p_out.scroll_cursor_damp = 0.25f;

    // ---- Precision ----
    p_out.precision_mode       = (uint8_t)EN_C10_E10_PREC_OFF;
    p_out.precision_deadzone   = 1.2f;
    p_out.precision_gain       = 0.65f;
    p_out.precision_accel      = 0.25f;
    p_out.precision_max_step   = 18;
    p_out.precision_smooth     = 0.85f;
    p_out.prec_entry_ms        = 450;
    p_out.prec_exit_ms         = 300;
    p_out.prec_entry_still_deg = 1.2f;
    p_out.prec_exit_move_deg   = 3.5f;
    p_out.prec_profile         = 0;

    // ---- v0410 신규 ----
    p_out.led_brightness      = 128;
    p_out.battery_adc_enabled = false;

    p_out.gyro_bias.still_th     = 2.0f;
    p_out.gyro_bias.still_win_ms = 250;
    p_out.gyro_bias.alpha        = 0.001f;

    p_out.linear.th         = 0.3f;
    p_out.linear.impulse_th = 0.5f;
    p_out.linear.window_ms  = 300;

    p_out.flick.p2p_th      = 400.0f;
    p_out.flick.window_ms   = 200;
    p_out.flick.cooldown_ms = 600;

    p_out.tilt_hold.angle_deg = 15.0f;
    p_out.tilt_hold.hold_ms   = 300;
    p_out.tilt_hold.repeat_hz = 3;

    p_out.sleep_idle_timeout_ms = 60000;
    p_out.active_mode           = 1;
    p_out.active_peer_index     = 0;

    for (uint8_t m = 0; m < C10_DEF::MODE_COUNT; m++) {
        makeDefaultsMode(m, p_out.modes[m]);
    }
}

// =======================================================
// Validation
// =======================================================
bool CL_C10_Config::validateWiFi(const ST_C10_WiFiConfig_t& p_w) const {
    if (p_w.mode > (uint8_t)EN_C10_WIFI_STA) return false;

    size_t v_apLen = strlen(p_w.ap_pass);
    if (v_apLen > 0 && v_apLen < 8) return false;

    if (strlen(p_w.mdns_host) > C10_DEF::MDNS_MAX) return false;
    return true;
}

bool CL_C10_Config::validateSlot(const ST_C20_ActionSlot_t& p_s) const {
    if (p_s.kind >= (uint8_t)EN_C20_ACT_MAX) return false;
    if (p_s.holdMode > (uint8_t)EN_C20_HOLD_REPEAT) return false;

    switch ((EN_C20_ActionKind_t)p_s.kind) {
        case EN_C20_ACT_NONE:
            return true;
        case EN_C20_ACT_MOUSE_CLICK:
        case EN_C20_ACT_MOUSE_HOLD:
            return (p_s.param16 & 0xFF) != 0;
        case EN_C20_ACT_MOUSE_WHEEL:
            return C20_WheelAxis(p_s.param16) <= 1 && C20_WheelDir(p_s.param16) <= 1;
        case EN_C20_ACT_KB_TAP:
        case EN_C20_ACT_KB_REPEAT:
            return (uint8_t)p_s.param16 <= 0xE7;
        case EN_C20_ACT_KB_COMBO:
            return true;
        case EN_C20_ACT_CONSUMER_TAP:
        case EN_C20_ACT_CONSUMER_REPEAT:
            return p_s.param32 != 0;
        case EN_C20_ACT_SPECIAL:
            return (uint8_t)p_s.param16 < (uint8_t)EN_C20_SP_MAX;
        default:
            return false;
    }
}

bool CL_C10_Config::validateMode(const ST_C10_ModeConfig_t& p_m, uint8_t p_modeIdx) const {
    if (p_modeIdx >= C10_DEF::MODE_COUNT) return false;

    for (uint8_t i = 0; i < C10_DEF::SLOT_BTN_COUNT; i++) {
        if (!validateSlot(p_m.slots[i])) return false;
    }
    for (uint8_t i = 0; i < C10_DEF::SLOT_FLICK_COUNT; i++) {
        if (!validateSlot(p_m.flick[i])) return false;
    }
    for (uint8_t i = 0; i < C10_DEF::SLOT_LINEAR_COUNT; i++) {
        if (!validateSlot(p_m.linear[i])) return false;
    }
    for (uint8_t i = 0; i < C10_DEF::SLOT_TILT_COUNT; i++) {
        if (!validateSlot(p_m.tilt[i])) return false;
    }
    return true;
}

bool CL_C10_Config::validateE10(const ST_C10_E10Config_t& p_e) const {
    if (p_e.dpi_level < 1 || p_e.dpi_level > 3) return false;

    if (p_e.accel_threshold < 0.0f || p_e.accel_threshold > 50.0f) return false;
    if (p_e.wheel_threshold_deg < 1.0f || p_e.wheel_threshold_deg > 360.0f) return false;
    if (p_e.wheel_step_max < 1 || p_e.wheel_step_max > 50) return false;

    if (p_e.gesture_flick_deg < 10.0f || p_e.gesture_flick_deg > 2000.0f) return false;
    if (p_e.gesture_cooldown_ms > 20000) return false;

    if (p_e.scroll_cursor_damp < 0.0f || p_e.scroll_cursor_damp > 1.0f) return false;

    // ---- Precision ----
    if (p_e.precision_mode >= (uint8_t)EN_C10_E10_PREC_MAX) return false;
    if (p_e.precision_deadzone < 0.0f || p_e.precision_deadzone > 50.0f) return false;
    if (p_e.precision_gain < 0.0f || p_e.precision_gain > 5.0f) return false;
    if (p_e.precision_accel < 0.0f || p_e.precision_accel > 5.0f) return false;
    if (p_e.precision_max_step < 1 || p_e.precision_max_step > 200) return false;
    if (p_e.precision_smooth < 0.0f || p_e.precision_smooth > 1.0f) return false;
    if (p_e.prec_entry_ms < 50 || p_e.prec_entry_ms > 5000) return false;
    if (p_e.prec_exit_ms < 50 || p_e.prec_exit_ms > 5000) return false;
    if (p_e.prec_entry_still_deg < 0.1f || p_e.prec_entry_still_deg > 20.0f) return false;
    if (p_e.prec_exit_move_deg < 0.1f || p_e.prec_exit_move_deg > 50.0f) return false;
    if (p_e.prec_profile > 5) return false;

    if (p_e.gyro_bias.still_th < 0.1f || p_e.gyro_bias.still_th > 20.0f) return false;
    if (p_e.gyro_bias.still_win_ms < 50 || p_e.gyro_bias.still_win_ms > 5000) return false;
    if (p_e.gyro_bias.alpha < 0.0001f || p_e.gyro_bias.alpha > 0.1f) return false;

    if (p_e.linear.th < 0.05f || p_e.linear.th > 5.0f) return false;
    if (p_e.linear.impulse_th < 0.1f || p_e.linear.impulse_th > 5.0f) return false;
    if (p_e.linear.window_ms < 50 || p_e.linear.window_ms > 1000) return false;

    if (p_e.flick.p2p_th < 50.0f || p_e.flick.p2p_th > 2000.0f) return false;
    if (p_e.flick.window_ms < 50 || p_e.flick.window_ms > 500) return false;
    if (p_e.flick.cooldown_ms < 100 || p_e.flick.cooldown_ms > 5000) return false;

    if (p_e.tilt_hold.angle_deg < 5.0f || p_e.tilt_hold.angle_deg > 45.0f) return false;
    if (p_e.tilt_hold.hold_ms < 100 || p_e.tilt_hold.hold_ms > 2000) return false;
    if (p_e.tilt_hold.repeat_hz < 1 || p_e.tilt_hold.repeat_hz > 20) return false;

    if (p_e.sleep_idle_timeout_ms < 5000 || p_e.sleep_idle_timeout_ms > 3600000) return false;

    if (p_e.active_mode < 1 || p_e.active_mode > C10_DEF::MODE_COUNT) return false;
    if (p_e.active_peer_index > 2) return false;

    for (uint8_t m = 0; m < C10_DEF::MODE_COUNT; m++) {
        if (!validateMode(p_e.modes[m], m)) return false;
    }
    return true;
}

// =======================================================
// SafeBoot APIs
// =======================================================
bool CL_C10_Config::clearSafeMode() {
    _boot.safe_mode  = false;
    _boot.fail_count = 0;
    _boot.pending    = false;
    return _saveBootState(_boot);
}

bool CL_C10_Config::bootMarkOkIfGracePassed(uint32_t p_graceMs) {
    if (_boot.boot_ms == 0) _boot.boot_ms = (uint32_t)millis();

    const uint32_t v_now     = (uint32_t)millis();
    const uint32_t v_aliveMs = (v_now >= _boot.boot_ms) ? (v_now - _boot.boot_ms) : 0;

    if (v_aliveMs < p_graceMs) return false;

    // 실패 후 백오프
    if (_bootOkRetryAtMs != 0 && (int32_t)(v_now - _bootOkRetryAtMs) < 0) {
        return false;
    }

    _boot.pending = false;
    if (!_boot.safe_mode) _boot.fail_count = 0;

    const bool v_ok = _saveBootState(_boot);
    if (v_ok) {
        _bootOkRetryAtMs = 0;
    } else {
        _bootOkRetryAtMs = v_now + 30000u;
    }
    return v_ok;
}

// =======================================================
// Build Patched
// =======================================================
bool CL_C10_Config::buildPatchedAll(const String&        p_patchJson,
                                    ST_C10_WiFiConfig_t& p_wifi,
                                    ST_C10_E10Config_t&  p_e10,
                                    bool                 p_loadCurrent) {
    makeDefaultsWiFi(p_wifi);
    makeDefaultsE10(p_e10);

    if (p_loadCurrent) {
        (void)loadAll(p_wifi, p_e10);
    }

    bool v_ok = true;
    v_ok      = v_ok && patchFromJsonWiFi(p_patchJson, p_wifi);
    v_ok      = v_ok && patchFromJsonE10(p_patchJson, p_e10);
    v_ok      = v_ok && validateWiFi(p_wifi);
    v_ok      = v_ok && validateE10(p_e10);
    return v_ok;
}

bool CL_C10_Config::buildPatchedE10(const String& p_patchJson, ST_C10_E10Config_t& p_e10, bool p_loadCurrent) {
    ST_C10_WiFiConfig_t v_dummy;
    makeDefaultsWiFi(v_dummy);
    makeDefaultsE10(p_e10);

    if (p_loadCurrent) {
        ST_C10_WiFiConfig_t v_w;
        ST_C10_E10Config_t  v_e;
        makeDefaultsWiFi(v_w);
        makeDefaultsE10(v_e);
        (void)loadAll(v_w, v_e);
        p_e10 = v_e;
    }

    bool v_ok = true;
    v_ok      = v_ok && patchFromJsonE10(p_patchJson, p_e10);
    v_ok      = v_ok && validateE10(p_e10);
    return v_ok;
}

// =======================================================
// Load / Save
// =======================================================
bool CL_C10_Config::loadAll(ST_C10_WiFiConfig_t& p_wifi, ST_C10_E10Config_t& p_e10) {
    File v_f = LittleFS.open(C10_DEF::CFG_PATH, "r");
    if (!v_f) return false;

    JsonDocument         v_doc;
    DeserializationError v_err = deserializeJson(v_doc, v_f);
    v_f.close();
    if (v_err) return false;

    makeDefaultsWiFi(p_wifi);
    makeDefaultsE10(p_e10);

    String v_json;
    serializeJson(v_doc, v_json);

    (void)patchFromJsonWiFi(v_json, p_wifi);
    (void)patchFromJsonE10(v_json, p_e10);
    return true;
}

bool CL_C10_Config::saveAll(const ST_C10_WiFiConfig_t& p_wifi, const ST_C10_E10Config_t& p_e10) {
    if (!validateWiFi(p_wifi)) return false;
    if (!validateE10(p_e10)) return false;

    JsonDocument v_doc;
    _buildJson(p_wifi, p_e10, v_doc);

    File v_tmp = LittleFS.open(C10_DEF::CFG_TMP, "w");
    if (!v_tmp) return false;

    if (serializeJson(v_doc, v_tmp) == 0) {
        v_tmp.close();
        (void)LittleFS.remove(C10_DEF::CFG_TMP);
        return false;
    }
    v_tmp.flush();
    v_tmp.close();

    if (!_verifyJsonFile(C10_DEF::CFG_TMP)) {
        (void)LittleFS.remove(C10_DEF::CFG_TMP);
        return false;
    }

    // rotate current -> bak (rename 우선, copy fallback)
    if (LittleFS.exists(C10_DEF::CFG_BAK)) (void)LittleFS.remove(C10_DEF::CFG_BAK);
    if (LittleFS.exists(C10_DEF::CFG_PATH)) {
        bool v_bakOk = LittleFS.rename(C10_DEF::CFG_PATH, C10_DEF::CFG_BAK);
        if (!v_bakOk) v_bakOk = _copyFile(C10_DEF::CFG_PATH, C10_DEF::CFG_BAK);
        if (!v_bakOk) {
            (void)LittleFS.remove(C10_DEF::CFG_TMP);
            return false;
        }
    }

    // commit tmp -> path
    bool v_commitOk = LittleFS.rename(C10_DEF::CFG_TMP, C10_DEF::CFG_PATH);
    if (!v_commitOk) {
        v_commitOk = _copyFile(C10_DEF::CFG_TMP, C10_DEF::CFG_PATH);
        (void)LittleFS.remove(C10_DEF::CFG_TMP);
    }

    if (!v_commitOk) {
        (void)LittleFS.remove(C10_DEF::CFG_PATH);
        (void)rollbackFromBak();
        (void)LittleFS.remove(C10_DEF::CFG_TMP);
        return false;
    }

    if (!_verifyJsonFile(C10_DEF::CFG_PATH)) {
        (void)LittleFS.remove(C10_DEF::CFG_PATH);
        (void)rollbackFromBak();
        return false;
    }
    return true;
}

bool CL_C10_Config::rollbackFromBak() {
    if (!LittleFS.exists(C10_DEF::CFG_BAK)) return false;
    if (!_verifyJsonFile(C10_DEF::CFG_BAK)) return false;

    if (LittleFS.exists(C10_DEF::CFG_PATH)) (void)LittleFS.remove(C10_DEF::CFG_PATH);

    // copy 우선(bak 유지) → 실패 시 rename
    if (_copyFile(C10_DEF::CFG_BAK, C10_DEF::CFG_PATH)) return true;
    if (LittleFS.rename(C10_DEF::CFG_BAK, C10_DEF::CFG_PATH)) {
        D10_LOGW("[C10] rollbackFromBak: copy failed, used rename (bak lost)");
        return true;
    }
    return false;
}

// =======================================================
// ETag / Export / Import
// =======================================================
bool CL_C10_Config::getConfigEtag(uint32_t& p_outHash, size_t* p_outSize) {
    p_outHash     = 2166136261u;
    size_t v_size = 0;

    auto fnv = [&](const uint8_t* b, size_t n) {
        uint32_t h = p_outHash;
        for (size_t i = 0; i < n; i++) {
            h ^= (uint32_t)b[i];
            h *= 16777619u;
        }
        p_outHash = h;
    };

    if (LittleFS.exists(C10_DEF::CFG_PATH)) {
        File f = LittleFS.open(C10_DEF::CFG_PATH, "r");
        if (!f) return false;
        uint8_t buf[256];
        while (true) {
            int n = f.read(buf, sizeof(buf));
            if (n <= 0) break;
            fnv(buf, (size_t)n);
            v_size += (size_t)n;
        }
        f.close();
    } else {
        ST_C10_WiFiConfig_t w;
        ST_C10_E10Config_t  e;
        makeDefaultsWiFi(w);
        makeDefaultsE10(e);
        JsonDocument d;
        _buildJson(w, e, d);
        String s;
        serializeJson(d, s);
        fnv((const uint8_t*)s.c_str(), s.length());
        v_size = s.length();
    }

    if (p_outSize) *p_outSize = v_size;
    return true;
}

bool CL_C10_Config::exportJson(String& p_out) {
    File v_f = LittleFS.open(C10_DEF::CFG_PATH, "r");
    if (!v_f) return false;
    p_out = v_f.readString();
    v_f.close();
    return (p_out.length() > 0);
}

bool CL_C10_Config::importJson(const String& p_json, bool& p_saved, bool& p_applied) {
    p_saved   = false;
    p_applied = false;

    {
        JsonDocument v_doc;
        if (deserializeJson(v_doc, p_json)) return false;
    }

    ST_C10_WiFiConfig_t v_w;
    ST_C10_E10Config_t  v_e;

    if (!buildPatchedAll(p_json, v_w, v_e, true)) return false;

    p_saved   = saveAll(v_w, v_e);
    p_applied = p_saved;
    return p_saved;
}

// =======================================================
// Patchers (WiFi)
// =======================================================
bool CL_C10_Config::patchFromJsonWiFi(const String& p_json, ST_C10_WiFiConfig_t& p_wifi) {
    JsonDocument v_doc;
    if (deserializeJson(v_doc, p_json)) return false;

    JsonVariant v_wifi = v_doc["wifi"];
    if (v_wifi.isNull()) return true;

    if (!v_wifi["mode"].isNull()) p_wifi.mode = (uint8_t)v_wifi["mode"];

    JsonVariant v_sta = v_wifi["sta"];
    if (!v_sta.isNull()) {
        if (!v_sta["ssid"].isNull()) strlcpy(p_wifi.sta_ssid, (const char*)v_sta["ssid"], sizeof(p_wifi.sta_ssid));
        if (!v_sta["pass"].isNull()) strlcpy(p_wifi.sta_pass, (const char*)v_sta["pass"], sizeof(p_wifi.sta_pass));
    }

    JsonVariant v_ap = v_wifi["ap"];
    if (!v_ap.isNull()) {
        if (!v_ap["ssid"].isNull()) strlcpy(p_wifi.ap_ssid, (const char*)v_ap["ssid"], sizeof(p_wifi.ap_ssid));
        if (!v_ap["pass"].isNull()) strlcpy(p_wifi.ap_pass, (const char*)v_ap["pass"], sizeof(p_wifi.ap_pass));
    }

    JsonVariant v_mdns = v_wifi["mdns"];
    if (!v_mdns.isNull()) {
        if (!v_mdns["host"].isNull()) strlcpy(p_wifi.mdns_host, (const char*)v_mdns["host"], sizeof(p_wifi.mdns_host));
    }
    return true;
}

// =======================================================
// Patchers (E10) — Mode/Slot 포함
// =======================================================
bool CL_C10_Config::patchFromJsonE10(const String& p_json, ST_C10_E10Config_t& p_e10) {
    JsonDocument v_doc;
    if (deserializeJson(v_doc, p_json)) return false;

    JsonVariant v_e10 = v_doc["e10"];
    if (v_e10.isNull()) return true;

    if (!v_e10["dpi_level"].isNull()) p_e10.dpi_level = (uint8_t)v_e10["dpi_level"];
    if (!v_e10["hard_click_lock"].isNull()) p_e10.hard_click_lock = (bool)v_e10["hard_click_lock"];

    JsonVariant v_sb = v_e10["scale_base"];
    if (v_sb.is<JsonArray>()) {
        JsonArray v_a = v_sb.as<JsonArray>();
        if (v_a.size() >= 3) {
            p_e10.scale_base[0] = (float)v_a[0];
            p_e10.scale_base[1] = (float)v_a[1];
            p_e10.scale_base[2] = (float)v_a[2];
        }
    }
    JsonVariant v_ag = v_e10["accel_gain"];
    if (v_ag.is<JsonArray>()) {
        JsonArray v_a = v_ag.as<JsonArray>();
        if (v_a.size() >= 3) {
            p_e10.accel_gain[0] = (float)v_a[0];
            p_e10.accel_gain[1] = (float)v_a[1];
            p_e10.accel_gain[2] = (float)v_a[2];
        }
    }

    if (!v_e10["accel_threshold"].isNull()) p_e10.accel_threshold = (float)v_e10["accel_threshold"];

    JsonVariant v_wh = v_e10["wheel"];
    if (!v_wh.isNull()) {
        if (!v_wh["threshold_deg"].isNull()) p_e10.wheel_threshold_deg = (float)v_wh["threshold_deg"];
        if (!v_wh["step_max"].isNull()) p_e10.wheel_step_max = (uint8_t)v_wh["step_max"];
    }

    JsonVariant v_g = v_e10["gesture"];
    if (!v_g.isNull()) {
        if (!v_g["flick_deg"].isNull()) p_e10.gesture_flick_deg = (float)v_g["flick_deg"];
        if (!v_g["cooldown_ms"].isNull()) p_e10.gesture_cooldown_ms = (uint16_t)v_g["cooldown_ms"];
    }

    if (!v_e10["scroll_cursor_damp"].isNull()) p_e10.scroll_cursor_damp = (float)v_e10["scroll_cursor_damp"];

    // ---- Precision ----
    JsonVariant v_p = v_e10["precision"];
    if (!v_p.isNull()) {
        if (!v_p["mode"].isNull()) p_e10.precision_mode = (uint8_t)v_p["mode"];
        if (!v_p["deadzone"].isNull()) p_e10.precision_deadzone = (float)v_p["deadzone"];
        if (!v_p["gain"].isNull()) p_e10.precision_gain = (float)v_p["gain"];
        if (!v_p["accel"].isNull()) p_e10.precision_accel = (float)v_p["accel"];
        if (!v_p["max_step"].isNull()) p_e10.precision_max_step = (uint8_t)v_p["max_step"];
        if (!v_p["smooth"].isNull()) p_e10.precision_smooth = (float)v_p["smooth"];
        if (!v_p["entry_ms"].isNull()) p_e10.prec_entry_ms = (uint16_t)v_p["entry_ms"];
        if (!v_p["exit_ms"].isNull()) p_e10.prec_exit_ms = (uint16_t)v_p["exit_ms"];
        if (!v_p["entry_still_deg"].isNull()) p_e10.prec_entry_still_deg = (float)v_p["entry_still_deg"];
        if (!v_p["exit_move_deg"].isNull()) p_e10.prec_exit_move_deg = (float)v_p["exit_move_deg"];
        if (!v_p["profile"].isNull()) p_e10.prec_profile = (uint8_t)v_p["profile"];
    }

    // ---- v0410 신규 ----
    if (!v_e10["led_brightness"].isNull()) p_e10.led_brightness = (uint8_t)v_e10["led_brightness"];
    if (!v_e10["battery_adc_enabled"].isNull()) p_e10.battery_adc_enabled = (bool)v_e10["battery_adc_enabled"];

    JsonVariant v_gb = v_e10["gyro_bias"];
    if (!v_gb.isNull()) {
        if (!v_gb["still_th"].isNull()) p_e10.gyro_bias.still_th = (float)v_gb["still_th"];
        if (!v_gb["still_win_ms"].isNull()) p_e10.gyro_bias.still_win_ms = (uint16_t)v_gb["still_win_ms"];
        if (!v_gb["alpha"].isNull()) p_e10.gyro_bias.alpha = (float)v_gb["alpha"];
    }

    JsonVariant v_ln = v_e10["linear"];
    if (!v_ln.isNull()) {
        if (!v_ln["th"].isNull()) p_e10.linear.th = (float)v_ln["th"];
        if (!v_ln["impulse_th"].isNull()) p_e10.linear.impulse_th = (float)v_ln["impulse_th"];
        if (!v_ln["window_ms"].isNull()) p_e10.linear.window_ms = (uint16_t)v_ln["window_ms"];
    }

    JsonVariant v_fl = v_e10["flick"];
    if (!v_fl.isNull()) {
        if (!v_fl["p2p_th"].isNull()) p_e10.flick.p2p_th = (float)v_fl["p2p_th"];
        if (!v_fl["window_ms"].isNull()) p_e10.flick.window_ms = (uint16_t)v_fl["window_ms"];
        if (!v_fl["cooldown_ms"].isNull()) p_e10.flick.cooldown_ms = (uint16_t)v_fl["cooldown_ms"];
    }

    JsonVariant v_th = v_e10["tilt_hold"];
    if (!v_th.isNull()) {
        if (!v_th["angle_deg"].isNull()) p_e10.tilt_hold.angle_deg = (float)v_th["angle_deg"];
        if (!v_th["hold_ms"].isNull()) p_e10.tilt_hold.hold_ms = (uint16_t)v_th["hold_ms"];
        if (!v_th["repeat_hz"].isNull()) p_e10.tilt_hold.repeat_hz = (uint8_t)v_th["repeat_hz"];
    }

    if (!v_e10["sleep_idle_timeout_ms"].isNull())
        p_e10.sleep_idle_timeout_ms = (uint32_t)v_e10["sleep_idle_timeout_ms"];
    if (!v_e10["active_mode"].isNull()) p_e10.active_mode = (uint8_t)v_e10["active_mode"];
    if (!v_e10["active_peer_index"].isNull()) p_e10.active_peer_index = (uint8_t)v_e10["active_peer_index"];

    // ---- Mode 슬롯 매트릭스 ----
    JsonVariant v_modes = v_e10["modes"];
    if (!v_modes.isNull()) {
        for (uint8_t m = 0; m < C10_DEF::MODE_COUNT; m++) {
            char v_key[4];
            snprintf(v_key, sizeof(v_key), "%u", (unsigned)(m + 1));
            JsonVariant v_m = v_modes[v_key];
            if (v_m.isNull()) continue;
            (void)_patchModeJson(v_m, p_e10.modes[m]);
        }
    }

    return true;
}

// =======================================================
// JSON build (struct -> Json)
// =======================================================
void CL_C10_Config::_buildJson(const ST_C10_WiFiConfig_t& p_w, const ST_C10_E10Config_t& p_e, JsonDocument& p_doc) {
    p_doc["ver"] = (uint16_t)G_C10_CFG_VER;

    // ---- WiFi ----
    JsonObject v_jw = p_doc["wifi"].to<JsonObject>();
    v_jw["mode"]    = p_w.mode;

    JsonObject v_jsta = v_jw["sta"].to<JsonObject>();
    v_jsta["ssid"]    = p_w.sta_ssid;
    v_jsta["pass"]    = p_w.sta_pass;

    JsonObject v_jap = v_jw["ap"].to<JsonObject>();
    v_jap["ssid"]    = p_w.ap_ssid;
    v_jap["pass"]    = p_w.ap_pass;

    JsonObject v_jmd = v_jw["mdns"].to<JsonObject>();
    v_jmd["host"]    = p_w.mdns_host;

    // ---- E10 ----
    JsonObject v_je = p_doc["e10"].to<JsonObject>();

    v_je["dpi_level"]       = p_e.dpi_level;
    v_je["hard_click_lock"] = p_e.hard_click_lock;

    JsonArray v_sb = v_je["scale_base"].to<JsonArray>();
    v_sb.add(p_e.scale_base[0]);
    v_sb.add(p_e.scale_base[1]);
    v_sb.add(p_e.scale_base[2]);

    JsonArray v_ag = v_je["accel_gain"].to<JsonArray>();
    v_ag.add(p_e.accel_gain[0]);
    v_ag.add(p_e.accel_gain[1]);
    v_ag.add(p_e.accel_gain[2]);

    v_je["accel_threshold"] = p_e.accel_threshold;

    JsonObject v_wh       = v_je["wheel"].to<JsonObject>();
    v_wh["threshold_deg"] = p_e.wheel_threshold_deg;
    v_wh["step_max"]      = p_e.wheel_step_max;

    JsonObject v_g     = v_je["gesture"].to<JsonObject>();
    v_g["flick_deg"]   = p_e.gesture_flick_deg;
    v_g["cooldown_ms"] = p_e.gesture_cooldown_ms;

    v_je["scroll_cursor_damp"] = p_e.scroll_cursor_damp;

    // ---- Precision ----
    JsonObject v_p         = v_je["precision"].to<JsonObject>();
    v_p["mode"]            = p_e.precision_mode;
    v_p["deadzone"]        = p_e.precision_deadzone;
    v_p["gain"]            = p_e.precision_gain;
    v_p["accel"]           = p_e.precision_accel;
    v_p["max_step"]        = p_e.precision_max_step;
    v_p["smooth"]          = p_e.precision_smooth;
    v_p["entry_ms"]        = p_e.prec_entry_ms;
    v_p["exit_ms"]         = p_e.prec_exit_ms;
    v_p["entry_still_deg"] = p_e.prec_entry_still_deg;
    v_p["exit_move_deg"]   = p_e.prec_exit_move_deg;
    v_p["profile"]         = p_e.prec_profile;

    v_je["led_brightness"]      = p_e.led_brightness;
    v_je["battery_adc_enabled"] = p_e.battery_adc_enabled;

    JsonObject v_gb      = v_je["gyro_bias"].to<JsonObject>();
    v_gb["still_th"]     = p_e.gyro_bias.still_th;
    v_gb["still_win_ms"] = p_e.gyro_bias.still_win_ms;
    v_gb["alpha"]        = p_e.gyro_bias.alpha;

    JsonObject v_ln    = v_je["linear"].to<JsonObject>();
    v_ln["th"]         = p_e.linear.th;
    v_ln["impulse_th"] = p_e.linear.impulse_th;
    v_ln["window_ms"]  = p_e.linear.window_ms;

    JsonObject v_fl     = v_je["flick"].to<JsonObject>();
    v_fl["p2p_th"]      = p_e.flick.p2p_th;
    v_fl["window_ms"]   = p_e.flick.window_ms;
    v_fl["cooldown_ms"] = p_e.flick.cooldown_ms;

    JsonObject v_th   = v_je["tilt_hold"].to<JsonObject>();
    v_th["angle_deg"] = p_e.tilt_hold.angle_deg;
    v_th["hold_ms"]   = p_e.tilt_hold.hold_ms;
    v_th["repeat_hz"] = p_e.tilt_hold.repeat_hz;

    v_je["sleep_idle_timeout_ms"] = p_e.sleep_idle_timeout_ms;
    v_je["active_mode"]           = p_e.active_mode;
    v_je["active_peer_index"]     = p_e.active_peer_index;

    // ---- Modes ----
    JsonObject v_modes = v_je["modes"].to<JsonObject>();
    for (uint8_t m = 0; m < C10_DEF::MODE_COUNT; m++) {
        char v_key[4];
        snprintf(v_key, sizeof(v_key), "%u", (unsigned)(m + 1));
        JsonObject v_mo = v_modes[v_key].to<JsonObject>();
        _buildModeJson(v_mo, p_e.modes[m]);
    }
}

void CL_C10_Config::_buildModeJson(JsonObject p_parent, const ST_C10_ModeConfig_t& p_m) {
    _buildSlotArray(p_parent, "slots", p_m.slots, C10_DEF::SLOT_BTN_COUNT);
    _buildSlotArray(p_parent, "flick", p_m.flick, C10_DEF::SLOT_FLICK_COUNT);
    _buildSlotArray(p_parent, "linear", p_m.linear, C10_DEF::SLOT_LINEAR_COUNT);
    _buildSlotArray(p_parent, "tilt", p_m.tilt, C10_DEF::SLOT_TILT_COUNT);
}

void CL_C10_Config::_buildSlotArray(JsonObject p_parent, const char* p_key, const ST_C20_ActionSlot_t* p_slots, uint8_t p_count) {
    JsonArray arr = p_parent[p_key].to<JsonArray>();
    for (uint8_t i = 0; i < p_count; i++) {
        JsonObject o = arr.add<JsonObject>();
        _buildSlotJson(o, p_slots[i]);
    }
}

void CL_C10_Config::_buildSlotJson(JsonObject p_obj, const ST_C20_ActionSlot_t& p_s) {
    p_obj["k"]   = p_s.kind;
    p_obj["h"]   = p_s.holdMode;
    p_obj["p16"] = p_s.param16;
    p_obj["p32"] = (uint32_t)p_s.param32;
}

// =======================================================
// JSON patch (Json -> struct) — Mode/Slot
// =======================================================
bool CL_C10_Config::_patchModeJson(JsonVariantConst p_v, ST_C10_ModeConfig_t& p_m) {
    if (p_v.isNull()) return false;

    (void)_patchSlotArray(p_v["slots"], p_m.slots, C10_DEF::SLOT_BTN_COUNT);
    (void)_patchSlotArray(p_v["flick"], p_m.flick, C10_DEF::SLOT_FLICK_COUNT);
    (void)_patchSlotArray(p_v["linear"], p_m.linear, C10_DEF::SLOT_LINEAR_COUNT);
    (void)_patchSlotArray(p_v["tilt"], p_m.tilt, C10_DEF::SLOT_TILT_COUNT);
    return true;
}

bool CL_C10_Config::_patchSlotArray(JsonVariantConst p_v, ST_C20_ActionSlot_t* p_slots, uint8_t p_count) {
    if (p_v.isNull()) return false;

    JsonArrayConst arr = p_v.as<JsonArrayConst>();
    if (arr.isNull()) return false;

    const uint8_t n = (arr.size() < p_count) ? (uint8_t)arr.size() : p_count;
    for (uint8_t i = 0; i < n; i++) {
        (void)_patchSlotJson(arr[i], p_slots[i]);
    }
    return true;
}

bool CL_C10_Config::_patchSlotJson(JsonVariantConst p_v, ST_C20_ActionSlot_t& p_s) {
    if (p_v.isNull()) return false;

    if (!p_v["k"].isNull()) p_s.kind = (uint8_t)p_v["k"];
    if (!p_v["h"].isNull()) p_s.holdMode = (uint8_t)p_v["h"];
    if (!p_v["p16"].isNull()) p_s.param16 = (uint16_t)p_v["p16"];
    if (!p_v["p32"].isNull()) p_s.param32 = (uint32_t)p_v["p32"];
    return true;
}

// =======================================================
// Boot state
// =======================================================
bool CL_C10_Config::_loadBootState(ST_C10_BootState_t& p_out) {
    memset(&p_out, 0, sizeof(p_out));

    if (!LittleFS.exists(C10_DEF::BOOT_PATH)) {
        return _saveBootState(p_out);
    }

    File v_f = LittleFS.open(C10_DEF::BOOT_PATH, "r");
    if (!v_f) return false;

    JsonDocument         v_d;
    DeserializationError v_err = deserializeJson(v_d, v_f);
    v_f.close();
    if (v_err) return false;

    if (!v_d["safe_mode"].isNull()) p_out.safe_mode = (bool)v_d["safe_mode"];
    if (!v_d["fail_count"].isNull()) p_out.fail_count = (uint8_t)v_d["fail_count"];
    if (!v_d["pending"].isNull()) p_out.pending = (bool)v_d["pending"];
    if (!v_d["boot_ms"].isNull()) p_out.boot_ms = (uint32_t)v_d["boot_ms"];
    if (!v_d["last_reset_reason"].isNull()) p_out.last_reset_reason = (uint8_t)v_d["last_reset_reason"];
    return true;
}

bool CL_C10_Config::_saveBootState(const ST_C10_BootState_t& p_in) {
    JsonDocument v_d;
    v_d["safe_mode"]         = p_in.safe_mode;
    v_d["fail_count"]        = p_in.fail_count;
    v_d["pending"]           = p_in.pending;
    v_d["boot_ms"]           = p_in.boot_ms;
    v_d["last_reset_reason"] = p_in.last_reset_reason;

    File v_tmp = LittleFS.open(C10_DEF::BOOT_TMP_PATH, "w");
    if (!v_tmp) return false;

    if (serializeJson(v_d, v_tmp) == 0) {
        v_tmp.close();
        (void)LittleFS.remove(C10_DEF::BOOT_TMP_PATH);
        return false;
    }
    v_tmp.flush();
    v_tmp.close();

    if (!_verifyJsonFile(C10_DEF::BOOT_TMP_PATH)) {
        (void)LittleFS.remove(C10_DEF::BOOT_TMP_PATH);
        return false;
    }

    if (LittleFS.exists(C10_DEF::BOOT_PATH)) (void)LittleFS.remove(C10_DEF::BOOT_PATH);
    return LittleFS.rename(C10_DEF::BOOT_TMP_PATH, C10_DEF::BOOT_PATH);
}

// =======================================================
// File utils
// =======================================================
bool CL_C10_Config::_verifyJsonFile(const char* p_path) {
    File v_f = LittleFS.open(p_path, "r");
    if (!v_f) return false;

    JsonDocument v_d;
    bool         v_ok = !deserializeJson(v_d, v_f);
    v_f.close();
    return v_ok;
}

bool CL_C10_Config::_copyFile(const char* p_src, const char* p_dst) {
    File v_s = LittleFS.open(p_src, "r");
    if (!v_s) return false;

    File v_d = LittleFS.open(p_dst, "w");
    if (!v_d) {
        v_s.close();
        return false;
    }

    uint8_t v_buf[256];
    while (true) {
        int v_n = v_s.read(v_buf, sizeof(v_buf));
        if (v_n <= 0) break;
        if (v_d.write(v_buf, (size_t)v_n) != (size_t)v_n) {
            v_s.close();
            v_d.close();
            return false;
        }
    }

    v_d.flush();
    v_s.close();
    v_d.close();
    return true;
}

// =======================================================
// Reset reason
// =======================================================
bool CL_C10_Config::_isBadResetReason(esp_reset_reason_t p_r) {
    switch (p_r) {
        case ESP_RST_PANIC:
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT:
        case ESP_RST_BROWNOUT:
            return true;
        default:
            return false;
    }
}

uint8_t CL_C10_Config::_getResetReasonU8() {
    return (uint8_t)esp_reset_reason();
}

// =======================================================
// Factory Reset
// =======================================================
bool CL_C10_Config::factoryReset(bool p_recreateDefault) {
    if (LittleFS.exists(C10_DEF::CFG_TMP)) (void)LittleFS.remove(C10_DEF::CFG_TMP);
    if (LittleFS.exists(C10_DEF::CFG_BAK)) (void)LittleFS.remove(C10_DEF::CFG_BAK);
    if (LittleFS.exists(C10_DEF::CFG_PATH)) (void)LittleFS.remove(C10_DEF::CFG_PATH);

    if (LittleFS.exists(C10_DEF::BOOT_TMP_PATH)) (void)LittleFS.remove(C10_DEF::BOOT_TMP_PATH);
    if (LittleFS.exists(C10_DEF::BOOT_PATH)) (void)LittleFS.remove(C10_DEF::BOOT_PATH);

    memset(&_boot, 0, sizeof(_boot));
    _boot.pending = false;
    (void)_saveBootState(_boot);

    if (p_recreateDefault) {
        ST_C10_WiFiConfig_t v_w;
        ST_C10_E10Config_t  v_e;
        makeDefaultsWiFi(v_w);
        makeDefaultsE10(v_e);
        return saveAll(v_w, v_e);
    }
    return true;
}
