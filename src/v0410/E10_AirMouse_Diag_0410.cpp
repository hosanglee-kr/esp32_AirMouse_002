// =======================================================
// File: E10_AirMouse_Diag_0410.cpp
// =======================================================
#include "E10_AirMouse_0410.h"

// =======================================================
// [C-2] errHist/spike 락 보호
// =======================================================
void CL_E10_EliteAirMouse::_pushErr(uint8_t p_code, uint16_t p_value) {
    _lock();

    ST_E10_ErrEvt_t v_e;
    v_e.ts_ms = (uint32_t)(millis() - _uptime0);
    v_e.code  = p_code;
    v_e.value = p_value;

    _errHist[_errHistHead] = v_e;
    _errHistHead = (uint8_t)((_errHistHead + 1) % E10_CONST::ERR_HIST_CAP);
    if (_errHistCount < E10_CONST::ERR_HIST_CAP) _errHistCount++;

    _unlock();
}

void CL_E10_EliteAirMouse::_pushSpike(uint32_t p_tsMs) {
    _lock();

    _spikes[_spikeHead].ts_ms = p_tsMs;
    _spikeHead = (uint8_t)((_spikeHead + 1) % 32);
    if (_spikeCount < 32) _spikeCount++;

    _unlock();
}

// =======================================================
// I2C recover
// =======================================================
bool CL_E10_EliteAirMouse::_recoverI2C() {
    pinMode(E10_CONST::PIN_I2C_SDA, INPUT_PULLUP);
    pinMode(E10_CONST::PIN_I2C_SCL, OUTPUT_OPEN_DRAIN);

    for (int v_i = 0; v_i < 9; v_i++) {
        digitalWrite(E10_CONST::PIN_I2C_SCL, HIGH);
        delayMicroseconds(6);
        digitalWrite(E10_CONST::PIN_I2C_SCL, LOW);
        delayMicroseconds(6);
    }
    digitalWrite(E10_CONST::PIN_I2C_SCL, HIGH);
    delayMicroseconds(6);

    Wire.end();
    delay(5);

    Wire.begin(E10_CONST::PIN_I2C_SDA, E10_CONST::PIN_I2C_SCL);
    Wire.setClock(400000);
    delay(5);

    bool v_ok = _mpu.begin();
    if (v_ok) {
        _mpu.setGyroRange(MPU6050_RANGE_250_DEG);
        _mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
        _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    }

    // [M-2] getStatus/clearDiagnostics와 공유하는 카운터는 락 안에서 갱신
    _lock();
    _i2cRecoverCount++;
    _i2cRecoverLastOk = v_ok;

    if (v_ok) {
        _consecutiveRecoverFail = 0;
    } else {
        _consecutiveRecoverFail++;
        _consecutiveFail++;
    }
    _unlock();

    // [R2-L-2] _pushErr는 자체 _lock() 재진입 (recursive mutex) — 안전
    //          희귀 경로이므로 락 사이클 최적화는 스킵
    _pushErr(v_ok ? EN_E10_ERR_I2C_RECOVER_OK : EN_E10_ERR_I2C_RECOVER_FAIL, 0);
    return v_ok;
}


