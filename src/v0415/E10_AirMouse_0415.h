// =======================================================
// File: src/v0415/E10_AirMouse_0415.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : E10_AirMouse_0415.h
 * 모듈약어 : E10
 * 모듈명 : Elite AirMouse - 통합 클래스 선언
 * ------------------------------------------------------
 * 기능 요약
 *  - 3-Mode AirMouse + 다중 프로파일 + 매크로 라이브러리
 *  - BLE HID Composite (Mouse + Keyboard + Consumer)
 *  - Motion Advanced (Click-Freeze / Adaptive EMA / Snap-to-Axis)
 *  - P20 전원 관리 연동 (Safe/Pairing Wake Mask)
 *  - C20 버튼 타이밍 + Special Action 라우팅
 *
 * [v0415 Phase 1-2]
 *  - _clamp<T>() 템플릿 헬퍼 추가
 *
 * [v0415 Phase 1-5]
 *  - _abortMacroLocked() 헬퍼 도입 (매크로 취소 SSOT)
 *
 * [v0415 Phase 3-A]
 *  - sensorTask 안전 분할 헬퍼 4종 선언
 *
 * [v0415 Phase 3-B]
 *  - commTask 안전 분할 헬퍼 4종 선언 + 멤버 3개 승격
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 *   - 클래스명              : CL_모듈약어_ 접두사 , 버전 제거
 *   - 클래스 private 멤버   : _ 접두사
 *   - 클래스 정적 멤버      : s_ 접두사
 *   - 함수 로컬 변수        : v_ 접두사
 *   - 함수 인자             : p_ 접두사
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <Wire.h>
#include <math.h>

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#include <BleCompositeHID.h>
#include <KeyboardDevice.h>
#include <MouseDevice.h>

#include "D10_Logger_0415.h"
#include "C10_Config_0415.h"
#include "E10_Def_0415.h"
#include "HW_Def_0415.h"

#include "M10_MotionProc_0415.h"
#include "M20_BiasTracker_0415.h"
#include "M30_Gesture_0415.h"

#include "C20_BtnDispatcher_0415.h"
#include "C20_ActionExec_0415.h"

#include "B20_Ble_0415.h"
#include "P20_Power_0415.h"
#include "L10_Led_0415.h"


class CL_E10_EliteAirMouse {
  private:
    // ====================================================
    // 하드웨어 / 디바이스
    // ====================================================
    Adafruit_MPU6050 _mpu;
    uint8_t          _mpuAddr = 0x68;

    BleCompositeHID _hid;
    KeyboardDevice  _keyboard;
    MouseDevice     _mouse;

    // ====================================================
    // 모션 엔진 / Config
    // ====================================================
    CL_M10_AdvancedMotionProcessor _engine;
    CL_C10_Config*                 _cfg = nullptr;

    ST_E10_State_t _state;

    volatile bool _safeMode = false;

    SemaphoreHandle_t _mutex = nullptr;

    // ====================================================
    // Frame Queue (sensor → comm)
    // ====================================================
    typedef struct ST_E10_Frame_t {
        int16_t x;
        int16_t y;
        int16_t wheel;
        int16_t pan;
        uint8_t btn_mask;
        bool    updated;
    } ST_E10_Frame_t;

    QueueHandle_t _qFrame = nullptr;

    // ====================================================
    // HID Command Queue
    // ====================================================
    enum EN_E10_HidCmd_t : uint8_t {
        EN_E10_HIDCMD_NONE        = 0,
        EN_E10_HIDCMD_RELEASE_ALL = 1,
        EN_E10_HIDCMD_TEST_PPT    = 2,
    };

    typedef struct ST_E10_HidCmd_t {
        uint8_t  cmd;
        uint8_t  arg0;
        uint8_t  arg1;
        uint32_t code;
    } ST_E10_HidCmd_t;

    QueueHandle_t _qHidCmd = nullptr;

    // ====================================================
    // Gyro calibration
    // ====================================================
    bool _gyroCalibDone = false;

    // ====================================================
    // Runtime config (applied)
    // ====================================================
    int           _dpiLevel      = 2;
    bool          _hardClickLock = true;

