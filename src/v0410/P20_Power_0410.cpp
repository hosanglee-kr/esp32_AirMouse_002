// =======================================================
// File: src/v0410/P20_Power_0410.cpp
// =======================================================
#include "P20_Power_0410.h"

#include <Wire.h>

#include "D10_Logger_0410.h"
#include "E10_Def_0410.h"

// -------------------------------------------------------
// MPU6050 register (raw I2C, driver 미경유)
// -------------------------------------------------------
static constexpr uint8_t G_MPU_ADDR       = 0x68;
static constexpr uint8_t G_MPU_PWR_MGMT_1 = 0x6B;
static constexpr uint8_t G_MPU_PWR_MGMT_2 = 0x6C;
static constexpr uint8_t G_MPU_ACCEL_CFG  = 0x1C;
static constexpr uint8_t G_MPU_MOT_THR    = 0x1F;
static constexpr uint8_t G_MPU_MOT_DUR    = 0x20;
static constexpr uint8_t G_MPU_INT_CFG    = 0x37;
static constexpr uint8_t G_MPU_INT_EN     = 0x38;
static constexpr uint8_t G_MPU_INT_STATUS = 0x3A;

static inline bool mpuWr(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(G_MPU_ADDR);
    Wire.write(reg);
    Wire.write(val);
    return (Wire.endTransmission() == 0);
}

static inline uint8_t mpuRd(uint8_t reg) {
    Wire.beginTransmission(G_MPU_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)G_MPU_ADDR, (uint8_t)1);
    return Wire.available() ? (uint8_t)Wire.read() : 0;
}

// =======================================================
// EXT1 wake mask (MPU INT + 6 버튼)
//   모든 소스가 LOW active
// =======================================================
static constexpr uint64_t G_EXT1_MASK =
    (1ULL << 6)  |   // MPU INT1
    (1ULL << 7)  |   // Side R
    (1ULL << 12) |   // Top L
    (1ULL << 13) |   // Side C
    (1ULL << 14) |   // Side F
    (1ULL << 15) |   // Top R
    (1ULL << 16);    // Top M

// =======================================================
// ctor / begin
// =======================================================
CL_P20_Power::CL_P20_Power() {
    _cfg.idle_timeout_ms[0]      = 60000;
    _cfg.idle_timeout_ms[1]      = 120000;
    _cfg.idle_timeout_ms[2]      = 300000;
    _cfg.idle_timeout_ble_ms     = 300000;
    _cfg.pairing_idle_timeout_ms = 30000;
    _cfg.deep_idle_timeout_ms    = 600000;
    _cfg.wake_min_active_ms      = 500;
    _cfg.wom_threshold           = 25;
    _cfg.wom_duration            = 4;
    _cfg.fast_recalib_ms         = 300;
}

void CL_P20_Power::begin() {
    _lastActivityMs = (uint32_t)millis();
    _lastWakeMs     = 0;
    _idle           = false;
    _ext1Armed      = false;

    // MPU INT / 버튼 모두 RTC 도메인 풀업 (light-sleep 중 유지)
    rtc_gpio_pullup_en((gpio_num_t)6);
    rtc_gpio_pulldown_dis((gpio_num_t)6);

    const gpio_num_t v_btns[] = {
        (gpio_num_t)7, (gpio_num_t)12, (gpio_num_t)13,
        (gpio_num_t)14, (gpio_num_t)15, (gpio_num_t)16
    };
    for (auto p : v_btns) {
        rtc_gpio_pullup_en(p);
        rtc_gpio_pulldown_dis(p);
    }

    D10_LOGI("[P20] begin: idle_timeout_m1=%ums", (unsigned)_cfg.idle_timeout_ms[0]);
}

void CL_P20_Power::setConfig(const ST_Config_t& p_cfg) {
    _cfg = p_cfg;
    D10_LOGI("[P20] config applied: M1=%u M2=%u M3=%u BLE=%u deep=%u",
             (unsigned)_cfg.idle_timeout_ms[0],
             (unsigned)_cfg.idle_timeout_ms[1],
             (unsigned)_cfg.idle_timeout_ms[2],
             (unsigned)_cfg.idle_timeout_ble_ms,
             (unsigned)_cfg.deep_idle_timeout_ms);
}


// =======================================================
// 활동 알림
// =======================================================
void CL_P20_Power::notifyActivity(uint32_t p_nowMs) {
    _lastActivityMs = p_nowMs;
    _idle           = false;
}

