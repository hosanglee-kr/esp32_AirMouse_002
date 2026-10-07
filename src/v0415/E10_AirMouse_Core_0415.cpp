// =======================================================
// File: E10_AirMouse_Core_0415.cpp
// =======================================================
#include "E10_AirMouse_0415.h"
#include "HW_Def_0415.h"

// =======================================================
// ctor / begin
// =======================================================
CL_E10_EliteAirMouse::CL_E10_EliteAirMouse()
    : _hid("Elite AirMouse S3", "ProMaker", 100) {

    memset(&_state, 0, sizeof(_state));

    memset(_errHist, 0, sizeof(_errHist));
    memset(_spikes,  0, sizeof(_spikes));
}

void CL_E10_EliteAirMouse::begin(CL_C10_Config* p_cfg) {
    _cfg     = p_cfg;
    _uptime0 = millis();

    Wire.begin(HW_DEF::PIN_I2C_SDA, HW_DEF::PIN_I2C_SCL);
    Wire.setClock(400000);

    if (!_mpu.begin()) {
        Serial.println("[E10] MPU begin fail");
        for (;;) delay(10);
    }
    _mpu.setGyroRange(MPU6050_RANGE_250_DEG);
    _mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
    _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    // [C-2] recursive mutex (setSafeMode/setOtaGuard가 락 보유 중 _pushErr 재진입)
    _mutex = xSemaphoreCreateRecursiveMutex();

    memset(&_cfgProfile, 0, sizeof(_cfgProfile));
    _cfgProfileValid = false;

    _qFrame = xQueueCreate(1, sizeof(ST_E10_Frame_t));
    if (_qFrame) {
        ST_E10_Frame_t v_init;
        memset(&v_init, 0, sizeof(v_init));
        v_init.updated = true;
        xQueueOverwrite(_qFrame, &v_init);
    }

    // [Phase2] HID command queue (C-3)
    _qHidCmd = xQueueCreate(4, sizeof(ST_E10_HidCmd_t));

    _safeMode = (_cfg && _cfg->isSafeMode());

    (void)_applyFromConfig();

    _hid.addDevice(&_keyboard);
    _hid.addDevice(&_mouse);
    _hid.begin();
    
    // ====================================================
    // [Phase 6-J] LED 초기화 + Mode 색 반영
    // ====================================================
    _led.begin(_cfgProfileValid ? _cfgProfile.e10.led_brightness : 128);
    _led.setModeColor(_activeMode);
    
    // [Phase 10] BLE Manager 초기화
    _ble.begin();
    
    // [Phase 8] Power Manager 초기화
    _power.begin();
    
    // LED 태스크 (저우선, 50ms tick)
    xTaskCreatePinnedToCore(_ledTask, "E10_Led", 2048, this, 1, nullptr, 0);
    
    // ====================================================
    // [Phase 5] Dispatcher + Executor 초기화
    // ====================================================
    _btnDisp.begin();
    _btnDisp.setCallback(&CL_E10_EliteAirMouse::_onBtnEvent, this);
    
    _actExec.begin(&_mouse, &_keyboard);

    _qActionExec = xQueueCreate(8, sizeof(ST_ActionCmd_t));
    if (!_qActionExec) {
        D10_LOGE("[E10] _qActionExec create failed");
    }
    
    // config에서 active_mode 초기값 반영
    if (_cfgProfileValid) {
        const uint8_t v_m = _cfgProfile.e10.active_mode;
        if (v_m >= 1 && v_m <= C10_DEF::MODE_COUNT) {
            _activeMode = v_m;
        }
    }


    xTaskCreatePinnedToCore(_sensorTask, "E10_Sensor", 8192, this, 3, &_thSensor, 1);
    xTaskCreatePinnedToCore(_commTask,   "E10_Comm",   4096, this, 2, &_thComm, 0);
}


// =======================================================
// HID cmd enqueue (producer: any task)
// - timeout 0: 웹 태스크 블로킹 금지
// - 큐 full이면 drop(false)
// =======================================================
bool CL_E10_EliteAirMouse::_enqueueHidCmd(const ST_E10_HidCmd_t& p_cmd) {
    if (!_qHidCmd) return false;
    return (xQueueSend(_qHidCmd, &p_cmd, 0) == pdTRUE);
}

