// =======================================================
// File: src/v0415/E10_AirMouse_Task_0415.cpp
// =======================================================
#include "E10_AirMouse_0415.h"

// =======================================================
// [v0415 Phase 3-A] sensorTask 안전 분할 헬퍼 구현
// =======================================================

// -------------------------------------------------------
// 부팅 시 마지막 연결 호스트 우선 재연결 (10초)
//   - sensorTask 진입 시 1회 호출
// -------------------------------------------------------
void CL_E10_EliteAirMouse::_bootPriorityReconnect() {
    if (!_cfg) return;

    char v_lastAddr[18];
    if (_cfg->getLastHostAddr(v_lastAddr, sizeof(v_lastAddr))) {
        D10_LOGI("[E10] Boot priority reconnect to last host: %s", v_lastAddr);
        _ble.selectHostAddr(v_lastAddr, B20_HOST::CONN_TIMEOUT_MS);
    }
}

// -------------------------------------------------------
// Web API 비동기 호스트 전환 요청 소비 (신규 큐 미생성 위임)
//   - 매 루프 진입부에서 1회
// -------------------------------------------------------
void CL_E10_EliteAirMouse::_consumeHostOp() {
    if (_reqHostOp == EN_C20_REQ_HOST_NONE) return;

    const EN_C20_ReqHostOp_t v_op = _reqHostOp;
    _reqHostOp = EN_C20_REQ_HOST_NONE;

    switch (v_op) {
        case EN_C20_REQ_HOST_SEL_0:
            _ble.selectHostIdx(0, B20_HOST::CONN_TIMEOUT_MS);
            break;
        case EN_C20_REQ_HOST_SEL_1:
            _ble.selectHostIdx(1, B20_HOST::CONN_TIMEOUT_MS);
            break;
        case EN_C20_REQ_HOST_SEL_2:
            _ble.selectHostIdx(2, B20_HOST::CONN_TIMEOUT_MS);
            break;
        case EN_C20_REQ_HOST_TOGGLE:
            _handleSpecial(EN_C20_SP_HOST_TOGGLE);
            break;
        case EN_C20_REQ_HOST_CANCEL:
            _ble.cancelSelect();
            break;
        default:
            break;
    }
}

// -------------------------------------------------------
// Power Management
//   - Sleep 판정
//   - LED suspend → deepSleep → lightSleep → resume
//   - Fast Recalib (wake 후)
//   - 순서 변경 금지
// -------------------------------------------------------
void CL_E10_EliteAirMouse::_handlePowerManagement() {
    const uint32_t v_nowP = (uint32_t)millis();

    // deferred notifyActivity
    if (_powerNotifyPending) {
        _powerNotifyPending = false;
        _power.notifyActivity(v_nowP);
    }

    // Sleep 조건 판정
    const bool v_bleConn       = _hid.isConnected();
    const bool v_pairing       = _ble.isPairing();
    const bool v_safe          = _safeMode;
    const bool v_safeOrPairing = (v_safe || v_pairing);

    const bool v_qActBusy =
        (uxQueueMessagesWaiting(_qActionExec) > 0) || (_macroState.active);

    const bool v_canSleep =
        !_moveGateHeld && !_frontHoldActive && !v_qActBusy &&
        !_safeMode && !_otaGuard &&
        !_ble.isSelecting();

    if (!v_canSleep || !_cfgProfileValid) return;

    uint32_t v_timeout;
    if (v_pairing) {
        v_timeout = _cfgProfile.e10.power.pairing_idle_timeout_ms;
    } else if (v_bleConn) {
        v_timeout = _cfgProfile.e10.power.idle_timeout_ble_ms;
    } else {
        v_timeout = _power.getIdleTimeout(_activeMode);
    }

    const uint32_t v_idleSince = _power.getIdleSince();
    const uint32_t v_elapsed   = (v_nowP >= v_idleSince) ? (v_nowP - v_idleSince) : 0;

    if (v_elapsed < v_timeout) return;

    // Deep/Light 공통 전처리: WS2812 latch 방지 위해 먼저 LED OFF
    CL_L10_Led::ST_LedSnapshot_t v_snap;
    _led.suspend(v_snap);

    // Deep-sleep 우선 판정
    const bool v_didDeepSleep =
        _power.deepSleepNow(v_nowP, v_bleConn, v_pairing, v_safe);
    (void)v_didDeepSleep;

    // 여기 도달 = deep 미진입 → light-sleep 시도
    const bool v_didSleep = _power.sleepNow(v_nowP, v_safeOrPairing);

    // wake 복귀 또는 실패 시 LED 원복
    _led.resume(v_snap);

    if (v_didSleep) {
        // Fast Recalib — E10 config 직접 참조
        _biasTracker.startFastRecalibrate(_cfgProfile.e10.power.fast_recalib_ms);
    }
}

