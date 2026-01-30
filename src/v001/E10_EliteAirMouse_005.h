#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : E10_EliteAirMouse_005.h
 * 모듈약어 : E10
 * 모듈명 : ESP32-S3 기반 에어마우스 + 프리젠터 (Composite HID)
 * ------------------------------------------------------
 * 기능 요약
 *  - MPU6050 기반 자이로 에어마우스(마우스 HID)
 *  - PPT 제스처(이전/다음/시작/종료 등) 키보드 HID 전송
 *  - BLE Composite HID(Mouse + Keyboard) 단일 디바이스로 안정성 강화
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

// C++ 타입/검출(컴파일 안전)
#include <type_traits>
#include <utility>

// Mystfit Composite HID
#include <BleCompositeHID.h>
#include <KeyboardDevice.h>
#include <MouseDevice.h>
#include <KeyboardHIDCodes.h>

#include "M10_MotionProc_003.h"

namespace E10_ {

class CL_E10_EliteAirMouse {
private:
    // ======================================================
    // HW/Engine
    // ======================================================
    Adafruit_MPU6050 _mpu;
    AdvancedMotionProcessor _engine;

    // ======================================================
    // BLE Composite HID
    // ======================================================
    BleCompositeHID _hid;
    KeyboardDevice _keyboard;
    MouseDevice _mouse;

    // ======================================================
    // GPIO (DevKitC 기준 안전 GPIO 예시)
    // ======================================================
    static constexpr int G_E10_BTN_L    = 12;
    static constexpr int G_E10_BTN_MODE = 13;

    // ======================================================
    // Runtime states
    // ======================================================
    volatile bool _isPPTMode = false;

    struct ST_E10_State {
        int x;
        int y;
        int wheel;
        bool updated;
    } _state;

    SemaphoreHandle_t _mutex;
    int _dpiLevel = 2; // 1~3

    // Mouse button mask (Left=bit0)
    static constexpr uint8_t G_E10_MOUSE_BTN_LEFT = 0x01;

    // ======================================================
    // [E10] 마우스 튜닝 파라미터
    //  - 최종 dx/dy는 int8_t(-127~127)로 전송됨
    //  - 스케일은 "픽셀 느낌의 v_tx/v_ty"를 HID delta로 매핑하는 계수
    // ------------------------------------------------------
    // [튜닝 TIP]
    //  - "전체적으로 느림"     : G_E10_SCALE_BASE_DPI2 값을 ↑ (예: 0.75 -> 0.90)
    //  - "빠르게 휘두를 때 더 빨리" : G_E10_ACCEL_GAIN_DPI2 값을 ↑ (예: 0.55 -> 0.75)
    //  - "가속이 너무 빨리 붙음"  : G_E10_ACCEL_TH 값을 ↑ (예: 8 -> 12)
    //  - "가속이 너무 과함"      : G_E10_ACCEL_GAIN_* 값을 ↓
    // ======================================================
    static constexpr float G_E10_SCALE_BASE_DPI1 = 0.55f;
    static constexpr float G_E10_SCALE_BASE_DPI2 = 0.75f;
    static constexpr float G_E10_SCALE_BASE_DPI3 = 1.00f;

    static constexpr float G_E10_ACCEL_GAIN_DPI1 = 0.35f;
    static constexpr float G_E10_ACCEL_GAIN_DPI2 = 0.55f;
    static constexpr float G_E10_ACCEL_GAIN_DPI3 = 0.85f;

    static constexpr float G_E10_ACCEL_TH = 8.0f;

    // ======================================================
    // [E10] 휠(스크롤) 튜닝 파라미터
    //  - "BTN_MODE 누르고 있는 동안" = 스크롤 모드
    //  - gyro.y(deg/s)를 기반으로 wheel step 생성
    // ------------------------------------------------------
    // [튜닝 TIP]
    //  - "스크롤이 너무 민감" : G_E10_WHEEL_TH_DEG 값을 ↑ (예: 90 -> 120)
    //  - "스크롤이 둔감"     : G_E10_WHEEL_TH_DEG 값을 ↓ (예: 90 -> 70)
    //  - "휠 속도 더 빠르게" : G_E10_WHEEL_STEP_MAX 값을 ↑ (예: 6 -> 10)
    // ======================================================
    static constexpr float G_E10_WHEEL_TH_DEG = 90.0f;
    static constexpr int   G_E10_WHEEL_STEP_MAX = 6;

    // ======================================================
    // [E10] MouseDevice wheel 지원 감지 + 전송 래퍼
    //  - 구현에 따라 mouseWheel() 또는 mouseMove(dx,dy,wheel) 지원 가능
    // ======================================================
    template <typename T>
    static auto _has_mouseWheel(int) -> decltype(std::declval<T&>().mouseWheel((int8_t)0), std::true_type{});
    template <typename T>
    static auto _has_mouseWheel(...) -> std::false_type;

