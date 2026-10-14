// =======================================================
// File: src/v0415/E10_Def_0415.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : E10_Def_0415.h
 * 모듈약어 : E10
 * 모듈명 : Elite AirMouse - Definitions
 * ------------------------------------------------------
 * 기능 요약
 *  - EliteAirMouse(E10) 공통 상수/enum/struct 정의
 *  - HW GPIO 핀은 HW_Def_0415.h(SSOT)로 이관됨
 *
 * [v0415 주요 변경]
 *  - ST_E10_Status_t.ppt_mode 필드 삭제
 *  - Wave F: ST_E10_Status_t.ble_peer_name[32] 추가
 *  - Phase 1-1: E10_CONST namespace에 태스크/임계값 상수 추가
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 *   - namespace 명        : 모듈약어_ 접두사
 *   - namespace 내 상수    : 모듈약어 접두사 미사용
 *   - 전역 상수,매크로      : G_모듈약어_ 접두사
 *   - 전역 변수             : g_모듈약어_ 접두사
 *   - 전역 함수             : 모듈약어_ 접두사
 *   - typedef               : _t  접미사
 *   - enum 상수             : EN_모듈약어_ 접두사
 *   - 구조체                : ST_모듈약어_ 접두사
 *   - 클래스명              : CL_모듈약어_ 접두사 , 버전 제거
 *   - 클래스 private 멤버   : _ 접두사
 *   - 클래스 정적 멤버      : s_ 접두사
 *   - 함수 로컬 변수        : v_ 접두사
 *   - 함수 인자             : p_ 접두사
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <string.h>

// ======================================================
// 시스템 상수 (로직 전용)
// ======================================================
namespace E10_CONST {

// -------- 센서 캘리브레이션 --------
static constexpr uint32_t CALIB_MS       = 1000;
static constexpr float    CALIB_STILL_TH = 3.0f;

// -------- 이상치 감지 --------
static constexpr float SPIKE_TH_DEG = 650.0f;

// -------- 오류 이력 링버퍼 --------
static constexpr uint8_t ERR_HIST_CAP = 16;

// =====================================================
// [v0415 Phase 1-1] 태스크 주기 / 임계값 상수화
// -----------------------------------------------------
//  - 매직 넘버 제거 → SSOT
//  - Task/Budget 문서와 1:1 정합
// =====================================================

// -------- 태스크 주기 (ms) --------
static constexpr uint32_t SENSOR_TICK_MS = 8;
static constexpr uint32_t COMM_TICK_MS   = 7;
static constexpr uint32_t LED_TICK_MS    = 50;

// -------- 커먼 태스크 대기 (ms) --------
static constexpr uint32_t COMM_DISCONN_POLL_MS = 12;
static constexpr uint32_t COMM_GATE_POLL_MS    = 20;

// -------- 오버런 임계 (ms) --------
static constexpr float DT_OVERRUN_MS   = 16.0f;
static constexpr float COMM_OVERRUN_MS = 20.0f;

// -------- 카운터/드레인 임계 --------
static constexpr uint8_t I2C_FAIL_MAX      = 10;
static constexpr uint8_t ACTION_DRAIN_MAX  = 4;
static constexpr uint8_t NAN_RECOVER_EVERY = 5;
static constexpr uint8_t OVERRUN_LOG_EVERY = 10;

// -------- 락 타임아웃 --------
static constexpr uint32_t STATE_LOCK_TIMEOUT_MS = 2;

} // namespace E10_CONST

// ======================================================
// Keypage / Health
// ======================================================
enum EN_C10_KEYPAGE_t : uint8_t {
    EN_C10_KEYPAGE_KB       = 0,
    EN_C10_KEYPAGE_CONSUMER = 1
};

enum EN_E10_Health_t : uint8_t {
    EN_E10_HEALTH_OK       = 0,
    EN_E10_HEALTH_WARN     = 1,
    EN_E10_HEALTH_DEGRADED = 2
};