// -------------------------------------------------------
// 웹 태스크 위임 리셋 플래그 + async 요청 소비
//   - 리셋 플래그 2종 (btnDisp / gesture)
//   - async 요청 4종 (clearDiag / i2cRecover / gyroCalib / specialAction)
// -------------------------------------------------------
void CL_E10_EliteAirMouse::_consumeResetAndAsyncFlags() {
    // H-3: 웹 태스크가 위임한 리셋 플래그
    if (_reqResetBtnDisp) {
        _reqResetBtnDisp = false;
        _btnDisp.resetAll();
    }
    if (_reqResetGesture) {
        _reqResetGesture = false;
        _gesture.reset();
    }

    // ---- async: clearDiag ----
    if (_reqClearDiag) {
        _reqClearDiag = false;

        _lock();
        _errMpuNan      = 0;
        _errMutexMiss   = 0;
        _errTaskOverrun = 0;

        _gyroN = 0; _gyroMean = 0.0; _gyroM2 = 0.0;
        _curN  = 0; _curMean  = 0.0; _curM2  = 0.0;

        _i2cRecoverCount  = 0;
        _i2cRecoverLastOk = true;

        _errHistHead  = 0;
        _errHistCount = 0;
        memset(_errHist, 0, sizeof(_errHist));

        _spikeHead  = 0;
        _spikeCount = 0;
        memset(_spikes, 0, sizeof(_spikes));

        _consecutiveFail        = 0;
        _consecutiveRecoverFail = 0;
        _consecutiveI2cFail     = 0;
        _unlock();

        _pushErr(EN_E10_ERR_NONE, 0);
    }

    // ---- async: i2cRecover ----
    if (_reqI2CRecover) {
        _reqI2CRecover = false;
        (void)_recoverI2C();
    }

    // ---- async: gyroCalib ----
    if (_reqGyroCalib) {
        _reqGyroCalib  = false;
        _gyroCalibDone = false;
        _biasTracker.reset();
        _runGyroCalibration();
        _gyroCalibDone = true;
    }

    // ---- async: special action (위임) ----
    if (_reqSpecialAction != 0) {
        const uint8_t v_sp = _reqSpecialAction;
        _reqSpecialAction = 0;
        _handleSpecial(v_sp);
    }
}

// =======================================================
// [v0415 Phase 3-B] commTask 안전 분할 헬퍼 구현
// =======================================================

// -------------------------------------------------------
// disconnect edge 처리
//   - 매크로 즉시 취소
//   - HID/액션 상태 전부 해제
//   - 잔여 큐 드레인
// -------------------------------------------------------
bool CL_E10_EliteAirMouse::_handleCommDisconnectEdge(bool p_conn) {
    if (p_conn || !_commPrevConn) return false;

    // Phase 1-5: 매크로 취소 (SSOT 헬퍼)
    _abortMacroLocked();

    // HID + 액션 상태 전부 해제
    _actExec.releaseAll();
    _doForceReleaseNow();
    _failsafeReleaseCount++;

    // 잔여 큐 드레인
    ST_ActionCmd_t v_adrop;
    while (_qActionExec &&
           xQueueReceive(_qActionExec, &v_adrop, 0) == pdTRUE) {}

    ST_E10_HidCmd_t v_hdrop;
    while (_qHidCmd &&
           xQueueReceive(_qHidCmd, &v_hdrop, 0) == pdTRUE) {}

    _commPrevConn = false;
    return false;
}

// -------------------------------------------------------
// SafeMode / OTA Guard gate 처리
//   return true = gate 활성 (메인 루프 continue)
// -------------------------------------------------------
bool CL_E10_EliteAirMouse::_handleCommGate(bool p_conn) {
    (void)p_conn;   // p_conn은 이미 true

    const bool v_gate = (_safeMode || _otaGuard);
    if (!v_gate) {
        _commReleasedOnSafe = false;
        return false;
    }

    // Phase 1-5: 매크로 취소 (SSOT 헬퍼)
    _abortMacroLocked();

    _drainAllQueuesOnGate();

    if (!_commReleasedOnSafe) {
        _actExec.releaseAll();
        _doForceReleaseNow();
        _failsafeReleaseCount++;
        _commReleasedOnSafe = true;
    }
    return true;
}