    float _scaleBase[3] = {0.55f, 0.75f, 1.0f};
    float _accelGain[3] = {0.35f, 0.55f, 0.85f};
    float _accelTh      = 8.0f;

    float _wheelThDeg   = 90.0f;
    int   _wheelStepMax = 6;

    float _scrollCursorDamp = 0.25f;

    // Precision config + FSM knobs
    uint8_t _precision_mode = (uint8_t)EN_C10_E10_PREC_OFF;

    float    _precDeadzone    = 1.2f;
    float    _precGain        = 0.65f;
    float    _precAccel       = 0.25f;
    uint8_t  _precMaxStep     = 18;
    float    _precSmooth      = 0.85f;
    float    _precSmX         = 0.0f;
    float    _precSmY         = 0.0f;
    uint16_t _precEntryMs       = 180;
    uint16_t _precExitMs        = 160;
    float    _precEntryStillDeg = 2.2f;
    float    _precExitMoveDeg   = 7.5f;
    uint8_t  _precProfile       = 1;

    // ====================================================
    // 활성 프로파일 스냅샷
    // ====================================================
    ST_C10_ProfileConfig_t _cfgProfile;
    bool                   _cfgProfileValid = false;

    // ====================================================
    // [H-1] 매크로 상태머신 + 취소 토큰
    // ====================================================
    volatile uint32_t _macroAbortToken = 0;

    struct ST_MacroState_t {
        uint8_t  macroIdx;
        uint8_t  stepIdx;
        volatile bool active;
        uint8_t  _pad;
        uint32_t stepStartMs;
        uint32_t startToken;
    };

    ST_MacroState_t _macroState = {};

    ST_C10_Macro_t _macroSnapshot = {};

    // ====================================================
    // 리셋 위임 플래그
    // ====================================================
    volatile bool _reqResetBtnDisp = false;
    volatile bool _reqResetGesture = false;

    // Multi-Host 위임 플래그
    volatile EN_C20_ReqHostOp_t _reqHostOp = EN_C20_REQ_HOST_NONE;

    // ====================================================
    // 통계 / 진단
    // ====================================================
    float    _tempC   = 0.0f;
    uint32_t _uptime0 = 0;
    float    _dtAvgMs = 8.0f;

    TaskHandle_t _thSensor = nullptr;
    TaskHandle_t _thComm   = nullptr;
    TaskHandle_t _thLed    = nullptr;

    uint32_t _stackSensorMinWords = 0;
    uint32_t _stackCommMinWords   = 0;
    uint32_t _stackLedMinWords    = 0;

    float    _dtMaxMs        = 0.0f;
    uint32_t _dtOverrunCount = 0;

    float    _commDtAvgMs       = 7.0f;
    float    _commDtMaxMs       = 0.0f;
    uint32_t _commOverrunCount  = 0;

    uint32_t _failsafeReleaseCount = 0;

    // ====================================================
    // 오류 카운터
    // ====================================================
    uint32_t _errMpuNan      = 0;
    uint32_t _errMutexMiss   = 0;
    uint32_t _errTaskOverrun = 0;

    // ====================================================
    // RMS Welford 통계
    // ====================================================
    uint32_t _gyroN    = 0;
    double   _gyroMean = 0.0;
    double   _gyroM2   = 0.0;

    uint32_t _curN    = 0;
    double   _curMean = 0.0;
    double   _curM2   = 0.0;

    // ====================================================
    // I2C recover
    // ====================================================
    uint32_t _i2cRecoverCount  = 0;
    bool     _i2cRecoverLastOk = true;

    // ====================================================
    // 이력 링버퍼
    // ====================================================
    ST_E10_ErrEvt_t _errHist[E10_CONST::ERR_HIST_CAP];
    uint8_t         _errHistHead  = 0;
    uint8_t         _errHistCount = 0;

    ST_E10_SpikeEvt_t _spikes[32];
    uint8_t           _spikeHead  = 0;
    uint8_t           _spikeCount = 0;