    template <typename T>
    static auto _has_mouseMove3(int) -> decltype(std::declval<T&>().mouseMove((int8_t)0, (int8_t)0, (int8_t)0), std::true_type{});
    template <typename T>
    static auto _has_mouseMove3(...) -> std::false_type;
    
    static void mouseSendCompat(MouseDevice& p_ms, int8_t p_dx, int8_t p_dy, int8_t p_wheel) {
        // MouseDevice.h: mouseMove(x,y,scrollX,scrollY)
        // - 일반 세로 스크롤로 쓰려면 scrollY에 넣는 구현도 있는데,
        //   현재 라이브러리 필드는 _mouseWheel / _mouseHWheel 이라
        //   관례상 scrollX=wheel, scrollY=hwheel 로 쓰는 경우가 많습니다.
        //   (PC에서 방향이 반대면 아래 두 인자를 바꾸면 됩니다.)
        p_ms.mouseMove(p_dx, p_dy, p_wheel, 0);
    }

     /*
    static void mouseSendCompat(MouseDevice& p_ms, int8_t p_dx, int8_t p_dy, int8_t p_wheel) {
        if constexpr (decltype(_has_mouseMove3<MouseDevice>(0))::value) {
            p_ms.mouseMove(p_dx, p_dy, p_wheel);
        } else {
            p_ms.mouseMove(p_dx, p_dy);
            if (p_wheel != 0) {
                if constexpr (decltype(_has_mouseWheel<MouseDevice>(0))::value) {
                    p_ms.mouseWheel(p_wheel);
                }
            }
        }
    }
    */

public:
    CL_E10_EliteAirMouse()
    : _hid("Elite AirMouse S3", "ProMaker", 100),
      _mutex(nullptr)
    {
        _state = {0, 0, 0, false};
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

        _mutex = xSemaphoreCreateMutex();

        // Composite HID: device 등록 후 begin()
        _hid.addDevice(&_keyboard);
        _hid.addDevice(&_mouse);
        _hid.begin();

        // 태스크 분리
        xTaskCreatePinnedToCore(sensorTask, "E10_Sensor", 8192, this, 3, nullptr, 1);
        xTaskCreatePinnedToCore(commTask,   "E10_Comm",   4096, this, 2, nullptr, 0);
    }

private:
    void togglePptMode() {
        _isPPTMode = !_isPPTMode;
    }

    void cycleDpi() {
        _dpiLevel++;
        if (_dpiLevel > 3) _dpiLevel = 1;
        _engine.setDPI(_dpiLevel);
    }

    // keyPress/keyRelease 기반 tap
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

        if (strcmp(p_label, "START") == 0) {           // Shift + F5
            tapCombo(KEY_LEFTSHIFT, KEY_F5, 25);
        }
        else if (strcmp(p_label, "EXIT") == 0) {       // ESC
            tapKey(KEY_ESC);
        }
        else if (strcmp(p_label, "NEXT") == 0) {       // PageDown
            tapKey(KEY_PAGEDOWN);
        }
        else if (strcmp(p_label, "PREV") == 0) {       // PageUp
            tapKey(KEY_PAGEUP);
        }
        else if (strcmp(p_label, "BLACK") == 0) {      // 'b'
            tapKey(KEY_B);
        }
        else if (strcmp(p_label, "LASER") == 0) {      // Ctrl + L
            tapCombo(KEY_LEFTCTRL, KEY_L, 20);
        }
    }

    // 제스처: deg/s로 통일
    void processGesturesDeg(float p_gzDegPerSec) {
        static unsigned long s_lastFlick = 0;
        if (millis() - s_lastFlick < 600) return;

        // ------------------------------------------------------
        // [튜닝 TIP]
        //  - 인식이 너무 잦다(민감) : 임계값(200)을 ↑ (예: 260)
        //  - 인식이 잘 안 된다(둔감) : 임계값(200)을 ↓ (예: 150)
        // ------------------------------------------------------
        if (p_gzDegPerSec > 200.0f) {
            sendPPTCommand("PREV");
            s_lastFlick = millis();
        } else if (p_gzDegPerSec < -200.0f) {
            sendPPTCommand("NEXT");
            s_lastFlick = millis();
        }
    }

