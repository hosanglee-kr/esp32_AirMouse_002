#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : E10_EliteAirMouse_024.h
 * 모듈약어 : E10
 * 모듈명 : AirMouse+Presenter (Composite HID, PPT Keymap v2, Status Snap, MediaKeys)
 * ------------------------------------------------------
 * 기능 요약
 *  - MPU6050 기반 에어마우스 + Composite HID(Mouse+Keyboard)
 *  - Gyro 오프셋 자동 캘리브레이션(부팅 1초 평균, 움직임 큰 샘플 제외)
 *  - BTN_SCROLL 전용 버튼 분리(스크롤 모드: 커서 억제 + 제스처 차단)
 *  - Hard Click-Lock 옵션
 *  - PPT Keymap v2: Keyboard(0x07) + Consumer(0x0C) 전송
 *  - /api/status 제공용 “스냅샷(Status Snapshot)” 구조체(노이즈 RMS/에러 히스토리/Recover/Anomaly)
 *  - 웹에서 PPT 모드 토글 / DPI 즉시 변경 / 정밀모드 토글 / PPT 임시 테스트(/api/ppt/test) 지원
 * ------------------------------------------------------
 * 변경점(024)
 *  - [P0] Media(Consumer) 키 전송을 위해 KeyboardConfiguration::setUseMediaKeys(true) 활성화
 *  - [P0] W10 mods(mask) ↔ E10 modifier byte 정책 1:1 확정: modifierKeyPress(mask) 사용
 *  - [P0] KB page에서 0xE0~0xE7(modifier usage)를 “일반 key”로 선택 시 차단(오동작 방지)
 *  - [P0] status는 getStatus()에서 실시간 멤버 읽기 대신, 센서루프가 200ms마다 갱신하는 스냅샷을 반환
 *  - [P1] health_score 산정에 consecutive_recover_fail 반영(현장 판단력 강화)
 * ------------------------------------------------------
 * 튜닝 팁(현장)
 *  - s_spikeThDeg: 스파이크 감지 임계(빠른 손목 스냅이 잦으면 ↑, 노이즈가 많으면 ↓)
 *  - _precDeadzone/_precSmooth: 정밀모드 체감 좌우(드리프트 있으면 deadzone↑, 떨림 있으면 smooth↑)
 *  - _wheelThDeg/_wheelStepMax: 스크롤 민감도(회의실/프레젠테이션 환경에서 과민하면 ThDeg↑)
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
#include <KeyboardConfiguration.h>
#include <MouseDevice.h>

#include "A40_ComFunc_070.h"
#include "C10_Config_023.h"
#include "M10_MotionProc_020.h"

typedef bool (*T_E10_ApplyFn)(void* p_ctx);

enum EN_E10_Health_t : uint8_t {
    EN_E10_HEALTH_OK       = 0,
    EN_E10_HEALTH_WARN     = 1,
    EN_E10_HEALTH_DEGRADED = 2
};

// err code 정의(대시보드/분석용)
enum EN_E10_ErrCode_t : uint8_t {
    EN_E10_ERR_NONE            = 0,
    EN_E10_ERR_MPU_NAN         = 1,
    EN_E10_ERR_MUTEX_MISS      = 2,
    EN_E10_ERR_TASK_OVERRUN    = 3,
    EN_E10_ERR_I2C_RECOVER_OK  = 4,
    EN_E10_ERR_I2C_RECOVER_FAIL= 5,
    EN_E10_ERR_INVALID_KB_USAGE= 6
};

struct ST_E10_ErrEvt_t {
    uint32_t ts_ms;
    uint8_t  code;
    uint16_t value;
};

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

    uint32_t uptime_ms;
};

class CL_E10_EliteAirMouse {
  private:
    Adafruit_MPU6050 _mpu;

    BleCompositeHID _hid;

    // [P0] Media keys enable을 위해 config 적용 생성(포인터)
    KeyboardDevice* _keyboard = nullptr;
    MouseDevice     _mouse;

    CL_M10_AdvancedMotionProcessor _engine;
    CL_C10_Config* _cfg = nullptr;

    // class static members -> s_ prefix
    static constexpr int s_btnL      = 12;
    static constexpr int s_btnMode   = 13;
    static constexpr int s_btnScroll = 14;

