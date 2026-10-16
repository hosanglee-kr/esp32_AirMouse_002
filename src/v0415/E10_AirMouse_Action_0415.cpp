// =======================================================
// File: src/v0415/E10_AirMouse_Action_0415.cpp
// =======================================================
#include "E10_AirMouse_0415.h"


// =======================================================
// [Phase 5] 물리 버튼 이벤트 → 슬롯 인덱스 매핑
// =======================================================
namespace {

struct ST_SlotMapEntry_t {
    uint8_t btnId;
    uint8_t evt;
    uint8_t trig;
};

// 하드코딩 처리되는 이벤트는 여기서 제외
constexpr ST_SlotMapEntry_t G_SLOT_MAP[] = {
    // Top L (DOUBLE/LONG만)
    { EN_C20_BTN_TOP_L,  EN_C20_EVT_DOUBLE, EN_C10_TRIG_TOP_L_DOUBLE },
    { EN_C20_BTN_TOP_L,  EN_C20_EVT_LONG,   EN_C10_TRIG_TOP_L_LONG   },

    // Top M
    { EN_C20_BTN_TOP_M,  EN_C20_EVT_CLICK,  EN_C10_TRIG_TOP_M_CLICK  },

    // Top R
    { EN_C20_BTN_TOP_R,  EN_C20_EVT_CLICK,  EN_C10_TRIG_TOP_R_CLICK  },
    { EN_C20_BTN_TOP_R,  EN_C20_EVT_DOUBLE, EN_C10_TRIG_TOP_R_DOUBLE },
    { EN_C20_BTN_TOP_R,  EN_C20_EVT_LONG,   EN_C10_TRIG_TOP_R_LONG   },

    // Side F
    { EN_C20_BTN_SIDE_F, EN_C20_EVT_CLICK,  EN_C10_TRIG_SIDE_F_CLICK },
    { EN_C20_BTN_SIDE_F, EN_C20_EVT_LONG,   EN_C10_TRIG_SIDE_F_LONG  },

    // Side C (CLICK만)
    { EN_C20_BTN_SIDE_C, EN_C20_EVT_CLICK,  EN_C10_TRIG_SIDE_C_CLICK },

    // Side R
    { EN_C20_BTN_SIDE_R, EN_C20_EVT_CLICK,  EN_C10_TRIG_SIDE_R_CLICK },
    { EN_C20_BTN_SIDE_R, EN_C20_EVT_LONG,   EN_C10_TRIG_SIDE_R_LONG  },
};

// 자주 쓰는 하드코딩 슬롯 (Top L Hold = 마우스 좌클릭 유지)
constexpr ST_C20_ActionSlot_t G_SLOT_MOUSE_L_HOLD_CONST = {
    (uint8_t)EN_C20_ACT_MOUSE_HOLD,
    (uint8_t)EN_C20_HOLD_PRESS,
    (uint16_t)EN_C20_M_L,
    0
};

// =======================================================
// [v0415] Momentary 액션 sanitization (BB-1)
// =======================================================
void _sanitizeMomentarySlot(ST_C20_ActionSlot_t& p_slot) {
    if (p_slot.kind == (uint8_t)EN_C20_ACT_MOUSE_HOLD) {
        p_slot.kind     = (uint8_t)EN_C20_ACT_MOUSE_CLICK;
        p_slot.holdMode = (uint8_t)EN_C20_HOLD_NONE;
    } else if (p_slot.kind == (uint8_t)EN_C20_ACT_KB_REPEAT) {
        p_slot.kind     = (uint8_t)EN_C20_ACT_KB_TAP;
        p_slot.holdMode = (uint8_t)EN_C20_HOLD_NONE;
    } else if (p_slot.kind == (uint8_t)EN_C20_ACT_CONSUMER_REPEAT) {
        p_slot.kind     = (uint8_t)EN_C20_ACT_CONSUMER_TAP;
        p_slot.holdMode = (uint8_t)EN_C20_HOLD_NONE;
    }
}

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

