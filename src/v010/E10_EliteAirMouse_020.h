#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : E10_EliteAirMouse_020.h
 * 모듈약어 : E10
 * 모듈명 : AirMouse+Presenter (Composite HID, P0 LockFree/Drift/Recover, Status/UI Control)
 * ------------------------------------------------------
 * 기능 요약
 *  - MPU6050 기반 에어마우스 + Composite HID(Mouse+Keyboard)
 *  - Gyro 오프셋 자동 캘리브레이션(부팅 1초 평균, 움직임 큰 샘플 제외)
 *  - Drift 완화: 정지 감지 시 slow bias tracking(EMA) + Zero Snap(떨림 제거)
 *  - BTN_SCROLL 전용 버튼 분리(스크롤 UX 충돌 제거)
 *  - Hard Click-Lock 옵션(150ms outX/outY=0 완전 고정)
 *  - Lock-free 더블버퍼로 센서/통신 분리(뮤텍스 지터 제거)
 *  - MPU read 실패 복구(I2C/MPU 재초기화) + degraded 안전화
 *  - /api/status 제공용 상태 구조체(gyro bias/temp/sampling/err/RSSI/stack 등)
 *  - 웹에서 PPT 모드 토글 / DPI 즉시 변경 + 튜닝 즉시 적용
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
#include "C10_Config_020.h"

typedef bool (*T_E10_ApplyFn)(void* p_ctx);

struct ST_E10_Status_t {
    // runtime
    bool     ble_connected;
    bool     ppt_mode;
    uint8_t  dpi_level;
    bool     degraded;

    // sensor
    bool  gyro_calib_done;
    float gyro_bias_x;
    float gyro_bias_y;
    float gyro_bias_z;
    float temp_c;

    // timing
    uint32_t sampling_ms_target;
    float    sampling_ms_avg;

    // counters
    uint32_t err_mpu_read;
    uint32_t err_mpu_recover;
    uint32_t err_task_overrun;
    uint32_t reconnect_count;

    // net
    int32_t  rssi;

    // stack
    uint32_t stack_sensor_min_words;
    uint32_t stack_comm_min_words;

    // misc
    uint32_t uptime_ms;
};

