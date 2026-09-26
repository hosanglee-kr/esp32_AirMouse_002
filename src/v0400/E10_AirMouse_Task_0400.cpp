// =======================================================
// File: E10_AirMouse_Task_0400.cpp
// =======================================================
#include "E10_AirMouse_0400.h"

// =======================================================
// sensorTask
// =======================================================
void CL_E10_EliteAirMouse::_sensorTask(void* p_pv) {
    CL_E10_EliteAirMouse* v_m = (CL_E10_EliteAirMouse*)p_pv;

    TickType_t     v_lastWake  = xTaskGetTickCount();
    unsigned long  v_lastUs    = micros();
    unsigned long  v_btnDownMs = 0;

    uint8_t v_lastBtnMask = 0;
    bool    v_lastBtnInit = false;

    if (!v_m->_gyroCalibDone) v_m->_runGyroCalibration();

    for (;;) {
        // ------------------------------------------------------------
        // [C-4] motion-critical config 스냅샷
        //   - 그룹으로 읽히는 필드만 스냅샷 (dpi+scale+accel, wheel)
        //   - 나머지(_isPptMode/_precision_mode/_accelTh/_scrollCursorDamp 등)는
        //     단일 워드 원자 read로 충분, eventually consistent 허용
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
            v_m->_reqGyroCalib = false;
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
        if (v_dtMs > 16.0f) {
            v_m->_dtOverrunCount++;
        }

        UBaseType_t v_hw = uxTaskGetStackHighWaterMark(nullptr);
        if (v_hw > 0) {
            if (v_m->_stackSensorMinWords == 0 || (uint32_t)v_hw < v_m->_stackSensorMinWords) {
                v_m->_stackSensorMinWords = (uint32_t)v_hw;
            }
        }

        // ---- buttons ----
        const bool v_scrollMode = (digitalRead(E10_CONST::PIN_BTN_SCROLL) == LOW);

        bool v_modeLongToggle = false;
        if (digitalRead(E10_CONST::PIN_BTN_MODE) == LOW) {
            if (v_btnDownMs == 0) v_btnDownMs = millis();
        } else {
            if (v_btnDownMs > 0) {
                unsigned long v_hold = millis() - v_btnDownMs;
                if (v_hold > 1000) {
                    v_modeLongToggle = true;
                } else {
                    v_m->_dpiLevel++;
                    if (v_m->_dpiLevel > 3) v_m->_dpiLevel = 1;
                    v_m->_engine.setDPI(v_m->_dpiLevel);
                }
                v_btnDownMs = 0;
            }
        }

        const bool v_leftClick  = (digitalRead(E10_CONST::PIN_BTN_L) == LOW);
        const bool v_rightClick = (digitalRead(E10_CONST::PIN_BTN_R) == LOW);
        const bool v_midClick   = (digitalRead(E10_CONST::PIN_BTN_M) == LOW);

        if (v_leftClick) v_m->_engine.notifyClick();

        uint8_t v_btnMask = 0;
        if (v_leftClick)  v_btnMask |= (uint8_t)EN_E10_BTN_LEFT;
        if (v_rightClick) v_btnMask |= (uint8_t)EN_E10_BTN_RIGHT;
        if (v_midClick)   v_btnMask |= (uint8_t)EN_E10_BTN_MIDDLE;

        // ---- gyro ----
        float v_gx = (v_g.gyro.x * RAD_TO_DEG) - v_m->_gyroBiasX;
        float v_gy = (v_g.gyro.y * RAD_TO_DEG) - v_m->_gyroBiasY;
        float v_gz = (v_g.gyro.z * RAD_TO_DEG) - v_m->_gyroBiasZ;

        const float v_gyroAbs = max(max(fabsf(v_gx), fabsf(v_gy)), fabsf(v_gz));

        // ---- spike detect ----
        const uint32_t v_ts = (uint32_t)(millis() - v_m->_uptime0);
        if (fabsf(v_gz) > E10_CONST::SPIKE_TH_DEG) v_m->_pushSpike(v_ts);

        // ---- fsm ----
        v_m->_fsmUpdate(v_scrollMode, v_modeLongToggle, v_gyroAbs);

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

        v_m->_engine.updateOrientation(v_a.acceleration.y, v_a.acceleration.z, v_gx, v_dt);

        int v_tx = 0;
        int v_ty = 0;
        v_m->_engine.process(-v_gz, -v_gx, v_tx, v_ty);

        // ---- accel shaping (C-4: v_cfg 스냅샷 사용) ----
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

        // ---- apply FSM ----
        if (v_m->_fsm == EN_FSM_SCROLL) {
            int v_wheel = 0;

            if (v_gy > v_cfg.wheelThDeg) {
                float v_n = (v_gy - v_cfg.wheelThDeg) / 120.0f;
                if (v_n > 1.0f) v_n = 1.0f;
                v_wheel = (int)(1 + (v_n * (v_cfg.wheelStepMax - 1)));
            } else if (v_gy < -v_cfg.wheelThDeg) {
                float v_n = (-v_gy - v_cfg.wheelThDeg) / 120.0f;
                if (v_n > 1.0f) v_n = 1.0f;
                v_wheel = -(int)(1 + (v_n * (v_cfg.wheelStepMax - 1)));
            }

            v_fx *= v_cfg.scrollCursorDamp;
            v_fy *= v_cfg.scrollCursorDamp;

            const int16_t v_xo = (int16_t)constrain((int)v_fx, -32767, 32767);
            const int16_t v_yo = (int16_t)constrain((int)v_fy, -32767, 32767);
            const int16_t v_wo = (int16_t)constrain((int)v_wheel, -32767, 32767);

            // 센서태스크 로컬 diff
            bool v_btnChanged = false;
            if (!v_lastBtnInit) {
                v_lastBtnInit = true;
                v_btnChanged  = true;
            } else if (v_btnMask != v_lastBtnMask) {
                v_btnChanged = true;
            }
            const bool v_updated = v_btnChanged || (v_xo != 0) || (v_yo != 0) || (v_wo != 0);

            // [C-1] (1) 관측용 _state
            if (xSemaphoreTakeRecursive(v_m->_mutex, pdMS_TO_TICKS(2)) == pdTRUE) {
                v_m->_state.x        = v_xo;
                v_m->_state.y        = v_yo;
                v_m->_state.wheel    = v_wo;
                v_m->_state.btn_mask = v_btnMask;
                if (v_updated) v_m->_state.updated = true;
                xSemaphoreGiveRecursive(v_m->_mutex);
            } else {
                v_m->_errMutexMiss++;
                v_m->_pushErr(EN_E10_ERR_MUTEX_MISS, 0);
            }

            // [C-1] (2) HID 전달 프레임
            {
                ST_E10_Frame_t v_fr;
                v_fr.x        = v_xo;
                v_fr.y        = v_yo;
                v_fr.wheel    = v_wo;
                v_fr.btn_mask = v_btnMask;
                v_fr.updated  = v_updated;
                v_m->_pushFrame(v_fr);
            }

            // (3) diff 기준 갱신
            v_lastBtnMask = v_btnMask;

        } else {
            // AIR/PPT/PREC cursor
            if (v_m->_precSub != EN_PREC_OFF) {
                v_m->_applyPrecision(v_fx, v_fy);
            }

            if (v_m->_fsm == EN_FSM_PPT) {
                if (!(v_m->_safeMode || v_m->_otaGuard)) {
                    v_m->_processGesturesDeg(v_gz);
                }
            }

            v_m->_welfordAdd(v_m->_curN, v_m->_curMean, v_m->_curM2,
                             (double)sqrtf(v_fx * v_fx + v_fy * v_fy));

            const int16_t v_xo = (int16_t)constrain((int)v_fx, -32767, 32767);
            const int16_t v_yo = (int16_t)constrain((int)v_fy, -32767, 32767);

            bool v_btnChanged = false;
            if (!v_lastBtnInit) {
                v_lastBtnInit = true;
                v_btnChanged  = true;
            } else if (v_btnMask != v_lastBtnMask) {
                v_btnChanged = true;
            }
            const bool v_updated = v_btnChanged || (v_xo != 0) || (v_yo != 0);

            // [C-1] (1) 관측용 _state
            if (xSemaphoreTakeRecursive(v_m->_mutex, pdMS_TO_TICKS(2)) == pdTRUE) {
                v_m->_state.x        = v_xo;
                v_m->_state.y        = v_yo;
                v_m->_state.wheel    = 0;
                v_m->_state.btn_mask = v_btnMask;
                if (v_updated) v_m->_state.updated = true;
                xSemaphoreGiveRecursive(v_m->_mutex);
            } else {
                v_m->_errMutexMiss++;
                v_m->_pushErr(EN_E10_ERR_MUTEX_MISS, 0);
            }

            // [C-1] (2) HID 전달 프레임
            {
                ST_E10_Frame_t v_fr;
                v_fr.x        = v_xo;
                v_fr.y        = v_yo;
                v_fr.wheel    = 0;
                v_fr.btn_mask = v_btnMask;
                v_fr.updated  = v_updated;
                v_m->_pushFrame(v_fr);
            }

            // (3) diff 기준 갱신
            v_lastBtnMask = v_btnMask;
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

    uint8_t v_lastBtnMask    = 0;
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

        // disconnect edge
        if (!v_conn && v_prevConn) {
            // [Phase2] commTask는 자기 큐에 enqueue하지 않고 직접 실행
            v_m->_doForceReleaseNow();
            v_m->_failsafeReleaseCount++;
            v_lastBtnMask = 0;
        }

        if (!v_conn) {
            v_prevConn = false;
            vTaskDelay(pdMS_TO_TICKS(12));
            continue;
        }

        // connect edge
        if (v_conn && !v_prevConn) {
            ST_E10_Frame_t v_fr0;
            memset(&v_fr0, 0, sizeof(v_fr0));
            if (v_m->_qFrame && (xQueuePeek(v_m->_qFrame, &v_fr0, 0) == pdTRUE)) {
                v_lastBtnMask = v_fr0.btn_mask;
            } else {
                v_lastBtnMask = 0;
            }
        }
        v_prevConn = true;

        // ---- Gate ----
        const bool v_gate = (v_m->_safeMode || v_m->_otaGuard);

        if (v_gate) {
            // [Phase2] gate 동안 커맨드는 전부 drop (큐 overflow 방지)
            ST_E10_HidCmd_t v_drop;
            while (v_m->_qHidCmd && xQueueReceive(v_m->_qHidCmd, &v_drop, 0) == pdTRUE) { }

            if (!v_releasedOnSafe) {
                v_m->_doForceReleaseNow();
                v_m->_failsafeReleaseCount++;
                v_lastBtnMask    = 0;
                v_releasedOnSafe = true;
            }

            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        } else {
            v_releasedOnSafe = false;
        }

        // ---- [Phase2] HID commands 우선 처리 ----
        {
            ST_E10_HidCmd_t v_cmd;
            while (v_m->_qHidCmd && xQueueReceive(v_m->_qHidCmd, &v_cmd, 0) == pdTRUE) {
                switch (v_cmd.cmd) {
                    case EN_E10_HIDCMD_RELEASE_ALL:
                        v_m->_doReleaseAllButtons();
                        v_lastBtnMask = 0;
                        break;

                    case EN_E10_HIDCMD_TEST_CLICK:
                        v_m->_doTestMouseClick(v_cmd.arg0, v_cmd.holdMs);
                        // 테스트는 양 끝이 release 상태로 종료되므로 다음 diff 기준 리셋
                        v_lastBtnMask = 0;
                        break;

                    case EN_E10_HIDCMD_TEST_PPT:
                        // [H-4] 실제 시퀀스는 commTask에서 수행 (vTaskDelay 포함)
                        v_m->_sendPptKey2(v_cmd.arg0, v_cmd.arg1, v_cmd.code);
                        break;

                    default:
                        break;
                }
            }
        }

        // ---- normal path ----
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
            const uint8_t v_btn   = v_fr.btn_mask;

            // 버튼 diff: updated 여부 무관하게 처리
            const uint8_t v_changed = (uint8_t)(v_btn ^ v_lastBtnMask);

            if (v_changed & (uint8_t)EN_E10_BTN_LEFT) {
                if (v_btn & (uint8_t)EN_E10_BTN_LEFT) v_m->_mouse.mousePress((uint8_t)EN_E10_BTN_LEFT);
                else                                  v_m->_mouse.mouseRelease((uint8_t)EN_E10_BTN_LEFT);
            }
            if (v_changed & (uint8_t)EN_E10_BTN_RIGHT) {
                if (v_btn & (uint8_t)EN_E10_BTN_RIGHT) v_m->_mouse.mousePress((uint8_t)EN_E10_BTN_RIGHT);
                else                                   v_m->_mouse.mouseRelease((uint8_t)EN_E10_BTN_RIGHT);
            }
            if (v_changed & (uint8_t)EN_E10_BTN_MIDDLE) {
                if (v_btn & (uint8_t)EN_E10_BTN_MIDDLE) v_m->_mouse.mousePress((uint8_t)EN_E10_BTN_MIDDLE);
                else                                    v_m->_mouse.mouseRelease((uint8_t)EN_E10_BTN_MIDDLE);
            }

            v_lastBtnMask = v_btn;

            if (v_upd) {
                const int8_t v_dx = (int8_t)constrain((int)v_x, -127, 127);
                const int8_t v_dy = (int8_t)constrain((int)v_y, -127, 127);
                const int8_t v_wh = (int8_t)constrain((int)v_wheel, -127, 127);

                _mouseSend(v_m->_mouse, v_dx, v_dy, v_wh);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(7));
    }
}
