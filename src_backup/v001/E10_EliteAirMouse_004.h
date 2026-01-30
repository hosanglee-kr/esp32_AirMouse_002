#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : E10_EliteAirMouse_004.h
 * 모듈약어 : E10
 * 모듈명 : ESP32-S3 기반 Elite AirMouse (BLE Gamepad Output)
 * ------------------------------------------------------
 * 기능 요약
 *  - MPU6050 센서 기반 모션 → BLE Gamepad(조이스틱) 축(X/Y) 출력
 *  - 버튼 입력: 클릭(Button1), 모드 토글/감도 변경(BTN_MODE)
 *  - PPT 제스처: Flick 감지 시 Gamepad 버튼 펄스 출력(PC에서 키 매핑 권장)
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

#include <BleGamepad.h>

#include "M10_MotionProc_004.h"

class CL_E10_EliteAirMouse {
private:
  Adafruit_MPU6050 _mpu;
  BleGamepad _gamepad;
  CL_M10_MotionProcessor _engine;

  // 안전 GPIO 예시 (DevKitC 기준). 실제 보드 회로에 맞게 조정 가능
  static constexpr int G_E10_BTN_L    = 12; // 클릭
  static constexpr int G_E10_BTN_MODE = 13; // 모드/감도

  volatile bool _isPptMode = false;

  struct ST_E10_State {
    int16_t x;
    int16_t y;
    bool updated;
  } _state;

  SemaphoreHandle_t _mutex;

  int _dpiLevel = 2; // 1~3

public:
  CL_E10_EliteAirMouse()
  : _gamepad("Elite AirMouse S3", "ProMaker", 100),
    _mutex(NULL) {
    _state = {0, 0, false};
  }

  void begin() {
    Wire.begin(4, 5);
    Wire.setClock(400000);

    if (!_mpu.begin()) {
      Serial.println("Failed to find MPU6050 chip");
      while (1) { delay(10); }
    }

    _mpu.setGyroRange(MPU6050_RANGE_250_DEG);
    _mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
    _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    pinMode(G_E10_BTN_L, INPUT_PULLUP);
    pinMode(G_E10_BTN_MODE, INPUT_PULLUP);

    _mutex = xSemaphoreCreateMutex();

    // 게임패드 축 범위는 -32767~32767로 사용
    _gamepad.begin();

    // 센서 처리(Core 1)
    xTaskCreatePinnedToCore(&CL_E10_EliteAirMouse::sensorTask, "E10_Sensor", 8192, this, 3, NULL, 1);
    // BLE 전송(Core 0)
    xTaskCreatePinnedToCore(&CL_E10_EliteAirMouse::commTask, "E10_Comm", 4096, this, 2, NULL, 0);
  }

private:
  void togglePptMode() {
    _isPptMode = !_isPptMode;
    // 필요 시 여기서 특정 버튼 펄스로 모드 변경 알림 가능
  }

  void cycleDpi() {
    _dpiLevel++;
    if (_dpiLevel > 3) _dpiLevel = 1;
    _engine.setDPI(_dpiLevel);
  }

  // PPT용 “버튼 펄스” 출력 (PC에서 JoyToKey 등으로 키 매핑 권장)
  void sendPptButtonPulse(uint8_t p_btnId, uint16_t p_ms = 30) {
    if (!_gamepad.isConnected()) return;
    _gamepad.press(p_btnId);
    vTaskDelay(pdMS_TO_TICKS(p_ms));
    _gamepad.release(p_btnId);
  }

  // Flick 제스처: deg/s 기준으로 통일
  void processGesturesDeg(float p_gzDegPerSec) {
    static unsigned long v_lastFlick = 0;
    if (millis() - v_lastFlick < 600) return;

    // 임계값(예: 200 deg/s). 필요시 튜닝
    if (p_gzDegPerSec > 200.0f) {
      // PREV (예: Button 6)
      sendPptButtonPulse(6);
      v_lastFlick = millis();
    } else if (p_gzDegPerSec < -200.0f) {
      // NEXT (예: Button 5)
      sendPptButtonPulse(5);
      v_lastFlick = millis();
    }
  }