// =======================================================
// apply-only (저장 없이 UI에서 반영)
// =======================================================
bool CL_E10_EliteAirMouse::applyRuntimeE10(const ST_C10_E10Config_t& p_e) {
    _lock();
    _applyRuntimeLocked(p_e);
    _unlock();
    return true;
}

// [H-3] caller가 _lock() 보유 상태에서 호출 (recursive mutex)
void CL_E10_EliteAirMouse::_applyRuntimeLocked(const ST_C10_E10Config_t& p_e) {
    _applyE10ToRuntime(p_e);

    _snapshotRuntimeToE10Config(_cfgProfile.e10);
    _cfgProfileValid = true;

    if (_precision_mode == (uint8_t)EN_C10_E10_PREC_OFF) {
        _precSub = EN_PREC_OFF;
        _precSmX = 0.0f;
        _precSmY = 0.0f;
    } else {
        if (_precSub == EN_PREC_OFF) {
            _precSub = EN_PREC_ENTRY;
            _precT0  = (uint32_t)millis();
            _precSmX = 0.0f;
            _precSmY = 0.0f;
        }
    }

    _state.updated = true;
}

// =======================================================
// control
// =======================================================
bool CL_E10_EliteAirMouse::setPptMode(bool p_enable) {
    _lock();
    _isPptMode = p_enable;
    _unlock();
    return true;
}

bool CL_E10_EliteAirMouse::setDpiLevel(uint8_t p_level) {
    uint8_t v_lv = p_level;
    if (v_lv < 1) v_lv = 1;
    if (v_lv > 3) v_lv = 3;

    // [H-3] read-modify-write 원자화 (get과 apply 사이에 다른 요청 끼어들기 방지)
    _lock();
    if (!_cfgProfileValid) {
        _snapshotRuntimeToE10Config(_cfgProfile.e10);
        _cfgProfileValid = true;
    }
    ST_C10_E10Config_t v_e = _cfgProfile.e10;
    v_e.dpi_level = v_lv;
    _applyRuntimeLocked(v_e);
    _unlock();
    return true;
}

bool CL_E10_EliteAirMouse::setPrecisionMode(uint8_t p_mode) {
    uint8_t v_mode = p_mode;
    if (v_mode > (uint8_t)EN_C10_E10_PREC_PPT) v_mode = (uint8_t)EN_C10_E10_PREC_PPT;

    // [H-3] RMW 원자화
    _lock();
    if (!_cfgProfileValid) {
        _snapshotRuntimeToE10Config(_cfgProfile.e10);
        _cfgProfileValid = true;
    }
    ST_C10_E10Config_t v_e = _cfgProfile.e10;
    v_e.precision_mode = v_mode;
    _applyRuntimeLocked(v_e);
    _unlock();
    return true;
}

bool CL_E10_EliteAirMouse::setHardClickLock(bool p_enable) {
    // [H-3] RMW 원자화
    _lock();
    if (!_cfgProfileValid) {
        _snapshotRuntimeToE10Config(_cfgProfile.e10);
        _cfgProfileValid = true;
    }
    ST_C10_E10Config_t v_e = _cfgProfile.e10;
    v_e.hard_click_lock = p_enable;
    _applyRuntimeLocked(v_e);
    _unlock();
    return true;
}

bool CL_E10_EliteAirMouse::setSafeMode(bool p_enable) {
    _lock();
    _safeMode = p_enable;

    _state.btn_mask = 0;
    _state.updated  = true;

    // [M-4] 전용 코드
    _pushErr(p_enable ? EN_E10_ERR_SAFE_MODE_ENTER : EN_E10_ERR_SAFE_MODE_EXIT, 0);

    _unlock();
    return true;
}

