#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : E10_EliteAirMouse_015.h
 * 모듈약어 : E10
 * 모듈명 : ESP32-S3 기반 에어마우스 + 프리젠터 (Apply from C10)
 * ------------------------------------------------------
 * 기능 요약
 *  - MPU6050 기반 에어마우스 + Composite HID(Mouse+Keyboard)
 *  - Gyro 오프셋 자동 캘리브레이션(부팅 1초)
 *  - BTN_SCROLL 전용 버튼 분리
 *  - Click-Lock Hard 옵션
 *  - ✅ C10 설정값을 런타임 반영(E10_W10Apply)
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
#include <KeyboardHIDCodes.h>

#include "M10_MotionProc_015.h"
#include "C10_Config_015.h"

// ------------------------------------------------------
// [옵션] 조이스틱 유무 (v0.1.0: 스텁만)
// ------------------------------------------------------
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

    // GPIO
    static constexpr int G_E10_BTN_L      = 12;
    static constexpr int G_E10_BTN_MODE   = 13;
    static constexpr int G_E10_BTN_SCROLL = 14;

    // runtime state
    volatile bool _isPPTMode = false;

    struct ST_E10_State {
        int  x;
        int  y;
        int  wheel;
        bool updated;
    } _state;

    SemaphoreHandle_t _mutex = nullptr;

    static constexpr uint8_t G_E10_MOUSE_BTN_LEFT = 0x01;

    // gyro bias
    float _gyroBiasX     = 0.0f;
    float _gyroBiasY     = 0.0f;
    float _gyroBiasZ     = 0.0f;
    bool  _gyroCalibDone = false;

    static constexpr uint32_t G_E10_CALIB_MS = 1000;
    static constexpr float    G_E10_CALIB_STILL_TH_DEG = 3.0f;

    // ✅ Apply 대상 튜닝값(기본값은 C10 defaults에서 채움)
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

    // ppt keys
    ST_C10_PptKey_t _pptStart = {0,0};
    ST_C10_PptKey_t _pptExit  = {0,0};
    ST_C10_PptKey_t _pptNext  = {0,0};
    ST_C10_PptKey_t _pptPrev  = {0,0};
    ST_C10_PptKey_t _pptBlack = {0,0};
    ST_C10_PptKey_t _pptLaser = {0,0};

  public:
    CL_E10_EliteAirMouse() : _hid("Elite AirMouse S3", "ProMaker", 100) {
        _state = {0,0,0,false};
    }

    void begin(CL_C10_Config* p_cfg) {
        _cfg = p_cfg;

        Serial.begin(115200);

        Wire.begin(4, 5);
        Wire.setClock(400000);

        if (!_mpu.begin()) {
            Serial.println("Failed to find MPU6050 chip");
            for (;;) delay(10);
        }

        _mpu.setGyroRange(MPU6050_RANGE_250_DEG);
        _mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
        _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

        pinMode(G_E10_BTN_L, INPUT_PULLUP);
        pinMode(G_E10_BTN_MODE, INPUT_PULLUP);
        pinMode(G_E10_BTN_SCROLL, INPUT_PULLUP);

        _mutex = xSemaphoreCreateMutex();

        // ✅ 부팅 시 1회 Apply (C10값 -> 런타임)
        (void)applyFromConfig();

        _hid.addDevice(&_keyboard);
        _hid.addDevice(&_mouse);
        _hid.begin();

        xTaskCreatePinnedToCore(sensorTask, "E10_Sensor", 8192, this, 3, nullptr, 1);
        xTaskCreatePinnedToCore(commTask,   "E10_Comm",   4096, this, 2, nullptr, 0);
    }

    // ------------------------------------------------------
    // W10에서 콜백으로 호출할 Apply 함수
    // - main.cpp에서: g_w10.begin(&g_c10, CL_E10_EliteAirMouse::E10_W10Apply, &g_e10);
    // ------------------------------------------------------
    static bool E10_W10Apply(void* p_ctx) {
        if (p_ctx == nullptr) return false;
        CL_E10_EliteAirMouse* v_e = (CL_E10_EliteAirMouse*)p_ctx;
        return v_e->applyFromConfig();
    }

  private:
    bool applyFromConfig() {
        if (_cfg == nullptr) return false;

        ST_C10_E10Config_t v_cfg;
        memset(&v_cfg, 0, sizeof(v_cfg));
        (void)_cfg->loadE10(v_cfg);

        // ✅ atomic-ish: 센서태스크와 충돌 최소화를 위해 mutex가 있으면 잡고 반영
        if (_mutex != nullptr) (void)xSemaphoreTake(_mutex, portMAX_DELAY);

        _dpiLevel = (int)v_cfg.dpi_level;
        _hardClickLock = v_cfg.hard_click_lock;

        _scaleBase[0] = v_cfg.scale_base[0];
        _scaleBase[1] = v_cfg.scale_base[1];
        _scaleBase[2] = v_cfg.scale_base[2];

        _accelGain[0] = v_cfg.accel_gain[0];
        _accelGain[1] = v_cfg.accel_gain[1];
        _accelGain[2] = v_cfg.accel_gain[2];

        _accelTh = v_cfg.accel_threshold;

        _wheelThDeg = v_cfg.wheel_threshold_deg;
        _wheelStepMax = (int)v_cfg.wheel_step_max;

        _gestureFlickDeg = v_cfg.gesture_flick_deg;
        _gestureCooldownMs = v_cfg.gesture_cooldown_ms;

        _scrollCursorDamp = v_cfg.scroll_cursor_damp;

        _pptStart = v_cfg.ppt_start;
        _pptExit  = v_cfg.ppt_exit;
        _pptNext  = v_cfg.ppt_next;
        _pptPrev  = v_cfg.ppt_prev;
        _pptBlack = v_cfg.ppt_black;
        _pptLaser = v_cfg.ppt_laser;

        // 엔진 반영
        _engine.setHardClickLock(_hardClickLock);
        _engine.setDPI(_dpiLevel);

        if (_mutex != nullptr) xSemaphoreGive(_mutex);

        Serial.printf("[E10] apply ok: dpi=%d hard=%d accelTh=%.2f wheelTh=%.1f step=%d flick=%.1f cd=%u damp=%.2f\n",
                      _dpiLevel, _hardClickLock ? 1 : 0, _accelTh, _wheelThDeg, _wheelStepMax,
                      _gestureFlickDeg, (unsigned)_gestureCooldownMs, _scrollCursorDamp);

        return true;
    }

    void tapKey(uint8_t p_key, uint16_t p_ms = 12) {
        _keyboard.keyPress(p_key);
        vTaskDelay(pdMS_TO_TICKS(p_ms));
        _keyboard.keyRelease(p_key);
    }

    void tapCombo(uint8_t p_modMask, uint8_t p_key, uint16_t p_ms = 18) {
        // modifier mask는 KeyboardDevice가 “단일 키” press만 지원하는 경우가 있어
        // 여기서는 “LeftCtrl/LeftShift” 같은 키코드로 보내는 방식 대신,
        // CompositeHID 구현이 modifier mask를 어떻게 처리하는지에 따라 달라질 수 있음.
        // 현재 프로젝트는 mod를 keyPress로 누르는 방식이 아니라, W10에서 저장한 mod는
        // “키코드 기반 모디파이어(KEY_LEFTCTRL 등)”로 구성하는 방식을 추천.
        // 그래서: modMask==0이면 key만, 아니면 modMask를 KEY_LEFTCTRL/SHIFT로 매핑.
        //
        // ✅ v0.1.0 마감: modMask는 W10 UI에서 LeftCtrl/LeftShift만 쓴다는 전제.
        uint8_t v_modKey = 0;

        if (p_modMask == 0x01) v_modKey = KEY_LEFTCTRL;
        else if (p_modMask == 0x02) v_modKey = KEY_LEFTSHIFT;
        else if (p_modMask == 0x04) v_modKey = KEY_LEFTALT;
        else if (p_modMask == 0x08) v_modKey = KEY_LEFTMETA;

        if (v_modKey == 0) {
            tapKey(p_key, p_ms);
            return;
        }

        _keyboard.keyPress(v_modKey);
        _keyboard.keyPress(p_key);
        vTaskDelay(pdMS_TO_TICKS(p_ms));
        _keyboard.keyRelease(p_key);
        _keyboard.keyRelease(v_modKey);
    }

    void sendPptKey(const ST_C10_PptKey_t& p_k) {
        if (!_hid.isConnected()) return;
        if (p_k.key == 0) return; // 아직 미설정이면 무시
        if (p_k.mod == 0) tapKey(p_k.key);
        else tapCombo(p_k.mod, p_k.key, 22);
    }

    void processGesturesDeg(float p_gzDegPerSec) {
        static unsigned long s_lastFlick = 0;
        if (millis() - s_lastFlick < _gestureCooldownMs) return;

        if (p_gzDegPerSec > _gestureFlickDeg) {
            sendPptKey(_pptPrev);
            s_lastFlick = millis();
        } else if (p_gzDegPerSec < -_gestureFlickDeg) {
            sendPptKey(_pptNext);
            s_lastFlick = millis();
        }
    }

    static void mouseSend(MouseDevice& p_ms, int8_t p_dx, int8_t p_dy, int8_t p_wheel) {
        // 기본: scrollX에 wheel 사용
        p_ms.mouseMove(p_dx, p_dy, p_wheel, 0);
    }

    void runGyroCalibration() {
        const uint32_t v_t0 = millis();
        uint32_t v_cnt = 0;
        double v_sumX = 0.0, v_sumY = 0.0, v_sumZ = 0.0;

        while (millis() - v_t0 < G_E10_CALIB_MS) {
            sensors_event_t v_a, v_g, v_temp;
            _mpu.getEvent(&v_a, &v_g, &v_temp);

            const float v_gx = v_g.gyro.x * RAD_TO_DEG;
            const float v_gy = v_g.gyro.y * RAD_TO_DEG;
            const float v_gz = v_g.gyro.z * RAD_TO_DEG;

            const float v_absMax = max(max(fabsf(v_gx), fabsf(v_gy)), fabsf(v_gz));
            if (v_absMax < G_E10_CALIB_STILL_TH_DEG) {
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

    static void sensorTask(void* p_pv) {
        CL_E10_EliteAirMouse* v_m = (CL_E10_EliteAirMouse*)p_pv;

        TickType_t v_lastWake = xTaskGetTickCount();
        unsigned long v_lastUs = micros();
        unsigned long v_btnDownMs = 0;

        if (!v_m->_gyroCalibDone) v_m->runGyroCalibration();

        for (;;) {
            sensors_event_t v_a, v_g, v_temp;
            v_m->_mpu.getEvent(&v_a, &v_g, &v_temp);

            const unsigned long v_nowUs = micros();
            const float v_dt = (v_nowUs - v_lastUs) / 1000000.0f;
            v_lastUs = v_nowUs;

            const bool v_scrollMode = (digitalRead(G_E10_BTN_SCROLL) == LOW);

            // BTN_MODE short/long
            if (digitalRead(G_E10_BTN_MODE) == LOW) {
                if (v_btnDownMs == 0) v_btnDownMs = millis();
            } else {
                if (v_btnDownMs > 0) {
                    const unsigned long v_hold = millis() - v_btnDownMs;
                    if (v_hold > 1000) v_m->_isPPTMode = !v_m->_isPPTMode;
                    else {
                        // DPI 사이클은 “로컬만” 변경 후 엔진 반영
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

            // orientation
            v_m->_engine.updateOrientation(v_a.acceleration.y, v_a.acceleration.z, v_gx, v_dt);

            int v_tx = 0, v_ty = 0;
            const float v_rawX = -v_gz;
            const float v_rawY = -v_gx;
            v_m->_engine.process(v_rawX, v_rawY, v_tx, v_ty);

            // gesture (no scroll)
            if (v_m->_isPPTMode && !v_scrollMode) v_m->processGesturesDeg(v_gz);

            // click
            const bool v_leftClick = (digitalRead(G_E10_BTN_L) == LOW);
            if (v_leftClick) v_m->_engine.notifyClick();

            // dpi scale + accel
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

                v_fx *= v_m->_scrollCursorDamp;
                v_fy *= v_m->_scrollCursorDamp;
            }

            if (xSemaphoreTake(v_m->_mutex, 0) == pdTRUE) {
                v_m->_state.x = (int)v_fx;
                v_m->_state.y = (int)v_fy;
                v_m->_state.wheel = v_wheel;
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

