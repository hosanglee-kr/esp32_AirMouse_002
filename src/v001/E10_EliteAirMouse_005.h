#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : E10_EliteAirMouse_005.h
 * 모듈약어 : E10
 * 모듈명 : ESP32-S3 기반 에어마우스 및 프리젠터 통합 제어 (Composite HID)
 * ------------------------------------------------------
 * 기능 요약
 *  - MPU6050 기반 자이로/가속도 입력으로 마우스 이동
 *  - PPT 제스처(이전/다음 등) 및 키보드 단축키 전송
 *  - BLE Composite HID(마우스+키보드 동시) 기반으로 Windows 호환성 강화
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

// Mystfit Composite HID
#include <BleCompositeHID.h>
#include <MouseDevice.h>
#include <KeyboardDevice.h>
#include <BLEHostConfiguration.h>   // (라이브러리 쪽 제공 헤더)

#include "M10_MotionProc_005.h"

class CL_E10_EliteAirMouse {
private:
    Adafruit_MPU6050 _mpu;

    // Composite HID 본체 + 디바이스 포인터
    BleCompositeHID _hid;
    MouseDevice* _mouse;
    KeyboardDevice* _keyboard;

    CL_M10_AdvancedMotionProcessor _engine;

    // GPIO
    const int BTN_L    = 1;
    const int BTN_MODE = 2;

    bool _isPPTMode;

    struct ST_E10_MouseState {
        int x;
        int y;
        int wheel;
        bool updated;
    } _state;

    SemaphoreHandle_t _mutex;

private:
    void sendPPTCommand(const char* p_label) {
        if (_keyboard == nullptr) return;
        if (!_keyboard->isConnected()) return;

        if (strcmp(p_label, "START") == 0) {
            // Shift + F5 : 현재 슬라이드부터 시작
            _keyboard->press(KEY_LEFT_SHIFT);
            _keyboard->press(KEY_F5);
            delay(50);
            _keyboard->releaseAll();
        } else if (strcmp(p_label, "EXIT") == 0) {
            _keyboard->write(KEY_ESC);
        } else if (strcmp(p_label, "NEXT") == 0) {
            _keyboard->write(KEY_PAGE_DOWN);
        } else if (strcmp(p_label, "PREV") == 0) {
            _keyboard->write(KEY_PAGE_UP);
        } else if (strcmp(p_label, "BLACK") == 0) {
            _keyboard->print("b");
        } else if (strcmp(p_label, "LASER") == 0) {
            // Ctrl + L
            _keyboard->press(KEY_LEFT_CTRL);
            _keyboard->print("l");
            delay(50);
            _keyboard->releaseAll();
        }
    }

    void processGestures(float p_gz_rad_s) {
        // 주의: Adafruit 센서 g.gyro.*는 rad/s 입니다.
        static unsigned long s_lastFlick = 0;
        if (millis() - s_lastFlick < 600) return;

        // 경험값: 약 3.5 rad/s 이상을 휘두르기로 판단
        if (p_gz_rad_s > 3.5f) {
            sendPPTCommand("PREV");
            s_lastFlick = millis();
        } else if (p_gz_rad_s < -3.5f) {
            sendPPTCommand("NEXT");
            s_lastFlick = millis();
        }
    }

