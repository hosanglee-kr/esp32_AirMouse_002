// =======================================================
// File: src/v022/E10_EliteAirMouse_022.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : E10_EliteAirMouse_022.h
 * 모듈약어 : E10
 * 모듈명 : AirMouse+Presenter (P0 Safe, Status P1, Precision FSM P2, Modifier Policy Fixed)
 * ------------------------------------------------------
 * 기능 요약
 *  - MPU6050 기반 에어마우스 + Composite HID(Mouse+Keyboard)
 *  - Gyro 오프셋 자동 캘리브레이션(부팅 1초 평균, 큰 움직임 샘플 제외)
 *  - BTN_SCROLL 전용 스크롤 모드(커서 억제 + PPT 제스처 차단)
 *  - Hard Click-Lock 옵션(완전 고정)
 *  - P1: 현장 판단용 상태(health/gyroRMS/cursorRMS/I2C recover/에러 히스토리)
 *  - P2: 조이스틱 느낌 Precision FSM(Deadzone/MaxStep/Accel/Smooth)
 *  - ✅ Modifier 정책 확정: W10 mods mask == KeyboardInputReport.modifiers(1:1)
 *    -> KeyboardDevice.modifierKeyPress/Release(mask) 사용
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
 *
 * [튜닝 TIP]
 *  - 캘리브 제외 임계(s_calibStillThDeg): 손에 들고 부팅하면 샘플 제외되어 bias가 0에 가까워질 수 있음
 *    -> "평평한 곳에 둔 상태로 부팅"을 권장하거나 임계를 3~6 deg/s로 조정
 *  - Precision 모드: deadzone↑=안정, gain↑=빠름, max_step↑=최대속도, smooth↑=부드러움(지연↑)
 *  - health 판단: gyroRMS↑, cursorRMS↑, err 증가, I2C recover 빈번이면 DEGRADED
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#include <BleCompositeHID.h>
#include <KeyboardDevice.h>
#include <MouseDevice.h>

#include "C10_Config_022.h"
#include "M10_MotionProc_015.h" // 기존 엔진 유지(하드클릭락 포함)

typedef bool (*T_E10_ApplyFn)(void* p_ctx);

enum EN_E10_Health_t : uint8_t {
    EN_E10_HEALTH_OK = 0,
    EN_E10_HEALTH_WARN = 1,
    EN_E10_HEALTH_DEGRADED = 2
};

struct ST_E10_ErrEvt_t {
    uint32_t ts_ms;
    uint8_t  code;    // 1=mpu_nan,2=mutex_miss,3=overrun,4=i2c_recover_ok,5=i2c_recover_fail
    uint16_t value;   // optional
};

struct ST_E10_Status_t {
    // runtime
    bool     ble_connected;
    bool     ppt_mode;
    uint8_t  dpi_level;
    bool     precision_mode;

    // health
    uint8_t  health;        // EN_E10_Health_t
    uint16_t health_score;  // 0~1000

    // sensor
    float gyro_bias_x;
    float gyro_bias_y;
    float gyro_bias_z;
    float temp_c;

    // noise (RMS)
    float gyro_rms;     // deg/s RMS
    float cursor_rms;   // px RMS

    // timing
    uint32_t sampling_ms_target;
    float    sampling_ms_avg;

    // i2c recover
    uint32_t i2c_recover_count;
    bool     i2c_recover_last_ok;

    // counters
    uint32_t err_mpu_nan;
    uint32_t err_mutex_miss;
    uint32_t err_task_overrun;

    // error history (latest N)
    uint8_t  err_hist_n;
    ST_E10_ErrEvt_t err_hist[16];

    // misc
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

    // GPIO (예시)
    static constexpr int s_btnL      = 12;
    static constexpr int s_btnMode   = 13;
    static constexpr int s_btnScroll = 14;

    static constexpr uint8_t s_mouseBtnLeft = 0x01;

    // shared state
    volatile bool _isPptMode = false;

    struct ST_E10_State_t {
        int  x;
        int  y;
        int  wheel;
        bool updated;
    } _state;

    SemaphoreHandle_t _mutex = nullptr;

    // gyro bias
    float _gyroBiasX = 0.0f;
    float _gyroBiasY = 0.0f;
    float _gyroBiasZ = 0.0f;
    bool  _gyroCalibDone = false;

