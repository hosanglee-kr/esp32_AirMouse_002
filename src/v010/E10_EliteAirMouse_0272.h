// =======================================================
// File: src/v0272/E10_EliteAirMouse_0272.h
// =======================================================
#pragma once
/*
 * (022 구조 유지 가정)
 * v0272:
 *  - Motion FSM 강화(Scroll/PPT/Precision)
 *  - Precision FSM: entry/track/exit + profile(joystick-like)
 *  - HID modifier 정책 확정: mod mask == HID modifier byte (W10와 1:1)
 *  - status: gyro/cursor RMS + spike + consecutive fail + err hist + i2c recover
 */
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <BleCompositeHID.h>
#include <KeyboardDevice.h>
#include <MouseDevice.h>

#include "A40_ComFunc_070.h"
#include "C10_Config_0273.h"
#include "M10_MotionProc_020.h"

enum EN_E10_Health_t : uint8_t { EN_E10_HEALTH_OK=0, EN_E10_HEALTH_WARN=1, EN_E10_HEALTH_DEGRADED=2 };

enum EN_E10_ErrCode_t : uint8_t {
    EN_E10_ERR_NONE=0,
    EN_E10_ERR_MPU_NAN=1,
    EN_E10_ERR_MUTEX_MISS=2,
    EN_E10_ERR_TASK_OVERRUN=3,
    EN_E10_ERR_I2C_RECOVER_OK=4,
    EN_E10_ERR_I2C_RECOVER_FAIL=5,
    EN_E10_ERR_OTA_GUARD=6
};

struct ST_E10_ErrEvt_t { uint32_t ts_ms; uint8_t code; uint16_t value; };

struct ST_E10_Status_t {
    bool     ble_connected;
    bool     ppt_mode;
    uint8_t  dpi_level;

    bool     precision_enable;
    bool     precision_mode;
    uint8_t  fsm_state;       // 디버깅용
    uint8_t  fsm_sub;         // precision substate

    uint8_t  health;
    uint16_t health_score;

    float gyro_bias_x, gyro_bias_y, gyro_bias_z;
    float temp_c;

    float gyro_rms;
    float cursor_rms;

    uint32_t sampling_ms_target;
    float    sampling_ms_avg;

    uint32_t i2c_recover_count;
    bool     i2c_recover_last_ok;

    uint32_t err_mpu_nan;
    uint32_t err_mutex_miss;
    uint32_t err_task_overrun;

    uint8_t  err_hist_n;
    ST_E10_ErrEvt_t err_hist[16];

    uint16_t spike_count_10s;
    uint16_t consecutive_fail;
    uint16_t consecutive_recover_fail;

    uint32_t uptime_ms;
};

class CL_E10_EliteAirMouse {
  private:
    Adafruit_MPU6050 _mpu;

    BleCompositeHID _hid;
    KeyboardDevice  _keyboard;
    MouseDevice     _mouse;

    CL_M10_AdvancedMotionProcessor _engine;
    CL_C10_Config* _cfg = nullptr;

    static constexpr int s_btnL=12, s_btnMode=13, s_btnScroll=14;
    static constexpr uint8_t s_mouseBtnLeft = 0x01;

    struct ST_State { int x,y,wheel; bool updated; } _state;

    SemaphoreHandle_t _mutex = nullptr;

    // gyro calib
    float _gyroBiasX=0, _gyroBiasY=0, _gyroBiasZ=0;
    bool  _gyroCalibDone=false;
    static constexpr uint32_t s_calibMs=1000;
    static constexpr float    s_calibStillThDeg=3.0f;

    // runtime config (applied)
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

    // precision config + fsm knobs
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
    uint8_t _precProfile=1; // 1=joystick-like

    // PPT v2
    ST_C10_PptKey2_t _ppt2_start, _ppt2_exit, _ppt2_next, _ppt2_prev, _ppt2_black, _ppt2_laser;

    float _tempC=0;
    uint32_t _uptime0=0;
    float _dtAvgMs=8.0f;

    // errors
    uint32_t _errMpuNan=0, _errMutexMiss=0, _errTaskOverrun=0;

    // RMS Welford
    uint32_t _gyroN=0; double _gyroMean=0, _gyroM2=0;
    uint32_t _curN=0;  double _curMean=0,  _curM2=0;

    // i2c recover
    uint32_t _i2cRecoverCount=0;
    bool     _i2cRecoverLastOk=true;

