// =======================================================
// File: src/v0410/P20_Power_0410.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : P20_Power_0410.h
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
    // ====================================================
    // [Phase 11.6] Config 확장
    // ====================================================
    struct ST_Config_t {
        uint32_t idle_timeout_ms[3];       // Mode 1/2/3
        uint32_t idle_timeout_ble_ms;
        uint32_t pairing_idle_timeout_ms;
        uint32_t deep_idle_timeout_ms;     // 0 = 비활성
        uint16_t wake_min_active_ms;
        uint8_t  wom_threshold;
        uint8_t  wom_duration;
        uint16_t fast_recalib_ms;
    };

    // [I-1] Wake 원인 조회
    enum EN_WakeReason_t : uint8_t {
        EN_WAKE_NONE       = 0,
        EN_WAKE_MPU_MOTION = 1,
        EN_WAKE_BUTTON     = 2,
        EN_WAKE_OTHER      = 3,
    };

    using WakeCallback_t = void (*)(void* p_ctx);

    CL_P20_Power();

    // 부팅 시 1회 (E10.begin): RTC GPIO 풀업 설정
    void begin();

    // Config 반영
    void setConfig(const ST_Config_t& p_cfg);
    void setIdleTimeout(uint32_t p_ms);
    uint32_t getIdleTimeout() const { return _cfg.idle_timeout_ms[0]; }
    uint32_t getIdleTimeout(uint8_t p_mode) const {
        if (p_mode < 1 || p_mode > 3) return 60000;
        return _cfg.idle_timeout_ms[p_mode - 1];
    }
    uint32_t getIdleTimeoutBle() const { return _cfg.idle_timeout_ble_ms; }
    uint32_t getIdleSince() const { return _lastActivityMs; }

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

    // ============================================
    // [N-1] Deep-sleep 진입
    // ============================================
    bool deepSleepNow(uint32_t p_nowMs, bool p_hidConnected, bool p_pairing);

    // ============================================
    // [C-3] Wake 후 콜백 (commTask 깨우기 등)
    // ============================================
    void setWakeCallback(WakeCallback_t p_cb, void* p_ctx) {
        _wakeCb = p_cb; _wakeCtx = p_ctx;
    }

    EN_WakeReason_t getLastWakeReason() const { return _lastWakeReason; }
    uint32_t        getWakeCount() const      { return _wakeCount; }

  private:
    ST_Config_t     _cfg = {};
    uint32_t        _lastActivityMs = 0;
    uint32_t        _lastWakeMs     = 0;
    bool            _idle           = false;
    bool            _ext1Armed      = false;

    EN_WakeReason_t _lastWakeReason = EN_WAKE_NONE;
    uint32_t        _wakeCount      = 0;

    WakeCallback_t  _wakeCb  = nullptr;
    void*           _wakeCtx = nullptr;

    bool _prepareMpuWom();
    void _restoreMpuAfterWake();
    void _armExt1();
};
