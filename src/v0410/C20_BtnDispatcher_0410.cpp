// =======================================================
// File: src/v0410/C20_BtnDispatcher_0410.cpp
// =======================================================
#include "C20_BtnDispatcher_0410.h"

CL_C20_BtnDispatcher::CL_C20_BtnDispatcher() {
    memset(_btn, 0, sizeof(_btn));
    for (uint8_t i = 0; i < EN_C20_BTN_MAX; i++) {
        _btn[i].phase = PHASE_IDLE;
    }
}

void CL_C20_BtnDispatcher::begin() {
    const uint32_t v_now = (uint32_t)millis();
    for (uint8_t i = 0; i < EN_C20_BTN_MAX; i++) {
        pinMode(G_PINS[i], INPUT_PULLUP);
        _btn[i].stableState      = false;   // released (HIGH)
        _btn[i].lastRaw          = false;
        _btn[i].lastRawChangeMs  = v_now;
        _btn[i].phase            = PHASE_IDLE;
        _btn[i].longFired        = false;
        _btn[i].hold2sFired      = false;
        _btn[i].hold3sFired      = false;
        _btn[i].stableCount      = 0;       // [C-3]
        _btn[i].rawDownMs        = 0;       // [I-1, I-3]
    }
}

void CL_C20_BtnDispatcher::update() {
    const uint32_t v_now = (uint32_t)millis();
    for (uint8_t i = 0; i < EN_C20_BTN_MAX; i++) {
        _updateOne(i, v_now);
    }
}

void CL_C20_BtnDispatcher::_updateOne(uint8_t p_btnId, uint32_t p_now) {
    ST_BtnState_t& b = _btn[p_btnId];

    const bool v_raw = (digitalRead(G_PINS[p_btnId]) == LOW);

    // [C-3] raw 변경 감지
    if (v_raw != b.lastRaw) {
        b.lastRaw         = v_raw;
        b.lastRawChangeMs = p_now;
        b.stableCount     = 1;   // [C-3] 새 상태 카운트 시작

        // [I-1, I-3] 실제 DOWN 최초 접촉 시점 기록 (바운스 반복 오버라이트 방지)
        if (v_raw && b.rawDownMs == 0) {
            b.rawDownMs = p_now;
        } else if (!v_raw && !b.stableState) {
            b.rawDownMs = 0;   // 글리치 노이즈 미확정 복귀 시 리셋
        }
    } else {
        // [C-3] 연속 동일 raw 유지 카운트
        if (b.stableCount < 255) b.stableCount++;
    }

    // [C-4] Press/Release 별도 임계
    //   - 현재 stableState가 false (UP 상태) → 다음 전이는 DOWN → Press 임계
    //   - 현재 stableState가 true (DOWN 상태) → 다음 전이는 UP → Release 임계
    const uint16_t v_debounceMs = b.stableState
        ? _debounceReleaseMs   // UP 이벤트 대기 중
        : _debouncePressMs;    // DOWN 이벤트 대기 중

    // [C-3] 하이브리드 판정: 시간 + 카운터 모두 만족
    const bool v_timeOk  = (p_now - b.lastRawChangeMs) >= v_debounceMs;
    const bool v_countOk = b.stableCount >= _debounceMinTicks;

    if (v_timeOk && v_countOk && (v_raw != b.stableState)) {
        b.stableState = v_raw;
        b.stableCount = 0;   // 리셋 (다음 전이 대비)
        _onStableChange(p_btnId, v_raw, p_now);
    }

    _checkTimers(p_btnId, p_now);
}

