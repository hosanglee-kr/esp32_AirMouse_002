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
 *    · _isPptMode 필드 삭제(Round G) 반영
 *    · PPT 상태는 active_mode(=2)로 일원화 → /api/status의 config.profile_idx
 *      및 별도 active_mode 노출 (Round M에서 W10 status 확장)
 *  - 나머지 상수/enum/struct 유지
 * ------------------------------------------------------
 * [구현 규칙]
 *  - 항상 소스 시작 주석 부분 체계 유지 및 내용 업데이트
 *  - ArduinoJson v7.x.x 사용 (v6 이하 사용 금지)
 *  - JsonDocument 단일 타입만 사용
 *  - createNestedArray/Object/containsKey 사용 금지
 *  - memset + strlcpy 기반 안전 초기화
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

// -------- 시스템 상수 (로직 전용) --------
namespace E10_CONST {

// 센서 캘리브레이션
static constexpr uint32_t CALIB_MS       = 1000;
static constexpr float    CALIB_STILL_TH = 3.0f;

// 이상치 감지
static constexpr float SPIKE_TH_DEG = 650.0f;

// 오류 이력 링버퍼
static constexpr uint8_t ERR_HIST_CAP = 16;
} // namespace E10_CONST

enum EN_C10_KEYPAGE_t : uint8_t { EN_C10_KEYPAGE_KB = 0, EN_C10_KEYPAGE_CONSUMER = 1 };

enum EN_E10_Health_t : uint8_t { EN_E10_HEALTH_OK = 0, EN_E10_HEALTH_WARN = 1, EN_E10_HEALTH_DEGRADED = 2 };

// [M-4] OTA/SAFE 진입·이탈 전용 코드
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

// -------- Motion FSM --------
// State: 0=AIR, 1=SCROLL, 2=PPT, 3=PRECISION
// [v0415] EN_FSM_SCROLL은 폐기됨 (SCROLL은 Front Hold로 재설계)
//        관측 호환성을 위해 enum 값은 유지 (Round M W10 status 노출 정합)
enum EN_FSM_t : uint8_t { EN_FSM_AIR = 0, EN_FSM_SCROLL = 1, EN_FSM_PPT = 2, EN_FSM_PREC = 3 };

// Precision sub: 0=OFF, 1=ENTRY, 2=TRACK, 3=EXIT
enum EN_PREC_SUB_t : uint8_t { EN_PREC_OFF = 0, EN_PREC_ENTRY = 1, EN_PREC_TRACK = 2, EN_PREC_EXIT = 3 };

enum EN_E10_MouseBtnMask_t : uint8_t { EN_E10_BTN_LEFT = 0x01, EN_E10_BTN_RIGHT = 0x02, EN_E10_BTN_MIDDLE = 0x04 };

struct ST_E10_PrecProfile_t {
    float   gain;
    uint8_t alpha;       // 0=필터없음, 255=최대 스무딩
    float   accel_limit; // 0=제한없음, 값이 낮을수록 급변 억제
};

static constexpr ST_E10_PrecProfile_t G_E10_PREC_PROFILES[] = {
    {1.00f,   0, 0.0f}, // OFF
    {0.85f,  64, 0.0f}, // LOW
    {0.70f, 128, 0.0f}, // MED
    {0.55f, 180, 0.0f}, // HIGH
    {0.45f, 210, 1.5f}, // PPT
};

struct ST_E10_ErrEvt_t {
    uint32_t ts_ms;
    uint8_t  code;
    uint16_t value;
};

struct ST_E10_SpikeEvt_t {
    uint32_t ts_ms;
};

// 상태 전달용: int16_t 고정(전송 시 -127~127로 clamp)
struct ST_E10_State_t {
    int16_t x;
    int16_t y;
    int16_t wheel;
    uint8_t btn_mask; // EN_E10_MouseBtnMask_t OR-mask
    bool    updated;
};

struct ST_E10_Status_t {
    bool    ble_connected;
    // [v0415 Round M-1] ppt_mode 삭제 → active_mode 일원화
    //   · E10 _isPptMode 필드 삭제 (Round G)
    //   · W10 status 노출은 active_mode (1=PC, 2=PPT, 3=TV)
    uint8_t active_mode;        // ← 신규 추가
    uint8_t dpi_level;

    uint8_t btn_mask;

    // ---- gate 상태 ----
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
    uint32_t task_stack_led_min_words;    // Round K에서 추가됨

    float    sensor_dt_max_ms;
    uint32_t sensor_overrun_count;

    float    comm_dt_avg_ms;
    float    comm_dt_max_ms;
    uint32_t comm_overrun_count;

    uint32_t failsafe_release_count;
};