    // history ring
    static constexpr uint8_t s_errHistCap=16;
    ST_E10_ErrEvt_t _errHist[s_errHistCap];
    uint8_t _errHistHead=0, _errHistCount=0;

    // anomaly
    static constexpr float s_spikeThDeg = 650.0f; // 튜닝 포인트
    struct ST_SpikeEvt { uint32_t ts_ms; };
    ST_SpikeEvt _spikes[32];
    uint8_t _spikeHead=0, _spikeCount=0;
    uint16_t _consecutiveFail=0;
    uint16_t _consecutiveRecoverFail=0;

    // -------- Motion FSM --------
    // State: 0=AIR, 1=SCROLL, 2=PPT, 3=PRECISION
    enum EN_FSM_t : uint8_t { EN_FSM_AIR=0, EN_FSM_SCROLL=1, EN_FSM_PPT=2, EN_FSM_PREC=3 };

    // Precision sub: 0=OFF, 1=ENTRY, 2=TRACK, 3=EXIT
    enum EN_PREC_SUB_t : uint8_t { EN_PREC_OFF=0, EN_PREC_ENTRY=1, EN_PREC_TRACK=2, EN_PREC_EXIT=3 };

    uint8_t _fsm = EN_FSM_AIR;
    uint8_t _precSub = EN_PREC_OFF;
    uint32_t _precT0 = 0;

  public:
    CL_E10_EliteAirMouse() : _hid("Elite AirMouse S3", "ProMaker", 100) {
        _state={0,0,0,false};
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

        pinMode(s_btnL,INPUT_PULLUP);
        pinMode(s_btnMode,INPUT_PULLUP);
        pinMode(s_btnScroll,INPUT_PULLUP);

        _mutex=xSemaphoreCreateMutex();

        (void)applyFromConfig();

        _hid.addDevice(&_keyboard);
        _hid.addDevice(&_mouse);
        _hid.begin();

        xTaskCreatePinnedToCore(sensorTask,"E10_Sensor",8192,this,3,nullptr,1);
        xTaskCreatePinnedToCore(commTask,"E10_Comm",4096,this,2,nullptr,0);
    }

    // W10 apply hook
    static bool E10_W10Apply(void* p_ctx){
        if(!p_ctx) return false;
        return ((CL_E10_EliteAirMouse*)p_ctx)->applyFromConfig();
    }

    // 런타임 apply-only (저장 없이 UI에서 반영)
    bool applyRuntimeE10(const ST_C10_E10Config_t& p_e){
        lock_();
        applyE10ToRuntime_(p_e);
        unlock_();
        return true;
    }

    // control
    bool setPptMode(bool p_enable){ lock_(); _isPptMode=p_enable; unlock_(); return true; }
    bool setDpiLevel(uint8_t p_level){
        if(p_level<1)p_level=1; if(p_level>3)p_level=3;
        lock_(); _dpiLevel=(int)p_level; _engine.setDPI(_dpiLevel); unlock_(); return true;
    }

    bool setPrecisionMode(bool p_enable){
        lock_();
        if(!_precisionEnable){ _precisionMode=false; _fsm=EN_FSM_AIR; _precSub=EN_PREC_OFF; unlock_(); return true; }
        _precisionMode = p_enable;
        if(_precisionMode){
            _fsm=EN_FSM_PREC; _precSub=EN_PREC_ENTRY; _precT0=(uint32_t)millis();
        }else{
            _fsm=(_isPptMode?EN_FSM_PPT:EN_FSM_AIR);
            _precSub=EN_PREC_OFF;
        }
        _precSmX=0; _precSmY=0;
        unlock_();
        return true;
    }

    // /api/ppt/test
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
        p_out.temp_c=_tempC;

        p_out.sampling_ms_target=8;
        p_out.sampling_ms_avg=_dtAvgMs;

        p_out.i2c_recover_count=_i2cRecoverCount;
        p_out.i2c_recover_last_ok=_i2cRecoverLastOk;

        p_out.err_mpu_nan=_errMpuNan;
        p_out.err_mutex_miss=_errMutexMiss;
        p_out.err_task_overrun=_errTaskOverrun;

        p_out.uptime_ms=(uint32_t)(millis()-_uptime0);

        p_out.gyro_rms=calcRms_(_gyroN,_gyroM2);
        p_out.cursor_rms=calcRms_(_curN,_curM2);