    static constexpr uint32_t s_calibMs = 1000;
    static constexpr float    s_calibStillThDeg = 3.0f;

    // config runtime
    int   _dpiLevel = 2;
    bool  _hardClickLock = true;

    float _scaleBase[3] = {0.55f, 0.75f, 1.00f};
    float _accelGain[3] = {0.35f, 0.55f, 0.85f};
    float _accelTh = 8.0f;

    float _wheelThDeg = 90.0f;
    int   _wheelStepMax = 6;

    float    _gestureFlickDeg = 200.0f;
    uint16_t _gestureCooldownMs = 600;

    float _scrollCursorDamp = 0.25f;

    // Precision FSM (P2)
    bool  _precisionEnable = false;
    bool  _precisionMode = false; // runtime toggle
    float _precDeadzone = 1.2f;
    float _precGain = 0.65f;
    float _precAccel = 0.25f;
    uint8_t _precMaxStep = 18;
    float _precSmooth = 0.85f;
    float _precSmX = 0.0f;
    float _precSmY = 0.0f;

    // PPT keys
    ST_C10_PptKey_t _pptStart;
    ST_C10_PptKey_t _pptExit;
    ST_C10_PptKey_t _pptNext;
    ST_C10_PptKey_t _pptPrev;
    ST_C10_PptKey_t _pptBlack;
    ST_C10_PptKey_t _pptLaser;

    // status vars
    float _tempC = 0.0f;
    uint32_t _uptime0 = 0;

    // timing avg
    float _dtAvgMs = 8.0f;

    // counters (P1)
    uint32_t _errMpuNan = 0;
    uint32_t _errMutexMiss = 0;
    uint32_t _errTaskOverrun = 0;

    // noise (Welford)
    uint32_t _gyroN = 0;
    double   _gyroMean = 0.0;
    double   _gyroM2 = 0.0;

    uint32_t _curN = 0;
    double   _curMean = 0.0;
    double   _curM2 = 0.0;

    // i2c recover
    uint32_t _i2cRecoverCount = 0;
    bool     _i2cRecoverLastOk = true;

    // error history ring
    static constexpr uint8_t s_errHistCap = 16;
    ST_E10_ErrEvt_t _errHist[s_errHistCap];
    uint8_t _errHistHead = 0;
    uint8_t _errHistCount = 0;

  public:
    CL_E10_EliteAirMouse() : _hid("Elite AirMouse S3", "ProMaker", 100) {
        _state = {0,0,0,false};
        memset(&_pptStart, 0, sizeof(_pptStart));
        memset(&_pptExit,  0, sizeof(_pptExit));
        memset(&_pptNext,  0, sizeof(_pptNext));
        memset(&_pptPrev,  0, sizeof(_pptPrev));
        memset(&_pptBlack, 0, sizeof(_pptBlack));
        memset(&_pptLaser, 0, sizeof(_pptLaser));
        memset(_errHist, 0, sizeof(_errHist));
    }

    void begin(CL_C10_Config* p_cfg) {
        _cfg = p_cfg;
        _uptime0 = millis();
        Serial.begin(115200);

        Wire.begin(4, 5);
        Wire.setClock(400000);

        if (!_mpu.begin()) {
            Serial.println("[E10] Failed to find MPU6050 chip");
            for (;;) delay(10);
        }

        _mpu.setGyroRange(MPU6050_RANGE_250_DEG);
        _mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
        _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

        pinMode(s_btnL, INPUT_PULLUP);
        pinMode(s_btnMode, INPUT_PULLUP);
        pinMode(s_btnScroll, INPUT_PULLUP);

        _mutex = xSemaphoreCreateMutex();

        (void)applyFromConfig();

        _hid.addDevice(&_keyboard);
        _hid.addDevice(&_mouse);
        _hid.begin();

        xTaskCreatePinnedToCore(sensorTask, "E10_Sensor", 8192, this, 3, nullptr, 1);
        xTaskCreatePinnedToCore(commTask,   "E10_Comm",   4096, this, 2, nullptr, 0);
    }

    static bool E10_W10Apply(void* p_ctx) {
        if (p_ctx == nullptr) return false;
        return ((CL_E10_EliteAirMouse*)p_ctx)->applyFromConfig();
    }

