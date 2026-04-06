// =======================================================
// File: src/v0272/E10_AirMouse_0309.h
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
#include "C10_Config_0304.h"
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
    
    // safe mode gate
    volatile bool _safeMode = false;

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
    uint8_t _precision_mode = (uint8_t)EN_C10_E10_PREC_OFF; // EN_C10_E10PrecisionMode_t
    
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
    
    
    // ---- (C) OTA Guard gate ----
    volatile bool _otaGuard = false;
    uint32_t      _otaGuardCount = 0;
    uint32_t      _otaGuardT0Ms  = 0; // uptime 기준(ms)


    // ---- async requests (handled in sensor task) ----
    volatile bool _reqGyroCalib  = false;
    volatile bool _reqI2CRecover = false;
    volatile bool _reqClearDiag  = false;
    


  public:
    CL_E10_EliteAirMouse()
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

    void begin(CL_C10_Config* p_cfg) {
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

        _mutex = xSemaphoreCreateMutex();
        
        _safeMode = (_cfg && _cfg->isSafeMode());

        (void)_applyFromConfig();

        _hid.addDevice(&_keyboard);
        _hid.addDevice(&_mouse);
        _hid.begin();

        xTaskCreatePinnedToCore(_sensorTask, "E10_Sensor", 8192, this, 3, nullptr, 1);
        xTaskCreatePinnedToCore(_commTask,   "E10_Comm",   4096, this, 2, nullptr, 0);
    }

     // -----------------------
     // apply from config file (W10 apply hook)
     // -----------------------
     static bool E10_W10Apply(void* p_ctx) {
         if (!p_ctx) return false;
         return ((CL_E10_EliteAirMouse*)p_ctx)->_applyFromConfig();
     }

    // -----------------------
    // runtime apply-only (저장 없이 UI에서 반영)
    // - 정책: WiFi 제외한 E10 런타임만 즉시 반영
    // - 안전: 값 클램프 + precision/fsm 일관성 유지
    // -----------------------
    bool applyRuntimeE10(const ST_C10_E10Config_t& p_e) {
        _lock();
    
        // 1) apply to runtime with guards
        _applyE10ToRuntime(p_e);
    
        // Mega E 일관성: mode==OFF면 precision sub/FSM 흔적 제거
        if (_precision_mode == (uint8_t)EN_C10_E10_PREC_OFF) {
            _precSub = EN_PREC_OFF;
            _precSmX = 0.0f;
            _precSmY = 0.0f;
        } else {
            // mode가 켜져 있고 아직 sub가 OFF면 entry로 진입 준비
            if (_precSub == EN_PREC_OFF) {
                _precSub = EN_PREC_ENTRY;
                _precT0  = (uint32_t)millis();
                _precSmX = 0.0f;
                _precSmY = 0.0f;
            }
        }
        
    
        // 3) 적용 직후: 센서/커뮤 사이 stuck 방지용 최소 리셋(이동값은 유지)
        _state.updated = true;   // 1회는 커밋되게(버튼 diff/초기상태 동기화)
        // btn_mask는 건드리지 않음(센서가 실제 버튼 상태를 곧 갱신)
    
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
    bool setPrecisionMode(uint8_t p_mode) {
        uint8_t v_mode = p_mode;
        if (v_mode > (uint8_t)EN_C10_E10_PREC_PPT) v_mode = (uint8_t)EN_C10_E10_PREC_PPT;
    
        _lock();
    
        _precision_mode = v_mode;
    
        if (_precision_mode == (uint8_t)EN_C10_E10_PREC_OFF) {
            _precSub = EN_PREC_OFF;
            _precSmX = 0.0f;
            _precSmY = 0.0f;
        } else {
            _precSub = EN_PREC_ENTRY;
            _precT0  = (uint32_t)millis();
            _precSmX = 0.0f;
            _precSmY = 0.0f;
        }
    
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
        
        p_out.btn_mask = _state.btn_mask;
        
        p_out.safe_mode = _safeMode;
        p_out.ota_guard = _otaGuard;
        p_out.ota_guard_count = _otaGuardCount;
        
        if (_otaGuard) {
            const uint32_t v_nowMs = (uint32_t)(millis() - _uptime0);
            p_out.ota_guard_uptime_ms = (uint32_t)(v_nowMs - _otaGuardT0Ms);
        } else {
            p_out.ota_guard_uptime_ms = 0;
        }


        //p_out.precision_enable = _precisionEnable;
        p_out.precision_mode   = _precision_mode; // ✅ 이게 맞음
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
    
    bool setSafeMode(bool p_enable) {
        _lock();
        _safeMode = p_enable;
    
        // safe mode 진입 시: 상태 초기화(버튼 stuck 방지용)
        _state.btn_mask = 0;
        _state.updated  = true;
        
        _pushErr(EN_E10_ERR_OTA_GUARD, p_enable ? 2 : 3); // 값은 임의(원치 않으면 제거)
        
        _unlock();
        return true;
    }
    
    // -----------------------
    // (C) Control/Diag helpers
    // -----------------------
    
    // OTA 시작/종료 게이트
    // - enable=true : HID 완전 차단 + 버튼 stuck 방지
    // - enable=false: 정상 복귀
    bool setOtaGuard(bool p_enable) {
        _lock();
    
        if (p_enable) {
            if (!_otaGuard) {
                _otaGuard = true;
                _otaGuardCount++;
                _otaGuardT0Ms = (uint32_t)(millis() - _uptime0);
                _pushErr(EN_E10_ERR_OTA_GUARD, 1);
            }
            // stuck 방지용 상태 플래그(CommTask가 릴리즈 1회 수행)
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
    
    // 버튼 stuck 강제 해제(진단/복구용)
    // - 내부 상태를 0으로 동기화 + HID 연결 상태면 즉시 release 시도
    bool forceReleaseAllButtons() {
        uint8_t v_btn = 0;
        bool    v_conn = false;
    
        _lock();
        v_btn  = _state.btn_mask;
        v_conn = _hid.isConnected();
    
        _state.btn_mask = 0;
        _state.updated  = true;
        _unlock();
    
        // HID I/O는 mutex 밖에서
        if (v_conn) {
            if (v_btn & (uint8_t)EN_E10_BTN_LEFT)   _mouse.mouseRelease((uint8_t)EN_E10_BTN_LEFT);
            if (v_btn & (uint8_t)EN_E10_BTN_RIGHT)  _mouse.mouseRelease((uint8_t)EN_E10_BTN_RIGHT);
            if (v_btn & (uint8_t)EN_E10_BTN_MIDDLE) _mouse.mouseRelease((uint8_t)EN_E10_BTN_MIDDLE);
        }
    
        // 로그를 남기고 싶으면 전용 코드 추천(EN_E10_ERR_NONE는 비추)
        // _pushErr(EN_E10_ERR_FORCE_RELEASE, v_btn);
    
        return true;
    }
    
    // 마우스 클릭 테스트
    // - mask: EN_E10_MouseBtnMask_t OR-mask
    bool testMouseClick(uint8_t p_btnMask, uint16_t p_holdMs = 25) {
        if (!_hid.isConnected()) return false;
        if (_safeMode || _otaGuard) return false;
    
        // hold ms clamp
        uint16_t v_hold = p_holdMs;
        if (v_hold < 5)   v_hold = 5;
        if (v_hold > 250) v_hold = 250;
    
        // press
        if (p_btnMask & (uint8_t)EN_E10_BTN_LEFT)   _mouse.mousePress((uint8_t)EN_E10_BTN_LEFT);
        if (p_btnMask & (uint8_t)EN_E10_BTN_RIGHT)  _mouse.mousePress((uint8_t)EN_E10_BTN_RIGHT);
        if (p_btnMask & (uint8_t)EN_E10_BTN_MIDDLE) _mouse.mousePress((uint8_t)EN_E10_BTN_MIDDLE);
    
        vTaskDelay(pdMS_TO_TICKS(v_hold));
    
        // release
        if (p_btnMask & (uint8_t)EN_E10_BTN_LEFT)   _mouse.mouseRelease((uint8_t)EN_E10_BTN_LEFT);
        if (p_btnMask & (uint8_t)EN_E10_BTN_RIGHT)  _mouse.mouseRelease((uint8_t)EN_E10_BTN_RIGHT);
        if (p_btnMask & (uint8_t)EN_E10_BTN_MIDDLE) _mouse.mouseRelease((uint8_t)EN_E10_BTN_MIDDLE);
    
        // 내부 상태 동기화(CommTask diff로 재-press되는 것 방지)
        _lock();
        _state.btn_mask = 0;
        _state.updated  = true;
        _unlock();
    
        return true;
    }
    
    bool isSafeMode() const { 
        return _safeMode; 
    }

    // 센서태스크에서 캘리브를 수행하도록 요청(비동기)
    bool requestGyroCalibration() {
        _lock();
        _reqGyroCalib = true;
        _unlock();
        return true;
    }

    // 센서태스크에서 I2C recover 수행 요청(비동기)
    bool requestI2CRecover() {
        _lock();
        _reqI2CRecover = true;
        _unlock();
        return true;
    }

    // 진단 카운터/히스토리/통계 리셋
    bool clearDiagnostics() {
        _lock();

        _errMpuNan = 0;
        _errMutexMiss = 0;
        _errTaskOverrun = 0;

        _gyroN = 0; _gyroMean = 0.0; _gyroM2 = 0.0;
        _curN  = 0; _curMean  = 0.0; _curM2  = 0.0;

        _i2cRecoverCount  = 0;
        _i2cRecoverLastOk = true;

        memset(_errHist, 0, sizeof(_errHist));
        _errHistHead  = 0;
        _errHistCount = 0;

        memset(_spikes, 0, sizeof(_spikes));
        _spikeHead  = 0;
        _spikeCount = 0;

        _consecutiveFail = 0;
        _consecutiveRecoverFail = 0;

        // 상태도 한 번 갱신 플래그(웹 status/comm stuck 방지에 도움)
        _state.updated = true;
        
        _reqClearDiag = true;

        _unlock();
        return true;
    }
    
    // SafeMode에서도 허용: 버튼 stuck 강제 해제 + state 초기화
    bool forceReleaseButtons() {
        _lock();

        // HID 연결 여부와 무관하게 "의도상" 릴리즈 시도
        // (연결 안돼도 mouseRelease가 내부적으로 무시되거나 안전해야 함)
        _mouse.mouseRelease((uint8_t)EN_E10_BTN_LEFT);
        _mouse.mouseRelease((uint8_t)EN_E10_BTN_RIGHT);
        _mouse.mouseRelease((uint8_t)EN_E10_BTN_MIDDLE);

        _state.btn_mask = 0;
        _state.x = 0;
        _state.y = 0;
        _state.wheel = 0;
        _state.updated = true;

        _unlock();
        return true;
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
    
    // -----------------------
    // apply cfg -> runtime (guarded)
    // -----------------------
    void _applyE10ToRuntime(const ST_C10_E10Config_t& p_e) {
        // dpi clamp
        int v_dpi = (int)p_e.dpi_level;
        if (v_dpi < 1) v_dpi = 1;
        if (v_dpi > 3) v_dpi = 3;
        _dpiLevel = v_dpi;
    
        _hardClickLock = p_e.hard_click_lock;
    
        for (int v_i = 0; v_i < 3; v_i++) {
            // scale/accel sanity(너무 과격한 값 방지: 최소 0, 최대 적당치)
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
    
        // precision
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
        
        // mode clamp
        if (_precision_mode > (uint8_t)EN_C10_E10_PREC_PPT) _precision_mode = (uint8_t)EN_C10_E10_PREC_PPT;
        
        // Mega E: mode==OFF면 sub OFF
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
    
        // ppt keys
        _ppt2_start = p_e.ppt2_start;
        _ppt2_exit  = p_e.ppt2_exit;
        _ppt2_next  = p_e.ppt2_next;
        _ppt2_prev  = p_e.ppt2_prev;
        _ppt2_black = p_e.ppt2_black;
        _ppt2_laser = p_e.ppt2_laser;
    
        // engine apply
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
        // Mega E: sub가 OFF면 적용 안 함(=mode OFF 또는 scroll 중)
        if (_precSub == EN_PREC_OFF) return;
    
        // mode -> profile (0..4)
        const uint8_t v_mode = (uint8_t)min((uint8_t)EN_C10_E10_PREC_PPT, _precision_mode);
        const ST_E10_PrecProfile_t& v_pf = G_E10_PREC_PROFILES[v_mode];
    
        // deadzone
        if (fabsf(p_fx) < _precDeadzone) p_fx = 0.0f;
        if (fabsf(p_fy) < _precDeadzone) p_fy = 0.0f;
    
        auto v_shape = [&](float p_v) -> float {
            float v_a = fabsf(p_v);
            if (v_a < 0.0001f) return 0.0f;
    
            float v_n   = min(1.0f, v_a / (float)_precMaxStep);
            float v_b   = v_n + (_precAccel * v_n * v_n);
            float v_out = v_b * (float)_precMaxStep;
    
            // gain: user gain * profile gain
            v_out *= (_precGain * v_pf.gain);
    
            return (p_v >= 0.0f) ? v_out : -v_out;
        };
    
        float v_tx = constrain(v_shape(p_fx), -(float)_precMaxStep, (float)_precMaxStep);
        float v_ty = constrain(v_shape(p_fy), -(float)_precMaxStep, (float)_precMaxStep);
    
        // smoothing: base smooth vs profile alpha(0..255)
        const float v_pfSmooth = (float)v_pf.alpha / 255.0f; // 0..1
        float v_sm = _precSmooth;
        if (v_pfSmooth > v_sm) v_sm = v_pfSmooth;
    
        // (선택) profile1(joystick-like) 보정 유지
        if (_precProfile == 1) {
            v_sm = min(0.95f, max(0.60f, v_sm));
        }
    
        // accel_limit(PPT): 프레임 간 변화량 제한(급변 억제)
        if (v_pf.accel_limit > 0.0f) {
            const float v_dx = v_tx - _precSmX;
            const float v_dy = v_ty - _precSmY;
            const float v_lim = v_pf.accel_limit;
    
            if (fabsf(v_dx) > v_lim) v_tx = _precSmX + (v_dx > 0 ? v_lim : -v_lim);
            if (fabsf(v_dy) > v_lim) v_ty = _precSmY + (v_dy > 0 ? v_lim : -v_lim);
        }
    
        _precSmX = _precSmX * v_sm + v_tx * (1.0f - v_sm);
        _precSmY = _precSmY * v_sm + v_ty * (1.0f - v_sm);
    
        p_fx = _precSmX;
        p_fy = _precSmY;
    }


    // -----------------------
    // FSM update
    // -----------------------
    void _fsmUpdate(bool p_btnScroll, bool p_btnModeLongToggle, float p_gyroAbs) {
        if (p_btnModeLongToggle) {
            _isPptMode = !_isPptMode;
        }
    
        // 1) scroll 최우선
        if (p_btnScroll) {
            _fsm = EN_FSM_SCROLL;
            _precSub = EN_PREC_OFF; // scroll 중엔 precision 오버레이 해제(정책)
            _precSmX = 0.0f;
            _precSmY = 0.0f;
            return;
        }
    
        // 2) base fsm
        _fsm = _isPptMode ? EN_FSM_PPT : EN_FSM_AIR;
    
        // 3) precision overlay (mode != OFF)
        if (_precision_mode == (uint8_t)EN_C10_E10_PREC_OFF) {
            _precSub = EN_PREC_OFF;
            return;
        }
    
        const uint32_t v_now = (uint32_t)millis();
    
        if (_precSub == EN_PREC_OFF) {
            _precSub = EN_PREC_ENTRY;
            _precT0  = v_now;
            return;
        }
    
        if (_precSub == EN_PREC_ENTRY) {
            if (p_gyroAbs <= _precEntryStillDeg) {
                if (v_now - _precT0 >= _precEntryMs) _precSub = EN_PREC_TRACK;
            } else {
                _precT0 = v_now;
            }
            return;
        }
    
        if (_precSub == EN_PREC_TRACK) {
            if (p_gyroAbs >= _precExitMoveDeg) {
                _precSub = EN_PREC_EXIT;
                _precT0  = v_now;
            }
            return;
        }
    
        if (_precSub == EN_PREC_EXIT) {
            if (p_gyroAbs <= _precEntryStillDeg) {
                if (v_now - _precT0 >= _precExitMs) _precSub = EN_PREC_TRACK;
            } else {
                _precT0 = v_now;
            }
            return;
        }
    
        // fallback
        _precSub = EN_PREC_ENTRY;
        _precT0  = v_now;
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
        
        pinMode(E10_CONST::PIN_I2C_SDA, INPUT_PULLUP);
        pinMode(E10_CONST::PIN_I2C_SCL, OUTPUT_OPEN_DRAIN);

        for (int v_i = 0; v_i < 9; v_i++) {
            digitalWrite(E10_CONST::PIN_I2C_SCL, HIGH);
            delayMicroseconds(6);
            digitalWrite(E10_CONST::PIN_I2C_SCL, LOW);
            delayMicroseconds(6);
        }
        digitalWrite(E10_CONST::PIN_I2C_SCL, HIGH);
        delayMicroseconds(6);

        Wire.end();
        delay(5);
        
        Wire.begin(E10_CONST::PIN_I2C_SDA, E10_CONST::PIN_I2C_SCL);
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
    
        TickType_t     v_lastWake  = xTaskGetTickCount();
        unsigned long  v_lastUs    = micros();
        unsigned long  v_btnDownMs = 0;
    
        // 버튼 변화 감지(커밋 성공 시에만 last 갱신)
        uint8_t v_lastBtnMask = 0;
        bool    v_lastBtnInit = false;
    
        if (!v_m->_gyroCalibDone) v_m->_runGyroCalibration();
    
        for (;;) {
            // ---- async requests (handled only here) ----
            // =================================================
            // Control requests (from /api/control)
            // - 반드시 센서태스크에서 처리해서 I2C/MPU 충돌 방지
            // =================================================
            if (v_m->_reqClearDiag) {
                v_m->_reqClearDiag = false;

                // 진단값 초기화
                v_m->_lock();
                v_m->_errMpuNan      = 0;
                v_m->_errMutexMiss   = 0;
                v_m->_errTaskOverrun = 0;

                v_m->_gyroN = 0; v_m->_gyroMean = 0.0; v_m->_gyroM2 = 0.0;
                v_m->_curN  = 0; v_m->_curMean  = 0.0; v_m->_curM2  = 0.0;

                v_m->_i2cRecoverCount  = 0;
                v_m->_i2cRecoverLastOk = true;

                v_m->_errHistHead  = 0;
                v_m->_errHistCount = 0;
                memset(v_m->_errHist, 0, sizeof(v_m->_errHist));

                v_m->_spikeHead  = 0;
                v_m->_spikeCount = 0;
                memset(v_m->_spikes, 0, sizeof(v_m->_spikes));

                v_m->_consecutiveFail        = 0;
                v_m->_consecutiveRecoverFail = 0;

                v_m->_unlock();

                v_m->_pushErr(EN_E10_ERR_NONE, 0);
            }

            if (v_m->_reqI2CRecover) {
                v_m->_reqI2CRecover = false;
                (void)v_m->_recoverI2C();
            }

            if (v_m->_reqGyroCalib) {
                v_m->_reqGyroCalib = false;

                // 캘리브 다시
                v_m->_gyroCalibDone = false;
                v_m->_runGyroCalibration();
                v_m->_gyroCalibDone = true;
            }
    
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
    
            const bool v_leftClick  = (digitalRead(E10_CONST::PIN_BTN_L) == LOW);
            const bool v_rightClick = (digitalRead(E10_CONST::PIN_BTN_R) == LOW);
            const bool v_midClick   = (digitalRead(E10_CONST::PIN_BTN_M) == LOW);
    
            // 기존 정책 유지: 클릭 notify는 LEFT 기준
            if (v_leftClick) v_m->_engine.notifyClick();
    
            uint8_t v_btnMask = 0;
            if (v_leftClick)  v_btnMask |= (uint8_t)EN_E10_BTN_LEFT;
            if (v_rightClick) v_btnMask |= (uint8_t)EN_E10_BTN_RIGHT;
            if (v_midClick)   v_btnMask |= (uint8_t)EN_E10_BTN_MIDDLE;
    
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
                    const int16_t v_xo = (int16_t)constrain((int)v_fx, -32767, 32767);
                    const int16_t v_yo = (int16_t)constrain((int)v_fy, -32767, 32767);
                    const int16_t v_wo = (int16_t)constrain((int)v_wheel, -32767, 32767);
    
                    // btn change detect는 "커밋 성공 시에만" last 갱신
                    bool v_btnChanged = false;
                    if (!v_lastBtnInit) {
                        v_lastBtnInit = true;
                        v_btnChanged  = true; // 첫 1회 동기화
                    } else if (v_btnMask != v_lastBtnMask) {
                        v_btnChanged = true;
                    }
    
                    v_m->_state.x        = v_xo;
                    v_m->_state.y        = v_yo;
                    v_m->_state.wheel    = v_wo;
                    v_m->_state.btn_mask = v_btnMask;
    
                    // updated 정책: 버튼/이동/휠 중 하나라도 변화면 true
                    if (v_btnChanged || (v_xo != 0) || (v_yo != 0) || (v_wo != 0)) {
                        v_m->_state.updated = true;
                    }
    
                    // 성공 커밋 시에만 갱신
                    v_lastBtnMask = v_btnMask;
    
                    xSemaphoreGive(v_m->_mutex);
                } else {
                    v_m->_errMutexMiss++;
                    v_m->_pushErr(EN_E10_ERR_MUTEX_MISS, 0);
    
                    // 버튼 변화가 있었다면 짧게 1회 재시도 (stuck 방지용)
                    // - last 갱신도 "성공 시에만"
                    const bool v_btnNeedRetry = (!v_lastBtnInit) || (v_btnMask != v_lastBtnMask);
                    if (v_btnNeedRetry) {
                        if (xSemaphoreTake(v_m->_mutex, pdMS_TO_TICKS(2)) == pdTRUE) {
                            v_m->_state.btn_mask = v_btnMask;
                            v_m->_state.updated  = true;
    
                            v_lastBtnInit = true;
                            v_lastBtnMask = v_btnMask;
    
                            xSemaphoreGive(v_m->_mutex);
                        }
                    }
                }
            } else {
                // AIR/PPT/PREC cursor
                if (v_m->_precSub != EN_PREC_OFF) {
                    v_m->_applyPrecision(v_fx, v_fy);
                }
                
                if (v_m->_fsm == EN_FSM_PPT) {
                    // (C) Safe/OTA gate 중엔 키 전송 금지
                    if (!(v_m->_safeMode || v_m->_otaGuard)) {              // ✅ 추가
                        v_m->_processGesturesDeg(v_gz);
                    }
                }
    
                v_m->_welfordAdd(v_m->_curN, v_m->_curMean, v_m->_curM2,
                                 (double)sqrtf(v_fx * v_fx + v_fy * v_fy));
    
                if (xSemaphoreTake(v_m->_mutex, 0) == pdTRUE) {
                    const int16_t v_xo = (int16_t)constrain((int)v_fx, -32767, 32767);
                    const int16_t v_yo = (int16_t)constrain((int)v_fy, -32767, 32767);
    
                    // btn change detect는 "커밋 성공 시에만" last 갱신
                    bool v_btnChanged = false;
                    if (!v_lastBtnInit) {
                        v_lastBtnInit = true;
                        v_btnChanged  = true; // 첫 1회 동기화
                    } else if (v_btnMask != v_lastBtnMask) {
                        v_btnChanged = true;
                    }
    
                    v_m->_state.x        = v_xo;
                    v_m->_state.y        = v_yo;
                    v_m->_state.wheel    = 0;
                    v_m->_state.btn_mask = v_btnMask;
    
                    // updated 정책: 버튼/이동/휠 중 하나라도 변화면 true
                    if (v_btnChanged || (v_xo != 0) || (v_yo != 0)) {
                        v_m->_state.updated = true;
                    }
    
                    // 성공 커밋 시에만 갱신
                    v_lastBtnMask = v_btnMask;
    
                    xSemaphoreGive(v_m->_mutex);
                } else {
                    v_m->_errMutexMiss++;
                    v_m->_pushErr(EN_E10_ERR_MUTEX_MISS, 0);
    
                    // 버튼 변화가 있었다면 짧게 1회 재시도 (stuck 방지용)
                    const bool v_btnNeedRetry = (!v_lastBtnInit) || (v_btnMask != v_lastBtnMask);
                    if (v_btnNeedRetry) {
                        if (xSemaphoreTake(v_m->_mutex, pdMS_TO_TICKS(2)) == pdTRUE) {
                            v_m->_state.btn_mask = v_btnMask;
                            v_m->_state.updated  = true;
    
                            v_lastBtnInit = true;
                            v_lastBtnMask = v_btnMask;
    
                            xSemaphoreGive(v_m->_mutex);
                        }
                    }
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
    
    // -----------------------
    // Task: Comm (HID send)
    // - SafeMode일 때 HID 출력 완전 차단
    // - btn_mask 전체(LEFT/RIGHT/MIDDLE) 지원
    // - 변경 시에만 press/release (스팸/지터 방지)
    // - 핵심 보강: updated=false여도 "버튼 변화(diff)"는 무조건 전송(버튼 stuck 방지)
    // -----------------------
    static void _commTask(void* p_pv) {
        CL_E10_EliteAirMouse* v_m = (CL_E10_EliteAirMouse*)p_pv;
    
        // 이전 버튼 상태(변경 감지용)
        uint8_t v_lastBtnMask    = 0;
        bool    v_releasedOnSafe = false;
    
        bool v_prevConn = false;
    
        for (;;) {
            const bool v_conn = v_m->_hid.isConnected();
    
            // disconnect edge: stuck 방지 릴리즈(강추)
            if (!v_conn && v_prevConn) {
                if (v_lastBtnMask & (uint8_t)EN_E10_BTN_LEFT)   v_m->_mouse.mouseRelease((uint8_t)EN_E10_BTN_LEFT);
                if (v_lastBtnMask & (uint8_t)EN_E10_BTN_RIGHT)  v_m->_mouse.mouseRelease((uint8_t)EN_E10_BTN_RIGHT);
                if (v_lastBtnMask & (uint8_t)EN_E10_BTN_MIDDLE) v_m->_mouse.mouseRelease((uint8_t)EN_E10_BTN_MIDDLE);
                v_lastBtnMask = 0;
            }
    
            if (!v_conn) {
                v_prevConn = false;
                vTaskDelay(pdMS_TO_TICKS(12));
                continue;
            }
    
            // connect edge: 1회만 동기화
            if (v_conn && !v_prevConn) {
                if (xSemaphoreTake(v_m->_mutex, portMAX_DELAY) == pdTRUE) {
                    v_lastBtnMask = v_m->_state.btn_mask;
                    // (선택) connect 시 잔여 updated/이동값이 남아있으면 초기화해도 됨
                    // v_m->_state.updated = false;
                    xSemaphoreGive(v_m->_mutex);
                }
            }
            v_prevConn = true;
    
            // ---- Gate(SafeMode or OTA Guard): HID 출력 차단 + stuck 방지 1회 릴리즈 ----
            const bool v_gate = (v_m->_safeMode || v_m->_otaGuard);
            
            if (v_gate) {
                if (!v_releasedOnSafe) {
                    if (v_lastBtnMask & (uint8_t)EN_E10_BTN_LEFT)   v_m->_mouse.mouseRelease((uint8_t)EN_E10_BTN_LEFT);
                    if (v_lastBtnMask & (uint8_t)EN_E10_BTN_RIGHT)  v_m->_mouse.mouseRelease((uint8_t)EN_E10_BTN_RIGHT);
                    if (v_lastBtnMask & (uint8_t)EN_E10_BTN_MIDDLE) v_m->_mouse.mouseRelease((uint8_t)EN_E10_BTN_MIDDLE);
            
                    v_lastBtnMask    = 0;
                    v_releasedOnSafe = true;
                }
            
                if (xSemaphoreTake(v_m->_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    v_m->_state.updated  = false;
                    v_m->_state.btn_mask = 0;
                    xSemaphoreGive(v_m->_mutex);
                }
            
                vTaskDelay(pdMS_TO_TICKS(20));
                continue;
            } else {
                v_releasedOnSafe = false;
            }
    
            // ---- normal path ----
            if (xSemaphoreTake(v_m->_mutex, portMAX_DELAY) == pdTRUE) {
                // snapshot (항상: 버튼 diff를 updated와 무관하게 처리하기 위해)
                const bool    v_upd   = v_m->_state.updated;
                const int16_t v_x     = v_m->_state.x;
                const int16_t v_y     = v_m->_state.y;
                const int16_t v_wheel = v_m->_state.wheel;
                const uint8_t v_btn   = v_m->_state.btn_mask;
    
                // 소비(이동/휠/버튼은 sensor가 계속 갱신, updated만 내림)
                v_m->_state.updated = false;
                xSemaphoreGive(v_m->_mutex);
    
                // ---- button diff: updated 여부와 무관하게 처리(버튼 stuck 방지 핵심) ----
                const uint8_t v_changed = (uint8_t)(v_btn ^ v_lastBtnMask);
    
                if (v_changed & (uint8_t)EN_E10_BTN_LEFT) {
                    if (v_btn & (uint8_t)EN_E10_BTN_LEFT) v_m->_mouse.mousePress((uint8_t)EN_E10_BTN_LEFT);
                    else                                  v_m->_mouse.mouseRelease((uint8_t)EN_E10_BTN_LEFT);
                }
                if (v_changed & (uint8_t)EN_E10_BTN_RIGHT) {
                    if (v_btn & (uint8_t)EN_E10_BTN_RIGHT) v_m->_mouse.mousePress((uint8_t)EN_E10_BTN_RIGHT);
                    else                                   v_m->_mouse.mouseRelease((uint8_t)EN_E10_BTN_RIGHT);
                }
                if (v_changed & (uint8_t)EN_E10_BTN_MIDDLE) {
                    if (v_btn & (uint8_t)EN_E10_BTN_MIDDLE) v_m->_mouse.mousePress((uint8_t)EN_E10_BTN_MIDDLE);
                    else                                    v_m->_mouse.mouseRelease((uint8_t)EN_E10_BTN_MIDDLE);
                }
    
                v_lastBtnMask = v_btn;
    
                // ---- move/wheel: updated==true일 때만 전송 (스팸/지터 방지) ----
                if (v_upd) {
                    // clamp to HID range
                    const int8_t v_dx = (int8_t)constrain((int)v_x, -127, 127);
                    const int8_t v_dy = (int8_t)constrain((int)v_y, -127, 127);
                    const int8_t v_wh = (int8_t)constrain((int)v_wheel, -127, 127);
    
                    _mouseSend(v_m->_mouse, v_dx, v_dy, v_wh);
                }
            }
    
            vTaskDelay(pdMS_TO_TICKS(7));
        }
    }

};