// =======================================================
// [C-5] 캘리브: 강제 릴리즈 + 버튼 샘플링
// =======================================================
void CL_E10_EliteAirMouse::_runGyroCalibration() {
    // 시작 시 강제 릴리즈 프레임
    {
        ST_E10_Frame_t v_rel;
        memset(&v_rel, 0, sizeof(v_rel));
        v_rel.updated = true;
        _pushFrame(v_rel);
    }

    const uint32_t v_t0 = millis();
    uint32_t v_cnt = 0;

    double v_sx = 0.0;
    double v_sy = 0.0;
    double v_sz = 0.0;

    while (millis() - v_t0 < E10_CONST::CALIB_MS) {
        sensors_event_t v_a, v_g, v_t;
        _mpu.getEvent(&v_a, &v_g, &v_t);

        float v_gx = v_g.gyro.x * RAD_TO_DEG;
        float v_gy = v_g.gyro.y * RAD_TO_DEG;
        float v_gz = v_g.gyro.z * RAD_TO_DEG;

        float v_m = max(max(fabsf(v_gx), fabsf(v_gy)), fabsf(v_gz));
        if (v_m < E10_CONST::CALIB_STILL_TH) {
            v_sx += v_gx;
            v_sy += v_gy;
            v_sz += v_gz;
            v_cnt++;
        }

        // [R3-D-1/D-2] 캘리브 중 버튼 상태 반영 (6버튼 전량, C20 G_PINS와 1:1)
        //   - 이전: E10_CONST::PIN_BTN_* 3버튼 하드코딩 → 매핑 드리프트 위험
        //   - 이후: C20_BtnDispatcher::G_PINS 순서로 6버튼 read
        {
            uint8_t v_btn = 0;
            for (uint8_t v_i = 0; v_i < EN_C20_BTN_MAX; v_i++) {
                if (digitalRead(CL_C20_BtnDispatcher::G_PINS[v_i]) == LOW) {
                    // EN_C20_BtnId_t → 물리 버튼 마스크 매핑
                    switch ((EN_C20_BtnId_t)v_i) {
                        case EN_C20_BTN_TOP_L:  v_btn |= (uint8_t)EN_E10_BTN_LEFT;   break;
                        case EN_C20_BTN_TOP_M:  v_btn |= (uint8_t)EN_E10_BTN_MIDDLE; break;
                        case EN_C20_BTN_TOP_R:  v_btn |= (uint8_t)EN_E10_BTN_RIGHT;  break;
                        // Side F/C/R은 마우스 버튼 마스크와 무관 (E10 상태에 반영 안 함)
                        default: break;
                    }
                }
            }
        
            ST_E10_Frame_t v_fr;
            memset(&v_fr, 0, sizeof(v_fr));
            v_fr.btn_mask = v_btn;
            v_fr.updated  = false;
            _pushFrame(v_fr);
        }

        // [D-2] 캘리브 1초 블로킹 중 BLE pairing 타임아웃 검사 유지
        _ble.tick(_hid.isConnected());

        vTaskDelay(pdMS_TO_TICKS(5));
    }

    if (v_cnt > 0) {
        _biasTracker.setBias(
            (float)(v_sx / v_cnt),
            (float)(v_sy / v_cnt),
            (float)(v_sz / v_cnt));
    }

    _gyroCalibDone = true;
}

