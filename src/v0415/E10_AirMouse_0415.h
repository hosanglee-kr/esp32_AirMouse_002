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
 * [v0415 주요 변경]
 *  - 파일명/심볼 접미사: _0412 → _0415
 *  - A40_ComFunc 삭제 반영 → D10_Logger_0415.h 직접 include
 *  - forceReleaseAllButtons() 삭제 (Phase 6 Q7-a)
 *    · 단일 API(forceReleaseButtons)로 통합
 *  - _thLed 멤버 추가 (Phase 6 L6a-A1-02) — LED 태스크 스택 관측
 *  - EN_E10_HidCmd_t::holdMs 삭제 (Phase 8 L8c-04, Dead)
 *    · TEST_CLICK 삭제(rev5) 이후 미사용
 *  - _modeToggleCooldownMs / _lastModeToggleMs 삭제 (Phase 6 L6a-A4-03)
 *    · _fsmUpdate의 p_btnModeLongToggle 분기가 Dead (Phase 6 L6d-A1-01)
 *  - _macroState 정책 주석 강화 (Round I/J에서 lock 적용)
 *  - HW_Def_0415.h SSOT 참조
 *
 * [참고]
 *  - E10 7파일 분할 (Core/Hid/Motion/Diag/Task/Action + .h)
 *  - 태스크 소유권/큐 계약은 CONTRACT_0415.md 준수
 * ------------------------------------------------------
 * [구현 규칙]
 *  - 항상 소스 시작 주석 부분 체계 유지 및 내용 업데이트
 *  - 소스 시작 주석 부분 구현규칙, 코드네이밍규칙 내용 그대로 유지, 수정금지
 *  - ArduinoJson v7.x.x 사용 (v6 이하 사용 금지)
 *  - JsonDocument 단일 타입만 사용
 *  - createNestedArray/Object/containsKey 사용 금지
 *  - memset + strlcpy 기반 안전 초기화
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

