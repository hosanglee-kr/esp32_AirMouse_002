#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : E10_EliteAirMouse_021.h
 * 모듈약어 : E10
 * 모듈명 : AirMouse+Presenter (P1 Status Noise/Recover/History, P2 Precision FSM)
 * ------------------------------------------------------
 * 기능 요약
 *  - /api/status 확장:
 *     - 센서 노이즈 지표(gyro/acc 분산; Welford EMA 형태)
 *     - I2C/MPU recover 결과(last ok/fail, last ms, fail streak)
 *     - 최근 N초 에러 히스토리(초단위 링버퍼: mpu_read/recover/task_overrun)
 *  - Precision(조이스틱 정밀 모드 FSM):
 *     - OFF -> ARMING(홀드) -> ON -> COOLDOWN
 *     - 버튼 롱프레스(기본: BTN_SCROLL) 또는 Web Control로 ON/OFF
 *     - ON 상태에서는 커서 출력에 precision_scale 적용(“조이스틱 정밀” 개념의 토대)
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
#include <WiFi.h>

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#include <BleCompositeHID.h>
#include <KeyboardDevice.h>
#include <MouseDevice.h>

#include "M10_MotionProc_020.h"
#include "C10_Config_021.h"

typedef bool (*T_E10_ApplyFn)(void* p_ctx);

enum EN_E10_PrecisionState : uint8_t {
    EN_E10_PREC_OFF = 0,
    EN_E10_PREC_ARMING = 1,
    EN_E10_PREC_ON = 2,
    EN_E10_PREC_COOLDOWN = 3
};

struct ST_E10_Status_t {
    // runtime
    bool     ble_connected;
    bool     ppt_mode;
    uint8_t  dpi_level;
    bool     degraded;

    // precision
    uint8_t  precision_state;
    bool     precision_enable;
    float    precision_scale;

    // sensor
    bool  gyro_calib_done;
    float gyro_bias_x;
    float gyro_bias_y;
    float gyro_bias_z;
    float temp_c;

    // noise (variance-like)
    float gyro_var_x;
    float gyro_var_y;
    float gyro_var_z;
    float acc_var_x;
    float acc_var_y;
    float acc_var_z;

    // timing
    uint32_t sampling_ms_target;
    float    sampling_ms_avg;

    // counters
    uint32_t err_mpu_read;
    uint32_t err_mpu_recover;
    uint32_t err_task_overrun;
    uint32_t reconnect_count;

    // recover last result
    bool     recover_last_ok;
    uint32_t recover_last_ms;
    uint16_t mpu_fail_streak;

    // error history (last N seconds)
    uint8_t  hist_len;
    uint8_t  hist_pos;
    // (주의) 여기에는 값만 담고, W10에서 JSON 배열로 풀어줌
    uint16_t hist_mpu_read[30];
    uint16_t hist_recover[30];
    uint16_t hist_overrun[30];

    // net
    int32_t  rssi;

    // stack
    uint32_t stack_sensor_min_words;
    uint32_t stack_comm_min_words;

    // misc
    uint32_t uptime_ms;
};

// W10 mods(mask) 정책: modifierKeyPress/Release(mask)로 그대로 전송
class CL_E10_EliteAirMouse {
  private:
    Adafruit_MPU6050 _mpu;

    BleCompositeHID _hid;
    KeyboardDevice  _keyboard;
    MouseDevice     _mouse;

    CL_M10_AdvancedMotionProcessor _engine;
    CL_C10_Config* _cfg = nullptr;

    static constexpr int s_btnL      = 12;
    static constexpr int s_btnMode   = 13;
    static constexpr int s_btnScroll = 14;

    static constexpr uint8_t s_mouseBtnLeft = 0x01;

    // lock-free double buffer
    struct ST_E10_Packet_t {
        int16_t dx;
        int16_t dy;
        int16_t wheel;
        uint8_t btn_mask;
        bool    valid;
    };
    ST_E10_Packet_t _buf[2];
    volatile uint8_t _idxW = 0;
    volatile uint8_t _idxR = 1;
    portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;

    volatile bool _isPptMode = false;

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

    // drift/idle
    float    _idleGyroThDeg = 2.0f;
    uint16_t _idleHoldMs = 1200;
    float    _biasTrackAlpha = 0.008f;
    float    _zeroSnapTh = 0.6f;

