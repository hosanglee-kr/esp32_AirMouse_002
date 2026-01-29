// main.cpp

#include <Arduino.h>
#include "v001/E10_EliteAirMouse_004.h"


// 에어마우스 객체 생성
EliteAirMouse airMouse;

/**
 * @brief 시스템 설정 및 에어마우스 가동
 */
void setup() {
    // 에어마우스 모듈 시작 (내부적으로 멀티코어 태스크 생성)
    airMouse.begin();
}

/**
 * @brief 메인 루프는 비워두어 자원을 최소화 (모든 로직은 태스크에서 동작)
 */
void loop() {
    // FreeRTOS가 태스크를 관리하므로 메인 루프 태스크는 삭제하여 메모리 확보 가능
    vTaskDelete(NULL); 
}
