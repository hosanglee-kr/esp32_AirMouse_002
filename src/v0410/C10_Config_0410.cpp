// File: src/v0410/C10_Config_0410.cpp
// =======================================================
#include "C10_Config_0410.h"

// =======================================================
// helper: profile index → 기본 이름
// =======================================================
static const char* _C10_defaultProfileName(uint8_t p_idx) {
    static const char* G_NAMES[C10_DEF::PROFILE_MAX] = {
        "Office", "Gaming", "Present", "TV", "Spare"
    };
    if (p_idx >= C10_DEF::PROFILE_MAX) return "Profile";
    return G_NAMES[p_idx];
}

// =======================================================
// ctor / begin
// =======================================================
CL_C10_Config::CL_C10_Config() {
    memset(&_boot,  0, sizeof(_boot));
    memset(&_index, 0, sizeof(_index));
    _bootOkRetryAtMs = 0;
    _profileSwitchInProgress = false;
}

void CL_C10_Config::begin(bool p_formatOnFail) {
    (void)LittleFS.begin(p_formatOnFail);

    if (!LittleFS.exists("/json"))               (void)LittleFS.mkdir("/json");
    if (!LittleFS.exists(C10_DEF::PROFILES_DIR)) (void)LittleFS.mkdir(C10_DEF::PROFILES_DIR);

    // ---------- boot state ----------
    _loadBootState(_boot);
    _boot.last_reset_reason = _getResetReasonU8();

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

    // ---------- index ----------
    if (!_loadIndex()) {
        _index.activeIndex  = 0;
        _index.profileCount = 1;
        (void)_saveIndex();
    }

    // ---------- profile_0.json 존재 보장 ----------
    {
        char v_p[64];
        if (C10_DEF::makeProfilePath(v_p, sizeof(v_p), 0)) {
            if (!LittleFS.exists(v_p)) {
                ST_C10_ProfileConfig_t v_def;
                makeDefaultsProfile(0, v_def);
                (void)saveProfile(0, v_def);
            }
        }
    }

    // 누락 프로파일 채우기
    for (uint8_t i = 1; i < _index.profileCount; i++) {
        char v_p[64];
        if (!C10_DEF::makeProfilePath(v_p, sizeof(v_p), i)) continue;
        if (!LittleFS.exists(v_p)) {
            ST_C10_ProfileConfig_t v_def;
            makeDefaultsProfile(i, v_def);
            (void)saveProfile(i, v_def);
        }
    }
}

// =======================================================
// Profile Index (active_profile.json)
// =======================================================
bool CL_C10_Config::_loadIndex() {
    memset(&_index, 0, sizeof(_index));

    if (!LittleFS.exists(C10_DEF::PROFILE_ACTIVE)) return false;

    File v_f = LittleFS.open(C10_DEF::PROFILE_ACTIVE, "r");
    if (!v_f) return false;

    JsonDocument v_d;
    DeserializationError v_err = deserializeJson(v_d, v_f);
    v_f.close();
    if (v_err) return false;

    uint8_t v_ai = (uint8_t)v_d["active_index"].as<int>();
    uint8_t v_pc = (uint8_t)v_d["profile_count"].as<int>();

    if (v_pc < 1) v_pc = 1;
    if (v_pc > C10_DEF::PROFILE_MAX) v_pc = C10_DEF::PROFILE_MAX;
    if (v_ai >= v_pc) v_ai = 0;

    _index.activeIndex  = v_ai;
    _index.profileCount = v_pc;
    return true;
}

bool CL_C10_Config::_saveIndex() {
    JsonDocument v_d;
    v_d["active_index"]  = _index.activeIndex;
    v_d["profile_count"] = _index.profileCount;

    if (LittleFS.exists(C10_DEF::PROFILE_ACTIVE_TMP))
        (void)LittleFS.remove(C10_DEF::PROFILE_ACTIVE_TMP);

    File v_f = LittleFS.open(C10_DEF::PROFILE_ACTIVE_TMP, "w");
    if (!v_f) return false;

    size_t v_bytes = serializeJson(v_d, v_f);
    v_f.flush();
    v_f.close();

    if (v_bytes == 0) {
        (void)LittleFS.remove(C10_DEF::PROFILE_ACTIVE_TMP);
        return false;
    }

    if (!_verifyJsonFile(C10_DEF::PROFILE_ACTIVE_TMP)) {
        (void)LittleFS.remove(C10_DEF::PROFILE_ACTIVE_TMP);
        return false;
    }

    if (LittleFS.exists(C10_DEF::PROFILE_ACTIVE))
        (void)LittleFS.remove(C10_DEF::PROFILE_ACTIVE);

    return LittleFS.rename(C10_DEF::PROFILE_ACTIVE_TMP, C10_DEF::PROFILE_ACTIVE);
}

bool CL_C10_Config::setActiveIndex(uint8_t p_idx) {
    if (p_idx >= _index.profileCount) return false;
    _index.activeIndex = p_idx;
    return _saveIndex();
}

// =======================================================
// Profile CRUD
// =======================================================
bool CL_C10_Config::loadProfile(uint8_t p_idx, ST_C10_ProfileConfig_t& p_out) {
    char v_path[64];
    if (!C10_DEF::makeProfilePath(v_path, sizeof(v_path), p_idx)) return false;

    File v_f = LittleFS.open(v_path, "r");
    if (!v_f) return false;

    String v_json = v_f.readString();
    v_f.close();
    if (v_json.length() == 0) return false;

    makeDefaultsProfile(p_idx, p_out);

    // partial patch (JSON에 있는 필드만 반영)
    return patchProfileFromJson(v_json, p_out);
}

