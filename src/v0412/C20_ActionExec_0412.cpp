// =======================================================
// File: src/v0412/C20_ActionExec_0412.cpp
// =======================================================
#include "C20_ActionExec_0412.h"

CL_C20_ActionExec::CL_C20_ActionExec() {
    memset(_rt, 0, sizeof(_rt));
}

void CL_C20_ActionExec::begin(MouseDevice* p_mouse, KeyboardDevice* p_keyboard) {
    _mouse    = p_mouse;
    _keyboard = p_keyboard;
}

// =======================================================
// public exec
// =======================================================
bool CL_C20_ActionExec::exec(const ST_C20_ActionSlot_t& p_slot, bool p_isDown) {
    if (!_mouse || !_keyboard) return false;
    if (p_slot.kind == (uint8_t)EN_C20_ACT_NONE) return false;

    switch ((EN_C20_ActionKind_t)p_slot.kind) {
        case EN_C20_ACT_MOUSE_CLICK:
            if (p_isDown) _execMouseClick(p_slot);
            return true;

        case EN_C20_ACT_MOUSE_HOLD:
            _execMouseHold(p_slot, p_isDown);
            return true;

        case EN_C20_ACT_MOUSE_WHEEL:
            if (p_isDown) _execMouseWheel(p_slot);
            return true;

        case EN_C20_ACT_KB_TAP:
            if (p_isDown) _execKbTap(p_slot);
            return true;

        case EN_C20_ACT_KB_COMBO:
            if (p_isDown) _execKbCombo(p_slot);
            return true;

        case EN_C20_ACT_KB_REPEAT:
            _execKbRepeat(p_slot, p_isDown);
            return true;

        case EN_C20_ACT_CONSUMER_TAP:
            if (p_isDown) _execConsumerTap(p_slot);
            return true;

        case EN_C20_ACT_CONSUMER_REPEAT:
            _execConsumerRep(p_slot, p_isDown);
            return true;

        case EN_C20_ACT_SPECIAL:
            // commTask에서는 실행 금지 (sensorTask 라우팅)
            // 여기 도달했다면 _handleSlotButton 경로가 아닌 것 → drop
            return true;

        default:
            return false;
    }
}

// =======================================================
// Repeat tick
// =======================================================
void CL_C20_ActionExec::tickRepeat() {
    const uint32_t v_now = (uint32_t)millis();

    for (uint8_t i = 0; i < MAX_RUNTIME; i++) {
        ST_Runtime_t& r = _rt[i];
        if (!r.active) continue;
        if (r.slot.kind == (uint8_t)EN_C20_ACT_NONE) { r.active = false; continue; }
        if ((v_now - r.lastRepeatMs) < r.repeatMs) continue;

        r.lastRepeatMs = v_now;

        switch ((EN_C20_ActionKind_t)r.slot.kind) {
            case EN_C20_ACT_KB_REPEAT: {
                uint8_t v_usage = (uint8_t)r.slot.param16;
                uint8_t v_mod   = (uint8_t)r.slot.param32;
                if (v_mod) _keyboard->modifierKeyPress(v_mod);
                _keyboard->keyPress(v_usage);
                vTaskDelay(pdMS_TO_TICKS(_kbTapMs));
                _keyboard->keyRelease(v_usage);
                if (v_mod) _keyboard->modifierKeyRelease(v_mod);
                break;
            }
            case EN_C20_ACT_CONSUMER_REPEAT: {
                _keyboard->mediaKeyPress(r.slot.param32);
                vTaskDelay(pdMS_TO_TICKS(_consumerTapMs));
                _keyboard->mediaKeyRelease(r.slot.param32);
                break;
            }
            default:
                break;
        }
    }
}

// =======================================================
// releaseAll (mode 전환 / sleep / disconnect)
// =======================================================
void CL_C20_ActionExec::releaseAll() {
    for (uint8_t i = 0; i < MAX_RUNTIME; i++) {
        ST_Runtime_t& r = _rt[i];
        if (!r.active) continue;

        // kind별 해제
        switch ((EN_C20_ActionKind_t)r.slot.kind) {
            case EN_C20_ACT_MOUSE_HOLD:
                _mouse->mouseRelease((uint8_t)r.slot.param16);
                break;
            case EN_C20_ACT_KB_REPEAT: {
                uint8_t v_usage = (uint8_t)r.slot.param16;
                uint8_t v_mod   = (uint8_t)r.slot.param32;
                _keyboard->keyRelease(v_usage);
                if (v_mod) _keyboard->modifierKeyRelease(v_mod);
                break;
            }
            case EN_C20_ACT_CONSUMER_REPEAT:
                _keyboard->mediaKeyRelease(r.slot.param32);
                break;
            default:
                break;
        }
        r.active = false;
    }

    // 안전: 전체 release
    _mouse->mouseRelease(EN_C20_M_L | EN_C20_M_R | EN_C20_M_M |
                         EN_C20_M_B | EN_C20_M_F);
}

// =======================================================
// Runtime 관리
// =======================================================
int CL_C20_ActionExec::_findRuntime(const ST_C20_ActionSlot_t& p_slot) {
    for (uint8_t i = 0; i < MAX_RUNTIME; i++) {
        if (_rt[i].active && _slotEq(_rt[i].slot, p_slot)) return i;
    }
    return -1;
}

int CL_C20_ActionExec::_allocRuntime(const ST_C20_ActionSlot_t& p_slot, uint16_t p_repeatMs) {
    for (uint8_t i = 0; i < MAX_RUNTIME; i++) {
        if (!_rt[i].active) {
            _rt[i].active       = true;
            _rt[i].lastRepeatMs = (uint32_t)millis();
            _rt[i].repeatMs     = p_repeatMs;
            _rt[i].slot         = p_slot;
            return i;
        }
    }
    return -1;  // full
}