    static void sensorTask(void* p_pv) {
        CL_E10_EliteAirMouse* v_m = (CL_E10_EliteAirMouse*)p_pv;

        TickType_t v_lastWake = xTaskGetTickCount();
        unsigned long v_lastTime = micros();

        for (;;) {
            sensors_event_t v_a, v_g, v_temp;
            v_m->_mpu.getEvent(&v_a, &v_g, &v_temp);

            float v_dt = (micros() - v_lastTime) / 1000000.0f;
            v_lastTime = micros();

            // 1) 버튼 처리
            static unsigned long s_btnTime = 0;
            if (digitalRead(v_m->BTN_MODE) == LOW) {
                if (s_btnTime == 0) s_btnTime = millis();
            } else {
                if (s_btnTime > 0) {
                    if (millis() - s_btnTime > 1000) {
                        v_m->_isPPTMode = !v_m->_isPPTMode;
                    } else {
                        v_m->_engine.setDPI(2);
                    }
                    s_btnTime = 0;
                }
            }

            // 2) 자세 업데이트 (gx는 deg/s로 넣도록 변환)
            v_m->_engine.updateOrientation(
                v_a.acceleration.y,
                v_a.acceleration.z,
                v_g.gyro.x * RAD_TO_DEG,
                v_dt
            );

            int v_tx = 0;
            int v_ty = 0;

            // 3) 마우스 이동량 산출 (raw 입력은 deg/s 기반으로 사용)
            v_m->_engine.process(
                -(v_g.gyro.z * RAD_TO_DEG),
                -(v_g.gyro.x * RAD_TO_DEG),
                v_tx,
                v_ty
            );

            // 4) PPT 모드 제스처
            if (v_m->_isPPTMode) {
                v_m->processGestures(v_g.gyro.z); // rad/s 그대로
            }

            // 5) 클릭 + 흔들림 억제
            bool v_leftClick = (digitalRead(v_m->BTN_L) == LOW);
            if (v_leftClick) v_m->_engine.notifyClick();

            // 상태 공유
            if (xSemaphoreTake(v_m->_mutex, 0) == pdTRUE) {
                v_m->_state.x = v_tx;
                v_m->_state.y = v_ty;
                v_m->_state.wheel = 0;
                v_m->_state.updated = true;
                xSemaphoreGive(v_m->_mutex);
            }

            // 버튼 상태 즉시 반영
            if (v_m->_mouse != nullptr && v_m->_mouse->isConnected()) {
                if (v_leftClick) v_m->_mouse->press(MOUSE_LEFT);
                else v_m->_mouse->release(MOUSE_LEFT);
            }

            vTaskDelayUntil(&v_lastWake, pdMS_TO_TICKS(8)); // 125Hz
        }
    }

    static void commTask(void* p_pv) {
        CL_E10_EliteAirMouse* v_m = (CL_E10_EliteAirMouse*)p_pv;

        for (;;) {
            if (v_m->_mouse != nullptr && v_m->_mouse->isConnected()) {
                if (xSemaphoreTake(v_m->_mutex, portMAX_DELAY) == pdTRUE) {
                    if (v_m->_state.updated) {
                        v_m->_mouse->move(v_m->_state.x, v_m->_state.y, v_m->_state.wheel);
                        v_m->_state.updated = false;
                    }
                    xSemaphoreGive(v_m->_mutex);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(7));
        }
    }

public:
    CL_E10_EliteAirMouse()
    : _hid("Elite AirMouse S3", "ProMaker", 100),
      _mouse(nullptr),
      _keyboard(nullptr),
      _isPPTMode(false),
      _mutex(nullptr)
    {
        _state.x = 0;
        _state.y = 0;
        _state.wheel = 0;
        _state.updated = false;
    }

    void begin() {
        Wire.begin(4, 5);
        Wire.setClock(400000);

        if (!_mpu.begin()) {
            Serial.begin(115200);
            Serial.println("Failed to find MPU6050 chip");
            while (1) delay(10);
        }

        _mpu.setGyroRange(MPU6050_RANGE_250_DEG);
        _mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
        _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

        pinMode(BTN_L, INPUT_PULLUP);
        pinMode(BTN_MODE, INPUT_PULLUP);

        _mutex = xSemaphoreCreateMutex();

        // Composite HID 시작 (HID 타입은 환경에 따라 조정 가능)
        BLEHostConfiguration v_cfg;
        v_cfg.setHidType(GENERIC_HID);   // 필요시 HID_MOUSE로 변경 가능 (표시/호환성)
        _hid.begin(v_cfg);

        // 디바이스 포인터 획득 (라이브러리 API에 따라 함수명이 다를 수 있어 2안 제공)
        // [안1] 권장 형태(CompositeHID 계열에서 흔한 패턴)
        _mouse = _hid.mouse();
        _keyboard = _hid.keyboard();

        // [안2] 위가 컴파일 실패하면 아래 형태로 교체 시도
        // _mouse = _hid.getMouse();
        // _keyboard = _hid.getKeyboard();

        xTaskCreatePinnedToCore(sensorTask, "E10_Sensor", 8192, this, 3, NULL, 1);
        xTaskCreatePinnedToCore(commTask,   "E10_Comm",   4096, this, 2, NULL, 0);
    }
};