bool CL_E10_EliteAirMouse::setOtaGuard(bool p_enable) {
    _lock();

    if (p_enable) {
        if (!_otaGuard) {
            _otaGuard = true;
            _otaGuardCount++;
            _otaGuardT0Ms = (uint32_t)(millis() - _uptime0);
            _pushErr(EN_E10_ERR_OTA_GUARD_ENTER, 0);
        }
        _state.btn_mask = 0;
        _state.updated  = true;
    } else {
        if (_otaGuard) {
            _otaGuard = false;
            _pushErr(EN_E10_ERR_OTA_GUARD_EXIT, 0);
        }
        _state.updated = true;
    }

    _unlock();
    return true;
}

// =======================================================
// Config apply
// =======================================================
bool CL_E10_EliteAirMouse::_applyFromConfig() {
    return _reloadActiveProfile();
}

// =======================================================
// [v0412] 활성 프로파일 로드/저장
// =======================================================
bool CL_E10_EliteAirMouse::_reloadActiveProfile() {
    if (!_cfg) return false;

    ST_C10_ProfileConfig_t v_p;
    _cfg->makeDefaultsProfile(_cfg->getActiveIndex(), v_p);
    (void)_cfg->loadActiveProfile(v_p);

    _lock();
    _applyE10ToRuntime(v_p.e10);
    _snapshotRuntimeToE10Config(v_p.e10);

    _cfgProfile      = v_p;
    _cfgProfileValid = true;
    _unlock();
    return true;
}

bool CL_E10_EliteAirMouse::_saveActiveProfile() {
    if (!_cfg || !_cfgProfileValid) return false;

    // 런타임 E10 값 반영 (슬롯/매크로는 _cfgProfile 그대로)
    _lock();
    _snapshotRuntimeToE10Config(_cfgProfile.e10);
    ST_C10_ProfileConfig_t v_copy = _cfgProfile;
    _unlock();

    return _cfg->saveActiveProfile(v_copy);
}

// =======================================================
// [v0412] 슬롯 조회 (Global + Mode Override)
// =======================================================
ST_C20_ActionSlot_t CL_E10_EliteAirMouse::_resolveSlot(uint8_t p_mode, uint8_t p_trig) const {
    // [C-4] 프로파일 스위치 중 부분 갱신된 _cfgProfile을 읽지 않도록 락
    //       (recursive mutex이므로 sensorTask/commTask 재진입 안전)
    auto* v_self = const_cast<CL_E10_EliteAirMouse*>(this);
    v_self->_lock();

    ST_C20_ActionSlot_t v_out;
    if (!_cfgProfileValid) {
        v_out = { EN_C20_ACT_NONE, EN_C20_HOLD_NONE, 0, 0 };
    } else {
        v_out = C10_ResolveSlot(_cfgProfile.slots, p_mode, p_trig);
    }

    v_self->_unlock();
    return v_out;
}

void CL_E10_EliteAirMouse::_snapshotRuntimeToE10Config(ST_C10_E10Config_t& p_out) {
    // [Phase 1~3, 11.6, 11.7] motion_adv, power, button 백업 (makeDefaults로 덮어쓰기 방지)
    ST_C10_MotionAdv_t   v_maBackup = p_out.motion_adv;
    ST_C10_PowerConfig_t  v_pwBackup = p_out.power;
    ST_C10_ButtonConfig_t v_btBackup = p_out.button;

    if (_cfg) {
        _cfg->makeDefaultsE10(p_out);
    } else {
        memset(&p_out, 0, sizeof(p_out));
    }

    // [Phase 1~3, 11.6, 11.7] 복원
    p_out.motion_adv = v_maBackup;
    p_out.power      = v_pwBackup;
    p_out.button     = v_btBackup;
    
    p_out.dpi_level       = (uint8_t)_dpiLevel;
    p_out.hard_click_lock = _hardClickLock;
    for (int i = 0; i < 3; i++) {
        p_out.scale_base[i] = _scaleBase[i];
        p_out.accel_gain[i] = _accelGain[i];
    }
    p_out.accel_threshold     = _accelTh;
    p_out.wheel_threshold_deg = _wheelThDeg;
    p_out.wheel_step_max      = (uint8_t)_wheelStepMax;
    p_out.gesture_flick_deg   = _gestureFlickDeg;
    p_out.gesture_cooldown_ms = (uint16_t)_gestureCooldownMs;
    p_out.scroll_cursor_damp  = _scrollCursorDamp;

    p_out.precision_mode       = (uint8_t)_precision_mode;
    p_out.precision_deadzone   = _precDeadzone;
    p_out.precision_gain       = _precGain;
    p_out.precision_accel      = _precAccel;
    p_out.precision_max_step   = (uint8_t)_precMaxStep;
    p_out.precision_smooth     = _precSmooth;
    p_out.prec_entry_ms        = (uint16_t)_precEntryMs;
    p_out.prec_exit_ms         = (uint16_t)_precExitMs;
    p_out.prec_entry_still_deg = _precEntryStillDeg;
    p_out.prec_exit_move_deg   = _precExitMoveDeg;
    p_out.prec_profile         = (uint8_t)_precProfile;

}

