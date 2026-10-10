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
// [v0415] _sendPptKey2 — KB usage 상한 상수화
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

    // "실행 성공" → "큐 적재 성공"
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
// [v0415] forceReleaseAllButtons() 삭제 (Round G 선언 + Round K 정의)
// =======================================================
bool CL_E10_EliteAirMouse::forceReleaseButtons() {
    // [H-1 / v0415] 매크로 취소 토큰 + 상태머신 종료 (lock 하 원자화)
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
// -------------------------------------------------------
// [v0415 L6c-A4-01] 5버튼 mask 통합 호출 (5회 → 1회)
//   - 라이브러리 mouseRelease(mask) 시그니처가 다중 bit 지원
//   - 결과는 동일, RMT/queue 호출 횟수 절감
// =======================================================
void CL_E10_EliteAirMouse::_doReleaseAllButtons() {
    const uint8_t v_mask = (uint8_t)(EN_C20_M_L | EN_C20_M_R | EN_C20_M_M |
                                     EN_C20_M_B | EN_C20_M_F);
    _mouse.mouseRelease(v_mask);
}

// 상태 리셋 + 즉시 HID release (commTask 내부 전용, enqueue 경유하지 않음)
void CL_E10_EliteAirMouse::_doForceReleaseNow() {
    _lock();
    
    _state.x        = 0;
    _state.y        = 0;
    _state.wheel    = 0;
    _state.updated  = true;
    _unlock();

    _doReleaseAllButtons();
}