// =======================================================
// MPU WoM 구성 (motion detection)
// =======================================================
bool CL_P20_Power::_prepareMpuWom() {
    // [R2-M-2] 각 write 실패 시 조기 반환 (I2C 장애 시 sleep 진입 방지)
    if (!mpuWr(G_MPU_PWR_MGMT_1, 0x00)) {
        D10_LOGW("[P20] WoM prep fail: PWR_MGMT_1 wake");
        return false;
    }
    delay(2);

    if (!mpuWr(G_MPU_ACCEL_CFG, 0x01)) {
        D10_LOGW("[P20] WoM prep fail: ACCEL_CFG HPF");
        return false;
    }

    const uint8_t v_th  = (_cfg.wom_threshold > 0) ? _cfg.wom_threshold : 25;
    if (!mpuWr(G_MPU_MOT_THR, v_th)) {
        D10_LOGW("[P20] WoM prep fail: MOT_THR");
        return false;
    }

    const uint8_t v_dur = (_cfg.wom_duration > 0) ? _cfg.wom_duration : 4;
    if (!mpuWr(G_MPU_MOT_DUR, v_dur)) {
        D10_LOGW("[P20] WoM prep fail: MOT_DUR");
        return false;
    }

    if (!mpuWr(G_MPU_INT_CFG, 0x80 | 0x40 | 0x20)) {
        D10_LOGW("[P20] WoM prep fail: INT_CFG");
        return false;
    }

    if (!mpuWr(G_MPU_INT_EN, 0x40)) {
        D10_LOGW("[P20] WoM prep fail: INT_EN");
        return false;
    }

    if (!mpuWr(G_MPU_PWR_MGMT_1, 0x20)) {
        D10_LOGW("[P20] WoM prep fail: PWR_MGMT_1 cycle");
        return false;
    }

    if (!mpuWr(G_MPU_PWR_MGMT_2, 0xC7)) {
        D10_LOGW("[P20] WoM prep fail: PWR_MGMT_2 LP_WAKE");
        return false;
    }
    delay(5);

    (void)mpuRd(G_MPU_INT_STATUS);   // 초기 래치 클리어
    return true;
}

// =======================================================
// Wake 후 MPU 정상 모드 복귀
// =======================================================
void CL_P20_Power::_restoreMpuAfterWake() {
    // 1) INT_STATUS 읽어 래치 클리어
    (void)mpuRd(G_MPU_INT_STATUS);

    // 2) INT_ENABLE: motion 인터럽트 해제
    mpuWr(G_MPU_INT_EN, 0x00);

    // 3) PWR_MGMT_1: PLL with X Gyro ref (정상)
    mpuWr(G_MPU_PWR_MGMT_1, 0x01);

    // 4) PWR_MGMT_2: Gyro/Accel 정상 (standby 해제)
    mpuWr(G_MPU_PWR_MGMT_2, 0x00);
    delay(5);
}

// =======================================================
// EXT1 wake arm
// =======================================================
void CL_P20_Power::_armExt1() {
    esp_sleep_enable_ext1_wakeup(G_EXT1_MASK, ESP_EXT1_WAKEUP_ANY_LOW);
    _ext1Armed = true;
}

// =======================================================
// 실제 sleep 진입
// =======================================================
bool CL_P20_Power::sleepNow(uint32_t p_nowMs) {
    // [C-4] Wake 후 최소 활성 시간 보장 (Thrashing 방지)
    if (_lastWakeMs != 0) {
        const uint16_t v_minActive = (_cfg.wake_min_active_ms > 0) ? _cfg.wake_min_active_ms : 500;
        if ((p_nowMs - _lastWakeMs) < v_minActive) return false;
    }

    // MPU WoM 준비 실패 시 sleep 금지
    if (!_prepareMpuWom()) return false;

    _armExt1();

    D10_LOGI("[P20] entering light sleep (idle=%ums)", (unsigned)(p_nowMs - _lastActivityMs));

    esp_light_sleep_start();

    // ---- wake ----
    const esp_sleep_wakeup_cause_t v_cause = esp_sleep_get_wakeup_cause();
    const uint64_t v_status = esp_sleep_get_ext1_wakeup_status();

    _restoreMpuAfterWake();

    _lastWakeMs     = (uint32_t)millis();
    _lastActivityMs = _lastWakeMs;
    _idle           = false;
    _ext1Armed      = false;
    _wakeCount++;

    if (v_cause == ESP_SLEEP_WAKEUP_EXT1) {
        if (v_status & (1ULL << 6)) {
            _lastWakeReason = EN_WAKE_MPU_MOTION;
        } else {
            _lastWakeReason = EN_WAKE_BUTTON;
        }
    } else if (v_cause != ESP_SLEEP_WAKEUP_UNDEFINED) {
        _lastWakeReason = EN_WAKE_OTHER;
    }

    D10_LOGI("[P20] woke: reason=%d cause=%d status=0x%llx count=%u",
             (int)_lastWakeReason, (int)v_cause, (unsigned long long)v_status, (unsigned)_wakeCount);

    // [C-3] commTask 즉시 통지
    if (_wakeCb) {
        _wakeCb(_wakeCtx);
    }

    return true;
}

// =======================================================
// [N-1] Deep-sleep 진입
// =======================================================
bool CL_P20_Power::deepSleepNow(uint32_t p_nowMs, bool p_hidConnected, bool p_pairing) {
    if (_cfg.deep_idle_timeout_ms == 0) return false;
    if (p_hidConnected || p_pairing)    return false;
    if ((p_nowMs - _lastActivityMs) < _cfg.deep_idle_timeout_ms) return false;

    D10_LOGI("[P20] entering deep-sleep (idle=%ums)", (unsigned)(p_nowMs - _lastActivityMs));

    // 버튼 wake 만 유지 (MPU INT 제외, 6개 버튼)
    const uint64_t v_btnMask =
        (1ULL << 7)  | (1ULL << 12) | (1ULL << 13) |
        (1ULL << 14) | (1ULL << 15) | (1ULL << 16);

    esp_sleep_enable_ext1_wakeup(v_btnMask, ESP_EXT1_WAKEUP_ANY_LOW);
    esp_deep_sleep_start();
    return true;
}