        // anomaly snapshot (최근 10초 spike)
        const uint32_t now = (uint32_t)(millis()-_uptime0);
        uint16_t sc=0;
        for(uint8_t i=0;i<_spikeCount;i++){
            int idx=(int)_spikeHead-1-(int)i;
            if(idx<0) idx+=32;
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

        // history newest-first
        p_out.err_hist_n = (uint8_t)min((uint8_t)s_errHistCap,_errHistCount);
        for(uint8_t i=0;i<p_out.err_hist_n;i++){
            int idx=(int)_errHistHead-1-(int)i;
            if(idx<0) idx+=s_errHistCap;
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

    // ---- Modifier policy ----
    // p_modMask == HID report modifier byte (W10 mods mask와 1:1)
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
        uint8_t usage = (uint8_t)min((uint32_t)0xE7, p_code);
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

    // precision shaping (joystick-like)
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

        // profile 1: joystick-like = 더 강한 smoothing + 작은 움직임 유지
        float sm = (_precProfile==1) ? min(0.95f, max(0.60f, _precSmooth)) : _precSmooth;

        _precSmX = _precSmX*sm + tx*(1.0f-sm);
        _precSmY = _precSmY*sm + ty*(1.0f-sm);
        fx=_precSmX; fy=_precSmY;
    }

    // ---- FSM update ----
    void fsmUpdate_(bool btnScroll, bool btnModeLongToggle, float gyroAbs){
        // ppt toggle is handled in sensor task (btnModeLongToggle)
        if(btnModeLongToggle){
            _isPptMode = !_isPptMode;
        }

        // highest priority: scroll
        if(btnScroll){
            _fsm = EN_FSM_SCROLL;
            if(_precisionMode){ _precisionMode=false; _precSub=EN_PREC_OFF; }
            return;
        }

        // precision (if enabled and user toggled precision_mode via api/control)
        if(_precisionMode && _precisionEnable){
            _fsm = EN_FSM_PREC;
            // entry/exit conditions based on still/move
            const uint32_t now=(uint32_t)millis();
            if(_precSub==EN_PREC_ENTRY){
                if(gyroAbs <= _precEntryStillDeg){
                    if(now - _precT0 >= _precEntryMs){
                        _precSub=EN_PREC_TRACK;
                    }
                }else{
                    // 움직이면 entry timer 리셋
                    _precT0=now;
                }
            } else if(_precSub==EN_PREC_TRACK){
                if(gyroAbs >= _precExitMoveDeg){
                    _precSub=EN_PREC_EXIT;
                    _precT0=now;
                }
            } else if(_precSub==EN_PREC_EXIT){
                if(gyroAbs <= _precEntryStillDeg){
                    if(now - _precT0 >= _precExitMs){
                        _precSub=EN_PREC_TRACK; // 안정되면 다시 track
                    }
                } else {
                    // 계속 움직이면 exit 유지
                    _precT0=now;
                }
            } else {
                _precSub=EN_PREC_ENTRY;
                _precT0=now;
            }
            return;
        }

        _precSub=EN_PREC_OFF;

        // ppt vs air
        _fsm = _isPptMode ? EN_FSM_PPT : EN_FSM_AIR;
    }

    // ---- stats helpers ----
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
        _errHistHead=(uint8_t)((_errHistHead+1)%s_errHistCap);
        if(_errHistCount<s_errHistCap) _errHistCount++;
    }

