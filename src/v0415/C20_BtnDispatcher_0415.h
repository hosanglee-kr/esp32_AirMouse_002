// File: src/v0415/C20_BtnDispatcher_0415.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : C20_BtnDispatcher_0415.h
 * 모듈약어 : C20
 * 모듈명 : Button Event Dispatcher (v0415)
 * ------------------------------------------------------
 * 기능 요약
 *  - 6개 물리 버튼의 상태머신
 *  - 이벤트: DOWN / UP / CLICK / DOUBLE / LONG / HOLD_2S / HOLD_3S
 *  - 비대칭 디바운스 (Press 32ms / Release 16ms)
 *  - 2-Stage 하이브리드 판정 (시간 + 카운터)
 *  - 콜백 기반 (HID 없음; 순수 상태머신)
 *
 * [v0415 주요 변경]
 *  - setTimings 구버전 5-param 오버로드 삭제 (Phase 5 Q6-a)
 *    · 사용처 없음 확인 (E10 Core만 8-param 사용)
 *  - resetButton stableState/lastRaw 유지 정책 주석 강화 (Phase 5 Q4-b)
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <string.h>

#include "C20_Action_0415.h"
#include "HW_Def_0415.h"

class CL_C20_BtnDispatcher {
  public:
    using EventCallback = void (*)(void* p_ctx, uint8_t p_btnId, uint8_t p_evt);

    // GPIO 핀 (EN_C20_BtnId_t 순서와 1:1, HW_DEF SSOT 참조)
    static constexpr int G_PINS[EN_C20_BTN_MAX] = {
        HW_DEF::PIN_BTN_TOP_L,
        HW_DEF::PIN_BTN_TOP_M,
        HW_DEF::PIN_BTN_TOP_R,
        HW_DEF::PIN_BTN_SIDE_F,
        HW_DEF::PIN_BTN_SIDE_C,
        HW_DEF::PIN_BTN_SIDE_R
    };

  private:
    enum EN_Phase_t : uint8_t { PHASE_IDLE = 0, PHASE_PRESSED, PHASE_WAIT_CLICK, PHASE_DOUBLE };

    struct ST_BtnState_t {
        EN_Phase_t phase;
        uint32_t   downMs;
        uint32_t   upMs;
        uint32_t   waitClickStartMs;
        bool       longFired;
        bool       hold2sFired;
        bool       hold3sFired;
        // Debounce
        bool       stableState;
        bool       lastRaw;
        uint32_t   lastRawChangeMs;
        uint8_t    stableCount;
        uint32_t   rawDownMs;
    };

    ST_BtnState_t _btn[EN_C20_BTN_MAX];
    EventCallback _cb  = nullptr;
    void*         _ctx = nullptr;

    // 타이밍 (config 연동)
    uint16_t _debouncePressMs   = 32;
    uint16_t _debounceReleaseMs = 16;
    uint16_t _longDelayMs       = 800;
    uint16_t _doubleDelayMs     = 320;
    uint16_t _hold2sMs          = 2000;
    uint16_t _hold3sMs          = 3000;
    uint16_t _minClickMs        = 16;
    uint8_t  _debounceMinTicks  = 3;

  public:
    CL_C20_BtnDispatcher();

    void begin();

    void setCallback(EventCallback p_cb, void* p_ctx) {
        _cb  = p_cb;
        _ctx = p_ctx;
    }

    void update();

    // --------------------------------------------------
    // [v0415] resetButton 정책 (Phase 5 Q4-b)
    // --------------------------------------------------
    //  - phase/downMs/upMs/waitClickStartMs/longFired/hold2sFired/hold3sFired
    //    /stableCount/rawDownMs 초기화
    //  - stableState/lastRaw는 물리 상태 반영을 위해 유지
    //    (프로파일 전환 중 버튼 눌림 상태가 이어지면 debounce 정상 동작)
    //  - lastRawChangeMs는 현재 시각으로 갱신 (타임아웃 즉시 발화 방지)
    void resetButton(uint8_t p_btnId);
    void resetAll();

    // --------------------------------------------------
    // [v0415] 타이밍 조정 (8-param 단일)
    //   구버전 5-param 오버로드 삭제 (Phase 5 Q6-a)
    // --------------------------------------------------
    void setTimings(uint16_t p_press,
                    uint16_t p_release,
                    uint16_t p_long,
                    uint16_t p_dbl,
                    uint16_t p_hold2s,
                    uint16_t p_hold3s,
                    uint16_t p_minClick,
                    uint8_t  p_minTicks) {
        _debouncePressMs   = (p_press < 8) ? 8 : p_press;
        _debounceReleaseMs = (p_release < 8) ? 8 : p_release;
        _longDelayMs       = p_long;
        _doubleDelayMs     = p_dbl;
        _hold2sMs          = p_hold2s;
        _hold3sMs          = p_hold3s;
        _minClickMs        = p_minClick;
        _debounceMinTicks  = (p_minTicks < 1) ? 1 : p_minTicks;
    }

  private:
    void _updateOne(uint8_t p_btnId, uint32_t p_now);
    void _onStableChange(uint8_t p_btnId, bool p_stable, uint32_t p_now);
    void _checkTimers(uint8_t p_btnId, uint32_t p_now);
    void _emit(uint8_t p_btnId, uint8_t p_evt);
};
