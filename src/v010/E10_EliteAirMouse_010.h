#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : E10_EliteAirMouse_010.h
 * 모듈약어 : E10
 * 모듈명 : ESP32-S3 기반 에어마우스 + 프리젠터 (Composite HID, Calib/Scroll/ClickLock)
 * ------------------------------------------------------
 * 기능 요약
 *  - MPU6050 기반 에어마우스(자이로) + BLE Composite HID(Mouse+Keyboard)
 *  - Gyro 오프셋 자동 캘리브레이션(부팅 후 1초 평균, 움직임 큰 샘플 제외)
 *  - 스크롤 전용 버튼(BTN_SCROLL)로 스크롤 모드 분리(UX 충돌 제거)
 *  - Click-Lock 완전고정 옵션 지원(150ms outX/outY=0)  *M10_MotionProc_010.h*
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

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#include <BleCompositeHID.h>
#include <KeyboardDevice.h>
#include <MouseDevice.h>
#include <KeyboardHIDCodes.h>

#include "M10_MotionProc_010.h"

// ------------------------------------------------------
// [옵션] 조이스틱 유무 (v0.1.0: 스텁만, 기능 구현은 최후순위)
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

    // ======================================================
    // GPIO (예시: DevKitC 안전핀, 실제 HW에 맞춰 수정)
    // ======================================================
    static constexpr int G_E10_BTN_L      = 12; // Left click
    static constexpr int G_E10_BTN_MODE   = 13; // Short=DPI, Long=PPT toggle
    static constexpr int G_E10_BTN_SCROLL = 14; // Scroll 전용(누르는 동안만 스크롤)

    // ======================================================
    // 상태
    // ======================================================
    volatile bool _isPPTMode = false;
    int           _dpiLevel  = 2; // 1~3

    struct ST_E10_State {
        int  x;
        int  y;
        int  wheel;
        bool updated;
    } _state;

    SemaphoreHandle_t _mutex = nullptr;

    static constexpr uint8_t G_E10_MOUSE_BTN_LEFT = 0x01;

    // ======================================================
    // Gyro bias (deg/s) : 캘리브레이션 결과
    // ======================================================
    float _gyroBiasX     = 0.0f;
    float _gyroBiasY     = 0.0f;
    float _gyroBiasZ     = 0.0f;
    bool  _gyroCalibDone = false;

    // ======================================================
    // [튜닝] 마우스 스케일/가속
    // ======================================================
    static constexpr float G_E10_SCALE_BASE_DPI1 = 0.55f;
    static constexpr float G_E10_SCALE_BASE_DPI2 = 0.75f;
    static constexpr float G_E10_SCALE_BASE_DPI3 = 1.00f;

    static constexpr float G_E10_ACCEL_GAIN_DPI1 = 0.35f;
    static constexpr float G_E10_ACCEL_GAIN_DPI2 = 0.55f;
    static constexpr float G_E10_ACCEL_GAIN_DPI3 = 0.85f;

    static constexpr float G_E10_ACCEL_TH = 8.0f;

    // ======================================================
    // [튜닝] 스크롤(휠)
    // ======================================================
    static constexpr float G_E10_WHEEL_TH_DEG   = 90.0f;
    static constexpr int   G_E10_WHEEL_STEP_MAX = 6;

    // ======================================================
    // 캘리브레이션
    // ======================================================
    static constexpr uint32_t G_E10_CALIB_MS           = 1000;
    static constexpr float    G_E10_CALIB_STILL_TH_DEG = 3.0f; // 움직임 큰 샘플 제외 임계(deg/s)

    // ======================================================
    // [옵션] PSP1000 Joystick (후순위 구현 예정)
    // ======================================================
#if (E10_HAS_JOYSTICK == 1)
    // ⚠️ 실제 보드에서 "ADC 가능한 핀"인지 반드시 확인 필요
    static constexpr int G_E10_JOY_X = 1; // ADC
    static constexpr int G_E10_JOY_Y = 2; // ADC

    // [튜닝 자리] (후순위 구현에서 사용)
    static constexpr int   G_E10_JOY_DEADZONE = 80;
    static constexpr float G_E10_JOY_GAIN     = 1.0f;
#endif

  public:
    CL_E10_EliteAirMouse() : _hid("Elite AirMouse S3", "ProMaker", 100) { _state = {0, 0, 0, false}; }

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
        // Joystick ADC 핀은 보통 pinMode 불필요(analogRead에서 처리)
        pinMode(G_E10_JOY_X, INPUT);
        pinMode(G_E10_JOY_Y, INPUT);
