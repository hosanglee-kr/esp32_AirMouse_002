// ======================================================
// File: src/v001/E10_EliteAirMouse_011.h
// ======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : E10_EliteAirMouse_011.h
 * 모듈약어 : E10
 * 모듈명 : ESP32-S3 기반 에어마우스 + 프리젠터 (Composite HID, Config 적용)
 * ------------------------------------------------------
 * 기능 요약
 *  - MPU6050 기반 에어마우스(자이로) + BLE Composite HID(Mouse+Keyboard)
 *  - Gyro 오프셋 자동 캘리브레이션(부팅 후 1초 평균, 움직임 큰 샘플 제외)
 *  - 스크롤 전용 버튼(BTN_SCROLL)로 스크롤 모드 분리(UX 충돌 제거)
 *  - Click-Lock 완전고정 옵션 지원(150ms outX/outY=0)  *M10_MotionProc_010.h*
 *  - LittleFS /config.json 설정 로드 후 런타임 적용(C10_Config_010.h)
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

// ------------------------------------------------------
// [옵션] 조이스틱 유무 (v0.1.0~: 스텁만, 기능 구현은 최후순위)
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

    CL_C10_Config _cfg;

    // GPIO (예시)
    static constexpr int G_E10_BTN_L      = 12;
    static constexpr int G_E10_BTN_MODE   = 13;
    static constexpr int G_E10_BTN_SCROLL = 14;

    // 상태
    volatile bool _isPPTMode = false;

    struct ST_E10_State {
        int  x;
        int  y;
        int  wheel;
        bool updated;
    } _state;

    SemaphoreHandle_t _mutex = nullptr;

    static constexpr uint8_t G_E10_MOUSE_BTN_LEFT = 0x01;

    // Gyro bias (deg/s)
    float _gyroBiasX     = 0.0f;
    float _gyroBiasY     = 0.0f;
    float _gyroBiasZ     = 0.0f;
    bool  _gyroCalibDone = false;

    // 캘리브레이션
    static constexpr uint32_t G_E10_CALIB_MS           = 1000;
    static constexpr float    G_E10_CALIB_STILL_TH_DEG = 3.0f;

    // ------------------------------
    // 런타임 Config(파일에서 로드)
    // ------------------------------
    ST_C10_E10Config_t _rtCfg;

#if (E10_HAS_JOYSTICK == 1)
    static constexpr int G_E10_JOY_X = 1;
    static constexpr int G_E10_JOY_Y = 2;
