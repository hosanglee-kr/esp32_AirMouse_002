// =======================================================
// File: src/v0410/C20_ActionExec_0410.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : C20_ActionExec_0410.h
 * 모듈약어 : C20
 * 모듈명 : Action Executor (v0410)
 * ------------------------------------------------------
 * 기능 요약
 *  - ActionSlot을 HID로 실행 (mouse / kb / consumer / special)
 *  - Hold/Repeat 상태 관리
 *  - 웹 태스크가 아닌 commTask에서만 호출됨 (C-3 원칙)
 * ------------------------------------------------------
 * [구현 규칙]
 *  - ArduinoJson v7.x.x 사용 (v6 이하 사용 금지)
 *  - memset + strlcpy 기반 안전 초기화
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <string.h>

#include <KeyboardDevice.h>
#include <MouseDevice.h>

#include "C20_Action_0410.h"

class CL_C20_ActionExec {
  public:
    // Special action 콜백 (E10이 처리: GyroRecalib, Sleep, ModeCycle 등)
    using SpecialCallback = void (*)(void* p_ctx, uint8_t p_special);

  private:
    MouseDevice*    _mouse    = nullptr;
    KeyboardDevice* _keyboard = nullptr;

    SpecialCallback _specialCb  = nullptr;
    void*           _specialCtx = nullptr;

    // Hold/Repeat 런타임 상태 (슬롯별)
    //  slotId: 고유 식별 (mode*50 + group*20 + idx 정도)
    struct ST_Runtime_t {
        bool     active;
        uint32_t lastRepeatMs;
        uint16_t repeatMs;
        ST_C20_ActionSlot_t slot;   // 활성 슬롯 스냅샷
    };
    static constexpr uint8_t MAX_RUNTIME = 8;
    ST_Runtime_t _rt[MAX_RUNTIME];

    uint32_t _kbTapMs        = 22;
    uint32_t _consumerTapMs  = 28;
    uint32_t _mouseTapMs     = 20;
    uint16_t _repeatDefaultMs = 100;  // 10Hz

  public:
    CL_C20_ActionExec();

    void begin(MouseDevice* p_mouse, KeyboardDevice* p_keyboard);
    void setSpecialCallback(SpecialCallback p_cb, void* p_ctx) {
        _specialCb  = p_cb;
        _specialCtx = p_ctx;
    }

    // ====================================================
    // 슬롯 실행 (commTask에서 호출)
    //  - p_isDown: DOWN 이벤트면 true, UP 이벤트면 false
    //  - tap 계열 슬롯은 p_isDown=true 1회만 호출되면 됨
    // ====================================================
    bool exec(const ST_C20_ActionSlot_t& p_slot, bool p_isDown);

    // 반복 처리 (commTask 루프에서 주기 호출)
    void tickRepeat();

    // 전체 해제 (mode 전환 / sleep 진입 / disconnect)
    void releaseAll();

  private:
    // Hold/Repeat 등록/해제
    int  _findRuntime(const ST_C20_ActionSlot_t& p_slot);
    int  _allocRuntime(const ST_C20_ActionSlot_t& p_slot, uint16_t p_repeatMs);
    void _freeRuntime(int p_idx);

    // Kind별 실행
    void _execMouseClick  (const ST_C20_ActionSlot_t& s);
    void _execMouseHold   (const ST_C20_ActionSlot_t& s, bool isDown);
    void _execMouseWheel  (const ST_C20_ActionSlot_t& s);
    void _execKbTap       (const ST_C20_ActionSlot_t& s);
    void _execKbCombo     (const ST_C20_ActionSlot_t& s);
    void _execKbRepeat    (const ST_C20_ActionSlot_t& s, bool isDown);
    void _execConsumerTap (const ST_C20_ActionSlot_t& s);
    void _execConsumerRep (const ST_C20_ActionSlot_t& s, bool isDown);
    void _execSpecial     (const ST_C20_ActionSlot_t& s);

    // 슬롯 비교 (kind/hold/p16/p32)
    static bool _slotEq(const ST_C20_ActionSlot_t& a,
                        const ST_C20_ActionSlot_t& b) {
        return a.kind == b.kind && a.holdMode == b.holdMode &&
               a.param16 == b.param16 && a.param32 == b.param32;
    }
};