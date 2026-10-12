// =======================================================
// File: src/v0415/E10_AirMouse_Diag_0415.cpp
// =======================================================
#include "E10_AirMouse_0415.h"
#include "HW_Def_0415.h"

// =======================================================
// [v0415] I2C 복구 매직 넘버 상수화 (L6e-A2-01)
// =======================================================
namespace {
static constexpr int      G_DIAG_I2C_UNLOCK_PULSES   = 9;
static constexpr uint32_t G_DIAG_I2C_HALF_PERIOD_US  = 6;
static constexpr uint32_t G_DIAG_I2C_SETTLE_SHORT_MS = 5;
static constexpr uint32_t G_DIAG_CALIB_TICK_MS       = 5;
static constexpr uint32_t G_DIAG_CALIB_MIN_SAMPLES   = 1;  // v_cnt > 0 조건
}  // namespace

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
    pinMode(HW_DEF::PIN_I2C_SDA, INPUT_PULLUP);
    pinMode(HW_DEF::PIN_I2C_SCL, OUTPUT_OPEN_DRAIN);

    // 표준 I2C bus unlock: 9 clock pulses
    for (int v_i = 0; v_i < G_DIAG_I2C_UNLOCK_PULSES; v_i++) {
        digitalWrite(HW_DEF::PIN_I2C_SCL, HIGH);
        delayMicroseconds(G_DIAG_I2C_HALF_PERIOD_US);
        digitalWrite(HW_DEF::PIN_I2C_SCL, LOW);
        delayMicroseconds(G_DIAG_I2C_HALF_PERIOD_US);
    }
    digitalWrite(HW_DEF::PIN_I2C_SCL, HIGH);
    delayMicroseconds(G_DIAG_I2C_HALF_PERIOD_US);

    Wire.end();
    delay(G_DIAG_I2C_SETTLE_SHORT_MS);

    Wire.begin(HW_DEF::PIN_I2C_SDA, HW_DEF::PIN_I2C_SCL);
    Wire.setClock(400000);
    delay(G_DIAG_I2C_SETTLE_SHORT_MS);

    bool v_ok = _mpu.begin(_mpuAddr, &Wire);
    if (v_ok) {
        _mpu.setGyroRange(MPU6050_RANGE_250_DEG);
        _mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
        _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    }

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

    _pushErr(v_ok ? EN_E10_ERR_I2C_RECOVER_OK : EN_E10_ERR_I2C_RECOVER_FAIL, 0);
    return v_ok;
}

