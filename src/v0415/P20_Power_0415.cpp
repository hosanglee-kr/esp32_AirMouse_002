// File: src/v0415/P20_Power_0415.cpp
// =======================================================
#include "P20_Power_0415.h"

#include <Wire.h>

#include "D10_Logger_0415.h"
#include "E10_Def_0415.h"
#include "HW_Def_0415.h"

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

// -------------------------------------------------------
// [v0415] MPU register 값 상수 (매직 비트 제거, L4-A2-07)
// -------------------------------------------------------
// PWR_MGMT_1
static constexpr uint8_t G_MPU_PWR1_WAKE         = 0x00;  // sleep 해제, 내부 osc
static constexpr uint8_t G_MPU_PWR1_CYCLE        = 0x20;  // cycle mode
static constexpr uint8_t G_MPU_PWR1_PLL_XGYRO    = 0x01;  // PLL w/ X Gyro ref

// PWR_MGMT_2
static constexpr uint8_t G_MPU_PWR2_LP_WAKE_5HZ  = 0xC7;  // LP_WAKE_CTRL=5Hz, accel only

// ACCEL_CONFIG
static constexpr uint8_t G_MPU_ACCEL_HPF_RESET   = 0x01;  // HPF 리셋 (WoM 초기화)

// INT_PIN_CFG (0x37)
static constexpr uint8_t G_MPU_INT_CFG_LATCH_EN  = 0x80;  // 래치 유지
static constexpr uint8_t G_MPU_INT_CFG_INT_OPEN  = 0x40;  // open-drain
static constexpr uint8_t G_MPU_INT_CFG_INT_LEVEL = 0x20;  // active-low

// INT_ENABLE (0x38)
static constexpr uint8_t G_MPU_INT_EN_MOTION     = 0x40;  // Motion Detection

// -------------------------------------------------------
// [v0415] MPU 안정화 지연
// -------------------------------------------------------
static constexpr uint16_t G_MPU_SETTLE_SHORT_MS = 2;
static constexpr uint16_t G_MPU_SETTLE_LONG_MS  = 5;

// -------------------------------------------------------
// I2C helpers
// -------------------------------------------------------
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
    // [v0415] fast_recalib_ms 삭제
}

