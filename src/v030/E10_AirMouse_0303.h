// =======================================================
// File: src/v0272/E10_AirMouse_0303.h
// =======================================================
#pragma once
/*
 * (022 구조 유지 가정)
 * v0272~0301:
 *  - Motion FSM 강화(Scroll/PPT/Precision)
 *  - Precision FSM: entry/track/exit + profile(joystick-like)
 *  - HID modifier 정책 확정: mod mask == HID modifier byte (W10와 1:1)
 *  - status: gyro/cursor RMS + spike + consecutive fail + err hist + i2c recover
 *
 * [이번 정리]
 *  - 명명규칙 준수:
 *    - private 멤버 함수: '_' 접두사 적용, '_' 접미사 제거
 *    - 함수 로컬 변수: v_ 접두사
 *    - 함수 인자: p_ 접두사
 *    - static class member: s_ 접두사
 *  - 고정 상수(핀/캘리브/임계/히스토리 cap 등): E10_Def_0301.h(E10_CONST) 사용
 */

#include <Arduino.h>
#include <Wire.h>
#include <math.h>

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#include <BleCompositeHID.h>
#include <KeyboardDevice.h>
#include <MouseDevice.h>

#include "A40_ComFunc_070.h"
#include "C10_Config_0302.h"
#include "M10_MotionProc_0300.h"

#include "E10_Def_0302.h"

class CL_E10_EliteAirMouse {
  private:
    Adafruit_MPU6050 _mpu;

    BleCompositeHID _hid;
    KeyboardDevice  _keyboard;
    MouseDevice     _mouse;

    CL_M10_AdvancedMotionProcessor _engine;
    CL_C10_Config*                 _cfg = nullptr;

    ST_E10_State_t _state;

    SemaphoreHandle_t _mutex = nullptr;

    // gyro calib
    float _gyroBiasX = 0.0f;
    float _gyroBiasY = 0.0f;
    float _gyroBiasZ = 0.0f;
    bool  _gyroCalibDone = false;

    // runtime config (applied)
    volatile bool _isPptMode = false;
    int           _dpiLevel  = 2;
    bool          _hardClickLock = true;

    float _scaleBase[3] = {0.55f, 0.75f, 1.0f};
    float _accelGain[3] = {0.35f, 0.55f, 0.85f};
    float _accelTh      = 8.0f;

    float _wheelThDeg   = 90.0f;
    int   _wheelStepMax = 6;

    float    _gestureFlickDeg   = 200.0f;
    uint16_t _gestureCooldownMs = 600;

    float _scrollCursorDamp = 0.25f;

    // precision config + fsm knobs
    bool    _precisionEnable = false;
    bool    _precisionMode   = false;
    float   _precDeadzone    = 1.2f;
    float   _precGain        = 0.65f;
    float   _precAccel       = 0.25f;
    uint8_t _precMaxStep     = 18;
    float   _precSmooth      = 0.85f;
    float   _precSmX         = 0.0f;
    float   _precSmY         = 0.0f;
    uint16_t _precEntryMs       = 180;
    uint16_t _precExitMs        = 160;
    float    _precEntryStillDeg = 2.2f;
    float    _precExitMoveDeg   = 7.5f;
    uint8_t  _precProfile       = 1; // 1=joystick-like

    // PPT v2
    ST_C10_PptKey2_t _ppt2_start;
    ST_C10_PptKey2_t _ppt2_exit;
    ST_C10_PptKey2_t _ppt2_next;
    ST_C10_PptKey2_t _ppt2_prev;
    ST_C10_PptKey2_t _ppt2_black;
    ST_C10_PptKey2_t _ppt2_laser;

    float    _tempC   = 0.0f;
    uint32_t _uptime0 = 0;
    float    _dtAvgMs = 8.0f;

    // errors
    uint32_t _errMpuNan      = 0;
    uint32_t _errMutexMiss   = 0;
    uint32_t _errTaskOverrun = 0;

    // RMS Welford
    uint32_t _gyroN    = 0;
    double   _gyroMean = 0.0;
    double   _gyroM2   = 0.0;

    uint32_t _curN    = 0;
    double   _curMean = 0.0;
    double   _curM2   = 0.0;

    // i2c recover
    uint32_t _i2cRecoverCount  = 0;
    bool     _i2cRecoverLastOk = true;

    // history ring
    ST_E10_ErrEvt_t _errHist[E10_CONST::ERR_HIST_CAP];
    uint8_t         _errHistHead  = 0;
    uint8_t         _errHistCount = 0;

    // anomaly ring
    ST_E10_SpikeEvt_t _spikes[32];
    uint8_t           _spikeHead  = 0;
    uint8_t           _spikeCount = 0;

    uint16_t _consecutiveFail        = 0;
    uint16_t _consecutiveRecoverFail = 0;

    // FSM
    uint8_t  _fsm     = EN_FSM_AIR;
    uint8_t  _precSub = EN_PREC_OFF;
    uint32_t _precT0  = 0;

    // class-static fixed pins (board wiring)
    static constexpr int s_i2cSda = 4;
    static constexpr int s_i2cScl = 5;

