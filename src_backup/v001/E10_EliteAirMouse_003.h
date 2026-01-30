/**
 * @file E10_EliteAirMouse_003.h
 * @brief ESP32-S3 기반 에어마우스 및 프리젠터 통합 제어
 * @details
 * - Adafruit MPU6050 라이브러리 사용
 * - 마우스 이동 및 PPT 단축키(키보드) 동시 지원
 * - FreeRTOS 듀얼 코어 태스크 분산 (Sensor: Core 1, Comm: Core 0)
 */

#pragma once

#include <BleMouse.h>
#include <BleKeyboard.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include "M10_MotionProc_003.h"

class EliteAirMouse {
private:
    Adafruit_MPU6050 _mpu;
    BleMouse _mouse;
    BleKeyboard _keyboard;
    AdvancedMotionProcessor _engine;
    
    // GPIO 설정 (사용자 하드웨어에 맞춰 수정 가능)
    const int BTN_L = 1;      // 왼쪽 클릭 버튼
    const int BTN_MODE = 2;   // 모드 전환(Long) / DPI 변경(Short) 버튼
    
    bool _isPPTMode = false;  // 현재 PPT 제스처 모드 활성화 여부
    struct { int x, y, wheel; bool updated; } _state;
    SemaphoreHandle_t _mutex;

public:
    EliteAirMouse() : 
        _mouse("Elite AirMouse S3", "ProMaker", 100), 
        _keyboard("Elite Presenter S3", "ProMaker", 100), 
        _mutex(NULL) {}

    /** @brief 하드웨어 초기화 및 RTOS 태스크 기동 */
    void begin() {
        Wire.begin(4, 5);
        Wire.setClock(400000);

        if (!_mpu.begin()) {
            Serial.println("Failed to find MPU6050 chip");
            while (1) delay(10);
        }

        // 센서 범위 설정 (에어마우스 최적화)
        _mpu.setGyroRange(MPU6050_RANGE_250_DEG);
        _mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
        _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

        pinMode(BTN_L, INPUT_PULLUP);
        pinMode(BTN_MODE, INPUT_PULLUP);

        _mutex = xSemaphoreCreateMutex();
        _mouse.begin();
        _keyboard.begin();

        // 고순위 센서 태스크 (Core 1)
        xTaskCreatePinnedToCore(this->sensorTask, "Sensor", 8192, this, 3, NULL, 1);
        // 통신 태스크 (Core 0)
        xTaskCreatePinnedToCore(this->commTask, "Comm", 4096, this, 2, NULL, 0);
    }

private:
    /** @brief PPT 전용 단축키 명령 전송 */
    void sendPPTCommand(const char* label) {
        if (!_keyboard.isConnected()) return;
        
        if (strcmp(label, "START") == 0) { // 현재 슬라이드부터 시작
            _keyboard.press(KEY_LEFT_SHIFT); _keyboard.press(KEY_F5);
            delay(50); _keyboard.releaseAll();
        } 
        else if (strcmp(label, "EXIT") == 0)  { _keyboard.write(KEY_ESC); }
        else if (strcmp(label, "NEXT") == 0)  { _keyboard.write(KEY_PAGE_DOWN); }
        else if (strcmp(label, "PREV") == 0)  { _keyboard.write(KEY_PAGE_UP); }
        else if (strcmp(label, "BLACK") == 0) { _keyboard.print("b"); }
        else if (strcmp(label, "LASER") == 0) { // 레이저 포인터 활성화 (Ctrl+L)
            _keyboard.press(KEY_LEFT_CTRL); _keyboard.print("l");
            delay(50); _keyboard.releaseAll();
        }
    }

    /** @brief 자이로 회전 속도를 이용한 휘두르기(Flick) 제스처 인식 */
    void processGestures(float gz) {
        static unsigned long lastFlick = 0;
        if (millis() - lastFlick < 600) return; // 제스처 연속 실행 방지

        if (gz > 3.5f) {        // 왼쪽으로 빠르게 휘두르기 (약 200deg/s 이상)
            sendPPTCommand("PREV");
            lastFlick = millis();
        } else if (gz < -3.5f) { // 오른쪽으로 빠르게 휘두르기
            sendPPTCommand("NEXT");
            lastFlick = millis();
        }
    }

    /** @brief 센서 데이터 수집 및 물리 연산 태스크 (Core 1) */
    static void sensorTask(void *pv) {
        EliteAirMouse *m = (EliteAirMouse *)pv;
        TickType_t lastWake = xTaskGetTickCount();
        unsigned long lastTime = micros();

        for (;;) {
            sensors_event_t a, g, temp;
            m->_mpu.getEvent(&a, &g, &temp);
            
            float dt = (micros() - lastTime) / 1000000.0f;
            lastTime = micros();

            // 1. 버튼 입력 처리 (모드 전환 및 DPI 조절)
            static unsigned long btnTime = 0;
            if (digitalRead(m->BTN_MODE) == LOW) {
                if (btnTime == 0) btnTime = millis();
            } else {
                if (btnTime > 0) {
                    if (millis() - btnTime > 1000) m->_isPPTMode = !m->_isPPTMode; // 1초 이상: 모드 전환
                    else m->_engine.setDPI(2); // 짧게: DPI 변경(예시로 2단계 고정)
                    btnTime = 0;
                }
            }

            // 2. 물리 엔진 업데이트
            m->_engine.updateOrientation(a.acceleration.y, a.acceleration.z, g.gyro.x * RAD_TO_DEG, dt);
            
            int tx, ty;
            m->_engine.process(-(g.gyro.z * RAD_TO_DEG), -(g.gyro.x * RAD_TO_DEG), tx, ty);

            // 3. PPT 모드 시 제스처 감지
            if (m->_isPPTMode) m->processGestures(g.gyro.z);

            // 4. 클릭 처리 및 데이터 공유 (Mutex 보호)
            bool leftClick = (digitalRead(m->BTN_L) == LOW);
            if (leftClick) m->_engine.notifyClick();

            if (xSemaphoreTake(m->_mutex, 0) == pdTRUE) {
                m->_state = {tx, ty, 0, true};
                xSemaphoreGive(m->_mutex);
            }

            // 5. 마우스 버튼 상태 즉시 반영
            if (m->_mouse.isConnected()) {
                if (leftClick) m->_mouse.press(MOUSE_LEFT);
                else m->_mouse.release(MOUSE_LEFT);
            }

            vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(8)); // 125Hz 샘플링
        }
    }

    /** @brief BLE 데이터 전송 태스크 (Core 0) */
    static void commTask(void *pv) {
        EliteAirMouse *m = (EliteAirMouse *)pv;
        for (;;) {
            if (m->_mouse.isConnected() && xSemaphoreTake(m->_mutex, portMAX_DELAY) == pdTRUE) {
                if (m->_state.updated) {
                    m->_mouse.move(m->_state.x, m->_state.y, m->_state.wheel);
                    m->_state.updated = false;
                }
                xSemaphoreGive(m->_mutex);
            }
            vTaskDelay(pdMS_TO_TICKS(7)); // 블루투스 스택 안정화 주기
        }
    }
};

