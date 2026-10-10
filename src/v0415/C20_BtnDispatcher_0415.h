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
 *  - resetButton downMs=0 → 현재 시각 (방어적 초기화, L5-A3-11 fix)
 *  - update() 주기 전제조건 명시 (5~10ms)
 *  - LONG/HOLD progressive 발화 정책 명시
 *  - DOUBLE 시 2번째 press의 DOWN 흡수 설계 근거 명시
 *
 * ------------------------------------------------------
 * [v0415 이벤트 발화 정책]
 * ------------------------------------------------------
 *  1. DOWN/UP: 안정 상태 전이 시 발화
 *     - DOWN:  stableState false→true
 *     - UP:    stableState true→false
 *
 *  2. CLICK: release 후 double 윈도우 내 재입력 없을 때
 *     - 타임아웃: _doubleDelayMs (320ms) 경과 시
 *
 *  3. DOUBLE: double 윈도우 내 두 번째 stable DOWN 시
 *     - **2번째 press의 DOWN은 흡수(미발화)하고 DOUBLE 이벤트로 대체**
 *     - 이유: 소비처(예: Top L 기본 슬롯)가 첫 DOWN/UP으로 1회 클릭을
 *             이미 발화했고, DOUBLE 슬롯이 2회째 클릭을 발화함.
 *             만약 2번째 DOWN도 발화하면 클릭 3회(오작동) 발생.
 *     - 이벤트 시퀀스: DOWN(1) → UP(1) → DOUBLE → UP(2)
 *
 *  4. LONG/HOLD_2S/HOLD_3S: **progressive(누적) 발화**
 *     - 각 임계(800ms/2000ms/3000ms) 도달 시각에 1회씩 발화
 *     - fired 플래그로 각 임계당 1회만 발화
 *     - 3초 이상 눌림 시 세 이벤트가 순차 발화
 *     - 소비처:
 *       · Side C 2s: Pairing 진입
 *       · Side C 3s: Host Cycle (진입 시 Pairing 자동 취소 — BB-2)
 *       · Side C 800ms LONG: 명시적 무시 (_handleHardcodedButton)
 *       · 기타 버튼: 슬롯 매핑 없음 → 무시
 *
 * ------------------------------------------------------
 * [v0415 호출 전제조건]
 * ------------------------------------------------------
 *  - update()는 5~10ms 주기로 호출 (sensorTask 8ms 권장)
 *  - update() 주기가 15ms 이상이면 _debounceMinTicks(3)이 시간 조건을
 *    지배하여 실효 디바운스가 늘어남 (역효과)
 *  - update() 주기가 20ms 이상이면 v_timeOk가 32ms를 초과하여
 *    디바운스 무력화 위험
 *  - 콜백(_cb)은 논블로킹 필수 (E10 _onBtnEvent는 _enqueueAction(timeout=0))
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 *   - 클래스명 : CL_모듈약어_ 접두사
 *   - private  : _ 접두사
 *   - 로컬     : v_ 접두사
 *   - 인자     : p_ 접두사
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

    // [v0415 호출 전제] 5~10ms 주기 권장 (sensorTask 8ms)
    void update();

    // --------------------------------------------------
    // [v0415] resetButton 정책 (Phase 5 Q4-b + L5-A3-11)
    // --------------------------------------------------
    //  - phase/downMs/upMs/waitClickStartMs/longFired/hold2sFired/hold3sFired
    //    /stableCount/rawDownMs 초기화
    //  - downMs/upMs/waitClickStartMs = 현재 시각 (0 아님)
    //    · 이유: reset 직후 release edge가 발생하면
    //            heldMs = (rawDownMs>0) ? ... : (now - downMs) 계산에서
    //            downMs=0 → 거대값 → phase가 PRESSED였을 경우 LONG 오발화 가능
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