void CL_E10_EliteAirMouse::_applyE10ToRuntime(const ST_C10_E10Config_t& p_e) {
    int v_dpi = (int)p_e.dpi_level;
    if (v_dpi < 1) v_dpi = 1;
    if (v_dpi > 3) v_dpi = 3;
    _dpiLevel = v_dpi;

    _hardClickLock = p_e.hard_click_lock;

    for (int v_i = 0; v_i < 3; v_i++) {
        float v_sb = p_e.scale_base[v_i];
        float v_ag = p_e.accel_gain[v_i];
        if (v_sb < 0.05f) v_sb = 0.05f;
        if (v_sb > 5.00f) v_sb = 5.00f;
        if (v_ag < 0.00f) v_ag = 0.00f;
        if (v_ag > 8.00f) v_ag = 8.00f;
        _scaleBase[v_i] = v_sb;
        _accelGain[v_i] = v_ag;
    }

    _accelTh = p_e.accel_threshold;
    if (_accelTh < 0.5f) _accelTh = 0.5f;
    if (_accelTh > 80.f) _accelTh = 80.f;

    _wheelThDeg = p_e.wheel_threshold_deg;
    if (_wheelThDeg < 5.0f)   _wheelThDeg = 5.0f;
    if (_wheelThDeg > 300.0f) _wheelThDeg = 300.0f;

    _wheelStepMax = (int)p_e.wheel_step_max;
    if (_wheelStepMax < 1)  _wheelStepMax = 1;
    if (_wheelStepMax > 25) _wheelStepMax = 25;

    _gestureFlickDeg = p_e.gesture_flick_deg;
    if (_gestureFlickDeg < 20.0f)  _gestureFlickDeg = 20.0f;
    if (_gestureFlickDeg > 2000.f) _gestureFlickDeg = 2000.f;

    _gestureCooldownMs = p_e.gesture_cooldown_ms;
    if (_gestureCooldownMs < 50)   _gestureCooldownMs = 50;
    if (_gestureCooldownMs > 8000) _gestureCooldownMs = 8000;

    _scrollCursorDamp = p_e.scroll_cursor_damp;
    if (_scrollCursorDamp < 0.01f) _scrollCursorDamp = 0.01f;
    if (_scrollCursorDamp > 1.00f) _scrollCursorDamp = 1.00f;

    _precision_mode    = p_e.precision_mode;
    _precDeadzone      = p_e.precision_deadzone;
    _precGain          = p_e.precision_gain;
    _precAccel         = p_e.precision_accel;
    _precMaxStep       = p_e.precision_max_step;
    _precSmooth        = p_e.precision_smooth;
    _precEntryMs       = p_e.prec_entry_ms;
    _precExitMs        = p_e.prec_exit_ms;
    _precEntryStillDeg = p_e.prec_entry_still_deg;
    _precExitMoveDeg   = p_e.prec_exit_move_deg;
    _precProfile       = p_e.prec_profile;

    if (_precDeadzone < 0.0f) _precDeadzone = 0.0f;
    if (_precDeadzone > 40.f) _precDeadzone = 40.f;

    if (_precGain < 0.01f) _precGain = 0.01f;
    if (_precGain > 10.0f) _precGain = 10.0f;

    if (_precAccel < 0.0f) _precAccel = 0.0f;
    if (_precAccel > 5.0f) _precAccel = 5.0f;

    if (_precMaxStep < 1)   _precMaxStep = 1;
    if (_precMaxStep > 127) _precMaxStep = 127;

    if (_precSmooth < 0.0f) _precSmooth = 0.0f;
    if (_precSmooth > 0.97f) _precSmooth = 0.97f;

    if (_precEntryMs < 0) _precEntryMs = 0;
    if (_precExitMs < 0)  _precExitMs  = 0;

    if (_precEntryStillDeg < 0.1f) _precEntryStillDeg = 0.1f;
    if (_precExitMoveDeg < _precEntryStillDeg) _precExitMoveDeg = _precEntryStillDeg + 0.5f;

    if (_precProfile == 0) _precProfile = 1;

    if (_precision_mode > (uint8_t)EN_C10_E10_PREC_PPT) _precision_mode = (uint8_t)EN_C10_E10_PREC_PPT;

    if (_precision_mode == (uint8_t)EN_C10_E10_PREC_OFF) {
        _precSub = EN_PREC_OFF;
        _precSmX = 0.0f;
        _precSmY = 0.0f;
    } else {
        if (_precSub == EN_PREC_OFF) {
            _precSub = EN_PREC_ENTRY;
            _precT0  = (uint32_t)millis();
            _precSmX = 0.0f;
            _precSmY = 0.0f;
        }
    }
    
    _biasTracker.setConfig(
    p_e.gyro_bias.still_th,
    p_e.gyro_bias.still_win_ms,
    p_e.gyro_bias.alpha);
    
    // ====================================================
    // [Phase 7] 제스처 감지기 config 반영
    // ====================================================
    {
        CL_M30_Gesture::ST_Config_t v_gc;
        v_gc.flick_p2p_th      = p_e.flick.p2p_th;
        v_gc.flick_window_ms   = p_e.flick.window_ms;
        v_gc.flick_cooldown_ms = p_e.flick.cooldown_ms;
    
        v_gc.linear_th         = p_e.linear.th;
        v_gc.linear_impulse_th = p_e.linear.impulse_th;
        v_gc.linear_window_ms  = p_e.linear.window_ms;
    
        v_gc.tilt_angle_deg    = p_e.tilt_hold.angle_deg;
        v_gc.tilt_hold_ms      = p_e.tilt_hold.hold_ms;
        v_gc.tilt_repeat_hz    = p_e.tilt_hold.repeat_hz;
    
        _gesture.setConfig(v_gc);
    }
    
    // [Phase 10] Active peer index 반영
    _ble.setActivePeerIndex(p_e.active_peer_index);
    
    // ====================================================
    // [Phase 11.6] Power config 반영
    // ====================================================
    {
        CL_P20_Power::ST_Config_t v_pcfg;
        v_pcfg.idle_timeout_ms[0]      = p_e.power.idle_timeout_ms[0];
        v_pcfg.idle_timeout_ms[1]      = p_e.power.idle_timeout_ms[1];
        v_pcfg.idle_timeout_ms[2]      = p_e.power.idle_timeout_ms[2];
        v_pcfg.idle_timeout_ble_ms     = p_e.power.idle_timeout_ble_ms;
        v_pcfg.pairing_idle_timeout_ms = p_e.power.pairing_idle_timeout_ms;
        v_pcfg.deep_idle_timeout_ms    = p_e.power.deep_idle_timeout_ms;
        v_pcfg.wake_min_active_ms      = p_e.power.wake_min_active_ms;
        v_pcfg.wom_threshold           = p_e.power.wom_threshold;
        v_pcfg.wom_duration            = p_e.power.wom_duration;
        v_pcfg.fast_recalib_ms         = p_e.power.fast_recalib_ms;
        _power.setConfig(v_pcfg);
    }

    // ====================================================
    // [Phase 11.7] Button Timing config 반영
    // ====================================================
    _btnDisp.setTimings(
        p_e.button.debounce_press_ms,
        p_e.button.debounce_release_ms,
        p_e.button.long_delay_ms,
        p_e.button.double_delay_ms,
        p_e.button.hold_2s_ms,
        p_e.button.hold_3s_ms,
        p_e.button.min_click_ms,
        p_e.button.debounce_min_ticks);

    _engine.setHardClickLock(_hardClickLock);
    _engine.setDPI(_dpiLevel);
    
    // ====================================================
    // [Phase 2] Adaptive EMA config 주입
    // ====================================================
    {
        CL_M10_AdvancedMotionProcessor::ST_EmaCfg_t v_ema;
        v_ema.alpha_min      = p_e.motion_adv.ema.alpha_min;
        v_ema.alpha_max      = p_e.motion_adv.ema.alpha_max;
        v_ema.deadzone_th    = p_e.motion_adv.ema.deadzone_th;
        v_ema.fast_th        = p_e.motion_adv.ema.fast_th;
        v_ema.reversal_th    = p_e.motion_adv.ema.reversal_th;
        v_ema.reversal_reset = p_e.motion_adv.ema.reversal_reset;

        // [Phase 2] alpha_min < alpha_max 클램프
        if (v_ema.alpha_min >= v_ema.alpha_max) {
            float v_tmp = v_ema.alpha_min;
            v_ema.alpha_min = v_ema.alpha_max;
            v_ema.alpha_max = v_tmp;
        }

        _engine.setEmaConfig(v_ema);
    }
    
    _led.setBrightness(p_e.led_brightness);
    _led.setFadeTimings(p_e.power.led_fadeout_ms, p_e.power.led_fadein_ms);

    // [Phase 5] Mode → _isPptMode 동기화 (FSM 호환)
    if (p_e.active_mode >= 1 && p_e.active_mode <= C10_DEF::MODE_COUNT) {
        _activeMode = p_e.active_mode;
        _isPptMode  = (p_e.active_mode == 2);
    }

}