bool CL_C10_Config::saveProfile(uint8_t p_idx, const ST_C10_ProfileConfig_t& p_in) {
    if (!validateProfile(p_in)) return false;

    char v_path[64];
    char v_tmp[80];
    if (!C10_DEF::makeProfilePath   (v_path, sizeof(v_path), p_idx)) return false;
    if (!C10_DEF::makeProfileTmpPath(v_tmp,  sizeof(v_tmp),  p_idx)) return false;

    JsonDocument v_doc;
    if (!buildProfileJson(p_in, v_doc)) return false;

    return _atomicWriteJson(v_path, v_tmp, v_doc);
}

bool CL_C10_Config::createProfile(const char* p_newName, uint8_t& p_outIdx) {
    if (_index.profileCount >= C10_DEF::PROFILE_MAX) return false;

    const uint8_t v_newIdx = _index.profileCount;

    ST_C10_ProfileConfig_t v_new;
    makeDefaultsProfile(v_newIdx, v_new);
    if (p_newName && p_newName[0]) {
        strlcpy(v_new.name, p_newName, sizeof(v_new.name));
    }

    if (!saveProfile(v_newIdx, v_new)) return false;

    _index.profileCount = v_newIdx + 1;
    if (!_saveIndex()) {
        // 롤백
        char v_p[64];
        if (C10_DEF::makeProfilePath(v_p, sizeof(v_p), v_newIdx))
            (void)LittleFS.remove(v_p);
        _index.profileCount = v_newIdx;
        return false;
    }

    p_outIdx = v_newIdx;
    return true;
}

bool CL_C10_Config::duplicateProfile(uint8_t p_srcIdx, const char* p_newName, uint8_t& p_outIdx) {
    if (p_srcIdx >= _index.profileCount) return false;
    if (_index.profileCount >= C10_DEF::PROFILE_MAX) return false;

    ST_C10_ProfileConfig_t v_src;
    if (!loadProfile(p_srcIdx, v_src)) return false;

    const uint8_t v_newIdx = _index.profileCount;
    if (p_newName && p_newName[0]) {
        strlcpy(v_src.name, p_newName, sizeof(v_src.name));
    } else {
        // "src copy"
        char v_buf[C10_DEF::PROFILE_NAME_LEN];
        snprintf(v_buf, sizeof(v_buf), "%s copy", v_src.name);
        v_buf[C10_DEF::PROFILE_NAME_LEN - 1] = '\0';
        strlcpy(v_src.name, v_buf, sizeof(v_src.name));
    }

    if (!saveProfile(v_newIdx, v_src)) return false;

    _index.profileCount = v_newIdx + 1;
    if (!_saveIndex()) {
        char v_p[64];
        if (C10_DEF::makeProfilePath(v_p, sizeof(v_p), v_newIdx))
            (void)LittleFS.remove(v_p);
        _index.profileCount = v_newIdx;
        return false;
    }

    p_outIdx = v_newIdx;
    return true;
}

bool CL_C10_Config::deleteProfile(uint8_t p_idx) {
    if (_index.profileCount <= 1) return false;   // 최소 1개 유지
    if (p_idx >= _index.profileCount) return false;

    // 파일 삭제 후 나머지 재배치 (fill gap)
    for (uint8_t i = p_idx; i + 1 < _index.profileCount; i++) {
        char v_src[64], v_dst[64];
        if (!C10_DEF::makeProfilePath(v_src, sizeof(v_src), i + 1)) return false;
        if (!C10_DEF::makeProfilePath(v_dst, sizeof(v_dst), i))     return false;

        if (LittleFS.exists(v_dst)) (void)LittleFS.remove(v_dst);
        if (!LittleFS.rename(v_src, v_dst)) {
            if (!_copyFile(v_src, v_dst)) return false;
            (void)LittleFS.remove(v_src);
        }
    }

    // 마지막 슬롯 삭제
    {
        char v_last[64];
        if (C10_DEF::makeProfilePath(v_last, sizeof(v_last), _index.profileCount - 1))
            (void)LittleFS.remove(v_last);
    }

    _index.profileCount--;
    if (_index.activeIndex >= _index.profileCount) {
        _index.activeIndex = _index.profileCount - 1;
    }

    return _saveIndex();
}

bool CL_C10_Config::renameProfile(uint8_t p_idx, const char* p_newName) {
    if (p_idx >= _index.profileCount) return false;
    if (!p_newName || !p_newName[0]) return false;

    ST_C10_ProfileConfig_t v_p;
    if (!loadProfile(p_idx, v_p)) return false;

    strlcpy(v_p.name, p_newName, sizeof(v_p.name));
    return saveProfile(p_idx, v_p);
}

bool CL_C10_Config::loadActiveProfile(ST_C10_ProfileConfig_t& p_out) {
    return loadProfile(_index.activeIndex, p_out);
}

bool CL_C10_Config::saveActiveProfile(const ST_C10_ProfileConfig_t& p_in) {
    return saveProfile(_index.activeIndex, p_in);
}

