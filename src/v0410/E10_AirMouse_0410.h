// =======================================================
// File: E10_AirMouse_0410.h
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
 * [Phase 1]
 *  - C-1: sensorTask 정상 경로에서 _pushFrame() 호출 (HID 전달 큐 활성화)
 *  - C-2: _pushErr/_pushSpike 락 보호 + recursive mutex
 *  - C-5: 캘리브 중 프레임 유지(강제 릴리즈 + 버튼 샘플링)
 *
 * [Phase 2]
 *  - C-3: HID 실행을 commTask 단독으로 (웹/sensor는 _qHidCmd enqueue)
 *  - H-2: forceReleaseButtons/forceReleaseAllButtons 통일
 *  - H-4: testPptKey2/testMouseClick 논블로킹
 *
 * [Phase 3]
 *  - H-3: setDpiLevel/setPrecisionMode/setHardClickLock RMW 원자화
 *  - C-4: sensorTask loop 시작에 motion-critical config 스냅샷
 *
 * [Phase 4]
 *  - M-2: _recoverI2C 카운터 락 통일
 *  - M-4: SAFE/OTA gate 진입·이탈 전용 에러코드 (E10_Def_0410.h)
 *
 * [분할]
 *  - E10_AirMouse_Core_0410.cpp   : 초기화/런타임 적용
 *  - E10_AirMouse_Hid_0410.cpp    : HID 출력/테스트/강제 릴리즈
 *  - E10_AirMouse_Motion_0410.cpp : precision/FSM
 *  - E10_AirMouse_Diag_0410.cpp   : 진단/캘리브/status
 *  - E10_AirMouse_Task_0410.cpp   : sensorTask / commTask
 */

#include <Arduino.h>
#include <Wire.h>
#include <math.h>

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#include <BleCompositeHID.h>
#include <KeyboardDevice.h>
#include <MouseDevice.h>

#include "A40_ComFunc_0410.h"
#include "C10_Config_0410.h"
#include "M10_MotionProc_0410.h"

#include "E10_Def_0410.h"

#include "M20_BiasTracker_0410.h"

#include "C20_BtnDispatcher_0410.h"
#include "C20_ActionExec_0410.h"

#include "M30_Gesture_0410.h"

#include "B20_Ble_0410.h"

#include "P20_Power_0410.h"