// =======================================================
// [Phase 10] main loop에서 호출 — BLE dirty 플래그 처리
// =======================================================
void CL_E10_EliteAirMouse::tickConfigSave() {
    if (!_reqSaveCfg) {
        if (_ble.consumeDirty()) _reqSaveCfg = true;
        else return;
    }

    if (!_cfg || !_cfgProfileValid) { _reqSaveCfg = false; return; }

    // 활성 프로파일 스냅샷 갱신 (BLE peer / Mode)
    _lock();
    _cfgProfile.e10.active_peer_index = _ble.getActivePeerIndex();
    _cfgProfile.e10.active_mode       = _activeMode;
    _unlock();

    const bool v_ok = _saveActiveProfile();
    if (v_ok) {
        D10_LOGI("[E10] profile saved: peer=%u mode=%u",
                 (unsigned)_ble.getActivePeerIndex(), (unsigned)_activeMode);
    } else {
        D10_LOGW("[E10] profile save failed");
    }

    _reqSaveCfg = false;
}

// =======================================================
// [v0412] Profile 관리 (public API)
// =======================================================
bool CL_E10_EliteAirMouse::reloadActiveProfile() {
    return _reloadActiveProfile();
}

bool CL_E10_EliteAirMouse::saveActiveProfile() {
    return _saveActiveProfile();
}

