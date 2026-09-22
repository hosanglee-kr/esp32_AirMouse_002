// =======================================================
// File: E10_AirMouse_0320.h
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
 * [Phase 1 반영]
 *  - C-1: sensorTask 정상 경로에서 _pushFrame() 호출 (HID 전달 큐 활성화)
 *  - C-2: _pushErr/_pushSpike 락 보호 + recursive mutex
 *  - C-5: 캘리브 중 프레임 유지(강제 릴리즈 + 버튼 샘플링)
 *
 * [분할]
 *  - E10_AirMouse_Core_0320.cpp   : 초기화/런타임 적용
 *  - E10_AirMouse_Hid_0320.cpp    : HID 출력/테스트/강제 릴리즈
 *  - E10_AirMouse_Motion_0320.cpp : precision/FSM
 *  - E10_AirMouse_Diag_0320.cpp   : 진단/캘리브/status
 *  - E10_AirMouse_Task_0320.cpp   : sensorTask / commTask
 */

#include <Arduino.h>
#include <Wire.h>
#include <math.h>

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#include <BleCompositeHID.h>
#include <KeyboardDevice.h>
#include <MouseDevice.h>

#include "A40_ComFunc_0320.h"
#include "C10_Config_0320.h"
#include "M10_MotionProc_0320.h"

#include "E10_Def_0320.h"

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

    // ----------------------------------------------------
    // Frame Queue (sensor -> comm)
    // ----------------------------------------------------
    typedef struct ST_E10_Frame_t {
        int16_t x;
        int16_t y;
        int16_t wheel;
        uint8_t btn_mask;
        bool    updated;
    } ST_E10_Frame_t;

    QueueHandle_t _qFrame = nullptr;

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
    uint8_t _precision_mode = (uint8_t)EN_C10_E10_PREC_OFF;

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
    uint8_t  _precProfile       = 1;

    // PPT v2
    ST_C10_PptKey2_t _ppt2_start;
    ST_C10_PptKey2_t _ppt2_exit;
    ST_C10_PptKey2_t _ppt2_next;
    ST_C10_PptKey2_t _ppt2_prev;
    ST_C10_PptKey2_t _ppt2_black;
    ST_C10_PptKey2_t _ppt2_laser;

    // 런타임 적용 경로 단일화용 snapshot
    ST_C10_E10Config_t _cfgE10Runtime;
    bool               _cfgE10RuntimeValid = false;

    float    _tempC   = 0.0f;
    uint32_t _uptime0 = 0;
    float    _dtAvgMs = 8.0f;

    TaskHandle_t _thSensor = nullptr;
    TaskHandle_t _thComm   = nullptr;

    uint32_t _stackSensorMinWords = 0;
    uint32_t _stackCommMinWords   = 0;

    float    _dtMaxMs = 0.0f;
    uint32_t _dtOverrunCount = 0;

    float    _commDtAvgMs = 7.0f;
    float    _commDtMaxMs = 0.0f;
    uint32_t _commOverrunCount = 0;

    uint32_t _failsafeReleaseCount = 0;

    // mode toggle cooldown
    uint32_t _lastModeToggleMs = 0;
    uint16_t _modeToggleCooldownMs = 1500;

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

    // OTA Guard gate
    volatile bool _otaGuard = false;
    uint32_t      _otaGuardCount = 0;
    uint32_t      _otaGuardT0Ms  = 0;

    // async requests
    volatile bool _reqGyroCalib  = false;
    volatile bool _reqI2CRecover = false;
    volatile bool _reqClearDiag  = false;

  public:
    CL_E10_EliteAirMouse();

    // -------- lifecycle --------
    void begin(CL_C10_Config* p_cfg);

    // -------- W10 hooks --------
    static bool E10_W10Apply(void* p_ctx);

    // -------- runtime apply (no persist) --------
    bool applyRuntimeE10(const ST_C10_E10Config_t& p_e);

    // -------- control --------
    bool setPptMode(bool p_enable);
    bool setDpiLevel(uint8_t p_level);
    bool setPrecisionMode(uint8_t p_mode);
    bool setHardClickLock(bool p_enable);
    bool setSafeMode(bool p_enable);
    bool setOtaGuard(bool p_enable);

    bool isSafeMode() const { return _safeMode; }

    // -------- diagnostics / control --------
    void getStatus(ST_E10_Status_t& p_out);
    bool requestGyroCalibration();
    bool requestI2CRecover();
    bool clearDiagnostics();

    // 버튼 stuck 강제 해제(진단/복구용)
    bool forceReleaseButtons();
    bool forceReleaseAllButtons();

    // -------- test --------
    bool testPptKey2(uint8_t p_page, uint8_t p_mod, uint32_t p_code);
    bool testMouseClick(uint8_t p_btnMask, uint16_t p_holdMs = 25);

  private:
    // -----------------------
    // Config apply
    // -----------------------
    bool _applyFromConfig();
    void _getE10RuntimeConfig(ST_C10_E10Config_t& p_out);
    void _snapshotRuntimeToE10Config(ST_C10_E10Config_t& p_out);
    void _applyE10ToRuntime(const ST_C10_E10Config_t& p_e);

    // -----------------------
    // HID helpers
    // -----------------------
    void _tapComboUsageKb(uint8_t p_modMask, uint8_t p_usage, uint16_t p_ms = 22);
    void _tapUsageKb(uint8_t p_usage, uint16_t p_ms = 12);
    void _tapConsumerMask(uint32_t p_mask, uint16_t p_ms = 28);
    void _sendPptKey2(uint8_t p_page, uint8_t p_mod, uint32_t p_code);
    void _sendPptKey2FromCfg(const ST_C10_PptKey2_t& p_k);
    void _processGesturesDeg(float p_gzDeg);

    // -----------------------
    // Motion helpers
    // -----------------------
    void _applyPrecision(float& p_fx, float& p_fy);
    void _fsmUpdate(bool p_btnScroll, bool p_btnModeLongToggle, float p_gyroAbs);

    // -----------------------
    // Diagnostics helpers
    // -----------------------
    void _pushErr(uint8_t p_code, uint16_t p_value = 0);
    void _pushSpike(uint32_t p_tsMs);
    bool _recoverI2C();
    void _runGyroCalibration();

    // -----------------------
    // inline utils
    // -----------------------
    static void _mouseSend(MouseDevice& p_ms, int8_t p_dx, int8_t p_dy, int8_t p_wheel) {
        p_ms.mouseMove(p_dx, p_dy, p_wheel, 0);
    }

    void _pushFrame(const ST_E10_Frame_t& p_fr) {
        if (!_qFrame) return;
        xQueueOverwrite(_qFrame, &p_fr);
    }

    void _lock() {
        // [C-2] recursive mutex
        if (_mutex) (void)xSemaphoreTakeRecursive(_mutex, portMAX_DELAY);
    }

    void _unlock() {
        if (_mutex) xSemaphoreGiveRecursive(_mutex);
    }

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

    // -----------------------
    // Tasks
    // -----------------------
    static void _sensorTask(void* p_pv);
    static void _commTask(void* p_pv);
};
