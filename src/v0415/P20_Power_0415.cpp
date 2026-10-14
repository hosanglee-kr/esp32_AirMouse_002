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
static uint8_t           G_MPU_ADDR       = 0x68;
static constexpr uint8_t G_MPU_PWR_MGMT_1 = 0x6B;
static constexpr uint8_t G_MPU_PWR_MGMT_2 = 0x6C;
static constexpr uint8_t G_MPU_ACCEL_CFG  = 0x1C;
static constexpr uint8_t G_MPU_MOT_THR    = 0x1F;
static constexpr uint8_t G_MPU_MOT_DUR    = 0x20;
static constexpr uint8_t G_MPU_INT_CFG    = 0x37;
static constexpr uint8_t G_MPU_INT_EN     = 0x38;
static constexpr uint8_t G_MPU_INT_STATUS = 0x3A;

// -------------------------------------------------------
// MPU register 값 상수
// -------------------------------------------------------
// PWR_MGMT_1
static constexpr uint8_t G_MPU_PWR1_WAKE      = 0x00;  // sleep 해제, 내부 osc
static constexpr uint8_t G_MPU_PWR1_CYCLE     = 0x20;  // cycle mode
static constexpr uint8_t G_MPU_PWR1_PLL_XGYRO = 0x01;  // PLL w/ X Gyro ref
static constexpr uint8_t G_MPU_PWR1_SLEEP     = 0x40;  // deep-sleep 시 사용

// PWR_MGMT_2
static constexpr uint8_t G_MPU_PWR2_LP_WAKE_5HZ = 0xC7;  // LP_WAKE_CTRL=5Hz

// ACCEL_CONFIG
static constexpr uint8_t G_MPU_ACCEL_HPF_RESET = 0x01;  // HPF 리셋

// INT_PIN_CFG (0x37)
static constexpr uint8_t G_MPU_INT_CFG_LATCH_EN  = 0x80;  // 래치 유지
static constexpr uint8_t G_MPU_INT_CFG_INT_OPEN  = 0x40;  // open-drain
static constexpr uint8_t G_MPU_INT_CFG_INT_LEVEL = 0x20;  // active-low

// INT_ENABLE (0x38)
static constexpr uint8_t G_MPU_INT_EN_MOTION = 0x40;  // Motion Detection

// -------------------------------------------------------
// MPU 안정화 지연
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
// [v0415 Phase 2-4] WoM 초기화 시퀀스 테이블
// -------------------------------------------------------
//  - 순서 중요: PWR_MGMT_1(wake) → ACCEL_CFG → MOT_THR
//              → MOT_DUR → INT_CFG → INT_EN
//              → PWR_MGMT_1(cycle) → PWR_MGMT_2(LP_WAKE)
//  - val = 0 인 항목은 "동적" (loop에서 config 기반 결정)
//  - delayAfterMs = 0은 지연 없음
// =======================================================
namespace {

struct ST_WomSeq_t {
    uint8_t     reg;
    uint8_t     val;             // 0 = 동적 (MOT_THR / MOT_DUR / INT_CFG)
    const char* name;
    uint16_t    delayAfterMs;
};

static const ST_WomSeq_t G_WOM_SEQ[] = {
    {G_MPU_PWR_MGMT_1, G_MPU_PWR1_WAKE,        "PWR_MGMT_1 wake",    2},
    {G_MPU_ACCEL_CFG,  G_MPU_ACCEL_HPF_RESET,  "ACCEL_CFG HPF",      0},
    {G_MPU_MOT_THR,    0,                      "MOT_THR",            0},
    {G_MPU_MOT_DUR,    0,                      "MOT_DUR",            0},
    {G_MPU_INT_CFG,    0,                      "INT_CFG",            0},
    {G_MPU_INT_EN,     G_MPU_INT_EN_MOTION,    "INT_EN",             0},
    {G_MPU_PWR_MGMT_1, G_MPU_PWR1_CYCLE,       "PWR_MGMT_1 cycle",   0},
    {G_MPU_PWR_MGMT_2, G_MPU_PWR2_LP_WAKE_5HZ, "PWR_MGMT_2 LP_WAKE", 5},
};

static constexpr size_t G_WOM_SEQ_COUNT =
    sizeof(G_WOM_SEQ) / sizeof(G_WOM_SEQ[0]);

}  // namespace

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
}