    void pushSpike_(uint32_t ts_ms){
        _spikes[_spikeHead].ts_ms=ts_ms;
        _spikeHead=(uint8_t)((_spikeHead+1)%32);
        if(_spikeCount<32) _spikeCount++;
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
        while(millis()-t0 < s_calibMs){
            sensors_event_t a,g,t;
            _mpu.getEvent(&a,&g,&t);
            float gx=g.gyro.x*RAD_TO_DEG;
            float gy=g.gyro.y*RAD_TO_DEG;
            float gz=g.gyro.z*RAD_TO_DEG;
            float m=max(max(fabsf(gx),fabsf(gy)),fabsf(gz));
            if(m < s_calibStillThDeg){ sx+=gx; sy+=gy; sz+=gz; cnt++; }
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

            const bool scrollMode = (digitalRead(s_btnScroll)==LOW);

            // mode short/long
            bool modeLongToggle=false;
            if(digitalRead(s_btnMode)==LOW){
                if(btnDownMs==0) btnDownMs=millis();
            }else{
                if(btnDownMs>0){
                    unsigned long hold=millis()-btnDownMs;
                    if(hold>1000) modeLongToggle=true;
                    else { m->_dpiLevel++; if(m->_dpiLevel>3)m->_dpiLevel=1; m->_engine.setDPI(m->_dpiLevel); }
                    btnDownMs=0;
                }
            }

            float gx=(g.gyro.x*RAD_TO_DEG)-m->_gyroBiasX;
            float gy=(g.gyro.y*RAD_TO_DEG)-m->_gyroBiasY;
            float gz=(g.gyro.z*RAD_TO_DEG)-m->_gyroBiasZ;

            const float gyroAbs=max(max(fabsf(gx),fabsf(gy)),fabsf(gz));

            // spike
            const uint32_t ts=(uint32_t)(millis()-m->_uptime0);
            if(fabsf(gz) > s_spikeThDeg) m->pushSpike_(ts);

            // FSM 결정 (우선순위 + precision)
            m->fsmUpdate_(scrollMode, modeLongToggle, gyroAbs);

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

            const bool leftClick=(digitalRead(s_btnL)==LOW);
            if(leftClick) m->_engine.notifyClick();

            // accel shaping
            float base=m->_scaleBase[m->_dpiLevel-1];
            float accg=m->_accelGain[m->_dpiLevel-1];
            float mag=sqrtf((float)tx*(float)tx + (float)ty*(float)ty);
            float acc=1.0f;
            if(mag > m->_accelTh){
                float ex=(mag - m->_accelTh);
                acc = 1.0f + (accg*(ex/(ex+18.0f)));
            }
            float fx=(float)tx*base*acc;
            float fy=(float)ty*base*acc;

            // FSM 적용
            if(m->_fsm==EN_FSM_SCROLL){
                // scroll: wheel only, cursor damp
                int wheel=0;
                if(gy > m->_wheelThDeg){
                    float n=(gy-m->_wheelThDeg)/120.0f; if(n>1)n=1;
                    wheel = (int)(1 + (n*(m->_wheelStepMax-1)));
                }else if(gy < -m->_wheelThDeg){
                    float n=(-gy-m->_wheelThDeg)/120.0f; if(n>1)n=1;
                    wheel = -(int)(1 + (n*(m->_wheelStepMax-1)));
                }
                fx*=m->_scrollCursorDamp;
                fy*=m->_scrollCursorDamp;

                if(xSemaphoreTake(m->_mutex,0)==pdTRUE){
                    m->_state.x=(int)fx; m->_state.y=(int)fy; m->_state.wheel=wheel; m->_state.updated=true;
                    xSemaphoreGive(m->_mutex);
                }else{
                    m->_errMutexMiss++;
                    m->pushErr_(EN_E10_ERR_MUTEX_MISS,0);
                }
            } else {
                // AIR/PPT/PREC cursor
                if(m->_fsm==EN_FSM_PREC){
                    m->applyPrecision_(fx,fy);
                }
                if(m->_fsm==EN_FSM_PPT){
                    m->processGesturesDeg_(gz);
                }

                m->welfordAdd_(m->_curN, m->_curMean, m->_curM2, (double)sqrtf(fx*fx+fy*fy));

                if(xSemaphoreTake(m->_mutex,0)==pdTRUE){
                    m->_state.x=(int)fx; m->_state.y=(int)fy; m->_state.wheel=0; m->_state.updated=true;
                    xSemaphoreGive(m->_mutex);
                }else{
                    m->_errMutexMiss++;
                    m->pushErr_(EN_E10_ERR_MUTEX_MISS,0);
                }
            }

            if(m->_hid.isConnected()){
                if(leftClick) m->_mouse.mousePress(s_mouseBtnLeft);
                else m->_mouse.mouseRelease(s_mouseBtnLeft);
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
        for(;;){
            if(m->_hid.isConnected() && xSemaphoreTake(m->_mutex, portMAX_DELAY)==pdTRUE){
                if(m->_state.updated){
                    int8_t dx=(int8_t)constrain(m->_state.x,-127,127);
                    int8_t dy=(int8_t)constrain(m->_state.y,-127,127);
                    int8_t wh=(int8_t)constrain(m->_state.wheel,-127,127);
                    mouseSend_(m->_mouse, dx, dy, wh);
                    m->_state.updated=false;
                }
                xSemaphoreGive(m->_mutex);
            }
            vTaskDelay(pdMS_TO_TICKS(7));
        }
    }
};