    // ---------- Side C ----------
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
        if (p_evt == EN_C20_EVT_LONG) {
            // Phase 6 L6g-A1-01: 무시 (의도적)
            return true;
        }
        return false;   // CLICK → 슬롯 매핑 위임
    }

    // ---------- Top M: Move Gate ----------
    if (p_btnId == EN_C20_BTN_TOP_M) {
        if (p_evt == EN_C20_EVT_DOWN) {
            _moveGateHeld = true;
            return true;
        }
        if (p_evt == EN_C20_EVT_UP) {
            _moveGateHeld = false;
            return true;
        }
        return false;
    }

    // ---------- Top L: 항상 마우스 좌클릭 ----------
    if (p_btnId == EN_C20_BTN_TOP_L) {
        if (p_evt == EN_C20_EVT_DOWN) {
            _btnLDown = true;
            (void)_enqueueAction(G_SLOT_MOUSE_L_HOLD, true);
            return true;
        }
        if (p_evt == EN_C20_EVT_UP) {
            _btnLDown = false;
            (void)_enqueueAction(G_SLOT_MOUSE_L_HOLD, false);
            return true;
        }
        if (p_evt == EN_C20_EVT_CLICK) return true;
        return false;
    }

    // ---------- Side F: Front Hold ----------
    if (p_btnId == EN_C20_BTN_SIDE_F) {
        if (p_evt == EN_C20_EVT_DOWN) {
            _frontHoldActive = true;
            return true;
        }
        if (p_evt == EN_C20_EVT_UP) {
            _frontHoldActive = false;
            return true;
        }
        return false;
    }

    return false;
}

// =======================================================
// 슬롯 매핑 처리
// =======================================================
void CL_E10_EliteAirMouse::_handleSlotButton(uint8_t p_btnId, uint8_t p_evt) {
    for (const auto& e : G_SLOT_MAP) {
        if (e.btnId != p_btnId) continue;
        if (e.evt   != p_evt)   continue;

        ST_C20_ActionSlot_t v_slot = _resolveSlot(_activeMode, e.trig);

        if (v_slot.kind == (uint8_t)EN_C20_ACT_NONE) return;

        if (v_slot.kind == (uint8_t)EN_C20_ACT_SPECIAL) {
            _handleSpecial((uint8_t)v_slot.param16);
            return;
        }

        _sanitizeMomentarySlot(v_slot);
        (void)_enqueueAction(v_slot, true);
        return;
    }
}

