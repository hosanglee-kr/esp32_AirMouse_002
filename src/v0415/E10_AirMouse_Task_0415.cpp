// =======================================================
// File: src/v0415/E10_AirMouse_Task_0415.cpp
// =======================================================
#include "E10_AirMouse_0415.h"

// =======================================================
// sensorTask
// -------------------------------------------------------
// [v0415 주요 변경]
//  - Safe/Pairing Wake Mask 분기 (Round E)
//    · sleepNow(now, safeOrPairing)
//    · deepSleepNow(now, hid, pairing, safe)
//  - Fast Recalib 파라미터는 E10 config 직접 참조
//    (P20의 fast_recalib_ms 삭제됨)
//  - _thLed 반영 (Round G, xTaskCreatePinnedToCore에서 처리)
//  - _fsmUpdate(false, false, v_gyroAbs) 유지 (Round K에서 정리)
// =======================================================
void CL_E10_EliteAirMouse::_sensorTask(void* p_pv) {
    CL_E10_EliteAirMouse* v_m = (CL_E10_EliteAirMouse*)p_pv;

    TickType_t     v_lastWake = xTaskGetTickCount();
    unsigned long  v_lastUs   = micros();

    if (!v_m->_gyroCalibDone) v_m->_runGyroCalibration();

    // [Phase 11.6 / C-3] Wake 콜백 등록
    v_m->_power.setWakeCallback(&CL_E10_EliteAirMouse::_onPowerWake, v_m);

    for (;;) {
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
        // [Phase 11.6 + v0415] Power Management
        //   - Safe/Pairing Wake Mask 분기 (Round E)
        //   - Fast Recalib: E10 config.power.fast_recalib_ms 직접
        // ============================================================
        {
            const uint32_t v_nowP = (uint32_t)millis();

            // [C-1] deferred notifyActivity
            if (v_m->_powerNotifyPending) {
                v_m->_powerNotifyPending = false;
                v_m->_power.notifyActivity(v_nowP);
            }

            // Sleep 조건 판정
            const bool v_bleConn  = v_m->_hid.isConnected();
            const bool v_pairing  = v_m->_ble.isPairing();
            const bool v_safe     = v_m->_safeMode;
            const bool v_safeOrPairing = (v_safe || v_pairing);

            const bool v_qActBusy =
                (uxQueueMessagesWaiting(v_m->_qActionExec) > 0) ||
                (v_m->_macroState.active);

            const bool v_canSleep =
                !v_m->_moveGateHeld && !v_m->_frontHoldActive && !v_qActBusy &&
                !v_m->_safeMode && !v_m->_otaGuard;

            if (v_canSleep && v_m->_cfgProfileValid) {
                uint32_t v_timeout;
                if (v_pairing) {
                    v_timeout = v_m->_cfgProfile.e10.power.pairing_idle_timeout_ms;
                } else if (v_bleConn) {
                    v_timeout = v_m->_cfgProfile.e10.power.idle_timeout_ble_ms;
                } else {
                    v_timeout = v_m->_power.getIdleTimeout(v_m->_activeMode);
                }

                const uint32_t v_idleSince = v_m->_power.getIdleSince();
                const uint32_t v_elapsed   = (v_nowP >= v_idleSince)
                                           ? (v_nowP - v_idleSince) : 0;

                if (v_elapsed >= v_timeout) {
                    // [N-1] Deep-sleep 우선 판정 (Safe 분기 포함)
                    const bool v_didDeepSleep =
                        v_m->_power.deepSleepNow(v_nowP, v_bleConn, v_pairing, v_safe);

                    if (!v_didDeepSleep) {
                        // LED 상태 저장 (blocking fadeout)
                        CL_L10_Led::ST_LedSnapshot_t v_snap;
                        v_m->_led.suspend(v_snap);

                        // [v0415] Safe/Pairing Wake Mask 분기
                        const bool v_didSleep =
                            v_m->_power.sleepNow(v_nowP, v_safeOrPairing);

                        // wake 복귀 또는 실패 시 LED 원복
                        v_m->_led.resume(v_snap);

                        if (v_didSleep) {
                            // [v0415] Fast Recalib — E10 config 직접 참조
                            //   (P20의 fast_recalib_ms 필드 삭제됨)
                            v_m->_biasTracker.startFastRecalibrate(
                                v_m->_cfgProfile.e10.power.fast_recalib_ms);
                        }
                    }
                }
            }
        }

        // ---- H-3: 웹 태스크가 위임한 리셋 플래그 처리 ----
        if (v_m->_reqResetBtnDisp) {
            v_m->_reqResetBtnDisp = false;
            v_m->_btnDisp.resetAll();
        }
        if (v_m->_reqResetGesture) {
            v_m->_reqResetGesture = false;
            v_m->_gesture.reset();
        }

        // ---- async requests ----
        if (v_m->_reqClearDiag) {
            v_m->_reqClearDiag = false;

            v_m->_lock();
            v_m->_errMpuNan      = 0;
            v_m->_errMutexMiss   = 0;
            v_m->_errTaskOverrun = 0;

            v_m->_gyroN = 0; v_m->_gyroMean = 0.0; v_m->_gyroM2 = 0.0;
            v_m->_curN  = 0; v_m->_curMean  = 0.0; v_m->_curM2  = 0.0;

            v_m->_i2cRecoverCount  = 0;
            v_m->_i2cRecoverLastOk = true;

            v_m->_errHistHead  = 0;
            v_m->_errHistCount = 0;
            memset(v_m->_errHist, 0, sizeof(v_m->_errHist));

            v_m->_spikeHead  = 0;
            v_m->_spikeCount = 0;
            memset(v_m->_spikes, 0, sizeof(v_m->_spikes));

            v_m->_consecutiveFail        = 0;
            v_m->_consecutiveRecoverFail = 0;
            v_m->_unlock();

            v_m->_pushErr(EN_E10_ERR_NONE, 0);
        }

        if (v_m->_reqI2CRecover) {
            v_m->_reqI2CRecover = false;
            (void)v_m->_recoverI2C();
        }

        if (v_m->_reqGyroCalib) {
            v_m->_reqGyroCalib  = false;
            v_m->_gyroCalibDone = false;
            // [D-1] _biasTracker.reset()을 sensorTask 컨텍스트로 이동
            v_m->_biasTracker.reset();
            v_m->_runGyroCalibration();
            v_m->_gyroCalibDone = true;
        }

        // [REQ-FIX-02] Special action 위임 처리 (sensorTask 단독 컨텍스트)
        //   - SLEEP_NOW가 이 경로로 진입 시 _handleSpecial 내부에서 직접 sleep
        if (v_m->_reqSpecialAction != 0) {
            const uint8_t v_sp = v_m->_reqSpecialAction;
            v_m->_reqSpecialAction = 0;
            v_m->_handleSpecial(v_sp);
        }

        // ---- sensor read ----
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
        if (v_dtMs > 16.0f) v_m->_dtOverrunCount++;

        UBaseType_t v_hw = uxTaskGetStackHighWaterMark(nullptr);
        if (v_hw > 0) {
            if (v_m->_stackSensorMinWords == 0 || (uint32_t)v_hw < v_m->_stackSensorMinWords) {
                v_m->_stackSensorMinWords = (uint32_t)v_hw;
            }
        }

        // ============================================================
        // [Phase 5] 버튼 디스패처 갱신 (콜백 → 액션 큐 enqueue)
        // ============================================================
        v_m->_btnDisp.update();

        // Move Gate 상태 읽기
        const bool v_moveGateHeld = v_m->_moveGateHeld;

        // ============================================================
        // [Phase 10] BLE tick (pairing timeout / auto-exit)
        // ============================================================
        v_m->_ble.tick(v_m->_hid.isConnected());

        // ---- gyro raw + Zero-rate Bias Tracking ----
        const float v_gxRaw = v_g.gyro.x * RAD_TO_DEG;
        const float v_gyRaw = v_g.gyro.y * RAD_TO_DEG;
        const float v_gzRaw = v_g.gyro.z * RAD_TO_DEG;

        // ---- NaN guard (BiasTracker 및 FSM으로의 NaN 오염 원천 차단) ----
        if (isnan(v_gxRaw) || isnan(v_gyRaw) || isnan(v_gzRaw) ||
            isnan(v_a.acceleration.x) || isnan(v_a.acceleration.y) || isnan(v_a.acceleration.z)) {
            v_m->_errMpuNan++;
            v_m->_consecutiveFail++;
            v_m->_pushErr(EN_E10_ERR_MPU_NAN, 0);

            if ((v_m->_errMpuNan % 5) == 0) (void)v_m->_recoverI2C();

            vTaskDelayUntil(&v_lastWake, pdMS_TO_TICKS(8));
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

        // Roll + Pitch 2축 자세
        v_m->_engine.updateOrientation2(
            v_a.acceleration.x,
            v_a.acceleration.y,
            v_a.acceleration.z,
            v_gx,   // Roll rate
            v_gy,   // Pitch rate
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
        //   - Side F 누름 중: 커서 감쇠 + 수직 휠 + 수평 팬
        // ============================================================
        const bool v_frontHold = v_m->_frontHoldActive;

        int16_t v_wheelY = 0;
        int16_t v_panX   = 0;

        if (v_frontHold) {
            // 커서 감쇠
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

        // Move Gate: Middle Hold 중에만 커서 이동
        if (!v_moveGateHeld) {
            v_fx = 0.0f;
            v_fy = 0.0f;
        }

        // Precision overlay
        if (v_m->_precSub != EN_PREC_OFF) {
            v_m->_applyPrecision(v_fx, v_fy);
        }

        // ============================================================
        // [Phase 3] Snap-to-Axis
        // ============================================================
        v_m->_applySnapToAxis(v_fx, v_fy);

        // ============================================================
        // [Phase 1] Click-Freeze — 최종 게이트
        // ============================================================
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
        if (xSemaphoreTakeRecursive(v_m->_mutex, pdMS_TO_TICKS(2)) == pdTRUE) {
            v_m->_state.x        = v_xo;
            v_m->_state.y        = v_yo;
            v_m->_state.wheel    = v_wheelY;
            v_m->_state.btn_mask = 0;
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

            // [Phase 11.6 / C-1] 커서 이동 기반 활동 알림 (deferred)
            if (v_updated) {
                v_m->_powerNotifyPending = true;
            }
        }

        // ---- pacing / overrun ----
        TickType_t v_before = xTaskGetTickCount();
        vTaskDelayUntil(&v_lastWake, pdMS_TO_TICKS(8));
        TickType_t v_after = xTaskGetTickCount();

        if ((v_after - v_before) == 0) {
            v_m->_errTaskOverrun++;
            if ((v_m->_errTaskOverrun % 10) == 0) v_m->_pushErr(EN_E10_ERR_TASK_OVERRUN, 0);
        }
    }
}

// =======================================================
// [Phase 11.6 / C-3] Power Wake 콜백
// -------------------------------------------------------
//  commTask 즉시 활성 신호 송출.
//  _thLed는 별도 참조 없음 (Round G에서 xTaskCreatePinnedToCore 저장).
// =======================================================
void CL_E10_EliteAirMouse::_onPowerWake(void* p_ctx) {
    auto* v_m = (CL_E10_EliteAirMouse*)p_ctx;
    if (!v_m) return;

    if (v_m->_thComm) {
        xTaskNotifyGive(v_m->_thComm);
    }
}

// =======================================================
// commTask
// -------------------------------------------------------
// [v0415 주요 변경]
//  - 매크로 abort 원자화 (disconnect edge / gate 진입)
//    · _macroAbortToken++ + active=false 를 _lock() 하
//  - gate 진입 시 _reqCommReleaseAll 처리 유지
//  - 큐 드레인 상한 유지 (4)
// =======================================================
void CL_E10_EliteAirMouse::_commTask(void* p_pv) {
    CL_E10_EliteAirMouse* v_m = (CL_E10_EliteAirMouse*)p_pv;

    bool    v_releasedOnSafe = false;
    bool    v_prevConn       = false;

    unsigned long v_lastUs = micros();

    for (;;) {
        const bool v_conn = v_m->_hid.isConnected();

        // (AB) comm dt
        unsigned long v_nowUs = micros();
        float v_dtMs = (v_nowUs - v_lastUs) / 1000.0f;
        v_lastUs = v_nowUs;
        v_m->_commDtAvgMs = v_m->_commDtAvgMs * 0.98f + v_dtMs * 0.02f;
        if (v_dtMs > v_m->_commDtMaxMs) v_m->_commDtMaxMs = v_dtMs;
        if (v_dtMs > 20.0f) v_m->_commOverrunCount++;

        // stack watermark
        UBaseType_t v_hw = uxTaskGetStackHighWaterMark(nullptr);
        if (v_hw > 0) {
            if (v_m->_stackCommMinWords == 0 || (uint32_t)v_hw < v_m->_stackCommMinWords) {
                v_m->_stackCommMinWords = (uint32_t)v_hw;
            }
        }

        // ---- disconnect edge ----
        if (!v_conn && v_prevConn) {
            // [v0415] 매크로 abort 원자화
            v_m->_lock();
            v_m->_macroAbortToken++;
            v_m->_macroState.active = false;
            v_m->_unlock();

            // HID + 액션 상태 전부 해제
            v_m->_actExec.releaseAll();
            v_m->_doForceReleaseNow();
            v_m->_failsafeReleaseCount++;

            // 잔여 큐 드레인
            ST_ActionCmd_t v_adrop;
            while (v_m->_qActionExec &&
                   xQueueReceive(v_m->_qActionExec, &v_adrop, 0) == pdTRUE) { }

            ST_E10_HidCmd_t v_hdrop;
            while (v_m->_qHidCmd &&
                   xQueueReceive(v_m->_qHidCmd, &v_hdrop, 0) == pdTRUE) { }
        }

        if (!v_conn) {
            v_prevConn = false;
            vTaskDelay(pdMS_TO_TICKS(12));
            continue;
        }

        // ---- connect edge ----
        v_prevConn = true;

        // ---- Gate (SafeMode / OTA Guard) ----
        const bool v_gate = (v_m->_safeMode || v_m->_otaGuard);

        if (v_gate) {
            // [v0415] 매크로 abort 원자화
            v_m->_lock();
            v_m->_macroAbortToken++;
            v_m->_macroState.active = false;
            v_m->_unlock();

            // 큐 드레인 (overflow 방지)
            ST_E10_HidCmd_t v_drop;
            while (v_m->_qHidCmd &&
                   xQueueReceive(v_m->_qHidCmd, &v_drop, 0) == pdTRUE) { }

            ST_ActionCmd_t v_adrop;
            while (v_m->_qActionExec &&
                   xQueueReceive(v_m->_qActionExec, &v_adrop, 0) == pdTRUE) { }

            // [R2-H-2] _qFrame(overwrite, size=1) 최신 프레임 폐기
            {
                ST_E10_Frame_t v_fdrop;
                (void)xQueueReceive(v_m->_qFrame, &v_fdrop, 0);
            }

            if (!v_releasedOnSafe) {
                v_m->_actExec.releaseAll();
                v_m->_doForceReleaseNow();
                v_m->_failsafeReleaseCount++;
                v_releasedOnSafe = true;
            }

            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        } else {
            v_releasedOnSafe = false;
        }

        // [REQ-FIX-03] 프로파일 스위치 등에서 요청된 안전 Release 위임 처리
        if (v_m->_reqCommReleaseAll) {
            v_m->_reqCommReleaseAll = false;
            v_m->_actExec.releaseAll();
            v_m->_doReleaseAllButtons();
        }

        // ---- HID commands 우선 처리 ----
        {
            ST_E10_HidCmd_t v_cmd;
            while (v_m->_qHidCmd && xQueueReceive(v_m->_qHidCmd, &v_cmd, 0) == pdTRUE) {
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

                if (++v_drainCount >= 4) break;
            }
        }

        // 매크로 상태머신 전진 (블로킹 없음)
        v_m->_tickMacro();

        // 반복 액션 tick (KB_REPEAT / CONSUMER_REPEAT)
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

        // [Phase 11.6 / C-3] Wake 신호 대기 (timeout은 tick 주기 7ms)
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(7));
    }
}

// =======================================================
// [Phase 6-J] LED 상태머신 태스크 (50ms tick)
// =======================================================
void CL_E10_EliteAirMouse::_ledTask(void* p_pv) {
    CL_E10_EliteAirMouse* v_m = (CL_E10_EliteAirMouse*)p_pv;

    TickType_t v_lastWake = xTaskGetTickCount();

    for (;;) {
        v_m->_led.tick();
        vTaskDelayUntil(&v_lastWake, pdMS_TO_TICKS(50));
    }
}