    static constexpr uint8_t s_mouseBtnLeft = 0x01;

    struct ST_State_t { int x; int y; int wheel; bool updated; } _state;
    SemaphoreHandle_t _mutex = nullptr;

    // gyro bias
    float _gyroBiasX = 0.0f;
    float _gyroBiasY = 0.0f;
    float _gyroBiasZ = 0.0f;
    bool  _gyroCalibDone = false;

    static constexpr uint32_t s_calibMs = 1000;
    static constexpr float    s_calibStillThDeg = 3.0f;

    // mode/status flags (setters use mutex, sensor reads are tolerant)
    volatile bool _isPptMode = false;

    // runtime config
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

    // precision mode (joystick-like)
    bool   _precisionEnable = false;
    volatile bool _precisionMode = false;

    float  _precDeadzone = 1.2f;
    float  _precGain     = 0.65f;
    float  _precAccel    = 0.25f;
    uint8_t _precMaxStep = 18;
    float  _precSmooth   = 0.85f;

    float _precSmX = 0.0f;
    float _precSmY = 0.0f;

    // PPT v2
    ST_C10_PptKey2_t _ppt2_start;
    ST_C10_PptKey2_t _ppt2_exit;
    ST_C10_PptKey2_t _ppt2_next;
    ST_C10_PptKey2_t _ppt2_prev;
    ST_C10_PptKey2_t _ppt2_black;
    ST_C10_PptKey2_t _ppt2_laser;

    // status vars (센서 루프가 갱신)
    float    _tempC = 0.0f;
    uint32_t _uptime0 = 0;

    float    _dtAvgMs = 8.0f;

    // errors
    uint32_t _errMpuNan = 0;
    uint32_t _errMutexMiss = 0;
    uint32_t _errTaskOverrun = 0;

    // RMS (Welford)
    uint32_t _gyroN = 0; double _gyroMean = 0.0; double _gyroM2 = 0.0;
    uint32_t _curN  = 0; double _curMean  = 0.0; double _curM2  = 0.0;

    // i2c recover
    uint32_t _i2cRecoverCount = 0;
    bool     _i2cRecoverLastOk = true;

    // history ring
    static constexpr uint8_t s_errHistCap = 16;
    ST_E10_ErrEvt_t _errHist[s_errHistCap];
    uint8_t _errHistHead = 0;
    uint8_t _errHistCount = 0;

    // anomaly spike
    static constexpr float s_spikeThDeg = 650.0f; // 튜닝 포인트
    struct ST_SpikeEvt_t { uint32_t ts_ms; };
    ST_SpikeEvt_t _spikes[32];
    uint8_t _spikeHead = 0;
    uint8_t _spikeCount = 0;

    uint16_t _consecutiveFail = 0;
    uint16_t _consecutiveRecoverFail = 0;

    // [P0] status snapshot (200ms 주기 갱신)
    ST_E10_Status_t _statusSnap;
    uint32_t _snapTsMs = 0;

  public:
    CL_E10_EliteAirMouse() : _hid("Elite AirMouse S3", "ProMaker", 100) {
        _state = {0,0,0,false};

        memset(&_ppt2_start, 0, sizeof(_ppt2_start));
        memset(&_ppt2_exit,  0, sizeof(_ppt2_exit));
        memset(&_ppt2_next,  0, sizeof(_ppt2_next));
        memset(&_ppt2_prev,  0, sizeof(_ppt2_prev));
        memset(&_ppt2_black, 0, sizeof(_ppt2_black));
        memset(&_ppt2_laser, 0, sizeof(_ppt2_laser));

        memset(_errHist, 0, sizeof(_errHist));
        memset(_spikes,  0, sizeof(_spikes));

        memset(&_statusSnap, 0, sizeof(_statusSnap));
    }