  public:
    CL_E10_EliteAirMouse()
        : _hid("Elite AirMouse S3", "ProMaker", 100) { 
            
        memset(&_state, 0, sizeof(_state));
        // _state = {0,0,0,0,false};
        // _state = {0, 0, 0, false};

        memset(_errHist, 0, sizeof(_errHist));
        memset(_spikes,  0, sizeof(_spikes));

        memset(&_ppt2_start, 0, sizeof(_ppt2_start));
        memset(&_ppt2_exit,  0, sizeof(_ppt2_exit));
        memset(&_ppt2_next,  0, sizeof(_ppt2_next));
        memset(&_ppt2_prev,  0, sizeof(_ppt2_prev));
        memset(&_ppt2_black, 0, sizeof(_ppt2_black));
        memset(&_ppt2_laser, 0, sizeof(_ppt2_laser));
    }

    void begin(CL_C10_Config* p_cfg) {
        _cfg     = p_cfg;
        _uptime0 = millis();

        Serial.begin(115200);

        Wire.begin(s_i2cSda, s_i2cScl);
        Wire.setClock(400000);

        if (!_mpu.begin()) {
            Serial.println("[E10] MPU begin fail");
            for (;;) delay(10);
        }
        _mpu.setGyroRange(MPU6050_RANGE_250_DEG);
        _mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
        _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

        pinMode(E10_CONST::PIN_BTN_L,      INPUT_PULLUP);
        pinMode(E10_CONST::PIN_BTN_MODE,   INPUT_PULLUP);
        pinMode(E10_CONST::PIN_BTN_SCROLL, INPUT_PULLUP);

        _mutex = xSemaphoreCreateMutex();

        (void)_applyFromConfig();

        _hid.addDevice(&_keyboard);
        _hid.addDevice(&_mouse);
        _hid.begin();

        xTaskCreatePinnedToCore(_sensorTask, "E10_Sensor", 8192, this, 3, nullptr, 1);
        xTaskCreatePinnedToCore(_commTask,   "E10_Comm",   4096, this, 2, nullptr, 0);
    }

    // W10 apply hook
    static bool E10_W10Apply(void* p_ctx) {
        if (!p_ctx) return false;
        return ((CL_E10_EliteAirMouse*)p_ctx)->_applyFromConfig();
    }

    // 런타임 apply-only (저장 없이 UI에서 반영)
    bool applyRuntimeE10(const ST_C10_E10Config_t& p_e) {
        _lock();
        _applyE10ToRuntime(p_e);
        _unlock();
        return true;
    }

    // control
    bool setPptMode(bool p_enable) {
        _lock();
        _isPptMode = p_enable;
        _unlock();
        return true;
    }

    bool setDpiLevel(uint8_t p_level) {
        uint8_t v_lv = p_level;
        if (v_lv < 1) v_lv = 1;
        if (v_lv > 3) v_lv = 3;

        _lock();
        _dpiLevel = (int)v_lv;
        _engine.setDPI(_dpiLevel);
        _unlock();
        return true;
    }

    bool setPrecisionMode(bool p_enable) {
        _lock();

        if (!_precisionEnable) {
            _precisionMode = false;
            _fsm           = EN_FSM_AIR;
            _precSub       = EN_PREC_OFF;
            _precSmX       = 0.0f;
            _precSmY       = 0.0f;
            _unlock();
            return true;
        }

        _precisionMode = p_enable;
        if (_precisionMode) {
            _fsm     = EN_FSM_PREC;
            _precSub = EN_PREC_ENTRY;
            _precT0  = (uint32_t)millis();
        } else {
            _fsm     = (_isPptMode ? EN_FSM_PPT : EN_FSM_AIR);
            _precSub = EN_PREC_OFF;
        }

        _precSmX = 0.0f;
        _precSmY = 0.0f;

        _unlock();
        return true;
    }

    // /api/ppt/test
    bool testPptKey2(uint8_t p_page, uint8_t p_mod, uint32_t p_code) {
        if (!_hid.isConnected()) return false;
        _sendPptKey2(p_page, p_mod, p_code);
        return true;
    }