// -----------------------------
// [정책 정리 - 짧게]
// W10 /api/keycodes mods는 HID modifier bitfield(mask) (LCtrl=0x01 ...)
// E10는 그 mask를 그대로 KeyboardDevice::modifierKeyPress/Release(mask)로 전송한다.
// => usage(0xE0~) press 방식보다 OS 호환이 안정.
// -----------------------------

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

    // states
    volatile bool _isPptMode = false;

    // -----------------------------
    // Lock-free double buffer
    // -----------------------------
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

    // -----------------------------
    // Gyro bias
    // -----------------------------
    float _gyroBiasX = 0.0f;
    float _gyroBiasY = 0.0f;
    float _gyroBiasZ = 0.0f;
    bool  _gyroCalibDone = false;

    static constexpr uint32_t s_calibMs = 1000;
    static constexpr float    s_calibStillThDeg = 3.0f;

    // -----------------------------
    // Config runtime (applied)
    // -----------------------------
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

    ST_C10_PptKey_t _pptStart;
    ST_C10_PptKey_t _pptExit;
    ST_C10_PptKey_t _pptNext;
    ST_C10_PptKey_t _pptPrev;
    ST_C10_PptKey_t _pptBlack;
    ST_C10_PptKey_t _pptLaser;

    // -----------------------------
    // Status / counters
    // -----------------------------
    float _tempC = 0.0f;
    uint32_t _uptime0 = 0;

    float _dtAvgMs = 8.0f;

    uint32_t _errMpuRead = 0;
    uint32_t _errMpuRecover = 0;
    uint32_t _errTaskOverrun = 0;

    uint32_t _reconnectCount = 0;
    bool _degraded = false;

    // mpu recover
    uint16_t _mpuFailStreak = 0;
    static constexpr uint16_t s_mpuFailStreakTh = 10;

    // stack watermark
    static TaskHandle_t s_taskSensor;
    static TaskHandle_t s_taskComm;
    uint32_t _stackSensorMin = 0;
    uint32_t _stackCommMin = 0;

  public:
    CL_E10_EliteAirMouse() : _hid("Elite AirMouse S3", "ProMaker", 100) {
        memset(&_buf, 0, sizeof(_buf));
        memset(&_pptStart, 0, sizeof(_pptStart));
        memset(&_pptExit,  0, sizeof(_pptExit));
        memset(&_pptNext,  0, sizeof(_pptNext));
        memset(&_pptPrev,  0, sizeof(_pptPrev));
        memset(&_pptBlack, 0, sizeof(_pptBlack));
        memset(&_pptLaser, 0, sizeof(_pptLaser));
    }

    void begin(CL_C10_Config* p_cfg) {
        _cfg = p_cfg;
        _uptime0 = millis();

        Serial.begin(115200);

        // I2C
        Wire.begin(4, 5);
        Wire.setClock(400000);

        // MPU init
        if (!_mpu.begin()) {
            Serial.println("[E10] Failed to find MPU6050 chip => degraded");
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

        // tasks
        xTaskCreatePinnedToCore(sensorTask, "E10_Sensor", 8192, this, 3, &s_taskSensor, 1);
        xTaskCreatePinnedToCore(commTask,   "E10_Comm",   4096, this, 2, &s_taskComm,   0);
    }

    // W10 apply hook
    static bool E10_W10Apply(void* p_ctx) {
        if (p_ctx == nullptr) return false;
        return ((CL_E10_EliteAirMouse*)p_ctx)->applyFromConfig();
    }

    // -----------------------------
    // UI control (immediate apply)
    // -----------------------------
    bool setPptMode(bool p_enable) {
        _isPptMode = p_enable;
        return true;
    }

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

    // quick tuning (optional)
    bool setTuning(float p_accelTh, float p_scrollDamp, float p_zeroSnapTh) {
        if (p_accelTh < 0.5f) p_accelTh = 0.5f;
        if (p_accelTh > 50.0f) p_accelTh = 50.0f;
        if (p_scrollDamp < 0.05f) p_scrollDamp = 0.05f;
        if (p_scrollDamp > 1.0f) p_scrollDamp = 1.0f;
        if (p_zeroSnapTh < 0.0f) p_zeroSnapTh = 0.0f;
        if (p_zeroSnapTh > 5.0f) p_zeroSnapTh = 5.0f;

        _accelTh = p_accelTh;
        _scrollCursorDamp = p_scrollDamp;
        _zeroSnapTh = p_zeroSnapTh;
        _engine.setZeroSnapTh(_zeroSnapTh);
        return true;
    }

    // /api/status
    void getStatus(ST_E10_Status_t& p_out) {
        memset(&p_out, 0, sizeof(p_out));

        p_out.ble_connected = _hid.isConnected();
        p_out.ppt_mode = _isPptMode;
        p_out.dpi_level = (uint8_t)_dpiLevel;
        p_out.degraded = _degraded;

        p_out.gyro_calib_done = _gyroCalibDone;
        p_out.gyro_bias_x = _gyroBiasX;
        p_out.gyro_bias_y = _gyroBiasY;
        p_out.gyro_bias_z = _gyroBiasZ;
        p_out.temp_c = _tempC;

        p_out.sampling_ms_target = 8;
        p_out.sampling_ms_avg = _dtAvgMs;

        p_out.err_mpu_read = _errMpuRead;
        p_out.err_mpu_recover = _errMpuRecover;
        p_out.err_task_overrun = _errTaskOverrun;
        p_out.reconnect_count = _reconnectCount;

        p_out.rssi = (WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;

        // stack watermark (words)
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
    // -----------------------------
    // Config apply
    // -----------------------------
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

        _pptStart = v_e.ppt_start;
        _pptExit  = v_e.ppt_exit;
        _pptNext  = v_e.ppt_next;
        _pptPrev  = v_e.ppt_prev;
        _pptBlack = v_e.ppt_black;
        _pptLaser = v_e.ppt_laser;

        _engine.setHardClickLock(_hardClickLock);
        _engine.setDPI(_dpiLevel);
        _engine.setZeroSnapTh(_zeroSnapTh);

        Serial.printf("[E10] apply ok: dpi=%d hard=%d accelTh=%.2f wheelTh=%.1f step=%d flick=%.1f cd=%u damp=%.2f idleTh=%.2f idleHold=%u alpha=%.4f snap=%.2f\n",
                      _dpiLevel, _hardClickLock ? 1 : 0, _accelTh, _wheelThDeg, _wheelStepMax,
                      _gestureFlickDeg, (unsigned)_gestureCooldownMs, _scrollCursorDamp,
                      _idleGyroThDeg, (unsigned)_idleHoldMs, _biasTrackAlpha, _zeroSnapTh);

        return true;
    }

    // -----------------------------
    // Keyboard send (policy: modifier mask)
    // -----------------------------
    void tapKeyUsage(uint8_t p_usage, uint16_t p_ms = 12) {
        if (p_usage == 0) return;
        _keyboard.keyPress(p_usage);
        vTaskDelay(pdMS_TO_TICKS(p_ms));
        _keyboard.keyRelease(p_usage);
    }

    void tapComboUsage(uint8_t p_modMask, uint8_t p_usage, uint16_t p_ms = 18) {
        if (p_usage == 0) return;

        // ✅ 정책: W10 mods mask 그대로 modifier byte로 전송 (가장 안정)
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

        // [튜닝 TIP] flick_deg ↑ = 둔감 / ↓ = 민감
        if (p_gzDegPerSec > _gestureFlickDeg) { sendPptKey(_pptPrev); s_lastFlick = millis(); }
        else if (p_gzDegPerSec < -_gestureFlickDeg) { sendPptKey(_pptNext); s_lastFlick = millis(); }
    }

    // -----------------------------
    // Mouse send
    // MouseDevice::mouseMove(x,y,scrollX,scrollY)
    // 기본은 scrollX에 wheel 사용
    // -----------------------------
    static void mouseSend(MouseDevice& p_ms, int8_t p_dx, int8_t p_dy, int8_t p_wheel) {
        p_ms.mouseMove(p_dx, p_dy, p_wheel, 0);
    }

    // -----------------------------
    // Gyro calibration (boot)
    // -----------------------------
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

    // -----------------------------
    // MPU recover
    // -----------------------------
    bool recoverMpu() {
        _errMpuRecover++;

        // I2C reset
        Wire.end();
        delay(10);
        Wire.begin(4, 5);
        Wire.setClock(400000);

        delay(10);

        if (!_mpu.begin()) {
            Serial.println("[E10] recoverMpu: begin failed");
            return false;
        }

        _mpu.setGyroRange(MPU6050_RANGE_250_DEG);
        _mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
        _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

        Serial.println("[E10] recoverMpu: OK");
        _mpuFailStreak = 0;
        _degraded = false;
        return true;
    }

    // -----------------------------
    // Lock-free buffer publish
    // -----------------------------
    void publishPacket(int16_t p_dx, int16_t p_dy, int16_t p_wheel, uint8_t p_btnMask) {
        uint8_t v_w = _idxW;
        _buf[v_w].dx = p_dx;
        _buf[v_w].dy = p_dy;
        _buf[v_w].wheel = p_wheel;
        _buf[v_w].btn_mask = p_btnMask;
        _buf[v_w].valid = true;

        // swap indexes atomically (짧은 크리티컬)
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

    // -----------------------------
    // Tasks
    // -----------------------------
    static void sensorTask(void* p_pv) {
        CL_E10_EliteAirMouse* v_m = (CL_E10_EliteAirMouse*)p_pv;

        TickType_t v_lastWake = xTaskGetTickCount();
        unsigned long v_lastUs = micros();
        unsigned long v_btnDownMs = 0;

        // idle tracking
        uint32_t v_idleStart = 0;

        if (!v_m->_degraded && !v_m->_gyroCalibDone) v_m->runGyroCalibration();

        bool v_prevConn = false;

        for (;;) {
            // connection change count
            bool v_conn = v_m->_hid.isConnected();
            if (v_conn != v_prevConn) {
                if (v_conn) v_m->_reconnectCount++;
                v_prevConn = v_conn;
            }

            sensors_event_t v_a, v_g, v_temp;

            if (v_m->_degraded) {
                // degraded: send 0 movement
                v_m->publishPacket(0, 0, 0, 0);
                vTaskDelayUntil(&v_lastWake, pdMS_TO_TICKS(8));
                continue;
            }

            // read sensor
            v_m->_mpu.getEvent(&v_a, &v_g, &v_temp);
            v_m->_tempC = v_temp.temperature;

            // NOTE: Adafruit getEvent는 실패 반환을 안 줌 (라이브러리 특성)
            // 실제 실패 감지 필요 시 I2C 예외/센서 값 sanity 체크로 보강 가능
            // 여기서는 "값 sanity"로 연속 실패를 추정한다.
            bool v_sane = isfinite(v_g.gyro.x) && isfinite(v_g.gyro.y) && isfinite(v_g.gyro.z);
            if (!v_sane) {
                v_m->_errMpuRead++;
                v_m->_mpuFailStreak++;
                if (v_m->_mpuFailStreak >= s_mpuFailStreakTh) {
                    if (!v_m->recoverMpu()) {
                        v_m->_degraded = true;
                    }
                }
                vTaskDelayUntil(&v_lastWake, pdMS_TO_TICKS(8));
                continue;
            }
            v_m->_mpuFailStreak = 0;

            // dt
            const unsigned long v_nowUs = micros();
            const float v_dt = (v_nowUs - v_lastUs) / 1000000.0f;
            v_lastUs = v_nowUs;

            const float v_dtMs = v_dt * 1000.0f;
            v_m->_dtAvgMs = v_m->_dtAvgMs * 0.98f + v_dtMs * 0.02f;

            const bool v_scrollMode = (digitalRead(s_btnScroll) == LOW);

            // mode button
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

            // gyro deg/s (bias applied)
            const float v_gx_raw = (v_g.gyro.x * RAD_TO_DEG);
            const float v_gy_raw = (v_g.gyro.y * RAD_TO_DEG);
            const float v_gz_raw = (v_g.gyro.z * RAD_TO_DEG);

            float v_gx = v_gx_raw - v_m->_gyroBiasX;
            float v_gy = v_gy_raw - v_m->_gyroBiasY;
            float v_gz = v_gz_raw - v_m->_gyroBiasZ;

            // orientation update
            v_m->_engine.updateOrientation(v_a.acceleration.y, v_a.acceleration.z, v_gx, v_dt);

            // engine process (rawX= -gz, rawY= -gx)
            int v_tx = 0, v_ty = 0;
            v_m->_engine.process(-v_gz, -v_gx, v_tx, v_ty);

            // gesture
            if (v_m->_isPptMode && !v_scrollMode) v_m->processGesturesDeg(v_gz);

            // click
            const bool v_leftClick = (digitalRead(s_btnL) == LOW);
            if (v_leftClick) v_m->_engine.notifyClick();

            // -----------------------------
            // Drift / idle bias tracking
            // - 조건: gyro(|x|,|y|,|z|) 모두 idle_th 이하면 정지 판정 유지
            // - idleHoldMs 넘으면 bias를 raw값으로 천천히 끌어감(EMA)
            // -----------------------------
            const float v_absMax = max(max(fabsf(v_gx), fabsf(v_gy)), fabsf(v_gz));
            if (v_absMax < v_m->_idleGyroThDeg) {
                if (v_idleStart == 0) v_idleStart = millis();
                if (millis() - v_idleStart >= v_m->_idleHoldMs) {
                    // bias <- bias*(1-a) + raw*a
                    float a = v_m->_biasTrackAlpha;
                    v_m->_gyroBiasX = v_m->_gyroBiasX * (1.0f - a) + v_gx_raw * a;
                    v_m->_gyroBiasY = v_m->_gyroBiasY * (1.0f - a) + v_gy_raw * a;
                    v_m->_gyroBiasZ = v_m->_gyroBiasZ * (1.0f - a) + v_gz_raw * a;
                }
            } else {
                v_idleStart = 0;
            }

            // DPI based scale + accel
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

            // scroll
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
                // [튜닝 TIP] scroll_cursor_damp ↓ = 스크롤 중 커서 더 고정(안 흔들림)
                v_fx *= v_m->_scrollCursorDamp;
                v_fy *= v_m->_scrollCursorDamp;
            }

            // publish packet (lock-free)
            uint8_t v_btn = v_leftClick ? s_mouseBtnLeft : 0;
            v_m->publishPacket((int16_t)v_fx, (int16_t)v_fy, (int16_t)v_wheel, v_btn);

            // rough overrun indicator
            TickType_t v_before = xTaskGetTickCount();
            vTaskDelayUntil(&v_lastWake, pdMS_TO_TICKS(8));
            TickType_t v_after = xTaskGetTickCount();
            if ((v_after - v_before) == 0) v_m->_errTaskOverrun++;
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

// static init
TaskHandle_t CL_E10_EliteAirMouse::s_taskSensor = nullptr;
TaskHandle_t CL_E10_EliteAirMouse::s_taskComm   = nullptr;