    // ====================================================
    // 연속 실패 카운터
    // ====================================================
    uint16_t _consecutiveFail        = 0;
    uint16_t _consecutiveRecoverFail = 0;
    uint16_t _consecutiveI2cFail     = 0;

    // ====================================================
    // FSM
    // ====================================================
    uint8_t  _fsm     = EN_FSM_AIR;
    uint8_t  _precSub = EN_PREC_OFF;
    uint32_t _precT0  = 0;

    // ====================================================
    // OTA Guard
    // ====================================================
    volatile bool _otaGuard      = false;
    uint32_t      _otaGuardCount = 0;
    uint32_t      _otaGuardT0Ms  = 0;

    // ====================================================
    // Async requests
    // ====================================================
    volatile bool    _reqGyroCalib      = false;
    volatile bool    _reqI2CRecover     = false;
    volatile bool    _reqClearDiag      = false;
    volatile uint8_t _reqSpecialAction  = 0;
    volatile bool    _reqCommReleaseAll = false;
    volatile bool    _reqSaveCfg        = false;

    // ====================================================
    // 서브시스템
    // ====================================================
    CL_L10_Led     _led;
    CL_B20_Ble     _ble;
    CL_P20_Power   _power;
    CL_M20_BiasTracker _biasTracker;
    CL_M30_Gesture     _gesture;
    
    // ====================================================
    // [Phase 11.6 / C-1] 커서 이동 기반 deferred 활동 알림
    //   - sensorTask가 프레임 push 후 set
    //   - _handlePowerManagement 진입부에서 소비
    // ====================================================
    volatile bool _powerNotifyPending = false;

    CL_C20_BtnDispatcher _btnDisp;
    CL_C20_ActionExec    _actExec;

    // 슬롯 실행 커맨드 큐
    struct ST_ActionCmd_t {
        ST_C20_ActionSlot_t slot;
        bool                isDown;
    };
    QueueHandle_t _qActionExec = nullptr;

    // ====================================================
    // 활성 상태
    // ====================================================
    volatile uint8_t _activeMode      = 1;
    volatile bool    _moveGateHeld    = false;
    volatile bool    _frontHoldActive = false;

    // ====================================================
    // [Phase 1] Click-Freeze FSM
    // ====================================================
    enum EN_E10_FreezeState_t : uint8_t {
        E10_FREEZE_IDLE    = 0,
        E10_FREEZE_LOCKED  = 1,
        E10_FREEZE_HOLD    = 2,
        E10_FREEZE_FADEOUT = 3,
    };

    EN_E10_FreezeState_t _freezeState = E10_FREEZE_IDLE;
    uint32_t             _freezeTimer = 0;
    uint32_t             _holdTimer   = 0;
    uint32_t             _fadeTimer   = 0;
    float                _accumDx     = 0.0f;
    float                _accumDy     = 0.0f;

    volatile bool _btnLDown = false;

    // ====================================================
    // [Phase 3] Snap-to-Axis FSM
    // ====================================================
    enum EN_E10_SnapAxis_t : uint8_t {
        E10_SNAP_NONE  = 0,
        E10_SNAP_HORIZ = 1,
        E10_SNAP_VERT  = 2,
    };

    EN_E10_SnapAxis_t _snapActiveAxis      = E10_SNAP_NONE;
    EN_E10_SnapAxis_t _snapCandidate       = E10_SNAP_NONE;
    uint8_t           _snapCandidateFrames = 0;

    // ====================================================
    // [v0415 Phase 3-B] commTask loop-local 상태 (멤버 승격)
    // ====================================================
    bool _commPrevConn       = false;   // 이전 루프의 연결 상태
    bool _commReleasedOnSafe = false;   // gate 진입 시 1회만 release
    bool _connEdgePrev       = false;   // BLE 연결 edge 감지용

    // ====================================================
    // 하드코딩 슬롯 상수
    // ====================================================
    static const ST_C20_ActionSlot_t G_SLOT_MOUSE_L_HOLD;

  public:
    CL_E10_EliteAirMouse();

    // -------- lifecycle --------
    void begin(CL_C10_Config* p_cfg);

