// =======================================================
// File: src/v0415/E10_AirMouse_Action_0415.cpp
// =======================================================
#include "E10_AirMouse_0415.h"


// =======================================================
// [Phase 5] 물리 버튼 이벤트 → 슬롯 인덱스 매핑
//   slotIdx = 0..14 (S1..S15)
// =======================================================
namespace {

struct ST_SlotMapEntry_t {
    uint8_t btnId;   // EN_C20_BtnId_t
    uint8_t evt;     // EN_C20_BtnEvent_t
    uint8_t trig;    // EN_C10_Trigger_t
};

// 하드코딩 처리되는 이벤트는 여기서 제외
//   - Top L DOWN/UP/CLICK (Mouse Hold로 소비)
//   - Top M DOWN/UP (Move Gate)
//   - Side C DOUBLE/HOLD_2S/HOLD_3S
//   - Side C LONG → 하드코딩 미처리 + 슬롯 미등록 → 무시 (Phase 6 L6g-A1-01)
//   - Side F DOWN/UP (Front Hold)
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

    // Side C (CLICK만 — DOUBLE/HOLD2S/HOLD3S는 하드코딩)
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
// -------------------------------------------------------
//  순간 이벤트(CLICK/DOUBLE/LONG)에 매핑된 지속 액션을 단발로 변환.
//  _handleSlotButton / _handleGesture 양쪽에서 사용.
//  isDown=true 1회만 인큐되므로 UP 짝이 없어도 stuck 발생 안 함.
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

