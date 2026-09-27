// =======================================================
// File: src/v040/E10_AirMouse_Action_0400.cpp
// =======================================================
#include "E10_AirMouse_0400.h"

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
        // CLICK/DOUBLE/LONG은 슬롯 매핑으로
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
            // Phase 8에서 실제 sleep 진입
            D10_LOGI("[E10] SP_SLEEP_NOW requested");
            break;

        case EN_C20_SP_MODE_CYCLE:
            _setActiveMode((_activeMode % 3) + 1);
            break;

        case EN_C20_SP_PAIRING:
            // Phase 10에서 실제 페어링 진입
            D10_LOGI("[E10] SP_PAIRING requested");
            break;

        case EN_C20_SP_HOST_CYCLE:
            // Phase 9에서 Multi-Host 순환
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

    // FSM 리셋 (다음 프레임부터 새 모드로)
    _precSub = EN_PREC_OFF;
    _precSmX = 0.0f;
    _precSmY = 0.0f;

    // HID 상태 강제 초기화 (안전)
    (void)forceReleaseButtons();

    D10_LOGI("[E10] Mode changed: %u -> %u", (unsigned)v_old, (unsigned)p_newMode);

    // TODO Phase 6-J: LED 색상 전환
    // _led.setModeColor(p_newMode);
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