void CL_C20_BtnDispatcher::_onStableChange(uint8_t p_btnId, bool p_stable, uint32_t p_now) {
    ST_BtnState_t& b = _btn[p_btnId];

    if (p_stable) {
        // =========== PRESS ===========
        // [I-1] 실제 물리적 누름 시점 (최초 접촉 시점) 기준
        b.downMs      = (b.rawDownMs > 0) ? b.rawDownMs : p_now;
        b.longFired   = false;
        b.hold2sFired = false;
        b.hold3sFired = false;

        if (b.phase == PHASE_WAIT_CLICK) {
            // 두 번째 클릭 → DOUBLE
            _emit(p_btnId, EN_C20_EVT_DOUBLE);
            b.phase = PHASE_DOUBLE;
            // (UP은 이후 무시)
        } else {
            _emit(p_btnId, EN_C20_EVT_DOWN);
            b.phase = PHASE_PRESSED;
        }
    } else {
        // =========== RELEASE ===========
        b.upMs = p_now;
        _emit(p_btnId, EN_C20_EVT_UP);

        // [I-3] 최소 누름 유지 시간 검사
        const uint32_t v_heldMs = (b.rawDownMs > 0)
            ? (b.lastRawChangeMs - b.rawDownMs) : (p_now - b.downMs);

        if (b.phase == PHASE_PRESSED) {
            if (b.longFired || b.hold2sFired || b.hold3sFired) {
                // Long/Hold로 이미 소비
                b.phase = PHASE_IDLE;
            } else if (v_heldMs < _minClickMs) {
                // [I-3] 초단 클릭 → CLICK 대기 진입 취소 (DISCARD)
                b.phase = PHASE_IDLE;
            } else {
                // 클릭 후보 → double 대기
                b.phase = PHASE_WAIT_CLICK;
                b.waitClickStartMs = p_now;
            }
        } else if (b.phase == PHASE_DOUBLE) {
            // 두 번째 UP → IDLE
            b.phase = PHASE_IDLE;
        }

        b.rawDownMs = 0;
    }
}

void CL_C20_BtnDispatcher::_checkTimers(uint8_t p_btnId, uint32_t p_now) {
    ST_BtnState_t& b = _btn[p_btnId];

    // Long / Hold (press 유지 중)
    // [R2-M-1] PHASE_DOUBLE은 제외 — 더블클릭 후 계속 hold 시 Pairing/Host Cycle 오발화 방지
    if (b.phase == PHASE_PRESSED) {
        const uint32_t v_held = p_now - b.downMs;

        if (!b.longFired && v_held >= _longDelayMs) {
            _emit(p_btnId, EN_C20_EVT_LONG);
            b.longFired = true;
        }
        if (!b.hold2sFired && v_held >= _hold2sMs) {
            _emit(p_btnId, EN_C20_EVT_HOLD_2S);
            b.hold2sFired = true;
        }
        if (!b.hold3sFired && v_held >= _hold3sMs) {
            _emit(p_btnId, EN_C20_EVT_HOLD_3S);
            b.hold3sFired = true;
        }
    }

    // Click 타임아웃 (double 대기 종료)
    if (b.phase == PHASE_WAIT_CLICK &&
        (p_now - b.waitClickStartMs) >= _doubleDelayMs) {
        _emit(p_btnId, EN_C20_EVT_CLICK);
        b.phase = PHASE_IDLE;
    }
}

void CL_C20_BtnDispatcher::_emit(uint8_t p_btnId, uint8_t p_evt) {
    if (_cb) _cb(_ctx, p_btnId, p_evt);
}

void CL_C20_BtnDispatcher::resetButton(uint8_t p_btnId) {
    if (p_btnId >= EN_C20_BTN_MAX) return;
    ST_BtnState_t& b = _btn[p_btnId];
    b.phase          = PHASE_IDLE;
    b.longFired      = false;
    b.hold2sFired    = false;
    b.hold3sFired    = false;

    // [I-4] Debounce 상태 초기화
    b.lastRawChangeMs = (uint32_t)millis();
    b.stableCount     = 0;
    b.rawDownMs       = 0;
}

void CL_C20_BtnDispatcher::resetAll() {
    for (uint8_t i = 0; i < EN_C20_BTN_MAX; i++) resetButton(i);
}
