// ======================================================
// File: src/v010/E10_EliteAirMouse_013.h
// - W10 Apply 콜백을 main에서 없애기 위한 정적 래퍼 제공
// - 나머지 로직은 012 기반 유지
// ======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : E10_EliteAirMouse_013.h
 * 모듈약어 : E10
 * 모듈명 : ESP32-S3 기반 에어마우스 + 프리젠터 (Composite HID, Config DI/공유, W10 Apply 래퍼)
 * ------------------------------------------------------
 * 기능 요약
 *  - MPU6050 기반 에어마우스(자이로) + BLE Composite HID(Mouse+Keyboard)
 *  - Gyro 오프셋 자동 캘리브레이션(부팅 후 1초 평균, 움직임 큰 샘플 제외)
 *  - 스크롤 전용 버튼(BTN_SCROLL)로 스크롤 모드 분리
 *  - Click-Lock 완전고정 옵션 지원(150ms outX/outY=0)  *M10_MotionProc_010.h*
 *  - C10_Config_010.h 외부 주입(공유)로 W10(Web)과 동일 config.json 사용
 *  - W10 저장 후 즉시 반영을 위한 정적 Apply 래퍼 제공
 *  - FreeRTOS 듀얼 코어 태스크 분산 (Sensor: Core 1, Comm: Core 0)
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
#include <string.h>

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#include <BleCompositeHID.h>
#include <KeyboardDevice.h>
#include <MouseDevice.h>
#include <KeyboardHIDCodes.h>

#include "M10_MotionProc_010.h"
#include "C10_Config_010.h"

#ifndef E10_HAS_JOYSTICK
#define E10_HAS_JOYSTICK 0
#endif

class CL_E10_EliteAirMouse {
  private:
    Adafruit_MPU6050 _mpu;

    BleCompositeHID _hid;
    KeyboardDevice  _keyboard;
    MouseDevice     _mouse;

    CL_M10_AdvancedMotionProcessor _engine;

    CL_C10_Config* _cfg = nullptr;

    static constexpr int G_E10_BTN_L      = 12;
    static constexpr int G_E10_BTN_MODE   = 13;
    static constexpr int G_E10_BTN_SCROLL = 14;

    volatile bool _isPPTMode = false;

    struct ST_E10_State {
        int  x;
        int  y;
        int  wheel;
        bool updated;
    } _state;

    SemaphoreHandle_t _mutex = nullptr;

    static constexpr uint8_t G_E10_MOUSE_BTN_LEFT = 0x01;

    float _gyroBiasX     = 0.0f;
    float _gyroBiasY     = 0.0f;
    float _gyroBiasZ     = 0.0f;
    bool  _gyroCalibDone = false;

    static constexpr uint32_t G_E10_CALIB_MS           = 1000;
    static constexpr float    G_E10_CALIB_STILL_TH_DEG = 3.0f;

    ST_C10_E10Config_t _rtCfg;

  public:
    CL_E10_EliteAirMouse() : _hid("Elite AirMouse S3", "ProMaker", 100) {
        memset(&_state, 0, sizeof(_state));
        memset(&_rtCfg, 0, sizeof(_rtCfg));
        _state.updated = false;
    }

    // ✅ W10에서 콜백으로 바로 사용 (main에 함수 만들 필요 없음)
    static bool E10_W10Apply(void* p_ctx) {
        CL_E10_EliteAirMouse* v_e = (CL_E10_EliteAirMouse*)p_ctx;
        if (v_e == nullptr) return false;
        return v_e->reloadConfig();
    }