    void begin(CL_C10_Config* p_cfg) {
        _cfg = p_cfg;
        _uptime0 = millis();

        Serial.begin(115200);

        Wire.begin(4, 5);
        Wire.setClock(400000);

        if (!_mpu.begin()) {
            Serial.println("[E10] MPU begin fail");
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

        // [P0] Media keys enable
        KeyboardConfiguration v_kcfg;
        v_kcfg.setUseMediaKeys(true);
        _keyboard = new (std::nothrow) KeyboardDevice(v_kcfg);
        if (_keyboard == nullptr) {
            Serial.println("[E10] WARN: KeyboardDevice alloc failed (media keys disabled).");
        }

        if (_keyboard != nullptr) _hid.addDevice(_keyboard);
        _hid.addDevice(&_mouse);
        _hid.begin();

        xTaskCreatePinnedToCore(sensorTask, "E10_Sensor", 8192, this, 3, nullptr, 1);
        xTaskCreatePinnedToCore(commTask,   "E10_Comm",   4096, this, 2, nullptr, 0);
    }

    // W10 Apply 콜백
    static bool E10_W10Apply(void* p_ctx) {
        if (p_ctx == nullptr) return false;
        return ((CL_E10_EliteAirMouse*)p_ctx)->applyFromConfig();
    }

    // control
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
        _precisionMode = (p_enable && _precisionEnable);
        _precSmX = 0.0f;
        _precSmY = 0.0f;
        unlock_();
        return true;
    }

    // /api/ppt/test에서 사용 (Apply without Save)
    bool testPptKey2(uint8_t p_page, uint8_t p_mod, uint32_t p_code) {
        if (!_hid.isConnected()) return false;
        sendPptKey2_(p_page, p_mod, p_code);
        return true;
    }

    // /api/status: 스냅샷 반환
    void getStatus(ST_E10_Status_t& p_out) {
        memset(&p_out, 0, sizeof(p_out));
        lock_();
        p_out = _statusSnap;
        unlock_();
    }

  private:
    // -------------------------
    // Config apply
    // -------------------------
    bool applyFromConfig() {
        if (_cfg == nullptr) return false;

        ST_C10_WiFiConfig_t v_w;
        ST_C10_E10Config_t  v_e;
        _cfg->makeDefaultsWiFi(v_w);
        _cfg->makeDefaultsE10(v_e);
        (void)_cfg->loadAll(v_w, v_e);

        lock_();

        _dpiLevel = (int)v_e.dpi_level;
        _hardClickLock = v_e.hard_click_lock;

        for (int i=0; i<3; i++) {
            _scaleBase[i] = v_e.scale_base[i];
            _accelGain[i] = v_e.accel_gain[i];
        }

        _accelTh = v_e.accel_threshold;

        _wheelThDeg = v_e.wheel_threshold_deg;
        _wheelStepMax = (int)v_e.wheel_step_max;

        _gestureFlickDeg = v_e.gesture_flick_deg;
        _gestureCooldownMs = v_e.gesture_cooldown_ms;

        _scrollCursorDamp = v_e.scroll_cursor_damp;

        _precisionEnable = v_e.precision_enable;
        _precDeadzone    = v_e.precision_deadzone;
        _precGain        = v_e.precision_gain;
        _precAccel       = v_e.precision_accel;
        _precMaxStep     = v_e.precision_max_step;
        _precSmooth      = v_e.precision_smooth;

        if (!_precisionEnable) _precisionMode = false;

        // PPT v2
        _ppt2_start = v_e.ppt2_start;
        _ppt2_exit  = v_e.ppt2_exit;
        _ppt2_next  = v_e.ppt2_next;
        _ppt2_prev  = v_e.ppt2_prev;
        _ppt2_black = v_e.ppt2_black;
        _ppt2_laser = v_e.ppt2_laser;

        _engine.setHardClickLock(_hardClickLock);
        _engine.setDPI(_dpiLevel);

        unlock_();

        Serial.printf("[E10] apply ok: dpi=%d hard=%d accelTh=%.2f wheelTh=%.1f step=%d flick=%.1f cd=%u damp=%.2f precEn=%d\n",
                      _dpiLevel, _hardClickLock ? 1 : 0, _accelTh, _wheelThDeg, _wheelStepMax,
                      _gestureFlickDeg, (unsigned)_gestureCooldownMs, _scrollCursorDamp,
                      _precisionEnable ? 1 : 0);

        return true;
    }