    void getStatus(ST_E10_Status_t& p_out) {
        memset(&p_out, 0, sizeof(p_out));
        _lock();

        p_out.ble_connected = _hid.isConnected();
        p_out.ppt_mode      = _isPptMode;
        p_out.dpi_level     = (uint8_t)_dpiLevel;

        p_out.precision_enable = _precisionEnable;
        p_out.precision_mode   = _precisionMode;
        p_out.fsm_state        = _fsm;
        p_out.fsm_sub          = _precSub;

        p_out.gyro_bias_x = _gyroBiasX;
        p_out.gyro_bias_y = _gyroBiasY;
        p_out.gyro_bias_z = _gyroBiasZ;
        p_out.temp_c      = _tempC;

        p_out.sampling_ms_target = 8;
        p_out.sampling_ms_avg    = _dtAvgMs;

        p_out.i2c_recover_count   = _i2cRecoverCount;
        p_out.i2c_recover_last_ok = _i2cRecoverLastOk;

        p_out.err_mpu_nan      = _errMpuNan;
        p_out.err_mutex_miss   = _errMutexMiss;
        p_out.err_task_overrun = _errTaskOverrun;

        p_out.uptime_ms = (uint32_t)(millis() - _uptime0);

        p_out.gyro_rms   = _calcRms(_gyroN, _gyroM2);
        p_out.cursor_rms = _calcRms(_curN,  _curM2);

        // anomaly snapshot (최근 10초 spike)
        const uint32_t v_nowMs = (uint32_t)(millis() - _uptime0);
        uint16_t v_sc = 0;
        for (uint8_t v_i = 0; v_i < _spikeCount; v_i++) {
            int v_idx = (int)_spikeHead - 1 - (int)v_i;
            if (v_idx < 0) v_idx += 32;

            if (v_nowMs - _spikes[v_idx].ts_ms <= 10000) v_sc++;
            else break;
        }

        p_out.spike_count_10s          = v_sc;
        p_out.consecutive_fail         = _consecutiveFail;
        p_out.consecutive_recover_fail = _consecutiveRecoverFail;

        // health score
        uint16_t v_score = 1000;
        v_score = (uint16_t)max(0, (int)v_score - (int)(p_out.gyro_rms * 25.0f));
        v_score = (uint16_t)max(0, (int)v_score - (int)(p_out.cursor_rms * 18.0f));
        v_score = (uint16_t)max(0, (int)v_score - (int)min((uint32_t)400, p_out.err_mpu_nan * 20));
        v_score = (uint16_t)max(0, (int)v_score - (int)min((uint32_t)300, p_out.i2c_recover_count * 35));
        v_score = (uint16_t)max(0, (int)v_score - (int)min((uint32_t)300, (uint32_t)p_out.spike_count_10s * 12));
        v_score = (uint16_t)max(0, (int)v_score - (int)min((uint32_t)400, (uint32_t)p_out.consecutive_fail * 18));

        p_out.health_score = v_score;
        p_out.health       = (v_score >= 820) ? (uint8_t)EN_E10_HEALTH_OK
                        : ((v_score >= 620) ? (uint8_t)EN_E10_HEALTH_WARN : (uint8_t)EN_E10_HEALTH_DEGRADED);

        // history newest-first
        p_out.err_hist_n = (uint8_t)min((uint8_t)E10_CONST::ERR_HIST_CAP, _errHistCount);
        for (uint8_t v_i = 0; v_i < p_out.err_hist_n; v_i++) {
            int v_idx = (int)_errHistHead - 1 - (int)v_i;
            if (v_idx < 0) v_idx += (int)E10_CONST::ERR_HIST_CAP;
            p_out.err_hist[v_i] = _errHist[v_idx];
        }

        _unlock();
    }

  private:
    // -----------------------
    // Config apply
    // -----------------------
    bool _applyFromConfig() {
        if (!_cfg) return false;

        ST_C10_WiFiConfig_t v_w;
        ST_C10_E10Config_t  v_e;

        _cfg->makeDefaultsWiFi(v_w);
        _cfg->makeDefaultsE10(v_e);
        (void)_cfg->loadAll(v_w, v_e);

        _lock();
        _applyE10ToRuntime(v_e);
        _unlock();
        return true;
    }

