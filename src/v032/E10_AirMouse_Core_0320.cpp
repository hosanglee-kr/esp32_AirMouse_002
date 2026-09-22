// =======================================================
// File: E10_AirMouse_Core_0320.cpp
// =======================================================
#include "E10_AirMouse_0320.h"

// =======================================================
// ctor / begin
// =======================================================
CL_E10_EliteAirMouse::CL_E10_EliteAirMouse()
    : _hid("Elite AirMouse S3", "ProMaker", 100) {

    memset(&_state, 0, sizeof(_state));

    memset(_errHist, 0, sizeof(_errHist));
    memset(_spikes,  0, sizeof(_spikes));

    memset(&_ppt2_start, 0, sizeof(_ppt2_start));
    memset(&_ppt2_exit,  0, sizeof(_ppt2_exit));
    memset(&_ppt2_next,  0, sizeof(_ppt2_next));
    memset(&_ppt2_prev,  0, sizeof(_ppt2_prev));
    memset(&_ppt2_black, 0, sizeof(_ppt2_black));
    memset(&_ppt2_laser, 0, sizeof(_ppt2_laser));
}

void CL_E10_EliteAirMouse::begin(CL_C10_Config* p_cfg) {
    _cfg     = p_cfg;
    _uptime0 = millis();

    Wire.begin(E10_CONST::PIN_I2C_SDA, E10_CONST::PIN_I2C_SCL);
    Wire.setClock(400000);

    if (!_mpu.begin()) {
        Serial.println("[E10] MPU begin fail");
        for (;;) delay(10);
    }
    _mpu.setGyroRange(MPU6050_RANGE_250_DEG);
    _mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
    _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    pinMode(E10_CONST::PIN_BTN_L,      INPUT_PULLUP);
    pinMode(E10_CONST::PIN_BTN_R,      INPUT_PULLUP);
    pinMode(E10_CONST::PIN_BTN_M,      INPUT_PULLUP);

    pinMode(E10_CONST::PIN_BTN_MODE,   INPUT_PULLUP);
    pinMode(E10_CONST::PIN_BTN_SCROLL, INPUT_PULLUP);

    // [C-2] recursive mutex (setSafeMode/setOtaGuard가 락 보유 중 _pushErr 재진입)
    _mutex = xSemaphoreCreateRecursiveMutex();

    memset(&_cfgE10Runtime, 0, sizeof(_cfgE10Runtime));
    _cfgE10RuntimeValid = false;

    _qFrame = xQueueCreate(1, sizeof(ST_E10_Frame_t));
    if (_qFrame) {
        ST_E10_Frame_t v_init;
        memset(&v_init, 0, sizeof(v_init));
        v_init.updated = true;
        xQueueOverwrite(_qFrame, &v_init);
    }

    _safeMode = (_cfg && _cfg->isSafeMode());

    (void)_applyFromConfig();

    _hid.addDevice(&_keyboard);
    _hid.addDevice(&_mouse);
    _hid.begin();

    xTaskCreatePinnedToCore(_sensorTask, "E10_Sensor", 8192, this, 3, &_thSensor, 1);
    xTaskCreatePinnedToCore(_commTask,   "E10_Comm",   4096, this, 2, &_thComm, 0);
}

// =======================================================
// W10 apply hook
// =======================================================
bool CL_E10_EliteAirMouse::E10_W10Apply(void* p_ctx) {
    if (!p_ctx) return false;
    return ((CL_E10_EliteAirMouse*)p_ctx)->_applyFromConfig();
}

// =======================================================
// apply-only (저장 없이 UI에서 반영)
// =======================================================
bool CL_E10_EliteAirMouse::applyRuntimeE10(const ST_C10_E10Config_t& p_e) {
    _lock();

    _applyE10ToRuntime(p_e);

    _snapshotRuntimeToE10Config(_cfgE10Runtime);
    _cfgE10RuntimeValid = true;

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

    _unlock();
    return true;
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

    ST_C10_E10Config_t v_e;
    _getE10RuntimeConfig(v_e);
    v_e.dpi_level = v_lv;
    return applyRuntimeE10(v_e);
}

bool CL_E10_EliteAirMouse::setPrecisionMode(uint8_t p_mode) {
    uint8_t v_mode = p_mode;
    if (v_mode > (uint8_t)EN_C10_E10_PREC_PPT) v_mode = (uint8_t)EN_C10_E10_PREC_PPT;

    ST_C10_E10Config_t v_e;
    _getE10RuntimeConfig(v_e);
    v_e.precision_mode = v_mode;
    return applyRuntimeE10(v_e);
}

bool CL_E10_EliteAirMouse::setHardClickLock(bool p_enable) {
    ST_C10_E10Config_t v_e;
    _getE10RuntimeConfig(v_e);
    v_e.hard_click_lock = p_enable;
    return applyRuntimeE10(v_e);
}

bool CL_E10_EliteAirMouse::setSafeMode(bool p_enable) {
    _lock();
    _safeMode = p_enable;

    _state.btn_mask = 0;
    _state.updated  = true;

    _pushErr(EN_E10_ERR_OTA_GUARD, p_enable ? 2 : 3);

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
            _pushErr(EN_E10_ERR_OTA_GUARD, 1);
        }
        _state.btn_mask = 0;
        _state.updated  = true;
    } else {
        if (_otaGuard) {
            _otaGuard = false;
            _pushErr(EN_E10_ERR_OTA_GUARD, 0);
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
    if (!_cfg) return false;

    ST_C10_WiFiConfig_t v_w;
    ST_C10_E10Config_t  v_e;

    _cfg->makeDefaultsWiFi(v_w);
    _cfg->makeDefaultsE10(v_e);
    (void)_cfg->loadAll(v_w, v_e);

    _lock();
    _applyE10ToRuntime(v_e);
    _snapshotRuntimeToE10Config(_cfgE10Runtime);
    _cfgE10RuntimeValid = true;
    _unlock();
    return true;
}

void CL_E10_EliteAirMouse::_getE10RuntimeConfig(ST_C10_E10Config_t& p_out) {
    memset(&p_out, 0, sizeof(p_out));

    _lock();
    if (_cfgE10RuntimeValid) {
        p_out = _cfgE10Runtime;
        _unlock();
        return;
    }

    _snapshotRuntimeToE10Config(_cfgE10Runtime);
    _cfgE10RuntimeValid = true;
    p_out = _cfgE10Runtime;
    _unlock();
}

void CL_E10_EliteAirMouse::_snapshotRuntimeToE10Config(ST_C10_E10Config_t& p_out) {
    // NOTE: _lock() 보유 상태에서만 호출
    if (_cfg) {
        _cfg->makeDefaultsE10(p_out);
    } else {
        memset(&p_out, 0, sizeof(p_out));
    }

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

    p_out.ppt2_start = _ppt2_start;
    p_out.ppt2_exit  = _ppt2_exit;
    p_out.ppt2_next  = _ppt2_next;
    p_out.ppt2_prev  = _ppt2_prev;
    p_out.ppt2_black = _ppt2_black;
    p_out.ppt2_laser = _ppt2_laser;
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

    _ppt2_start = p_e.ppt2_start;
    _ppt2_exit  = p_e.ppt2_exit;
    _ppt2_next  = p_e.ppt2_next;
    _ppt2_prev  = p_e.ppt2_prev;
    _ppt2_black = p_e.ppt2_black;
    _ppt2_laser = p_e.ppt2_laser;

    _engine.setHardClickLock(_hardClickLock);
    _engine.setDPI(_dpiLevel);
}