// =======================================================
// 통합 콜백
// =======================================================
void CL_E10_EliteAirMouse::_onBtnEvent(void* p_ctx, uint8_t p_btnId, uint8_t p_evt) {
    auto* v_m = (CL_E10_EliteAirMouse*)p_ctx;
    if (!v_m) return;

    v_m->_power.notifyActivity((uint32_t)millis());

    if (v_m->_handleHardcodedButton(p_btnId, p_evt)) return;
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

        // ============================================================
        // SLEEP_NOW 즉시 진입
        // ============================================================
        case EN_C20_SP_SLEEP_NOW: {
            CL_L10_Led::ST_LedSnapshot_t v_snap;
            _led.suspend(v_snap);

            const bool v_safeOrPairing = (_safeMode || _ble.isPairing());
            const bool v_didSleep = _power.sleepNow((uint32_t)millis(), v_safeOrPairing);

            _led.resume(v_snap);

            if (v_didSleep && _cfgProfileValid) {
                _biasTracker.startFastRecalibrate(
                    _cfgProfile.e10.power.fast_recalib_ms);
            }

            D10_LOGI("[E10] SP_SLEEP_NOW: didSleep=%d safeOrPair=%d",
                     (int)v_didSleep, (int)v_safeOrPairing);
            break;
        }

        case EN_C20_SP_MODE_CYCLE:
            _setActiveMode((_activeMode % 3) + 1);
            break;

        case EN_C20_SP_PAIRING: {
            _ble.enterPairing(30000);
            _led.blink(_led.getBaseColor(), 1000, 0);
            D10_LOGI("[E10] SP_PAIRING → pairing mode");
            break;
        }

        case EN_C20_SP_HOST_CYCLE: {
            _ble.exitPairing();

            const uint8_t v_idx = _ble.cycleActivePeer();
            _led.flash(EN_L10_COLOR_WHITE, 500);

            D10_LOGI("[E10] SP_HOST_CYCLE → target=%u bond=%u",
                     (unsigned)v_idx,
                     (unsigned)_ble.getBondCount());
            break;
        }

        // ============================================================
        // [v0415 Phase 1-4] HOST_SELECT 0/1/2 통합 (fallthrough)
        //   idx = p_special - EN_C20_SP_HOST_SELECT_0
        //   전제: SELECT_0=6, SELECT_1=7, SELECT_2=8 (연속값)
        // ============================================================
        case EN_C20_SP_HOST_SELECT_0:
        case EN_C20_SP_HOST_SELECT_1:
        case EN_C20_SP_HOST_SELECT_2: {
            const uint8_t v_idx = (uint8_t)(p_special - EN_C20_SP_HOST_SELECT_0);
            _ble.exitPairing();
            _ble.selectHostIdx(v_idx, B20_HOST::CONN_TIMEOUT_MS);
            _led.flash(EN_L10_COLOR_WHITE, 300);
            D10_LOGI("[E10] SP_HOST_SELECT_%u", (unsigned)v_idx);
            break;
        }

        case EN_C20_SP_HOST_TOGGLE: {
            _ble.exitPairing();
            uint8_t v_next = (_ble.getActivePeerIndex() == 0) ? 1 : 0;
            _ble.selectHostIdx(v_next, B20_HOST::CONN_TIMEOUT_MS);
            _led.flash(EN_L10_COLOR_WHITE, 300);
            D10_LOGI("[E10] SP_HOST_TOGGLE -> target=%u", (unsigned)v_next);
            break;
        }

        case EN_C20_SP_HOST_CANCEL: {
            _ble.cancelSelect();
            _led.flash(EN_L10_COLOR_RED, 300);
            D10_LOGI("[E10] SP_HOST_CANCEL");
            break;
        }

        default:
            break;
    }
}

// =======================================================
// Mode 전환
// =======================================================
void CL_E10_EliteAirMouse::_setActiveMode(uint8_t p_newMode) {
    if (p_newMode < 1 || p_newMode > C10_DEF::MODE_COUNT) return;

    _power.notifyActivity((uint32_t)millis());

    // =====================================================
    // [v0415 Phase 1-5] 매크로 취소 (SSOT 헬퍼)
    // =====================================================
    _abortMacroLocked();

    const uint8_t v_old = _activeMode;

    _lock();
    _activeMode = p_newMode;
    if (_cfgProfileValid) {
        _cfgProfile.e10.active_mode = p_newMode;
    }
    _unlock();

    if (v_old == p_newMode) return;

    _btnDisp.resetAll();
    _gesture.reset();

    _frontHoldActive = false;

    if (_ble.isPairing()) {
        _ble.exitPairing();
        _led.setModeColor(p_newMode);
    }

    _precSub = EN_PREC_OFF;
    _resetSnapState();

    _precSmX = 0.0f;
    _precSmY = 0.0f;

    (void)forceReleaseButtons();

    _engine.resetEmaState();

    D10_LOGI("[E10] Mode changed: %u -> %u", (unsigned)v_old, (unsigned)p_newMode);

    _led.setModeColor(p_newMode);
    _led.flash(EN_L10_COLOR_WHITE, 500);
}

// =======================================================
// Action 큐 enqueue
// =======================================================
bool CL_E10_EliteAirMouse::_enqueueAction(const ST_C20_ActionSlot_t& p_slot, bool p_isDown) {
    if (!_qActionExec) return false;

    ST_ActionCmd_t v_cmd;
    v_cmd.slot   = p_slot;
    v_cmd.isDown = p_isDown;

    return (xQueueSend(_qActionExec, &v_cmd, 0) == pdTRUE);
}

