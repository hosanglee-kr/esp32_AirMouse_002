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

static inline void mpuWr(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(G_MPU_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
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
CL_P20_Power::CL_P20_Power() {}

void CL_P20_Power::begin() {
    _lastActivityMs = (uint32_t)millis();
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

    D10_LOGI("[P20] begin: idle_timeout=%ums", (unsigned)_idleTimeoutMs);
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
    // 1) PWR_MGMT_1: 내부 8MHz osc, sleep 해제
    mpuWr(G_MPU_PWR_MGMT_1, 0x00);
    delay(2);

    // 2) ACCEL_CONFIG: HPF 5Hz (중력 성분 제거)
    mpuWr(G_MPU_ACCEL_CFG, 0x01);

    // 3) MOT_THR: ~100mg (테이블 진동 방지)
    mpuWr(G_MPU_MOT_THR, 25);

    // 4) MOT_DUR: 4ms 연속 (스파이크 노이즈 필터)
    mpuWr(G_MPU_MOT_DUR, 4);

    // 5) INT_PIN_CFG: LATCH | OPEN_DRAIN | ACTIVE_LOW
    mpuWr(G_MPU_INT_CFG, 0x80 | 0x40 | 0x20);

    // 6) INT_ENABLE: MOT_EN
    mpuWr(G_MPU_INT_EN, 0x40);

    // 7) PWR_MGMT_1: CYCLE=1, SLEEP=0 (저전력 가속도계 사이클)
    mpuWr(G_MPU_PWR_MGMT_1, 0x20);

    // 8) PWR_MGMT_2: LP_WAKE_CTRL=5Hz, Gyro Standby
    mpuWr(G_MPU_PWR_MGMT_2, 0xC7);
    delay(5);

    // 검증: INT_STATUS 읽기 (초기 래치 클리어)
    (void)mpuRd(G_MPU_INT_STATUS);
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

    // 참고: Adafruit_MPU6050 드라이버는 재begin 필요할 수 있음.
    //       E10 sensorTask에서 다음 프레임에 이상 감지 시 _recoverI2C()로 복구.
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
    if (_idleTimeoutMs == 0) return false;              // 비활성
    if ((p_nowMs - _lastActivityMs) < _idleTimeoutMs) return false;

    // MPU WoM 준비 실패 시 sleep 금지
    if (!_prepareMpuWom()) return false;

    _armExt1();

    D10_LOGI("[P20] entering light sleep (idle=%ums)", (unsigned)(p_nowMs - _lastActivityMs));

    esp_light_sleep_start();

    // ---- wake ----
    const esp_sleep_wakeup_cause_t v_cause = esp_sleep_get_wakeup_cause();
    const uint64_t v_status = esp_sleep_get_ext1_wakeup_status();

    _restoreMpuAfterWake();

    D10_LOGI("[P20] woke: cause=%d status=0x%llx",
             (int)v_cause, (unsigned long long)v_status);

    _lastActivityMs = (uint32_t)millis();
    _idle           = false;
    _ext1Armed      = false;
    return true;
}