    // -------------------------
    // Remote control (P1)
    // -------------------------
    bool setPptMode(bool p_enable) {
        lock_();
        _isPptMode = p_enable;
        unlock_();
        return true;
    }

    bool setDpiLevel(uint8_t p_level) {
        if (p_level < 1) p_level = 1;
        if (p_level > 3) p_level = 3;

        lock_();
        _dpiLevel = (int)p_level;
        _engine.setDPI(_dpiLevel);
        unlock_();
        return true;
    }

    bool setPrecisionMode(bool p_enable) {
        lock_();
        _precisionMode = p_enable && _precisionEnable;
        // reset smoothing when toggled
        _precSmX = 0.0f; _precSmY = 0.0f;
        unlock_();
        return true;
    }

    // -------------------------
    // Status (P1)
    // -------------------------
    void getStatus(ST_E10_Status_t& p_out) {
        memset(&p_out, 0, sizeof(p_out));

        lock_();

        p_out.ble_connected = _hid.isConnected();
        p_out.ppt_mode = _isPptMode;
        p_out.dpi_level = (uint8_t)_dpiLevel;
        p_out.precision_mode = _precisionMode;

        p_out.gyro_bias_x = _gyroBiasX;
        p_out.gyro_bias_y = _gyroBiasY;
        p_out.gyro_bias_z = _gyroBiasZ;
        p_out.temp_c = _tempC;

        p_out.sampling_ms_target = 8;
        p_out.sampling_ms_avg = _dtAvgMs;

        p_out.i2c_recover_count = _i2cRecoverCount;
        p_out.i2c_recover_last_ok = _i2cRecoverLastOk;

        p_out.err_mpu_nan = _errMpuNan;
        p_out.err_mutex_miss = _errMutexMiss;
        p_out.err_task_overrun = _errTaskOverrun;

        p_out.uptime_ms = (uint32_t)(millis() - _uptime0);

        // rms
        p_out.gyro_rms = calcRms_(_gyroN, _gyroM2);
        p_out.cursor_rms = calcRms_(_curN, _curM2);

        // health
        uint16_t v_score = 1000;
        // heuristic: noise + errors + recover frequency
        v_score = (uint16_t)max(0, (int)v_score - (int)(p_out.gyro_rms * 25.0f));    // 1 deg/s RMS -> -25
        v_score = (uint16_t)max(0, (int)v_score - (int)(p_out.cursor_rms * 18.0f));  // 1px RMS -> -18
        v_score = (uint16_t)max(0, (int)v_score - (int)min((uint32_t)400, p_out.err_mpu_nan * 20));
        v_score = (uint16_t)max(0, (int)v_score - (int)min((uint32_t)300, p_out.err_task_overrun * 2));
        v_score = (uint16_t)max(0, (int)v_score - (int)min((uint32_t)300, p_out.i2c_recover_count * 35));

        p_out.health_score = v_score;
        if (v_score >= 820) p_out.health = (uint8_t)EN_E10_HEALTH_OK;
        else if (v_score >= 620) p_out.health = (uint8_t)EN_E10_HEALTH_WARN;
        else p_out.health = (uint8_t)EN_E10_HEALTH_DEGRADED;

        // err history snapshot
        p_out.err_hist_n = (uint8_t)min((uint8_t)s_errHistCap, _errHistCount);
        // copy newest-first
        for (uint8_t i=0; i<p_out.err_hist_n; i++) {
            int idx = (int)_errHistHead - 1 - (int)i;
            if (idx < 0) idx += s_errHistCap;
            p_out.err_hist[i] = _errHist[idx];
        }

        unlock_();
    }