    // ---------- Side C: 모드 전환 / 페어링 / 호스트 순환 ----------
    //   DOUBLE   → Mode Cycle
    //   HOLD_2S  → Pairing
    //   HOLD_3S  → Host Cycle (BB-2: Pairing 자동 취소)
    //   CLICK    → 슬롯 위임 (S11)
    //   LONG     → 하드코딩 미처리 + 슬롯 미등록 → 무시 (Phase 6 L6g-A1-01)
    //              (800ms EVT_LONG은 2000ms HOLD_2S 진입 과정에서 자연 발생)
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
        // CLICK → 슬롯 매핑 위임
        return false;
    }

    // ---------- Top M: Move Gate (Hold) ----------
    // [C-1 fix] CLICK/LONG은 슬롯 매핑(S4)에 위임
    if (p_btnId == EN_C20_BTN_TOP_M) {
        if (p_evt == EN_C20_EVT_DOWN) {
            _moveGateHeld = true;
            return true;
        }
        if (p_evt == EN_C20_EVT_UP) {
            _moveGateHeld = false;
            return true;
        }
        // CLICK / LONG → 슬롯 매핑
        return false;
    }

    // ---------- Top L: 항상 마우스 좌클릭 (고정) ----------
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

    // ---------- Side F: Front Hold (스크롤 모드) ----------
    // [BB-6] DOWN/UP만 처리 후 조기 소비. CLICK/LONG은 슬롯 매핑(S9/S10).
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
// 슬롯 매핑 처리 (config)
// =======================================================
void CL_E10_EliteAirMouse::_handleSlotButton(uint8_t p_btnId, uint8_t p_evt) {
    for (const auto& e : G_SLOT_MAP) {
        if (e.btnId != p_btnId) continue;
        if (e.evt   != p_evt)   continue;

        ST_C20_ActionSlot_t v_slot = _resolveSlot(_activeMode, e.trig);

        if (v_slot.kind == (uint8_t)EN_C20_ACT_NONE) return;

        // SPECIAL은 sensorTask에서 즉시 처리
        if (v_slot.kind == (uint8_t)EN_C20_ACT_SPECIAL) {
            _handleSpecial((uint8_t)v_slot.param16);
            return;
        }

        // [BB-1] Momentary sanitization (공통 헬퍼)
        _sanitizeMomentarySlot(v_slot);

        // G_SLOT_MAP 이벤트는 모두 단발성이므로 isDown=true 전달
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

    // [Phase 8] 모든 이벤트는 활동 (idle 타이머 리셋)
    v_m->_power.notifyActivity((uint32_t)millis());

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

        // ============================================================
        // [v0415 Q4-a] SLEEP_NOW 즉시 진입
        // ------------------------------------------------------------
        //  이전 (v0412): _led.off() + 로그만 (실제 sleep 미진입 → idle 대기)
        //  이후 (v0415): LED suspend → sleepNow 직접 호출 → LED resume + Fast Recalib
        //
        //  컨텍스트: sensorTask 단독 (_handleSlotButton/_handleGesture에서만 진입)
        //  blocking: LED suspend ~700ms + sleepNow (wake까지 blocking)
        //  wake 후: biasTracker Fast Recalib (온도 드리프트 흡수)
        // ============================================================
        case EN_C20_SP_SLEEP_NOW: {
            CL_L10_Led::ST_LedSnapshot_t v_snap;
            _led.suspend(v_snap);   // blocking fadeout RED

            const bool v_safeOrPairing = (_safeMode || _ble.isPairing());
            const bool v_didSleep = _power.sleepNow((uint32_t)millis(), v_safeOrPairing);

            // wake 복귀 또는 실패 시 LED 원복
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
            // [BB-2] 2초 시점에 진입한 Pairing 모드 해제
            _ble.exitPairing();

            const uint8_t v_idx = _ble.cycleActivePeer();
            const bool v_reconn = _ble.reconnectToActivePeer(10000);

            _led.flash(EN_L10_COLOR_WHITE, 500);

            D10_LOGI("[E10] SP_HOST_CYCLE → peer=%u bond=%u reconnect=%d",
                     (unsigned)v_idx,
                     (unsigned)_ble.getBondCount(),
                     (int)v_reconn);
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

    // [Phase 8] Mode 전환은 활동
    _power.notifyActivity((uint32_t)millis());

    // [H-1] 매크로 취소 토큰 + 상태머신 즉시 종료 (lock 하 원자화)
    _lock();
    _macroAbortToken++;
    _macroState.active = false;

    const uint8_t v_old = _activeMode;
    _activeMode = p_newMode;

    if (_cfgProfileValid) {
        _cfgProfile.e10.active_mode = p_newMode;
    }
    _unlock();

    if (v_old == p_newMode) return;

    // [R2-C-2] HID release는 큐 경유
    _btnDisp.resetAll();
    _gesture.reset();

    // [Front Hold] Mode 전환 시 스크롤 상태 리셋
    _frontHoldActive = false;

    // [Phase 10] Mode 전환 시 pairing mode 취소
    if (_ble.isPairing()) {
        _ble.exitPairing();
        _led.setModeColor(p_newMode);
    }

    // FSM 리셋
    _precSub = EN_PREC_OFF;

    // [Phase 3] Snap 상태 리셋
    _resetSnapState();

    _precSmX = 0.0f;
    _precSmY = 0.0f;

    // HID 상태 강제 초기화
    (void)forceReleaseButtons();
    
    // [v0415 Phase 5 Q2-a] Mode 전환 시 EMA 상태 리셋
    _engine.resetEmaState();

    D10_LOGI("[E10] Mode changed: %u -> %u", (unsigned)v_old, (unsigned)p_newMode);

    // LED: 새 모드 색 + 흰색 0.5초 flash
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
// -------------------------------------------------------
//  [v0415 Critical Fix — BB-1 확장 (Phase 6 L6g-A3-01)]
//   - _handleSlotButton과 동일한 sanitization 적용
//   - Tilt Hold를 KB_REPEAT로 매핑 시 키 stuck 방지
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

    // SPECIAL은 sensorTask 즉시
    if (v_slot.kind == (uint8_t)EN_C20_ACT_SPECIAL) {
        _handleSpecial((uint8_t)v_slot.param16);
        return;
    }

    // [BB-1 확장] Momentary sanitization (제스처도 단발)
    _sanitizeMomentarySlot(v_slot);

    (void)_enqueueAction(v_slot, true);
}

// =======================================================
// [C-3/H-1/H-4] 매크로 서브시스템
// -------------------------------------------------------
//  [v0415 정책]
//   - _startMacro: _lock() 하 snapshot + state 초기화
//     · active=true를 마지막에 write (다른 태스크가 snapshot을
//       완전히 본 후 active를 관측하도록 순서 보장)
//     · 컴파일러 재정렬 방지 (lock 경계)
//   - _tickMacro: lock 없이 진행 (성능)
//     · active/startToken read는 volatile로 원자
//     · stepIdx/macroIdx/stepStartMs는 commTask 단독 write
//     · 다른 태스크는 active만 read → 재정렬 영향 없음
//   - switchProfile/_setActiveMode/forceReleaseButtons:
//     _lock() 하 token++ + active=false
// =======================================================

void CL_E10_EliteAirMouse::_startMacro(uint8_t p_idx) {
    if (_macroState.active) {
        D10_LOGW("[E10] macro replace: prev idx=%u step=%u",
                 (unsigned)_macroState.macroIdx,
                 (unsigned)_macroState.stepIdx);
    }

    // H-4: 락 하에 검증 + 스냅샷 + 상태 초기화 (재정렬 방지)
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

    // state 초기화 (active는 마지막)
    _macroState.macroIdx    = p_idx;
    _macroState.stepIdx     = 0;
    _macroState.stepStartMs = (uint32_t)millis();
    _macroState.startToken  = _macroAbortToken;
    _macroState.active      = true;   // ← 마지막 write

    const uint8_t v_stepCount = _macroSnapshot.stepCount;
    const char*   v_name      = _macroSnapshot.name;

    _unlock();

    D10_LOGI("[E10] macro start: idx=%u name=%s steps=%u",
             (unsigned)p_idx, v_name, (unsigned)v_stepCount);
}

void CL_E10_EliteAirMouse::_tickMacro() {
    if (!_macroState.active) return;

    // H-1: 취소 토큰 검사
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

    // Delay 대기 (블로킹 아님, 다음 tick에서 재검사)
    if (s.delayMs > 0 && (v_now - _macroState.stepStartMs) < (uint32_t)s.delayMs) {
        return;
    }

    // Step 실행 (primitive; MACRO/SPECIAL은 validate에서 제외됨)
    ST_C20_ActionSlot_t v_slot;
    v_slot.kind     = s.kind;
    v_slot.holdMode = s.holdMode;
    v_slot.param16  = s.param16;
    v_slot.param32  = s.param32;

    (void)_actExec.exec(v_slot, true);

    _macroState.stepIdx++;
    _macroState.stepStartMs = (uint32_t)millis();
}