// =======================================================
// [v0415] MPU I2C 상세 진단 및 버스 자동 스캔/복구
// =======================================================
bool CL_E10_EliteAirMouse::_diagAndRecoverMpu() {
    Serial.println();
    Serial.println("========================================================");
    Serial.println("   [E10][Diag] MPU6050 I2C Auto-Scan & Diagnostic Start  ");
    Serial.println("========================================================");

    // 1. 물리 핀 전압 레벨 검사 (내부 풀업 활성화 후 측정)
    pinMode(HW_DEF::PIN_I2C_SDA, INPUT_PULLUP);
    pinMode(HW_DEF::PIN_I2C_SCL, INPUT_PULLUP);
    delay(5);
    const int v_sdaLvl = digitalRead(HW_DEF::PIN_I2C_SDA);
    const int v_sclLvl = digitalRead(HW_DEF::PIN_I2C_SCL);

    Serial.printf("[E10][Diag] Line status: SDA(GP%d)=%s, SCL(GP%d)=%s\n",
                  HW_DEF::PIN_I2C_SDA, v_sdaLvl ? "HIGH (Normal)" : "LOW (STUCK/GND!)",
                  HW_DEF::PIN_I2C_SCL, v_sclLvl ? "HIGH (Normal)" : "LOW (STUCK/GND!)");
    if (!v_sdaLvl) {
        Serial.println("[E10][Diag]  ⚠️ Warning: SDA is held LOW. Possible bus hang or wiring short.");
    }
    if (!v_sclLvl) {
        Serial.println("[E10][Diag]  ⚠️ Warning: SCL is held LOW. Possible clock line short.");
    }

    // 2. 표준 I2C 9-클럭 펄스 버스 잠금 해제 (Bus Unlock)
    Serial.println("[E10][Diag] Sending 9 I2C clock pulses to unlock bus...");
    pinMode(HW_DEF::PIN_I2C_SCL, OUTPUT_OPEN_DRAIN);
    for (int v_i = 0; v_i < G_DIAG_I2C_UNLOCK_PULSES; v_i++) {
        digitalWrite(HW_DEF::PIN_I2C_SCL, HIGH);
        delayMicroseconds(G_DIAG_I2C_HALF_PERIOD_US);
        digitalWrite(HW_DEF::PIN_I2C_SCL, LOW);
        delayMicroseconds(G_DIAG_I2C_HALF_PERIOD_US);
    }
    digitalWrite(HW_DEF::PIN_I2C_SCL, HIGH);
    delayMicroseconds(G_DIAG_I2C_HALF_PERIOD_US);

    // 3. I2C 버스 스캔 (100kHz 안정 모드, 다중 라운드)
    uint8_t v_devices[8] = {0};
    uint8_t v_devCount   = 0;
    constexpr int G_MAX_SCAN_ROUNDS = 3;

    for (int v_round = 1; v_round <= G_MAX_SCAN_ROUNDS; v_round++) {
        Wire.end();
        delay(10);
        Wire.begin(HW_DEF::PIN_I2C_SDA, HW_DEF::PIN_I2C_SCL);
        Wire.setClock(100000);  // 진단용 100kHz 저속 안정 통신
        delay(10);

        v_devCount = 0;
        Serial.printf("[E10][Diag] Scanning I2C bus (Round %d/%d)...\n", v_round, G_MAX_SCAN_ROUNDS);
        for (uint8_t v_addr = 1; v_addr < 127; v_addr++) {
            Wire.beginTransmission(v_addr);
            const uint8_t v_err = Wire.endTransmission();
            if (v_err == 0) {
                Serial.printf("[E10][Diag]  -> Detected I2C device at 0x%02X (%d)\n", v_addr, v_addr);
                if (v_devCount < 8) v_devices[v_devCount++] = v_addr;
            }
        }

        if (v_devCount > 0) {
            break;
        }

        if (v_round < G_MAX_SCAN_ROUNDS) {
            Serial.println("[E10][Diag]  No devices detected yet. Retrying in 1.5s (check wiring)...");
            delay(1500);
        }
    }

    if (v_devCount == 0) {
        Serial.println("[E10][Diag] ❌ NO I2C devices detected on the bus!");
        Serial.println("[E10][Diag] [Hardware Checklist]:");
        Serial.printf("[E10][Diag]  1) SDA pin must be ESP32 GPIO %d\n", HW_DEF::PIN_I2C_SDA);
        Serial.printf("[E10][Diag]  2) SCL pin must be ESP32 GPIO %d\n", HW_DEF::PIN_I2C_SCL);
        Serial.println("[E10][Diag]  3) Check if SDA and SCL jumper wires are swapped");
        Serial.println("[E10][Diag]  4) Check VCC (3.3V or 5V) and GND connections & LED on module");
        Serial.println("========================================================");
        return false;
    }

    // 4. 감지된 주소의 WHO_AM_I 레지스터 (0x75) 확인
    uint8_t v_bestAddr = 0;
    for (uint8_t v_i = 0; v_i < v_devCount; v_i++) {
        const uint8_t v_addr = v_devices[v_i];
        Wire.beginTransmission(v_addr);
        Wire.write(0x75);
        if (Wire.endTransmission(false) == 0) {
            Wire.requestFrom(v_addr, (uint8_t)1);
            if (Wire.available()) {
                const uint8_t v_who = Wire.read();
                Serial.printf("[E10][Diag] Device 0x%02X -> WHO_AM_I (reg 0x75) = 0x%02X", v_addr, v_who);
                if (v_who == 0x68) {
                    Serial.println(" (MPU-6050 genuine)");
                    if (v_bestAddr == 0) v_bestAddr = v_addr;
                } else if (v_who == 0x70) {
                    Serial.println(" (MPU-6500 - Note: Adafruit MPU6050 expects 0x68)");
                    if (v_bestAddr == 0) v_bestAddr = v_addr;
                } else if (v_who == 0x71) {
                    Serial.println(" (MPU-9250)");
                    if (v_bestAddr == 0) v_bestAddr = v_addr;
                } else if (v_who == 0x72 || v_who == 0x98) {
                    Serial.println(" (MPU variant/clone)");
                    if (v_bestAddr == 0) v_bestAddr = v_addr;
                } else {
                    Serial.println(" (Other I2C device)");
                }
            }
        }
    }

    // 5. Adafruit_MPU6050::begin 재시도 (최우선 후보 주소 먼저 시도)
    uint8_t v_tryAddrs[2] = {0x68, 0x69};
    if (v_bestAddr == 0x69) {
        v_tryAddrs[0] = 0x69;
        v_tryAddrs[1] = 0x68;
    }

    for (int v_t = 0; v_t < 2; v_t++) {
        const uint8_t v_target = v_tryAddrs[v_t];
        Serial.printf("[E10][Diag] Attempting MPU begin at 0x%02X (100kHz)...\n", v_target);
        if (_mpu.begin(v_target, &Wire)) {
            Serial.printf("[E10][Diag] ✅ MPU initialized successfully at 0x%02X!\n", v_target);
            _mpuAddr = v_target;
            _power.setMpuAddr(v_target);
            Wire.setClock(400000);  // 정상 운용 클럭(400kHz) 복귀
            Serial.println("========================================================");
            return true;
        }
    }

    Serial.println("[E10][Diag] ❌ MPU begin failed even though I2C device responded.");
    Serial.println("[E10][Diag]    If WHO_AM_I was != 0x68, your module may be an MPU6500/clone.");
    Serial.println("========================================================");
    return false;
}