// -------------------------------------------------------
// gate 진입 시 4중 큐 드레인 (HID + Action + Frame)
// -------------------------------------------------------
void CL_E10_EliteAirMouse::_drainAllQueuesOnGate() {
    ST_E10_HidCmd_t v_drop;
    while (_qHidCmd &&
           xQueueReceive(_qHidCmd, &v_drop, 0) == pdTRUE) {}

    ST_ActionCmd_t v_adrop;
    while (_qActionExec &&
           xQueueReceive(_qActionExec, &v_adrop, 0) == pdTRUE) {}

    // size=1 overwrite 큐 최신 프레임 폐기
    ST_E10_Frame_t v_fdrop;
    (void)xQueueReceive(_qFrame, &v_fdrop, 0);
}

// -------------------------------------------------------
// 연결 edge 감지 → last_host_addr NVS 저장
// -------------------------------------------------------
void CL_E10_EliteAirMouse::_notifyConnectionEdge(bool p_conn) {
    if (p_conn && !_connEdgePrev) {
        char v_peerName[32];
        if (_ble.getPeerName(v_peerName, sizeof(v_peerName))) {
            if (_cfg) (void)_cfg->setLastHostAddr(v_peerName);
        }
    }
    _connEdgePrev = p_conn;
}

// =======================================================
// sensorTask (Phase 3-A 분할 반영)
// -------------------------------------------------------
//  - 원본 순서 유지
//  - motion pipeline 지역변수(v_cfg, v_a/v_g/v_t 등)는 그대로
//  - 상수는 E10_CONST::* 사용
// =======================================================
void CL_E10_EliteAirMouse::_sensorTask(void* p_pv) {
    CL_E10_EliteAirMouse* v_m = (CL_E10_EliteAirMouse*)p_pv;

    TickType_t     v_lastWake = xTaskGetTickCount();
    unsigned long  v_lastUs   = micros();

    if (!v_m->_gyroCalibDone) v_m->_runGyroCalibration();

    // [Phase 11.6 / C-3] Wake 콜백 등록
    v_m->_power.setWakeCallback(&CL_E10_EliteAirMouse::_onPowerWake, v_m);

    // [Phase 3-A] 부팅 시 마지막 연결 호스트 우선 연결 (10초)
    v_m->_bootPriorityReconnect();

    for (;;) {
        // ============================================================
        // [Phase 3-A] Web API 호스트 전환 요청 소비
        // ============================================================
        v_m->_consumeHostOp();

        // ------------------------------------------------------------
        // [C-4] motion-critical config 스냅샷
        // ------------------------------------------------------------
        struct {
            int   dpiLevel;
            float scaleBase[3];
            float accelGain[3];
            float accelTh;
            float wheelThDeg;
            int   wheelStepMax;
            float scrollCursorDamp;
        } v_cfg;

        v_m->_lock();
        v_cfg.dpiLevel = v_m->_dpiLevel;
        for (int v_i = 0; v_i < 3; v_i++) {
            v_cfg.scaleBase[v_i] = v_m->_scaleBase[v_i];
            v_cfg.accelGain[v_i] = v_m->_accelGain[v_i];
        }
        v_cfg.accelTh          = v_m->_accelTh;
        v_cfg.wheelThDeg       = v_m->_wheelThDeg;
        v_cfg.wheelStepMax     = v_m->_wheelStepMax;
        v_cfg.scrollCursorDamp = v_m->_scrollCursorDamp;
        v_m->_unlock();

        // ============================================================
        // [Phase 3-A] Power Management
        // ============================================================
        v_m->_handlePowerManagement();

        // ============================================================
        // [Phase 3-A] 웹 태스크 위임 리셋 + async 요청 소비
        // ============================================================
        v_m->_consumeResetAndAsyncFlags();

        // ---- sensor read ----
        Wire.beginTransmission(v_m->_mpuAddr);
        const uint8_t v_i2cProbe = Wire.endTransmission();
        if (v_i2cProbe != 0) {
            v_m->_consecutiveI2cFail++;
            if (v_m->_consecutiveI2cFail >= E10_CONST::I2C_FAIL_MAX) {
                (void)v_m->_recoverI2C();
                v_m->_consecutiveI2cFail = 0;
            }
            vTaskDelayUntil(&v_lastWake, pdMS_TO_TICKS(E10_CONST::SENSOR_TICK_MS));
            continue;
        }
        v_m->_consecutiveI2cFail = 0;

        sensors_event_t v_a, v_g, v_t;
        v_m->_mpu.getEvent(&v_a, &v_g, &v_t);
        v_m->_tempC = v_t.temperature;

        // ---- dt ----
        unsigned long v_nowUs = micros();
        float v_dt = (v_nowUs - v_lastUs) / 1000000.0f;
        v_lastUs = v_nowUs;

        float v_dtMs = v_dt * 1000.0f;
        v_m->_dtAvgMs = v_m->_dtAvgMs * 0.98f + v_dtMs * 0.02f;

        if (v_dtMs > v_m->_dtMaxMs) v_m->_dtMaxMs = v_dtMs;
        if (v_dtMs > E10_CONST::DT_OVERRUN_MS) v_m->_dtOverrunCount++;

        UBaseType_t v_hw = uxTaskGetStackHighWaterMark(nullptr);
        if (v_hw > 0) {
            if (v_m->_stackSensorMinWords == 0 ||
                (uint32_t)v_hw < v_m->_stackSensorMinWords) {
                v_m->_stackSensorMinWords = (uint32_t)v_hw;
            }
        }

        // ============================================================
        // [Phase 5] 버튼 디스패처 갱신
        // ============================================================
        v_m->_btnDisp.update();

        const bool v_moveGateHeld = v_m->_moveGateHeld;

        // ============================================================
        // [Phase 10] BLE tick
        // ============================================================
        const bool v_currConnected = v_m->_hid.isConnected();
        v_m->_ble.tick(v_currConnected);

        // [Phase 3-B] 연결 edge → last_host_addr 저장
        v_m->_notifyConnectionEdge(v_currConnected);

        // ---- gyro raw + Zero-rate Bias Tracking ----
        const float v_gxRaw = v_g.gyro.x * RAD_TO_DEG;
        const float v_gyRaw = v_g.gyro.y * RAD_TO_DEG;
        const float v_gzRaw = v_g.gyro.z * RAD_TO_DEG;

        // ---- NaN guard ----
        if (isnan(v_gxRaw) || isnan(v_gyRaw) || isnan(v_gzRaw) ||
            isnan(v_a.acceleration.x) ||
            isnan(v_a.acceleration.y) ||
            isnan(v_a.acceleration.z)) {
            v_m->_errMpuNan++;
            v_m->_consecutiveFail++;
            v_m->_pushErr(EN_E10_ERR_MPU_NAN, 0);

            if ((v_m->_errMpuNan % E10_CONST::NAN_RECOVER_EVERY) == 0) {
                (void)v_m->_recoverI2C();
            }

            vTaskDelayUntil(&v_lastWake, pdMS_TO_TICKS(E10_CONST::SENSOR_TICK_MS));
            continue;
        } else {
            if (v_m->_consecutiveFail > 0) v_m->_consecutiveFail--;
        }

        v_m->_biasTracker.update(v_gxRaw, v_gyRaw, v_gzRaw, (uint32_t)millis());

        const float v_gx = v_m->_biasTracker.correctX(v_gxRaw);
        const float v_gy = v_m->_biasTracker.correctY(v_gyRaw);
        const float v_gz = v_m->_biasTracker.correctZ(v_gzRaw);

        const float v_gyroAbs = max(max(fabsf(v_gx), fabsf(v_gy)), fabsf(v_gz));

        // ---- spike detect ----
        const uint32_t v_ts = (uint32_t)(millis() - v_m->_uptime0);
        if (fabsf(v_gz) > E10_CONST::SPIKE_TH_DEG) v_m->_pushSpike(v_ts);

        // ---- fsm ----
        v_m->_fsmUpdate(v_gyroAbs);

        // ---- stats + motion engine ----
        v_m->_welfordAdd(v_m->_gyroN, v_m->_gyroMean, v_m->_gyroM2, (double)v_gz);

        v_m->_engine.updateOrientation2(
            v_a.acceleration.x,
            v_a.acceleration.y,
            v_a.acceleration.z,
            v_gx,
            v_gy,
            v_dt);

        int v_tx = 0;
        int v_ty = 0;
        v_m->_engine.process(-v_gz, -v_gx, v_tx, v_ty);

        // ============================================================
        // [Phase 7] 제스처 감지 (Flick / Linear / Tilt)
        // ============================================================
        {
            auto v_g_out = v_m->_gesture.update(
                v_gx, v_gy, v_gz,
                v_a.acceleration.x,
                v_a.acceleration.y,
                v_a.acceleration.z,
                v_m->_engine.getRoll(),
                v_m->_engine.getPitch(),
                v_moveGateHeld,
                v_m->_activeMode,
                (uint32_t)millis());

            if (v_g_out.flick  != EN_M30_DIR_NONE) v_m->_handleGesture(0, (uint8_t)v_g_out.flick);
            if (v_g_out.linear != EN_M30_DIR_NONE) v_m->_handleGesture(1, (uint8_t)v_g_out.linear);
            if (v_g_out.tilt   != EN_M30_DIR_NONE) v_m->_handleGesture(2, (uint8_t)v_g_out.tilt);
        }

        // ---- accel shaping ----
        float v_base = v_cfg.scaleBase[v_cfg.dpiLevel - 1];
        float v_accg = v_cfg.accelGain[v_cfg.dpiLevel - 1];

        float v_mag = sqrtf((float)v_tx * (float)v_tx + (float)v_ty * (float)v_ty);
        float v_acc = 1.0f;

        if (v_mag > v_cfg.accelTh) {
            float v_ex = (v_mag - v_cfg.accelTh);
            v_acc = 1.0f + (v_accg * (v_ex / (v_ex + 18.0f)));
        }

        float v_fx = (float)v_tx * v_base * v_acc;
        float v_fy = (float)v_ty * v_base * v_acc;

        // ============================================================
        // [Front Hold] 스크롤 모드
        // ============================================================
        const bool v_frontHold = v_m->_frontHoldActive;

        int16_t v_wheelY = 0;
        int16_t v_panX   = 0;

        if (v_frontHold) {
            v_fx *= v_cfg.scrollCursorDamp;
            v_fy *= v_cfg.scrollCursorDamp;

            // 수직 휠 (gy)
            if (v_gy > v_cfg.wheelThDeg) {
                float v_n = (v_gy - v_cfg.wheelThDeg) / 120.0f;
                if (v_n > 1.0f) v_n = 1.0f;
                v_wheelY = (int16_t)(1 + (v_n * (v_cfg.wheelStepMax - 1)));
            } else if (v_gy < -v_cfg.wheelThDeg) {
                float v_n = (-v_gy - v_cfg.wheelThDeg) / 120.0f;
                if (v_n > 1.0f) v_n = 1.0f;
                v_wheelY = -(int16_t)(1 + (v_n * (v_cfg.wheelStepMax - 1)));
            }

            // 수평 팬 (gx)
            if (v_gx > v_cfg.wheelThDeg) {
                float v_n = (v_gx - v_cfg.wheelThDeg) / 120.0f;
                if (v_n > 1.0f) v_n = 1.0f;
                v_panX = (int16_t)(1 + (v_n * (v_cfg.wheelStepMax - 1)));
            } else if (v_gx < -v_cfg.wheelThDeg) {
                float v_n = (-v_gx - v_cfg.wheelThDeg) / 120.0f;
                if (v_n > 1.0f) v_n = 1.0f;
                v_panX = -(int16_t)(1 + (v_n * (v_cfg.wheelStepMax - 1)));
            }
        }

        // Move Gate
        if (!v_moveGateHeld) {
            v_fx = 0.0f;
            v_fy = 0.0f;
        }

        // Precision overlay
        if (v_m->_precSub != EN_PREC_OFF) {
            v_m->_applyPrecision(v_fx, v_fy);
        }

        // Snap-to-Axis
        v_m->_applySnapToAxis(v_fx, v_fy);

        // Click-Freeze — 최종 게이트
        {
            const float v_rawDx = v_fx;
            const float v_rawDy = v_fy;
            v_m->_applyClickFreeze(v_fx, v_fy, v_rawDx, v_rawDy,
                                   v_gyroAbs, v_m->_btnLDown);
        }

        // 커서 통계
        v_m->_welfordAdd(v_m->_curN, v_m->_curMean, v_m->_curM2,
                         (double)sqrtf(v_fx * v_fx + v_fy * v_fy));

        const int16_t v_xo = (int16_t)constrain((int)v_fx, -32767, 32767);
        const int16_t v_yo = (int16_t)constrain((int)v_fy, -32767, 32767);
        const bool v_updated = (v_xo != 0) || (v_yo != 0) ||
                               (v_wheelY != 0) || (v_panX != 0);

        // (1) 관측용 _state
        if (xSemaphoreTakeRecursive(v_m->_mutex,
                                    pdMS_TO_TICKS(E10_CONST::STATE_LOCK_TIMEOUT_MS)) == pdTRUE) {
            v_m->_state.x     = v_xo;
            v_m->_state.y     = v_yo;
            v_m->_state.wheel = v_wheelY;

            if (v_updated) v_m->_state.updated = true;
            xSemaphoreGiveRecursive(v_m->_mutex);
        } else {
            v_m->_errMutexMiss++;
            v_m->_pushErr(EN_E10_ERR_MUTEX_MISS, 0);
        }

        // (2) HID 전달 프레임
        {
            ST_E10_Frame_t v_fr;
            v_fr.x        = v_xo;
            v_fr.y        = v_yo;
            v_fr.wheel    = v_wheelY;
            v_fr.pan      = v_panX;
            v_fr.btn_mask = 0;
            v_fr.updated  = v_updated;
            v_m->_pushFrame(v_fr);

            if (v_updated) {
                v_m->_powerNotifyPending = true;
            }
        }

        // ---- pacing / overrun ----
        TickType_t v_before = xTaskGetTickCount();
        vTaskDelayUntil(&v_lastWake, pdMS_TO_TICKS(E10_CONST::SENSOR_TICK_MS));
        TickType_t v_after = xTaskGetTickCount();

        if ((v_after - v_before) == 0) {
            v_m->_errTaskOverrun++;
            if ((v_m->_errTaskOverrun % E10_CONST::OVERRUN_LOG_EVERY) == 0) {
                v_m->_pushErr(EN_E10_ERR_TASK_OVERRUN, 0);
            }
        }
    }
}