void CL_P20_Power::begin() {
    _lastActivityMs = (uint32_t)millis();
    _lastWakeMs     = 0;
    _ext1Armed      = false;

    // MPU INT / 버튼 모두 RTC 도메인 풀업 (light-sleep 중 유지, HW_DEF 참조)
    rtc_gpio_pullup_en((gpio_num_t)HW_DEF::PIN_MPU_INT);
    rtc_gpio_pulldown_dis((gpio_num_t)HW_DEF::PIN_MPU_INT);

    for (uint8_t i = 0; i < HW_DEF::BTN_COUNT; i++) {
        rtc_gpio_pullup_en((gpio_num_t)HW_DEF::BTN_PINS[i]);
        rtc_gpio_pulldown_dis((gpio_num_t)HW_DEF::BTN_PINS[i]);
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
    // [v0415] _idle 갱신 삭제 (Dead)
}

// =======================================================
// MPU WoM 구성 (motion detection)
// =======================================================
bool CL_P20_Power::_prepareMpuWom() {
    // [R2-M-2] 각 write 실패 시 조기 반환 (I2C 장애 시 sleep 진입 방지)

    if (!mpuWr(G_MPU_PWR_MGMT_1, G_MPU_PWR1_WAKE)) {
        D10_LOGW("[P20] WoM prep fail: PWR_MGMT_1 wake");
        return false;
    }
    delay(G_MPU_SETTLE_SHORT_MS);

    if (!mpuWr(G_MPU_ACCEL_CFG, G_MPU_ACCEL_HPF_RESET)) {
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

    const uint8_t v_intCfg =
        G_MPU_INT_CFG_LATCH_EN | G_MPU_INT_CFG_INT_OPEN | G_MPU_INT_CFG_INT_LEVEL;
    if (!mpuWr(G_MPU_INT_CFG, v_intCfg)) {
        D10_LOGW("[P20] WoM prep fail: INT_CFG");
        return false;
    }

    if (!mpuWr(G_MPU_INT_EN, G_MPU_INT_EN_MOTION)) {
        D10_LOGW("[P20] WoM prep fail: INT_EN");
        return false;
    }

    if (!mpuWr(G_MPU_PWR_MGMT_1, G_MPU_PWR1_CYCLE)) {
        D10_LOGW("[P20] WoM prep fail: PWR_MGMT_1 cycle");
        return false;
    }

    if (!mpuWr(G_MPU_PWR_MGMT_2, G_MPU_PWR2_LP_WAKE_5HZ)) {
        D10_LOGW("[P20] WoM prep fail: PWR_MGMT_2 LP_WAKE");
        return false;
    }
    delay(G_MPU_SETTLE_LONG_MS);

    (void)mpuRd(G_MPU_INT_STATUS);   // 초기 래치 클리어
    return true;
}

// =======================================================
// Wake 후 MPU 정상 모드 복귀
// -------------------------------------------------------
// [v0415 L4-A3-13] p_sleepOccurred=false(UNDEFINED) 시 skip
//   - esp_light_sleep_start()가 실패하거나 즉시 리턴된 경우
//     MPU는 아직 정상 상태이므로 register 재설정 불필요.
// =======================================================
void CL_P20_Power::_restoreMpuAfterWake(bool p_sleepOccurred) {
    if (!p_sleepOccurred) return;

    // 1) INT_STATUS 읽어 래치 클리어
    (void)mpuRd(G_MPU_INT_STATUS);

    // 2) INT_ENABLE: motion 인터럽트 해제
    mpuWr(G_MPU_INT_EN, 0x00);

    // 3) PWR_MGMT_1: PLL with X Gyro ref (정상)
    mpuWr(G_MPU_PWR_MGMT_1, G_MPU_PWR1_PLL_XGYRO);

    // 4) PWR_MGMT_2: Gyro/Accel 정상 (standby 해제)
    mpuWr(G_MPU_PWR_MGMT_2, 0x00);
    delay(G_MPU_SETTLE_LONG_MS);
}

// =======================================================
// EXT1 wake arm (Safe/Pairing 분기)
// -------------------------------------------------------
// [v0415 Phase 1 방안 2-A]
//   - p_safeOrPairing=true  → buildWakeMaskSafe()   (Side C 단독)
//   - p_safeOrPairing=false → buildWakeMaskNormal() (MPU + 6버튼)
// =======================================================
void CL_P20_Power::_armExt1(bool p_safeOrPairing) {
    const uint64_t v_mask = p_safeOrPairing
        ? HW_DEF::buildWakeMaskSafe()
        : HW_DEF::buildWakeMaskNormal();

    esp_sleep_enable_ext1_wakeup(v_mask, ESP_EXT1_WAKEUP_ANY_LOW);
    _ext1Armed = true;
}

// =======================================================
// 실제 Light-sleep 진입
// =======================================================
bool CL_P20_Power::sleepNow(uint32_t p_nowMs, bool p_safeOrPairing) {
    // [C-4] Wake 후 최소 활성 시간 보장 (Thrashing 방지)
    if (_lastWakeMs != 0) {
        const uint16_t v_minActive = (_cfg.wake_min_active_ms > 0) ? _cfg.wake_min_active_ms : 500;
        if ((p_nowMs - _lastWakeMs) < v_minActive) return false;
    }

    // MPU WoM 준비 실패 시 sleep 금지
    if (!_prepareMpuWom()) return false;

    // [v0415] Safe/Pairing 분기 마스크
    _armExt1(p_safeOrPairing);

    D10_LOGI("[P20] entering light sleep (idle=%ums, safeOrPair=%d)",
             (unsigned)(p_nowMs - _lastActivityMs),
             (int)p_safeOrPairing);

    esp_light_sleep_start();

    // ---- wake ----
    const esp_sleep_wakeup_cause_t v_cause = esp_sleep_get_wakeup_cause();
    const bool v_sleepOccurred = (v_cause != ESP_SLEEP_WAKEUP_UNDEFINED);

    // [v0415 L4-A3-13] UNDEFINED 경로 가드
    _restoreMpuAfterWake(v_sleepOccurred);

    _lastWakeMs     = (uint32_t)millis();
    _lastActivityMs = _lastWakeMs;
    _ext1Armed      = false;

    // [v0415] _wakeCount / _lastWakeReason 삭제 (Phase 4 Q5-b)
    //   wake 통계는 esp_sleep_get_wakeup_cause()로 on-demand 확인 가능

    D10_LOGI("[P20] woke: cause=%d",
             (int)v_cause);

    // [C-3] commTask 즉시 통지
    if (_wakeCb) {
        _wakeCb(_wakeCtx);
    }

    return true;
}

// =======================================================
// [N-1] Deep-sleep 진입 (Safe 분기)
// =======================================================
bool CL_P20_Power::deepSleepNow(uint32_t p_nowMs,
                                bool p_hidConnected,
                                bool p_pairing,
                                bool p_safe) {
    if (_cfg.deep_idle_timeout_ms == 0) return false;
    if (p_hidConnected || p_pairing)    return false;
    if ((p_nowMs - _lastActivityMs) < _cfg.deep_idle_timeout_ms) return false;

    D10_LOGI("[P20] entering deep-sleep (idle=%ums, safe=%d)",
             (unsigned)(p_nowMs - _lastActivityMs),
             (int)p_safe);

    // [v0415] Safe 분기 마스크
    //   - Safe  → buildWakeMaskSafe()    (Side C 단독)
    //   - 일반  → buildWakeMaskButtons() (6버튼, MPU 제외)
    const uint64_t v_mask = p_safe
        ? HW_DEF::buildWakeMaskSafe()
        : HW_DEF::buildWakeMaskButtons();

    esp_sleep_enable_ext1_wakeup(v_mask, ESP_EXT1_WAKEUP_ANY_LOW);
    esp_deep_sleep_start();

    // [v0415 L4-A3-11] unreachable — esp_deep_sleep_start()는 리턴하지 않음
    //   컴파일러 경고 방지 및 API 일관성을 위해 false 반환.
    return false;
}