    // precision FSM (P2 foundation)
    bool _precisionEnable = false;
    float _precisionScale = 0.35f;
    uint16_t _precisionHoldMs = 450;
    uint16_t _precisionCooldownMs = 350;
    EN_E10_PrecisionState _precisionState = EN_E10_PREC_OFF;
    uint32_t _precisionT0 = 0;

    ST_C10_PptKey_t _pptStart;
    ST_C10_PptKey_t _pptExit;
    ST_C10_PptKey_t _pptNext;
    ST_C10_PptKey_t _pptPrev;
    ST_C10_PptKey_t _pptBlack;
    ST_C10_PptKey_t _pptLaser;

    // status vars
    float _tempC = 0.0f;
    uint32_t _uptime0 = 0;

    float _dtAvgMs = 8.0f;

    // counters
    uint32_t _errMpuRead = 0;
    uint32_t _errMpuRecover = 0;
    uint32_t _errTaskOverrun = 0;
    uint32_t _reconnectCount = 0;

    bool _degraded = false;

    // mpu recover
    uint16_t _mpuFailStreak = 0;
    static constexpr uint16_t s_mpuFailStreakTh = 10;

    bool _recoverLastOk = true;
    uint32_t _recoverLastMs = 0;

    // stack watermark
    static TaskHandle_t s_taskSensor;
    static TaskHandle_t s_taskComm;
    uint32_t _stackSensorMin = 0;
    uint32_t _stackCommMin = 0;

    // noise estimator (EMA-ish Welford)
    struct ST_E10_Noise_t {
        float mean;
        float m2;
        uint32_t n;
    };
    ST_E10_Noise_t _ngX, _ngY, _ngZ, _naX, _naY, _naZ;
    static constexpr float s_noiseDecay = 0.985f; // 1.0에 가까울수록 긴 시간 평균

    // error history per second
    static constexpr uint8_t s_histLen = 30;
    uint16_t _histMpuRead[s_histLen];
    uint16_t _histRecover[s_histLen];
    uint16_t _histOver[s_histLen];
    uint8_t  _histPos = 0;
    uint32_t _histTickMs = 0;

  public:
    CL_E10_EliteAirMouse() : _hid("Elite AirMouse S3", "ProMaker", 100) {
        memset(&_buf, 0, sizeof(_buf));
        memset(&_pptStart, 0, sizeof(_pptStart));
        memset(&_pptExit,  0, sizeof(_pptExit));
        memset(&_pptNext,  0, sizeof(_pptNext));
        memset(&_pptPrev,  0, sizeof(_pptPrev));
        memset(&_pptBlack, 0, sizeof(_pptBlack));
        memset(&_pptLaser, 0, sizeof(_pptLaser));

        memset(&_ngX, 0, sizeof(_ngX));
        memset(&_ngY, 0, sizeof(_ngY));
        memset(&_ngZ, 0, sizeof(_ngZ));
        memset(&_naX, 0, sizeof(_naX));
        memset(&_naY, 0, sizeof(_naY));
        memset(&_naZ, 0, sizeof(_naZ));

        memset(_histMpuRead, 0, sizeof(_histMpuRead));
        memset(_histRecover, 0, sizeof(_histRecover));
        memset(_histOver,    0, sizeof(_histOver));
        _histTickMs = millis();
    }

    void begin(CL_C10_Config* p_cfg) {
        _cfg = p_cfg;
        _uptime0 = millis();

        Serial.begin(115200);

        Wire.begin(4, 5);
        Wire.setClock(400000);

        if (!_mpu.begin()) {
            Serial.println("[E10] MPU not found => degraded");
            _degraded = true;
        } else {
            _mpu.setGyroRange(MPU6050_RANGE_250_DEG);
            _mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
            _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
        }

        pinMode(s_btnL, INPUT_PULLUP);
        pinMode(s_btnMode, INPUT_PULLUP);
        pinMode(s_btnScroll, INPUT_PULLUP);

        (void)applyFromConfig();

        _hid.addDevice(&_keyboard);
        _hid.addDevice(&_mouse);
        _hid.begin();

        xTaskCreatePinnedToCore(sensorTask, "E10_Sensor", 8192, this, 3, &s_taskSensor, 1);
        xTaskCreatePinnedToCore(commTask,   "E10_Comm",   4096, this, 2, &s_taskComm,   0);
    }