    static void sensorTask(void* p_pv) {
        CL_E10_EliteAirMouse* v_m = (CL_E10_EliteAirMouse*)p_pv;

        TickType_t v_lastWake = xTaskGetTickCount();
        unsigned long v_lastUs = micros();
        unsigned long v_btnDownMs = 0;

        for (;;) {
            sensors_event_t v_a, v_g, v_temp;
            v_m->_mpu.getEvent(&v_a, &v_g, &v_temp);

            const unsigned long v_nowUs = micros();
            const float v_dt = (v_nowUs - v_lastUs) / 1000000.0f;
            v_lastUs = v_nowUs;

            // 1) BTN_MODE: short=감도 변경, long=모드 토글
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

            // 2) 물리 엔진 업데이트 (deg/s 기준 통일)
            v_m->_engine.updateOrientation(
                v_a.acceleration.y,
                v_a.acceleration.z,
                v_g.gyro.x * RAD_TO_DEG,
                v_dt
            );

            int v_tx = 0, v_ty = 0;
            const float v_rawX = -(v_g.gyro.z * RAD_TO_DEG);
            const float v_rawY = -(v_g.gyro.x * RAD_TO_DEG);
            v_m->_engine.process(v_rawX, v_rawY, v_tx, v_ty);

            // 3) PPT 모드 제스처
            if (v_m->_isPPTMode) {
                v_m->processGesturesDeg(v_g.gyro.z * RAD_TO_DEG);
            }

            // 4) 클릭 처리
            const bool v_leftClick = (digitalRead(G_E10_BTN_L) == LOW);
            if (v_leftClick) v_m->_engine.notifyClick();

            // ======================================================
            // (1) DPI 기반 base scale 선택
            // ======================================================
            float v_base = G_E10_SCALE_BASE_DPI2;
            float v_accg = G_E10_ACCEL_GAIN_DPI2;

            if (v_m->_dpiLevel == 1) { v_base = G_E10_SCALE_BASE_DPI1; v_accg = G_E10_ACCEL_GAIN_DPI1; }
            else if (v_m->_dpiLevel == 3) { v_base = G_E10_SCALE_BASE_DPI3; v_accg = G_E10_ACCEL_GAIN_DPI3; }

            // ======================================================
            // (2) 가속 적용 (움직임이 커지면 추가 배율)
            // ======================================================
            const float v_mag = sqrtf((float)v_tx * (float)v_tx + (float)v_ty * (float)v_ty);
            float v_acc = 1.0f;
            if (v_mag > G_E10_ACCEL_TH) {
                const float v_ex = (v_mag - G_E10_ACCEL_TH);
                // 완만한 곡선(과가속 방지)
                v_acc = 1.0f + (v_accg * (v_ex / (v_ex + 18.0f)));
            }

            float v_fx = (float)v_tx * v_base * v_acc;
            float v_fy = (float)v_ty * v_base * v_acc;

            // ======================================================
            // (3) 휠 계산: BTN_MODE 누르고 있는 동안 = 스크롤 모드
            //     gyro.y(deg/s)로 wheel step 생성
            // ======================================================
            int v_wheel = 0;
            const bool v_scrollMode = (digitalRead(G_E10_BTN_MODE) == LOW);
            if (v_scrollMode) {
                const float v_gyDeg = v_g.gyro.y * RAD_TO_DEG;

                if (v_gyDeg > G_E10_WHEEL_TH_DEG) {
                    float v_norm = (v_gyDeg - G_E10_WHEEL_TH_DEG) / 120.0f;
                    if (v_norm > 1.0f) v_norm = 1.0f;
                    v_wheel = (int)(1 + (v_norm * (G_E10_WHEEL_STEP_MAX - 1)));
                }
                else if (v_gyDeg < -G_E10_WHEEL_TH_DEG) {
                    float v_norm = (-v_gyDeg - G_E10_WHEEL_TH_DEG) / 120.0f;
                    if (v_norm > 1.0f) v_norm = 1.0f;
                    v_wheel = -(int)(1 + (v_norm * (G_E10_WHEEL_STEP_MAX - 1)));
                }

                // 스크롤 모드에서는 커서 이동을 줄여 오동작 방지
                v_fx *= 0.25f;
                v_fy *= 0.25f;
            }

            // ======================================================
            // (4) state 저장
            // ======================================================
            if (xSemaphoreTake(v_m->_mutex, 0) == pdTRUE) {
                v_m->_state.x = (int)v_fx;
                v_m->_state.y = (int)v_fy;
                v_m->_state.wheel = v_wheel;
                v_m->_state.updated = true;
                xSemaphoreGive(v_m->_mutex);
            }

            // 6) 버튼은 즉시 반영
            if (v_m->_hid.isConnected()) {
                if (v_leftClick) v_m->_mouse.mousePress(G_E10_MOUSE_BTN_LEFT);
                else            v_m->_mouse.mouseRelease(G_E10_MOUSE_BTN_LEFT);
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

                    mouseSendCompat(v_m->_mouse, v_dx, v_dy, v_wh);
                    v_m->_state.updated = false;
                }
                xSemaphoreGive(v_m->_mutex);
            }
            vTaskDelay(pdMS_TO_TICKS(7));
        }
    }
};

} // namespace E10_