// [v0415] A40_ComFunc_0412.h 삭제 → D10_Logger_0415.h 직접 include
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
    Adafruit_MPU6050 _mpu;

    BleCompositeHID _hid;
    KeyboardDevice  _keyboard;
    MouseDevice     _mouse;

    CL_M10_AdvancedMotionProcessor _engine;
    CL_C10_Config*                 _cfg = nullptr;

    ST_E10_State_t _state;

    // Safe mode gate
    volatile bool _safeMode = false;

    SemaphoreHandle_t _mutex = nullptr;

    // ----------------------------------------------------
    // Frame Queue (sensor → comm)
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
    // HID Command Queue
    //  - 웹/sensor 태스크가 _mouse/_keyboard를 직접 만지지 않는다
    //  - commTask가 유일한 HID 실행자
    //
    // [v0415] holdMs 필드 삭제 (Phase 8 L8c-04)
    //   - TEST_CLICK 삭제(rev5) 이후 미사용
    // ----------------------------------------------------
    enum EN_E10_HidCmd_t : uint8_t {
        EN_E10_HIDCMD_NONE        = 0,
        EN_E10_HIDCMD_RELEASE_ALL = 1,
        EN_E10_HIDCMD_TEST_PPT    = 2,
    };

    typedef struct ST_E10_HidCmd_t {
        uint8_t  cmd;
        uint8_t  arg0;    // mouse mask or key page
        uint8_t  arg1;    // key mod
        uint32_t code;    // TEST_PPT code
        // [v0415 삭제] uint16_t holdMs; — TEST_CLICK 삭제 이후 Dead
    } ST_E10_HidCmd_t;

    QueueHandle_t _qHidCmd = nullptr;

    // Gyro calib
    bool _gyroCalibDone = false;

    // Runtime config (applied)
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
    //   _cfgProfile.e10    : E10 파라미터
    //   _cfgProfile.slots  : Global + Mode Override 슬롯 매트릭스
    //   _cfgProfile.macros : 매크로 라이브러리 (최대 8×8)
    // ====================================================
    ST_C10_ProfileConfig_t _cfgProfile;
    bool                   _cfgProfileValid = false;

    // ====================================================
    // [H-1] 매크로 상태머신 + 취소 토큰
    // ----------------------------------------------------
    // [v0415 정책 강화 — Round I/J에서 lock 적용]
    //   - active는 writer 3 태스크(sensor/comm/web) → volatile 필요
    //   - 나머지 필드는 commTask(단일 writer)이지만, 컴파일러 재정렬 방어 및
    //     sensor/web 태스크의 관측(active 여부)과의 일관성을 위해
    //     Round I/J에서 _lock() 하에 전체 갱신하도록 리팩터 예정
    //   - 현재 헤더는 선언만 유지 (사용부에서 정책 적용)
    // ====================================================
    volatile uint32_t _macroAbortToken = 0;

    // [v0415 OPT-F] 정렬 최적화 (16B → 12B)
    struct ST_MacroState_t {
        uint8_t  macroIdx;
        uint8_t  stepIdx;
        volatile bool active;
        uint8_t  _pad;
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
    // [v0415] LED 태스크 핸들 추가 (Phase 6 L6a-A1-02)
    TaskHandle_t _thLed    = nullptr;

    uint32_t _stackSensorMinWords = 0;
    uint32_t _stackCommMinWords   = 0;

    float    _dtMaxMs = 0.0f;
    uint32_t _dtOverrunCount = 0;

    float    _commDtAvgMs = 7.0f;
    float    _commDtMaxMs = 0.0f;
    uint32_t _commOverrunCount = 0;

    uint32_t _failsafeReleaseCount = 0;

    // [v0415 삭제] _lastModeToggleMs / _modeToggleCooldownMs
    //   - _fsmUpdate의 p_btnModeLongToggle 분기가 Dead (Phase 6 L6d-A1-01)
    //   - Round K에서 _fsmUpdate 정리와 함께 완전 삭제

    // Errors
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

    // I2C recover
    uint32_t _i2cRecoverCount  = 0;
    bool     _i2cRecoverLastOk = true;

    // History ring
    ST_E10_ErrEvt_t _errHist[E10_CONST::ERR_HIST_CAP];
    uint8_t         _errHistHead  = 0;
    uint8_t         _errHistCount = 0;

    // Anomaly ring
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

    // Async requests
    volatile bool    _reqGyroCalib      = false;
    volatile bool    _reqI2CRecover     = false;
    volatile bool    _reqClearDiag      = false;
    volatile uint8_t _reqSpecialAction  = 0;
    volatile bool    _reqCommReleaseAll = false;

    // config 저장 필요 플래그
    //   - set: tickConfigSave() 내부에서 _ble.consumeDirty() 결과로만 true
    //   - clear: tickConfigSave() 저장 완료 시 false
    //   - 외부에서 직접 set 금지 (single-writer 원칙)
    volatile bool _reqSaveCfg = false;

    // ====================================================
    // [Phase 6-J] LED 컨트롤러
    // ====================================================
    CL_L10_Led _led;

    // ====================================================
    // [Phase 10] BLE Manager
    // ====================================================
    CL_B20_Ble _ble;

    // ====================================================
    // [Phase 8 / 11.6] Power Manager
    // ====================================================
    CL_P20_Power  _power;
    volatile bool _powerNotifyPending = false;

    // ====================================================
    // [Phase 4] Zero-rate Bias Tracker
    // ====================================================
    CL_M20_BiasTracker _biasTracker;

    // ====================================================
    // [Phase 7] 제스처 감지기
    // ====================================================
    CL_M30_Gesture _gesture;

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

    // 하드코딩 액션 (자주 쓰는 슬롯)
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
    bool setHardClickLock(bool p_enable);
    bool setSafeMode(bool p_enable);
    bool setOtaGuard(bool p_enable);

    // -------- diagnostics --------
    void getStatus(ST_E10_Status_t& p_out);
    bool requestGyroCalibration();
    bool requestI2CRecover();
    bool clearDiagnostics();

    // -------- HID 안전 Release (W10/switchProfile 등에서 호출) --------
    // [v0415] forceReleaseAllButtons() 삭제 (Phase 6 Q7-a 확정)
    //   - 호출처 없음 확인 → 단일 API로 통합
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

    // 프로파일 전환 (안전 처리: 매크로 abort, 액션/repeat 해제, HID release)
    bool switchProfile(uint8_t p_idx);

    uint8_t getMacroCount() const;

    // ====================================================
    // Live Test (단일 액션 즉시 실행)
    //   - SPECIAL: sensorTask 즉시 (동기)
    //   - MACRO / 기타: 큐 경유 (비동기)
    // ====================================================
    bool execLiveTest(uint8_t p_kind, uint8_t p_hMode,
                      uint16_t p_p16, uint32_t p_p32);

    // ====================================================
    // main loop에서 호출 (200ms cadence)
    //   - BLE dirty 플래그 → config 저장
    // ====================================================
    void tickConfigSave();

  private:
    // -----------------------
    // Config apply
    // -----------------------
    bool _applyFromConfig();
    void _snapshotRuntimeToE10Config(ST_C10_E10Config_t& p_out);
    void _applyE10ToRuntime(const ST_C10_E10Config_t& p_e);

    bool _reloadActiveProfile();
    bool _saveActiveProfile();

    ST_C20_ActionSlot_t _resolveSlot(uint8_t p_mode, uint8_t p_trig) const;

    // 매크로 실행기 (commTask, 스텝 단위 상태머신)
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
    void _doForceReleaseNow();

    // -----------------------
    // Motion helpers
    // -----------------------
    void _applyPrecision(float& p_fx, float& p_fy);
    

    // [v0415] FSM Dead 분기 정리 (Phase 6 L6d-A1-01)
    //   - p_btnScroll / p_btnModeLongToggle 인자 삭제
    //   - FSM 판정은 _activeMode == 2 (PPT) 로 일원화
    //   - SCROLL 상태는 폐기 (제스처 Phase 7에서 재설계됨)
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

    // ====================================================
    // [Phase 5 / 11.6] Action & Power 콜백
    // ====================================================
    static void _onBtnEvent(void* p_ctx, uint8_t p_btnId, uint8_t p_evt);
    static void _onPowerWake(void* p_ctx);

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