#endif

        _mutex = xSemaphoreCreateMutex();

        // ✅ Click-Lock 완전 고정 옵션 ON (원하면 false로)
        _engine.setHardClickLock(true);

        // ✅ 초기 DPI 적용 (축소/누락 방지)
        _engine.setDPI(_dpiLevel);

        // Composite HID: device 등록 후 begin()
        _hid.addDevice(&_keyboard);
        _hid.addDevice(&_mouse);
        _hid.begin();

        xTaskCreatePinnedToCore(sensorTask, "E10_Sensor", 8192, this, 3, nullptr, 1);
        xTaskCreatePinnedToCore(commTask, "E10_Comm", 4096, this, 2, nullptr, 0);
    }

  private:
    void togglePptMode() { _isPPTMode = !_isPPTMode; }

    void cycleDpi() {
        _dpiLevel++;
        if (_dpiLevel > 3) _dpiLevel = 1;
        _engine.setDPI(_dpiLevel);
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

        if (strcmp(p_label, "START") == 0) { // Shift + F5
            tapCombo(KEY_LEFTSHIFT, KEY_F5, 25);
        } else if (strcmp(p_label, "EXIT") == 0) { // ESC
            tapKey(KEY_ESC);
        } else if (strcmp(p_label, "NEXT") == 0) { // PageDown
            tapKey(KEY_PAGEDOWN);
        } else if (strcmp(p_label, "PREV") == 0) { // PageUp
            tapKey(KEY_PAGEUP);
        } else if (strcmp(p_label, "BLACK") == 0) { // 'b'
            tapKey(KEY_B);
        } else if (strcmp(p_label, "LASER") == 0) { // Ctrl + L
            tapCombo(KEY_LEFTCTRL, KEY_L, 20);
        }
    }

    void processGesturesDeg(float p_gzDegPerSec) {
        static unsigned long s_lastFlick = 0;
        if (millis() - s_lastFlick < 600) return;

        // [튜닝 TIP] 200을 ↑=둔감 / ↓=민감
        if (p_gzDegPerSec > 200.0f) {
            sendPPTCommand("PREV");
            s_lastFlick = millis();
        } else if (p_gzDegPerSec < -200.0f) {
            sendPPTCommand("NEXT");
            s_lastFlick = millis();
        }
    }

    // ✅ MouseDevice.h 기준 wheel 전송
    // - mouseMove(x, y, scrollX, scrollY)
    static void mouseSend(MouseDevice& p_ms, int8_t p_dx, int8_t p_dy, int8_t p_wheel) {
        // 기본: scrollX에 wheel 사용
        // 만약 PC에서 스크롤이 “가로/세로 반대”라면 아래 인자만 바꾸면 됨:
        //   p_ms.mouseMove(p_dx, p_dy, 0, p_wheel);
        p_ms.mouseMove(p_dx, p_dy, p_wheel, 0);
    }

    // ✅ Gyro bias 캘리브레이션(약 1초 평균) + 움직임 큰 샘플 제외
    void runGyroCalibration() {
        const uint32_t v_t0  = millis();
        uint32_t       v_cnt = 0;

        double v_sumX = 0.0, v_sumY = 0.0, v_sumZ = 0.0;

        while (millis() - v_t0 < G_E10_CALIB_MS) {
            sensors_event_t v_a, v_g, v_temp;
            _mpu.getEvent(&v_a, &v_g, &v_temp);

            // rad/s -> deg/s
            const float v_gx = v_g.gyro.x * RAD_TO_DEG;
            const float v_gy = v_g.gyro.y * RAD_TO_DEG;
            const float v_gz = v_g.gyro.z * RAD_TO_DEG;

            // ✅ 움직임이 큰 샘플은 캘리브에서 제외(손에 들고 켜는 상황 방지)
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
                      _gyroBiasX,
                      _gyroBiasY,
                      _gyroBiasZ,
                      (unsigned int)v_cnt);
    }

#if (E10_HAS_JOYSTICK == 1)
    // v0.1.0: 스텁(후순위 구현)
    void readJoystickStub(int& p_outDx, int& p_outDy) {
        (void)p_outDx;
        (void)p_outDy;
        // TODO(v0.3.0): analogRead로 센터 캘리브/데드존/가속 적용 후 dx/dy 산출
    }