#endif

  public:
    CL_E10_EliteAirMouse() : _hid("Elite AirMouse S3", "ProMaker", 100) {
        memset(&_state, 0, sizeof(_state));
        memset(&_rtCfg, 0, sizeof(_rtCfg));
        _state.updated = false;
    }

    void begin() {
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

#if (E10_HAS_JOYSTICK == 1)
        pinMode(G_E10_JOY_X, INPUT);
        pinMode(G_E10_JOY_Y, INPUT);
#endif

        _mutex = xSemaphoreCreateMutex();

        // 1) 설정 로드(없으면 기본 생성)
        (void)_cfg.begin(true);
        (void)_cfg.loadE10(_rtCfg);

        // 2) 설정 적용
        applyRuntimeConfig();

        // 3) Composite HID 시작
        _hid.addDevice(&_keyboard);
        _hid.addDevice(&_mouse);
        _hid.begin();

        xTaskCreatePinnedToCore(sensorTask, "E10_Sensor", 8192, this, 3, nullptr, 1);
        xTaskCreatePinnedToCore(commTask,   "E10_Comm",   4096, this, 2, nullptr, 0);
    }

    // (선택) 런타임에 config.json 재적용 (Web UI 붙일 때 사용)
    bool reloadConfig() {
        ST_C10_E10Config_t v_new;
        memset(&v_new, 0, sizeof(v_new));

        if (!_cfg.loadE10(v_new)) {
            // load 실패해도 기본값으로 저장되므로, new를 적용하는 편이 낫다
        }

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
        // 엔진 쪽은 Task에서 쓰므로, 여기서 엔진 설정은 바로 적용
        // hard_click_lock
        _engine.setHardClickLock(_rtCfg.hard_click_lock);

        // dpi_level
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

        // 저장까지는 v0.1.0에선 불필요하지만, v0.2.0 호환 위해 here 제공
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            _rtCfg.dpi_level = v_next;
            xSemaphoreGive(_mutex);
        }
        _engine.setDPI(v_next);

        // 저장을 원하면 아래 한 줄 활성화(버튼 누를 때마다 flash write 발생)
        // (void)_cfg.saveE10(_rtCfg);
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
        static unsigned long v_lastFlickMs = 0;
        if (millis() - v_lastFlickMs < p_cooldownMs) return;

        if (p_gzDegPerSec > p_flickDeg) {
            sendPPTCommand("PREV");
            v_lastFlickMs = millis();
        } else if (p_gzDegPerSec < -p_flickDeg) {
            sendPPTCommand("NEXT");
            v_lastFlickMs = millis();
        }
    }

    // MouseDevice.h 기준: mouseMove(x,y,scrollX,scrollY)
    static void mouseSend(MouseDevice& p_ms, int8_t p_dx, int8_t p_dy, int8_t p_wheel) {
        p_ms.mouseMove(p_dx, p_dy, p_wheel, 0);
        // 세로 스크롤이 반대로 느껴지면:
        // p_ms.mouseMove(p_dx, p_dy, 0, p_wheel);
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
            // 1) 현재 config 스냅샷(락 짧게)
            ST_C10_E10Config_t v_cfg;
            memset(&v_cfg, 0, sizeof(v_cfg));
            if (xSemaphoreTake(v_m->_mutex, 0) == pdTRUE) {
                v_cfg = v_m->_rtCfg;
                xSemaphoreGive(v_m->_mutex);
            } else {
                // 락 실패 시에도 이전 값 대신 최소 디폴트로 동작
                v_cfg.dpi_level = 2;
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

            // BTN_MODE: short=감도 변경, long=PPT 토글
            if (digitalRead(G_E10_BTN_MODE) == LOW) {
                if (v_btnDownMs == 0) v_btnDownMs = millis();
            } else {
                if (v_btnDownMs > 0) {
                    const unsigned long v_hold = millis() - v_btnDownMs;
                    if (v_hold > 1000)
                        v_m->togglePptMode();
                    else
                        v_m->cycleDpi();
                    v_btnDownMs = 0;
                }
            }

            // gyro rad/s -> deg/s + bias 제거
            float v_gx = (v_g.gyro.x * RAD_TO_DEG) - v_m->_gyroBiasX;
            float v_gy = (v_g.gyro.y * RAD_TO_DEG) - v_m->_gyroBiasY;
            float v_gz = (v_g.gyro.z * RAD_TO_DEG) - v_m->_gyroBiasZ;

            // 엔진 orientation 업데이트
            v_m->_engine.updateOrientation(v_a.acceleration.y, v_a.acceleration.z, v_gx, v_dt);

            int v_tx = 0, v_ty = 0;

            // rawX=gyro.z, rawY=gyro.x
            v_m->_engine.process(-v_gz, -v_gx, v_tx, v_ty);

            // PPT 제스처(스크롤 중 차단)
            if (v_m->_isPPTMode && !v_scrollMode) {
                v_m->processGesturesDeg(v_gz, v_cfg.gesture_flick_deg, v_cfg.gesture_cooldown_ms);
            }

            // 클릭 처리
            const bool v_leftClick = (digitalRead(G_E10_BTN_L) == LOW);
            if (v_leftClick) v_m->_engine.notifyClick();

            // DPI 기반 scale + 가속
            const uint8_t v_dpiIdx = (v_cfg.dpi_level < 1) ? 1 : ((v_cfg.dpi_level > 3) ? 3 : v_cfg.dpi_level);
            const uint8_t v_i = (uint8_t)(v_dpiIdx - 1);

            float v_base = v_cfg.scale_base[v_i];
            float v_accg = v_cfg.accel_gain[v_i];

            const float v_mag = sqrtf((float)v_tx * (float)v_tx + (float)v_ty * (float)v_ty);
            float       v_acc = 1.0f;

            if (v_mag > v_cfg.accel_threshold) {
                const float v_ex = (v_mag - v_cfg.accel_threshold);
                v_acc            = 1.0f + (v_accg * (v_ex / (v_ex + 18.0f)));
            }

            float v_fx = (float)v_tx * v_base * v_acc;
            float v_fy = (float)v_ty * v_base * v_acc;

            // 스크롤 모드(BTN_SCROLL)
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

                // 스크롤 중 커서 억제
                v_fx *= v_cfg.scroll_cursor_damp;
                v_fy *= v_cfg.scroll_cursor_damp;
            }

            // 공유 상태 저장
            if (xSemaphoreTake(v_m->_mutex, 0) == pdTRUE) {
                v_m->_state.x       = (int)v_fx;
                v_m->_state.y       = (int)v_fy;
                v_m->_state.wheel   = v_wheel;
                v_m->_state.updated = true;
                xSemaphoreGive(v_m->_mutex);
            }

            // 버튼 상태 즉시 반영
            if (v_m->_hid.isConnected()) {
                if (v_leftClick)
                    v_m->_mouse.mousePress(G_E10_MOUSE_BTN_LEFT);
                else
                    v_m->_mouse.mouseRelease(G_E10_MOUSE_BTN_LEFT);
            }

            vTaskDelayUntil(&v_lastWake, pdMS_TO_TICKS(8)); // 125Hz
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