// =======================================================
// Defaults
// =======================================================
void CL_C10_Config::makeDefaultsWiFi(ST_C10_WiFiConfig_t& p_out) {
    memset(&p_out, 0, sizeof(p_out));
    p_out.mode = (uint8_t)EN_C10_WIFI_AUTO;
    strlcpy(p_out.ap_ssid,   "EliteAirMouse",  sizeof(p_out.ap_ssid));
    strlcpy(p_out.ap_pass,   "12345678",       sizeof(p_out.ap_pass));
    strlcpy(p_out.mdns_host, "elite-airmouse", sizeof(p_out.mdns_host));
}

void CL_C10_Config::makeDefaultsE10(ST_C10_E10Config_t& p_out) {
    memset(&p_out, 0, sizeof(p_out));

    p_out.dpi_level       = 2;
    p_out.hard_click_lock = true;

    p_out.scale_base[0] = 0.55f; p_out.scale_base[1] = 0.75f; p_out.scale_base[2] = 1.00f;
    p_out.accel_gain[0] = 0.35f; p_out.accel_gain[1] = 0.55f; p_out.accel_gain[2] = 0.85f;
    p_out.accel_threshold = 8.0f;

    p_out.wheel_threshold_deg = 90.0f;
    p_out.wheel_step_max      = 6;

    p_out.gesture_flick_deg   = 200.0f;
    p_out.gesture_cooldown_ms = 600;

    p_out.scroll_cursor_damp = 0.25f;

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

    p_out.led_brightness      = 128;
    p_out.battery_adc_enabled = false;

    p_out.gyro_bias.still_th     = 2.0f;
    p_out.gyro_bias.still_win_ms = 250;
    p_out.gyro_bias.alpha        = 0.001f;

    p_out.linear.th         = 0.3f;
    p_out.linear.impulse_th = 0.5f;
    p_out.linear.window_ms  = 300;

    p_out.flick.p2p_th       = 400.0f;
    p_out.flick.window_ms    = 200;
    p_out.flick.cooldown_ms  = 600;

    p_out.tilt_hold.angle_deg = 15.0f;
    p_out.tilt_hold.hold_ms   = 300;
    p_out.tilt_hold.repeat_hz = 3;

    p_out.sleep_idle_timeout_ms = 60000;
    p_out.active_mode           = 1;
    p_out.active_peer_index     = 0;
}

