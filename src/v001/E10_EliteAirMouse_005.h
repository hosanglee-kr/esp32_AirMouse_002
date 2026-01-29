#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : E10_EliteAirMouse_005.h
 * 모듈약어 : E10
 * 모듈명 : ESP32-S3 기반 에어마우스 + 프리젠터(Composite HID)
 * ------------------------------------------------------
 * 기능 요약
 *  - MPU6050(Adafruit) 기반 자이로 에어마우스
 *  - PPT 제스처(이전/다음 등) 키보드 HID 전송
 *  - FreeRTOS 듀얼 코어 태스크 분산 (Sensor: Core 1, Comm: Core 0)
 *  - Mystfit/ESP32-BLE-CompositeHID 기반 Composite HID(Mouse+Keyboard)
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

// Mystfit Composite HID
#include <BleCompositeHID.h>
#include <KeyboardDevice.h>
#include <MouseDevice.h>
#include <KeyboardHIDCodes.h>

#include "M10_MotionProc_003.h"

namespace E10_ {

// ----- 키 상수 매핑(라이브러리별 네이밍 차이 흡수) -----
#ifndef KEY_LEFT_SHIFT
  #ifdef KEY_LEFTSHIFT
    #define KEY_LEFT_SHIFT KEY_LEFTSHIFT
  #endif
#endif
#ifndef KEY_LEFT_CTRL
  #ifdef KEY_LEFTCTRL
    #define KEY_LEFT_CTRL KEY_LEFTCTRL
  #endif
#endif
#ifndef KEY_PAGE_UP
  #ifdef KEY_PAGEUP
    #define KEY_PAGE_UP KEY_PAGEUP
  #endif
#endif
#ifndef KEY_PAGE_DOWN
  #ifdef KEY_PAGEDOWN
    #define KEY_PAGE_DOWN KEY_PAGEDOWN
  #endif
#endif

class CL_E10_EliteAirMouse {
private:
    // 센서/엔진
    Adafruit_MPU6050 _mpu;
    AdvancedMotionProcessor _engine;

    // Composite HID
    BleCompositeHID _hid;
    KeyboardDevice _keyboard;
    MouseDevice _mouse;

    // GPIO (사용 HW에 맞춰 조정)
    const int _btnL = 1;
    const int _btnMode = 2;

    bool _isPPTMode = false;

    struct ST_E10_MouseState {
        int x;
        int y;
        int wheel;
        bool updated;
    } _state;

    SemaphoreHandle_t _mutex = nullptr;

    // HID Mouse 버튼(일반적으로 bit0=Left)
    static constexpr uint8_t s_mouseBtnLeft = 0x01;

public:
    CL_E10_EliteAirMouse()
        : _hid("Elite AirMouse S3", "ProMaker", 100) {
        _state = {0, 0, 0, false};
    }

    void begin() {
        // I2C
        Wire.begin(4, 5);
        Wire.setClock(400000);

        if (!_mpu.begin()) {
            Serial.begin(115200);
            Serial.println("Failed to find MPU6050 chip");
            for (;;) delay(10);
        }

        _mpu.setGyroRange(MPU6050_RANGE_250_DEG);
        _mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
        _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

        pinMode(_btnL, INPUT_PULLUP);
        pinMode(_btnMode, INPUT_PULLUP);

        _mutex = xSemaphoreCreateMutex();

        // Composite HID에 device 추가 후 시작 2
        _hid.addDevice(&_keyboard);
        _hid.addDevice(&_mouse);
        _hid.begin();

        // 태스크
        xTaskCreatePinnedToCore(sensorTask, "E10_Sensor", 8192, this, 3, nullptr, 1);
        xTaskCreatePinnedToCore(commTask,   "E10_Comm",   4096, this, 2, nullptr, 0);
    }

private:
    // ---- C++17 detection idiom: 라이브러리별 함수명 차이 흡수 ----
    template <typename T>
    static auto _has_keyPress(int) -> decltype(std::declval<T&>().keyPress(0), std::true_type{});
    template <typename T>
    static auto _has_keyPress(...) -> std::false_type;