    bool begin(CL_C10_Config* p_cfg) {
        Serial.begin(115200);

        _cfg = p_cfg;
        if (_cfg == nullptr) {
            Serial.println("[E10] begin failed: cfg is null");
            return false;
        }

        (void)_cfg->begin(true);
        (void)_cfg->loadE10(_rtCfg);

        Wire.begin(4, 5);
        Wire.setClock(400000);

        if (!_mpu.begin()) {
            Serial.println("[E10] Failed to find MPU6050 chip");
            return false;
        }

        _mpu.setGyroRange(MPU6050_RANGE_250_DEG);
        _mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
        _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

        pinMode(G_E10_BTN_L, INPUT_PULLUP);
        pinMode(G_E10_BTN_MODE, INPUT_PULLUP);
        pinMode(G_E10_BTN_SCROLL, INPUT_PULLUP);

        _mutex = xSemaphoreCreateMutex();

        applyRuntimeConfig();

        _hid.addDevice(&_keyboard);
        _hid.addDevice(&_mouse);
        _hid.begin();

        xTaskCreatePinnedToCore(sensorTask, "E10_Sensor", 8192, this, 3, nullptr, 1);
        xTaskCreatePinnedToCore(commTask,   "E10_Comm",   4096, this, 2, nullptr, 0);

        Serial.println("[E10] begin ok");
        return true;
    }

    CL_C10_Config* getConfig() { return _cfg; }

    bool reloadConfig() {
        if (_cfg == nullptr) return false;

        ST_C10_E10Config_t v_new;
        memset(&v_new, 0, sizeof(v_new));
        (void)_cfg->loadE10(v_new);

        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            _rtCfg = v_new;
            xSemaphoreGive(_mutex);
        } else {
            return false;
        }

