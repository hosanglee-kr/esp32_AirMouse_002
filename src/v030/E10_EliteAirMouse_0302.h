// =======================================================
// File: src/v010/E10_EliteAirMouse_0302.h
// =======================================================
#pragma once

/*
 * ------------------------------------------------------
 * 소스명 : E10_EliteAirMouse_0302.h
 * 모듈약어 : E10
 * 모듈명 : Elite AirMouse (Motion + HID)
 * ------------------------------------------------------
 * 기능 요약
 *  - MPU6050 기반 AirMouse + PPT/Scroll/Precision FSM
 *  - 고급 안정화: Drift Auto-Bias(정지 구간), Adaptive Smoothing, 더블버퍼 상태전달
 *  - HID modifier 정책: mod mask == HID modifier byte (W10와 1:1)
 *  - status: gyro/cursor RMS + spike + consecutive fail + err hist + i2c recover
 * ------------------------------------------------------
 * [구현 규칙]
 *  - 항상 소스 시작 주석 부분 체계 유지 및 내용 업데이트
 *  - 소스 시작 주석 부분 구현규칙, 코드네이밍규칙 내용 그대로 유지, 수정금지
 *  - ArduinoJson v7.x.x 사용 (v6 이하 사용 금지)
 *  - JsonDocument 단일 타입만 사용
 *  - createNestedArray/Object/containsKey 사용 금지
 *  - memset + strlcpy 기반 안전 초기화
 *  - 주석/필드명은 JSON 구조와 동일하게 유지
 *  - 변수명은 가능한 해석 가능하게
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 *   - namespace 명        : 모듈약어_ 접두사
 *   - namespace 내 상수    : 모둘약어 접두시 미사용
 *   - 전역 상수,매크로      : G_모듈약어_ 접두사
 *   - 전역 변수             : g_모듈약어_ 접두사
 *   - 전역 함수             : 모듈약어_ 접두사
 *   - type                  : T_모듈약어_ 접두사
 *   - typedef               : _t  접미사
 *   - enum 상수             : EN_모듈약어_ 접두사
 *   - 구조체                : ST_모듈약어_ 접두사
 *   - 클래스명              : CL_모듈약어_ 접두사 , 버전 제거
 *   - 클래스 private 멤버 함수/변수   : _ 접두사
 *   - 클래스 멤버(함수/변수) : 모듈약어 접두사 미사용
 *   - 클래스 정적 멤버      : s_ 접두사
 *   - 함수 로컬 변수        : v_ 접두사
 *   - 함수 인자             : p_ 접두사
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <Wire.h>
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

    // (0301) 상태전달: 더블버퍼 + 짧은 swap
    ST_E10_State_t  _stateBuf[2];
    volatile uint8_t _wrIdx = 0;
    volatile uint8_t _rdIdx = 1;
    volatile bool    _stateReady = false;
    portMUX_TYPE     _stateMux = portMUX_INITIALIZER_UNLOCKED;

    SemaphoreHandle_t _mutex = nullptr;

    // gyro static calib
    float _gyroBiasX = 0, _gyroBiasY = 0, _gyroBiasZ = 0;
    bool  _gyroCalibDone = false;

    // (0301) drift dyn bias(정지 구간만)
    float    _biasDynX = 0, _biasDynY = 0, _biasDynZ = 0;
    uint32_t _stillT0 = 0;
    bool     _stillActive = false;
    static constexpr float    s_stillEnterDeg = 1.6f;
    static constexpr float    s_stillExitDeg  = 3.2f;
    static constexpr uint16_t s_stillHoldMs   = 900;
    static constexpr float    s_biasAlpha     = 0.0022f;

    // runtime config
    volatile bool _isPptMode=false;
    int   _dpiLevel=2;
    bool  _hardClickLock=true;

    float _scaleBase[3]={0.55f,0.75f,1.0f};
    float _accelGain[3]={0.35f,0.55f,0.85f};
    float _accelTh=8.0f;

    float _wheelThDeg=90.0f;
    int   _wheelStepMax=6;

    float    _gestureFlickDeg=200.0f;
    uint16_t _gestureCooldownMs=600;

    float _scrollCursorDamp=0.25f;

    // precision
    bool  _precisionEnable=false;
    bool  _precisionMode=false;
    float _precDeadzone=1.2f, _precGain=0.65f, _precAccel=0.25f;
    uint8_t _precMaxStep=18;
    float _precSmooth=0.85f;
    float _precSmX=0, _precSmY=0;
    uint16_t _precEntryMs=180;
    uint16_t _precExitMs=160;
    float _precEntryStillDeg=2.2f;
    float _precExitMoveDeg=7.5f;
    uint8_t _precProfile=1;

    // (0301) adaptive smoothing
    float _outSmX=0, _outSmY=0;
    float _outPrecSmX=0, _outPrecSmY=0;
    float _lastOutSmooth=0.0f;

    // PPT v2
    ST_C10_PptKey2_t _ppt2_start, _ppt2_exit, _ppt2_next, _ppt2_prev, _ppt2_black, _ppt2_laser;

    float    _tempC=0;
    uint32_t _uptime0=0;
    float    _dtAvgMs=8.0f;

    // errors
    uint32_t _errMpuNan=0, _errMutexMiss=0, _errTaskOverrun=0;

    // RMS Welford
    uint32_t _gyroN=0; double _gyroMean=0, _gyroM2=0;
    uint32_t _curN=0;  double _curMean=0,  _curM2=0;

    // i2c recover
    uint32_t _i2cRecoverCount=0;
    bool     _i2cRecoverLastOk=true;

    // history ring
    ST_E10_ErrEvt_t _errHist[E10_CONST::ERR_HIST_CAP];
    uint8_t _errHistHead=0, _errHistCount=0;

    // spike ring
    ST_E10_SpikeEvt_t _spikes[E10_CONST::SPIKE_CAP];
    uint8_t _spikeHead=0, _spikeCount=0;
    uint16_t _consecutiveFail=0;
    uint16_t _consecutiveRecoverFail=0;

    // FSM
    uint8_t  _fsm     = EN_FSM_AIR;
    uint8_t  _precSub = EN_PREC_OFF;
    uint32_t _precT0  = 0;

  public:
    CL_E10_EliteAirMouse() : _hid("Elite AirMouse S3", "ProMaker", 100) {
        memset(_stateBuf,0,sizeof(_stateBuf));
        memset(_errHist,0,sizeof(_errHist));
        memset(_spikes,0,sizeof(_spikes));
        memset(&_ppt2_start,0,sizeof(_ppt2_start));
        memset(&_ppt2_exit,0,sizeof(_ppt2_exit));
        memset(&_ppt2_next,0,sizeof(_ppt2_next));
        memset(&_ppt2_prev,0,sizeof(_ppt2_prev));
        memset(&_ppt2_black,0,sizeof(_ppt2_black));
        memset(&_ppt2_laser,0,sizeof(_ppt2_laser));
    }

    void begin(CL_C10_Config* p_cfg) {
        _cfg=p_cfg;
        _uptime0=millis();
        Serial.begin(115200);

        Wire.begin(4,5);
        Wire.setClock(400000);

        if(!_mpu.begin()){
            Serial.println("[E10] MPU begin fail");
            for(;;) delay(10);
        }
        _mpu.setGyroRange(MPU6050_RANGE_250_DEG);
        _mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
        _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

        pinMode(E10_CONST::PIN_BTN_L,INPUT_PULLUP);
        pinMode(E10_CONST::PIN_BTN_MODE,INPUT_PULLUP);
        pinMode(E10_CONST::PIN_BTN_SCROLL,INPUT_PULLUP);

        _mutex=xSemaphoreCreateMutex();

        (void)applyFromConfig();

        _hid.addDevice(&_keyboard);
        _hid.addDevice(&_mouse);
        _hid.begin();

        xTaskCreatePinnedToCore(sensorTask,"E10_Sensor",8192,this,3,nullptr,1);
        xTaskCreatePinnedToCore(commTask,"E10_Comm",4096,this,2,nullptr,0);
    }

    static bool E10_W10Apply(void* p_ctx){
        if(!p_ctx) return false;
        return ((CL_E10_EliteAirMouse*)p_ctx)->applyFromConfig();
    }

    bool applyRuntimeE10(const ST_C10_E10Config_t& p_e){
        lock_();
        applyE10ToRuntime_(p_e);
        unlock_();
        return true;
    }

    bool setPptMode(bool p_enable){ lock_(); _isPptMode=p_enable; unlock_(); return true; }

    bool setDpiLevel(uint8_t p_level){
        if(p_level<1)p_level=1; if(p_level>3)p_level=3;
        lock_(); _dpiLevel=(int)p_level; _engine.setDPI(_dpiLevel); unlock_(); return true;
    }

    bool setPrecisionMode(bool p_enable){
        lock_();
        if(!_precisionEnable){
            _precisionMode=false; _fsm=EN_FSM_AIR; _precSub=EN_PREC_OFF;
            _precSmX=0; _precSmY=0; _outPrecSmX=0; _outPrecSmY=0;
            unlock_();
            return true;
        }
        _precisionMode = p_enable;
        if(_precisionMode){
            _fsm=EN_FSM_PREC; _precSub=EN_PREC_ENTRY; _precT0=(uint32_t)millis();
        }else{
            _fsm=(_isPptMode?EN_FSM_PPT:EN_FSM_AIR);
            _precSub=EN_PREC_OFF;
        }
        _precSmX=0; _precSmY=0; _outPrecSmX=0; _outPrecSmY=0;
        unlock_();
        return true;
    }

    bool testPptKey2(uint8_t p_page, uint8_t p_mod, uint32_t p_code){
        if(!_hid.isConnected()) return false;
        sendPptKey2_(p_page, p_mod, p_code);
        return true;
    }

    void getStatus(ST_E10_Status_t& p_out){
        memset(&p_out,0,sizeof(p_out));
        lock_();

        p_out.ble_connected=_hid.isConnected();
        p_out.ppt_mode=_isPptMode;
        p_out.dpi_level=(uint8_t)_dpiLevel;

        p_out.precision_enable=_precisionEnable;
        p_out.precision_mode=_precisionMode;
        p_out.fsm_state=_fsm;
        p_out.fsm_sub=_precSub;

        p_out.gyro_bias_x=_gyroBiasX; p_out.gyro_bias_y=_gyroBiasY; p_out.gyro_bias_z=_gyroBiasZ;
        p_out.gyro_bias_dyn_x=_biasDynX; p_out.gyro_bias_dyn_y=_biasDynY; p_out.gyro_bias_dyn_z=_biasDynZ;
        p_out.drift_still_active=_stillActive;

        p_out.temp_c=_tempC;
        p_out.sampling_ms_target=8;
        p_out.sampling_ms_avg=_dtAvgMs;
        p_out.out_smooth=_lastOutSmooth;

        p_out.i2c_recover_count=_i2cRecoverCount;
        p_out.i2c_recover_last_ok=_i2cRecoverLastOk;

        p_out.err_mpu_nan=_errMpuNan;
        p_out.err_mutex_miss=_errMutexMiss;
        p_out.err_task_overrun=_errTaskOverrun;

        p_out.uptime_ms=(uint32_t)(millis()-_uptime0);

        p_out.gyro_rms=calcRms_(_gyroN,_gyroM2);
        p_out.cursor_rms=calcRms_(_curN,_curM2);

        // spike count 10s
        const uint32_t now = (uint32_t)(millis()-_uptime0);
        uint16_t sc=0;
        for(uint8_t i=0;i<_spikeCount;i++){
            int idx=(int)_spikeHead-1-(int)i;
            if(idx<0) idx+=E10_CONST::SPIKE_CAP;
            if(now - _spikes[idx].ts_ms <= 10000) sc++;
            else break;
        }
        p_out.spike_count_10s=sc;
        p_out.consecutive_fail=_consecutiveFail;
        p_out.consecutive_recover_fail=_consecutiveRecoverFail;

        // health score
        uint16_t score=1000;
        score = (uint16_t)max(0, (int)score - (int)(p_out.gyro_rms*25.0f));
        score = (uint16_t)max(0, (int)score - (int)(p_out.cursor_rms*18.0f));
        score = (uint16_t)max(0, (int)score - (int)min((uint32_t)400, p_out.err_mpu_nan*20));
        score = (uint16_t)max(0, (int)score - (int)min((uint32_t)300, p_out.i2c_recover_count*35));
        score = (uint16_t)max(0, (int)score - (int)min((uint32_t)300, (uint32_t)p_out.spike_count_10s*12));
        score = (uint16_t)max(0, (int)score - (int)min((uint32_t)400, (uint32_t)p_out.consecutive_fail*18));

        p_out.health_score=score;
        p_out.health = (score>=820)?(uint8_t)EN_E10_HEALTH_OK:((score>=620)?(uint8_t)EN_E10_HEALTH_WARN:(uint8_t)EN_E10_HEALTH_DEGRADED);

        // err hist newest-first
        p_out.err_hist_n = (uint8_t)min((uint8_t)E10_CONST::ERR_HIST_CAP,_errHistCount);
        for(uint8_t i=0;i<p_out.err_hist_n;i++){
            int idx=(int)_errHistHead-1-(int)i;
            if(idx<0) idx+=E10_CONST::ERR_HIST_CAP;
            p_out.err_hist[i]=_errHist[idx];
        }

        unlock_();
    }

  private:
    bool applyFromConfig(){
        if(!_cfg) return false;
        ST_C10_WiFiConfig_t w; ST_C10_E10Config_t e;
        _cfg->makeDefaultsWiFi(w);
        _cfg->makeDefaultsE10(e);
        (void)_cfg->loadAll(w,e);

        lock_();
        applyE10ToRuntime_(e);
        unlock_();
        return true;
    }

    void applyE10ToRuntime_(const ST_C10_E10Config_t& e){
        _dpiLevel=(int)e.dpi_level;
        _hardClickLock=e.hard_click_lock;

        for(int i=0;i<3;i++){ _scaleBase[i]=e.scale_base[i]; _accelGain[i]=e.accel_gain[i]; }
        _accelTh=e.accel_threshold;

        _wheelThDeg=e.wheel_threshold_deg;
        _wheelStepMax=(int)e.wheel_step_max;
        _gestureFlickDeg=e.gesture_flick_deg;
        _gestureCooldownMs=e.gesture_cooldown_ms;
        _scrollCursorDamp=e.scroll_cursor_damp;

        _precisionEnable=e.precision_enable;
        _precDeadzone=e.precision_deadzone;
        _precGain=e.precision_gain;
        _precAccel=e.precision_accel;
        _precMaxStep=e.precision_max_step;
        _precSmooth=e.precision_smooth;
        _precEntryMs=e.prec_entry_ms;
        _precExitMs=e.prec_exit_ms;
        _precEntryStillDeg=e.prec_entry_still_deg;
        _precExitMoveDeg=e.prec_exit_move_deg;
        _precProfile=e.prec_profile;

        if(!_precisionEnable){
            _precisionMode=false; _precSub=EN_PREC_OFF;
            _fsm=_isPptMode?EN_FSM_PPT:EN_FSM_AIR;
        }

        _ppt2_start=e.ppt2_start; _ppt2_exit=e.ppt2_exit; _ppt2_next=e.ppt2_next;
        _ppt2_prev=e.ppt2_prev; _ppt2_black=e.ppt2_black; _ppt2_laser=e.ppt2_laser;

        _engine.setHardClickLock(_hardClickLock);
        _engine.setDPI(_dpiLevel);
    }

    // ----- state buffer -----
    void writeState_(int16_t x, int16_t y, int16_t wheel, bool leftDown){
        ST_E10_State_t& w = _stateBuf[_wrIdx];
        w.x=x; w.y=y; w.wheel=wheel;
        w.btn_mask = leftDown ? 0x01 : 0x00;

        portENTER_CRITICAL(&_stateMux);
        uint8_t oldWr=_wrIdx;
        _wrIdx=_rdIdx;
        _rdIdx=oldWr;
        _stateReady=true;
        portEXIT_CRITICAL(&_stateMux);
    }

    bool readState_(ST_E10_State_t& out){
        bool ok=false;
        portENTER_CRITICAL(&_stateMux);
        if(_stateReady){
            out=_stateBuf[_rdIdx];
            _stateReady=false;
            ok=true;
        }
        portEXIT_CRITICAL(&_stateMux);
        return ok;
    }

    // ----- drift bias -----
    bool stillForBias_(float gyroAbs){
        const uint32_t now=(uint32_t)millis();
        if(!_stillActive){
            if(gyroAbs <= s_stillEnterDeg){
                _stillActive=true;
                _stillT0=now;
            }
            return false;
        }
        if(gyroAbs >= s_stillExitDeg){
            _stillActive=false;
            return false;
        }
        return (uint16_t)(now-_stillT0) >= s_stillHoldMs;
    }

    void trackBias_(float rawGxDeg, float rawGyDeg, float rawGzDeg){
        const float ex = rawGxDeg - (_gyroBiasX + _biasDynX);
        const float ey = rawGyDeg - (_gyroBiasY + _biasDynY);
        const float ez = rawGzDeg - (_gyroBiasZ + _biasDynZ);

        _biasDynX += ex*s_biasAlpha;
        _biasDynY += ey*s_biasAlpha;
        _biasDynZ += ez*s_biasAlpha;

        _biasDynX = constrain(_biasDynX,-20.0f,20.0f);
        _biasDynY = constrain(_biasDynY,-20.0f,20.0f);
        _biasDynZ = constrain(_biasDynZ,-20.0f,20.0f);
    }

    // ----- adaptive smoothing -----
    float computeSmooth_(float dtAvgMs, bool bleConnected){
        float j = constrain((dtAvgMs - 8.0f)/10.0f, 0.0f, 1.0f);
        float sm = 0.72f + 0.20f*j;          // 0.72~0.92
        if(!bleConnected) sm -= 0.06f;
        sm = constrain(sm,0.60f,0.95f);
        _lastOutSmooth = sm;
        return sm;
    }

    void smooth2_(float& fx, float& fy, float sm, float& smx, float& smy){
        smx = smx*sm + fx*(1.0f-sm);
        smy = smy*sm + fy*(1.0f-sm);
        fx=smx; fy=smy;
    }

    // ---- Modifier policy ----
    void tapComboUsageKb_(uint8_t p_modMask, uint8_t p_usage, uint16_t p_ms=22){
        if(p_usage==0) return;
        if(p_modMask) _keyboard.modifierKeyPress(p_modMask);
        _keyboard.keyPress(p_usage);
        vTaskDelay(pdMS_TO_TICKS(p_ms));
        _keyboard.keyRelease(p_usage);
        if(p_modMask) _keyboard.modifierKeyRelease(p_modMask);
    }
    void tapUsageKb_(uint8_t p_usage, uint16_t p_ms=12){
        if(p_usage==0) return;
        _keyboard.keyPress(p_usage);
        vTaskDelay(pdMS_TO_TICKS(p_ms));
        _keyboard.keyRelease(p_usage);
    }
    void tapConsumerMask_(uint32_t p_mask, uint16_t p_ms=28){
        if(p_mask==0) return;
        _keyboard.mediaKeyPress(p_mask);
        vTaskDelay(pdMS_TO_TICKS(p_ms));
        _keyboard.mediaKeyRelease(p_mask);
    }

    void sendPptKey2_(uint8_t p_page, uint8_t p_mod, uint32_t p_code){
        if(p_page == (uint8_t)EN_C10_KEYPAGE_CONSUMER){
            tapConsumerMask_(p_code);
            return;
        }
        uint8_t usage=(uint8_t)min((uint32_t)0xE7, p_code);
        if(p_mod) tapComboUsageKb_(p_mod, usage);
        else tapUsageKb_(usage);
    }
    void sendPptKey2FromCfg_(const ST_C10_PptKey2_t& k){ sendPptKey2_(k.page, k.mod, k.code); }

    void processGesturesDeg_(float gzDeg){
        static unsigned long last=0;
        if(millis()-last<_gestureCooldownMs) return;
        if(gzDeg>_gestureFlickDeg){ sendPptKey2FromCfg_(_ppt2_prev); last=millis(); }
        else if(gzDeg<-_gestureFlickDeg){ sendPptKey2FromCfg_(_ppt2_next); last=millis(); }
    }

    void applyPrecision_(float& fx, float& fy){
        if(!_precisionMode) return;

        if(fabsf(fx)<_precDeadzone) fx=0;
        if(fabsf(fy)<_precDeadzone) fy=0;

        auto shape=[&](float v)->float{
            float a=fabsf(v);
            if(a<0.0001f) return 0;
            float n=min(1.0f, a/(float)_precMaxStep);
            float b=n + (_precAccel*n*n);
            float out=b*(float)_precMaxStep;
            out*=_precGain;
            return (v>=0)?out:-out;
        };

        float tx=constrain(shape(fx), -(float)_precMaxStep, (float)_precMaxStep);
        float ty=constrain(shape(fy), -(float)_precMaxStep, (float)_precMaxStep);

        float sm = (_precProfile==1) ? min(0.95f, max(0.60f, _precSmooth)) : _precSmooth;
        _precSmX = _precSmX*sm + tx*(1.0f-sm);
        _precSmY = _precSmY*sm + ty*(1.0f-sm);
        fx=_precSmX; fy=_precSmY;
    }

    void fsmUpdate_(bool btnScroll, bool btnModeLongToggle, float gyroAbs){
        if(btnModeLongToggle){
            _isPptMode = !_isPptMode;
        }

        if(btnScroll){
            _fsm = EN_FSM_SCROLL;
            if(_precisionMode){ _precisionMode=false; _precSub=EN_PREC_OFF; }
            return;
        }

        if(_precisionMode && _precisionEnable){
            _fsm = EN_FSM_PREC;
            const uint32_t now=(uint32_t)millis();
            if(_precSub==EN_PREC_ENTRY){
                if(gyroAbs <= _precEntryStillDeg){
                    if(now - _precT0 >= _precEntryMs) _precSub=EN_PREC_TRACK;
                }else _precT0=now;
            } else if(_precSub==EN_PREC_TRACK){
                if(gyroAbs >= _precExitMoveDeg){
                    _precSub=EN_PREC_EXIT;
                    _precT0=now;
                }
            } else if(_precSub==EN_PREC_EXIT){
                if(gyroAbs <= _precEntryStillDeg){
                    if(now - _precT0 >= _precExitMs) _precSub=EN_PREC_TRACK;
                } else _precT0=now;
            } else {
                _precSub=EN_PREC_ENTRY;
                _precT0=now;
            }
            return;
        }

        _precSub=EN_PREC_OFF;
        _fsm = _isPptMode ? EN_FSM_PPT : EN_FSM_AIR;
    }

    void welfordAdd_(uint32_t& n, double& mean, double& m2, double x){
        n++; double d=x-mean; mean += d/(double)n; double d2=x-mean; m2 += d*d2;
        if(n>2500){ n=1; mean=x; m2=0; }
    }
    float calcRms_(uint32_t n, double m2){
        if(n<2) return 0;
        double var=m2/(double)(n-1);
        if(var<0) var=0;
        return (float)sqrt(var);
    }

    void pushErr_(uint8_t code, uint16_t value=0){
        ST_E10_ErrEvt_t e; e.ts_ms=(uint32_t)(millis()-_uptime0); e.code=code; e.value=value;
        _errHist[_errHistHead]=e;
        _errHistHead=(uint8_t)((_errHistHead+1)%E10_CONST::ERR_HIST_CAP);
        if(_errHistCount<E10_CONST::ERR_HIST_CAP) _errHistCount++;
    }

    void pushSpike_(uint32_t ts_ms){
        _spikes[_spikeHead].ts_ms=ts_ms;
        _spikeHead=(uint8_t)((_spikeHead+1)%E10_CONST::SPIKE_CAP);
        if(_spikeCount<E10_CONST::SPIKE_CAP) _spikeCount++;
    }

    bool recoverI2C_(){
        const int sda=4, scl=5;
        pinMode(sda, INPUT_PULLUP);
        pinMode(scl, OUTPUT_OPEN_DRAIN);

        for(int i=0;i<9;i++){
            digitalWrite(scl, HIGH); delayMicroseconds(6);
            digitalWrite(scl, LOW);  delayMicroseconds(6);
        }
        digitalWrite(scl, HIGH); delayMicroseconds(6);

        Wire.end(); delay(5);
        Wire.begin(sda,scl); Wire.setClock(400000); delay(5);

        bool ok=_mpu.begin();
        if(ok){
            _mpu.setGyroRange(MPU6050_RANGE_250_DEG);
            _mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
            _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
        }
        _i2cRecoverCount++;
        _i2cRecoverLastOk=ok;

        if(ok) _consecutiveRecoverFail=0;
        else { _consecutiveRecoverFail++; _consecutiveFail++; }

        pushErr_(ok?EN_E10_ERR_I2C_RECOVER_OK:EN_E10_ERR_I2C_RECOVER_FAIL, 0);
        return ok;
    }

    void runGyroCalibration_(){
        const uint32_t t0=millis();
        uint32_t cnt=0;
        double sx=0, sy=0, sz=0;
        while(millis()-t0 < E10_CONST::CALIB_MS){
            sensors_event_t a,g,t;
            _mpu.getEvent(&a,&g,&t);
            float gx=g.gyro.x*RAD_TO_DEG;
            float gy=g.gyro.y*RAD_TO_DEG;
            float gz=g.gyro.z*RAD_TO_DEG;
            float m=max(max(fabsf(gx),fabsf(gy)),fabsf(gz));
            if(m < E10_CONST::CALIB_STILL_TH){ sx+=gx; sy+=gy; sz+=gz; cnt++; }
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        if(cnt>0){ _gyroBiasX=(float)(sx/cnt); _gyroBiasY=(float)(sy/cnt); _gyroBiasZ=(float)(sz/cnt); }
        _gyroCalibDone=true;
    }

    static void mouseSend_(MouseDevice& ms, int8_t dx, int8_t dy, int8_t wheel){
        ms.mouseMove(dx,dy,wheel,0);
    }

    void lock_(){ if(_mutex) (void)xSemaphoreTake(_mutex, portMAX_DELAY); }
    void unlock_(){ if(_mutex) xSemaphoreGive(_mutex); }

    static void sensorTask(void* pv){
        CL_E10_EliteAirMouse* m=(CL_E10_EliteAirMouse*)pv;
        TickType_t lastWake=xTaskGetTickCount();
        unsigned long lastUs=micros();
        unsigned long btnDownMs=0;

        if(!m->_gyroCalibDone) m->runGyroCalibration_();

        for(;;){
            sensors_event_t a,g,t;
            m->_mpu.getEvent(&a,&g,&t);
            m->_tempC=t.temperature;

            unsigned long nowUs=micros();
            float dt=(nowUs-lastUs)/1000000.0f;
            lastUs=nowUs;
            float dtMs=dt*1000.0f;
            m->_dtAvgMs = m->_dtAvgMs*0.98f + dtMs*0.02f;

            const bool scrollMode = (digitalRead(E10_CONST::PIN_BTN_SCROLL)==LOW);

            bool modeLongToggle=false;
            if(digitalRead(E10_CONST::PIN_BTN_MODE)==LOW){
                if(btnDownMs==0) btnDownMs=millis();
            }else{
                if(btnDownMs>0){
                    unsigned long hold=millis()-btnDownMs;
                    if(hold>1000) modeLongToggle=true;
                    else {
                        m->lock_();
                        m->_dpiLevel++; if(m->_dpiLevel>3)m->_dpiLevel=1;
                        m->_engine.setDPI(m->_dpiLevel);
                        m->unlock_();
                    }
                    btnDownMs=0;
                }
            }

            const float rawGx = (g.gyro.x*RAD_TO_DEG);
            const float rawGy = (g.gyro.y*RAD_TO_DEG);
            const float rawGz = (g.gyro.z*RAD_TO_DEG);

            float gx = rawGx - (m->_gyroBiasX + m->_biasDynX);
            float gy = rawGy - (m->_gyroBiasY + m->_biasDynY);
            float gz = rawGz - (m->_gyroBiasZ + m->_biasDynZ);

            const float gyroAbs=max(max(fabsf(gx),fabsf(gy)),fabsf(gz));

            const uint32_t ts=(uint32_t)(millis()-m->_uptime0);
            if(fabsf(gz) > E10_CONST::SPIKE_TH_DEG) m->pushSpike_(ts);

            if(m->stillForBias_(gyroAbs)){
                m->trackBias_(rawGx,rawGy,rawGz);
                gx = rawGx - (m->_gyroBiasX + m->_biasDynX);
                gy = rawGy - (m->_gyroBiasY + m->_biasDynY);
                gz = rawGz - (m->_gyroBiasZ + m->_biasDynZ);
            }

            m->lock_();
            m->fsmUpdate_(scrollMode, modeLongToggle, gyroAbs);
            const uint8_t fsm = m->_fsm;
            const bool precisionMode = m->_precisionMode;
            const bool bleConn = m->_hid.isConnected();
            const float sm = m->computeSmooth_(m->_dtAvgMs, bleConn);
            m->unlock_();

            if(isnan(gx)||isnan(gy)||isnan(gz)){
                m->_errMpuNan++;
                m->_consecutiveFail++;
                m->pushErr_(EN_E10_ERR_MPU_NAN,0);
                if((m->_errMpuNan % 5)==0) (void)m->recoverI2C_();
                vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(8));
                continue;
            } else {
                if(m->_consecutiveFail>0) m->_consecutiveFail--;
            }

            m->welfordAdd_(m->_gyroN, m->_gyroMean, m->_gyroM2, (double)gz);

            m->_engine.updateOrientation(a.acceleration.y, a.acceleration.z, gx, dt);

            int tx=0, ty=0;
            m->_engine.process(-gz, -gx, tx, ty);

            const bool leftClick=(digitalRead(E10_CONST::PIN_BTN_L)==LOW);
            if(leftClick) m->_engine.notifyClick();

            // accel shaping
            m->lock_();
            const float base=m->_scaleBase[m->_dpiLevel-1];
            const float accg=m->_accelGain[m->_dpiLevel-1];
            const float accTh=m->_accelTh;
            const float wheelTh=m->_wheelThDeg;
            const int   wheelMax=m->_wheelStepMax;
            const float damp=m->_scrollCursorDamp;
            const bool  pptMode=m->_isPptMode;
            m->unlock_();

            float mag=sqrtf((float)tx*(float)tx + (float)ty*(float)ty);
            float acc=1.0f;
            if(mag > accTh){
                float ex=(mag - accTh);
                acc = 1.0f + (accg*(ex/(ex+18.0f)));
            }
            float fx=(float)tx*base*acc;
            float fy=(float)ty*base*acc;

            int wheel=0;

            if(fsm==EN_FSM_SCROLL){
                if(gy > wheelTh){
                    float n=(gy-wheelTh)/120.0f; if(n>1)n=1;
                    wheel = (int)(1 + (n*(wheelMax-1)));
                }else if(gy < -wheelTh){
                    float n=(-gy-wheelTh)/120.0f; if(n>1)n=1;
                    wheel = -(int)(1 + (n*(wheelMax-1)));
                }
                fx*=damp; fy*=damp;
                m->smooth2_(fx,fy, min(0.90f,max(0.65f,sm)), m->_outSmX, m->_outSmY);

                m->writeState_((int16_t)constrain((int)fx,-32767,32767),
                               (int16_t)constrain((int)fy,-32767,32767),
                               (int16_t)constrain(wheel,-32767,32767),
                               leftClick);
            } else {
                if(fsm==EN_FSM_PREC && precisionMode){
                    m->lock_(); m->applyPrecision_(fx,fy); m->unlock_();
                    m->smooth2_(fx,fy, min(0.95f,max(0.70f,sm)), m->_outPrecSmX, m->_outPrecSmY);
                } else {
                    m->smooth2_(fx,fy, sm, m->_outSmX, m->_outSmY);
                }

                if(fsm==EN_FSM_PPT && pptMode){
                    m->lock_(); m->processGesturesDeg_(gz); m->unlock_();
                }

                m->welfordAdd_(m->_curN, m->_curMean, m->_curM2, (double)sqrtf(fx*fx+fy*fy));

                m->writeState_((int16_t)constrain((int)fx,-32767,32767),
                               (int16_t)constrain((int)fy,-32767,32767),
                               0,
                               leftClick);
            }

            TickType_t before=xTaskGetTickCount();
            vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(8));
            TickType_t after=xTaskGetTickCount();
            if((after-before)==0){
                m->_errTaskOverrun++;
                if((m->_errTaskOverrun%10)==0) m->pushErr_(EN_E10_ERR_TASK_OVERRUN,0);
            }
        }
    }

    static void commTask(void* pv){
        CL_E10_EliteAirMouse* m=(CL_E10_EliteAirMouse*)pv;
        ST_E10_State_t st; memset(&st,0,sizeof(st));

        for(;;){
            if(m->_hid.isConnected()){
                if(m->readState_(st)){
                    int8_t dx=(int8_t)constrain((int)st.x,-127,127);
                    int8_t dy=(int8_t)constrain((int)st.y,-127,127);
                    int8_t wh=(int8_t)constrain((int)st.wheel,-127,127);
                    mouseSend_(m->_mouse, dx, dy, wh);
                }

                if(st.btn_mask & 0x01) m->_mouse.mousePress(E10_CONST::MOUSE_BTN_LEFT);
                else m->_mouse.mouseRelease(E10_CONST::MOUSE_BTN_LEFT);
            }
            vTaskDelay(pdMS_TO_TICKS(7));
        }
    }
};

