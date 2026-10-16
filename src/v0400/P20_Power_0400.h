// =======================================================
// File: src/v040/P20_Power_0400.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : P20_Power_0400.h
 * 모듈약어 : P20
 * 모듈명 : Power Manager (Light-sleep + WoM)
 * ------------------------------------------------------
 * 기능 요약
 *  - Idle 추적: 마지막 활동 시각 기록
 *  - 미연결 + idle 초과 → Light-sleep (EXT1 wake)
 *  - MPU6050 WoM(Motion Detection) 인터럽트 구성
 *  - EXT1 wake 소스: MPU INT + 6버튼 (모두 LOW active)
 *  - Wake 후 INT_STATUS 래치 클리어
 *
 * [안전 규칙]
 *  - sleep_idle_timeout_ms == 0 → 완전 비활성
 *  - BLE 연결 중에는 실제 sleep 금지 (LED off만)
 *  - Move Gate/Top M Hold 중에는 idle 판정만 하고 sleep 진입 금지
 *  - ESP32 deep-sleep 아님 (RAM 보존)
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 *   - 클래스명 : CL_모듈약어_ 접두사
 *   - private  : _ 접두사
 *   - 로컬     : v_ 접두사
 *   - 인자     : p_ 접두사
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include <driver/gpio.h>

class CL_P20_Power {
  public:
    CL_P20_Power();

    // 부팅 시 1회 (E10.begin): RTC GPIO 풀업 설정
    void begin();

    // Config 반영
    void setIdleTimeout(uint32_t p_ms) { _idleTimeoutMs = p_ms; }
    uint32_t getIdleTimeout() const { return _idleTimeoutMs; }

    // 활동 알림 (버튼 DOWN, 커서 이동, Move Gate Held 등)
    void notifyActivity(uint32_t p_nowMs);

    bool isIdle() const { return _idle; }

    // ============================================
    // 실제 Light-sleep 진입
    //   - 성공 시 wake 후 리턴 (호출자는 계속 실행)
    //   - 실패/미충족 시 즉시 리턴
    //   - 반드시 sensorTask에서만 호출 (MPU/Wire 단일 소유)
    // ============================================
    bool sleepNow(uint32_t p_nowMs);

  private:
    uint32_t _lastActivityMs = 0;
    uint32_t _idleTimeoutMs  = 60000;
    bool     _idle           = false;
    bool     _ext1Armed      = false;

    bool _prepareMpuWom();
    void _restoreMpuAfterWake();
    void _armExt1();
};
