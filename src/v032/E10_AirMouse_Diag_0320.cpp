// =======================================================
// File: E10_AirMouse_Diag_0320.cpp
// =======================================================
#include "E10_AirMouse_0320.h"

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

    _i2cRecoverCount++;
    _i2cRecoverLastOk = v_ok;

    if (v_ok) {
        _consecutiveRecoverFail = 0;
    } else {
        _consecutiveRecoverFail++;
        _consecutiveFail++;
    }

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

        // 캘리브 중 버튼 상태 반영(이동/휠 0)
        {
            uint8_t v_btn = 0;
            if (digitalRead(E10_CONST::PIN_BTN_L) == LOW) v_btn |= (uint8_t)EN_E10_BTN_LEFT;
            if (digitalRead(E10_CONST::PIN_BTN_R) == LOW) v_btn |= (uint8_t)EN_E10_BTN_RIGHT;
            if (digitalRead(E10_CONST::PIN_BTN_M) == LOW) v_btn |= (uint8_t)EN_E10_BTN_MIDDLE;

            ST_E10_Frame_t v_fr;
            memset(&v_fr, 0, sizeof(v_fr));
            v_fr.btn_mask = v_btn;
            v_fr.updated  = false;
            _pushFrame(v_fr);
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }

    if (v_cnt > 0) {
        _gyroBiasX = (float)(v_sx / v_cnt);
        _gyroBiasY = (float)(v_sy / v_cnt);
        _gyroBiasZ = (float)(v_sz / v_cnt);
    }

    _gyroCalibDone = true;
}

// =======================================================
// Status
// =======================================================
void CL_E10_EliteAirMouse::getStatus(ST_E10_Status_t& p_out) {
    memset(&p_out, 0, sizeof(p_out));
    _lock();

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

    p_out.gyro_bias_x = _gyroBiasX;
    p_out.gyro_bias_y = _gyroBiasY;
    p_out.gyro_bias_z = _gyroBiasZ;
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
    uint16_t v_score = 1000;
    v_score = (uint16_t)max(0, (int)v_score - (int)(p_out.gyro_rms * 25.0f));
    v_score = (uint16_t)max(0, (int)v_score - (int)(p_out.cursor_rms * 18.0f));
    v_score = (uint16_t)max(0, (int)v_score - (int)min((uint32_t)400, p_out.err_mpu_nan * 20));
    v_score = (uint16_t)max(0, (int)v_score - (int)min((uint32_t)300, p_out.i2c_recover_count * 35));
    v_score = (uint16_t)max(0, (int)v_score - (int)min((uint32_t)300, (uint32_t)p_out.spike_count_10s * 12));
    v_score = (uint16_t)max(0, (int)v_score - (int)min((uint32_t)400, (uint32_t)p_out.consecutive_fail * 18));

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
    _lock();
    _reqGyroCalib = true;
    _unlock();
    return true;
}

bool CL_E10_EliteAirMouse::requestI2CRecover() {
    _lock();
    _reqI2CRecover = true;
    _unlock();
    return true;
}

bool CL_E10_EliteAirMouse::clearDiagnostics() {
    _lock();

    _errMpuNan = 0;
    _errMutexMiss = 0;
    _errTaskOverrun = 0;

    _gyroN = 0; _gyroMean = 0.0; _gyroM2 = 0.0;
    _curN  = 0; _curMean  = 0.0; _curM2  = 0.0;

    _i2cRecoverCount  = 0;
    _i2cRecoverLastOk = true;

    memset(_errHist, 0, sizeof(_errHist));
    _errHistHead  = 0;
    _errHistCount = 0;

    memset(_spikes, 0, sizeof(_spikes));
    _spikeHead  = 0;
    _spikeCount = 0;

    _consecutiveFail = 0;
    _consecutiveRecoverFail = 0;

    _state.updated = true;
    _reqClearDiag  = true;

    _unlock();
    return true;
}