    // -------------------------
    // Modifier policy (W10 mods mask == HID modifier byte 1:1)
    // -------------------------
    static bool isModifierUsage_(uint8_t p_usage) {
        return (p_usage >= 0xE0 && p_usage <= 0xE7);
    }

    void tapUsageKb_(uint8_t p_usage, uint16_t p_ms = 12) {
        if (p_usage == 0) return;
        if (_keyboard == nullptr) return;
        _keyboard->keyPress(p_usage);
        vTaskDelay(pdMS_TO_TICKS(p_ms));
        _keyboard->keyRelease(p_usage);
    }

    void tapComboUsageKb_(uint8_t p_modMask, uint8_t p_usage, uint16_t p_ms = 22) {
        if (p_usage == 0) return;
        if (_keyboard == nullptr) return;

        // [P0] 일반 key에 modifier usage가 들어오면 차단(정책 일관성)
        if (isModifierUsage_(p_usage)) {
            pushErr_(EN_E10_ERR_INVALID_KB_USAGE, p_usage);
            return;
        }

        if (p_modMask) _keyboard->modifierKeyPress(p_modMask);
        _keyboard->keyPress(p_usage);
        vTaskDelay(pdMS_TO_TICKS(p_ms));
        _keyboard->keyRelease(p_usage);
        if (p_modMask) _keyboard->modifierKeyRelease(p_modMask);
    }

    void tapConsumerMask_(uint32_t p_mask, uint16_t p_ms = 28) {
        if (p_mask == 0) return;
        if (_keyboard == nullptr) return;
        // KeyboardDevice::mediaKeyPress는 "mask(bitflag)" 방식
        _keyboard->mediaKeyPress(p_mask);
        vTaskDelay(pdMS_TO_TICKS(p_ms));
        _keyboard->mediaKeyRelease(p_mask);
    }

    void sendPptKey2_(uint8_t p_page, uint8_t p_mod, uint32_t p_code) {
        if (p_page == (uint8_t)EN_C10_KEYPAGE_CONSUMER) {
            // Consumer page: code=mask
            tapConsumerMask_(p_code);
            return;
        }

        // Keyboard page(0x07): code=usage_id (0x00~0xE7)
        uint8_t v_usage = (uint8_t)min((uint32_t)0xE7, p_code);

        // [P0] UI 실수 방어: key로 modifier usage가 오면 차단(모디파이어는 mod byte로만)
        if (isModifierUsage_(v_usage)) {
            pushErr_(EN_E10_ERR_INVALID_KB_USAGE, v_usage);
            return;
        }

        if (p_mod) tapComboUsageKb_(p_mod, v_usage, 22);
        else       tapUsageKb_(v_usage, 12);
    }

    void sendPptKey2FromCfg_(const ST_C10_PptKey2_t& p_k) {
        sendPptKey2_(p_k.page, p_k.mod, p_k.code);
    }

    void processGesturesDeg_(float p_gzDeg) {
        static unsigned long s_last = 0;
        if (millis() - s_last < _gestureCooldownMs) return;

        if (p_gzDeg > _gestureFlickDeg)      { sendPptKey2FromCfg_(_ppt2_prev);  s_last = millis(); }
        else if (p_gzDeg < -_gestureFlickDeg){ sendPptKey2FromCfg_(_ppt2_next);  s_last = millis(); }
    }

    // -------------------------
    // Precision shaping
    // -------------------------
    void applyPrecision_(float& p_fx, float& p_fy) {
        if (!_precisionMode) return;

        if (fabsf(p_fx) < _precDeadzone) p_fx = 0.0f;
        if (fabsf(p_fy) < _precDeadzone) p_fy = 0.0f;

        auto shape = [&](float v)->float {
            float a = fabsf(v);
            if (a < 0.0001f) return 0.0f;
            float n = min(1.0f, a / (float)_precMaxStep);
            float b = n + (_precAccel * n * n);
            float out = b * (float)_precMaxStep;
            out *= _precGain;
            return (v >= 0) ? out : -out;
        };

        float v_tx = constrain(shape(p_fx), -(float)_precMaxStep, (float)_precMaxStep);
        float v_ty = constrain(shape(p_fy), -(float)_precMaxStep, (float)_precMaxStep);

        _precSmX = _precSmX * _precSmooth + v_tx * (1.0f - _precSmooth);
        _precSmY = _precSmY * _precSmooth + v_ty * (1.0f - _precSmooth);

        p_fx = _precSmX;
        p_fy = _precSmY;
    }

