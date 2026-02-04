#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : E10_EliteAirMouse_025.h
 * 모듈약어 : E10
 * 모듈명 : AirMouse+Presenter (Composite HID, Field Dashboard v0.2.5)
 * ------------------------------------------------------
 * 기능 요약
 *  - MPU6050 기반 에어마우스 + Composite HID(Mouse+Keyboard)
 *  - Gyro 오프셋 자동 캘리브레이션(부팅 1초 평균, 움직임 큰 샘플 제외)
 *  - BTN_SCROLL 전용 버튼 분리(스크롤 모드 시 커서 이동 억제)
 *  - Hard Click-Lock 옵션
 *  - /api/status 제공용 상태 구조체(gyro bias/temp/rms/err/anomaly/health 등)
 *  - PPT Keymap v2: kb(0x07) + consumer(media 32-bit mask) 전송
 *  - ✅ 런타임 PPT 오버레이: UI Apply(no save) 즉시 적용, 재부팅 시 원복
 *  - ✅ Modifier 정책 확정: W10 mods mask == HID modifier byte(1:1) (KeyboardDevice.modifierKeyPress 사용)
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
 *   - 클래스 private 멤버   : _ 접두사
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

#include "M10_MotionProc_020.h"
#include "C10_Config_025.h" // ✅ (프로젝트에서 025로 올렸다면 교체)

// ---- Health / Err codes ----
enum EN_E10_Health_t : uint8_t { EN_E10_HEALTH_OK=0, EN_E10_HEALTH_WARN=1, EN_E10_HEALTH_DEGRADED=2 };

// err codes (표준화) - W10 alerts와 1:1 매핑 가능
enum EN_E10_ErrCode_t : uint8_t {
    EN_E10_ERR_MPU_NAN = 1,
    EN_E10_ERR_MUTEX_MISS = 2,
    EN_E10_ERR_TASK_OVERRUN = 3,
    EN_E10_ERR_I2C_RECOVER_OK = 4,
    EN_E10_ERR_I2C_RECOVER_FAIL = 5,
};

struct ST_E10_ErrEvt_t { uint32_t ts_ms; uint8_t code; uint16_t value; };

struct ST_E10_Status_t {
    bool     ble_connected;
    bool     ppt_mode;
    uint8_t  dpi_level;
    bool     precision_mode;

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

    // anomaly
    uint16_t spike_count_10s;
    uint16_t consecutive_fail;
    uint16_t consecutive_recover_fail;

    // runtime overlay (디버그/현장 확인용)
    bool     ppt_runtime_override;

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

    // gyro bias
    float _gyroBiasX=0, _gyroBiasY=0, _gyroBiasZ=0;
    bool  _gyroCalibDone=false;
    static constexpr uint32_t s_calibMs=1000;
    static constexpr float    s_calibStillThDeg=3.0f;

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

    // precision (P2)
    bool  _precisionEnable=false;
    bool  _precisionMode=false;
    float _precDeadzone=1.2f, _precGain=0.65f, _precAccel=0.25f;
    uint8_t _precMaxStep=18;
    float _precSmooth=0.85f;
    float _precSmX=0, _precSmY=0;

    // PPT v2 (config-backed)
    ST_C10_PptKey2_t _ppt2_start, _ppt2_exit, _ppt2_next, _ppt2_prev, _ppt2_black, _ppt2_laser;

    // ✅ runtime overlay (Apply without save)
    bool _pptRtValid=false;
    ST_C10_PptKey2_t _pptRt_start, _pptRt_exit, _pptRt_next, _pptRt_prev, _pptRt_black, _pptRt_laser;

    // status vars
    float _tempC=0;
    uint32_t _uptime0=0;
    float _dtAvgMs=8.0f;

    // errors
    uint32_t _errMpuNan=0, _errMutexMiss=0, _errTaskOverrun=0;

    // RMS (Welford)
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
    static constexpr float s_spikeThDeg = 650.0f; // 튜닝포인트
    struct ST_SpikeEvt { uint32_t ts_ms; };
    ST_SpikeEvt _spikes[32];
    uint8_t _spikeHead=0, _spikeCount=0;
    uint16_t _consecutiveFail=0;
    uint16_t _consecutiveRecoverFail=0;

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

        memset(&_pptRt_start,0,sizeof(_pptRt_start));
        memset(&_pptRt_exit,0,sizeof(_pptRt_exit));
        memset(&_pptRt_next,0,sizeof(_pptRt_next));
        memset(&_pptRt_prev,0,sizeof(_pptRt_prev));
        memset(&_pptRt_black,0,sizeof(_pptRt_black));
        memset(&_pptRt_laser,0,sizeof(_pptRt_laser));
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