#endif

    static void sensorTask(void* p_pv) {
        CL_E10_EliteAirMouse* v_m = (CL_E10_EliteAirMouse*)p_pv;

        TickType_t    v_lastWake  = xTaskGetTickCount();
        unsigned long v_lastUs    = micros();
        unsigned long v_btnDownMs = 0;

        // ✅ 부팅 후 1회 캘리브레이션
        if (!v_m->_gyroCalibDone) {
            v_m->runGyroCalibration();
        }

        for (;;) {
            sensors_event_t v_a, v_g, v_temp;
            v_m->_mpu.getEvent(&v_a, &v_g, &v_temp);

            const unsigned long v_nowUs = micros();
            const float         v_dt    = (v_nowUs - v_lastUs) / 1000000.0f;
            v_lastUs                    = v_nowUs;

            // 0) 스크롤 모드(전용 버튼)
            const bool v_scrollMode = (digitalRead(G_E10_BTN_SCROLL) == LOW);

            // 1) BTN_MODE: short=감도 변경, long=PPT 토글
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

            // 2) gyro rad/s -> deg/s + bias 제거
            float v_gx = (v_g.gyro.x * RAD_TO_DEG) - v_m->_gyroBiasX;
            float v_gy = (v_g.gyro.y * RAD_TO_DEG) - v_m->_gyroBiasY;
            float v_gz = (v_g.gyro.z * RAD_TO_DEG) - v_m->_gyroBiasZ;

            // 3) 엔진 orientation 업데이트 (gx 사용)
            v_m->_engine.updateOrientation(v_a.acceleration.y, v_a.acceleration.z, v_gx, v_dt);

            int v_tx = 0, v_ty = 0;

            // rawX=gyro.z, rawY=gyro.x
            const float v_rawX = -v_gz;
            const float v_rawY = -v_gx;

            v_m->_engine.process(v_rawX, v_rawY, v_tx, v_ty);

            // 4) PPT 모드 제스처 (스크롤 중에는 차단)
            if (v_m->_isPPTMode && !v_scrollMode) {
                v_m->processGesturesDeg(v_gz);
            }

            // 5) 클릭 처리
            const bool v_leftClick = (digitalRead(G_E10_BTN_L) == LOW);
            if (v_leftClick) v_m->_engine.notifyClick();

            // 6) DPI 기반 scale + 가속
            float v_base = G_E10_SCALE_BASE_DPI2;
            float v_accg = G_E10_ACCEL_GAIN_DPI2;

            if (v_m->_dpiLevel == 1) {
                v_base = G_E10_SCALE_BASE_DPI1;
                v_accg = G_E10_ACCEL_GAIN_DPI1;
            } else if (v_m->_dpiLevel == 3) {
                v_base = G_E10_SCALE_BASE_DPI3;
                v_accg = G_E10_ACCEL_GAIN_DPI3;
            }

            const float v_mag = sqrtf((float)v_tx * (float)v_tx + (float)v_ty * (float)v_ty);
            float       v_acc = 1.0f;
            if (v_mag > G_E10_ACCEL_TH) {
                const float v_ex = (v_mag - G_E10_ACCEL_TH);
                v_acc            = 1.0f + (v_accg * (v_ex / (v_ex + 18.0f)));
            }

            float v_fx = (float)v_tx * v_base * v_acc;
            float v_fy = (float)v_ty * v_base * v_acc;

#if (E10_HAS_JOYSTICK == 1)
            // [v0.1.0] Joystick은 후순위 구현: 현재는 동작에 반영하지 않음
            // int v_jdx=0, v_jdy=0;
            // v_m->readJoystickStub(v_jdx, v_jdy);
            // TODO(v0.3.0): 에어모드/정밀모드 FSM에 따라 v_tx/v_ty를 대체/혼합
#endif

            // 7) 스크롤 전용 버튼 처리(BTN_SCROLL)
            int v_wheel = 0;
            if (v_scrollMode) {
                // gyro.y(deg/s)로 wheel 생성
                if (v_gy > G_E10_WHEEL_TH_DEG) {
                    float v_norm = (v_gy - G_E10_WHEEL_TH_DEG) / 120.0f;
                    if (v_norm > 1.0f) v_norm = 1.0f;
                    v_wheel = (int)(1 + (v_norm * (G_E10_WHEEL_STEP_MAX - 1)));
                } else if (v_gy < -G_E10_WHEEL_TH_DEG) {
                    float v_norm = (-v_gy - G_E10_WHEEL_TH_DEG) / 120.0f;
                    if (v_norm > 1.0f) v_norm = 1.0f;
                    v_wheel = -(int)(1 + (v_norm * (G_E10_WHEEL_STEP_MAX - 1)));
                }

                // 스크롤 중 커서 이동 억제(오동작 방지)
                v_fx *= 0.25f;
                v_fy *= 0.25f;
            }

            // 8) 공유 상태 저장
            if (xSemaphoreTake(v_m->_mutex, 0) == pdTRUE) {
                v_m->_state.x       = (int)v_fx;
                v_m->_state.y       = (int)v_fy;
                v_m->_state.wheel   = v_wheel;
                v_m->_state.updated = true;
                xSemaphoreGive(v_m->_mutex);
            }

            // 9) 버튼 상태 즉시 반영
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


