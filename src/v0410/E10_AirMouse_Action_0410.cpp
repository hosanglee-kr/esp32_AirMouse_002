// =======================================================
// File: src/v0410/E10_AirMouse_Action_0410.cpp
// =======================================================
#include "E10_AirMouse_0410.h"


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
//   - Side F DOWN/UP (Front Hold)
//
// slotIdx → trig 로 재설계 (v0410)
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
    
    // ---------- Side F: Front Hold (스크롤 모드) ----------
    // [Front Hold] DOWN/UP만 처리. CLICK/LONG은 슬롯 매핑(S9/S10)으로 위임.
    if (p_btnId == EN_C20_BTN_SIDE_F) {
        if (p_evt == EN_C20_EVT_DOWN) {
            _frontHoldActive = true;
            return false;   // CLICK/LONG 슬롯 정상 처리
        }
        if (p_evt == EN_C20_EVT_UP) {
            _frontHoldActive = false;
            return false;
        }
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

        // [v0410] Global + Mode Override 기반 조회
        const ST_C20_ActionSlot_t v_slot = _resolveSlot(_activeMode, e.trig);

        if (v_slot.kind == (uint8_t)EN_C20_ACT_NONE) return;

        // SPECIAL은 sensorTask에서 즉시 처리
        if (v_slot.kind == (uint8_t)EN_C20_ACT_SPECIAL) {
            _handleSpecial((uint8_t)v_slot.param16);
            return;
        }

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

        case EN_C20_SP_SLEEP_NOW:
            // Phase 8에서 실제 sleep 진입. 지금은 LED만 정리.
            _led.off();
            D10_LOGI("[E10] SP_SLEEP_NOW requested");
            break;
    

        case EN_C20_SP_MODE_CYCLE:
            _setActiveMode((_activeMode % 3) + 1);
            break;

        case EN_C20_SP_PAIRING: {
            // [Phase 10] Pairing Mode 진입
            //  - 최대 3 peer까지 자동 추가 (NimBLE FIFO)
            //  - 30초 타임아웃, 연결 시 자동 종료
            _ble.enterPairing(30000);
            _led.blink(_led.getBaseColor(), 1000, 0);   // 현재 Mode 색 1Hz
            D10_LOGI("[E10] SP_PAIRING → pairing mode");
            break;
        }
        
        case EN_C20_SP_HOST_CYCLE: {
            // [Phase 9] Multi-Host 순환 + 실제 재연결 (disconnect + 재광고)
            const uint8_t v_idx = _ble.cycleActivePeer();
            
            // 10초 재연결 윈도우
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

    // [Phase 8] Mode 전환은 활동
    _power.notifyActivity((uint32_t)millis());
    
    // [v0410] 매크로 즉시 취소
    _macroAbort = true;

    _lock();
    const uint8_t v_old = _activeMode;
    _activeMode = p_newMode;

    // 프로파일 스냅샷에도 반영 (다음 저장 시 함께 저장됨)
    if (_cfgProfileValid) {
        _cfgProfile.e10.active_mode = p_newMode;
    }
    _unlock();

    if (v_old == p_newMode) return;

    // 진행 중 액션 전부 해제 (stuck 방지)
    _actExec.releaseAll();

    // 디스패처 상태 리셋 (클릭 대기 등)
    _btnDisp.resetAll();
    
    // [Phase 7] 제스처 상태 리셋
    _gesture.reset();
    
    // [Front Hold] Mode 전환 시 스크롤 상태 리셋
    _frontHoldActive = false;
    
    // [Phase 10] Mode 전환 시 pairing mode 취소
    if (_ble.isPairing()) {
        _ble.exitPairing();
        _led.setModeColor(p_newMode);   // blink 종료 → base solid 복귀
    }


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

    // [Phase 8] 제스처 = 활동
    _power.notifyActivity((uint32_t)millis());

    // [v0410] group + dir → trigger 변환
    //   group 0: FLICK  (15~18)
    //   group 1: LINEAR (19~22)
    //   group 2: TILT   (23~26)
    uint8_t v_trig = EN_C10_TRIG_MAX;
    switch (p_group) {
        case 0: v_trig = (uint8_t)(EN_C10_TRIG_FLICK_LEFT  + p_dir); break;
        case 1: v_trig = (uint8_t)(EN_C10_TRIG_LINEAR_LEFT + p_dir); break;
        case 2: v_trig = (uint8_t)(EN_C10_TRIG_TILT_LEFT   + p_dir); break;
        default: return;
    }

    const ST_C20_ActionSlot_t v_slot = _resolveSlot(_activeMode, v_trig);

    if (v_slot.kind == (uint8_t)EN_C20_ACT_NONE) return;

    // SPECIAL은 sensorTask 즉시
    if (v_slot.kind == (uint8_t)EN_C20_ACT_SPECIAL) {
        _handleSpecial((uint8_t)v_slot.param16);
        return;
    }

    // 제스처는 단발 (tap)
    (void)_enqueueAction(v_slot, true);
}

// =======================================================
// [v0410] 매크로 실행 (commTask 전용)
//   - 각 step을 순서대로 실행
//   - delayMs 만큼 대기 (abort 플래그 감시)
//   - step 실행은 _actExec.exec()로 (SPECIAL 제외 검증됨)
// =======================================================
void CL_E10_EliteAirMouse::_runMacro(uint8_t p_idx) {
    if (!_cfgProfileValid) return;
    if (p_idx >= _cfgProfile.macros.count) {
        D10_LOGW("[E10] macro index out of range: %u", (unsigned)p_idx);
        return;
    }

    const ST_C10_Macro_t& v_m = _cfgProfile.macros.macros[p_idx];

    _macroAbort = false;

    D10_LOGI("[E10] macro start: idx=%u name=%s steps=%u",
             (unsigned)p_idx, v_m.name, (unsigned)v_m.stepCount);

    for (uint8_t i = 0; i < v_m.stepCount; i++) {
        if (_macroAbort) {
            D10_LOGW("[E10] macro aborted at step %u", (unsigned)i);
            break;
        }

        const ST_C10_MacroStep_t& s = v_m.steps[i];

        // 1) delay (abort 감시)
        if (s.delayMs > 0) {
            const uint32_t v_t0 = (uint32_t)millis();
            while (((uint32_t)millis() - v_t0) < (uint32_t)s.delayMs) {
                if (_macroAbort) break;
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }
        if (_macroAbort) break;

        // 2) step 실행 (primitive)
        //    - step.kind는 validateMacroStep에서 MACRO/SPECIAL 제외됨
        ST_C20_ActionSlot_t v_slot;
        v_slot.kind     = s.kind;
        v_slot.holdMode = s.holdMode;
        v_slot.param16  = s.param16;
        v_slot.param32  = s.param32;

        (void)_actExec.exec(v_slot, true);
    }

    _macroAbort = false;

    D10_LOGI("[E10] macro end: idx=%u", (unsigned)p_idx);
}