    static bool E10_W10Apply(void* p_ctx){
        if(!p_ctx) return false;
        return ((CL_E10_EliteAirMouse*)p_ctx)->applyFromConfig();
    }

    // ---- runtime control ----
    bool setPptMode(bool p_enable){ lock_(); _isPptMode=p_enable; unlock_(); return true; }

    bool setDpiLevel(uint8_t p_level){
        if(p_level<1)p_level=1;
        if(p_level>3)p_level=3;
        lock_();
        _dpiLevel=(int)p_level;
        _engine.setDPI(_dpiLevel);
        unlock_();
        return true;
    }

    bool setPrecisionMode(bool p_enable){
        lock_();
        _precisionMode = p_enable && _precisionEnable;
        _precSmX=0; _precSmY=0;
        unlock_();
        return true;
    }

    // ✅ 런타임 PPT 맵 적용 (save=false 경로)
    bool setPptMapRuntime(const ST_C10_PptKey2_t& p_start,
                          const ST_C10_PptKey2_t& p_exit,
                          const ST_C10_PptKey2_t& p_next,
                          const ST_C10_PptKey2_t& p_prev,
                          const ST_C10_PptKey2_t& p_black,
                          const ST_C10_PptKey2_t& p_laser){
        lock_();
        _pptRt_start=p_start;
        _pptRt_exit =p_exit;
        _pptRt_next =p_next;
        _pptRt_prev =p_prev;
        _pptRt_black=p_black;
        _pptRt_laser=p_laser;
        _pptRtValid=true;
        unlock_();
        return true;
    }

    bool clearPptMapRuntime(){
        lock_();
        _pptRtValid=false;
        unlock_();
        return true;
    }

    // ✅ /api/ppt/test에서 사용 (1회 전송)
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
        p_out.precision_mode=_precisionMode;

        p_out.gyro_bias_x=_gyroBiasX;
        p_out.gyro_bias_y=_gyroBiasY;
        p_out.gyro_bias_z=_gyroBiasZ;
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
        const uint32_t v_now = (uint32_t)(millis()-_uptime0);
        uint16_t v_sc=0;
        for(uint8_t i=0;i<_spikeCount;i++){
            int v_idx = (int)_spikeHead - 1 - (int)i;
            if(v_idx<0) v_idx += 32;
            if(v_now - _spikes[v_idx].ts_ms <= 10000) v_sc++;
            else break;
        }
        p_out.spike_count_10s = v_sc;
        p_out.consecutive_fail = _consecutiveFail;
        p_out.consecutive_recover_fail = _consecutiveRecoverFail;

        p_out.ppt_runtime_override = _pptRtValid;

        // health score (현장용 간단 가중치)
        uint16_t v_score=1000;
        v_score = (uint16_t)max(0, (int)v_score - (int)(p_out.gyro_rms*25.0f));
        v_score = (uint16_t)max(0, (int)v_score - (int)(p_out.cursor_rms*18.0f));
        v_score = (uint16_t)max(0, (int)v_score - (int)min((uint32_t)400, p_out.err_mpu_nan*20));
        v_score = (uint16_t)max(0, (int)v_score - (int)min((uint32_t)300, p_out.i2c_recover_count*35));
        v_score = (uint16_t)max(0, (int)v_score - (int)min((uint32_t)300, (uint32_t)p_out.spike_count_10s*12));
        v_score = (uint16_t)max(0, (int)v_score - (int)min((uint32_t)400, (uint32_t)p_out.consecutive_fail*18));

        p_out.health_score=v_score;
        p_out.health = (v_score>=820)?(uint8_t)EN_E10_HEALTH_OK:((v_score>=620)?(uint8_t)EN_E10_HEALTH_WARN:(uint8_t)EN_E10_HEALTH_DEGRADED);

        // history newest-first
        p_out.err_hist_n = (uint8_t)min((uint8_t)s_errHistCap,_errHistCount);
        for(uint8_t i=0;i<p_out.err_hist_n;i++){
            int v_idx = (int)_errHistHead - 1 - (int)i;
            if(v_idx<0) v_idx += s_errHistCap;
            p_out.err_hist[i]=_errHist[v_idx];
        }

