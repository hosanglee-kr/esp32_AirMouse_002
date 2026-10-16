// =======================================================
// File: E10_AirMouse_Hid_0412.cpp
// =======================================================
#include "E10_AirMouse_0412.h"

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

void CL_E10_EliteAirMouse::_sendPptKey2(uint8_t p_page, uint8_t p_mod, uint32_t p_code) {
    if (p_page == (uint8_t)EN_C10_KEYPAGE_CONSUMER) {
        _tapConsumerMask(p_code);
        return;
    }

    uint8_t v_usage = (uint8_t)min((uint32_t)0xE7, p_code);
    if (p_mod) _tapComboUsageKb(p_mod, v_usage);
    else       _tapUsageKb(v_usage);
}

// =======================================================
// test / force release (public)
// =======================================================
bool CL_E10_EliteAirMouse::testPptKey2(uint8_t p_page, uint8_t p_mod, uint32_t p_code) {
    if (!_hid.isConnected()) return false;

    // [H-4] 반환값 의미: "실행 성공" → "큐 적재 성공"
    ST_E10_HidCmd_t v_cmd;
    memset(&v_cmd, 0, sizeof(v_cmd));
    v_cmd.cmd  = (uint8_t)EN_E10_HIDCMD_TEST_PPT;
    v_cmd.arg0 = p_page;
    v_cmd.arg1 = p_mod;
    v_cmd.code = p_code;
    return _enqueueHidCmd(v_cmd);
}

// [H-2] 공개 API: 상태 리셋(즉시) + RELEASE_ALL enqueue
// [R3-H-2/3] enqueue 실패 시 _reqCommReleaseAll 위임 → 큐 full 상태에서도 release 100% 보장
bool CL_E10_EliteAirMouse::forceReleaseButtons() {
    // [H-1] 매크로 취소 토큰 + 상태머신 종료
    _macroAbortToken++;
    _macroState.active = false;

    _lock();
    _state.btn_mask = 0;
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

bool CL_E10_EliteAirMouse::forceReleaseAllButtons() {
    return forceReleaseButtons();
}

// =======================================================
// [commTask ONLY] HID 실행 프리미티브
// =======================================================
void CL_E10_EliteAirMouse::_doReleaseAllButtons() {
    // [R2-C-3] 5버튼 전량 release (EN_E10_*는 L/R/M 3개만 정의)
    //   - C20 마스크는 Back(0x08)/Forward(0x10) 포함 → stuck 방지
    _mouse.mouseRelease((uint8_t)EN_C20_M_L);
    _mouse.mouseRelease((uint8_t)EN_C20_M_R);
    _mouse.mouseRelease((uint8_t)EN_C20_M_M);
    _mouse.mouseRelease((uint8_t)EN_C20_M_B);
    _mouse.mouseRelease((uint8_t)EN_C20_M_F);
}

// 상태 리셋 + 즉시 HID release (commTask 내부 전용, enqueue 경유하지 않음)
void CL_E10_EliteAirMouse::_doForceReleaseNow() {
    _lock();
    _state.btn_mask = 0;
    _state.x        = 0;
    _state.y        = 0;
    _state.wheel    = 0;
    _state.updated  = true;
    _unlock();

    _doReleaseAllButtons();
}