    void _applyE10ToRuntime(const ST_C10_E10Config_t& p_e) {
        _dpiLevel      = (int)p_e.dpi_level;
        _hardClickLock = p_e.hard_click_lock;

        for (int v_i = 0; v_i < 3; v_i++) {
            _scaleBase[v_i] = p_e.scale_base[v_i];
            _accelGain[v_i] = p_e.accel_gain[v_i];
        }
        _accelTh = p_e.accel_threshold;

        _wheelThDeg        = p_e.wheel_threshold_deg;
        _wheelStepMax      = (int)p_e.wheel_step_max;
        _gestureFlickDeg   = p_e.gesture_flick_deg;
        _gestureCooldownMs = p_e.gesture_cooldown_ms;
        _scrollCursorDamp  = p_e.scroll_cursor_damp;

        _precisionEnable   = p_e.precision_enable;
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

        if (!_precisionEnable) {
            _precisionMode = false;
            _precSub       = EN_PREC_OFF;
            _fsm           = _isPptMode ? EN_FSM_PPT : EN_FSM_AIR;
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

    // -----------------------
    // Modifier policy
    // -----------------------
    // p_modMask == HID report modifier byte (W10 mods mask와 1:1)
    void _tapComboUsageKb(uint8_t p_modMask, uint8_t p_usage, uint16_t p_ms = 22) {
        if (p_usage == 0) return;
        if (p_modMask) _keyboard.modifierKeyPress(p_modMask);
        _keyboard.keyPress(p_usage);
        vTaskDelay(pdMS_TO_TICKS(p_ms));
        _keyboard.keyRelease(p_usage);
        if (p_modMask) _keyboard.modifierKeyRelease(p_modMask);
    }

    void _tapUsageKb(uint8_t p_usage, uint16_t p_ms = 12) {
        if (p_usage == 0) return;
        _keyboard.keyPress(p_usage);
        vTaskDelay(pdMS_TO_TICKS(p_ms));
        _keyboard.keyRelease(p_usage);
    }

    void _tapConsumerMask(uint32_t p_mask, uint16_t p_ms = 28) {
        if (p_mask == 0) return;
        _keyboard.mediaKeyPress(p_mask);
        vTaskDelay(pdMS_TO_TICKS(p_ms));
        _keyboard.mediaKeyRelease(p_mask);
    }

    void _sendPptKey2(uint8_t p_page, uint8_t p_mod, uint32_t p_code) {
        if (p_page == (uint8_t)EN_C10_KEYPAGE_CONSUMER) {
            _tapConsumerMask(p_code);
            return;
        }

        uint8_t v_usage = (uint8_t)min((uint32_t)0xE7, p_code);
        if (p_mod) _tapComboUsageKb(p_mod, v_usage);
        else       _tapUsageKb(v_usage);
    }

    void _sendPptKey2FromCfg(const ST_C10_PptKey2_t& p_k) {
        _sendPptKey2(p_k.page, p_k.mod, p_k.code);
    }

    void _processGesturesDeg(float p_gzDeg) {
        static unsigned long s_lastMs = 0;
        if (millis() - s_lastMs < _gestureCooldownMs) return;

        if (p_gzDeg > _gestureFlickDeg) {
            _sendPptKey2FromCfg(_ppt2_prev);
            s_lastMs = millis();
        } else if (p_gzDeg < -_gestureFlickDeg) {
            _sendPptKey2FromCfg(_ppt2_next);
            s_lastMs = millis();
        }
    }

    // -----------------------
    // Precision shaping (joystick-like)
    // -----------------------
    void _applyPrecision(float& p_fx, float& p_fy) {
        if (!_precisionMode) return;

        if (fabsf(p_fx) < _precDeadzone) p_fx = 0.0f;
        if (fabsf(p_fy) < _precDeadzone) p_fy = 0.0f;

        auto v_shape = [&](float p_v) -> float {
            float v_a = fabsf(p_v);
            if (v_a < 0.0001f) return 0.0f;

            float v_n   = min(1.0f, v_a / (float)_precMaxStep);
            float v_b   = v_n + (_precAccel * v_n * v_n);
            float v_out = v_b * (float)_precMaxStep;
            v_out      *= _precGain;

            return (p_v >= 0.0f) ? v_out : -v_out;
        };

        float v_tx = constrain(v_shape(p_fx), -(float)_precMaxStep, (float)_precMaxStep);
        float v_ty = constrain(v_shape(p_fy), -(float)_precMaxStep, (float)_precMaxStep);

        // profile 1: joystick-like = 더 강한 smoothing + 작은 움직임 유지
        float v_sm = (_precProfile == 1) ? min(0.95f, max(0.60f, _precSmooth)) : _precSmooth;

        _precSmX = _precSmX * v_sm + v_tx * (1.0f - v_sm);
        _precSmY = _precSmY * v_sm + v_ty * (1.0f - v_sm);
        p_fx     = _precSmX;
        p_fy     = _precSmY;
    }

    // -----------------------
    // FSM update
    // -----------------------
    void _fsmUpdate(bool p_btnScroll, bool p_btnModeLongToggle, float p_gyroAbs) {
        (void)p_gyroAbs;

        // ppt toggle is handled in sensor task (btnModeLongToggle)
        if (p_btnModeLongToggle) {
            _isPptMode = !_isPptMode;
        }

        // highest priority: scroll
        if (p_btnScroll) {
            _fsm = EN_FSM_SCROLL;
            if (_precisionMode) {
                _precisionMode = false;
                _precSub       = EN_PREC_OFF;
            }
            return;
        }

        // precision (if enabled and user toggled precision_mode via api/control)
        if (_precisionMode && _precisionEnable) {
            _fsm = EN_FSM_PREC;

            const uint32_t v_now = (uint32_t)millis();
            if (_precSub == EN_PREC_ENTRY) {
                if (p_gyroAbs <= _precEntryStillDeg) {
                    if (v_now - _precT0 >= _precEntryMs) {
                        _precSub = EN_PREC_TRACK;
                    }
                } else {
                    _precT0 = v_now;
                }
            } else if (_precSub == EN_PREC_TRACK) {
                if (p_gyroAbs >= _precExitMoveDeg) {
                    _precSub = EN_PREC_EXIT;
                    _precT0  = v_now;
                }
            } else if (_precSub == EN_PREC_EXIT) {
                if (p_gyroAbs <= _precEntryStillDeg) {
                    if (v_now - _precT0 >= _precExitMs) {
                        _precSub = EN_PREC_TRACK; // 안정되면 다시 track
                    }
                } else {
                    _precT0 = v_now;
                }
            } else {
                _precSub = EN_PREC_ENTRY;
                _precT0  = v_now;
            }
            return;
        }

        _precSub = EN_PREC_OFF;
        _fsm     = _isPptMode ? EN_FSM_PPT : EN_FSM_AIR;
    }

    // -----------------------
    // Stats helpers
    // -----------------------
    void _welfordAdd(uint32_t& p_n, double& p_mean, double& p_m2, double p_x) {
        p_n++;
        double v_d  = p_x - p_mean;
        p_mean     += v_d / (double)p_n;
        double v_d2 = p_x - p_mean;
        p_m2       += v_d * v_d2;

        if (p_n > 2500) {
            p_n    = 1;
            p_mean = p_x;
            p_m2   = 0.0;
        }
    }

    float _calcRms(uint32_t p_n, double p_m2) {
        if (p_n < 2) return 0.0f;
        double v_var = p_m2 / (double)(p_n - 1);
        if (v_var < 0.0) v_var = 0.0;
        return (float)sqrt(v_var);
    }

    void _pushErr(uint8_t p_code, uint16_t p_value = 0) {
        ST_E10_ErrEvt_t v_e;
        v_e.ts_ms = (uint32_t)(millis() - _uptime0);
        v_e.code  = p_code;
        v_e.value = p_value;

        _errHist[_errHistHead] = v_e;
        _errHistHead = (uint8_t)((_errHistHead + 1) % E10_CONST::ERR_HIST_CAP);
        if (_errHistCount < E10_CONST::ERR_HIST_CAP) _errHistCount++;
    }

    void _pushSpike(uint32_t p_tsMs) {
        _spikes[_spikeHead].ts_ms = p_tsMs;
        _spikeHead = (uint8_t)((_spikeHead + 1) % 32);
        if (_spikeCount < 32) _spikeCount++;
    }

    // -----------------------
    // I2C recover
    // -----------------------
    bool _recoverI2C() {
        const int v_sda = s_i2cSda;
        const int v_scl = s_i2cScl;

        pinMode(v_sda, INPUT_PULLUP);
        pinMode(v_scl, OUTPUT_OPEN_DRAIN);

        for (int v_i = 0; v_i < 9; v_i++) {
            digitalWrite(v_scl, HIGH);
            delayMicroseconds(6);
            digitalWrite(v_scl, LOW);
            delayMicroseconds(6);
        }
        digitalWrite(v_scl, HIGH);
        delayMicroseconds(6);

        Wire.end();
        delay(5);
        Wire.begin(v_sda, v_scl);
        Wire.setClock(400000);
        delay(5);

        bool v_ok = _mpu.begin();
        if (v_ok) {
            _mpu.setGyroRange(MPU6050_RANGE_250_DEG);
            _mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
            _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
        }

        _i2cRecoverCount++;
        _i2cRecoverLastOk = v_ok;

        if (v_ok) {
            _consecutiveRecoverFail = 0;
        } else {
            _consecutiveRecoverFail++;
            _consecutiveFail++;
        }

        _pushErr(v_ok ? EN_E10_ERR_I2C_RECOVER_OK : EN_E10_ERR_I2C_RECOVER_FAIL, 0);
        return v_ok;
    }

    // -----------------------
    // Calibration
    // -----------------------
    void _runGyroCalibration() {
        const uint32_t v_t0 = millis();
        uint32_t v_cnt = 0;

        double v_sx = 0.0;
        double v_sy = 0.0;
        double v_sz = 0.0;

        while (millis() - v_t0 < E10_CONST::CALIB_MS) {
            sensors_event_t v_a, v_g, v_t;
            _mpu.getEvent(&v_a, &v_g, &v_t);

            float v_gx = v_g.gyro.x * RAD_TO_DEG;
            float v_gy = v_g.gyro.y * RAD_TO_DEG;
            float v_gz = v_g.gyro.z * RAD_TO_DEG;

            float v_m = max(max(fabsf(v_gx), fabsf(v_gy)), fabsf(v_gz));
            if (v_m < E10_CONST::CALIB_STILL_TH) {
                v_sx += v_gx;
                v_sy += v_gy;
                v_sz += v_gz;
                v_cnt++;
            }
            vTaskDelay(pdMS_TO_TICKS(5));
        }

        if (v_cnt > 0) {
            _gyroBiasX = (float)(v_sx / v_cnt);
            _gyroBiasY = (float)(v_sy / v_cnt);
            _gyroBiasZ = (float)(v_sz / v_cnt);
        }

        _gyroCalibDone = true;
    }

    // -----------------------
    // Mouse send
    // -----------------------
    static void _mouseSend(MouseDevice& p_ms, int8_t p_dx, int8_t p_dy, int8_t p_wheel) {
        p_ms.mouseMove(p_dx, p_dy, p_wheel, 0);
    }

    // -----------------------
    // Mutex helpers
    // -----------------------
    void _lock() {
        if (_mutex) (void)xSemaphoreTake(_mutex, portMAX_DELAY);
    }

    void _unlock() {
        if (_mutex) xSemaphoreGive(_mutex);
    }


    // -----------------------
    // Tasks
    // -----------------------
    static void _sensorTask(void* p_pv) {
        CL_E10_EliteAirMouse* v_m = (CL_E10_EliteAirMouse*)p_pv;
    
        TickType_t     v_lastWake   = xTaskGetTickCount();
        unsigned long  v_lastUs     = micros();
        unsigned long  v_btnDownMs  = 0;
    
        if (!v_m->_gyroCalibDone) v_m->_runGyroCalibration();
    
        for (;;) {
            // ---- sensor read ----
            sensors_event_t v_a, v_g, v_t;
            v_m->_mpu.getEvent(&v_a, &v_g, &v_t);
            v_m->_tempC = v_t.temperature;
    
            // ---- dt ----
            unsigned long v_nowUs = micros();
            float v_dt = (v_nowUs - v_lastUs) / 1000000.0f;
            v_lastUs = v_nowUs;
    
            float v_dtMs = v_dt * 1000.0f;
            v_m->_dtAvgMs = v_m->_dtAvgMs * 0.98f + v_dtMs * 0.02f;
    
            // ---- buttons ----
            const bool v_scrollMode = (digitalRead(E10_CONST::PIN_BTN_SCROLL) == LOW);
    
            // mode short/long (short=dpi cycle, long=ppt toggle)
            bool v_modeLongToggle = false;
            if (digitalRead(E10_CONST::PIN_BTN_MODE) == LOW) {
                if (v_btnDownMs == 0) v_btnDownMs = millis();
            } else {
                if (v_btnDownMs > 0) {
                    unsigned long v_hold = millis() - v_btnDownMs;
                    if (v_hold > 1000) {
                        v_modeLongToggle = true;
                    } else {
                        v_m->_dpiLevel++;
                        if (v_m->_dpiLevel > 3) v_m->_dpiLevel = 1;
                        v_m->_engine.setDPI(v_m->_dpiLevel);
                    }
                    v_btnDownMs = 0;
                }
            }
    
            const bool v_leftClick = (digitalRead(E10_CONST::PIN_BTN_L) == LOW);
            if (v_leftClick) v_m->_engine.notifyClick();
    
            uint8_t v_btnMask = 0;
            if (v_leftClick) v_btnMask |= (uint8_t)EN_E10_BTN_LEFT;
    
            // ---- gyro ----
            float v_gx = (v_g.gyro.x * RAD_TO_DEG) - v_m->_gyroBiasX;
            float v_gy = (v_g.gyro.y * RAD_TO_DEG) - v_m->_gyroBiasY;
            float v_gz = (v_g.gyro.z * RAD_TO_DEG) - v_m->_gyroBiasZ;
    
            const float v_gyroAbs = max(max(fabsf(v_gx), fabsf(v_gy)), fabsf(v_gz));
    
            // ---- spike detect ----
            const uint32_t v_ts = (uint32_t)(millis() - v_m->_uptime0);
            if (fabsf(v_gz) > E10_CONST::SPIKE_TH_DEG) v_m->_pushSpike(v_ts);
    
            // ---- fsm ----
            v_m->_fsmUpdate(v_scrollMode, v_modeLongToggle, v_gyroAbs);
    
            // ---- NaN guard ----
            if (isnan(v_gx) || isnan(v_gy) || isnan(v_gz)) {
                v_m->_errMpuNan++;
                v_m->_consecutiveFail++;
                v_m->_pushErr(EN_E10_ERR_MPU_NAN, 0);
    
                if ((v_m->_errMpuNan % 5) == 0) (void)v_m->_recoverI2C();
    
                vTaskDelayUntil(&v_lastWake, pdMS_TO_TICKS(8));
                continue;
            } else {
                if (v_m->_consecutiveFail > 0) v_m->_consecutiveFail--;
            }
    
            // ---- stats + motion engine ----
            v_m->_welfordAdd(v_m->_gyroN, v_m->_gyroMean, v_m->_gyroM2, (double)v_gz);
    
            v_m->_engine.updateOrientation(v_a.acceleration.y, v_a.acceleration.z, v_gx, v_dt);
    
            int v_tx = 0;
            int v_ty = 0;
            v_m->_engine.process(-v_gz, -v_gx, v_tx, v_ty);
    
            // ---- accel shaping ----
            float v_base = v_m->_scaleBase[v_m->_dpiLevel - 1];
            float v_accg = v_m->_accelGain[v_m->_dpiLevel - 1];
    
            float v_mag = sqrtf((float)v_tx * (float)v_tx + (float)v_ty * (float)v_ty);
            float v_acc = 1.0f;
    
            if (v_mag > v_m->_accelTh) {
                float v_ex = (v_mag - v_m->_accelTh);
                v_acc = 1.0f + (v_accg * (v_ex / (v_ex + 18.0f)));
            }
    
            float v_fx = (float)v_tx * v_base * v_acc;
            float v_fy = (float)v_ty * v_base * v_acc;
    
            // ---- apply FSM ----
            if (v_m->_fsm == EN_FSM_SCROLL) {
                // scroll: wheel only + cursor damp
                int v_wheel = 0;
    
                if (v_gy > v_m->_wheelThDeg) {
                    float v_n = (v_gy - v_m->_wheelThDeg) / 120.0f;
                    if (v_n > 1.0f) v_n = 1.0f;
                    v_wheel = (int)(1 + (v_n * (v_m->_wheelStepMax - 1)));
                } else if (v_gy < -v_m->_wheelThDeg) {
                    float v_n = (-v_gy - v_m->_wheelThDeg) / 120.0f;
                    if (v_n > 1.0f) v_n = 1.0f;
                    v_wheel = -(int)(1 + (v_n * (v_m->_wheelStepMax - 1)));
                }
    
                v_fx *= v_m->_scrollCursorDamp;
                v_fy *= v_m->_scrollCursorDamp;
    
                if (xSemaphoreTake(v_m->_mutex, 0) == pdTRUE) {
                    v_m->_state.x        = (int16_t)constrain((int)v_fx, -32767, 32767);
                    v_m->_state.y        = (int16_t)constrain((int)v_fy, -32767, 32767);
                    v_m->_state.wheel    = (int16_t)constrain((int)v_wheel, -32767, 32767);
                    v_m->_state.btn_mask = v_btnMask;
                    v_m->_state.updated  = true;
                    xSemaphoreGive(v_m->_mutex);
                } else {
                    v_m->_errMutexMiss++;
                    v_m->_pushErr(EN_E10_ERR_MUTEX_MISS, 0);
                }
            } else {
                // AIR/PPT/PREC cursor
                if (v_m->_fsm == EN_FSM_PREC) {
                    v_m->_applyPrecision(v_fx, v_fy);
                }
                if (v_m->_fsm == EN_FSM_PPT) {
                    v_m->_processGesturesDeg(v_gz);
                }
    
                v_m->_welfordAdd(v_m->_curN, v_m->_curMean, v_m->_curM2, (double)sqrtf(v_fx * v_fx + v_fy * v_fy));
    
                if (xSemaphoreTake(v_m->_mutex, 0) == pdTRUE) {
                    v_m->_state.x        = (int16_t)constrain((int)v_fx, -32767, 32767);
                    v_m->_state.y        = (int16_t)constrain((int)v_fy, -32767, 32767);
                    v_m->_state.wheel    = 0;
                    v_m->_state.btn_mask = v_btnMask;
                    v_m->_state.updated  = true;
                    xSemaphoreGive(v_m->_mutex);
                } else {
                    v_m->_errMutexMiss++;
                    v_m->_pushErr(EN_E10_ERR_MUTEX_MISS, 0);
                }
            }
    
            // ---- pacing / overrun ----
            TickType_t v_before = xTaskGetTickCount();
            vTaskDelayUntil(&v_lastWake, pdMS_TO_TICKS(8));
            TickType_t v_after = xTaskGetTickCount();
    
            if ((v_after - v_before) == 0) {
                v_m->_errTaskOverrun++;
                if ((v_m->_errTaskOverrun % 10) == 0) v_m->_pushErr(EN_E10_ERR_TASK_OVERRUN, 0);
            }
        }
    }

    static void _commTask(void* p_pv) {
        CL_E10_EliteAirMouse* v_m = (CL_E10_EliteAirMouse*)p_pv;
    
        for (;;) {
            if (v_m->_hid.isConnected() && xSemaphoreTake(v_m->_mutex, portMAX_DELAY) == pdTRUE) {
                if (v_m->_state.updated) {
                    const int8_t v_dx = (int8_t)constrain((int)v_m->_state.x, -127, 127);
                    const int8_t v_dy = (int8_t)constrain((int)v_m->_state.y, -127, 127);
                    const int8_t v_wh = (int8_t)constrain((int)v_m->_state.wheel, -127, 127);
    
                    const bool v_left = ((v_m->_state.btn_mask & (uint8_t)EN_E10_BTN_LEFT) != 0);
    
                    if (v_left) v_m->_mouse.mousePress(E10_CONST::MOUSE_BTN_LEFT);
                    else        v_m->_mouse.mouseRelease(E10_CONST::MOUSE_BTN_LEFT);
    
                    _mouseSend(v_m->_mouse, v_dx, v_dy, v_wh);
    
                    v_m->_state.updated = false;
                }
                xSemaphoreGive(v_m->_mutex);
            }
            vTaskDelay(pdMS_TO_TICKS(7));
        }
    }

    
    /*
    static void _sensorTask(void* p_pv) {
        CL_E10_EliteAirMouse* v_m = (CL_E10_EliteAirMouse*)p_pv;

        TickType_t v_lastWake = xTaskGetTickCount();
        unsigned long v_lastUs = micros();
        unsigned long v_btnDownMs = 0;

        if (!v_m->_gyroCalibDone) v_m->_runGyroCalibration();

        for (;;) {
            sensors_event_t v_a, v_g, v_t;
            v_m->_mpu.getEvent(&v_a, &v_g, &v_t);
            v_m->_tempC = v_t.temperature;

            unsigned long v_nowUs = micros();
            float v_dt = (v_nowUs - v_lastUs) / 1000000.0f;
            v_lastUs = v_nowUs;

            float v_dtMs = v_dt * 1000.0f;
            v_m->_dtAvgMs = v_m->_dtAvgMs * 0.98f + v_dtMs * 0.02f;

            const bool v_scrollMode = (digitalRead(E10_CONST::PIN_BTN_SCROLL) == LOW);

            // mode short/long
            bool v_modeLongToggle = false;
            if (digitalRead(E10_CONST::PIN_BTN_MODE) == LOW) {
                if (v_btnDownMs == 0) v_btnDownMs = millis();
            } else {
                if (v_btnDownMs > 0) {
                    unsigned long v_hold = millis() - v_btnDownMs;
                    if (v_hold > 1000) {
                        v_modeLongToggle = true;
                    } else {
                        v_m->_dpiLevel++;
                        if (v_m->_dpiLevel > 3) v_m->_dpiLevel = 1;
                        v_m->_engine.setDPI(v_m->_dpiLevel);
                    }
                    v_btnDownMs = 0;
                }
            }

            float v_gx = (v_g.gyro.x * RAD_TO_DEG) - v_m->_gyroBiasX;
            float v_gy = (v_g.gyro.y * RAD_TO_DEG) - v_m->_gyroBiasY;
            float v_gz = (v_g.gyro.z * RAD_TO_DEG) - v_m->_gyroBiasZ;

            const float v_gyroAbs = max(max(fabsf(v_gx), fabsf(v_gy)), fabsf(v_gz));

            // spike
            const uint32_t v_ts = (uint32_t)(millis() - v_m->_uptime0);
            if (fabsf(v_gz) > E10_CONST::SPIKE_TH_DEG) v_m->_pushSpike(v_ts);

            // FSM 결정
            v_m->_fsmUpdate(v_scrollMode, v_modeLongToggle, v_gyroAbs);

            if (isnan(v_gx) || isnan(v_gy) || isnan(v_gz)) {
                v_m->_errMpuNan++;
                v_m->_consecutiveFail++;
                v_m->_pushErr(EN_E10_ERR_MPU_NAN, 0);

                if ((v_m->_errMpuNan % 5) == 0) (void)v_m->_recoverI2C();

                vTaskDelayUntil(&v_lastWake, pdMS_TO_TICKS(8));
                continue;
            } else {
                if (v_m->_consecutiveFail > 0) v_m->_consecutiveFail--;
            }

            v_m->_welfordAdd(v_m->_gyroN, v_m->_gyroMean, v_m->_gyroM2, (double)v_gz);

            v_m->_engine.updateOrientation(v_a.acceleration.y, v_a.acceleration.z, v_gx, v_dt);

            int v_tx = 0;
            int v_ty = 0;
            v_m->_engine.process(-v_gz, -v_gx, v_tx, v_ty);

            const bool v_leftClick = (digitalRead(E10_CONST::PIN_BTN_L) == LOW);
            if (v_leftClick) v_m->_engine.notifyClick();

            // accel shaping
            float v_base = v_m->_scaleBase[v_m->_dpiLevel - 1];
            float v_accg = v_m->_accelGain[v_m->_dpiLevel - 1];

            float v_mag = sqrtf((float)v_tx * (float)v_tx + (float)v_ty * (float)v_ty);
            float v_acc = 1.0f;

            if (v_mag > v_m->_accelTh) {
                float v_ex = (v_mag - v_m->_accelTh);
                v_acc = 1.0f + (v_accg * (v_ex / (v_ex + 18.0f)));
            }

            float v_fx = (float)v_tx * v_base * v_acc;
            float v_fy = (float)v_ty * v_base * v_acc;

            // FSM 적용
            if (v_m->_fsm == EN_FSM_SCROLL) {
                // scroll: wheel only, cursor damp
                int v_wheel = 0;

                if (v_gy > v_m->_wheelThDeg) {
                    float v_n = (v_gy - v_m->_wheelThDeg) / 120.0f;
                    if (v_n > 1.0f) v_n = 1.0f;
                    v_wheel = (int)(1 + (v_n * (v_m->_wheelStepMax - 1)));
                } else if (v_gy < -v_m->_wheelThDeg) {
                    float v_n = (-v_gy - v_m->_wheelThDeg) / 120.0f;
                    if (v_n > 1.0f) v_n = 1.0f;
                    v_wheel = -(int)(1 + (v_n * (v_m->_wheelStepMax - 1)));
                }

                v_fx *= v_m->_scrollCursorDamp;
                v_fy *= v_m->_scrollCursorDamp;

                if (xSemaphoreTake(v_m->_mutex, 0) == pdTRUE) {
                    m->_state.x = (int16_t)constrain((int)fx, -32767, 32767);
                    m->_state.y = (int16_t)constrain((int)fy, -32767, 32767);
                    m->_state.wheel = (int16_t)constrain((int)wheel, -32767, 32767);
                    m->_state.btn_mask = v_btnMask;
                    m->_state.updated = true;
                    xSemaphoreGive(m->_mutex);
    
                } else {
                    v_m->_errMutexMiss++;
                    v_m->_pushErr(EN_E10_ERR_MUTEX_MISS, 0);
                }
            } else {
                // AIR/PPT/PREC cursor
                if (v_m->_fsm == EN_FSM_PREC) {
                    v_m->_applyPrecision(v_fx, v_fy);
                }
                if (v_m->_fsm == EN_FSM_PPT) {
                    v_m->_processGesturesDeg(v_gz);
                }

                v_m->_welfordAdd(v_m->_curN, v_m->_curMean, v_m->_curM2, (double)sqrtf(v_fx * v_fx + v_fy * v_fy));

                if (xSemaphoreTake(v_m->_mutex, 0) == pdTRUE) {
                    v_m->_state.x       = (int)v_fx;
                    v_m->_state.y       = (int)v_fy;
                    v_m->_state.wheel   = 0;
                    v_m->_state.updated = true;
                    xSemaphoreGive(v_m->_mutex);
                } else {
                    v_m->_errMutexMiss++;
                    v_m->_pushErr(EN_E10_ERR_MUTEX_MISS, 0);
                }
            }
            
            // 여기 있던 mousePress/mouseRelease 즉시처리는 제거(아래 commTask로 이동)
            // if (v_m->_hid.isConnected()) {
            //     if (v_leftClick) v_m->_mouse.mousePress(E10_CONST::MOUSE_BTN_LEFT);
            //     else             v_m->_mouse.mouseRelease(E10_CONST::MOUSE_BTN_LEFT);
            // }

            TickType_t v_before = xTaskGetTickCount();
            vTaskDelayUntil(&v_lastWake, pdMS_TO_TICKS(8));
            TickType_t v_after = xTaskGetTickCount();

            // (기존 로직 유지) 오버런/지연 감지용 카운트
            if ((v_after - v_before) == 0) {
                v_m->_errTaskOverrun++;
                if ((v_m->_errTaskOverrun % 10) == 0) v_m->_pushErr(EN_E10_ERR_TASK_OVERRUN, 0);
            }
        }
    }

    static void _commTask(void* p_pv) {
        CL_E10_EliteAirMouse* v_m = (CL_E10_EliteAirMouse*)p_pv;

        for (;;) {
            if (v_m->_hid.isConnected() && xSemaphoreTake(v_m->_mutex, portMAX_DELAY) == pdTRUE) {
                if (v_m->_state.updated) {
                    int8_t v_dx = (int8_t)constrain((int)m->_state.x, -127, 127);
                    int8_t v_dy = (int8_t)constrain((int)m->_state.y, -127, 127);
                    int8_t v_wh = (int8_t)constrain((int)m->_state.wheel, -127, 127);
                
                    const bool v_left = ((m->_state.btn_mask & (uint8_t)EN_E10_BTN_LEFT) != 0);
                
                    if (v_left) m->_mouse.mousePress(E10_CONST::MOUSE_BTN_LEFT);
                    else        m->_mouse.mouseRelease(E10_CONST::MOUSE_BTN_LEFT);
                
                    mouseSend_(m->_mouse, v_dx, v_dy, v_wh);
                
                    m->_state.updated = false;
    
                }
                xSemaphoreGive(v_m->_mutex);
            }
            vTaskDelay(pdMS_TO_TICKS(7));
        }
    }
    */
};
