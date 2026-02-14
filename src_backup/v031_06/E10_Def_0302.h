// =======================================================
// File: src/v010/E10_Def_0302.h
// =======================================================
#pragma once

/*
 * ------------------------------------------------------
 * 소스명 : E10_Def_0302.h
 * 모듈약어 : E10
 * 모듈명 : Elite AirMouse - Definitions
 * ------------------------------------------------------
 * 기능 요약
 *  - EliteAirMouse(E10) 공통 상수/enum/struct 정의
 *  - E10_EliteAirMouse_0301.h 등에서 include 하여 사용
 * ------------------------------------------------------
 * [구현 규칙]
 *  - 항상 소스 시작 주석 부분 체계 유지 및 내용 업데이트
 *  - 소스 시작 주석 부분 구현규칙, 코드네이밍규칙 내용 그대로 유지, 수정금지
 *  - ArduinoJson v7.x.x 사용 (v6 이하 사용 금지)
 *  - JsonDocument 단일 타입만 사용
 *  - createNestedArray/Object/containsKey 사용 금지
 *  - memset + strlcpy 기반 안전 초기화
 *  - 주석/필드명은 JSON 구조와 동일하게 유지
 *  - 변수명은 가능한 해석 가능하게
 * ------------------------------------------------------
 * [코드 네이밍 규칙]
 *   - namespace 명        : 모듈약어_ 접두사
 *   - namespace 내 상수    : 모둘약어 접두시 미사용
 *   - 전역 상수,매크로      : G_모듈약어_ 접두사
 *   - 전역 변수             : g_모듈약어_ 접두사
 *   - 전역 함수             : 모듈약어_ 접두사
 *   - type                  : T_모듈약어_ 접두사
 *   - typedef               : _t  접미사
 *   - enum 상수             : EN_모듈약어_ 접두사
 *   - 구조체                : ST_모듈약어_ 접두사
 *   - 클래스명              : CL_모듈약어_ 접두사 , 버전 제거
 *   - 클래스 private 멤버 함수/변수   : _ 접두사
 *   - 클래스 멤버(함수/변수) : 모듈약어 접두사 미사용
 *   - 클래스 정적 멤버      : s_ 접두사
 *   - 함수 로컬 변수        : v_ 접두사
 *   - 함수 인자             : p_ 접두사
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <string.h>

// -------- 시스템 상수 --------
namespace E10_CONST {
    static constexpr int      PIN_BTN_L      = 12;
    static constexpr int      PIN_BTN_R      = 15;
    static constexpr int      PIN_BTN_M      = 16;
    
    static constexpr int      PIN_BTN_MODE   = 13;
    static constexpr int      PIN_BTN_SCROLL = 14;
    
    // class-static fixed pins (board wiring)
    static constexpr int      PIN_I2C_SDA = 4;
    static constexpr int      PIN_I2C_SCL = 5;
     
    

    // static constexpr uint8_t  MOUSE_BTN_LEFT = 0x01;

    static constexpr uint32_t CALIB_MS       = 1000;
    static constexpr float    CALIB_STILL_TH = 3.0f;

    static constexpr float    SPIKE_TH_DEG   = 650.0f;

    static constexpr uint8_t  ERR_HIST_CAP   = 16;
    static constexpr uint8_t  SPIKE_CAP      = 32;
}

enum EN_E10_Health_t : uint8_t {
    EN_E10_HEALTH_OK = 0,
    EN_E10_HEALTH_WARN = 1,
    EN_E10_HEALTH_DEGRADED = 2
};

enum EN_E10_ErrCode_t : uint8_t {
    EN_E10_ERR_NONE             = 0,
    EN_E10_ERR_MPU_NAN          = 1,
    EN_E10_ERR_MUTEX_MISS       = 2,
    EN_E10_ERR_TASK_OVERRUN     = 3,
    EN_E10_ERR_I2C_RECOVER_OK   = 4,
    EN_E10_ERR_I2C_RECOVER_FAIL = 5,
    EN_E10_ERR_OTA_GUARD        = 6
};

// -------- Motion FSM --------
// State: 0=AIR, 1=SCROLL, 2=PPT, 3=PRECISION
enum EN_FSM_t : uint8_t {
    EN_FSM_AIR = 0,
    EN_FSM_SCROLL = 1,
    EN_FSM_PPT = 2,
    EN_FSM_PREC = 3
};

// Precision sub: 0=OFF, 1=ENTRY, 2=TRACK, 3=EXIT
enum EN_PREC_SUB_t : uint8_t {
    EN_PREC_OFF = 0,
    EN_PREC_ENTRY = 1,
    EN_PREC_TRACK = 2,
    EN_PREC_EXIT = 3
};

enum EN_E10_MouseBtnMask_t : uint8_t {
    EN_E10_BTN_LEFT = 0x01,
    EN_E10_BTN_RIGHT = 0x02, 
    EN_E10_BTN_MIDDLE = 0x04 
};


struct ST_E10_PrecProfile_t {
    float   gain;
    uint8_t alpha;       // 0=필터없음, 255=최대 스무딩
    float   accel_limit; // 0=제한없음, 값이 낮을수록 급변 억제
};

static constexpr ST_E10_PrecProfile_t G_E10_PREC_PROFILES[] = {
    /* OFF  */ {1.00f,   0, 0.0f},
    /* LOW  */ {0.85f,  64, 0.0f},
    /* MED  */ {0.70f, 128, 0.0f},
    /* HIGH */ {0.55f, 180, 0.0f},
    /* PPT  */ {0.45f, 210, 1.5f},
};


struct ST_E10_ErrEvt_t {
    uint32_t ts_ms;
    uint8_t  code;
    uint16_t value;
};

struct ST_E10_SpikeEvt_t {
    uint32_t ts_ms;
};

// (0301) 상태 전달용: int16_t로 고정(전송 시 -127~127로 clamp)
struct ST_E10_State_t {
    int16_t x;
    int16_t y;
    int16_t wheel;
    uint8_t btn_mask; // EN_E10_MouseBtnMask_t OR-mask
    bool updated;
};

struct ST_E10_Status_t {
    bool    ble_connected;
    bool    ppt_mode;
    uint8_t dpi_level;
    
    uint8_t btn_mask;
    
    // ---- (C) gate 상태 노출 ----
    bool    safe_mode;          // 현재 SafeMode 게이트
    bool    ota_guard;          // OTA Guard 게이트
    uint32_t ota_guard_count;   // OTA guard 진입 횟수
    uint32_t ota_guard_uptime_ms; // 마지막 OTA guard 진입 후 경과(ms)
    

    uint8_t precision_mode; //
    // bool    precision_enable;
    // bool    precision_mode;
    
    uint8_t fsm_state; // 디버깅용
    uint8_t fsm_sub;   // precision substate

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

    // (0301) 고급 안정화 진단
    float gyro_bias_dyn_x, gyro_bias_dyn_y, gyro_bias_dyn_z;
    bool  drift_still_active;
    float out_smooth;
};