void CL_C20_ActionExec::_freeRuntime(int p_idx) {
    if (p_idx < 0 || p_idx >= MAX_RUNTIME) return;
    _rt[p_idx].active = false;
    _rt[p_idx].slot   = { EN_C20_ACT_NONE, EN_C20_HOLD_NONE, 0, 0 };
}

// =======================================================
// Kind별 실행
// =======================================================
void CL_C20_ActionExec::_execMouseClick(const ST_C20_ActionSlot_t& s) {
    uint8_t v_mask = (uint8_t)s.param16;
    _mouse->mousePress(v_mask);
    vTaskDelay(pdMS_TO_TICKS(_mouseTapMs));
    _mouse->mouseRelease(v_mask);
}

void CL_C20_ActionExec::_execMouseHold(const ST_C20_ActionSlot_t& s, bool p_isDown) {
    uint8_t v_mask = (uint8_t)s.param16;

    if (p_isDown) {
        _mouse->mousePress(v_mask);
        // 런타임 등록 (UP에서 해제 대응)
        if (_findRuntime(s) < 0) _allocRuntime(s, 0);
    } else {
        _mouse->mouseRelease(v_mask);
        const int v_idx = _findRuntime(s);
        if (v_idx >= 0) _freeRuntime(v_idx);
    }
}

void CL_C20_ActionExec::_execMouseWheel(const ST_C20_ActionSlot_t& s) {
    const uint8_t v_axis = C20_WheelAxis(s.param16);
    const uint8_t v_dir  = C20_WheelDir (s.param16);

    int8_t v_wh  = 0;
    int8_t v_pan = 0;

    if (v_axis == EN_C20_WHEEL_Y) {
        v_wh = (v_dir == EN_C20_WHEEL_UP) ? +1 : -1;
    } else {
        v_pan = (v_dir == EN_C20_WHEEL_UP) ? +1 : -1;   // UP=Left
    }
    _mouse->mouseMove(0, 0, v_wh, v_pan);
}

void CL_C20_ActionExec::_execKbTap(const ST_C20_ActionSlot_t& s) {
    const uint8_t v_mod   = (uint8_t)s.param32;
    const uint8_t v_usage = (uint8_t)s.param16;
    if (v_usage == 0) return;

    if (v_mod) _keyboard->modifierKeyPress(v_mod);
    _keyboard->keyPress(v_usage);
    vTaskDelay(pdMS_TO_TICKS(_kbTapMs));
    _keyboard->keyRelease(v_usage);
    if (v_mod) _keyboard->modifierKeyRelease(v_mod);
}

void CL_C20_ActionExec::_execKbCombo(const ST_C20_ActionSlot_t& s) {
    const uint8_t v_mod = C20_ComboMod(s.param32);
    const uint8_t v_u1  = C20_ComboU1 (s.param32);
    const uint8_t v_u2  = C20_ComboU2 (s.param32);
    const uint8_t v_u3  = C20_ComboU3 (s.param32);

    if (v_mod) _keyboard->modifierKeyPress(v_mod);
    if (v_u1)  _keyboard->keyPress(v_u1);
    if (v_u2)  _keyboard->keyPress(v_u2);
    if (v_u3)  _keyboard->keyPress(v_u3);

    vTaskDelay(pdMS_TO_TICKS(_kbTapMs + 6));

    if (v_u3)  _keyboard->keyRelease(v_u3);
    if (v_u2)  _keyboard->keyRelease(v_u2);
    if (v_u1)  _keyboard->keyRelease(v_u1);
    if (v_mod) _keyboard->modifierKeyRelease(v_mod);
}

void CL_C20_ActionExec::_execKbRepeat(const ST_C20_ActionSlot_t& s, bool p_isDown) {
    if (p_isDown) {
        // 최초 1회 즉시 실행 + 반복 등록
        const uint8_t v_mod   = (uint8_t)s.param32;
        const uint8_t v_usage = (uint8_t)s.param16;
        if (v_mod) _keyboard->modifierKeyPress(v_mod);
        _keyboard->keyPress(v_usage);

        int v_idx = _findRuntime(s);
        if (v_idx < 0) v_idx = _allocRuntime(s, _repeatDefaultMs);
    } else {
        const uint8_t v_mod   = (uint8_t)s.param32;
        const uint8_t v_usage = (uint8_t)s.param16;
        _keyboard->keyRelease(v_usage);
        if (v_mod) _keyboard->modifierKeyRelease(v_mod);

        const int v_idx = _findRuntime(s);
        if (v_idx >= 0) _freeRuntime(v_idx);
    }
}

void CL_C20_ActionExec::_execConsumerTap(const ST_C20_ActionSlot_t& s) {
    if (s.param32 == 0) return;
    _keyboard->mediaKeyPress(s.param32);
    vTaskDelay(pdMS_TO_TICKS(_consumerTapMs));
    _keyboard->mediaKeyRelease(s.param32);
}

void CL_C20_ActionExec::_execConsumerRep(const ST_C20_ActionSlot_t& s, bool p_isDown) {
    if (p_isDown) {
        _keyboard->mediaKeyPress(s.param32);
        int v_idx = _findRuntime(s);
        if (v_idx < 0) _allocRuntime(s, _repeatDefaultMs);
    } else {
        _keyboard->mediaKeyRelease(s.param32);
        const int v_idx = _findRuntime(s);
        if (v_idx >= 0) _freeRuntime(v_idx);
    }
}