    static bool E10_W10Apply(void* p_ctx) {
        if (p_ctx == nullptr) return false;
        return ((CL_E10_EliteAirMouse*)p_ctx)->applyFromConfig();
    }

    // web immediate control
    bool setPptMode(bool p_enable) { _isPptMode = p_enable; return true; }

    bool setDpiLevel(uint8_t p_level) {
        if (p_level < 1) p_level = 1;
        if (p_level > 3) p_level = 3;
        _dpiLevel = (int)p_level;
        _engine.setDPI(_dpiLevel);
        return true;
    }

    bool setHardClickLock(bool p_enable) {
        _hardClickLock = p_enable;
        _engine.setHardClickLock(_hardClickLock);
        return true;
    }

    bool setPrecisionEnable(bool p_enable) {
        _precisionEnable = p_enable;
        if (!p_enable) {
            _precisionState = EN_E10_PREC_OFF;
            _precisionT0 = 0;
        }
        return true;
    }

    bool setPrecisionScale(float p_scale) {
        if (p_scale < 0.05f) p_scale = 0.05f;
        if (p_scale > 1.0f) p_scale = 1.0f;
        _precisionScale = p_scale;
        return true;
    }

    void getStatus(ST_E10_Status_t& p_out) {
        memset(&p_out, 0, sizeof(p_out));

        p_out.ble_connected = _hid.isConnected();
        p_out.ppt_mode = _isPptMode;
        p_out.dpi_level = (uint8_t)_dpiLevel;
        p_out.degraded = _degraded;

        p_out.precision_state = (uint8_t)_precisionState;
        p_out.precision_enable = _precisionEnable;
        p_out.precision_scale = _precisionScale;

        p_out.gyro_calib_done = _gyroCalibDone;
        p_out.gyro_bias_x = _gyroBiasX;
        p_out.gyro_bias_y = _gyroBiasY;
        p_out.gyro_bias_z = _gyroBiasZ;
        p_out.temp_c = _tempC;

        // variance-like (m2/n)
        p_out.gyro_var_x = (_ngX.n > 5) ? (_ngX.m2 / (float)_ngX.n) : 0.0f;
        p_out.gyro_var_y = (_ngY.n > 5) ? (_ngY.m2 / (float)_ngY.n) : 0.0f;
        p_out.gyro_var_z = (_ngZ.n > 5) ? (_ngZ.m2 / (float)_ngZ.n) : 0.0f;
        p_out.acc_var_x  = (_naX.n > 5) ? (_naX.m2 / (float)_naX.n) : 0.0f;
        p_out.acc_var_y  = (_naY.n > 5) ? (_naY.m2 / (float)_naY.n) : 0.0f;
        p_out.acc_var_z  = (_naZ.n > 5) ? (_naZ.m2 / (float)_naZ.n) : 0.0f;

        p_out.sampling_ms_target = 8;
        p_out.sampling_ms_avg = _dtAvgMs;

        p_out.err_mpu_read = _errMpuRead;
        p_out.err_mpu_recover = _errMpuRecover;
        p_out.err_task_overrun = _errTaskOverrun;
        p_out.reconnect_count = _reconnectCount;

        p_out.recover_last_ok = _recoverLastOk;
        p_out.recover_last_ms = _recoverLastMs;
        p_out.mpu_fail_streak = _mpuFailStreak;

        p_out.hist_len = s_histLen;
        p_out.hist_pos = _histPos;
        for (uint8_t i = 0; i < s_histLen; i++) {
            p_out.hist_mpu_read[i] = _histMpuRead[i];
            p_out.hist_recover[i]  = _histRecover[i];
            p_out.hist_overrun[i]  = _histOver[i];
        }

        p_out.rssi = (WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;

        if (s_taskSensor != nullptr) {
            UBaseType_t v_w = uxTaskGetStackHighWaterMark(s_taskSensor);
            if (_stackSensorMin == 0 || (uint32_t)v_w < _stackSensorMin) _stackSensorMin = (uint32_t)v_w;
            p_out.stack_sensor_min_words = _stackSensorMin;
        }
        if (s_taskComm != nullptr) {
            UBaseType_t v_w = uxTaskGetStackHighWaterMark(s_taskComm);
            if (_stackCommMin == 0 || (uint32_t)v_w < _stackCommMin) _stackCommMin = (uint32_t)v_w;
            p_out.stack_comm_min_words = _stackCommMin;
        }

        p_out.uptime_ms = (uint32_t)(millis() - _uptime0);
    }

  private:
    bool applyFromConfig() {
        if (_cfg == nullptr) return false;

        ST_C10_WiFiConfig_t v_w;
        ST_C10_E10Config_t  v_e;
        memset(&v_w, 0, sizeof(v_w));
        memset(&v_e, 0, sizeof(v_e));

        (void)_cfg->loadAll(v_w, v_e);

        _dpiLevel = (int)v_e.dpi_level;
        _hardClickLock = v_e.hard_click_lock;

        _scaleBase[0] = v_e.scale_base[0];
        _scaleBase[1] = v_e.scale_base[1];
        _scaleBase[2] = v_e.scale_base[2];

        _accelGain[0] = v_e.accel_gain[0];
        _accelGain[1] = v_e.accel_gain[1];
        _accelGain[2] = v_e.accel_gain[2];

        _accelTh = v_e.accel_threshold;

        _wheelThDeg = v_e.wheel_threshold_deg;
        _wheelStepMax = (int)v_e.wheel_step_max;

        _gestureFlickDeg = v_e.gesture_flick_deg;
        _gestureCooldownMs = v_e.gesture_cooldown_ms;

        _scrollCursorDamp = v_e.scroll_cursor_damp;

        _idleGyroThDeg = v_e.idle_gyro_th_deg;
        _idleHoldMs = v_e.idle_hold_ms;
        _biasTrackAlpha = v_e.bias_track_alpha;
        _zeroSnapTh = v_e.zero_snap_th;

        _precisionEnable = v_e.precision_enable;
        _precisionScale = v_e.precision_scale;
        _precisionHoldMs = v_e.precision_hold_ms;
        _precisionCooldownMs = v_e.precision_cooldown_ms;

        _pptStart = v_e.ppt_start;
        _pptExit  = v_e.ppt_exit;
        _pptNext  = v_e.ppt_next;
        _pptPrev  = v_e.ppt_prev;
        _pptBlack = v_e.ppt_black;
        _pptLaser = v_e.ppt_laser;

        _engine.setHardClickLock(_hardClickLock);
        _engine.setDPI(_dpiLevel);
        _engine.setZeroSnapTh(_zeroSnapTh);

        if (!_precisionEnable) _precisionState = EN_E10_PREC_OFF;

        Serial.printf("[E10] apply ok: dpi=%d hard=%d accelTh=%.2f wheelTh=%.1f step=%d flick=%.1f cd=%u damp=%.2f idleTh=%.2f idleHold=%u alpha=%.4f snap=%.2f precEn=%d precScale=%.2f\n",
                      _dpiLevel, _hardClickLock ? 1 : 0, _accelTh, _wheelThDeg, _wheelStepMax,
                      _gestureFlickDeg, (unsigned)_gestureCooldownMs, _scrollCursorDamp,
                      _idleGyroThDeg, (unsigned)_idleHoldMs, _biasTrackAlpha, _zeroSnapTh,
                      _precisionEnable ? 1 : 0, _precisionScale);

        return true;
    }

    void tapKeyUsage(uint8_t p_usage, uint16_t p_ms = 12) {
        if (p_usage == 0) return;
        _keyboard.keyPress(p_usage);
        vTaskDelay(pdMS_TO_TICKS(p_ms));
        _keyboard.keyRelease(p_usage);
    }

    void tapComboUsage(uint8_t p_modMask, uint8_t p_usage, uint16_t p_ms = 18) {
        if (p_usage == 0) return;
        if (p_modMask != 0) _keyboard.modifierKeyPress(p_modMask);
        _keyboard.keyPress(p_usage);
        vTaskDelay(pdMS_TO_TICKS(p_ms));
        _keyboard.keyRelease(p_usage);
        if (p_modMask != 0) _keyboard.modifierKeyRelease(p_modMask);
    }

    void sendPptKey(const ST_C10_PptKey_t& p_k) {
        if (!_hid.isConnected()) return;
        if (p_k.key == 0) return;
        if (p_k.mod == 0) tapKeyUsage(p_k.key);
        else tapComboUsage(p_k.mod, p_k.key, 22);
    }

    void processGesturesDeg(float p_gzDegPerSec) {
        static unsigned long s_lastFlick = 0;
        if (millis() - s_lastFlick < _gestureCooldownMs) return;

        if (p_gzDegPerSec > _gestureFlickDeg) { sendPptKey(_pptPrev); s_lastFlick = millis(); }
        else if (p_gzDegPerSec < -_gestureFlickDeg) { sendPptKey(_pptNext); s_lastFlick = millis(); }
    }

    static void mouseSend(MouseDevice& p_ms, int8_t p_dx, int8_t p_dy, int8_t p_wheel) {
        p_ms.mouseMove(p_dx, p_dy, p_wheel, 0);
    }

    void publishPacket(int16_t p_dx, int16_t p_dy, int16_t p_wheel, uint8_t p_btnMask) {
        uint8_t v_w = _idxW;
        _buf[v_w].dx = p_dx;
        _buf[v_w].dy = p_dy;
        _buf[v_w].wheel = p_wheel;
        _buf[v_w].btn_mask = p_btnMask;
        _buf[v_w].valid = true;

        portENTER_CRITICAL(&_mux);
        _idxR = v_w;
        _idxW = (uint8_t)(1 - v_w);
        portEXIT_CRITICAL(&_mux);
    }

    bool readLatestPacket(ST_E10_Packet_t& p_out) {
        uint8_t v_r;
        portENTER_CRITICAL(&_mux);
        v_r = _idxR;
        portEXIT_CRITICAL(&_mux);

        if (!_buf[v_r].valid) return false;
        p_out = _buf[v_r];
        return true;
    }

    void runGyroCalibration() {
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

    bool recoverMpu() {
        _errMpuRecover++;

        Wire.end();
        delay(10);
        Wire.begin(4, 5);
        Wire.setClock(400000);
        delay(10);

        bool v_ok = _mpu.begin();
        if (!v_ok) {
            Serial.println("[E10] recoverMpu: begin failed");
            _recoverLastOk = false;
            _recoverLastMs = millis();
            return false;
        }

        _mpu.setGyroRange(MPU6050_RANGE_250_DEG);
        _mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
        _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

        _mpuFailStreak = 0;
        _degraded = false;

        _recoverLastOk = true;
        _recoverLastMs = millis();
        Serial.println("[E10] recoverMpu: OK");
        return true;
    }

    // noise update with decay (keeps responsiveness)
    void updateNoise(ST_E10_Noise_t& p_s, float p_x) {
        // decay old accumulation
        p_s.mean *= s_noiseDecay;
        p_s.m2   *= s_noiseDecay;
        p_s.n     = (uint32_t)((float)p_s.n * s_noiseDecay);

        float v_n1 = (float)(p_s.n + 1);
        float v_delta = p_x - p_s.mean;
        p_s.mean += v_delta / v_n1;
        float v_delta2 = p_x - p_s.mean;
        p_s.m2 += v_delta * v_delta2;
        p_s.n += 1;
        if (p_s.n > 1000000UL) { p_s.n = 20000; p_s.m2 *= 0.02f; } // 안전 클램프
    }

    void tickHistory1s() {
        uint32_t v_now = millis();
        if (v_now - _histTickMs < 1000) return;

        uint32_t v_steps = (v_now - _histTickMs) / 1000;
        if (v_steps > s_histLen) v_steps = s_histLen;

        for (uint32_t i = 0; i < v_steps; i++) {
            _histPos = (uint8_t)((_histPos + 1) % s_histLen);
            _histMpuRead[_histPos] = 0;
            _histRecover[_histPos] = 0;
            _histOver[_histPos] = 0;
        }
        _histTickMs = v_now;
    }

    void precisionFsmUpdate(bool p_btnScrollHeld, bool p_btnScrollReleased) {
        if (!_precisionEnable) {
            _precisionState = EN_E10_PREC_OFF;
            _precisionT0 = 0;
            return;
        }

        uint32_t now = millis();

        switch (_precisionState) {
            case EN_E10_PREC_OFF:
                if (p_btnScrollHeld) {
                    if (_precisionT0 == 0) _precisionT0 = now;
                    if (now - _precisionT0 >= _precisionHoldMs) {
                        _precisionState = EN_E10_PREC_ON;
                        _precisionT0 = now;
                    }
                } else {
                    _precisionT0 = 0;
                }
                break;

            case EN_E10_PREC_ON:
                // release -> cooldown (토글형 UX)
                if (p_btnScrollReleased) {
                    _precisionState = EN_E10_PREC_COOLDOWN;
                    _precisionT0 = now;
                }
                break;

            case EN_E10_PREC_COOLDOWN:
                if (now - _precisionT0 >= _precisionCooldownMs) {
                    _precisionState = EN_E10_PREC_OFF;
                    _precisionT0 = 0;
                }
                break;

            case EN_E10_PREC_ARMING:
            default:
                _precisionState = EN_E10_PREC_OFF;
                _precisionT0 = 0;
                break;
        }
    }

    static void sensorTask(void* p_pv) {
        CL_E10_EliteAirMouse* v_m = (CL_E10_EliteAirMouse*)p_pv;

        TickType_t v_lastWake = xTaskGetTickCount();
        unsigned long v_lastUs = micros();
        unsigned long v_btnModeDownMs = 0;

        // scroll hold tracking (precision)
        bool v_prevScroll = false;
        uint32_t v_scrollDownMs = 0;

        // drift idle tracking
        uint32_t v_idleStart = 0;

        if (!v_m->_degraded && !v_m->_gyroCalibDone) v_m->runGyroCalibration();

        bool v_prevConn = false;

        for (;;) {
            v_m->tickHistory1s();

            bool v_conn = v_m->_hid.isConnected();
            if (v_conn != v_prevConn) {
                if (v_conn) v_m->_reconnectCount++;
                v_prevConn = v_conn;
            }

            if (v_m->_degraded) {
                v_m->publishPacket(0, 0, 0, 0);
                vTaskDelayUntil(&v_lastWake, pdMS_TO_TICKS(8));
                continue;
            }

            sensors_event_t v_a, v_g, v_temp;
            v_m->_mpu.getEvent(&v_a, &v_g, &v_temp);
            v_m->_tempC = v_temp.temperature;

            bool v_sane = isfinite(v_g.gyro.x) && isfinite(v_g.gyro.y) && isfinite(v_g.gyro.z) &&
                          isfinite(v_a.acceleration.x) && isfinite(v_a.acceleration.y) && isfinite(v_a.acceleration.z);

            if (!v_sane) {
                v_m->_errMpuRead++;
                v_m->_histMpuRead[v_m->_histPos]++;

                v_m->_mpuFailStreak++;
                if (v_m->_mpuFailStreak >= s_mpuFailStreakTh) {
                    if (!v_m->recoverMpu()) {
                        v_m->_degraded = true;
                    } else {
                        v_m->_histRecover[v_m->_histPos]++;
                    }
                }
                vTaskDelayUntil(&v_lastWake, pdMS_TO_TICKS(8));
                continue;
            }
            v_m->_mpuFailStreak = 0;

            const unsigned long v_nowUs = micros();
            const float v_dt = (v_nowUs - v_lastUs) / 1000000.0f;
            v_lastUs = v_nowUs;

            const float v_dtMs = v_dt * 1000.0f;
            v_m->_dtAvgMs = v_m->_dtAvgMs * 0.98f + v_dtMs * 0.02f;

            // buttons
            const bool v_scrollDown = (digitalRead(s_btnScroll) == LOW);
            const bool v_scrollHeld = v_scrollDown;
            const bool v_scrollReleased = (!v_scrollDown && v_prevScroll);

            if (v_scrollDown && !v_prevScroll) v_scrollDownMs = millis();
            v_prevScroll = v_scrollDown;

            // precision FSM runs always (uses scroll hold)
            v_m->precisionFsmUpdate(v_scrollHeld, v_scrollReleased);

            const bool v_scrollMode = v_scrollDown; // scroll 버튼은 스크롤 모드도 겸함

            // mode 버튼: short=dpi cycle, long=ppt toggle
            if (digitalRead(s_btnMode) == LOW) {
                if (v_btnModeDownMs == 0) v_btnModeDownMs = millis();
            } else {
                if (v_btnModeDownMs > 0) {
                    const unsigned long v_hold = millis() - v_btnModeDownMs;
                    if (v_hold > 1000) v_m->_isPptMode = !v_m->_isPptMode;
                    else {
                        v_m->_dpiLevel++;
                        if (v_m->_dpiLevel > 3) v_m->_dpiLevel = 1;
                        v_m->_engine.setDPI(v_m->_dpiLevel);
                    }
                    v_btnModeDownMs = 0;
                }
            }

            // raw gyro deg/s
            const float v_gx_raw = (v_g.gyro.x * RAD_TO_DEG);
            const float v_gy_raw = (v_g.gyro.y * RAD_TO_DEG);
            const float v_gz_raw = (v_g.gyro.z * RAD_TO_DEG);

            float v_gx = v_gx_raw - v_m->_gyroBiasX;
            float v_gy = v_gy_raw - v_m->_gyroBiasY;
            float v_gz = v_gz_raw - v_m->_gyroBiasZ;

            // noise update (raw values recommended)
            v_m->updateNoise(v_m->_ngX, v_gx_raw);
            v_m->updateNoise(v_m->_ngY, v_gy_raw);
            v_m->updateNoise(v_m->_ngZ, v_gz_raw);
            v_m->updateNoise(v_m->_naX, v_a.acceleration.x);
            v_m->updateNoise(v_m->_naY, v_a.acceleration.y);
            v_m->updateNoise(v_m->_naZ, v_a.acceleration.z);

            v_m->_engine.updateOrientation(v_a.acceleration.y, v_a.acceleration.z, v_gx, v_dt);

            int v_tx = 0, v_ty = 0;
            v_m->_engine.process(-v_gz, -v_gx, v_tx, v_ty);

            if (v_m->_isPptMode && !v_scrollMode) v_m->processGesturesDeg(v_gz);

            const bool v_leftClick = (digitalRead(s_btnL) == LOW);
            if (v_leftClick) v_m->_engine.notifyClick();

            // drift idle bias tracking
            const float v_absMax = max(max(fabsf(v_gx), fabsf(v_gy)), fabsf(v_gz));
            if (v_absMax < v_m->_idleGyroThDeg) {
                if (v_idleStart == 0) v_idleStart = millis();
                if (millis() - v_idleStart >= v_m->_idleHoldMs) {
                    float a = v_m->_biasTrackAlpha;
                    v_m->_gyroBiasX = v_m->_gyroBiasX * (1.0f - a) + v_gx_raw * a;
                    v_m->_gyroBiasY = v_m->_gyroBiasY * (1.0f - a) + v_gy_raw * a;
                    v_m->_gyroBiasZ = v_m->_gyroBiasZ * (1.0f - a) + v_gz_raw * a;
                }
            } else {
                v_idleStart = 0;
            }

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

            // precision scaling (P2 foundation)
            if (v_m->_precisionState == EN_E10_PREC_ON) {
                // [TIP] precision_scale는 0.25~0.45가 UX가 좋음
                v_fx *= v_m->_precisionScale;
                v_fy *= v_m->_precisionScale;
            }

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

            uint8_t v_btn = v_leftClick ? s_mouseBtnLeft : 0;
            v_m->publishPacket((int16_t)v_fx, (int16_t)v_fy, (int16_t)v_wheel, v_btn);

            TickType_t v_before = xTaskGetTickCount();
            vTaskDelayUntil(&v_lastWake, pdMS_TO_TICKS(8));
            TickType_t v_after = xTaskGetTickCount();
            if ((v_after - v_before) == 0) {
                v_m->_errTaskOverrun++;
                v_m->_histOver[v_m->_histPos]++;
            }
        }
    }

    static void commTask(void* p_pv) {
        CL_E10_EliteAirMouse* v_m = (CL_E10_EliteAirMouse*)p_pv;

        for (;;) {
            if (v_m->_hid.isConnected()) {
                ST_E10_Packet_t v_p;
                if (v_m->readLatestPacket(v_p)) {
                    const int8_t v_dx = (int8_t)constrain(v_p.dx, -127, 127);
                    const int8_t v_dy = (int8_t)constrain(v_p.dy, -127, 127);
                    const int8_t v_wh = (int8_t)constrain(v_p.wheel, -127, 127);

                    mouseSend(v_m->_mouse, v_dx, v_dy, v_wh);

                    if (v_p.btn_mask & s_mouseBtnLeft) v_m->_mouse.mousePress(s_mouseBtnLeft);
                    else v_m->_mouse.mouseRelease(s_mouseBtnLeft);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(7));
        }
    }
};

TaskHandle_t CL_E10_EliteAirMouse::s_taskSensor = nullptr;
TaskHandle_t CL_E10_EliteAirMouse::s_taskComm   = nullptr;