// =======================================================
// Status
// =======================================================
void CL_E10_EliteAirMouse::getStatus(ST_E10_Status_t& p_out) {
    memset(&p_out, 0, sizeof(p_out));
    _lock();

    // [R3-D-5] _hid는 commTask 소유이나 isConnected() read는 예외 허용 (STATE.md §1)
    //   - _lock 보유 중 read: BLE 스택 내부 락 취득 가능성 → 잠재적 lock ordering 이슈
    //   - 실무 영향 없음 (NimBLE read는 lock-free atomic 수준)
    //   - 필요 시 _lock 이전으로 이동 가능 (상태 일관성 trade-off)
    p_out.ble_connected = _hid.isConnected();
    p_out.ppt_mode      = _isPptMode;
    p_out.dpi_level     = (uint8_t)_dpiLevel;

    p_out.btn_mask = _state.btn_mask;

    p_out.safe_mode = _safeMode;
    p_out.ota_guard = _otaGuard;
    p_out.ota_guard_count = _otaGuardCount;

    if (_otaGuard) {
        const uint32_t v_nowMs = (uint32_t)(millis() - _uptime0);
        p_out.ota_guard_uptime_ms = (uint32_t)(v_nowMs - _otaGuardT0Ms);
    } else {
        p_out.ota_guard_uptime_ms = 0;
    }

    p_out.precision_mode   = _precision_mode;
    p_out.fsm_state        = _fsm;
    p_out.fsm_sub          = _precSub;

    p_out.gyro_bias_x = _biasTracker.x();
    p_out.gyro_bias_y = _biasTracker.y();
    p_out.gyro_bias_z = _biasTracker.z();

    p_out.temp_c      = _tempC;

    p_out.sampling_ms_target = 8;
    p_out.sampling_ms_avg    = _dtAvgMs;

    p_out.sensor_dt_max_ms       = _dtMaxMs;
    p_out.sensor_overrun_count   = _dtOverrunCount;
    p_out.comm_dt_avg_ms         = _commDtAvgMs;
    p_out.comm_dt_max_ms         = _commDtMaxMs;
    p_out.comm_overrun_count     = _commOverrunCount;
    p_out.failsafe_release_count = _failsafeReleaseCount;

    p_out.task_stack_sensor_min_words = _stackSensorMinWords;
    p_out.task_stack_comm_min_words   = _stackCommMinWords;

    p_out.i2c_recover_count   = _i2cRecoverCount;
    p_out.i2c_recover_last_ok = _i2cRecoverLastOk;

    p_out.err_mpu_nan      = _errMpuNan;
    p_out.err_mutex_miss   = _errMutexMiss;
    p_out.err_task_overrun = _errTaskOverrun;

    p_out.uptime_ms = (uint32_t)(millis() - _uptime0);

    p_out.gyro_rms   = _calcRms(_gyroN, _gyroM2);
    p_out.cursor_rms = _calcRms(_curN,  _curM2);

    // anomaly snapshot (최근 10초)
    const uint32_t v_nowMs = (uint32_t)(millis() - _uptime0);
    uint16_t v_sc = 0;
    for (uint8_t v_i = 0; v_i < _spikeCount; v_i++) {
        int v_idx = (int)_spikeHead - 1 - (int)v_i;
        if (v_idx < 0) v_idx += 32;

        if (v_nowMs - _spikes[v_idx].ts_ms <= 10000) v_sc++;
        else break;
    }

    p_out.spike_count_10s          = v_sc;
    p_out.consecutive_fail         = _consecutiveFail;
    p_out.consecutive_recover_fail = _consecutiveRecoverFail;

    // health score
    // [R3-D-4] 클램프 헬퍼로 곱셈 오버플로 방어 (카운터 × 승수 → cap 이전 uint32 승격)
    auto v_deduce = [](int p_score, uint32_t p_count, uint32_t p_mult, uint32_t p_cap) -> int {
        // p_count > UINT32_MAX / p_mult 인 경우 안전 처리
        uint32_t v_penalty;
        if (p_mult == 0 || p_count > (0xFFFFFFFFu / p_mult)) {
            v_penalty = p_cap;
        } else {
            v_penalty = p_count * p_mult;
            if (v_penalty > p_cap) v_penalty = p_cap;
        }
        int v_ret = p_score - (int)v_penalty;
        return (v_ret < 0) ? 0 : v_ret;
    };
    
    int v_scoreI = 1000;
    v_scoreI -= (int)(p_out.gyro_rms * 25.0f);          if (v_scoreI < 0) v_scoreI = 0;
    v_scoreI -= (int)(p_out.cursor_rms * 18.0f);        if (v_scoreI < 0) v_scoreI = 0;
    v_scoreI = v_deduce(v_scoreI, p_out.err_mpu_nan,          20, 400);
    v_scoreI = v_deduce(v_scoreI, p_out.i2c_recover_count,    35, 300);
    v_scoreI = v_deduce(v_scoreI, p_out.spike_count_10s,      12, 300);
    v_scoreI = v_deduce(v_scoreI, p_out.consecutive_fail,     18, 400);
    uint16_t v_score = (uint16_t)v_scoreI;

    p_out.health_score = v_score;
    p_out.health       = (v_score >= 820) ? (uint8_t)EN_E10_HEALTH_OK
                      : ((v_score >= 620) ? (uint8_t)EN_E10_HEALTH_WARN : (uint8_t)EN_E10_HEALTH_DEGRADED);

    p_out.err_hist_n = (uint8_t)min((uint8_t)E10_CONST::ERR_HIST_CAP, _errHistCount);
    for (uint8_t v_i = 0; v_i < p_out.err_hist_n; v_i++) {
        int v_idx = (int)_errHistHead - 1 - (int)v_i;
        if (v_idx < 0) v_idx += (int)E10_CONST::ERR_HIST_CAP;
        p_out.err_hist[v_i] = _errHist[v_idx];
    }

    _unlock();
}

// =======================================================
// Async requests / clear
// =======================================================
bool CL_E10_EliteAirMouse::requestGyroCalibration() {
    // [R3-D-3] _reqGyroCalib는 volatile single-bit write. 락은 불필요하나
    //           "위임 플래그 접근 시 락 사용" 일관성 정책에 따라 유지.
    _lock();
    _reqGyroCalib = true;
    // [D-1] _biasTracker.reset()은 sensorTask로 위임 (SPEC §상태 소유권)
    //       실제 reset은 sensorTask가 _reqGyroCalib 플래그 처리 시 실행.
    _unlock();
    return true;
}

bool CL_E10_EliteAirMouse::requestI2CRecover() {
    // [R3-D-3] _reqGyroCalib는 volatile single-bit write. 락은 불필요하나
    //           "위임 플래그 접근 시 락 사용" 일관성 정책에 따라 유지.
    
    _lock();
    _reqI2CRecover = true;
    _unlock();
    return true;
}

bool CL_E10_EliteAirMouse::clearDiagnostics() {
    // [R3-D-3] _reqGyroCalib는 volatile single-bit write. 락은 불필요하나
    //           "위임 플래그 접근 시 락 사용" 일관성 정책에 따라 유지.
    
    // [D-3] 웹 태스크는 플래그만 설정. 실 클리어는 sensorTask가 담당
    //       (SPEC §상태 소유권: errHist/spikes/RMS 카운터는 sensorTask 소유)
    _lock();
    _reqClearDiag = true;
    _unlock();
    return true;
}