#include "L10_Led_0410.h"


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
        int16_t pan;        // [Front Hold] AC Pan (수평 스크롤)
        uint8_t btn_mask;
        bool    updated;
    } ST_E10_Frame_t;

    QueueHandle_t _qFrame = nullptr;

    // ----------------------------------------------------
    // HID Command Queue (Phase 2: C-3 / H-2 / H-4)
    // - 웹/sensor 태스크가 _mouse/_keyboard를 직접 만지지 않는다
    // - commTask가 유일한 HID 실행자
    // ----------------------------------------------------
    enum EN_E10_HidCmd_t : uint8_t {
        EN_E10_HIDCMD_NONE        = 0,
        EN_E10_HIDCMD_RELEASE_ALL = 1,
        EN_E10_HIDCMD_TEST_CLICK  = 2,
        EN_E10_HIDCMD_TEST_PPT    = 3,
    };

    typedef struct ST_E10_HidCmd_t {
        uint8_t  cmd;
        uint8_t  arg0;    // mouse mask or key page
        uint8_t  arg1;    // key mod
        uint16_t holdMs;  // TEST_CLICK hold
        uint32_t code;    // TEST_PPT code
    } ST_E10_HidCmd_t;

    QueueHandle_t _qHidCmd = nullptr;

    // gyro calib
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

    // ====================================================
    // [v0410] 활성 프로파일 스냅샷
    //   _cfgProfile.e10    : E10 파라미터 (기존 _cfgE10Runtime에 해당)
    //   _cfgProfile.slots  : Global + Mode Override 슬롯 매트릭스
    //   _cfgProfile.macros : 매크로 라이브러리 (최대 8×8)
    // ====================================================
    ST_C10_ProfileConfig_t _cfgProfile;
    bool                   _cfgProfileValid = false;

    // [C-3/H-1] 매크로 상태머신 + 취소 토큰
    //  - _macroAbort(bool) → _macroAbortToken(uint32)로 변경 (재실행 초기화 경합 제거)
    //  - commTask 블로킹 제거: 스텝 단위 상태머신
    volatile uint32_t _macroAbortToken = 0;
    
    struct ST_MacroState_t {
        // [a-1] 다중 태스크(sensor/comm/web)에서 write됨 → volatile 명시
        //       (실제 취소 판정은 _macroAbortToken 카운터가 담당)
        volatile bool active;
        uint8_t  macroIdx;
        uint8_t  stepIdx;
        uint32_t stepStartMs;
        uint32_t startToken;
    };
    ST_MacroState_t _macroState = {};

    // [H-4] 실행 시점 매크로 스냅샷 (락 유지 시간 최소화 + OOB 방지)
    ST_C10_Macro_t _macroSnapshot = {};
    
    // [H-3] 리셋 위임 플래그
    volatile bool _reqResetBtnDisp = false;
    volatile bool _reqResetGesture = false;
    

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
    volatile bool    _reqGyroCalib      = false;
    volatile bool    _reqI2CRecover     = false;
    volatile bool    _reqClearDiag      = false;
    volatile uint8_t _reqSpecialAction  = 0;      // [REQ-FIX-02] Special action delegation (Web -> sensorTask)
    volatile bool    _reqCommReleaseAll = false;  // [REQ-FIX-03] HID release delegation (Profile switch -> commTask)
    
    // [Phase 10] config 저장 필요 플래그 (main loop에서 처리)
    volatile bool _reqSaveCfg = false;
    
    // ====================================================
    // [Phase 6-J] LED 컨트롤러
    // ====================================================
    CL_L10_Led _led;
    
    // ====================================================
    // [Phase 10] BLE Manager (Pairing / Bonds)
    // ====================================================
    CL_B20_Ble           _ble;
    
    // ====================================================
    // [Phase 8 / 11.6] Power Manager (Light-sleep + WoM)
    // ====================================================
    CL_P20_Power         _power;
    volatile bool        _powerNotifyPending = false;   // [C-1] 커서 이동 deferred activity

    // ====================================================
    // [Phase 4] Zero-rate Bias Tracker
    // ====================================================
    CL_M20_BiasTracker   _biasTracker;
    
    // ====================================================
    // [Phase 7] 제스처 감지기 (Flick / Linear / Tilt)
    // ====================================================
    CL_M30_Gesture       _gesture;

    // ====================================================
    // [Phase 5] 버튼 디스패처 + 액션 실행기
    // ====================================================
    CL_C20_BtnDispatcher _btnDisp;
    CL_C20_ActionExec    _actExec;

    // 슬롯 실행 커맨드 큐 (sensorTask → commTask)
    struct ST_ActionCmd_t {
        ST_C20_ActionSlot_t slot;
        bool                isDown;
    };
    QueueHandle_t _qActionExec = nullptr;

    // 활성 모드 (1/2/3)
    volatile uint8_t _activeMode = 1;

    // Move Gate 상태 (Top M Hold 중 true)
    volatile bool _moveGateHeld = false;
    
    // [Front Hold] Side F 누름 중 true → 스크롤 모드
    volatile bool _frontHoldActive = false;
    
    // ====================================================
    // [Phase 1] Click-Freeze FSM
    // ====================================================
    enum EN_E10_FreezeState_t : uint8_t {
        E10_FREEZE_IDLE    = 0,
        E10_FREEZE_LOCKED  = 1,
        E10_FREEZE_HOLD    = 2,
        E10_FREEZE_FADEOUT = 3,
    };

    EN_E10_FreezeState_t _freezeState  = E10_FREEZE_IDLE;
    uint32_t             _freezeTimer  = 0;
    uint32_t             _holdTimer    = 0;
    uint32_t             _fadeTimer    = 0;
    float                _accumDx      = 0.0f;
    float                _accumDy      = 0.0f;

    volatile bool _btnLDown = false;

    // ====================================================
    // [Phase 3] Snap-to-Axis FSM
    // ====================================================
    enum EN_E10_SnapAxis_t : uint8_t {
        E10_SNAP_NONE   = 0,
        E10_SNAP_HORIZ  = 1,
        E10_SNAP_VERT   = 2,
    };

    EN_E10_SnapAxis_t _snapActiveAxis      = E10_SNAP_NONE;
    EN_E10_SnapAxis_t _snapCandidate       = E10_SNAP_NONE;
    uint8_t           _snapCandidateFrames = 0;
    


    // Top M DOWN 시각 (Mode 3 클릭 판정용)
    uint32_t _topMDownMs = 0;

    // 하드코딩 액션 (자주 쓰는 슬롯)
    static const ST_C20_ActionSlot_t G_SLOT_MOUSE_L_HOLD;



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

    // -------- diagnostics --------
    void getStatus(ST_E10_Status_t& p_out);
    bool requestGyroCalibration();
    bool requestI2CRecover();
    bool clearDiagnostics();

    // [H-2] 공개 API는 동일 동작(호환용 alias). 모두 enqueue.
    bool forceReleaseButtons();
    bool forceReleaseAllButtons();

    // -------- test --------
    bool testPptKey2(uint8_t p_page, uint8_t p_mod, uint32_t p_code);
    bool testMouseClick(uint8_t p_btnMask, uint16_t p_holdMs = 25);

    // ====================================================
    // [v0410] Profile 관리
    // ====================================================
    bool reloadActiveProfile();          // LittleFS → _cfgProfile 재로드 + 런타임 반영
    bool saveActiveProfile();            // 런타임 → _cfgProfile → LittleFS 저장

    bool getActiveProfileInfo(uint8_t& p_outIdx, uint8_t& p_outCount,
                              char* p_outName, size_t p_outNameSize);

    // 프로파일 전환 (안전 처리: 매크로 abort, 액션/repeat 해제, HID release)
    bool switchProfile(uint8_t p_idx);

    // [v0410] 매크로 개수 (status 노출용, 읽기 전용)
    uint8_t getMacroCount() const;

    // ====================================================
    // [v0410] Live Test (단일 액션 즉시 실행)
    //   - SPECIAL: sensorTask 즉시 (동기)
    //   - MACRO / 기타: 큐 경유 (비동기)
    // ====================================================
    bool execLiveTest(uint8_t p_kind, uint8_t p_hMode,
                      uint16_t p_p16, uint32_t p_p32);
    
    // ====================================================
    // [Phase 10] main loop에서 호출 (200ms cadence)
    //   - BLE dirty 플래그 → config 저장
    // ====================================================
    void tickConfigSave();


  private:
    // -----------------------
    // Config apply
    // -----------------------
    bool _applyFromConfig();
    void _getE10RuntimeConfig(ST_C10_E10Config_t& p_out);
    void _snapshotRuntimeToE10Config(ST_C10_E10Config_t& p_out);
    void _applyE10ToRuntime(const ST_C10_E10Config_t& p_e);

    // [v0410] 활성 프로파일 로드/저장 (LittleFS)
    bool _reloadActiveProfile();
    bool _saveActiveProfile();

    // [v0410] Global + Mode Override 슬롯 조회 (O(1))
    //   p_mode: 1~3, p_trig: EN_C10_Trigger_t (0~26)
    ST_C20_ActionSlot_t _resolveSlot(uint8_t p_mode, uint8_t p_trig) const;

    // 매크로 실행기 
    // C-3: 매크로 상태머신
    void _startMacro(uint8_t p_idx);
    void _tickMacro();
    
    // [H-3] 락 보유 상태에서 실행. caller가 _lock() 잡고 호출.
    void _applyRuntimeLocked(const ST_C10_E10Config_t& p_e);

    // -----------------------
    // HID helpers (실행 primitives)
    // -----------------------
    void _tapComboUsageKb(uint8_t p_modMask, uint8_t p_usage, uint16_t p_ms = 22);
    void _tapUsageKb(uint8_t p_usage, uint16_t p_ms = 12);
    void _tapConsumerMask(uint32_t p_mask, uint16_t p_ms = 28);
    void _sendPptKey2(uint8_t p_page, uint8_t p_mod, uint32_t p_code);
    
    // ---- HID cmd queue (producer: any task) ----
    bool _enqueueHidCmd(const ST_E10_HidCmd_t& p_cmd);

    // ---- HID exec primitives (consumer: commTask ONLY) ----
    void _doReleaseAllButtons();
    void _doTestMouseClick(uint8_t p_mask, uint16_t p_holdMs);
    void _doForceReleaseNow();

    // -----------------------
    // Motion helpers
    // -----------------------
    void _applyPrecision(float& p_fx, float& p_fy);
    void _fsmUpdate(bool p_btnScroll, bool p_btnModeLongToggle, float p_gyroAbs);
    void _applyClickFreeze(float& p_fx, float& p_fy,
                           float p_rawDx, float p_rawDy,
                           float p_gyroAbs,
                           bool  p_btnDown);       // [Phase 1]
    void _applySnapToAxis(float& p_fx, float& p_fy);   // [Phase 3]

    void _resetSnapState() {
        _snapActiveAxis      = E10_SNAP_NONE;
        _snapCandidate       = E10_SNAP_NONE;
        _snapCandidateFrames = 0;
    }
    
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
    static void _mouseSend(MouseDevice& p_ms, int8_t p_dx, int8_t p_dy,
                           int8_t p_wheel, int8_t p_pan) {
        p_ms.mouseMove(p_dx, p_dy, p_wheel, p_pan);
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
    
    static void _ledTask(void* p_pv);
    TaskHandle_t _thLed = nullptr;
    
    
    // ====================================================
    // [Phase 5 / 11.6] Action & Power 콜백
    // ====================================================
    static void _onBtnEvent(void* p_ctx, uint8_t p_btnId, uint8_t p_evt);
    static void _onSpecial(void* p_ctx, uint8_t p_special);
    static void _onPowerWake(void* p_ctx);   // [Phase 11.6 / C-3]
    
    bool _enqueueAction(const ST_C20_ActionSlot_t& p_slot, bool p_isDown);
    
    // 하드코딩 처리 (모드 전환/페어링/Move Gate/Top L Hold/Top M Enter)
    //  - true 반환 시 슬롯 매핑 진행 안 함
    bool _handleHardcodedButton(uint8_t p_btnId, uint8_t p_evt);
    
    // 슬롯 매핑 처리 (config)
    void _handleSlotButton(uint8_t p_btnId, uint8_t p_evt);
    
    // [Phase 7] 제스처 슬롯 발동 (group: 0=flick, 1=linear, 2=tilt)
    void _handleGesture(uint8_t p_group, uint8_t p_dir);
    
    // Mode 전환
    void _setActiveMode(uint8_t p_newMode);
    
    // 특수 액션 처리
    void _handleSpecial(uint8_t p_special);

    // -----------------------
    // Tasks
    // -----------------------
    static void _sensorTask(void* p_pv);
    static void _commTask(void* p_pv);
};