    template <typename T>
    static auto _has_keyRelease(int) -> decltype(std::declval<T&>().keyRelease(0), std::true_type{});
    template <typename T>
    static auto _has_keyRelease(...) -> std::false_type;

    template <typename T>
    static auto _has_mouseMove(int) -> decltype(std::declval<T&>().mouseMove((int8_t)0, (int8_t)0), std::true_type{});
    template <typename T>
    static auto _has_mouseMove(...) -> std::false_type;

    template <typename T>
    static auto _has_mouseButtonPress(int) -> decltype(std::declval<T&>().mouseButtonPress((uint8_t)0), std::true_type{});
    template <typename T>
    static auto _has_mouseButtonPress(...) -> std::false_type;

    template <typename T>
    static auto _has_mouseButtonRelease(int) -> decltype(std::declval<T&>().mouseButtonRelease((uint8_t)0), std::true_type{});
    template <typename T>
    static auto _has_mouseButtonRelease(...) -> std::false_type;

    static void keyPressCompat(KeyboardDevice& p_kb, uint8_t p_key) {
        if constexpr (decltype(_has_keyPress<KeyboardDevice>(0))::value) {
            p_kb.keyPress(p_key);
        }
    }
    static void keyReleaseCompat(KeyboardDevice& p_kb, uint8_t p_key) {
        if constexpr (decltype(_has_keyRelease<KeyboardDevice>(0))::value) {
            p_kb.keyRelease(p_key);
        }
    }
    static void mouseMoveCompat(MouseDevice& p_ms, int p_x, int p_y, int p_wheel) {
        (void)p_wheel;
        if constexpr (decltype(_has_mouseMove<MouseDevice>(0))::value) {
            // 라이브러리 예제가 int8_t 기반 mouseMove 사용 3
            int8_t v_x = (int8_t)constrain(p_x, -127, 127);
            int8_t v_y = (int8_t)constrain(p_y, -127, 127);
            p_ms.mouseMove(v_x, v_y);
        }
    }
    static void mousePressCompat(MouseDevice& p_ms, uint8_t p_btnMask) {
        if constexpr (decltype(_has_mouseButtonPress<MouseDevice>(0))::value) {
            p_ms.mouseButtonPress(p_btnMask);
        }
    }
    static void mouseReleaseCompat(MouseDevice& p_ms, uint8_t p_btnMask) {
        if constexpr (decltype(_has_mouseButtonRelease<MouseDevice>(0))::value) {
            p_ms.mouseButtonRelease(p_btnMask);
        }
    }

    void sendPPTCommand(const char* p_label) {
        // 연결 체크는 compositeHID로 4
        if (!_hid.isConnected()) return;

        if (strcmp(p_label, "START") == 0) {
            // Shift+F5
            keyPressCompat(_keyboard, KEY_LEFTSHIFT);
            keyPressCompat(_keyboard, KEY_F5);
            delay(30);
            keyReleaseCompat(_keyboard, KEY_F5);
            keyReleaseCompat(_keyboard, KEY_LEFTSHIFT);
        }
        else if (strcmp(p_label, "EXIT") == 0) {
            keyPressCompat(_keyboard, KEY_ESC);
            delay(10);
            keyReleaseCompat(_keyboard, KEY_ESC);
        }
        else if (strcmp(p_label, "NEXT") == 0) {
            keyPressCompat(_keyboard, KEY_PAGEDOWN);
            delay(10);
            keyReleaseCompat(_keyboard, KEY_PAGEDOWN);
        }
        else if (strcmp(p_label, "PREV") == 0) {
            keyPressCompat(_keyboard, KEY_PAGEUP);
            delay(10);
            keyReleaseCompat(_keyboard, KEY_PAGEUP);
        }
        else if (strcmp(p_label, "BLACK") == 0) {
            // 'b'
            keyPressCompat(_keyboard, KEY_B);
            delay(10);
            keyReleaseCompat(_keyboard, KEY_B);
        }
        else if (strcmp(p_label, "LASER") == 0) {
            // Ctrl+L
            keyPressCompat(_keyboard, KEY_LEFTCTRL);
            keyPressCompat(_keyboard, KEY_L);
            delay(20);
            keyReleaseCompat(_keyboard, KEY_L);
            keyReleaseCompat(_keyboard, KEY_LEFTCTRL);
        }
    }