    // -------- runtime apply (no persist) --------
    bool applyRuntimeE10(const ST_C10_E10Config_t& p_e);

    // -------- control --------
    bool setDpiLevel(uint8_t p_level);
    bool setPrecisionMode(uint8_t p_mode);

    // -------- Multi-Host --------
    void requestHostOp(EN_C20_ReqHostOp_t p_op) {
        _reqHostOp = p_op;
    }
    bool getHostState(uint8_t* p_outBondCount,
                      uint8_t* p_outActiveIdx,
                      uint8_t* p_outTargetIdx,
                      bool* p_outSelecting,
                      char* p_outActivePeerName,
                      size_t p_nameBufSize);

    bool setActiveMode(uint8_t p_mode);

    bool setHardClickLock(bool p_enable);
    bool setSafeMode(bool p_enable);
    bool setOtaGuard(bool p_enable);

    // -------- diagnostics --------
    void getStatus(ST_E10_Status_t& p_out);
    bool requestGyroCalibration();
    bool requestI2CRecover();
    bool clearDiagnostics();

    // -------- HID 안전 Release --------
    bool forceReleaseButtons();

    // -------- test --------
    bool testPptKey2(uint8_t p_page, uint8_t p_mod, uint32_t p_code);

    // ====================================================
    // Profile 관리
    // ====================================================
    bool reloadActiveProfile();
    bool saveActiveProfile();

    bool getActiveProfileInfo(uint8_t& p_outIdx, uint8_t& p_outCount,
                              char* p_outName, size_t p_outNameSize);

    bool switchProfile(uint8_t p_idx);

    uint8_t getMacroCount() const;

    // ====================================================
    // Live Test
    // ====================================================
    bool execLiveTest(uint8_t p_kind, uint8_t p_hMode,
                      uint16_t p_p16, uint32_t p_p32);

    // ====================================================
    // main loop tick
    // ====================================================
    void tickConfigSave();

  private:
    // ====================================================
    // Config apply
    // ====================================================
    bool _applyFromConfig();
    void _snapshotRuntimeToE10Config(ST_C10_E10Config_t& p_out);
    void _applyE10ToRuntime(const ST_C10_E10Config_t& p_e);

    bool _reloadActiveProfile();
    bool _saveActiveProfile();

    ST_C20_ActionSlot_t _resolveSlot(uint8_t p_mode, uint8_t p_trig) const;

    // 매크로
    void _startMacro(uint8_t p_idx);
    void _tickMacro();

    void _applyRuntimeLocked(const ST_C10_E10Config_t& p_e);

    // ====================================================
    // HID primitives
    // ====================================================
    void _tapComboUsageKb(uint8_t p_modMask, uint8_t p_usage, uint16_t p_ms = 22);
    void _tapUsageKb(uint8_t p_usage, uint16_t p_ms = 12);
    void _tapConsumerMask(uint32_t p_mask, uint16_t p_ms = 28);
    void _sendPptKey2(uint8_t p_page, uint8_t p_mod, uint32_t p_code);

    bool _enqueueHidCmd(const ST_E10_HidCmd_t& p_cmd);

    void _doReleaseAllButtons();
    void _doForceReleaseNow();

    // ====================================================
    // Motion helpers
    // ====================================================
    void _applyPrecision(float& p_fx, float& p_fy);
    void _fsmUpdate(float p_gyroAbs);
    void _applyClickFreeze(float& p_fx, float& p_fy,
                           float p_rawDx, float p_rawDy,
                           float p_gyroAbs,
                           bool  p_btnDown);
    void _applySnapToAxis(float& p_fx, float& p_fy);

    void _resetSnapState() {
        _snapActiveAxis      = E10_SNAP_NONE;
        _snapCandidate       = E10_SNAP_NONE;
        _snapCandidateFrames = 0;
    }

    // ====================================================
    // Diagnostics helpers
    // ====================================================
    void _pushErr(uint8_t p_code, uint16_t p_value = 0);
    void _pushSpike(uint32_t p_tsMs);
    bool _recoverI2C();
    bool _diagAndRecoverMpu();
    void _runGyroCalibration();