        unlock_();
    }

  private:
    bool applyFromConfig(){
        if(!_cfg) return false;

        ST_C10_WiFiConfig_t v_w;
        ST_C10_E10Config_t  v_e;
        _cfg->makeDefaultsWiFi(v_w);
        _cfg->makeDefaultsE10(v_e);
        (void)_cfg->loadAll(v_w,v_e);

        lock_();

        _dpiLevel=(int)v_e.dpi_level;
        _hardClickLock=v_e.hard_click_lock;

        for(int i=0;i<3;i++){
            _scaleBase[i]=v_e.scale_base[i];
            _accelGain[i]=v_e.accel_gain[i];
        }
        _accelTh=v_e.accel_threshold;

        _wheelThDeg=v_e.wheel_threshold_deg;
        _wheelStepMax=(int)v_e.wheel_step_max;

        _gestureFlickDeg=v_e.gesture_flick_deg;
        _gestureCooldownMs=v_e.gesture_cooldown_ms;

        _scrollCursorDamp=v_e.scroll_cursor_damp;

        _precisionEnable=v_e.precision_enable;
        _precDeadzone=v_e.precision_deadzone;
        _precGain=v_e.precision_gain;
        _precAccel=v_e.precision_accel;
        _precMaxStep=v_e.precision_max_step;
        _precSmooth=v_e.precision_smooth;
        if(!_precisionEnable) _precisionMode=false;

        _ppt2_start=v_e.ppt2_start; _ppt2_exit=v_e.ppt2_exit; _ppt2_next=v_e.ppt2_next;
        _ppt2_prev=v_e.ppt2_prev; _ppt2_black=v_e.ppt2_black; _ppt2_laser=v_e.ppt2_laser;

        _engine.setHardClickLock(_hardClickLock);
        _engine.setDPI(_dpiLevel);

        unlock_();
        return true;
    }

    // ---- Modifier policy (mask == modifier byte 1:1) ----
    // p_modMask uses KEY_MOD_* mask (0x01..0x80). Exactly matches HID report modifier byte.
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
        // KeyboardDevice mediaKeyPress assumes 32-bit mask 방식
        _keyboard.mediaKeyPress(p_mask);
        vTaskDelay(pdMS_TO_TICKS(p_ms));
        _keyboard.mediaKeyRelease(p_mask);
    }

    void sendPptKey2_(uint8_t p_page, uint8_t p_mod, uint32_t p_code){
        if(p_page == (uint8_t)EN_C10_KEYPAGE_CONSUMER){
            tapConsumerMask_(p_code);
            return;
        }
        // kb page: 0x00~0xE7 (0xE0~0xE7는 'modifier usage' 영역이므로 UI에서 선택 비권장)
        uint8_t v_usage = (uint8_t)min((uint32_t)0xE7, p_code);
        if(p_mod) tapComboUsageKb_(p_mod, v_usage);
        else tapUsageKb_(v_usage);
    }

    // ✅ 실제 사용할 PPT 키(오버레이 우선)
    const ST_C10_PptKey2_t& pickPpt_(const ST_C10_PptKey2_t& p_cfg, const ST_C10_PptKey2_t& p_rt) const {
        return _pptRtValid ? p_rt : p_cfg;
    }
    void sendPptKey2Sel_(const ST_C10_PptKey2_t& p_cfg, const ST_C10_PptKey2_t& p_rt){
        const ST_C10_PptKey2_t& k = pickPpt_(p_cfg, p_rt);
        sendPptKey2_(k.page, k.mod, k.code);
    }

    void processGesturesDeg_(float p_gzDeg){
        static unsigned long s_last=0;
        if(millis()-s_last<_gestureCooldownMs) return;
        if(p_gzDeg>_gestureFlickDeg){ sendPptKey2Sel_(_ppt2_prev,_pptRt_prev); s_last=millis(); }
        else if(p_gzDeg<-_gestureFlickDeg){ sendPptKey2Sel_(_ppt2_next,_pptRt_next); s_last=millis(); }
    }

    void applyPrecision_(float& p_fx, float& p_fy){
        if(!_precisionMode) return;

        if(fabsf(p_fx)<_precDeadzone) p_fx=0;
        if(fabsf(p_fy)<_precDeadzone) p_fy=0;

        auto v_shape=[&](float v)->float{
            float a=fabsf(v);
            if(a<0.0001f) return 0;
            float n=min(1.0f, a/(float)_precMaxStep);
            float b=n+(_precAccel*n*n);
            float out=b*(float)_precMaxStep;
            out*=_precGain;
            return (v>=0)?out:-out;
        };

        float tx=constrain(v_shape(p_fx), -(float)_precMaxStep, (float)_precMaxStep);
        float ty=constrain(v_shape(p_fy), -(float)_precMaxStep, (float)_precMaxStep);

        _precSmX = _precSmX*_precSmooth + tx*(1.0f-_precSmooth);
        _precSmY = _precSmY*_precSmooth + ty*(1.0f-_precSmooth);
        p_fx=_precSmX; p_fy=_precSmY;
    }

    void welfordAdd_(uint32_t& n, double& mean, double& m2, double x){
        n++;
        double d=x-mean;
        mean += d/(double)n;
        double d2=x-mean;
        m2 += d*d2;
        if(n>2500){ n=1; mean=x; m2=0; } // 메모리/오버플로우 방지용 리셋
    }
    float calcRms_(uint32_t n, double m2){
        if(n<2) return 0;
        double var=m2/(double)(n-1);
        if(var<0) var=0;
        return (float)sqrt(var);
    }

    void pushErr_(uint8_t p_code, uint16_t p_value=0){
        ST_E10_ErrEvt_t e;
        e.ts_ms=(uint32_t)(millis()-_uptime0);
        e.code=p_code;
        e.value=p_value;
        _errHist[_errHistHead]=e;
        _errHistHead=(uint8_t)((_errHistHead+1)%s_errHistCap);
        if(_errHistCount<s_errHistCap) _errHistCount++;
    }

    void pushSpike_(uint32_t p_ts_ms){
        _spikes[_spikeHead].ts_ms=p_ts_ms;
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

        if(ok){ _consecutiveRecoverFail=0; pushErr_(EN_E10_ERR_I2C_RECOVER_OK, 0); }
        else { _consecutiveRecoverFail++; _consecutiveFail++; pushErr_(EN_E10_ERR_I2C_RECOVER_FAIL, 0); }

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
            if(m < s_calibStillThDeg){
                sx+=gx; sy+=gy; sz+=gz; cnt++;
            }
            vTaskDelay(pdMS_TO_TICKS(5));
        }

        if(cnt>0){
            _gyroBiasX=(float)(sx/(double)cnt);
            _gyroBiasY=(float)(sy/(double)cnt);
            _gyroBiasZ=(float)(sz/(double)cnt);
        }
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

            // mode 버튼: short=dpi cycle, long=ppt toggle
            if(digitalRead(s_btnMode)==LOW){
                if(btnDownMs==0) btnDownMs=millis();
            }else{
                if(btnDownMs>0){
                    unsigned long hold=millis()-btnDownMs;
                    if(hold>1000) m->_isPptMode = !m->_isPptMode;
                    else {
                        m->_dpiLevel++;
                        if(m->_dpiLevel>3)m->_dpiLevel=1;
                        m->_engine.setDPI(m->_dpiLevel);
                    }
                    btnDownMs=0;
                }
            }

            float gx=(g.gyro.x*RAD_TO_DEG)-m->_gyroBiasX;
            float gy=(g.gyro.y*RAD_TO_DEG)-m->_gyroBiasY;
            float gz=(g.gyro.z*RAD_TO_DEG)-m->_gyroBiasZ;

            // anomaly spike
            const uint32_t ts = (uint32_t)(millis()-m->_uptime0);
            if(fabsf(gz) > s_spikeThDeg) m->pushSpike_(ts);

            if(isnan(gx)||isnan(gy)||isnan(gz)){
                m->_errMpuNan++;
                m->_consecutiveFail++;
                m->pushErr_(EN_E10_ERR_MPU_NAN, 0);

                // 일정 누적마다 recover
                if((m->_errMpuNan % 5)==0) (void)m->recoverI2C_();

                vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(8));
                continue;
            }else{
                if(m->_consecutiveFail>0) m->_consecutiveFail--;
            }

            m->welfordAdd_(m->_gyroN, m->_gyroMean, m->_gyroM2, (double)gz);

            m->_engine.updateOrientation(a.acceleration.y, a.acceleration.z, gx, dt);

            int tx=0, ty=0;
            m->_engine.process(-gz, -gx, tx, ty);

            const bool leftClick=(digitalRead(s_btnL)==LOW);
            if(leftClick) m->_engine.notifyClick();

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

            if(!scrollMode) m->applyPrecision_(fx,fy);

            m->welfordAdd_(m->_curN, m->_curMean, m->_curM2, (double)sqrtf(fx*fx+fy*fy));

            if(m->_isPptMode && !scrollMode) m->processGesturesDeg_(gz);

            int wheel=0;
            if(scrollMode){
                if(gy > m->_wheelThDeg){
                    float n=(gy-m->_wheelThDeg)/120.0f; if(n>1)n=1;
                    wheel = (int)(1 + (n*(m->_wheelStepMax-1)));
                }else if(gy < -m->_wheelThDeg){
                    float n=(-gy-m->_wheelThDeg)/120.0f; if(n>1)n=1;
                    wheel = -(int)(1 + (n*(m->_wheelStepMax-1)));
                }
                fx*=m->_scrollCursorDamp;
                fy*=m->_scrollCursorDamp;
            }

            if(xSemaphoreTake(m->_mutex,0)==pdTRUE){
                m->_state.x=(int)fx;
                m->_state.y=(int)fy;
                m->_state.wheel=wheel;
                m->_state.updated=true;
                xSemaphoreGive(m->_mutex);
            }else{
                m->_errMutexMiss++;
                m->pushErr_(EN_E10_ERR_MUTEX_MISS, 0);
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
                if((m->_errTaskOverrun%10)==0) m->pushErr_(EN_E10_ERR_TASK_OVERRUN, 0);
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