// =======================================================
// [Phase 7] 제스처 슬롯 발동
// =======================================================
void CL_E10_EliteAirMouse::_handleGesture(uint8_t p_group, uint8_t p_dir) {
    if (p_dir > 3) return;

    _power.notifyActivity((uint32_t)millis());

    uint8_t v_trig = EN_C10_TRIG_MAX;
    switch (p_group) {
        case 0: v_trig = (uint8_t)(EN_C10_TRIG_FLICK_LEFT  + p_dir); break;
        case 1: v_trig = (uint8_t)(EN_C10_TRIG_LINEAR_LEFT + p_dir); break;
        case 2: v_trig = (uint8_t)(EN_C10_TRIG_TILT_LEFT   + p_dir); break;
        default: return;
    }

    ST_C20_ActionSlot_t v_slot = _resolveSlot(_activeMode, v_trig);

    if (v_slot.kind == (uint8_t)EN_C20_ACT_NONE) return;

    if (v_slot.kind == (uint8_t)EN_C20_ACT_SPECIAL) {
        _handleSpecial((uint8_t)v_slot.param16);
        return;
    }

    _sanitizeMomentarySlot(v_slot);
    (void)_enqueueAction(v_slot, true);
}

// =======================================================
// 매크로 서브시스템
// =======================================================
void CL_E10_EliteAirMouse::_startMacro(uint8_t p_idx) {
    if (_macroState.active) {
        D10_LOGW("[E10] macro replace: prev idx=%u step=%u",
                 (unsigned)_macroState.macroIdx,
                 (unsigned)_macroState.stepIdx);
    }

    _lock();

    if (!_cfgProfileValid) {
        _unlock();
        D10_LOGW("[E10] macro start: profile not valid");
        return;
    }
    if (p_idx >= _cfgProfile.macros.count) {
        _unlock();
        D10_LOGW("[E10] macro index out of range: %u", (unsigned)p_idx);
        return;
    }

    _macroSnapshot = _cfgProfile.macros.macros[p_idx];

    _macroState.macroIdx    = p_idx;
    _macroState.stepIdx     = 0;
    _macroState.stepStartMs = (uint32_t)millis();
    _macroState.startToken  = _macroAbortToken;
    _macroState.active      = true;

    const uint8_t v_stepCount = _macroSnapshot.stepCount;
    const char*   v_name      = _macroSnapshot.name;

    _unlock();

    D10_LOGI("[E10] macro start: idx=%u name=%s steps=%u",
             (unsigned)p_idx, v_name, (unsigned)v_stepCount);
}

void CL_E10_EliteAirMouse::_tickMacro() {
    if (!_macroState.active) return;

    if (_macroState.startToken != _macroAbortToken) {
        D10_LOGW("[E10] macro aborted at step %u", (unsigned)_macroState.stepIdx);
        _macroState.active = false;
        return;
    }

    if (_macroState.stepIdx >= _macroSnapshot.stepCount) {
        D10_LOGI("[E10] macro end: idx=%u", (unsigned)_macroState.macroIdx);
        _macroState.active = false;
        return;
    }

    const ST_C10_MacroStep_t& s = _macroSnapshot.steps[_macroState.stepIdx];
    const uint32_t v_now = (uint32_t)millis();

    if (s.delayMs > 0 && (v_now - _macroState.stepStartMs) < (uint32_t)s.delayMs) {
        return;
    }

    ST_C20_ActionSlot_t v_slot;
    v_slot.kind     = s.kind;
    v_slot.holdMode = s.holdMode;
    v_slot.param16  = s.param16;
    v_slot.param32  = s.param32;

    (void)_actExec.exec(v_slot, true);

    _macroState.stepIdx++;
    _macroState.stepStartMs = (uint32_t)millis();
}
