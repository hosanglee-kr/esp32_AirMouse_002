// E10_EliteAirMouse_002.h

#ifndef ELITE_AIR_MOUSE_H
#define ELITE_AIR_MOUSE_H

#include <BleMouse.h>
#include <MPU6050.h>
#include "M10_MotionProc_002.h"


class EliteAirMouse {
private:
    MPU6050 _mpu;
    BleMouse _bleMouse;
    AdvancedMotionProcessor _engine;
    
    const int L_BTN = 1;
    const int DPI_BTN = 2; // DPI 변경용 버튼
    int _currentDpiLevel = 1;

    struct { int x, y, wheel; bool updated; } _state;
    SemaphoreHandle_t _mutex;
    unsigned long _lastMoveTime = 0;

public:
    EliteAirMouse() : _bleMouse("Elite AirMouse S3", "ProMaker", 100) {
        _mutex = xSemaphoreCreateMutex();
    }

    void begin() {
        Wire.begin(4, 5);
        Wire.setClock(400000);
        _mpu.initialize();
        _mpu.setFullScaleGyroRange(MPU6050_GYRO_RANGE_250);

        pinMode(L_BTN, INPUT_PULLUP);
        pinMode(DPI_BTN, INPUT_PULLUP);

        _bleMouse.begin();
        xTaskCreatePinnedToCore(this->sensorTask, "Sensor", 8192, this, 3, NULL, 1);
        xTaskCreatePinnedToCore(this->commTask, "Comm", 4096, this, 2, NULL, 0);
    }

private:
    // 제스처 인식: 좌/우 빠른 휘두르기 (Flick)
    void checkGesture(float gz) {
        static unsigned long lastFlick = 0;
        if (millis() - lastFlick < 500) return;

        if (gz > 400.0f) { // 왼쪽 Flick
            // _bleMouse.click(MOUSE_BACK); // 예시: 뒤로가기
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
            
            unsigned long now = micros();
            float dt = (now - lastTime) / 1000000.0f;
            lastTime = now;

            // 1. DPI 전환 체크
            if (digitalRead(m->DPI_BTN) == LOW) {
                m->_currentDpiLevel = (m->_currentDpiLevel % 3) + 1;
                m->_engine.setDPI(m->_currentDpiLevel);
                delay(200); // 디바운스
            }

            // 2. 물리 엔진 업데이트 (기울기 보정 및 필터)
            m->_engine.updateOrientation((float)ay, (float)az, (float)gx, dt);
            
            int tx, ty;
            m->_engine.process(-(gz / 131.0f), -(gx / 131.0f), tx, ty);
            
            // 3. 제스처 체크
            m->checkGesture(gz / 131.0f);

            // 4. 데이터 공유
            if (xSemaphoreTake(m->_mutex, 0) == pdTRUE) {
                m->_state = {tx, ty, 0, true};
                if(tx != 0 || ty != 0) m->_lastMoveTime = millis();
                xSemaphoreGive(m->_mutex);
            }

            // 버튼 처리 (클릭 보정 알림 포함)
            bool lClick = (digitalRead(m->L_BTN) == LOW);
            static bool prevL = false;
            if (lClick && !prevL) m->_engine.notifyClick();
            prevL = lClick;

            if (m->_bleMouse.isConnected()) {
                if (lClick) m->_bleMouse.press(); else m->_bleMouse.release();
            }

            vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(8));
        }
    }

    static void commTask(void *pv) {
        EliteAirMouse *m = (EliteAirMouse *)pv;
        for (;;) {
            if (m->_bleMouse.isConnected()) {
                // 절전 모드 검토: 2분 미사용 시 동작 최소화 로직 등 추가 가능
                if (xSemaphoreTake(m->_mutex, portMAX_DELAY) == pdTRUE) {
                    if (m->_state.updated) {
                        m->_bleMouse.move(m->_state.x, m->_state.y, m->_state.wheel);
                        m->_state.updated = false;
                    }
                    xSemaphoreGive(m->_mutex);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(7));
        }
    }
};

#endif

