// =======================================================
// File: src/v0415/E10_AirMouse_Hid_0415.cpp
// =======================================================
#include "E10_AirMouse_0415.h"

// =======================================================
// Modifier policy: p_modMask == HID modifier byte (W10 mods mask와 1:1)
// =======================================================
void CL_E10_EliteAirMouse::_tapComboUsageKb(uint8_t p_modMask, uint8_t p_usage, uint16_t p_ms) {
    if (p_usage == 0) return;
    if (p_modMask) _keyboard.modifierKeyPress(p_modMask);
    _keyboard.keyPress(p_usage);
    vTaskDelay(pdMS_TO_TICKS(p_ms));
    _keyboard.keyRelease(p_usage);
    if (p_modMask) _keyboard.modifierKeyRelease(p_modMask);
}

void CL_E10_EliteAirMouse::_tapUsageKb(uint8_t p_usage, uint16_t p_ms) {
    if (p_usage == 0) return;
    _keyboard.keyPress(p_usage);
    vTaskDelay(pdMS_TO_TICKS(p_ms));
    _keyboard.keyRelease(p_usage);
}

void CL_E10_EliteAirMouse::_tapConsumerMask(uint32_t p_mask, uint16_t p_ms) {
    if (p_mask == 0) return;
    _keyboard.mediaKeyPress(p_mask);
    vTaskDelay(pdMS_TO_TICKS(p_ms));
    _keyboard.mediaKeyRelease(p_mask);
}

// =======================================================
// _sendPptKey2 — KB usage 상한 상수화
// =======================================================
static constexpr uint32_t G_E10_KB_USAGE_MAX = 0xE7;

void CL_E10_EliteAirMouse::_sendPptKey2(uint8_t p_page, uint8_t p_mod, uint32_t p_code) {
    if (p_page == (uint8_t)EN_C10_KEYPAGE_CONSUMER) {
        _tapConsumerMask(p_code);
        return;
    }

    uint8_t v_usage = (uint8_t)min(G_E10_KB_USAGE_MAX, p_code);
    if (p_mod) _tapComboUsageKb(p_mod, v_usage);
    else       _tapUsageKb(v_usage);
}

// =======================================================
// test / force release (public)
// =======================================================
bool CL_E10_EliteAirMouse::testPptKey2(uint8_t p_page, uint8_t p_mod, uint32_t p_code) {
    if (!_hid.isConnected()) return false;

    ST_E10_HidCmd_t v_cmd;
    memset(&v_cmd, 0, sizeof(v_cmd));
    v_cmd.cmd  = (uint8_t)EN_E10_HIDCMD_TEST_PPT;
    v_cmd.arg0 = p_page;
    v_cmd.arg1 = p_mod;
    v_cmd.code = p_code;
    return _enqueueHidCmd(v_cmd);
}

// =======================================================
// [H-2] 공개 API: 상태 리셋(즉시) + RELEASE_ALL enqueue
// -------------------------------------------------------
// [v0415 Phase 1-5]
//   - 매크로 abort + state reset이 "동일 lock 블록 내"에서
//     원자적으로 이루어져야 하므로 _abortMacroLocked() 헬퍼를
//     사용하지 않고 lock 블록을 유지함 (정책 예외)
//   - 다른 호출 지점(_setActiveMode, switchProfile, _commTask)은
//     state reset이 필요 없어 헬퍼 사용
// =======================================================
bool CL_E10_EliteAirMouse::forceReleaseButtons() {
    // [Phase 1-5 예외] state reset + macro abort 동일 임계구역 유지
    _lock();
    _macroAbortToken++;
    _macroState.active = false;

    _state.x        = 0;
    _state.y        = 0;
    _state.wheel    = 0;
    _state.updated  = true;
    _unlock();

    ST_E10_HidCmd_t v_cmd;
    memset(&v_cmd, 0, sizeof(v_cmd));
    v_cmd.cmd = (uint8_t)EN_E10_HIDCMD_RELEASE_ALL;

    if (!_enqueueHidCmd(v_cmd)) {
        // 큐 full: commTask 루프 진입부에서 안전망 release 실행
        _reqCommReleaseAll = true;
        return false;
    }
    return true;
}

// =======================================================
// [commTask ONLY] HID 실행 프리미티브
// =======================================================
void CL_E10_EliteAirMouse::_doReleaseAllButtons() {
    const uint8_t v_mask = (uint8_t)(EN_C20_M_L | EN_C20_M_R | EN_C20_M_M |
                                     EN_C20_M_B | EN_C20_M_F);
    _mouse.mouseRelease(v_mask);
}

// 상태 리셋 + 즉시 HID release (commTask 내부 전용)
void CL_E10_EliteAirMouse::_doForceReleaseNow() {
    _lock();

    _state.x        = 0;
    _state.y        = 0;
    _state.wheel    = 0;
    _state.updated  = true;
    _unlock();

    _doReleaseAllButtons();
}