void CL_P20_Power::begin() {
    _lastActivityMs = (uint32_t)millis();
    _lastWakeMs     = 0;
    _ext1Armed      = false;

    rtc_gpio_pullup_en((gpio_num_t)HW_DEF::PIN_MPU_INT);
    rtc_gpio_pulldown_dis((gpio_num_t)HW_DEF::PIN_MPU_INT);

    for (uint8_t i = 0; i < HW_DEF::BTN_COUNT; i++) {
        rtc_gpio_pullup_en((gpio_num_t)HW_DEF::PIN_BTN_PINS[i]);
        rtc_gpio_pulldown_dis((gpio_num_t)HW_DEF::PIN_BTN_PINS[i]);
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

void CL_P20_Power::setMpuAddr(uint8_t p_addr) {
    G_MPU_ADDR = p_addr;
}

// =======================================================
// 활동 알림
// =======================================================
void CL_P20_Power::notifyActivity(uint32_t p_nowMs) {
    _lastActivityMs = p_nowMs;
}

// =======================================================
// [v0415 Phase 2-4] MPU WoM 구성 (motion detection)
// -------------------------------------------------------
//  - 테이블 기반 루프
//  - 동적 값: MOT_THR / MOT_DUR / INT_CFG는 config 기반 결정
//  - 실패 시 조기 반환 (I2C 장애 시 sleep 진입 방지)
//  - 순서·로그·지연·초기 래치 클리어 원본과 동일
// =======================================================
bool CL_P20_Power::_prepareMpuWom() {
    // 동적 값 사전 계산
    const uint8_t v_th  = (_cfg.wom_threshold > 0) ? _cfg.wom_threshold : 25;
    const uint8_t v_dur = (_cfg.wom_duration  > 0) ? _cfg.wom_duration  : 4;
    const uint8_t v_intCfg =
        G_MPU_INT_CFG_LATCH_EN | G_MPU_INT_CFG_INT_OPEN | G_MPU_INT_CFG_INT_LEVEL;

    // 테이블 순회
    for (size_t i = 0; i < G_WOM_SEQ_COUNT; i++) {
        const ST_WomSeq_t& s = G_WOM_SEQ[i];

        // 동적 값 결정 (테이블의 val==0은 "동적" 의미)
        uint8_t v_val = s.val;
        if (s.reg == G_MPU_MOT_THR) {
            v_val = v_th;
        } else if (s.reg == G_MPU_MOT_DUR) {
            v_val = v_dur;
        } else if (s.reg == G_MPU_INT_CFG) {
            v_val = v_intCfg;
        }

        if (!mpuWr(s.reg, v_val)) {
            D10_LOGW("[P20] WoM prep fail: %s", s.name);
            return false;
        }

        if (s.delayAfterMs > 0) {
            delay(s.delayAfterMs);
        }
    }

    // 초기 래치 클리어
    (void)mpuRd(G_MPU_INT_STATUS);
    return true;
}

// =======================================================
// Wake 후 MPU 정상 모드 복귀
// =======================================================
void CL_P20_Power::_restoreMpuAfterWake(bool p_sleepOccurred) {
    if (!p_sleepOccurred) return;

    Wire.end();
    delay(G_MPU_SETTLE_SHORT_MS);
    Wire.begin(HW_DEF::PIN_I2C_SDA, HW_DEF::PIN_I2C_SCL);
    Wire.setClock(400000);
    delay(G_MPU_SETTLE_SHORT_MS);

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
// =======================================================
void CL_P20_Power::_armExt1(bool p_safeOrPairing) {
    const uint64_t v_mask = p_safeOrPairing
        ? HW_DEF::buildWakeMaskSafe()
        : HW_DEF::buildWakeMaskNormal();

    esp_sleep_enable_ext1_wakeup(v_mask, ESP_EXT1_WAKEUP_ANY_LOW);
    _ext1Armed = true;
}

// =======================================================
// Light-sleep 진입
// =======================================================
bool CL_P20_Power::sleepNow(uint32_t p_nowMs, bool p_safeOrPairing) {
    // Wake 후 최소 활성 시간 보장 (Thrashing 방지)
    if (_lastWakeMs != 0) {
        const uint16_t v_minActive = (_cfg.wake_min_active_ms > 0)
                                     ? _cfg.wake_min_active_ms : 500;
        if ((p_nowMs - _lastWakeMs) < v_minActive) return false;
    }

    // MPU WoM 준비 실패 시 sleep 금지
    if (!_prepareMpuWom()) return false;

    // Safe/Pairing 분기 마스크
    _armExt1(p_safeOrPairing);

    D10_LOGI("[P20] entering light sleep (idle=%ums, safeOrPair=%d)",
             (unsigned)(p_nowMs - _lastActivityMs),
             (int)p_safeOrPairing);

    esp_light_sleep_start();

    // ---- wake ----
    const esp_sleep_wakeup_cause_t v_cause = esp_sleep_get_wakeup_cause();

    // MPU 복원
    _restoreMpuAfterWake(true);

    _lastWakeMs     = (uint32_t)millis();
    _lastActivityMs = _lastWakeMs;
    _ext1Armed      = false;

    D10_LOGI("[P20] woke: cause=%d", (int)v_cause);

    // commTask 즉시 통지
    if (_wakeCb) {
        _wakeCb(_wakeCtx);
    }

    return true;
}

// =======================================================
// Deep-sleep 진입 (Safe 분기)
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

    // MPU6050 sleep bit 설정 (~4mA 절감)
    (void)mpuWr(G_MPU_PWR_MGMT_1, G_MPU_PWR1_SLEEP);

    // Safe 분기 마스크
    const uint64_t v_mask = p_safe
        ? HW_DEF::buildWakeMaskSafe()
        : HW_DEF::buildWakeMaskButtons();

    esp_sleep_enable_ext1_wakeup(v_mask, ESP_EXT1_WAKEUP_ANY_LOW);
    esp_deep_sleep_start();

    // unreachable
    return false;
}