void CL_C10_Config::makeDefaultsSlots(ST_C10_ProfileSlots_t& p_out) {
    memset(&p_out, 0, sizeof(p_out));

    // ---- Global = Mode 1 (PC Air Mouse) 기본값 ----
    p_out.global[EN_C10_TRIG_TOP_L_CLICK]    = C20_MakeMouseHold(EN_C20_M_L);
    p_out.global[EN_C10_TRIG_TOP_L_DOUBLE]   = C20_MakeMouseClick(EN_C20_M_L);
    p_out.global[EN_C10_TRIG_TOP_L_LONG]     = C20_MakeKbTap(EN_C20_MOD_LALT, EN_C20_KB_TAB);
    p_out.global[EN_C10_TRIG_TOP_M_CLICK]    = C20_MakeMouseClick(EN_C20_M_M);
    p_out.global[EN_C10_TRIG_TOP_M_HOLD]     = C20_MakeNone();   // 잠금: Move Gate
    p_out.global[EN_C10_TRIG_TOP_R_CLICK]    = C20_MakeMouseClick(EN_C20_M_R);
    p_out.global[EN_C10_TRIG_TOP_R_DOUBLE]   = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_ESC);
    p_out.global[EN_C10_TRIG_TOP_R_LONG]     = C20_MakeKbTap(EN_C20_MOD_LGUI | EN_C20_MOD_LSHIFT, EN_C20_KB_S);
    p_out.global[EN_C10_TRIG_SIDE_F_CLICK]   = C20_MakeConsumer(EN_C20_CON_VOL_UP);
    p_out.global[EN_C10_TRIG_SIDE_F_LONG]    = C20_MakeConsumer(EN_C20_CON_NEXT_TRACK);
    p_out.global[EN_C10_TRIG_SIDE_C_CLICK]   = C20_MakeConsumer(EN_C20_CON_PLAY_PAUSE);
    p_out.global[EN_C10_TRIG_SIDE_C_DOUBLE]  = C20_MakeNone();   // 잠금: Mode Cycle
    p_out.global[EN_C10_TRIG_SIDE_C_HOLD_2S] = C20_MakeNone();   // 잠금: Pairing
    p_out.global[EN_C10_TRIG_SIDE_R_CLICK]   = C20_MakeConsumer(EN_C20_CON_VOL_DOWN);
    p_out.global[EN_C10_TRIG_SIDE_R_LONG]    = C20_MakeConsumer(EN_C20_CON_PREV_TRACK);

    // Flick (Mode 1)
    {
        ST_C20_ActionSlot_t v = { EN_C20_ACT_KB_COMBO, EN_C20_HOLD_NONE, 0,
                                  C20_EncodeCombo(EN_C20_MOD_LCTRL | EN_C20_MOD_LGUI, EN_C20_KB_LEFT) };
        p_out.global[EN_C10_TRIG_FLICK_LEFT] = v;
    }
    {
        ST_C20_ActionSlot_t v = { EN_C20_ACT_KB_COMBO, EN_C20_HOLD_NONE, 0,
                                  C20_EncodeCombo(EN_C20_MOD_LCTRL | EN_C20_MOD_LGUI, EN_C20_KB_RIGHT) };
        p_out.global[EN_C10_TRIG_FLICK_RIGHT] = v;
    }
    p_out.global[EN_C10_TRIG_FLICK_UP]   = C20_MakeKbTap(EN_C20_MOD_LGUI, EN_C20_KB_D);
    p_out.global[EN_C10_TRIG_FLICK_DOWN] = C20_MakeKbTap(EN_C20_MOD_LALT, EN_C20_KB_TAB);
    // Linear L/R/U/D + Tilt L/R/U/D = None

    // ---- Mode 1: 완전 상속 (mask=0) ----
    p_out.overrideMask[0] = 0;

    // ---- Mode 2: 완전 오버라이드 (mask = 0x07FFFFFF) ----
    p_out.overrideMask[1] = 0x07FFFFFFu;

    p_out.modes[1][EN_C10_TRIG_TOP_L_CLICK]    = C20_MakeKbTap(EN_C20_MOD_NONE,   EN_C20_KB_PAGEDOWN);
    p_out.modes[1][EN_C10_TRIG_TOP_L_DOUBLE]   = C20_MakeKbTap(EN_C20_MOD_NONE,   EN_C20_KB_PAGEUP);
    p_out.modes[1][EN_C10_TRIG_TOP_L_LONG]     = C20_MakeKbTap(EN_C20_MOD_LSHIFT, EN_C20_KB_F5);
    p_out.modes[1][EN_C10_TRIG_TOP_M_CLICK]    = C20_MakeKbTap(EN_C20_MOD_LCTRL,  EN_C20_KB_L);
    p_out.modes[1][EN_C10_TRIG_TOP_M_HOLD]     = C20_MakeNone();
    p_out.modes[1][EN_C10_TRIG_TOP_R_CLICK]    = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_ESC);
    p_out.modes[1][EN_C10_TRIG_TOP_R_DOUBLE]   = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_B);
    p_out.modes[1][EN_C10_TRIG_TOP_R_LONG]     = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_W);
    p_out.modes[1][EN_C10_TRIG_SIDE_F_CLICK]   = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_PAGEDOWN);
    p_out.modes[1][EN_C10_TRIG_SIDE_F_LONG]    = C20_MakeKbTap(EN_C20_MOD_LSHIFT, EN_C20_KB_F5);
    p_out.modes[1][EN_C10_TRIG_SIDE_C_CLICK]   = C20_MakeKbTap(EN_C20_MOD_LCTRL,  EN_C20_KB_P);
    p_out.modes[1][EN_C10_TRIG_SIDE_C_DOUBLE]  = C20_MakeNone();
    p_out.modes[1][EN_C10_TRIG_SIDE_C_HOLD_2S] = C20_MakeNone();
    p_out.modes[1][EN_C10_TRIG_SIDE_R_CLICK]   = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_PAGEUP);
    p_out.modes[1][EN_C10_TRIG_SIDE_R_LONG]    = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_ESC);
    p_out.modes[1][EN_C10_TRIG_FLICK_LEFT]     = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_PAGEDOWN);
    p_out.modes[1][EN_C10_TRIG_FLICK_RIGHT]    = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_PAGEUP);
    p_out.modes[1][EN_C10_TRIG_FLICK_UP]       = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_B);
    // FLICK_DOWN, LINEAR_*, TILT_* = None

    // ---- Mode 3: 완전 오버라이드 (mask = 0x07FFFFFF) ----
    p_out.overrideMask[2] = 0x07FFFFFFu;

    p_out.modes[2][EN_C10_TRIG_TOP_L_CLICK]    = C20_MakeConsumer(EN_C20_CON_AC_BACK);
    p_out.modes[2][EN_C10_TRIG_TOP_L_DOUBLE]   = C20_MakeConsumer(EN_C20_CON_AC_HOME);
    p_out.modes[2][EN_C10_TRIG_TOP_L_LONG]     = C20_MakeConsumer(EN_C20_CON_POWER);
    p_out.modes[2][EN_C10_TRIG_TOP_M_CLICK]    = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_ENTER);
    p_out.modes[2][EN_C10_TRIG_TOP_M_HOLD]     = C20_MakeNone();
    p_out.modes[2][EN_C10_TRIG_TOP_R_CLICK]    = C20_MakeConsumer(EN_C20_CON_AC_HOME);
    p_out.modes[2][EN_C10_TRIG_TOP_R_DOUBLE]   = C20_MakeConsumer(EN_C20_CON_TV_INPUT);
    p_out.modes[2][EN_C10_TRIG_TOP_R_LONG]     = C20_MakeConsumer(EN_C20_CON_POWER);
    p_out.modes[2][EN_C10_TRIG_SIDE_F_CLICK]   = C20_MakeConsumer(EN_C20_CON_VOL_UP);
    p_out.modes[2][EN_C10_TRIG_SIDE_F_LONG]    = C20_MakeConsumer(EN_C20_CON_CH_UP);
    p_out.modes[2][EN_C10_TRIG_SIDE_C_CLICK]   = C20_MakeConsumer(EN_C20_CON_PLAY_PAUSE);
    p_out.modes[2][EN_C10_TRIG_SIDE_C_DOUBLE]  = C20_MakeNone();
    p_out.modes[2][EN_C10_TRIG_SIDE_C_HOLD_2S] = C20_MakeNone();
    p_out.modes[2][EN_C10_TRIG_SIDE_R_CLICK]   = C20_MakeConsumer(EN_C20_CON_VOL_DOWN);
    p_out.modes[2][EN_C10_TRIG_SIDE_R_LONG]    = C20_MakeConsumer(EN_C20_CON_CH_DOWN);
    p_out.modes[2][EN_C10_TRIG_FLICK_LEFT]     = C20_MakeConsumer(EN_C20_CON_REWIND);
    p_out.modes[2][EN_C10_TRIG_FLICK_RIGHT]    = C20_MakeConsumer(EN_C20_CON_FF);
    p_out.modes[2][EN_C10_TRIG_FLICK_UP]       = C20_MakeConsumer(EN_C20_CON_MUTE);
    // FLICK_DOWN, LINEAR_* = None
    p_out.modes[2][EN_C10_TRIG_TILT_UP]        = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_UP);
    p_out.modes[2][EN_C10_TRIG_TILT_DOWN]      = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_DOWN);
    p_out.modes[2][EN_C10_TRIG_TILT_LEFT]      = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_LEFT);
    p_out.modes[2][EN_C10_TRIG_TILT_RIGHT]     = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_RIGHT);
}

