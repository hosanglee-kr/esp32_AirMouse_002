// =======================================================
// File: E10_AirMouse_Hid_0320.cpp
// =======================================================
#include "E10_AirMouse_0320.h"

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

    if (p_gzDeg > _gestureFlickDeg) {
        _sendPptKey2FromCfg(_ppt2_prev);
        s_lastMs = millis();
    } else if (p_gzDeg < -_gestureFlickDeg) {
        _sendPptKey2FromCfg(_ppt2_next);
        s_lastMs = millis();
    }
}

// =======================================================
// test / force release
// =======================================================
bool CL_E10_EliteAirMouse::testPptKey2(uint8_t p_page, uint8_t p_mod, uint32_t p_code) {
    if (!_hid.isConnected()) return false;
    _sendPptKey2(p_page, p_mod, p_code);
    return true;
}

bool CL_E10_EliteAirMouse::testMouseClick(uint8_t p_btnMask, uint16_t p_holdMs) {
    if (!_hid.isConnected()) return false;
    if (_safeMode || _otaGuard) return false;

    uint16_t v_hold = p_holdMs;
    if (v_hold < 5)   v_hold = 5;
    if (v_hold > 250) v_hold = 250;

    if (p_btnMask & (uint8_t)EN_E10_BTN_LEFT)   _mouse.mousePress((uint8_t)EN_E10_BTN_LEFT);
    if (p_btnMask & (uint8_t)EN_E10_BTN_RIGHT)  _mouse.mousePress((uint8_t)EN_E10_BTN_RIGHT);
    if (p_btnMask & (uint8_t)EN_E10_BTN_MIDDLE) _mouse.mousePress((uint8_t)EN_E10_BTN_MIDDLE);

    vTaskDelay(pdMS_TO_TICKS(v_hold));

    if (p_btnMask & (uint8_t)EN_E10_BTN_LEFT)   _mouse.mouseRelease((uint8_t)EN_E10_BTN_LEFT);
    if (p_btnMask & (uint8_t)EN_E10_BTN_RIGHT)  _mouse.mouseRelease((uint8_t)EN_E10_BTN_RIGHT);
    if (p_btnMask & (uint8_t)EN_E10_BTN_MIDDLE) _mouse.mouseRelease((uint8_t)EN_E10_BTN_MIDDLE);

    _lock();
    _state.btn_mask = 0;
    _state.updated  = true;
    _unlock();

    return true;
}

bool CL_E10_EliteAirMouse::forceReleaseButtons() {
    _lock();

    _mouse.mouseRelease((uint8_t)EN_E10_BTN_LEFT);
    _mouse.mouseRelease((uint8_t)EN_E10_BTN_RIGHT);
    _mouse.mouseRelease((uint8_t)EN_E10_BTN_MIDDLE);

    _state.btn_mask = 0;
    _state.x = 0;
    _state.y = 0;
    _state.wheel = 0;
    _state.updated = true;

    _unlock();
    return true;
}

bool CL_E10_EliteAirMouse::forceReleaseAllButtons() {
    uint8_t v_btn = 0;
    bool    v_conn = false;

    _lock();
    v_btn  = _state.btn_mask;
    v_conn = _hid.isConnected();

    _state.btn_mask = 0;
    _state.updated  = true;
    _unlock();

    if (v_conn) {
        if (v_btn & (uint8_t)EN_E10_BTN_LEFT)   _mouse.mouseRelease((uint8_t)EN_E10_BTN_LEFT);
        if (v_btn & (uint8_t)EN_E10_BTN_RIGHT)  _mouse.mouseRelease((uint8_t)EN_E10_BTN_RIGHT);
        if (v_btn & (uint8_t)EN_E10_BTN_MIDDLE) _mouse.mouseRelease((uint8_t)EN_E10_BTN_MIDDLE);
    }

    return true;
}