    // -------------------------
    // Stats helpers
    // -------------------------
    void welfordAdd_(uint32_t& n, double& mean, double& m2, double x) {
        n++;
        double d  = x - mean;
        mean     += d / (double)n;
        double d2 = x - mean;
        m2       += d * d2;

        // 과도 누적 방지(현장 장시간 운영)
        if (n > 2500) { n = 1; mean = x; m2 = 0.0; }
    }

    float calcRms_(uint32_t n, double m2) {
        if (n < 2) return 0.0f;
        double var = m2 / (double)(n - 1);
        if (var < 0.0) var = 0.0;
        return (float)sqrt(var);
    }

    void pushErr_(uint8_t p_code, uint16_t p_value = 0) {
        ST_E10_ErrEvt_t v_e;
        v_e.ts_ms = (uint32_t)(millis() - _uptime0);
        v_e.code  = p_code;
        v_e.value = p_value;

        _errHist[_errHistHead] = v_e;
        _errHistHead = (uint8_t)((_errHistHead + 1) % s_errHistCap);
        if (_errHistCount < s_errHistCap) _errHistCount++;
    }

    void pushSpike_(uint32_t p_tsMs) {
        _spikes[_spikeHead].ts_ms = p_tsMs;
        _spikeHead = (uint8_t)((_spikeHead + 1) % 32);
        if (_spikeCount < 32) _spikeCount++;
    }

    // -------------------------
    // I2C Recover
    // -------------------------
    bool recoverI2C_() {
        const int v_sda = 4;
        const int v_scl = 5;

        pinMode(v_sda, INPUT_PULLUP);
        pinMode(v_scl, OUTPUT_OPEN_DRAIN);

        // SCL 9 pulses
        for (int i=0; i<9; i++) {
            digitalWrite(v_scl, HIGH); delayMicroseconds(6);
            digitalWrite(v_scl, LOW);  delayMicroseconds(6);
        }
        digitalWrite(v_scl, HIGH); delayMicroseconds(6);

        Wire.end(); delay(5);
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
            pushErr_(EN_E10_ERR_I2C_RECOVER_OK, 0);
        } else {
            _consecutiveRecoverFail++;
            _consecutiveFail++;
            pushErr_(EN_E10_ERR_I2C_RECOVER_FAIL, 0);
        }