  private:
    // -------------------------
    // Apply config
    // -------------------------
    bool applyFromConfig() {
        if (_cfg == nullptr) return false;

        ST_C10_WiFiConfig_t v_w;
        ST_C10_E10Config_t  v_e;
        memset(&v_w, 0, sizeof(v_w));
        memset(&v_e, 0, sizeof(v_e));

        (void)_cfg->loadAll(v_w, v_e);

        lock_();

        _dpiLevel = (int)v_e.dpi_level;
        _hardClickLock = v_e.hard_click_lock;

        for (int i=0;i<3;i++) { _scaleBase[i] = v_e.scale_base[i]; _accelGain[i] = v_e.accel_gain[i]; }
        _accelTh = v_e.accel_threshold;

        _wheelThDeg = v_e.wheel_threshold_deg;
        _wheelStepMax = (int)v_e.wheel_step_max;

        _gestureFlickDeg = v_e.gesture_flick_deg;
        _gestureCooldownMs = v_e.gesture_cooldown_ms;

        _scrollCursorDamp = v_e.scroll_cursor_damp;

        // Precision
        _precisionEnable = v_e.precision_enable;
        _precDeadzone = v_e.precision_deadzone;
        _precGain = v_e.precision_gain;
        _precAccel = v_e.precision_accel;
        _precMaxStep = v_e.precision_max_step;
        _precSmooth = v_e.precision_smooth;

        if (!_precisionEnable) _precisionMode = false;

        // PPT keys
        _pptStart = v_e.ppt_start;
        _pptExit  = v_e.ppt_exit;
        _pptNext  = v_e.ppt_next;
        _pptPrev  = v_e.ppt_prev;
        _pptBlack = v_e.ppt_black;
        _pptLaser = v_e.ppt_laser;

        _engine.setHardClickLock(_hardClickLock);
        _engine.setDPI(_dpiLevel);

        unlock_();

        Serial.printf("[E10] apply ok: dpi=%d hard=%d prec=%d(dz=%.2f gain=%.2f acc=%.2f max=%u sm=%.2f)\n",
                      _dpiLevel, _hardClickLock ? 1 : 0,
                      _precisionEnable ? 1 : 0, _precDeadzone, _precGain, _precAccel, (unsigned)_precMaxStep, _precSmooth);
        return true;
    }

    // -------------------------
    // Modifier policy (FIXED)
    // -------------------------
    // W10 /api/keycodes mods: mask == KeyboardInputReport.modifiers (1:1)
    // - LCtrl 0x01, LShift 0x02, LAlt 0x04, LMeta 0x08, RCtrl 0x10, RShift 0x20, RAlt 0x40, RMeta 0x80
    void tapKeyUsage_(uint16_t p_usage, uint16_t p_ms = 12) {
        if (p_usage == 0) return;
        _keyboard.keyPress((uint8_t)p_usage);
        vTaskDelay(pdMS_TO_TICKS(p_ms));
        _keyboard.keyRelease((uint8_t)p_usage);
    }

    void tapComboUsage_(uint8_t p_modMask, uint16_t p_usage, uint16_t p_ms = 18) {
        if (p_usage == 0) return;

        // ✅ modifier byte 방식 확정
        if (p_modMask != 0) _keyboard.modifierKeyPress(p_modMask);
        _keyboard.keyPress((uint8_t)p_usage);

        vTaskDelay(pdMS_TO_TICKS(p_ms));

        _keyboard.keyRelease((uint8_t)p_usage);
        if (p_modMask != 0) _keyboard.modifierKeyRelease(p_modMask);
    }

    void sendPptKey_(const ST_C10_PptKey_t& p_k) {
        if (!_hid.isConnected()) return;
        if (p_k.key == 0) return;
        if (p_k.mod == 0) tapKeyUsage_(p_k.key);
        else tapComboUsage_(p_k.mod, p_k.key, 22);
    }

    // -------------------------
    // Gesture
    // -------------------------
    void processGesturesDeg_(float p_gzDegPerSec) {
        static unsigned long s_lastFlick = 0;
        if (millis() - s_lastFlick < _gestureCooldownMs) return;

        if (p_gzDegPerSec > _gestureFlickDeg) { sendPptKey_(_pptPrev); s_lastFlick = millis(); }
        else if (p_gzDegPerSec < -_gestureFlickDeg) { sendPptKey_(_pptNext); s_lastFlick = millis(); }
    }

    // -------------------------
    // Mouse send
    // -------------------------
    static void mouseSend_(MouseDevice& p_ms, int8_t p_dx, int8_t p_dy, int8_t p_wheel) {
        // MouseDevice.h: mouseMove(x,y,scrollX,scrollY)
        // 여기서는 scrollX에 wheel을 사용(기존 정책 유지)
        p_ms.mouseMove(p_dx, p_dy, p_wheel, 0);
    }

