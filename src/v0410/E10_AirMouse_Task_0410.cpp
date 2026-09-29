// =======================================================
// File: src/v0410/E10_AirMouse_Task_0410.cpp
// =======================================================
#include "E10_AirMouse_0410.h"

// =======================================================
// sensorTask
// =======================================================
void CL_E10_EliteAirMouse::_sensorTask(void* p_pv) {
    CL_E10_EliteAirMouse* v_m = (CL_E10_EliteAirMouse*)p_pv;

    TickType_t     v_lastWake = xTaskGetTickCount();
    unsigned long  v_lastUs   = micros();

    if (!v_m->_gyroCalibDone) v_m->_runGyroCalibration();

    for (;;) {
        // ------------------------------------------------------------
        // [C-4] motion-critical config 스냅샷
        //   - 그룹으로 읽히는 필드만 스냅샷 (dpi+scale+accel, wheel)
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
            v_m->_runGyroCalibration();
            v_m->_gyroCalibDone = true;
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
        //   - 물리 버튼 처리는 전부 디스패처가 담당
        //   - sensorTask는 여기서 상태만 얻음
        // ============================================================
        v_m->_btnDisp.update();

        // Move Gate 상태 읽기 (Top M Hold 중 true)
        const bool v_moveGateHeld = v_m->_moveGateHeld;
        
        // ====================================================
        // [Phase 10] BLE tick (pairing timeout / auto-exit)
        //   - 연결 판정은 _hid.isConnected() 재사용 (B20 중복 조회 회피)
        // ====================================================
        v_m->_ble.tick(v_m->_hid.isConnected());
        
        // ====================================================
        // [Phase 8] Power manager 활동 알림 + idle 판정
        //   - 활동 트리거: Move Gate Held, 커서 이동 (v_moveGateHeld 값으로 대체 판정),
        //                  버튼 DOWN (dispatcher가 콜백으로 처리)
        // ====================================================
        {
            const uint32_t v_nowP = (uint32_t)millis();
        
            // 커서 이동 감지 시 활동 갱신
            if (v_moveGateHeld) v_m->_power.notifyActivity(v_nowP);
        
            // idle → 연결 중이면 LED off만 (E10이 처리), 미연결이면 실제 sleep
            const bool v_bleConn = v_m->_hid.isConnected();
            const bool v_idle    = ((v_nowP - v_m->_power.getIdleTimeout() * 0) && false);  // 자리표시
            (void)v_idle;
        
            // 미연결 + idle 초과 → sleepNow
            //   (내부에서 timeout 검사. sleep 진입 시 wake 후 리턴)
            const bool v_frontHold = v_m->_frontHoldActive;
            const bool v_pairing   = v_m->_ble.isPairing();
            const bool v_qActBusy  = (uxQueueMessagesWaiting(v_m->_qActionExec) > 0);
            
            if (!v_bleConn && !v_moveGateHeld && !v_frontHold &&
                !v_pairing && !v_qActBusy &&
                !v_m->_safeMode && !v_m->_otaGuard) {
                    
                const bool v_didSleep = v_m->_power.sleepNow(v_nowP);
                if (v_didSleep) {
                    // wake 후 LED 복귀
                    v_m->_led.setModeColor(v_m->_activeMode);
                }
            }

        }

        // ---- gyro raw + Zero-rate Bias Tracking ----
        const float v_gxRaw = v_g.gyro.x * RAD_TO_DEG;
        const float v_gyRaw = v_g.gyro.y * RAD_TO_DEG;
        const float v_gzRaw = v_g.gyro.z * RAD_TO_DEG;

        v_m->_biasTracker.update(v_gxRaw, v_gyRaw, v_gzRaw, (uint32_t)millis());

        const float v_gx = v_m->_biasTracker.correctX(v_gxRaw);
        const float v_gy = v_m->_biasTracker.correctY(v_gyRaw);
        const float v_gz = v_m->_biasTracker.correctZ(v_gzRaw);

        const float v_gyroAbs = max(max(fabsf(v_gx), fabsf(v_gy)), fabsf(v_gz));

        // ---- spike detect ----
        const uint32_t v_ts = (uint32_t)(millis() - v_m->_uptime0);
        if (fabsf(v_gz) > E10_CONST::SPIKE_TH_DEG) v_m->_pushSpike(v_ts);

        // ---- fsm ----
        //  [Phase 5] SCROLL/MODE 토글 폐기 → 2·3번째 인자는 항상 false
        //            (SCROLL은 제스처 Phase 7에서 재설계)
        v_m->_fsmUpdate(false, false, v_gyroAbs);

        // ---- NaN guard ----
        if (isnan(v_gx) || isnan(v_gy) || isnan(v_gz)) {
            v_m->_errMpuNan++;
            v_m->_consecutiveFail++;
            v_m->_pushErr(EN_E10_ERR_MPU_NAN, 0);

            if ((v_m->_errMpuNan % 5) == 0) (void)v_m->_recoverI2C();

            vTaskDelayUntil(&v_lastWake, pdMS_TO_TICKS(8));
            continue;
        } else {
            if (v_m->_consecutiveFail > 0) v_m->_consecutiveFail--;
        }

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
        //   - roll/pitch 자세 확정 후 호출
        //   - 결과를 _handleGesture로 위임 (액션 큐 경유)
        // ============================================================
        {
            auto v_g = v_m->_gesture.update(
                v_gx, v_gy, v_gz,
                v_a.acceleration.x,
                v_a.acceleration.y,
                v_a.acceleration.z,
                v_m->_engine.getRoll(),
                v_m->_engine.getPitch(),
                v_moveGateHeld,
                v_m->_activeMode,
                (uint32_t)millis());
        
            if (v_g.flick  != EN_M30_DIR_NONE) v_m->_handleGesture(0, (uint8_t)v_g.flick);
            if (v_g.linear != EN_M30_DIR_NONE) v_m->_handleGesture(1, (uint8_t)v_g.linear);
            if (v_g.tilt   != EN_M30_DIR_NONE) v_m->_handleGesture(2, (uint8_t)v_g.tilt);
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
        // [Phase 5] Move Gate → Precision → 상태/프레임 push
        //   - SCROLL 분기 폐기 (제스처 Phase 7에서 재설계)
        //   - btn_mask는 액션 큐가 담당 → _qFrame에선 0 고정
        // ============================================================

        // ============================================================
        // [Front Hold] 스크롤 모드
        //   - Side F 누름 중: 커서 감쇠 + 수직 휠 + 수평 팬
        //   - 모든 Mode 일관 (D4-A)
        //   - 커서 감쇠: scroll_cursor_damp (기본 0.25)
        //   - 수직 휠: gy (pitch, 앞뒤 기울기)
        //   - 수평 팬: gx (roll, 좌우 기울기)
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
        // (Front Hold와 독립 — wheel/pan은 Move Gate 무관)
        if (!v_moveGateHeld) {
            v_fx = 0.0f;
            v_fy = 0.0f;
        }
        
        // Precision overlay
        if (v_m->_precSub != EN_PREC_OFF) {
            v_m->_applyPrecision(v_fx, v_fy);
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
// commTask
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
            // [v0410] 매크로 취소
            v_m->_macroAbort = true;

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
        if (v_conn && !v_prevConn) {
            // (Phase 5) 버튼 상태는 액션 큐가 담당 → 별도 동기화 불필요
        }
        v_prevConn = true;

        // ---- Gate (SafeMode / OTA Guard) ----
        const bool v_gate = (v_m->_safeMode || v_m->_otaGuard);

        if (v_gate) {
            // [v0410] 매크로 취소
            v_m->_macroAbort = true;

            // 큐 드레인 (overflow 방지)
            ST_E10_HidCmd_t v_drop;
            while (v_m->_qHidCmd &&
                   xQueueReceive(v_m->_qHidCmd, &v_drop, 0) == pdTRUE) { }

            ST_ActionCmd_t v_adrop;
            while (v_m->_qActionExec &&
                   xQueueReceive(v_m->_qActionExec, &v_adrop, 0) == pdTRUE) { }
                   
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

        // ---- [Phase2] HID commands 우선 처리 (테스트/강제 릴리즈) ----
        {
            ST_E10_HidCmd_t v_cmd;
            while (v_m->_qHidCmd && xQueueReceive(v_m->_qHidCmd, &v_cmd, 0) == pdTRUE) {
                switch (v_cmd.cmd) {
                    case EN_E10_HIDCMD_RELEASE_ALL:
                        v_m->_doReleaseAllButtons();
                        break;

                    case EN_E10_HIDCMD_TEST_CLICK:
                        v_m->_doTestMouseClick(v_cmd.arg0, v_cmd.holdMs);
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
        // [Phase 5/v0410] Action 큐 드레인
        //   - MACRO kind는 _runMacro로 위임 (블로킹 주의)
        //   - 프레임당 상한 4 (커서 지연 방지)
        // ============================================================
        {
            ST_ActionCmd_t v_acmd;
            uint8_t v_drainCount = 0;
            while (v_m->_qActionExec &&
                   xQueueReceive(v_m->_qActionExec, &v_acmd, 0) == pdTRUE) {

                if (v_acmd.slot.kind == (uint8_t)EN_C20_ACT_MACRO) {
                    v_m->_runMacro((uint8_t)v_acmd.slot.param32);
                } else {
                    v_m->_actExec.exec(v_acmd.slot, v_acmd.isDown);
                }

                if (++v_drainCount >= 4) break;
            }
        }

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

        vTaskDelay(pdMS_TO_TICKS(7));
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