        return v_ok;
    }

    // -------------------------
    // Calibration
    // -------------------------
    void runGyroCalibration_() {
        const uint32_t v_t0 = millis();
        uint32_t v_cnt = 0;
        double v_sx = 0.0, v_sy = 0.0, v_sz = 0.0;

        while (millis() - v_t0 < s_calibMs) {
            sensors_event_t v_a, v_g, v_t;
            _mpu.getEvent(&v_a, &v_g, &v_t);

            float v_gx = v_g.gyro.x * RAD_TO_DEG;
            float v_gy = v_g.gyro.y * RAD_TO_DEG;
            float v_gz = v_g.gyro.z * RAD_TO_DEG;

            float v_m = max(max(fabsf(v_gx), fabsf(v_gy)), fabsf(v_gz));
            if (v_m < s_calibStillThDeg) {
                v_sx += v_gx; v_sy += v_gy; v_sz += v_gz; v_cnt++;
            }
            vTaskDelay(pdMS_TO_TICKS(5));
        }

        if (v_cnt > 0) {
            _gyroBiasX = (float)(v_sx / (double)v_cnt);
            _gyroBiasY = (float)(v_sy / (double)v_cnt);
            _gyroBiasZ = (float)(v_sz / (double)v_cnt);
        }

        _gyroCalibDone = true;
        Serial.printf("[E10] gyro calib: bx=%.3f by=%.3f bz=%.3f samples=%u\n",
                      _gyroBiasX, _gyroBiasY, _gyroBiasZ, (unsigned)v_cnt);
    }

    // -------------------------
    // Status Snapshot
    // -------------------------
    uint16_t calcSpikeCount10s_(uint32_t p_nowMs) {
        uint16_t v_sc = 0;
        for (uint8_t i=0; i<_spikeCount; i++) {
            int v_idx = (int)_spikeHead - 1 - (int)i;
            if (v_idx < 0) v_idx += 32;
            if (p_nowMs - _spikes[v_idx].ts_ms <= 10000) v_sc++;
            else break;
        }
        return v_sc;
    }

    void updateStatusSnapIfDue_(uint32_t p_nowMs) {
        if (p_nowMs - _snapTsMs < 200) return;
        _snapTsMs = p_nowMs;

        ST_E10_Status_t v_s;
        memset(&v_s, 0, sizeof(v_s));

        v_s.ble_connected = _hid.isConnected();
        v_s.ppt_mode      = _isPptMode;
        v_s.dpi_level     = (uint8_t)_dpiLevel;
        v_s.precision_mode= _precisionMode;

        v_s.gyro_bias_x = _gyroBiasX;
        v_s.gyro_bias_y = _gyroBiasY;
        v_s.gyro_bias_z = _gyroBiasZ;
        v_s.temp_c      = _tempC;

        v_s.sampling_ms_target = 8;
        v_s.sampling_ms_avg    = _dtAvgMs;

        v_s.i2c_recover_count   = _i2cRecoverCount;
        v_s.i2c_recover_last_ok = _i2cRecoverLastOk;

        v_s.err_mpu_nan     = _errMpuNan;
        v_s.err_mutex_miss  = _errMutexMiss;
        v_s.err_task_overrun= _errTaskOverrun;

        v_s.gyro_rms   = calcRms_(_gyroN, _gyroM2);
        v_s.cursor_rms = calcRms_(_curN,  _curM2);

        v_s.spike_count_10s          = calcSpikeCount10s_(p_nowMs);
        v_s.consecutive_fail         = _consecutiveFail;
        v_s.consecutive_recover_fail = _consecutiveRecoverFail;

        v_s.uptime_ms = p_nowMs;

        // err history newest-first
        v_s.err_hist_n = (uint8_t)min((uint8_t)s_errHistCap, _errHistCount);
        for (uint8_t i=0; i<v_s.err_hist_n; i++) {
            int v_idx = (int)_errHistHead - 1 - (int)i;
            if (v_idx < 0) v_idx += s_errHistCap;
            v_s.err_hist[i] = _errHist[v_idx];
        }

        // health score (현장 판단용)
        uint16_t v_score = 1000;
        v_score = (uint16_t)max(0, (int)v_score - (int)(v_s.gyro_rms * 25.0f));
        v_score = (uint16_t)max(0, (int)v_score - (int)(v_s.cursor_rms * 18.0f));
        v_score = (uint16_t)max(0, (int)v_score - (int)min((uint32_t)400, v_s.err_mpu_nan * 20));
        v_score = (uint16_t)max(0, (int)v_score - (int)min((uint32_t)300, v_s.i2c_recover_count * 35));
        v_score = (uint16_t)max(0, (int)v_score - (int)min((uint32_t)300, (uint32_t)v_s.spike_count_10s * 12));
        v_score = (uint16_t)max(0, (int)v_score - (int)min((uint32_t)400, (uint32_t)v_s.consecutive_fail * 18));

        // [P1] recover fail도 반영(현장 체감에 매우 중요)
        v_score = (uint16_t)max(0, (int)v_score - (int)min((uint32_t)500, (uint32_t)v_s.consecutive_recover_fail * 35));

        v_s.health_score = v_score;
        v_s.health = (v_score >= 820) ? (uint8_t)EN_E10_HEALTH_OK
                   : (v_score >= 620) ? (uint8_t)EN_E10_HEALTH_WARN
                                      : (uint8_t)EN_E10_HEALTH_DEGRADED;

        // snapshot commit (mutex 보호)
        lock_();
        _statusSnap = v_s;
        unlock_();
    }

    // -------------------------
    // Mouse send
    // -------------------------
    static void mouseSend_(MouseDevice& p_ms, int8_t p_dx, int8_t p_dy, int8_t p_wheel) {
        p_ms.mouseMove(p_dx, p_dy, p_wheel, 0);
    }

    // -------------------------
    // Mutex helpers
    // -------------------------
    void lock_()   { if (_mutex) (void)xSemaphoreTake(_mutex, portMAX_DELAY); }
    void unlock_() { if (_mutex) xSemaphoreGive(_mutex); }

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
            sensors_event_t v_a, v_g, v_t;
            v_m->_mpu.getEvent(&v_a, &v_g, &v_t);
            v_m->_tempC = v_t.temperature;

            const unsigned long v_nowUs = micros();
            const float v_dt = (v_nowUs - v_lastUs) / 1000000.0f;
            v_lastUs = v_nowUs;

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

            float v_gx = (v_g.gyro.x * RAD_TO_DEG) - v_m->_gyroBiasX;
            float v_gy = (v_g.gyro.y * RAD_TO_DEG) - v_m->_gyroBiasY;
            float v_gz = (v_g.gyro.z * RAD_TO_DEG) - v_m->_gyroBiasZ;

            const uint32_t v_ts = (uint32_t)(millis() - v_m->_uptime0);

            // spike anomaly
            if (fabsf(v_gz) > s_spikeThDeg) v_m->pushSpike_(v_ts);

            // NaN guard + recover
            if (isnan(v_gx) || isnan(v_gy) || isnan(v_gz)) {
                v_m->_errMpuNan++;
                v_m->_consecutiveFail++;
                v_m->pushErr_(EN_E10_ERR_MPU_NAN, 0);

                if ((v_m->_errMpuNan % 5) == 0) (void)v_m->recoverI2C_();

                v_m->updateStatusSnapIfDue_(v_ts);
                vTaskDelayUntil(&v_lastWake, pdMS_TO_TICKS(8));
                continue;
            } else {
                // 정상 샘플이면 연속실패를 완만히 감소
                if (v_m->_consecutiveFail > 0) v_m->_consecutiveFail--;
            }

            v_m->welfordAdd_(v_m->_gyroN, v_m->_gyroMean, v_m->_gyroM2, (double)v_gz);

            v_m->_engine.updateOrientation(v_a.acceleration.y, v_a.acceleration.z, v_gx, v_dt);

            int v_tx = 0, v_ty = 0;
            v_m->_engine.process(-v_gz, -v_gx, v_tx, v_ty);

            const bool v_leftClick = (digitalRead(s_btnL) == LOW);
            if (v_leftClick) v_m->_engine.notifyClick();

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

            if (!v_scrollMode) v_m->applyPrecision_(v_fx, v_fy);

            v_m->welfordAdd_(v_m->_curN, v_m->_curMean, v_m->_curM2, (double)sqrtf(v_fx*v_fx + v_fy*v_fy));

            if (v_m->_isPptMode && !v_scrollMode) v_m->processGesturesDeg_(v_gz);

            int v_wheel = 0;
            if (v_scrollMode) {
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
            }

            if (xSemaphoreTake(v_m->_mutex, 0) == pdTRUE) {
                v_m->_state.x = (int)v_fx;
                v_m->_state.y = (int)v_fy;
                v_m->_state.wheel = v_wheel;
                v_m->_state.updated = true;
                xSemaphoreGive(v_m->_mutex);
            } else {
                v_m->_errMutexMiss++;
                v_m->pushErr_(EN_E10_ERR_MUTEX_MISS, 0);
            }

            if (v_m->_hid.isConnected()) {
                if (v_leftClick) v_m->_mouse.mousePress(s_mouseBtnLeft);
                else             v_m->_mouse.mouseRelease(s_mouseBtnLeft);
            }

            // status snapshot update (200ms)
            v_m->updateStatusSnapIfDue_(v_ts);

            // overrun 감지(대략)
            TickType_t v_before = xTaskGetTickCount();
            vTaskDelayUntil(&v_lastWake, pdMS_TO_TICKS(8));
            TickType_t v_after = xTaskGetTickCount();
            if ((v_after - v_before) == 0) {
                v_m->_errTaskOverrun++;
                if ((v_m->_errTaskOverrun % 10) == 0) v_m->pushErr_(EN_E10_ERR_TASK_OVERRUN, 0);
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