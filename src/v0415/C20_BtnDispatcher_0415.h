// =======================================================
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
 * - 6개 물리 버튼의 상태머신
 * - 이벤트 발생: DOWN / UP / CLICK / DOUBLE / LONG / HOLD_2S / HOLD_3S
 * - 디바운스 (20ms)
 * - Long 임계(800ms), Double 윈도우(300ms), Hold2s(2000ms), Hold3s(3000ms)
 * - 콜백 기반 (HID 없음; 순수 상태머신)
 * ------------------------------------------------------
 * [구현 규칙]
 * - ArduinoJson v7.x.x 사용 (v6 이하 사용 금지)
 * - memset + strlcpy 기반 안전 초기화
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 * - 클래스명 : CL_모듈약어_ 접두사
 * - 클래스 private 멤버 : _ 접두사
 * - 클래스 정적 멤버 : s_ 접두사
 * - 함수 로컬 변수 : v_ 접두사
 * - 함수 인자 : p_ 접두사
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <string.h>

#include "C20_Action_0415.h"
#include "HW_Def_0415.h"

class CL_C20_BtnDispatcher {
  public:
    // 이벤트 콜백: (ctx, btnId, evt)
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
        uint8_t    stableCount; // [C-3] 연속 동일 raw 카운트
        uint32_t   rawDownMs;   // [I-1, I-3] 최초 물리적 DOWN 시점
    };

    ST_BtnState_t _btn[EN_C20_BTN_MAX];
    EventCallback _cb  = nullptr;
    void*         _ctx = nullptr;

    // 타이밍 (ms / ticks)
    uint16_t _debouncePressMs   = 32;  // [C-1, C-4]
    uint16_t _debounceReleaseMs = 16;  // [C-4]
    uint16_t _longDelayMs       = 800; // [I-1]
    uint16_t _doubleDelayMs     = 320; // [I-2]
    uint16_t _hold2sMs          = 2000;
    uint16_t _hold3sMs          = 3000;
    uint16_t _minClickMs        = 16; // [I-3]
    uint8_t  _debounceMinTicks  = 3;  // [C-3]

  public:
    CL_C20_BtnDispatcher();

    // 초기화: pinMode + state 초기화
    void begin();

    // 콜백 등록
    void setCallback(EventCallback p_cb, void* p_ctx) {
        _cb  = p_cb;
        _ctx = p_ctx;
    }

    // 매 프레임 호출 (sensorTask)
    void update();

    // Mode 전환 등에서 상태 리셋 (진행 중 hold 정리)
    void resetButton(uint8_t p_btnId);
    void resetAll();

    // [Phase 11.7] 타이밍 조정 (config 연동)
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

    // 구버전 오버로드 (호환성 유지)
    void setTimings(uint16_t p_debounce, uint16_t p_long, uint16_t p_dbl, uint16_t p_hold2s, uint16_t p_hold3s) {
        setTimings(p_debounce, p_debounce / 2, p_long, p_dbl, p_hold2s, p_hold3s, 16, 3);
    }

  private:
    void _updateOne(uint8_t p_btnId, uint32_t p_now);
    void _onStableChange(uint8_t p_btnId, bool p_stable, uint32_t p_now);
    void _checkTimers(uint8_t p_btnId, uint32_t p_now);
    void _emit(uint8_t p_btnId, uint8_t p_evt);
};