bool CL_E10_EliteAirMouse::getActiveProfileInfo(uint8_t& p_outIdx, uint8_t& p_outCount,
                                                char* p_outName, size_t p_outNameSize) {
    if (!_cfg) return false;

    p_outIdx   = _cfg->getActiveIndex();
    p_outCount = _cfg->getProfileCount();

    if (p_outName && p_outNameSize > 0) {
        _lock();
        if (_cfgProfileValid) {
            strlcpy(p_outName, _cfgProfile.name, p_outNameSize);
        } else {
            p_outName[0] = '\0';
        }
        _unlock();
    }
    return true;
}

bool CL_E10_EliteAirMouse::switchProfile(uint8_t p_idx) {
    if (!_cfg) return false;
    // [C-4] AsyncWebServer는 단일 이벤트 태스크에서 직렬 처리되므로
    //   isProfileSwitchInProgress → setProfileSwitchInProgress 사이의
    //   이론적 race는 실무적으로 발생하지 않는다. (재진입 방어 목적)
    if (_cfg->isProfileSwitchInProgress()) {
        D10_LOGW("[E10] switchProfile: already in progress");
        return false;
    }
    
    if (p_idx >= _cfg->getProfileCount()) {
        D10_LOGW("[E10] switchProfile: invalid idx=%u", (unsigned)p_idx);
        return false;
    }

    _cfg->setProfileSwitchInProgress(true);

    // 1) 매크로 취소 (H-1: 토큰 카운터)
    _macroAbortToken++;
    _macroState.active = false;

    // 2) 큐 드레인 (H-2): 이전 프로파일의 잔여 커맨드 제거
    {
        ST_ActionCmd_t v_adrop;
        while (_qActionExec && xQueueReceive(_qActionExec, &v_adrop, 0) == pdTRUE) {}
        ST_E10_HidCmd_t v_hdrop;
        while (_qHidCmd && xQueueReceive(_qHidCmd, &v_hdrop, 0) == pdTRUE) {}
    }

    // 3) 리셋 플래그 위임 (H-3): 실제 리셋은 sensorTask에서
    _reqResetBtnDisp = true;
    _reqResetGesture = true;
    _frontHoldActive = false;
    _moveGateHeld    = false;

    // 4) HID 안전 release (C-2: 큐 경유 + REQ-FIX-03 위임 플래그로 완벽 보장)
    //   - 큐 Drop 발생 시에도 commTask가 반드시 릴리즈하도록 보장
    _reqCommReleaseAll = true;
    (void)forceReleaseButtons();

    // 5) active index 저장
    const bool v_idxOk = _cfg->setActiveIndex(p_idx);
    if (!v_idxOk) {
        D10_LOGW("[E10] switchProfile: setActiveIndex failed");
        _cfg->setProfileSwitchInProgress(false);
        return false;
    }

    // 6) 새 프로파일 로드 + 런타임 반영
    const bool v_reloadOk = _reloadActiveProfile();

    // 7) Active Mode도 새 프로파일 기준으로 (_lock 보호 하에 원자적 갱신)
    if (v_reloadOk && _cfgProfileValid) {
        const uint8_t v_m = _cfgProfile.e10.active_mode;
        if (v_m >= 1 && v_m <= C10_DEF::MODE_COUNT) {
            _lock();
            _activeMode = v_m;
            _unlock();
        }
    }

    // 8) LED 표시
    _led.setModeColor(_activeMode);
    _led.flash(EN_L10_COLOR_WHITE, 500);

    _cfg->setProfileSwitchInProgress(false);

    D10_LOGI("[E10] switchProfile: idx=%u reload=%d",
             (unsigned)p_idx, (int)v_reloadOk);

    return v_reloadOk;
}

uint8_t CL_E10_EliteAirMouse::getMacroCount() const {
    if (!_cfgProfileValid) return 0;
    return _cfgProfile.macros.count;
}

bool CL_E10_EliteAirMouse::execLiveTest(uint8_t p_kind, uint8_t p_hMode,
                                        uint16_t p_p16, uint32_t p_p32) {
    if (!_hid.isConnected()) return false;
    if (_safeMode || _otaGuard) return false;

    ST_C20_ActionSlot_t v_slot;
    v_slot.kind     = p_kind;
    v_slot.holdMode = p_hMode;
    v_slot.param16  = p_p16;
    v_slot.param32  = p_p32;

    // [REQ-FIX-02] SPECIAL은 Web 태스크에서 직접 실행 금지 -> sensorTask로 안전하게 위임
    if (p_kind == (uint8_t)EN_C20_ACT_SPECIAL) {
        _reqSpecialAction = (uint8_t)p_p16;
        return true;
    }

    // MACRO / 기타는 큐 경유 (비동기)
    return _enqueueAction(v_slot, true);
}
