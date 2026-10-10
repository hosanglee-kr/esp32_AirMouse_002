// File: src/v0415/P20_Power_0415.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : P20_Power_0415.h
 * 모듈약어 : P20
 * 모듈명 : Power Manager (Light-sleep + WoM)
 * ------------------------------------------------------
 * 기능 요약
 *  - Idle 추적: 마지막 활동 시각 기록
 *  - 미연결 + idle 초과 → Light-sleep (EXT1 wake)
 *  - MPU6050 WoM(Motion Detection) 인터럽트 구성
 *  - EXT1 wake 소스: HW_Def의 마스크 빌더로 결정
 *  - Wake 후 INT_STATUS 래치 클리어
 *
 * [v0415 주요 변경]
 *  - Safe/Pairing Wake Mask 분기 (SPEC rev8, Phase 1 방안 2-A)
 *    · sleepNow(now, safeOrPairing)
 *      - false → buildWakeMaskNormal() (MPU + 6버튼)
 *      - true  → buildWakeMaskSafe()   (Side C 단독)
 *    · deepSleepNow(now, hid, pairing, safe)
 *      - pairing → sleep 금지 (기존 정책)
 *      - safe    → buildWakeMaskSafe()
 *      - normal  → buildWakeMaskButtons() (6버튼, MPU 제외)
 *  - _armExt1(bool) 파라미터화
 *  - fast_recalib_ms 필드 삭제 (E10 config 단독 SSOT, Phase 4 Q3-a)
 *  - _lastWakeReason / _wakeCount / EN_WakeReason_t 삭제 (Phase 4 Q5-b)
 *  - _idle Dead 멤버 삭제 (Phase 4 L4-A4-06)
 *  - _restoreMpuAfterWake: UNDEFINED 경로 가드 (L4-A3-13)
 *  - deepSleepNow: unreachable return 정리 (L4-A3-11)
 *
 * [안전 규칙]
 *  - sleep_idle_timeout_ms == 0 → 완전 비활성
 *  - BLE 연결 중에는 실제 sleep 금지 (LED off만)
 *  - Move Gate/Top M Hold 중에는 idle 판정만 하고 sleep 진입 금지
 *  - ESP32 deep-sleep 아님 (RAM 보존) — Light-sleep만
 *
 * [SSOT 계약]
 *  - Wake 마스크는 HW_Def_0415.h의 빌더 함수만 사용
 *  - 로컬 마스크 계산 금지 (CONTRACT rev7 §"HW SSOT")
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include <driver/gpio.h>

class CL_P20_Power {
  public:
    // ====================================================
    // [Phase 11.6] Config (fast_recalib_ms 삭제됨)
    // ----------------------------------------------------
    //   fast_recalib_ms는 E10 config(ST_C10_PowerConfig_t) 단독 SSOT.
    //   E10_Task_0415.cpp가 _biasTracker.startFastRecalibrate()에 직접 전달.
    // ====================================================
    struct ST_Config_t {
        uint32_t idle_timeout_ms[3];       // Mode 1/2/3
        uint32_t idle_timeout_ble_ms;
        uint32_t pairing_idle_timeout_ms;
        uint32_t deep_idle_timeout_ms;     // 0 = 비활성
        uint16_t wake_min_active_ms;
        uint8_t  wom_threshold;
        uint8_t  wom_duration;
        // [v0415 삭제] uint16_t fast_recalib_ms; — E10 config 단독
    };

    using WakeCallback_t = void (*)(void* p_ctx);

    CL_P20_Power();

    // 부팅 시 1회 (E10.begin): RTC GPIO 풀업 설정
    void begin();

    // Config 반영
    void setConfig(const ST_Config_t& p_cfg);

    uint32_t getIdleTimeout(uint8_t p_mode) const {
        if (p_mode < 1 || p_mode > 3) return 60000;
        return _cfg.idle_timeout_ms[p_mode - 1];
    }

    uint32_t getIdleSince() const { return _lastActivityMs; }

    // 활동 알림 (버튼 DOWN, 커서 이동, Move Gate Held 등)
    void notifyActivity(uint32_t p_nowMs);

    // ============================================
    // 실제 Light-sleep 진입
    //   - p_safeOrPairing=true 시 Safe 마스크 (Side C 단독)
    //   - 성공 시 wake 후 리턴 (호출자는 계속 실행)
    //   - 실패/미충족 시 즉시 리턴
    //   - 반드시 sensorTask에서만 호출 (MPU/Wire 단일 소유)
    // ============================================
    bool sleepNow(uint32_t p_nowMs, bool p_safeOrPairing);

    // ============================================
    // [N-1] Deep-sleep 진입
    //   - p_pairing=true 시 sleep 금지 (기존 정책)
    //   - p_safe=true  시 Safe 마스크 (Side C 단독)
    //   - 그 외       → Buttons 마스크 (MPU 제외)
    // ============================================
    bool deepSleepNow(uint32_t p_nowMs,
                      bool p_hidConnected,
                      bool p_pairing,
                      bool p_safe);

    // ============================================
    // [C-3] Wake 후 콜백 (commTask 깨우기 등)
    // ============================================
    void setWakeCallback(WakeCallback_t p_cb, void* p_ctx) {
        _wakeCb = p_cb; _wakeCtx = p_ctx;
    }

  private:
    ST_Config_t     _cfg = {};
    uint32_t        _lastActivityMs = 0;
    uint32_t        _lastWakeMs     = 0;
    bool            _ext1Armed      = false;
    // [v0415 삭제] bool _idle; — Dead
    // [v0415 삭제] EN_WakeReason_t _lastWakeReason;
    // [v0415 삭제] uint32_t _wakeCount;

    WakeCallback_t  _wakeCb  = nullptr;
    void*           _wakeCtx = nullptr;

    bool _prepareMpuWom();
    void _restoreMpuAfterWake(bool p_sleepOccurred);
    void _armExt1(bool p_safeOrPairing);
};