    // -------------------------
    // Gyro calibration
    // -------------------------
    void runGyroCalibration_() {
        const uint32_t v_t0 = millis();
        uint32_t v_cnt = 0;
        double v_sumX = 0.0, v_sumY = 0.0, v_sumZ = 0.0;

        while (millis() - v_t0 < s_calibMs) {
            sensors_event_t v_a, v_g, v_temp;
            _mpu.getEvent(&v_a, &v_g, &v_temp);

            const float v_gx = v_g.gyro.x * RAD_TO_DEG;
            const float v_gy = v_g.gyro.y * RAD_TO_DEG;
            const float v_gz = v_g.gyro.z * RAD_TO_DEG;

            const float v_absMax = max(max(fabsf(v_gx), fabsf(v_gy)), fabsf(v_gz));
            if (v_absMax < s_calibStillThDeg) {
                v_sumX += v_gx; v_sumY += v_gy; v_sumZ += v_gz; v_cnt++;
            }
            vTaskDelay(pdMS_TO_TICKS(5));
        }

        if (v_cnt > 0) {
            _gyroBiasX = (float)(v_sumX / (double)v_cnt);
            _gyroBiasY = (float)(v_sumY / (double)v_cnt);
            _gyroBiasZ = (float)(v_sumZ / (double)v_cnt);
        }

        _gyroCalibDone = true;
        Serial.printf("[E10] gyro calib: bx=%.3f by=%.3f bz=%.3f samples=%u\n",
                      _gyroBiasX, _gyroBiasY, _gyroBiasZ, (unsigned)v_cnt);
    }

    // -------------------------
    // Precision FSM (P2)
    // -------------------------
    void applyPrecision_(float& p_fx, float& p_fy) {
        if (!_precisionMode) return;

        // joystick-like shaping:
        // 1) deadzone
        float v_ax = fabsf(p_fx);
        float v_ay = fabsf(p_fy);
        if (v_ax < _precDeadzone) p_fx = 0.0f;
        if (v_ay < _precDeadzone) p_fy = 0.0f;

        // 2) accel curve (soft)
        auto shape = [&](float v_in)->float{
            float v = v_in;
            float a = fabsf(v);
            if (a <= 0.0001f) return 0.0f;

            // normalize & accel
            float n = min(1.0f, a / (float)_precMaxStep);
            float boosted = n + (_precAccel * n * n); // gentle accel
            float out = boosted * (float)_precMaxStep;

            // gain
            out *= _precGain;

            // restore sign
            return (v >= 0.0f) ? out : -out;
        };

        float v_tx = shape(p_fx);
        float v_ty = shape(p_fy);

        // 3) clamp max_step
        v_tx = constrain(v_tx, -(float)_precMaxStep, (float)_precMaxStep);
        v_ty = constrain(v_ty, -(float)_precMaxStep, (float)_precMaxStep);

        // 4) smoothing (EMA). smooth↑ -> 더 부드러움(지연↑)
        _precSmX = _precSmX * _precSmooth + v_tx * (1.0f - _precSmooth);
        _precSmY = _precSmY * _precSmooth + v_ty * (1.0f - _precSmooth);

        p_fx = _precSmX;
        p_fy = _precSmY;
    }

    // -------------------------
    // Noise accumulators
    // -------------------------
    void welfordAdd_(uint32_t& p_n, double& p_mean, double& p_m2, double p_x) {
        p_n++;
        double d = p_x - p_mean;
        p_mean += d / (double)p_n;
        double d2 = p_x - p_mean;
        p_m2 += d * d2;
        // 너무 오래 누적되면 옛데이터 영향이 커짐 -> 주기적으로 리셋 (현장판단용)
        if (p_n > 2500) { // 약 20초@125Hz
            p_n = 1;
            p_mean = p_x;
            p_m2 = 0.0;
        }
    }

    float calcRms_(uint32_t p_n, double p_m2) {
        if (p_n < 2) return 0.0f;
        double var = p_m2 / (double)(p_n - 1);
        if (var < 0.0) var = 0.0;
        return (float)sqrt(var);
    }

    // -------------------------
    // Error ring
    // -------------------------
    void pushErr_(uint8_t p_code, uint16_t p_value = 0) {
        ST_E10_ErrEvt_t v_e;
        v_e.ts_ms = (uint32_t)(millis() - _uptime0);
        v_e.code = p_code;
        v_e.value = p_value;

        _errHist[_errHistHead] = v_e;
        _errHistHead = (uint8_t)((_errHistHead + 1) % s_errHistCap);
        if (_errHistCount < s_errHistCap) _errHistCount++;
    }