  static void sensorTask(void* pv) {
    CL_E10_EliteAirMouse* m = (CL_E10_EliteAirMouse*)pv;

    TickType_t v_lastWake = xTaskGetTickCount();
    unsigned long v_lastUs = micros();

    unsigned long v_btnDownMs = 0;

    for (;;) {
      sensors_event_t a, g, temp;
      m->_mpu.getEvent(&a, &g, &temp);

      const unsigned long v_nowUs = micros();
      const float v_dt = (v_nowUs - v_lastUs) / 1000000.0f;
      v_lastUs = v_nowUs;

      // 1) BTN_MODE: short=감도 변경, long=모드 토글
      if (digitalRead(G_E10_BTN_MODE) == LOW) {
        if (v_btnDownMs == 0) v_btnDownMs = millis();
      } else {
        if (v_btnDownMs > 0) {
          const unsigned long v_hold = millis() - v_btnDownMs;
          if (v_hold > 1000) m->togglePptMode();
          else m->cycleDpi();
          v_btnDownMs = 0;
        }
      }

      // 2) 물리 엔진 업데이트
      m->_engine.updateOrientation(a.acceleration.y, a.acceleration.z, g.gyro.x * RAD_TO_DEG, v_dt);

      int v_tx = 0, v_ty = 0;
      // 마우스/커서 느낌을 위해 자이로를 deg/s로 통일하여 전달
      const float v_rawX = -(g.gyro.z * RAD_TO_DEG);
      const float v_rawY = -(g.gyro.x * RAD_TO_DEG);
      m->_engine.process(v_rawX, v_rawY, v_tx, v_ty);

      // 3) PPT 모드 제스처 감지 (gz도 deg/s로 통일)
      if (m->_isPptMode) {
        m->processGesturesDeg(g.gyro.z * RAD_TO_DEG);
      }

      // 4) 클릭 버튼 처리 → Gamepad Button 1
      const bool v_leftClick = (digitalRead(G_E10_BTN_L) == LOW);
      if (v_leftClick) m->_engine.notifyClick();

      // 5) 공유 상태 갱신 (축은 int16_t 범위로 클램프)
      // v_tx/v_ty는 픽셀 느낌의 정수이므로, 조이스틱 범위로 스케일
      // (여기 스케일은 PC 매핑 방식에 따라 튜닝 필요)
      int32_t v_sx = (int32_t)v_tx * 900; // 대략 스케일
      int32_t v_sy = (int32_t)v_ty * 900;

      if (v_sx > 32767) v_sx = 32767;
      if (v_sx < -32767) v_sx = -32767;
      if (v_sy > 32767) v_sy = 32767;
      if (v_sy < -32767) v_sy = -32767;

      if (xSemaphoreTake(m->_mutex, 0) == pdTRUE) {
        m->_state.x = (int16_t)v_sx;
        m->_state.y = (int16_t)v_sy;
        m->_state.updated = true;
        xSemaphoreGive(m->_mutex);
      }

      // 버튼은 센서 태스크에서 즉시 반영(지연 최소화)
      if (m->_gamepad.isConnected()) {
        if (v_leftClick) m->_gamepad.press(1);
        else m->_gamepad.release(1);
      }

      vTaskDelayUntil(&v_lastWake, pdMS_TO_TICKS(8)); // 125Hz
    }
  }

  static void commTask(void* pv) {
    CL_E10_EliteAirMouse* m = (CL_E10_EliteAirMouse*)pv;

    for (;;) {
      if (m->_gamepad.isConnected()) {
        if (xSemaphoreTake(m->_mutex, portMAX_DELAY) == pdTRUE) {
          if (m->_state.updated) {
            // ESP32-BLE-Gamepad: setAxes(x,y,z,rx,ry,rz,slider1,slider2)
            // 여기서는 좌스틱 X/Y만 사용(다른 축은 0)
            m->_gamepad.setAxes(m->_state.x, m->_state.y, 0, 0, 0, 0, 0, 0);
            m->_state.updated = false;
          }
          xSemaphoreGive(m->_mutex);
        }
      }
      vTaskDelay(pdMS_TO_TICKS(7));
    }
  }
};

