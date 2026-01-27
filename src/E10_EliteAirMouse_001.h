// E10_EliteAirMouse_001.h

#ifndef ELITE_AIR_MOUSE_H
#define ELITE_AIR_MOUSE_H

#include <BleMouse.h>
#include <MPU6050.h>
#include "M10_MotionProc_001.h"

class EliteAirMouse {
private:
    MPU6050 _mpu;
    BleMouse _bleMouse;
    AdvancedMotionProcessor _engine;
    
    // 하드웨어 핀 정의 (사용자 환경에 맞게 수정)
    const int L_BTN = 1;
    const int SCROLL_BTN = 2;

    // FreeRTOS 멀티코어 동기화를 위한 구조체 및 뮤텍스
    struct MouseState { int x, y, wheel; bool updated; } _state;
    SemaphoreHandle_t _mutex;

public:
    EliteAirMouse() : _bleMouse("Elite AirMouse S3", "ProMaker", 100) {
        _mutex = xSemaphoreCreateMutex();
    }

    /**
     * @brief 마우스 초기화 및 RTOS 태스크 기동
     */
    void begin() {
        Wire.begin(4, 5); // SDA, SCL 핀 설정
        Wire.setClock(400000); // I2C 통신 속도 향상

        _mpu.initialize();
        _mpu.setXGyroOffset(55); // 정지 상태에서 커서가 흐르지 않게 교정값 입력
        _mpu.setZGyroOffset(-15);

        pinMode(L_BTN, INPUT_PULLUP);
        pinMode(SCROLL_BTN, INPUT_PULLUP);

        _bleMouse.begin();

        // Core 1: 센서 읽기 및 물리 엔진 처리 (높은 우선순위)
        xTaskCreatePinnedToCore(this->sensorTask, "Sensor", 8192, this, 3, NULL, 1);
        // Core 0: 블루투스 통신 처리 (낮은 우선순위)
        xTaskCreatePinnedToCore(this->commTask, "Comm", 4096, this, 2, NULL, 0);
    }

private:
    // 센서 처리 태스크: 125Hz 주기로 정밀 계산
    static void sensorTask(void *pv) {
        EliteAirMouse *m = (EliteAirMouse *)pv;
        TickType_t lastWake = xTaskGetTickCount();

        for (;;) {
            int16_t ax, ay, az, gx, gy, gz;
            m->_mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

            // 클릭 버튼 상태 모니터링
            bool leftPressed = (digitalRead(m->L_BTN) == LOW);
            static bool lastL = false;
            if (leftPressed && !lastL) m->_engine.notifyClick(); // 클릭 시 엔진에 알림
            lastL = leftPressed;

            int tx, ty, tw = 0;
            // 4대 알고리즘 엔진 실행
            m->_engine.process(-(gz / 131.0f), -(gx / 131.0f), (float)ay, (float)az, tx, ty);

            // 스크롤 모드: 버튼을 누른 상태에서는 상하 움직임을 휠로 전환
            if (digitalRead(m->SCROLL_BTN) == LOW) {
                tw = ty / 4; 
                tx = ty = 0;
            }

            // 공유 변수 업데이트 (뮤텍스 보호)
            if (xSemaphoreTake(m->_mutex, 0) == pdTRUE) {
                m->_state = {tx, ty, tw, true};
                xSemaphoreGive(m->_mutex);
            }

            // 버튼 클릭 신호 전송
            if (m->_bleMouse.isConnected()) {
                if (leftPressed) { if(!m->_bleMouse.isPressed()) m->_bleMouse.press(); }
                else { if(m->_bleMouse.isPressed()) m->_bleMouse.release(); }
            }
            vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(8)); // 정확히 8ms(125Hz) 유지
        }
    }

    // 블루투스 전송 태스크: 비동기적으로 데이터 송신
    static void commTask(void *pv) {
        EliteAirMouse *m = (EliteAirMouse *)pv;
        for (;;) {
            if (m->_bleMouse.isConnected()) {
                int dx = 0, dy = 0, dw = 0;
                bool needsSend = false;

                if (xSemaphoreTake(m->_mutex, portMAX_DELAY) == pdTRUE) {
                    if (m->_state.updated) {
                        dx = m->_state.x; dy = m->_state.y; dw = m->_state.wheel;
                        m->_state.updated = false;
                        needsSend = true;
                    }
                    xSemaphoreGive(m->_mutex);
                }

                if (needsSend && (dx != 0 || dy != 0 || dw != 0)) {
                    m->_bleMouse.move(dx, dy, dw);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
};

#endif