    // -------------------------
    // I2C recover (P0/P1)
    // -------------------------
    bool recoverI2C_() {
        // 간단 I2C bus recover: SCL 9 pulses
        // NOTE: 실제 핀은 Wire.begin(SDA=4,SCL=5) 기준
        const int v_sda = 4;
        const int v_scl = 5;

        pinMode(v_sda, INPUT_PULLUP);
        pinMode(v_scl, OUTPUT_OPEN_DRAIN);

        for (int i=0; i<9; i++) {
            digitalWrite(v_scl, HIGH);
            delayMicroseconds(6);
            digitalWrite(v_scl, LOW);
            delayMicroseconds(6);
        }
        digitalWrite(v_scl, HIGH);
        delayMicroseconds(6);

        // re-init Wire + re-init mpu
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

        pushErr_(v_ok ? 4 : 5, 0);
        return v_ok;
    }

    // -------------------------
    // Mutex helpers
    // -------------------------
    void lock_() {
        if (_mutex != nullptr) (void)xSemaphoreTake(_mutex, portMAX_DELAY);
    }
    void unlock_() {
        if (_mutex != nullptr) xSemaphoreGive(_mutex);
    }

    // -------------------------
    // Tasks
    // -------------------------
    static void sensorTask(void* p_pv) {
        CL_E10_EliteAirMouse* v_m = (CL_E10_EliteAirMouse*)p_pv;

        TickType_t v_lastWake = xTaskGetTickCount();
        unsigned long v_lastUs = micros();
        unsigned long v_btnDownMs = 0;

        if (!v_m->_gyroCalibDone) v_m->runGyroCalibration_();

        for (;;) {
            sensors_event_t v_a, v_g, v_temp;
            v_m->_mpu.getEvent(&v_a, &v_g, &v_temp);
            v_m->_tempC = v_temp.temperature;

            const unsigned long v_nowUs = micros();
            const float v_dt = (v_nowUs - v_lastUs) / 1000000.0f;
            v_lastUs = v_nowUs;

            // dt 평균
            const float v_dtMs = v_dt * 1000.0f;
            v_m->_dtAvgMs = v_m->_dtAvgMs * 0.98f + v_dtMs * 0.02f;

            const bool v_scrollMode = (digitalRead(s_btnScroll) == LOW);

            // mode 버튼: short=dpi cycle, long=ppt toggle
            if (digitalRead(s_btnMode) == LOW) {
                if (v_btnDownMs == 0) v_btnDownMs = millis();
            } else {
                if (v_btnDownMs > 0) {
                    const unsigned long v_hold = millis() - v_btnDownMs;
                    if (v_hold > 1000) v_m->_isPptMode = !v_m->_isPptMode;
                    else {
                        v_m->_dpiLevel++;
                        if (v_m->_dpiLevel > 3) v_m->_dpiLevel = 1;
                        v_m->_engine.setDPI(v_m->_dpiLevel);
                    }
                    v_btnDownMs = 0;
                }
            }

            // gyro deg/s + bias
            float v_gx = (v_g.gyro.x * RAD_TO_DEG) - v_m->_gyroBiasX;
            float v_gy = (v_g.gyro.y * RAD_TO_DEG) - v_m->_gyroBiasY;
            float v_gz = (v_g.gyro.z * RAD_TO_DEG) - v_m->_gyroBiasZ;

            // validate (P0)
            if (isnan(v_gx) || isnan(v_gy) || isnan(v_gz)) {
                v_m->_errMpuNan++;
                v_m->pushErr_(1, 0);
                // try recover periodically
                if ((v_m->_errMpuNan % 5) == 0) (void)v_m->recoverI2C_();
                vTaskDelayUntil(&v_lastWake, pdMS_TO_TICKS(8));
                continue;
            }

            // noise (gyro RMS uses gz)
            v_m->welfordAdd_(v_m->_gyroN, v_m->_gyroMean, v_m->_gyroM2, (double)v_gz);

            v_m->_engine.updateOrientation(v_a.acceleration.y, v_a.acceleration.z, v_gx, v_dt);

            int v_tx = 0, v_ty = 0;
            v_m->_engine.process(-v_gz, -v_gx, v_tx, v_ty);

            // click
            const bool v_leftClick = (digitalRead(s_btnL) == LOW);
            if (v_leftClick) v_m->_engine.notifyClick();

            // base scale + accel
            float v_base = v_m->_scaleBase[v_m->_dpiLevel - 1];
            float v_accg = v_m->_accelGain[v_m->_dpiLevel - 1];

            const float v_mag = sqrtf((float)v_tx * (float)v_tx + (float)v_ty * (float)v_ty);
            float v_acc = 1.0f;
            if (v_mag > v_m->_accelTh) {
                const float v_ex = (v_mag - v_m->_accelTh);
                v_acc = 1.0f + (v_accg * (v_ex / (v_ex + 18.0f)));
            }

            float v_fx = (float)v_tx * v_base * v_acc;
            float v_fy = (float)v_ty * v_base * v_acc;

            // Precision FSM (P2) – scroll 중에는 precision 적용하지 않음
            if (!v_scrollMode) v_m->applyPrecision_(v_fx, v_fy);

            // cursor noise (RMS) uses vector magnitude
            v_m->welfordAdd_(v_m->_curN, v_m->_curMean, v_m->_curM2, (double)sqrtf(v_fx*v_fx + v_fy*v_fy));

            // gesture (scroll 중엔 차단)
            if (v_m->_isPptMode && !v_scrollMode) v_m->processGesturesDeg_(v_gz);

            // wheel
            int v_wheel = 0;
            if (v_scrollMode) {
                if (v_gy > v_m->_wheelThDeg) {
                    float v_norm = (v_gy - v_m->_wheelThDeg) / 120.0f;
                    if (v_norm > 1.0f) v_norm = 1.0f;
                    v_wheel = (int)(1 + (v_norm * (v_m->_wheelStepMax - 1)));
                } else if (v_gy < -v_m->_wheelThDeg) {
                    float v_norm = (-v_gy - v_m->_wheelThDeg) / 120.0f;
                    if (v_norm > 1.0f) v_norm = 1.0f;
                    v_wheel = -(int)(1 + (v_norm * (v_m->_wheelStepMax - 1)));
                }
                v_fx *= v_m->_scrollCursorDamp;
                v_fy *= v_m->_scrollCursorDamp;
            }

            // share state
            if (xSemaphoreTake(v_m->_mutex, 0) == pdTRUE) {
                v_m->_state.x = (int)v_fx;
                v_m->_state.y = (int)v_fy;
                v_m->_state.wheel = v_wheel;
                v_m->_state.updated = true;
                xSemaphoreGive(v_m->_mutex);
            } else {
                v_m->_errMutexMiss++;
                v_m->pushErr_(2, 0);
            }

            // mouse button immediate
            if (v_m->_hid.isConnected()) {
                if (v_leftClick) v_m->_mouse.mousePress(s_mouseBtnLeft);
                else v_m->_mouse.mouseRelease(s_mouseBtnLeft);
            }

            // overrun hint
            TickType_t v_before = xTaskGetTickCount();
            vTaskDelayUntil(&v_lastWake, pdMS_TO_TICKS(8));
            TickType_t v_after = xTaskGetTickCount();
            if ((v_after - v_before) == 0) {
                v_m->_errTaskOverrun++;
                if ((v_m->_errTaskOverrun % 10) == 0) v_m->pushErr_(3, 0);
            }
        }
    }

    static void commTask(void* p_pv) {
        CL_E10_EliteAirMouse* v_m = (CL_E10_EliteAirMouse*)p_pv;

        for (;;) {
            if (v_m->_hid.isConnected() && xSemaphoreTake(v_m->_mutex, portMAX_DELAY) == pdTRUE) {
                if (v_m->_state.updated) {
                    const int8_t v_dx = (int8_t)constrain(v_m->_state.x, -127, 127);
                    const int8_t v_dy = (int8_t)constrain(v_m->_state.y, -127, 127);
                    const int8_t v_wh = (int8_t)constrain(v_m->_state.wheel, -127, 127);

                    mouseSend_(v_m->_mouse, v_dx, v_dy, v_wh);
                    v_m->_state.updated = false;
                }
                xSemaphoreGive(v_m->_mutex);
            }
            vTaskDelay(pdMS_TO_TICKS(7));
        }
    }
};