        applyRuntimeConfig();
        return true;
    }

  private:
    void applyRuntimeConfig() {
        _engine.setHardClickLock(_rtCfg.hard_click_lock);
        _engine.setDPI(_rtCfg.dpi_level);

        Serial.printf("[E10] cfg: dpi=%u hardClickLock=%d wheelTh=%.1f wheelMax=%d flick=%.1f cooldown=%u damp=%.2f\n",
                      (unsigned)_rtCfg.dpi_level,
                      _rtCfg.hard_click_lock ? 1 : 0,
                      _rtCfg.wheel_threshold_deg,
                      (int)_rtCfg.wheel_step_max,
                      _rtCfg.gesture_flick_deg,
                      (unsigned)_rtCfg.gesture_cooldown_ms,
                      _rtCfg.scroll_cursor_damp);
    }

    void togglePptMode() { _isPPTMode = !_isPPTMode; }

    void cycleDpi() {
        uint8_t v_next = _rtCfg.dpi_level;
        v_next++;
        if (v_next > 3) v_next = 1;

        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            _rtCfg.dpi_level = v_next;
            xSemaphoreGive(_mutex);
        }
        _engine.setDPI(v_next);
    }

    void tapKey(uint8_t p_key, uint16_t p_ms = 12) {
        _keyboard.keyPress(p_key);
        vTaskDelay(pdMS_TO_TICKS(p_ms));
        _keyboard.keyRelease(p_key);
    }

    void tapCombo(uint8_t p_modKey, uint8_t p_key, uint16_t p_ms = 18) {
        _keyboard.keyPress(p_modKey);
        _keyboard.keyPress(p_key);
        vTaskDelay(pdMS_TO_TICKS(p_ms));
        _keyboard.keyRelease(p_key);
        _keyboard.keyRelease(p_modKey);
    }

    void sendPPTCommand(const char* p_label) {
        if (!_hid.isConnected()) return;

        if (strcmp(p_label, "START") == 0) {
            tapCombo(KEY_LEFTSHIFT, KEY_F5, 25);
        } else if (strcmp(p_label, "EXIT") == 0) {
            tapKey(KEY_ESC);
        } else if (strcmp(p_label, "NEXT") == 0) {
            tapKey(KEY_PAGEDOWN);
        } else if (strcmp(p_label, "PREV") == 0) {
            tapKey(KEY_PAGEUP);
        } else if (strcmp(p_label, "BLACK") == 0) {
            tapKey(KEY_B);
        } else if (strcmp(p_label, "LASER") == 0) {
            tapCombo(KEY_LEFTCTRL, KEY_L, 20);
        }
    }

    void processGesturesDeg(float p_gzDegPerSec, float p_flickDeg, uint16_t p_cooldownMs) {
        static unsigned long s_lastFlickMs = 0;
        if (millis() - s_lastFlickMs < p_cooldownMs) return;

        if (p_gzDegPerSec > p_flickDeg) {
            sendPPTCommand("PREV");
            s_lastFlickMs = millis();
        } else if (p_gzDegPerSec < -p_flickDeg) {
            sendPPTCommand("NEXT");
            s_lastFlickMs = millis();
        }
    }

    static void mouseSend(MouseDevice& p_ms, int8_t p_dx, int8_t p_dy, int8_t p_wheel) {
        p_ms.mouseMove(p_dx, p_dy, p_wheel, 0);
    }

    void runGyroCalibration() {
        const uint32_t v_t0  = millis();
        uint32_t       v_cnt = 0;

        double v_sumX = 0.0, v_sumY = 0.0, v_sumZ = 0.0;

        while (millis() - v_t0 < G_E10_CALIB_MS) {
            sensors_event_t v_a, v_g, v_temp;
            _mpu.getEvent(&v_a, &v_g, &v_temp);

            const float v_gx = v_g.gyro.x * RAD_TO_DEG;
            const float v_gy = v_g.gyro.y * RAD_TO_DEG;
            const float v_gz = v_g.gyro.z * RAD_TO_DEG;

            const float v_absMax = max(max(fabsf(v_gx), fabsf(v_gy)), fabsf(v_gz));
            if (v_absMax < G_E10_CALIB_STILL_TH_DEG) {
                v_sumX += v_gx;
                v_sumY += v_gy;
                v_sumZ += v_gz;
                v_cnt++;
            }
            vTaskDelay(pdMS_TO_TICKS(5));
        }

        if (v_cnt > 0) {
            _gyroBiasX = (float)(v_sumX / (double)v_cnt);
            _gyroBiasY = (float)(v_sumY / (double)v_cnt);
            _gyroBiasZ = (float)(v_sumZ / (double)v_cnt);
        }

        _gyroCalibDone = true;

        Serial.printf("[E10] Gyro calib done: biasX=%.3f biasY=%.3f biasZ=%.3f (deg/s), samples=%u\n",
                      _gyroBiasX, _gyroBiasY, _gyroBiasZ, (unsigned)v_cnt);
    }

    static void sensorTask(void* p_pv) {
        CL_E10_EliteAirMouse* v_m = (CL_E10_EliteAirMouse*)p_pv;

        TickType_t    v_lastWake  = xTaskGetTickCount();
        unsigned long v_lastUs    = micros();
        unsigned long v_btnDownMs = 0;

        if (!v_m->_gyroCalibDone) v_m->runGyroCalibration();

        for (;;) {
            ST_C10_E10Config_t v_cfg;
            memset(&v_cfg, 0, sizeof(v_cfg));
            if (xSemaphoreTake(v_m->_mutex, 0) == pdTRUE) {
                v_cfg = v_m->_rtCfg;
                xSemaphoreGive(v_m->_mutex);
            } else {
                v_cfg.dpi_level = 2;
                v_cfg.hard_click_lock = true;
                v_cfg.scale_base[0] = 0.55f; v_cfg.scale_base[1] = 0.75f; v_cfg.scale_base[2] = 1.0f;
                v_cfg.accel_gain[0] = 0.35f; v_cfg.accel_gain[1] = 0.55f; v_cfg.accel_gain[2] = 0.85f;
                v_cfg.accel_threshold = 8.0f;
                v_cfg.wheel_threshold_deg = 90.0f;
                v_cfg.wheel_step_max = 6;
                v_cfg.gesture_flick_deg = 200.0f;
                v_cfg.gesture_cooldown_ms = 600;
                v_cfg.scroll_cursor_damp = 0.25f;
            }

            sensors_event_t v_a, v_g, v_temp;
            v_m->_mpu.getEvent(&v_a, &v_g, &v_temp);

            const unsigned long v_nowUs = micros();
            const float         v_dt    = (v_nowUs - v_lastUs) / 1000000.0f;
            v_lastUs                    = v_nowUs;

            const bool v_scrollMode = (digitalRead(G_E10_BTN_SCROLL) == LOW);

            if (digitalRead(G_E10_BTN_MODE) == LOW) {
                if (v_btnDownMs == 0) v_btnDownMs = millis();
            } else {
                if (v_btnDownMs > 0) {
                    const unsigned long v_hold = millis() - v_btnDownMs;
                    if (v_hold > 1000) v_m->togglePptMode();
                    else v_m->cycleDpi();
                    v_btnDownMs = 0;
                }
            }

            float v_gx = (v_g.gyro.x * RAD_TO_DEG) - v_m->_gyroBiasX;
            float v_gy = (v_g.gyro.y * RAD_TO_DEG) - v_m->_gyroBiasY;
            float v_gz = (v_g.gyro.z * RAD_TO_DEG) - v_m->_gyroBiasZ;

            v_m->_engine.updateOrientation(v_a.acceleration.y, v_a.acceleration.z, v_gx, v_dt);

            int v_tx = 0, v_ty = 0;
            v_m->_engine.process(-v_gz, -v_gx, v_tx, v_ty);

            if (v_m->_isPPTMode && !v_scrollMode) {
                v_m->processGesturesDeg(v_gz, v_cfg.gesture_flick_deg, v_cfg.gesture_cooldown_ms);
            }

            const bool v_leftClick = (digitalRead(G_E10_BTN_L) == LOW);
            if (v_leftClick) v_m->_engine.notifyClick();

            const uint8_t v_dpiIdx = (v_cfg.dpi_level < 1) ? 1 : ((v_cfg.dpi_level > 3) ? 3 : v_cfg.dpi_level);
            const uint8_t v_i = (uint8_t)(v_dpiIdx - 1);

            float v_base = v_cfg.scale_base[v_i];
            float v_accg = v_cfg.accel_gain[v_i];

            const float v_mag = sqrtf((float)v_tx * (float)v_tx + (float)v_ty * (float)v_ty);
            float v_acc = 1.0f;
            if (v_mag > v_cfg.accel_threshold) {
                const float v_ex = (v_mag - v_cfg.accel_threshold);
                v_acc = 1.0f + (v_accg * (v_ex / (v_ex + 18.0f)));
            }

            float v_fx = (float)v_tx * v_base * v_acc;
            float v_fy = (float)v_ty * v_base * v_acc;

            int v_wheel = 0;
            if (v_scrollMode) {
                if (v_gy > v_cfg.wheel_threshold_deg) {
                    float v_norm = (v_gy - v_cfg.wheel_threshold_deg) / 120.0f;
                    if (v_norm > 1.0f) v_norm = 1.0f;
                    v_wheel = (int)(1 + (v_norm * ((int)v_cfg.wheel_step_max - 1)));
                } else if (v_gy < -v_cfg.wheel_threshold_deg) {
                    float v_norm = (-v_gy - v_cfg.wheel_threshold_deg) / 120.0f;
                    if (v_norm > 1.0f) v_norm = 1.0f;
                    v_wheel = -(int)(1 + (v_norm * ((int)v_cfg.wheel_step_max - 1)));
                }

                v_fx *= v_cfg.scroll_cursor_damp;
                v_fy *= v_cfg.scroll_cursor_damp;
            }

            if (xSemaphoreTake(v_m->_mutex, 0) == pdTRUE) {
                v_m->_state.x       = (int)v_fx;
                v_m->_state.y       = (int)v_fy;
                v_m->_state.wheel   = v_wheel;
                v_m->_state.updated = true;
                xSemaphoreGive(v_m->_mutex);
            }

            if (v_m->_hid.isConnected()) {
                if (v_leftClick) v_m->_mouse.mousePress(G_E10_MOUSE_BTN_LEFT);
                else v_m->_mouse.mouseRelease(G_E10_MOUSE_BTN_LEFT);
            }

            vTaskDelayUntil(&v_lastWake, pdMS_TO_TICKS(8));
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

                    mouseSend(v_m->_mouse, v_dx, v_dy, v_wh);
                    v_m->_state.updated = false;
                }
                xSemaphoreGive(v_m->_mutex);
            }
            vTaskDelay(pdMS_TO_TICKS(7));
        }
    }
};
