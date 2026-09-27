// =======================================================
// File: src/v040/E10_AirMouse_Action_0400.cpp
// =======================================================
#include "E10_AirMouse_0400.h"


// =======================================================
// [Phase 5] 물리 버튼 이벤트 → 슬롯 인덱스 매핑
//   slotIdx = 0..14 (S1..S15)
// =======================================================
namespace {

struct ST_SlotMapEntry_t {
    uint8_t btnId;   // EN_C20_BtnId_t
    uint8_t evt;     // EN_C20_BtnEvent_t
    uint8_t slotIdx; // 0..14
};

// 하드코딩 처리되는 이벤트는 여기서 제외
//   - Top M DOWN/UP (Move Gate)
//   - Top L DOWN/UP/CLICK (Mouse Hold로 소비)
//   - Side C DOUBLE/HOLD_2S/HOLD_3S
constexpr ST_SlotMapEntry_t G_SLOT_MAP[] = {
    // Top L (DOUBLE/LONG만 슬롯. CLICK은 스킵)
    { EN_C20_BTN_TOP_L,  EN_C20_EVT_DOUBLE, 1 },   // S2
    { EN_C20_BTN_TOP_L,  EN_C20_EVT_LONG,   2 },   // S3

    // Top M
    { EN_C20_BTN_TOP_M,  EN_C20_EVT_CLICK,  3 },   // S4

    // Top R
    { EN_C20_BTN_TOP_R,  EN_C20_EVT_CLICK,  5 },   // S6
    { EN_C20_BTN_TOP_R,  EN_C20_EVT_DOUBLE, 6 },   // S7
    { EN_C20_BTN_TOP_R,  EN_C20_EVT_LONG,   7 },   // S8

    // Side F
    { EN_C20_BTN_SIDE_F, EN_C20_EVT_CLICK,  8 },   // S9
    { EN_C20_BTN_SIDE_F, EN_C20_EVT_LONG,   9 },   // S10

    // Side C (CLICK만 슬롯)
    { EN_C20_BTN_SIDE_C, EN_C20_EVT_CLICK,  10},   // S11

    // Side R
    { EN_C20_BTN_SIDE_R, EN_C20_EVT_CLICK,  13},   // S14
    { EN_C20_BTN_SIDE_R, EN_C20_EVT_LONG,   14},   // S15
};

// 자주 쓰는 하드코딩 슬롯 (Top L Hold = 마우스 좌클릭 유지)
constexpr ST_C20_ActionSlot_t G_SLOT_MOUSE_L_HOLD_CONST = {
    (uint8_t)EN_C20_ACT_MOUSE_HOLD,
    (uint8_t)EN_C20_HOLD_PRESS,
    (uint16_t)EN_C20_M_L,
    0
};

}  // namespace

// =======================================================
// 정적 상수 정의
// =======================================================
const ST_C20_ActionSlot_t CL_E10_EliteAirMouse::G_SLOT_MOUSE_L_HOLD =
    G_SLOT_MOUSE_L_HOLD_CONST;