// =======================================================
// Power Wake 콜백
// =======================================================
void CL_E10_EliteAirMouse::_onPowerWake(void* p_ctx) {
    auto* v_m = (CL_E10_EliteAirMouse*)p_ctx;
    if (!v_m) return;

    if (v_m->_thComm) {
        xTaskNotifyGive(v_m->_thComm);
    }
}

// =======================================================
// commTask (Phase 3-B 분할 반영)
// -------------------------------------------------------
//  - dt / 스택 워터마크
//  - disconnect edge (헬퍼)
//  - gate (헬퍼)
//  - reqCommReleaseAll 위임
//  - HID / Action 큐 드레인
//  - 매크로 / repeat tick
//  - 프레임 forward
// =======================================================
void CL_E10_EliteAirMouse::_commTask(void* p_pv) {
    CL_E10_EliteAirMouse* v_m = (CL_E10_EliteAirMouse*)p_pv;

    unsigned long v_lastUs = micros();

    for (;;) {
        const bool v_conn = v_m->_hid.isConnected();

        // ---- comm dt ----
        unsigned long v_nowUs = micros();
        float v_dtMs = (v_nowUs - v_lastUs) / 1000.0f;
        v_lastUs = v_nowUs;
        v_m->_commDtAvgMs = v_m->_commDtAvgMs * 0.98f + v_dtMs * 0.02f;
        if (v_dtMs > v_m->_commDtMaxMs) v_m->_commDtMaxMs = v_dtMs;
        if (v_dtMs > E10_CONST::COMM_OVERRUN_MS) v_m->_commOverrunCount++;

        // ---- stack watermark ----
        UBaseType_t v_hw = uxTaskGetStackHighWaterMark(nullptr);
        if (v_hw > 0) {
            if (v_m->_stackCommMinWords == 0 ||
                (uint32_t)v_hw < v_m->_stackCommMinWords) {
                v_m->_stackCommMinWords = (uint32_t)v_hw;
            }
        }

        // ---- disconnect edge (Phase 3-B 헬퍼) ----
        v_m->_handleCommDisconnectEdge(v_conn);

        if (!v_conn) {
            v_m->_commPrevConn = false;
            vTaskDelay(pdMS_TO_TICKS(E10_CONST::COMM_DISCONN_POLL_MS));
            continue;
        }

        // ---- connect edge ----
        v_m->_commPrevConn = true;

        // ---- gate (Phase 3-B 헬퍼) ----
        if (v_m->_handleCommGate(v_conn)) {
            vTaskDelay(pdMS_TO_TICKS(E10_CONST::COMM_GATE_POLL_MS));
            continue;
        }

        // ---- release 위임 처리 ----
        if (v_m->_reqCommReleaseAll) {
            v_m->_reqCommReleaseAll = false;
            v_m->_actExec.releaseAll();
            v_m->_doReleaseAllButtons();
        }

        // ---- HID commands 우선 처리 ----
        {
            ST_E10_HidCmd_t v_cmd;
            while (v_m->_qHidCmd &&
                   xQueueReceive(v_m->_qHidCmd, &v_cmd, 0) == pdTRUE) {
                switch (v_cmd.cmd) {
                    case EN_E10_HIDCMD_RELEASE_ALL:
                        v_m->_actExec.releaseAll();
                        v_m->_doReleaseAllButtons();
                        break;

                    case EN_E10_HIDCMD_TEST_PPT:
                        v_m->_sendPptKey2(v_cmd.arg0, v_cmd.arg1, v_cmd.code);
                        break;

                    default:
                        break;
                }
            }
        }

        // ============================================================
        // Action 큐 드레인 (프레임당 상한 4)
        // ============================================================
        {
            ST_ActionCmd_t v_acmd;
            uint8_t v_drainCount = 0;
            while (v_m->_qActionExec &&
                   xQueueReceive(v_m->_qActionExec, &v_acmd, 0) == pdTRUE) {

                if (v_acmd.slot.kind == (uint8_t)EN_C20_ACT_MACRO) {
                    v_m->_startMacro((uint8_t)v_acmd.slot.param32);
                } else {
                    v_m->_actExec.exec(v_acmd.slot, v_acmd.isDown);
                }

                if (++v_drainCount >= E10_CONST::ACTION_DRAIN_MAX) break;
            }
        }

        // 매크로 상태머신 전진
        v_m->_tickMacro();

        // 반복 액션 tick
        v_m->_actExec.tickRepeat();

        // ---- normal path: 커서 프레임 ----
        ST_E10_Frame_t v_fr;
        memset(&v_fr, 0, sizeof(v_fr));

        bool v_hasFrame = false;
        if (v_m->_qFrame) {
            if (xQueueReceive(v_m->_qFrame, &v_fr, 0) == pdTRUE) v_hasFrame = true;
        }

        if (v_hasFrame) {
            const bool    v_upd   = v_fr.updated;
            const int16_t v_x     = v_fr.x;
            const int16_t v_y     = v_fr.y;
            const int16_t v_wheel = v_fr.wheel;
            const int16_t v_pan   = v_fr.pan;

            if (v_upd) {
                const int8_t v_dx = (int8_t)constrain((int)v_x,     -127, 127);
                const int8_t v_dy = (int8_t)constrain((int)v_y,     -127, 127);
                const int8_t v_wh = (int8_t)constrain((int)v_wheel, -127, 127);
                const int8_t v_pn = (int8_t)constrain((int)v_pan,   -127, 127);

                _mouseSend(v_m->_mouse, v_dx, v_dy, v_wh, v_pn);
            }
        }

        // Wake 신호 대기 (tick 주기 7ms)
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(E10_CONST::COMM_TICK_MS));
    }
}

// =======================================================
// LED 상태머신 태스크 (50ms tick)
// =======================================================
void CL_E10_EliteAirMouse::_ledTask(void* p_pv) {
    CL_E10_EliteAirMouse* v_m = (CL_E10_EliteAirMouse*)p_pv;

    TickType_t v_lastWake = xTaskGetTickCount();

    for (;;) {
        v_m->_led.tick();

        // LED 태스크 스택 워터마크 관측
        UBaseType_t v_hw = uxTaskGetStackHighWaterMark(nullptr);
        if (v_hw > 0) {
            if (v_m->_stackLedMinWords == 0 ||
                (uint32_t)v_hw < v_m->_stackLedMinWords) {
                v_m->_stackLedMinWords = (uint32_t)v_hw;
            }
        }

        vTaskDelayUntil(&v_lastWake, pdMS_TO_TICKS(E10_CONST::LED_TICK_MS));
    }
}