void CL_C10_Config::makeDefaultsMacros(ST_C10_MacroLib_t& p_out) {
    memset(&p_out, 0, sizeof(p_out));
    p_out.count = 0;
}

void CL_C10_Config::makeDefaultsProfile(uint8_t p_idx, ST_C10_ProfileConfig_t& p_out) {
    memset(&p_out, 0, sizeof(p_out));
    p_out.ver = (uint16_t)G_C10_CFG_VER;
    strlcpy(p_out.name, _C10_defaultProfileName(p_idx), sizeof(p_out.name));

    makeDefaultsWiFi  (p_out.wifi);
    makeDefaultsE10   (p_out.e10);
    makeDefaultsSlots (p_out.slots);
    makeDefaultsMacros(p_out.macros);
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

bool CL_C10_Config::validateE10(const ST_C10_E10Config_t& p_e) const {
    if (p_e.dpi_level < 1 || p_e.dpi_level > 3) return false;
    if (p_e.accel_threshold < 0.0f || p_e.accel_threshold > 50.0f) return false;
    if (p_e.wheel_threshold_deg < 1.0f || p_e.wheel_threshold_deg > 360.0f) return false;
    if (p_e.wheel_step_max < 1 || p_e.wheel_step_max > 50) return false;
    if (p_e.gesture_flick_deg < 10.0f || p_e.gesture_flick_deg > 2000.0f) return false;
    if (p_e.gesture_cooldown_ms > 20000) return false;
    if (p_e.scroll_cursor_damp < 0.0f || p_e.scroll_cursor_damp > 1.0f) return false;

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

    return true;
}

bool CL_C10_Config::validateSlot(const ST_C20_ActionSlot_t& p_s, uint8_t p_macroCount) const {
    if (p_s.kind >= (uint8_t)EN_C20_ACT_MAX)         return false;
    if (p_s.holdMode > (uint8_t)EN_C20_HOLD_REPEAT)  return false;

    switch ((EN_C20_ActionKind_t)p_s.kind) {
        case EN_C20_ACT_NONE:            return true;
        case EN_C20_ACT_MOUSE_CLICK:
        case EN_C20_ACT_MOUSE_HOLD:      return (p_s.param16 & 0xFF) != 0;
        case EN_C20_ACT_MOUSE_WHEEL:     return C20_WheelAxis(p_s.param16) <= 1 &&
                                                C20_WheelDir (p_s.param16) <= 1;
        case EN_C20_ACT_KB_TAP:
        case EN_C20_ACT_KB_REPEAT:       return (uint8_t)p_s.param16 <= 0xE7;
        case EN_C20_ACT_KB_COMBO:        return true;
        case EN_C20_ACT_CONSUMER_TAP:
        case EN_C20_ACT_CONSUMER_REPEAT: return p_s.param32 != 0;
        case EN_C20_ACT_SPECIAL:         return (uint8_t)p_s.param16 < (uint8_t)EN_C20_SP_MAX;
        case EN_C20_ACT_MACRO:           return p_s.param32 < (uint32_t)p_macroCount;
        default:                         return false;
    }
}

bool CL_C10_Config::validateSlots(const ST_C10_ProfileSlots_t& p_s, uint8_t p_macroCount) const {
    for (uint8_t i = 0; i < EN_C10_TRIG_MAX; i++) {
        if (!validateSlot(p_s.global[i], p_macroCount)) return false;
    }
    for (uint8_t m = 0; m < C10_DEF::MODE_COUNT; m++) {
        for (uint8_t i = 0; i < EN_C10_TRIG_MAX; i++) {
            if (!validateSlot(p_s.modes[m][i], p_macroCount)) return false;
        }
    }
    return true;
}

bool CL_C10_Config::validateMacroStep(const ST_C10_MacroStep_t& p_s) const {
    // MACRO 중첩 금지
    if (p_s.kind == (uint8_t)EN_C20_ACT_MACRO) return false;
    if (p_s.kind >= (uint8_t)EN_C20_ACT_MAX)   return false;
    if (p_s.holdMode > (uint8_t)EN_C20_HOLD_REPEAT) return false;
    if (p_s.delayMs > C10_DEF::MACRO_DELAY_MAX_MS) return false;

    // primitive 검증
    switch ((EN_C20_ActionKind_t)p_s.kind) {
        case EN_C20_ACT_NONE:            return true;
        case EN_C20_ACT_MOUSE_CLICK:
        case EN_C20_ACT_MOUSE_HOLD:      return (p_s.param16 & 0xFF) != 0;
        case EN_C20_ACT_MOUSE_WHEEL:     return C20_WheelAxis(p_s.param16) <= 1 &&
                                                C20_WheelDir (p_s.param16) <= 1;
        case EN_C20_ACT_KB_TAP:
        case EN_C20_ACT_KB_REPEAT:       return (uint8_t)p_s.param16 <= 0xE7;
        case EN_C20_ACT_KB_COMBO:        return true;
        case EN_C20_ACT_CONSUMER_TAP:
        case EN_C20_ACT_CONSUMER_REPEAT: return p_s.param32 != 0;
        case EN_C20_ACT_SPECIAL:         return (uint8_t)p_s.param16 < (uint8_t)EN_C20_SP_MAX;
        default:                         return false;
    }
}

bool CL_C10_Config::validateMacro(const ST_C10_Macro_t& p_m) const {
    if (p_m.stepCount < 1 || p_m.stepCount > C10_DEF::MACRO_STEP_MAX) return false;
    for (uint8_t i = 0; i < p_m.stepCount; i++) {
        if (!validateMacroStep(p_m.steps[i])) return false;
    }
    return true;
}

bool CL_C10_Config::validateMacros(const ST_C10_MacroLib_t& p_m) const {
    if (p_m.count > C10_DEF::MACRO_MAX) return false;
    for (uint8_t i = 0; i < p_m.count; i++) {
        if (!validateMacro(p_m.macros[i])) return false;
    }
    return true;
}

bool CL_C10_Config::validateProfile(const ST_C10_ProfileConfig_t& p_p) const {
    if (!validateWiFi(p_p.wifi)) return false;
    if (!validateE10 (p_p.e10))  return false;
    if (!validateMacros(p_p.macros)) return false;
    if (!validateSlots (p_p.slots, p_p.macros.count)) return false;
    return true;
}

// =======================================================
// SafeBoot
// =======================================================
bool CL_C10_Config::clearSafeMode() {
    _boot.safe_mode  = false;
    _boot.fail_count = 0;
    _boot.pending    = false;
    return _saveBootState(_boot);
}

bool CL_C10_Config::bootMarkOkIfGracePassed(uint32_t p_graceMs) {
    if (_boot.boot_ms == 0) _boot.boot_ms = (uint32_t)millis();

    const uint32_t v_now = (uint32_t)millis();
    const uint32_t v_alive = (v_now >= _boot.boot_ms) ? (v_now - _boot.boot_ms) : 0;

    if (v_alive < p_graceMs) return false;

    if (_bootOkRetryAtMs != 0 && (int32_t)(v_now - _bootOkRetryAtMs) < 0) return false;

    _boot.pending = false;
    if (!_boot.safe_mode) _boot.fail_count = 0;

    const bool v_ok = _saveBootState(_boot);
    if (v_ok) _bootOkRetryAtMs = 0;
    else      _bootOkRetryAtMs = v_now + 30000u;
    return v_ok;
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

    JsonDocument v_d;
    DeserializationError v_err = deserializeJson(v_d, v_f);
    v_f.close();
    if (v_err) return false;

    if (!v_d["safe_mode"].isNull())         p_out.safe_mode  = (bool)v_d["safe_mode"];
    if (!v_d["fail_count"].isNull())        p_out.fail_count = (uint8_t)v_d["fail_count"];
    if (!v_d["pending"].isNull())           p_out.pending    = (bool)v_d["pending"];
    if (!v_d["boot_ms"].isNull())           p_out.boot_ms    = (uint32_t)v_d["boot_ms"];
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

    File v_f = LittleFS.open(C10_DEF::BOOT_TMP_PATH, "w");
    if (!v_f) return false;

    if (serializeJson(v_d, v_f) == 0) {
        v_f.close();
        (void)LittleFS.remove(C10_DEF::BOOT_TMP_PATH);
        return false;
    }
    v_f.flush();
    v_f.close();

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
    bool v_ok = !deserializeJson(v_d, v_f);
    v_f.close();
    return v_ok;
}

bool CL_C10_Config::_copyFile(const char* p_src, const char* p_dst) {
    File v_s = LittleFS.open(p_src, "r");
    if (!v_s) return false;

    File v_d = LittleFS.open(p_dst, "w");
    if (!v_d) { v_s.close(); return false; }

    uint8_t v_buf[256];
    while (true) {
        int v_n = v_s.read(v_buf, sizeof(v_buf));
        if (v_n <= 0) break;
        if (v_d.write(v_buf, (size_t)v_n) != (size_t)v_n) {
            v_s.close(); v_d.close();
            return false;
        }
    }

    v_d.flush();
    v_s.close();
    v_d.close();
    return true;
}

bool CL_C10_Config::_atomicWriteJson(const char* p_path, const char* p_tmpPath, JsonDocument& p_doc) {
    // tmp cleanup
    if (LittleFS.exists(p_tmpPath)) (void)LittleFS.remove(p_tmpPath);

    // 1) tmp write
    File v_f = LittleFS.open(p_tmpPath, "w");
    if (!v_f) return false;

    size_t v_bytes = serializeJson(p_doc, v_f);
    v_f.flush();
    v_f.close();

    if (v_bytes == 0) {
        (void)LittleFS.remove(p_tmpPath);
        return false;
    }

    // 2) verify
    if (!_verifyJsonFile(p_tmpPath)) {
        (void)LittleFS.remove(p_tmpPath);
        return false;
    }

    // 3) rotate: main → main.old
    char v_old[96];
    snprintf(v_old, sizeof(v_old), "%s.old", p_path);

    if (LittleFS.exists(v_old)) (void)LittleFS.remove(v_old);
    if (LittleFS.exists(p_path)) {
        if (!LittleFS.rename(p_path, v_old)) {
            if (!_copyFile(p_path, v_old)) {
                (void)LittleFS.remove(p_tmpPath);
                return false;
            }
            (void)LittleFS.remove(p_path);
        }
    }

    // 4) commit: tmp → main
    if (!LittleFS.rename(p_tmpPath, p_path)) {
        if (!_copyFile(p_tmpPath, p_path)) {
            (void)LittleFS.remove(p_tmpPath);
            if (LittleFS.exists(v_old)) (void)LittleFS.rename(v_old, p_path);
            return false;
        }
        (void)LittleFS.remove(p_tmpPath);
    }

    // 5) verify final
    if (!_verifyJsonFile(p_path)) {
        (void)LittleFS.remove(p_path);
        if (LittleFS.exists(v_old)) (void)LittleFS.rename(v_old, p_path);
        return false;
    }

    // 6) cleanup old
    if (LittleFS.exists(v_old)) (void)LittleFS.remove(v_old);
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

void CL_C10_Config::_buildE10Json(JsonObject p_parent, const ST_C10_E10Config_t& p_in) {
    p_parent["dpi_level"]       = p_in.dpi_level;
    p_parent["hard_click_lock"] = p_in.hard_click_lock;

    JsonArray v_sb = p_parent["scale_base"].to<JsonArray>();
    v_sb.add(p_in.scale_base[0]); v_sb.add(p_in.scale_base[1]); v_sb.add(p_in.scale_base[2]);

    JsonArray v_ag = p_parent["accel_gain"].to<JsonArray>();
    v_ag.add(p_in.accel_gain[0]); v_ag.add(p_in.accel_gain[1]); v_ag.add(p_in.accel_gain[2]);

    p_parent["accel_threshold"]   = p_in.accel_threshold;
    p_parent["scroll_cursor_damp"] = p_in.scroll_cursor_damp;

    JsonObject v_w = p_parent["wheel"].to<JsonObject>();
    v_w["threshold_deg"] = p_in.wheel_threshold_deg;
    v_w["step_max"]      = p_in.wheel_step_max;

    JsonObject v_g = p_parent["gesture"].to<JsonObject>();
    v_g["flick_deg"]   = p_in.gesture_flick_deg;
    v_g["cooldown_ms"] = p_in.gesture_cooldown_ms;

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
}

void CL_C10_Config::_buildSlotsJson(JsonObject p_parent, const ST_C10_ProfileSlots_t& p_in) {
    // global[]
    JsonArray v_glob = p_parent["global"].to<JsonArray>();
    _buildSlotArrayJson(v_glob, p_in.global, EN_C10_TRIG_MAX);

    // modes[] (dense)
    JsonArray v_modes = p_parent["modes"].to<JsonArray>();
    for (uint8_t m = 0; m < C10_DEF::MODE_COUNT; m++) {
        JsonObject v_m = v_modes.add<JsonObject>();
        v_m["mask"] = (uint32_t)p_in.overrideMask[m];
        JsonArray v_arr = v_m["slots"].to<JsonArray>();
        _buildSlotArrayJson(v_arr, p_in.modes[m], EN_C10_TRIG_MAX);
    }
}

void CL_C10_Config::_buildMacroStepJson(JsonObject p_obj, const ST_C10_MacroStep_t& p_in) {
    p_obj["k"] = p_in.kind;
    p_obj["h"] = p_in.holdMode;
    p_obj["d"] = p_in.delayMs;
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

    if (!p_v["accel_threshold"].isNull())   p_io.accel_threshold = (float)p_v["accel_threshold"];
    if (!p_v["scroll_cursor_damp"].isNull()) p_io.scroll_cursor_damp = (float)p_v["scroll_cursor_damp"];

    JsonVariantConst v_w = p_v["wheel"];
    if (!v_w.isNull()) {
        if (!v_w["threshold_deg"].isNull()) p_io.wheel_threshold_deg = (float)v_w["threshold_deg"];
        if (!v_w["step_max"].isNull())      p_io.wheel_step_max = (uint8_t)v_w["step_max"];
    }

    JsonVariantConst v_g = p_v["gesture"];
    if (!v_g.isNull()) {
        if (!v_g["flick_deg"].isNull())   p_io.gesture_flick_deg = (float)v_g["flick_deg"];
        if (!v_g["cooldown_ms"].isNull()) p_io.gesture_cooldown_ms = (uint16_t)v_g["cooldown_ms"];
    }

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

    return true;
}

bool CL_C10_Config::_patchSlotsJson(JsonVariantConst p_v, ST_C10_ProfileSlots_t& p_io) {
    if (p_v.isNull()) return false;

    // global[]
    (void)_patchSlotArrayJson(p_v["global"], p_io.global, EN_C10_TRIG_MAX);

    // modes[] (dense)
    JsonVariantConst v_modes = p_v["modes"];
    if (!v_modes.is<JsonArrayConst>()) return true;

    JsonArrayConst arr = v_modes.as<JsonArrayConst>();
    const uint8_t n = (arr.size() < C10_DEF::MODE_COUNT) ? (uint8_t)arr.size() : C10_DEF::MODE_COUNT;

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
        // MACRO(10) 중첩 및 SPECIAL(9) 금지 (R1/R7)
        if (v_k != (uint8_t)EN_C20_ACT_MACRO && v_k != (uint8_t)EN_C20_ACT_SPECIAL && v_k < (uint8_t)EN_C20_ACT_MAX) {
            p_io.kind = v_k;
        }
    }
    // [v0410] 매크로 step은 holdMode == NONE(0)만 허용
    p_io.holdMode = (uint8_t)EN_C20_HOLD_NONE;
    if (!p_v["d"].isNull()) {
        uint16_t v_d = (uint16_t)p_v["d"];
        if (v_d > C10_DEF::MACRO_DELAY_MAX_MS) v_d = C10_DEF::MACRO_DELAY_MAX_MS;
        p_io.delayMs = v_d;
    }
    if (!p_v["p16"].isNull()) p_io.param16 = (uint16_t)p_v["p16"];
    if (!p_v["p32"].isNull()) p_io.param32 = (uint32_t)p_v["p32"];
    return true;
}

bool CL_C10_Config::_patchMacroJson(JsonVariantConst p_v, ST_C10_Macro_t& p_io) {
    if (p_v.isNull()) return false;

    if (!p_v["name"].isNull()) strlcpy(p_io.name, (const char*)p_v["name"], sizeof(p_io.name));

    JsonVariantConst v_steps = p_v["steps"];
    if (v_steps.is<JsonArrayConst>()) {
        JsonArrayConst arr = v_steps.as<JsonArrayConst>();
        const uint8_t n = (arr.size() < C10_DEF::MACRO_STEP_MAX) ? (uint8_t)arr.size() : C10_DEF::MACRO_STEP_MAX;
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

    const uint8_t n = (arr.size() < C10_DEF::MACRO_MAX) ? (uint8_t)arr.size() : C10_DEF::MACRO_MAX;
    for (uint8_t i = 0; i < n; i++) {
        (void)_patchMacroJson(arr[i], p_io.macros[i]);
    }
    p_io.count = n;
    return true;
}

bool CL_C10_Config::patchProfileFromJson(const String& p_json, ST_C10_ProfileConfig_t& p_io) {
    JsonDocument v_doc;
    if (deserializeJson(v_doc, p_json)) return false;

    if (!v_doc["name"].isNull()) strlcpy(p_io.name, (const char*)v_doc["name"], sizeof(p_io.name));

    (void)_patchWiFiJson  (v_doc["wifi"],   p_io.wifi);
    (void)_patchE10Json   (v_doc["e10"],    p_io.e10);
    (void)_patchSlotsJson (v_doc["slots"],  p_io.slots);
    (void)_patchMacrosJson(v_doc["macros"], p_io.macros);

    return true;
}

// =======================================================
// Factory Reset
// =======================================================
bool CL_C10_Config::factoryReset(bool p_recreateDefault) {
    // 모든 프로파일 삭제
    for (uint8_t i = 0; i < C10_DEF::PROFILE_MAX; i++) {
        char v_p[64];
        if (!C10_DEF::makeProfilePath(v_p, sizeof(v_p), i)) continue;
        if (LittleFS.exists(v_p)) (void)LittleFS.remove(v_p);

        char v_old[96];
        snprintf(v_old, sizeof(v_old), "%s.old", v_p);
        if (LittleFS.exists(v_old)) (void)LittleFS.remove(v_old);

        char v_tmp[80];
        if (!C10_DEF::makeProfileTmpPath(v_tmp, sizeof(v_tmp), i)) continue;
        if (LittleFS.exists(v_tmp)) (void)LittleFS.remove(v_tmp);
    }

    // index / boot state 삭제
    if (LittleFS.exists(C10_DEF::PROFILE_ACTIVE))     (void)LittleFS.remove(C10_DEF::PROFILE_ACTIVE);
    if (LittleFS.exists(C10_DEF::PROFILE_ACTIVE_TMP)) (void)LittleFS.remove(C10_DEF::PROFILE_ACTIVE_TMP);

    if (LittleFS.exists(C10_DEF::BOOT_PATH))     (void)LittleFS.remove(C10_DEF::BOOT_PATH);
    if (LittleFS.exists(C10_DEF::BOOT_TMP_PATH)) (void)LittleFS.remove(C10_DEF::BOOT_TMP_PATH);

    memset(&_boot, 0, sizeof(_boot));
    memset(&_index, 0, sizeof(_index));
    _boot.pending = false;
    (void)_saveBootState(_boot);

    if (p_recreateDefault) {
        _index.activeIndex  = 0;
        _index.profileCount = 1;
        (void)_saveIndex();

        ST_C10_ProfileConfig_t v_def;
        makeDefaultsProfile(0, v_def);
        return saveProfile(0, v_def);
    }
    return true;
}