// =======================================================
// 하드코딩 처리 (config 무관)
// =======================================================
bool CL_E10_EliteAirMouse::_handleHardcodedButton(uint8_t p_btnId, uint8_t p_evt) {
    const uint32_t v_now = (uint32_t)millis();

    // ---------- Side C: 모드 전환 / 페어링 / 호스트 순환 ----------
    if (p_btnId == EN_C20_BTN_SIDE_C) {
        if (p_evt == EN_C20_EVT_DOUBLE) {
            const uint8_t v_next = (_activeMode % 3) + 1;
            _setActiveMode(v_next);
            return true;
        }
        if (p_evt == EN_C20_EVT_HOLD_2S) {
            _handleSpecial(EN_C20_SP_PAIRING);
            return true;
        }
        if (p_evt == EN_C20_EVT_HOLD_3S) {
            _handleSpecial(EN_C20_SP_HOST_CYCLE);
            return true;
        }
        // CLICK은 슬롯 매핑으로 위임
        return false;
    }

    // ---------- Top M: Move Gate (Hold) ----------
    if (p_btnId == EN_C20_BTN_TOP_M) {
        if (p_evt == EN_C20_EVT_DOWN) {
            _moveGateHeld = true;
            _topMDownMs   = v_now;
            return true;
        }
        if (p_evt == EN_C20_EVT_UP) {
            _moveGateHeld = false;

            // Mode 3: 짧은 클릭(< 800ms)은 Enter (즉시, 딜레이 없음)
            if (_activeMode == 3) {
                const uint32_t v_held = v_now - _topMDownMs;
                if (v_held < 800u) {
                    (void)_enqueueAction(C20_MakeKbTap(EN_C20_MOD_NONE, EN_C20_KB_ENTER), true);
                }
            }
            // Mode 1/2는 CLICK 이벤트가 S4로 라우팅됨
            return true;
        }
        // DOWN/UP 이외 이벤트(LONG 등)는 무시
        return true;
    }

    // ---------- Top L: 항상 마우스 좌클릭 (고정) ----------
    if (p_btnId == EN_C20_BTN_TOP_L) {
        if (p_evt == EN_C20_EVT_DOWN) {
            (void)_enqueueAction(G_SLOT_MOUSE_L_HOLD, true);
            return true;
        }
        if (p_evt == EN_C20_EVT_UP) {
            (void)_enqueueAction(G_SLOT_MOUSE_L_HOLD, false);
            return true;
        }
        
        // [Phase 5] CLICK은 이미 DOWN/UP(Hold)로 소비됨 → 중복 방지 스킵
        if (p_evt == EN_C20_EVT_CLICK) return true;
    
        // DOUBLE / LONG만 슬롯 매핑
        return false;
    }

    // 그 외는 슬롯 매핑
    return false;
}

// =======================================================
// 슬롯 매핑 처리 (config)
// =======================================================
void CL_E10_EliteAirMouse::_handleSlotButton(uint8_t p_btnId, uint8_t p_evt) {
    for (const auto& e : G_SLOT_MAP) {
        if (e.btnId != p_btnId) continue;
        if (e.evt   != p_evt)   continue;

        // config에서 슬롯 조회
        ST_C20_ActionSlot_t v_slot;
        _lock();
        v_slot = _cfgE10Runtime.modes[_activeMode - 1].slots[e.slotIdx];
        _unlock();

        if (v_slot.kind == (uint8_t)EN_C20_ACT_NONE) return;

        // 이벤트 종류 → isDown 판정
        //   DOWN/UP은 그대로, 그 외(CLICK/DOUBLE/LONG)는 isDown=true로 1회
        bool v_isDown = true;
        if (p_evt == EN_C20_EVT_UP) v_isDown = false;

        (void)_enqueueAction(v_slot, v_isDown);
        return;
    }
}

// =======================================================
// 통합 콜백
// =======================================================
void CL_E10_EliteAirMouse::_onBtnEvent(void* p_ctx, uint8_t p_btnId, uint8_t p_evt) {
    auto* v_m = (CL_E10_EliteAirMouse*)p_ctx;
    if (!v_m) return;

    // 1) 하드코딩 먼저
    if (v_m->_handleHardcodedButton(p_btnId, p_evt)) return;

    // 2) 슬롯 매핑
    v_m->_handleSlotButton(p_btnId, p_evt);
}

// =======================================================
// 특수 액션
// =======================================================
void CL_E10_EliteAirMouse::_handleSpecial(uint8_t p_special) {
    switch ((EN_C20_Special_t)p_special) {
        case EN_C20_SP_GYRO_RECALIB:
            requestGyroCalibration();
            break;

        case EN_C20_SP_SLEEP_NOW:
            // Phase 8에서 실제 sleep 진입. 지금은 LED만 정리.
            _led.off();
            D10_LOGI("[E10] SP_SLEEP_NOW requested");
            break;
    

        case EN_C20_SP_MODE_CYCLE:
            _setActiveMode((_activeMode % 3) + 1);
            break;

        case EN_C20_SP_PAIRING:
            // Phase 10에서 실제 페어링 진입. 지금은 LED 표시만.
            _led.blink(_led.getBaseColor(), 1000, 0);   // 현재 Mode 색 1Hz 무한
            D10_LOGI("[E10] SP_PAIRING requested");
            break;
        
        case EN_C20_SP_HOST_CYCLE:
            // Phase 9에서 Multi-Host 순환. 지금은 LED 표시만.
            _led.flash(EN_L10_COLOR_WHITE, 500);
            D10_LOGI("[E10] SP_HOST_CYCLE requested");
            break;
    

        default:
            break;
    }
}