// ======================================================
// [M-4] OTA/SAFE 진입·이탈 전용 코드
// ======================================================
enum EN_E10_ErrCode_t : uint8_t {
    EN_E10_ERR_NONE             = 0,
    EN_E10_ERR_MPU_NAN          = 1,
    EN_E10_ERR_MUTEX_MISS       = 2,
    EN_E10_ERR_TASK_OVERRUN     = 3,
    EN_E10_ERR_I2C_RECOVER_OK   = 4,
    EN_E10_ERR_I2C_RECOVER_FAIL = 5,
    EN_E10_ERR_OTA_GUARD_ENTER  = 6,
    EN_E10_ERR_OTA_GUARD_EXIT   = 7,
    EN_E10_ERR_SAFE_MODE_ENTER  = 8,
    EN_E10_ERR_SAFE_MODE_EXIT   = 9
};

// ======================================================
// Motion FSM
//   State: 0=AIR, 1=SCROLL, 2=PPT, 3=PRECISION
//   [v0415] EN_FSM_SCROLL은 폐기됨 (관측 호환성 위해 enum 값 유지)
// ======================================================
enum EN_FSM_t : uint8_t {
    EN_FSM_AIR   = 0,
    EN_FSM_SCROLL = 1,
    EN_FSM_PPT   = 2,
    EN_FSM_PREC  = 3
};

// Precision sub: 0=OFF, 1=ENTRY, 2=TRACK, 3=EXIT
enum EN_PREC_SUB_t : uint8_t {
    EN_PREC_OFF   = 0,
    EN_PREC_ENTRY = 1,
    EN_PREC_TRACK = 2,
    EN_PREC_EXIT  = 3
};

enum EN_E10_MouseBtnMask_t : uint8_t {
    EN_E10_BTN_LEFT   = 0x01,
    EN_E10_BTN_RIGHT  = 0x02,
    EN_E10_BTN_MIDDLE = 0x04
};

// ======================================================
// Precision Profile
// ======================================================
struct ST_E10_PrecProfile_t {
    float   gain;
    uint8_t alpha;       // 0=필터없음, 255=최대 스무딩
    float   accel_limit; // 0=제한없음
};

static constexpr ST_E10_PrecProfile_t G_E10_PREC_PROFILES[] = {
    {1.00f,   0, 0.0f}, // OFF
    {0.85f,  64, 0.0f}, // LOW
    {0.70f, 128, 0.0f}, // MED
    {0.55f, 180, 0.0f}, // HIGH
    {0.45f, 210, 1.5f}, // PPT
};

// ======================================================
// 이벤트 / 상태 구조체
// ======================================================
struct ST_E10_ErrEvt_t {
    uint32_t ts_ms;
    uint8_t  code;
    uint16_t value;
};

struct ST_E10_SpikeEvt_t {
    uint32_t ts_ms;
};

struct ST_E10_State_t {
    int16_t x;
    int16_t y;
    int16_t wheel;
    bool    updated;
};

struct ST_E10_Status_t {
    bool    ble_connected;
    char    ble_peer_name[32];    // Wave F

    uint8_t active_mode;          // [v0415 Round M-1] ppt_mode → active_mode
    uint8_t dpi_level;

    // gate 상태
    bool     safe_mode;
    bool     ota_guard;
    uint32_t ota_guard_count;
    uint32_t ota_guard_uptime_ms;

    uint8_t precision_mode;

    uint8_t fsm_state;
    uint8_t fsm_sub;

    uint8_t  health;
    uint16_t health_score;

    float gyro_bias_x, gyro_bias_y, gyro_bias_z;
    float temp_c;

    float gyro_rms;
    float cursor_rms;

    uint32_t sampling_ms_target;
    float    sampling_ms_avg;

    uint32_t i2c_recover_count;
    bool     i2c_recover_last_ok;

    uint32_t err_mpu_nan;
    uint32_t err_mutex_miss;
    uint32_t err_task_overrun;

    uint8_t         err_hist_n;
    ST_E10_ErrEvt_t err_hist[E10_CONST::ERR_HIST_CAP];

    uint16_t spike_count_10s;
    uint16_t consecutive_fail;
    uint16_t consecutive_recover_fail;

    uint32_t uptime_ms;

    float gyro_bias_dyn_x, gyro_bias_dyn_y, gyro_bias_dyn_z;
    bool  drift_still_active;
    float out_smooth;

    uint32_t task_stack_sensor_min_words;
    uint32_t task_stack_comm_min_words;
    uint32_t task_stack_led_min_words;

    float    sensor_dt_max_ms;
    uint32_t sensor_overrun_count;

    float    comm_dt_avg_ms;
    float    comm_dt_max_ms;
    uint32_t comm_overrun_count;

    uint32_t failsafe_release_count;
};
