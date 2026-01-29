// E10_EliteAirMouse_002.h

#ifndef ELITE_AIR_MOUSE_H
#define ELITE_AIR_MOUSE_H

#include <BleMouse.h>
#include <BleKeyboard.h>
#include <MPU6050.h>
#include "M10_MotionProc_002.h"

class EliteAirMouse {
private:
    MPU6050 _mpu;
    BLEMouse _mouse;
    BLEKeyboard _keyboard;
    AdvancedMotionProcessor _engine;
    
    // GPIO 설정
    const int BTN_L = 1;      // 마우스 왼쪽 클릭
    const int BTN_MODE = 2;   // 모드 전환 및 DPI 조절
    
    bool _isPPTMode = false;
    struct { int x, y, wheel; bool updated; } _state;
    SemaphoreHandle_t _mutex;

public:
    EliteAirMouse() : _mouse("Elite AirMouse S3"), _keyboard("Elite Presenter S3"), _mutex(NULL) {}

    void begin() {
        Wire.begin(4, 5);
        Wire.setClock(400000);
        _mpu.initialize();
        _mpu.setFullScaleGyroRange(MPU6050_GYRO_RANGE_250);

        pinMode(BTN_L, INPUT_PULLUP);
        pinMode(BTN_MODE, INPUT_PULLUP);

        _mutex = xSemaphoreCreateMutex();
        _mouse.begin();
        _keyboard.begin();

        xTaskCreatePinnedToCore(this->sensorTask, "Sensor", 8192, this, 3, NULL, 1);
        xTaskCreatePinnedToCore(this->commTask, "Comm", 4096, this, 2, NULL, 0);
    }

private:
    // PPT 단축키 실행 함수
    void sendPPTCommand(const char* label) {
        if (!_keyboard.isConnected()) return;
        
        if (strcmp(label, "START") == 0) { // Shift + F5
            _keyboard.press(KEY_LEFT_SHIFT);
            _keyboard.press(KEY_F5);
            delay(50); _keyboard.releaseAll();
        } else if (strcmp(label, "EXIT") == 0) { _keyboard.write(KEY_ESC); }
        else if (strcmp(label, "NEXT") == 0) { _keyboard.write(KEY_PAGE_DOWN); }
        else if (strcmp(label, "PREV") == 0) { _keyboard.write(KEY_PAGE_UP); }
        else if (strcmp(label, "BLACK") == 0) { _keyboard.print("b"); }
        else if (strcmp(label, "LASER") == 0) { // Ctrl + L (토글)
            _keyboard.press(KEY_LEFT_CTRL); _keyboard.print("l");
            delay(50); _keyboard.releaseAll();
        } else if (strcmp(label, "PEN") == 0) { // Ctrl + P
            _keyboard.press(KEY_LEFT_CTRL); _keyboard.print("p");
            delay(50); _keyboard.releaseAll();
        }
    }

    // 제스처 감지 (Flick)
    void processGestures(float gz) {
        static unsigned long lastFlick = 0;
        if (millis() - lastFlick < 600) return;

        if (gz > 450.0f) { // 왼쪽으로 세게 휘두르기 -> 이전 슬라이드
            sendPPTCommand("PREV");
            lastFlick = millis();
        } else if (gz < -450.0f) { // 오른쪽으로 세게 휘두르기 -> 다음 슬라이드
            sendPPTCommand("NEXT");
            lastFlick = millis();
        }
    }

    static void sensorTask(void *pv) {
        EliteAirMouse *m = (EliteAirMouse *)pv;
        TickType_t lastWake = xTaskGetTickCount();
        unsigned long lastTime = micros();

        for (;;) {
            int16_t ax, ay, az, gx, gy, gz;
            m->_mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
            
            float dt = (micros() - lastTime) / 1000000.0f;
            lastTime = micros();

            // 1. 모드/DPI 전환 (길게 누르면 PPT 모드 토글)
            static unsigned long btnPressTime = 0;
            if (digitalRead(m->BTN_MODE) == LOW) {
                if (btnPressTime == 0) btnPressTime = millis();
            } else {
                if (btnPressTime > 0) {
                    if (millis() - btnPressTime > 1000) m->_isPPTMode = !m->_isPPTMode;
                    else m->_engine.setDPI(2); // 짧게 누르면 DPI 변경 등
                    btnPressTime = 0;
                }
            }

            // 2. 물리 엔진 연산
            m->_engine.updateOrientation((float)ay, (float)az, (float)gx, dt);
            int tx, ty;
            m->_engine.process(-(gz / 131.0f), -(gx / 131.0f), tx, ty);

            // 3. 제스처 처리 (PPT 모드에서 활성화)
            if (m->_isPPTMode) m->processGestures(gz / 131.0f);

            // 4. 클릭 처리 및 공유
            bool leftClick = (digitalRead(m->BTN_L) == LOW);
            if (leftClick) m->_engine.notifyClick();

            if (xSemaphoreTake(m->_mutex, 0) == pdTRUE) {
                m->_state = {tx, ty, 0, true};
                xSemaphoreGive(m->_mutex);
            }

            if (m->_mouse.isConnected()) {
                if (leftClick) m->_mouse.press(MOUSE_LEFT);
                else m->_mouse.release(MOUSE_LEFT);
            }

            vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(8));
        }
    }

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
            vTaskDelay(pdMS_TO_TICKS(7));
        }
    }
};

#endif

