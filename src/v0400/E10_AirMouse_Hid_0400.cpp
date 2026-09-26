// =======================================================
// File: E10_AirMouse_Hid_0400.cpp
// =======================================================
#include "E10_AirMouse_0400.h"

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

void CL_E10_EliteAirMouse::_sendPptKey2FromCfg(const ST_C10_PptKey2_t& p_k) {
    _sendPptKey2(p_k.page, p_k.mod, p_k.code);
}

void CL_E10_EliteAirMouse::_processGesturesDeg(float p_gzDeg) {
    static unsigned long s_lastMs = 0;
    if (millis() - s_lastMs < _gestureCooldownMs) return;

    // [Phase2/C-3] sensorTask는 HID 직접 호출 금지 → 커맨드 큐로 위임
    ST_E10_HidCmd_t v_cmd;
    memset(&v_cmd, 0, sizeof(v_cmd));
    v_cmd.cmd = (uint8_t)EN_E10_HIDCMD_TEST_PPT;

    if (p_gzDeg > _gestureFlickDeg) {
        v_cmd.arg0 = _ppt2_prev.page;
        v_cmd.arg1 = _ppt2_prev.mod;
        v_cmd.code = _ppt2_prev.code;
        (void)_enqueueHidCmd(v_cmd);
        s_lastMs = millis();
    } else if (p_gzDeg < -_gestureFlickDeg) {
        v_cmd.arg0 = _ppt2_next.page;
        v_cmd.arg1 = _ppt2_next.mod;
        v_cmd.code = _ppt2_next.code;
        (void)_enqueueHidCmd(v_cmd);
        s_lastMs = millis();
    }
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

bool CL_E10_EliteAirMouse::testMouseClick(uint8_t p_btnMask, uint16_t p_holdMs) {
    if (!_hid.isConnected()) return false;
    if (_safeMode || _otaGuard) return false;

    uint16_t v_hold = p_holdMs;
    if (v_hold < 5)   v_hold = 5;
    if (v_hold > 250) v_hold = 250;

    // [H-4] 웹 태스크 블로킹 제거 → commTask가 실행
    ST_E10_HidCmd_t v_cmd;
    memset(&v_cmd, 0, sizeof(v_cmd));
    v_cmd.cmd    = (uint8_t)EN_E10_HIDCMD_TEST_CLICK;
    v_cmd.arg0   = p_btnMask;
    v_cmd.holdMs = v_hold;
    return _enqueueHidCmd(v_cmd);
}

// [H-2] 공개 API는 하나의 동작으로 통일: 상태 리셋(즉시) + RELEASE_ALL enqueue
bool CL_E10_EliteAirMouse::forceReleaseButtons() {
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
    (void)_enqueueHidCmd(v_cmd);
    return true;
}

bool CL_E10_EliteAirMouse::forceReleaseAllButtons() {
    return forceReleaseButtons();
}

// =======================================================
// [commTask ONLY] HID 실행 프리미티브
// =======================================================
void CL_E10_EliteAirMouse::_doReleaseAllButtons() {
    _mouse.mouseRelease((uint8_t)EN_E10_BTN_LEFT);
    _mouse.mouseRelease((uint8_t)EN_E10_BTN_RIGHT);
    _mouse.mouseRelease((uint8_t)EN_E10_BTN_MIDDLE);
}

void CL_E10_EliteAirMouse::_doTestMouseClick(uint8_t p_mask, uint16_t p_holdMs) {
    if (p_mask & (uint8_t)EN_E10_BTN_LEFT)   _mouse.mousePress((uint8_t)EN_E10_BTN_LEFT);
    if (p_mask & (uint8_t)EN_E10_BTN_RIGHT)  _mouse.mousePress((uint8_t)EN_E10_BTN_RIGHT);
    if (p_mask & (uint8_t)EN_E10_BTN_MIDDLE) _mouse.mousePress((uint8_t)EN_E10_BTN_MIDDLE);

    vTaskDelay(pdMS_TO_TICKS(p_holdMs));

    if (p_mask & (uint8_t)EN_E10_BTN_LEFT)   _mouse.mouseRelease((uint8_t)EN_E10_BTN_LEFT);
    if (p_mask & (uint8_t)EN_E10_BTN_RIGHT)  _mouse.mouseRelease((uint8_t)EN_E10_BTN_RIGHT);
    if (p_mask & (uint8_t)EN_E10_BTN_MIDDLE) _mouse.mouseRelease((uint8_t)EN_E10_BTN_MIDDLE);
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
