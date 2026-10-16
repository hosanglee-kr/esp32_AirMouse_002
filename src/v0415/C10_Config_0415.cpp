// File: src/v0415/C10_Config_0415.cpp
// =======================================================
/*
 * ------------------------------------------------------
 * 소스명 : C10_Config_0415.cpp
 * 모듈약어 : C10
 * 모듈명 : Config Manager (v0415) - Lifecycle / CRUD / File IO / Hosts
 * ------------------------------------------------------
 * 기능 요약
 *  - C10 클래스 lifecycle (ctor / begin)
 *  - 다중 프로파일 CRUD (load / save / create / delete / rename)
 *  - SafeBoot 상태 저장/로드 (원자적 rotate save)
 *  - LittleFS 파일 IO (verify / copy / atomic write)
 *  - Multi-Host 저장소 (hosts_0415.json, 3-tier 원자적 회전)
 *  - Factory Reset (프로파일 + 호스트 + 부팅 상태 일괄 초기화)
 *
 * [v0415 rev18.1 - Phase 5-A 분할]
 *  - 본 .cpp는 3-TU로 분할됨
 *    · C10_Config_0415.cpp           : 본 파일 (Lifecycle / CRUD / File IO / Hosts)
 *    · C10_Config_Json_0415.cpp      : _build* / _patch* / JSON 직렬화 (A-3)
 *    · C10_Config_Validate_0415.cpp  : validate* + G_E10_RULES[] (A-4)
 *  - legacy 410→411 마이그레이션 함수 삭제 (불필요)
 *    · _migrateProfileV410ToV411 삭제
 *    · _migrateSlotConsumerV410ToV415 삭제 (static helper)
 *    · _migrateConsumerMaskV410ToV415 삭제 (static helper)
 *    · loadProfile은 patchProfileFromJson + validateProfile만 사용
 *    · ver=410 파일은 patch 미적용 필드가 기본값으로 fallback됨
 *
 * [v0415 rev15 이전]
 *  - 스키마 412 전환 (gesture.* 삭제, flick.* SSOT)
 *  - deleteProfile: swap-with-last (원자성 강화)
 *  - _atomicWriteJson mkdir 캐시 (_dirsEnsured)
 *  - C10 rotate save (_saveIndex / _saveBootState)
 *  - _bootMutex 스레드 안전성 확보
 *
 * [구현 규칙]
 *  - ArduinoJson v7.x.x 사용
 *  - JsonDocument 단일 타입만 사용
 *  - createNestedArray/Object/containsKey 사용 금지
 *  - memset + strlcpy 기반 안전 초기화
 *  - 주석/필드명은 JSON 구조와 동일하게 유지
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
    _bootMutex = xSemaphoreCreateMutex();
}

void CL_C10_Config::begin(bool p_formatOnFail) {
    (void)LittleFS.begin(p_formatOnFail);

    (void)LittleFS.mkdir("/json");
    (void)LittleFS.mkdir(C10_DEF::PROFILES_DIR);
    _dirsEnsured = true;

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
// Profile Index
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

    uint8_t v_ai = 0;
    if (!v_d["active_index"].isNull()) {
        v_ai = (uint8_t)v_d["active_index"].as<int>();
    } else if (!v_d["active"].isNull()) {
        v_ai = (uint8_t)v_d["active"].as<int>();
    }

    uint8_t v_pc = 0;
    if (!v_d["profile_count"].isNull()) {
        v_pc = (uint8_t)v_d["profile_count"].as<int>();
    } else if (!v_d["count"].isNull()) {
        v_pc = (uint8_t)v_d["count"].as<int>();
    } else if (v_d["profiles"].is<JsonArrayConst>()) {
        v_pc = (uint8_t)v_d["profiles"].as<JsonArrayConst>().size();
    }

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
    v_d["active"]        = _index.activeIndex;
    v_d["count"]         = _index.profileCount;

    const char* v_tmp  = C10_DEF::PROFILE_ACTIVE_TMP;
    const char* v_old  = C10_DEF::PROFILE_ACTIVE_OLD;
    const char* v_main = C10_DEF::PROFILE_ACTIVE;

    if (LittleFS.exists(v_tmp)) (void)LittleFS.remove(v_tmp);

    File v_f = LittleFS.open(v_tmp, "w");
    if (!v_f) return false;

    size_t v_bytes = serializeJson(v_d, v_f);
    v_f.flush();
    v_f.close();

    if (v_bytes == 0 || !_verifyJsonFile(v_tmp)) {
        (void)LittleFS.remove(v_tmp);
        return false;
    }

    // [P1] rotate: main → old (main을 먼저 삭제하지 않음)
    if (LittleFS.exists(v_old)) (void)LittleFS.remove(v_old);
    if (LittleFS.exists(v_main)) {
        if (!LittleFS.rename(v_main, v_old)) {
            (void)LittleFS.remove(v_tmp);
            return false;
        }
    }

    // commit: tmp → main
    if (!LittleFS.rename(v_tmp, v_main)) {
        if (LittleFS.exists(v_old)) (void)LittleFS.rename(v_old, v_main);
        (void)LittleFS.remove(v_tmp);
        return false;
    }

    if (LittleFS.exists(v_old)) (void)LittleFS.remove(v_old);
    return true;
}

bool CL_C10_Config::setActiveIndex(uint8_t p_idx) {
    if (p_idx >= _index.profileCount) return false;
    _index.activeIndex = p_idx;
    return _saveIndex();
}


// =======================================================
// Profile CRUD
// =======================================================
// [v0415 rev18.1] loadProfile — 마이그레이션 제거
//   - 기존 legacy 410→411 마이그레이션 함수 삭제
//   - patchProfileFromJson + validateProfile만으로 처리
//   - ver=410 파일의 미지원 필드는 기본값으로 자연 fallback
// =======================================================
bool CL_C10_Config::loadProfile(uint8_t p_idx, ST_C10_ProfileConfig_t& p_out) {
    if (p_idx >= C10_DEF::PROFILE_MAX) return false;
    char v_path[64];
    if (!C10_DEF::makeProfilePath(v_path, sizeof(v_path), p_idx)) return false;

    File v_f = LittleFS.open(v_path, "r");
    if (!v_f) return false;

    String v_json = v_f.readString();
    v_f.close();
    if (v_json.length() == 0) return false;

    // 1) defaults 초기화 (기본값 baseline)
    makeDefaultsProfile(p_idx, p_out);

    // 2) sentinel: JSON의 ver 필드로 overwrite 예정
    p_out.ver = 0;

    // 3) partial patch (ver 포함)
    if (!patchProfileFromJson(v_json, p_out)) {
        D10_LOGW_C("C10::loadProfile", "patch failed: idx=%u", (unsigned)p_idx);
        makeDefaultsProfile(p_idx, p_out);
        return false;
    }

    // 4) validate — 손상 프로파일 방어
    if (!validateProfile(p_out)) {
        D10_LOGW_C("C10::loadProfile",
                   "validate failed: idx=%u → using defaults",
                   (unsigned)p_idx);
        makeDefaultsProfile(p_idx, p_out);
        return false;
    }

    return true;
}

bool CL_C10_Config::saveProfile(uint8_t p_idx, const ST_C10_ProfileConfig_t& p_in) {
    if (p_idx >= C10_DEF::PROFILE_MAX) return false;
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

// =======================================================
// deleteProfile — swap-with-last (원자성 강화)
// -------------------------------------------------------
//   - 삭제 대상이 마지막이면: 파일만 삭제
//   - 아니면: 마지막 프로파일을 삭제 슬롯으로 rename (swap)
//
//   특징:
//     - 파일 rename 1회 (원자적)
//     - 실패 시 rename 실패 → 이전 상태 유지
//     - copy fallback 시에도 원본은 유지되며 완료 후 remove
// =======================================================
bool CL_C10_Config::deleteProfile(uint8_t p_idx) {
    if (_index.profileCount <= 1) return false;
    if (p_idx >= _index.profileCount) return false;

    const uint8_t v_last = (uint8_t)(_index.profileCount - 1);

    // ---- Case 1: 삭제 대상이 마지막 ----
    if (p_idx == v_last) {
        char v_path[64];
        if (C10_DEF::makeProfilePath(v_path, sizeof(v_path), p_idx)) {
            (void)LittleFS.remove(v_path);

            char v_old[96];
            snprintf(v_old, sizeof(v_old), "%s.old", v_path);
            if (LittleFS.exists(v_old)) (void)LittleFS.remove(v_old);

            char v_tmp[80];
            if (C10_DEF::makeProfileTmpPath(v_tmp, sizeof(v_tmp), p_idx)) {
                if (LittleFS.exists(v_tmp)) (void)LittleFS.remove(v_tmp);
            }
        }
    }
    // ---- Case 2: 중간 삭제 → 마지막을 삭제 슬롯으로 이동 ----
    else {
        char v_src[64];  // 마지막 프로파일
        char v_dst[64];  // 삭제 대상 슬롯
        if (!C10_DEF::makeProfilePath(v_src, sizeof(v_src), v_last)) return false;
        if (!C10_DEF::makeProfilePath(v_dst, sizeof(v_dst), p_idx))  return false;

        // [P3] 삭제 대상 슬롯의 main + old + tmp 잔재 전부 정리
        if (LittleFS.exists(v_dst)) (void)LittleFS.remove(v_dst);

        char v_dstOld[96];
        snprintf(v_dstOld, sizeof(v_dstOld), "%s.old", v_dst);
        if (LittleFS.exists(v_dstOld)) (void)LittleFS.remove(v_dstOld);

        char v_dstTmp[80];
        if (C10_DEF::makeProfileTmpPath(v_dstTmp, sizeof(v_dstTmp), p_idx)) {
            if (LittleFS.exists(v_dstTmp)) (void)LittleFS.remove(v_dstTmp);
        }

        // 마지막 → 삭제 슬롯 이동
        if (!LittleFS.rename(v_src, v_dst)) {
            if (!_copyFile(v_src, v_dst)) {
                D10_LOGE_C("C10::deleteProfile",
                           "swap failed: %s → %s",
                           v_src, v_dst);
                return false;
            }
            (void)LittleFS.remove(v_src);
        }

        // 마지막 슬롯의 잔재 정리
        char v_lastOld[96];
        snprintf(v_lastOld, sizeof(v_lastOld), "%s.old", v_src);
        if (LittleFS.exists(v_lastOld)) (void)LittleFS.remove(v_lastOld);

        char v_lastTmp[80];
        if (C10_DEF::makeProfileTmpPath(v_lastTmp, sizeof(v_lastTmp), v_last)) {
            if (LittleFS.exists(v_lastTmp)) (void)LittleFS.remove(v_lastTmp);
        }

        D10_LOGI_C("C10::deleteProfile",
                   "swap-with-last: idx=%u ← last=%u",
                   (unsigned)p_idx, (unsigned)v_last);
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

    p_out.sta_ssid[0] = '\0';
    p_out.sta_pass[0] = '\0';

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

    // Motion Advanced
    p_out.motion_adv.click_freeze.enable         = true;
    p_out.motion_adv.click_freeze.gyro_th        = 15.0f;
    p_out.motion_adv.click_freeze.max_ms         = 150;
    p_out.motion_adv.click_freeze.hold_ms        = 20;
    p_out.motion_adv.click_freeze.fadeout_ms     = 30;
    p_out.motion_adv.click_freeze.move_th        = 2.0f;
    p_out.motion_adv.click_freeze.freeze_move_th = 30.0f;
    memset(p_out.motion_adv.click_freeze._pad, 0, sizeof(p_out.motion_adv.click_freeze._pad));

    p_out.motion_adv.ema.alpha_min      = 0.05f;
    p_out.motion_adv.ema.alpha_max      = 0.80f;
    p_out.motion_adv.ema.deadzone_th    = 3.0f;
    p_out.motion_adv.ema.fast_th        = 15.0f;
    p_out.motion_adv.ema.reversal_th    = 8.0f;
    p_out.motion_adv.ema.reversal_reset = true;
    memset(p_out.motion_adv.ema._pad, 0, sizeof(p_out.motion_adv.ema._pad));

    p_out.motion_adv.snap.enable         = true;
    p_out.motion_adv.snap.mode_mask      = 0x02;
    p_out.motion_adv.snap.axis_mode      = 0;
    p_out.motion_adv.snap.confirm_frames = 3;
    p_out.motion_adv.snap.ratio_enter    = 4.0f;
    p_out.motion_adv.snap.strength       = 0.85f;
    memset(p_out.motion_adv.snap._pad, 0, sizeof(p_out.motion_adv.snap._pad));

    // Power Management
    p_out.power.idle_timeout_ms[0]      = 60000;
    p_out.power.idle_timeout_ms[1]      = 120000;
    p_out.power.idle_timeout_ms[2]      = 300000;
    p_out.power.idle_timeout_ble_ms     = 300000;
    p_out.power.pairing_idle_timeout_ms = 30000;
    p_out.power.deep_idle_timeout_ms    = 600000;
    p_out.power.wake_min_active_ms      = 500;
    p_out.power.wom_threshold           = 25;
    p_out.power.wom_duration            = 4;
    p_out.power.fast_recalib_ms         = 300;
    p_out.power.led_fadeout_ms          = 500;
    p_out.power.led_fadein_ms           = 300;
    memset(p_out.power._pad, 0, sizeof(p_out.power._pad));

    // Button Timing
    p_out.button.debounce_press_ms   = 32;
    p_out.button.debounce_release_ms = 16;
    p_out.button.long_delay_ms       = 800;
    p_out.button.double_delay_ms     = 320;
    p_out.button.hold_2s_ms          = 2000;
    p_out.button.hold_3s_ms          = 3000;
    p_out.button.min_click_ms        = 16;
    p_out.button.debounce_min_ticks  = 3;
    memset(p_out.button._pad, 0, sizeof(p_out.button._pad));
}

void CL_C10_Config::makeDefaultsSlots(ST_C10_ProfileSlots_t& p_out) {
    memset(&p_out, 0, sizeof(p_out));

    // ---- Global = Mode 1 (PC Air Mouse) 기본값 ----
    p_out.global[EN_C10_TRIG_TOP_L_CLICK]    = C20_MakeMouseHold(EN_C20_M_L);
    p_out.global[EN_C10_TRIG_TOP_L_DOUBLE]   = C20_MakeMouseClick(EN_C20_M_L);
    p_out.global[EN_C10_TRIG_TOP_L_LONG]     = C20_MakeKbTap(EN_C20_MOD_LALT, EN_C20_KB_TAB);
    p_out.global[EN_C10_TRIG_TOP_M_CLICK]    = C20_MakeMouseClick(EN_C20_M_M);
    p_out.global[EN_C10_TRIG_TOP_M_HOLD]     = C20_MakeNone();
    p_out.global[EN_C10_TRIG_TOP_R_CLICK]    = C20_MakeMouseClick(EN_C20_M_R);
    p_out.global[EN_C10_TRIG_TOP_R_DOUBLE]   = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_ESC);
    p_out.global[EN_C10_TRIG_TOP_R_LONG]     = C20_MakeKbTap(EN_C20_MOD_LGUI | EN_C20_MOD_LSHIFT, EN_C20_KB_S);
    p_out.global[EN_C10_TRIG_SIDE_F_CLICK]   = C20_MakeConsumer(EN_C20_CON_VOL_UP);
    p_out.global[EN_C10_TRIG_SIDE_F_LONG]    = C20_MakeConsumer(EN_C20_CON_NEXT_TRACK);
    p_out.global[EN_C10_TRIG_SIDE_C_CLICK]   = C20_MakeConsumer(EN_C20_CON_PLAY_PAUSE);
    p_out.global[EN_C10_TRIG_SIDE_C_DOUBLE]  = C20_MakeNone();
    p_out.global[EN_C10_TRIG_SIDE_C_HOLD_2S] = C20_MakeNone();
    p_out.global[EN_C10_TRIG_SIDE_R_CLICK]   = C20_MakeConsumer(EN_C20_CON_VOL_DOWN);
    p_out.global[EN_C10_TRIG_SIDE_R_LONG]    = C20_MakeConsumer(EN_C20_CON_PREV_TRACK);

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

    // ---- Mode 1: 완전 상속 (mask=0) ----
    p_out.overrideMask[0] = 0;

    // ---- Mode 2: 완전 오버라이드 (mask = FULL) ----
    p_out.overrideMask[1] = G_C10_FULL_OVERRIDE_MASK;

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

    // ---- Mode 3: 완전 오버라이드 (mask = FULL) ----
    p_out.overrideMask[2] = G_C10_FULL_OVERRIDE_MASK;

    p_out.modes[2][EN_C10_TRIG_TOP_L_CLICK]    = C20_MakeConsumer(EN_C20_CON_WWW_BACK);
    p_out.modes[2][EN_C10_TRIG_TOP_L_DOUBLE]   = C20_MakeConsumer(EN_C20_CON_WWW_HOME);
    p_out.modes[2][EN_C10_TRIG_TOP_L_LONG]     = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_POWER);

    p_out.modes[2][EN_C10_TRIG_TOP_M_CLICK]    = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_ENTER);
    p_out.modes[2][EN_C10_TRIG_TOP_M_HOLD]     = C20_MakeNone();

    p_out.modes[2][EN_C10_TRIG_TOP_R_CLICK]    = C20_MakeConsumer(EN_C20_CON_WWW_HOME);
    p_out.modes[2][EN_C10_TRIG_TOP_R_DOUBLE]   = C20_MakeConsumer(EN_C20_CON_WWW_SEARCH);
    p_out.modes[2][EN_C10_TRIG_TOP_R_LONG]     = C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_POWER);

    p_out.modes[2][EN_C10_TRIG_SIDE_F_CLICK]   = C20_MakeConsumer(EN_C20_CON_VOL_UP);
    p_out.modes[2][EN_C10_TRIG_SIDE_F_LONG]    = C20_MakeNone();

    p_out.modes[2][EN_C10_TRIG_SIDE_C_CLICK]   = C20_MakeConsumer(EN_C20_CON_PLAY_PAUSE);
    p_out.modes[2][EN_C10_TRIG_SIDE_C_DOUBLE]  = C20_MakeNone();
    p_out.modes[2][EN_C10_TRIG_SIDE_C_HOLD_2S] = C20_MakeNone();

    p_out.modes[2][EN_C10_TRIG_SIDE_R_CLICK]   = C20_MakeConsumer(EN_C20_CON_VOL_DOWN);
    p_out.modes[2][EN_C10_TRIG_SIDE_R_LONG]    = C20_MakeNone();

    p_out.modes[2][EN_C10_TRIG_FLICK_LEFT]     = C20_MakeConsumer(EN_C20_CON_REWIND);
    p_out.modes[2][EN_C10_TRIG_FLICK_RIGHT]    = C20_MakeConsumer(EN_C20_CON_FF);
    p_out.modes[2][EN_C10_TRIG_FLICK_UP]       = C20_MakeConsumer(EN_C20_CON_MUTE);

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
// SafeBoot
// =======================================================
void CL_C10_Config::getBootState(ST_C10_BootState_t& p_out) const {
    auto* v_self = const_cast<CL_C10_Config*>(this);
    if (v_self->_bootMutex) xSemaphoreTake(v_self->_bootMutex, portMAX_DELAY);
    p_out = v_self->_boot;
    if (v_self->_bootMutex) xSemaphoreGive(v_self->_bootMutex);
}

bool CL_C10_Config::clearSafeMode() {
    ST_C10_BootState_t v_snap;
    if (_bootMutex) xSemaphoreTake(_bootMutex, portMAX_DELAY);
    _boot.safe_mode  = false;
    _boot.fail_count = 0;
    _boot.pending    = false;
    v_snap = _boot;
    if (_bootMutex) xSemaphoreGive(_bootMutex);
    return _saveBootState(v_snap);
}

bool CL_C10_Config::bootMarkOkIfGracePassed(uint32_t p_graceMs) {
    uint32_t v_now = (uint32_t)millis();
    ST_C10_BootState_t v_snap;
    bool v_doSave = false;

    if (_bootMutex) xSemaphoreTake(_bootMutex, portMAX_DELAY);
    if (_boot.boot_ms == 0) _boot.boot_ms = v_now;
    const uint32_t v_alive = (v_now >= _boot.boot_ms) ? (v_now - _boot.boot_ms) : 0;

    if (v_alive < p_graceMs) {
        if (_bootMutex) xSemaphoreGive(_bootMutex);
        return false;
    }
    if (_bootOkRetryAtMs != 0 && (int32_t)(v_now - _bootOkRetryAtMs) < 0) {
        if (_bootMutex) xSemaphoreGive(_bootMutex);
        return false;
    }

    _boot.pending = false;
    if (!_boot.safe_mode) _boot.fail_count = 0;
    v_snap = _boot;
    v_doSave = true;
    if (_bootMutex) xSemaphoreGive(_bootMutex);

    const bool v_ok = _saveBootState(v_snap);
    if (v_ok) _bootOkRetryAtMs = 0;
    else      _bootOkRetryAtMs = v_now + 30000u;
    (void)v_doSave;
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
    if (!v_d["last_host_addr"].isNull()) {
        strlcpy(p_out.last_host_addr, (const char*)v_d["last_host_addr"],
                sizeof(p_out.last_host_addr));
    } else {
        p_out.last_host_addr[0] = '\0';
    }
    return true;
}

bool CL_C10_Config::_saveBootState(const ST_C10_BootState_t& p_in) {
    JsonDocument v_d;
    v_d["safe_mode"]         = p_in.safe_mode;
    v_d["fail_count"]        = p_in.fail_count;
    v_d["pending"]           = p_in.pending;
    v_d["boot_ms"]           = p_in.boot_ms;
    v_d["last_reset_reason"] = p_in.last_reset_reason;
    v_d["last_host_addr"]    = p_in.last_host_addr;

    const char* v_tmp  = C10_DEF::BOOT_TMP_PATH;
    const char* v_old  = C10_DEF::BOOT_OLD_PATH;
    const char* v_main = C10_DEF::BOOT_PATH;

    if (LittleFS.exists(v_tmp)) (void)LittleFS.remove(v_tmp);

    File v_f = LittleFS.open(v_tmp, "w");
    if (!v_f) return false;

    if (serializeJson(v_d, v_f) == 0) {
        v_f.close();
        (void)LittleFS.remove(v_tmp);
        return false;
    }
    v_f.flush();
    v_f.close();

    if (!_verifyJsonFile(v_tmp)) {
        (void)LittleFS.remove(v_tmp);
        return false;
    }

    // [P2] rotate: main → old
    if (LittleFS.exists(v_old)) (void)LittleFS.remove(v_old);
    if (LittleFS.exists(v_main)) {
        if (!LittleFS.rename(v_main, v_old)) {
            (void)LittleFS.remove(v_tmp);
            return false;
        }
    }

    // commit: tmp → main
    if (!LittleFS.rename(v_tmp, v_main)) {
        if (LittleFS.exists(v_old)) (void)LittleFS.rename(v_old, v_main);
        (void)LittleFS.remove(v_tmp);
        return false;
    }

    if (LittleFS.exists(v_old)) (void)LittleFS.remove(v_old);
    return true;
}


// =======================================================
// File utils
// =======================================================
bool CL_C10_Config::_verifyJsonFile(const char* p_path) {
    File v_f = LittleFS.open(p_path, "r");
    if (!v_f) return false;

    // [v0415 OPT-B] 파일 크기 사전 검증 (heap 할당 방지 + 손상 파일 조기 검출)
    const size_t v_sz = v_f.size();
    if (v_sz == 0 || v_sz > 128 * 1024) {
        v_f.close();
        D10_LOGW_C("C10::_verifyJsonFile", "invalid file size: %u", (unsigned)v_sz);
        return false;
    }

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

bool CL_C10_Config::_atomicWriteJson(const char* p_path, const char* p_tmpPath,
                                     JsonDocument& p_doc) {
    // [P5] begin() 이후 1회만 mkdir (매 저장 반복 제거)
    if (!_dirsEnsured) {
        (void)LittleFS.mkdir("/json");
        (void)LittleFS.mkdir(C10_DEF::PROFILES_DIR);
        _dirsEnsured = true;
    }

    if (LittleFS.exists(p_tmpPath)) (void)LittleFS.remove(p_tmpPath);

    // 1) tmp write
    File v_f = LittleFS.open(p_tmpPath, "w");
    if (!v_f) {
        D10_LOGE_C("C10::_atomicWriteJson", "Failed to open tmp file: %s", p_tmpPath);
        return false;
    }

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
// Factory Reset
// =======================================================
bool CL_C10_Config::factoryReset(bool p_recreateDefault) {
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

    if (LittleFS.exists(C10_DEF::PROFILE_ACTIVE))     (void)LittleFS.remove(C10_DEF::PROFILE_ACTIVE);
    if (LittleFS.exists(C10_DEF::PROFILE_ACTIVE_TMP)) (void)LittleFS.remove(C10_DEF::PROFILE_ACTIVE_TMP);

    if (LittleFS.exists(C10_DEF::BOOT_PATH))     (void)LittleFS.remove(C10_DEF::BOOT_PATH);
    if (LittleFS.exists(C10_DEF::BOOT_TMP_PATH)) (void)LittleFS.remove(C10_DEF::BOOT_TMP_PATH);

    // Multi-Host 저장소 정리
    if (LittleFS.exists(C10_DEF::HOSTS_PATH))     (void)LittleFS.remove(C10_DEF::HOSTS_PATH);
    if (LittleFS.exists(C10_DEF::HOSTS_TMP_PATH)) (void)LittleFS.remove(C10_DEF::HOSTS_TMP_PATH);
    if (LittleFS.exists(C10_DEF::HOSTS_OLD_PATH)) (void)LittleFS.remove(C10_DEF::HOSTS_OLD_PATH);

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


// =======================================================
// Multi-Host: LastHostAddr
// =======================================================
bool CL_C10_Config::setLastHostAddr(const char* p_addr) {
    if (_bootMutex) xSemaphoreTake(_bootMutex, portMAX_DELAY);
    if (p_addr) {
        strlcpy(_boot.last_host_addr, p_addr, sizeof(_boot.last_host_addr));
    } else {
        _boot.last_host_addr[0] = '\0';
    }
    ST_C10_BootState_t v_snap = _boot;
    if (_bootMutex) xSemaphoreGive(_bootMutex);
    return _saveBootState(v_snap);
}

bool CL_C10_Config::getLastHostAddr(char* p_out, size_t p_outSize) const {
    if (!p_out || p_outSize < 18) return false;
    if (_bootMutex) xSemaphoreTake(_bootMutex, portMAX_DELAY);
    strlcpy(p_out, _boot.last_host_addr, p_outSize);
    if (_bootMutex) xSemaphoreGive(_bootMutex);
    return (p_out[0] != '\0');
}


// =======================================================
// Multi-Host: Hosts CRUD
// =======================================================
bool CL_C10_Config::loadHosts(ST_C10_HostsConfig_t& p_out) {
    memset(&p_out, 0, sizeof(p_out));
    p_out.ver = 1;

    const char* v_main = C10_DEF::HOSTS_PATH;
    const char* v_old  = C10_DEF::HOSTS_OLD_PATH;

    const char* v_target = nullptr;
    if (LittleFS.exists(v_main) && _verifyJsonFile(v_main)) {
        v_target = v_main;
    } else if (LittleFS.exists(v_old) && _verifyJsonFile(v_old)) {
        v_target = v_old;
    }

    if (!v_target) {
        return false;
    }

    File v_f = LittleFS.open(v_target, "r");
    if (!v_f) return false;

    JsonDocument v_doc;
    DeserializationError v_err = deserializeJson(v_doc, v_f);
    v_f.close();
    if (v_err) return false;

    if (!v_doc["ver"].isNull()) p_out.ver = (uint16_t)v_doc["ver"];

    JsonArrayConst v_arr = v_doc["hosts"].as<JsonArrayConst>();
    if (!v_arr.isNull()) {
        uint8_t v_cnt = 0;
        for (JsonObjectConst v_h : v_arr) {
            if (v_cnt >= C10_DEF::HOSTS_MAX) break;
            ST_C10_Host_t& v_dst = p_out.hosts[v_cnt];
            if (!v_h["addr"].isNull())   strlcpy(v_dst.addr,  (const char*)v_h["addr"],  sizeof(v_dst.addr));
            if (!v_h["label"].isNull())  strlcpy(v_dst.label, (const char*)v_h["label"], sizeof(v_dst.label));
            if (!v_h["color"].isNull())  strlcpy(v_dst.color, (const char*)v_h["color"], sizeof(v_dst.color));
            if (!v_h["order"].isNull())  v_dst.order  = (uint8_t)v_h["order"].as<int>();
            if (!v_h["pinned"].isNull()) v_dst.pinned = (bool)v_h["pinned"];
            v_cnt++;
        }
        p_out.count = v_cnt;
    }
    return true;
}

bool CL_C10_Config::saveHosts(const ST_C10_HostsConfig_t& p_in) {
    JsonDocument v_doc;
    v_doc["ver"] = p_in.ver > 0 ? p_in.ver : 1;
    JsonArray v_arr = v_doc["hosts"].to<JsonArray>();

    const uint8_t v_count = p_in.count > C10_DEF::HOSTS_MAX
                          ? C10_DEF::HOSTS_MAX : p_in.count;
    for (uint8_t i = 0; i < v_count; i++) {
        const ST_C10_Host_t& v_src = p_in.hosts[i];
        JsonObject v_h = v_arr.add<JsonObject>();
        v_h["addr"]   = v_src.addr;
        v_h["label"]  = v_src.label;
        v_h["color"]  = v_src.color;
        v_h["order"]  = v_src.order;
        v_h["pinned"] = v_src.pinned;
    }

    const char* v_tmp  = C10_DEF::HOSTS_TMP_PATH;
    const char* v_old  = C10_DEF::HOSTS_OLD_PATH;
    const char* v_main = C10_DEF::HOSTS_PATH;

    if (LittleFS.exists(v_tmp)) (void)LittleFS.remove(v_tmp);

    File v_f = LittleFS.open(v_tmp, "w");
    if (!v_f) return false;

    if (serializeJson(v_doc, v_f) == 0) {
        v_f.close();
        (void)LittleFS.remove(v_tmp);
        return false;
    }
    v_f.flush();
    v_f.close();

    if (!_verifyJsonFile(v_tmp)) {
        (void)LittleFS.remove(v_tmp);
        return false;
    }

    // 3-tier rotate: main → old
    if (LittleFS.exists(v_old)) (void)LittleFS.remove(v_old);
    if (LittleFS.exists(v_main)) {
        if (!LittleFS.rename(v_main, v_old)) {
            (void)LittleFS.remove(v_tmp);
            return false;
        }
    }

    // commit: tmp → main
    if (!LittleFS.rename(v_tmp, v_main)) {
        if (LittleFS.exists(v_old)) (void)LittleFS.rename(v_old, v_main);
        (void)LittleFS.remove(v_tmp);
        return false;
    }

    if (LittleFS.exists(v_old)) (void)LittleFS.remove(v_old);
    return true;
}