void CL_E10_EliteAirMouse::_onSpecial(void* p_ctx, uint8_t p_special) {
    auto* v_m = (CL_E10_EliteAirMouse*)p_ctx;
    if (!v_m) return;
    v_m->_handleSpecial(p_special);
}

// =======================================================
// Mode 전환
// =======================================================
void CL_E10_EliteAirMouse::_setActiveMode(uint8_t p_newMode) {
    if (p_newMode < 1 || p_newMode > C10_DEF::MODE_COUNT) return;

    _lock();
    const uint8_t v_old = _activeMode;
    _activeMode = p_newMode;

    // config에도 반영 (다음 부팅 시 복원)
    _cfgE10Runtime.active_mode = p_newMode;
    _unlock();

    if (v_old == p_newMode) return;

    // 진행 중 액션 전부 해제 (stuck 방지)
    _actExec.releaseAll();

    // 디스패처 상태 리셋 (클릭 대기 등)
    _btnDisp.resetAll();
    
    // [Phase 7] 제스처 상태 리셋 (Mode별 슬롯이 다르므로 잔여 상태 제거)
    _gesture.reset();

    // FSM 리셋 (다음 프레임부터 새 모드로)
    _precSub = EN_PREC_OFF;
    _precSmX = 0.0f;
    _precSmY = 0.0f;

    // HID 상태 강제 초기화 (안전)
    (void)forceReleaseButtons();

    D10_LOGI("[E10] Mode changed: %u -> %u", (unsigned)v_old, (unsigned)p_newMode);

    // LED: 새 모드 색 + 0.5초 흰색 flash
    _led.setModeColor(p_newMode);
    _led.flash(EN_L10_COLOR_WHITE, 500);

}

// =======================================================
// Action 큐 enqueue (sensorTask → commTask)
// =======================================================
bool CL_E10_EliteAirMouse::_enqueueAction(const ST_C20_ActionSlot_t& p_slot, bool p_isDown) {
    if (!_qActionExec) return false;

    ST_ActionCmd_t v_cmd;
    v_cmd.slot   = p_slot;
    v_cmd.isDown = p_isDown;

    // timeout=0: sensorTask 블로킹 금지
    return (xQueueSend(_qActionExec, &v_cmd, 0) == pdTRUE);
}


// =======================================================
// [Phase 7] 제스처 슬롯 발동
//   group: 0=flick, 1=linear, 2=tilt
//   dir  : EN_M30_Dir_t (0=LEFT, 1=RIGHT, 2=UP, 3=DOWN)
// =======================================================
void CL_E10_EliteAirMouse::_handleGesture(uint8_t p_group, uint8_t p_dir) {
    if (p_dir > 3) return;

    ST_C20_ActionSlot_t v_slot;
    memset(&v_slot, 0, sizeof(v_slot));

    _lock();
    const uint8_t v_mode = _activeMode;
    if (!_cfgE10RuntimeValid || v_mode < 1 || v_mode > C10_DEF::MODE_COUNT) {
        _unlock();
        return;
    }

    const ST_C10_ModeConfig_t& v_m = _cfgE10Runtime.modes[v_mode - 1];

    switch (p_group) {
        case 0: v_slot = v_m.flick [p_dir]; break;
        case 1: v_slot = v_m.linear[p_dir]; break;
        case 2: v_slot = v_m.tilt  [p_dir]; break;
        default: _unlock(); return;
    }
    _unlock();

    if (v_slot.kind == (uint8_t)EN_C20_ACT_NONE) return;

    // 제스처는 단발 (tap) — isDown=true 로 1회 enqueue
    (void)_enqueueAction(v_slot, true);
}