// =======================================================
// 캘리브: 강제 릴리즈 + 버튼 샘플링
// =======================================================

void CL_E10_EliteAirMouse::_runGyroCalibration() {
    // [v0415 LC-A/OPT-E] v_rel dead code 삭제
    //   - 이전 v_rel(updated=true, x/y/wheel/pan=0)은 commTask에서 _mouseSend(0,0,0,0) → no-op
    //   - 캘리브 1초 동안 커서는 자연 정지 (프레임 push 없음)
    //   - SPEC §"캘리브 중 프레임 유지" 표현은 오류 → v0416에서 정정 예정

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

        // [v0415 LC-B] 캘리브 중 btn_mask push 삭제 (dead code)
        //   - commTask는 v_upd=false 프레임의 btn_mask를 사용 안 함
        //   - 물리 버튼은 _btnDisp.update()가 별도 처리 (프레임과 무관)

        // [D-2] 캘리브 1초 블로킹 중 BLE pairing 타임아웃 검사 유지
        _ble.tick(_hid.isConnected());
        vTaskDelay(pdMS_TO_TICKS(G_DIAG_CALIB_TICK_MS));
        
    }

    // [v0415 L6e-A3-02] v_cnt==0 시 로그 (실패 피드백)
    if (v_cnt > 0) {
        _biasTracker.setBias(
            (float)(v_sx / v_cnt),
            (float)(v_sy / v_cnt),
            (float)(v_sz / v_cnt));
    } else {
        D10_LOGW("[E10] gyro calib: no still samples (motion during 1s)");
        _pushErr(EN_E10_ERR_NONE, 0);   // 히스토리 이벤트 마커
    }

    _gyroCalibDone = true;
}

// =======================================================
// Status
// -------------------------------------------------------
// [v0415] ppt_mode 필드 노출 제거 (Round G/K)
//   - active_mode는 /api/status의 config.profile_idx + 별도 W10 status 확장으로 대체
//   - 본 함수에서 E10_Status_t.ppt_mode 미설정 (구조체에서 삭제됨)
// =======================================================

void CL_E10_EliteAirMouse::getStatus(ST_E10_Status_t& p_out) {
    memset(&p_out, 0, sizeof(p_out));
    _lock();

    // [R3-D-5] _hid read 예외 (STATE §1)
    p_out.ble_connected = _hid.isConnected();

    // [v0415 Round M-1] ppt_mode → active_mode
    //   - E10 _isPptMode 필드 삭제 (Round G)
    //   - PPT 판정은 active_mode == 2 로 일원화
    p_out.active_mode = _activeMode;
    p_out.dpi_level   = (uint8_t)_dpiLevel;

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
    // [v0415 rev13 / B-2] LED 태스크 스택 (실측)
    p_out.task_stack_led_min_words    = _stackLedMinWords;

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

    // health score (클램프 헬퍼)
    auto v_deduce = [](int p_score, uint32_t p_count, uint32_t p_mult, uint32_t p_cap) -> int {
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
    _lock();
    _reqGyroCalib = true;
    // [D-1] _biasTracker.reset()은 sensorTask로 위임
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
    // [D-3] 웹 태스크는 플래그만 설정
    _lock();
    _reqClearDiag = true;
    _unlock();
    return true;
}