    void processGestures(float p_gz) {
        static unsigned long s_lastFlick = 0;
        if (millis() - s_lastFlick < 600) return;

        if (p_gz > 3.5f) {
            sendPPTCommand("PREV");
            s_lastFlick = millis();
        } else if (p_gz < -3.5f) {
            sendPPTCommand("NEXT");
            s_lastFlick = millis();
        }
    }

    static void sensorTask(void* p_pv) {
        auto* v_m = (CL_E10_EliteAirMouse*)p_pv;

        TickType_t v_lastWake = xTaskGetTickCount();
        unsigned long v_lastTime = micros();

        for (;;) {
            sensors_event_t v_a, v_g, v_temp;
            v_m->_mpu.getEvent(&v_a, &v_g, &v_temp);

            float v_dt = (micros() - v_lastTime) / 1000000.0f;
            v_lastTime = micros();

            // 버튼 처리
            static unsigned long s_btnTime = 0;
            if (digitalRead(v_m->_btnMode) == LOW) {
                if (s_btnTime == 0) s_btnTime = millis();
            } else {
                if (s_btnTime > 0) {
                    if (millis() - s_btnTime > 1000) v_m->_isPPTMode = !v_m->_isPPTMode;
                    else v_m->_engine.setDPI(2);
                    s_btnTime = 0;
                }
            }

            // 엔진 업데이트
            v_m->_engine.updateOrientation(
                v_a.acceleration.y,
                v_a.acceleration.z,
                v_g.gyro.x * RAD_TO_DEG,
                v_dt
            );

            int v_tx = 0, v_ty = 0;
            v_m->_engine.process(
                -(v_g.gyro.z * RAD_TO_DEG),
                -(v_g.gyro.x * RAD_TO_DEG),
                v_tx, v_ty
            );

            if (v_m->_isPPTMode) v_m->processGestures(v_g.gyro.z);

            bool v_leftClick = (digitalRead(v_m->_btnL) == LOW);
            if (v_leftClick) v_m->_engine.notifyClick();

            if (xSemaphoreTake(v_m->_mutex, 0) == pdTRUE) {
                v_m->_state.x = v_tx;
                v_m->_state.y = v_ty;
                v_m->_state.wheel = 0;
                v_m->_state.updated = true;
                xSemaphoreGive(v_m->_mutex);
            }

            // 클릭은 MouseDevice에 버튼 press/release가 있으면 사용(없으면 컴파일만 통과)
            if (v_m->_hid.isConnected()) {
                if (v_leftClick) mousePressCompat(v_m->_mouse, s_mouseBtnLeft);
                else            mouseReleaseCompat(v_m->_mouse, s_mouseBtnLeft);
            }

            vTaskDelayUntil(&v_lastWake, pdMS_TO_TICKS(8)); // 125Hz
        }
    }

    static void commTask(void* p_pv) {
        auto* v_m = (CL_E10_EliteAirMouse*)p_pv;

        for (;;) {
            if (v_m->_hid.isConnected() && xSemaphoreTake(v_m->_mutex, portMAX_DELAY) == pdTRUE) {
                if (v_m->_state.updated) {
                    mouseMoveCompat(v_m->_mouse, v_m->_state.x, v_m->_state.y, v_m->_state.wheel);
                    v_m->_state.updated = false;
                }
                xSemaphoreGive(v_m->_mutex);
            }
            vTaskDelay(pdMS_TO_TICKS(7));
        }
    }
};

} // namespace E10_