    // ====================================================
    // [v0415 Phase 3-A] sensorTask 안전 분할 헬퍼
    //   - 원본 순서 그대로 유지
    //   - motion pipeline 지역변수와 무관한 블록만 추출
    // ====================================================
    void _bootPriorityReconnect();       // for 루프 진입 전 (1회)
    void _consumeHostOp();               // 루프 진입 직후
    void _handlePowerManagement();       // config 스냅샷 이후
    void _consumeResetAndAsyncFlags();   // power 블록 이후

    // ====================================================
    // [v0415 Phase 3-B] commTask 안전 분할 헬퍼
    // ====================================================
    bool _handleCommDisconnectEdge(bool p_conn);
    bool _handleCommGate(bool p_conn);
    void _drainAllQueuesOnGate();
    void _notifyConnectionEdge(bool p_conn);

    // ====================================================
    // Inline 유틸
    // ====================================================
    static void _mouseSend(MouseDevice& p_ms, int8_t p_dx, int8_t p_dy,
                           int8_t p_wheel, int8_t p_pan) {
        p_ms.mouseMove(p_dx, p_dy, p_wheel, p_pan);
    }

    void _pushFrame(const ST_E10_Frame_t& p_fr) {
        if (!_qFrame) return;
        xQueueOverwrite(_qFrame, &p_fr);
    }

    void _lock() {
        if (_mutex) (void)xSemaphoreTakeRecursive(_mutex, portMAX_DELAY);
    }

    void _unlock() {
        if (_mutex) xSemaphoreGiveRecursive(_mutex);
    }

    // =====================================================
    // [v0415 Phase 1-2] 범위 클램프 헬퍼 (혼합 타입 지원)
    // -----------------------------------------------------
    //  - T: 값 타입, U: 경계 리터럴 타입
    //  - 예: _clamp(uint16_t_val, 50, 500)  ← U=int, T=uint16_t
    //        _clamp(float_val, 0.05f, 5.0f) ← U=float, T=float
    //  - T로 캐스팅 후 비교·반환 (오버플로 방지 위해 경계값은 T 범위 내 사용)
    // =====================================================
    template <typename T, typename U>
    static constexpr T _clamp(T p_v, U p_lo, U p_hi) {
        static_assert(sizeof(U) <= sizeof(T) || std::is_signed<U>::value,
                      "Bounds type too large for value type");
        return (p_v < (T)p_lo) ? (T)p_lo
             : (p_v > (T)p_hi) ? (T)p_hi
             : p_v;
    }

    // =====================================================
    // [v0415 Phase 1-5] 매크로 즉시 취소 (lock 원자화 SSOT)
    // -----------------------------------------------------
    //  사용처:
    //    - _setActiveMode
    //    - switchProfile
    //    - _commTask disconnect edge / gate 진입
    //  ※ forceReleaseButtons는 state 리셋과 함께 lock 블록 유지
    //    (개별 처리, 헬퍼 미사용)
    // =====================================================
    void _abortMacroLocked() {
        _lock();
        _macroAbortToken++;
        _macroState.active = false;
        _unlock();
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

    // ====================================================
    // Task 진입점
    // ====================================================
    static void _sensorTask(void* p_pv);
    static void _commTask(void* p_pv);
    static void _ledTask(void* p_pv);

    // ====================================================
    // 콜백
    // ====================================================
    static void _onBtnEvent(void* p_ctx, uint8_t p_btnId, uint8_t p_evt);
    static void _onPowerWake(void* p_ctx);

    bool _enqueueAction(const ST_C20_ActionSlot_t& p_slot, bool p_isDown);

    // 하드코딩 처리
    bool _handleHardcodedButton(uint8_t p_btnId, uint8_t p_evt);

    // 슬롯 매핑 처리
    void _handleSlotButton(uint8_t p_btnId, uint8_t p_evt);

    // 제스처 슬롯 발동
    void _handleGesture(uint8_t p_group, uint8_t p_dir);

    // Mode 전환
    void _setActiveMode(uint8_t p_newMode);

    // 특수 액션 처리
    void _handleSpecial(uint8_t p_special);
};
