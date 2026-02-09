// =======================================================
// File: src/v010/C10_Def_0301.h
// =======================================================
#pragma once

#include <Arduino.h>



static constexpr uint16_t G_C10_CFG_VER = 274;

enum EN_C10_WIFI_MODE_t : uint8_t {
    EN_C10_WIFI_AUTO = 0,
    EN_C10_WIFI_AP   = 1,
    EN_C10_WIFI_STA  = 2
};

struct ST_C10_WiFiConfig_t {
    uint8_t mode;
    char sta_ssid[33];
    char sta_pass[65];
    char ap_ssid[33];
    char ap_pass[65];
    char mdns_host[33];
};

// (기존) ppt_keys.*
struct ST_C10_PptKey_t {
    uint8_t  mod;
    uint16_t key;
};

// (신규) ppt_keys2.*
enum EN_C10_KEYPAGE_t : uint8_t {
    EN_C10_KEYPAGE_KB = 0,
    EN_C10_KEYPAGE_CONSUMER = 1
};

struct ST_C10_PptKey2_t {
    uint8_t  page;   // EN_C10_KEYPAGE_t
    uint8_t  mod;    // kb 전용
    uint32_t code;   // kb=usage(<=0xE7), consumer=mask(32bit)
};

struct ST_C10_E10Config_t {
    uint8_t dpi_level;
    bool hard_click_lock;

    float scale_base[3];
    float accel_gain[3];
    float accel_threshold;

    float wheel_threshold_deg;
    uint8_t wheel_step_max;

    float gesture_flick_deg;
    uint16_t gesture_cooldown_ms;

    float scroll_cursor_damp;

    bool  precision_enable;
    float precision_deadzone;
    float precision_gain;
    float precision_accel;
    uint8_t precision_max_step;
    float precision_smooth;

	uint16_t prec_entry_ms;          // 정밀모드 진입 판정 유지시간(ms)
	uint16_t prec_exit_ms;           // 정밀모드 이탈 판정 유지시간(ms)
	float    prec_entry_still_deg;   // 진입: “정지” 판정 각속도(deg/s) 임계
	float    prec_exit_move_deg;     // 이탈: “움직임” 판정 각속도(deg/s) 임계
	uint8_t  prec_profile;           // 정밀모드 프로파일(0=default, 1=soft, 2=hard...)

    // (기존) ppt_keys
    ST_C10_PptKey_t ppt_start;
    ST_C10_PptKey_t ppt_exit;
    ST_C10_PptKey_t ppt_next;
    ST_C10_PptKey_t ppt_prev;
    ST_C10_PptKey_t ppt_black;
    ST_C10_PptKey_t ppt_laser;

    // (신규) ppt_keys2
    ST_C10_PptKey2_t ppt2_start;
    ST_C10_PptKey2_t ppt2_exit;
    ST_C10_PptKey2_t ppt2_next;
    ST_C10_PptKey2_t ppt2_prev;
    ST_C10_PptKey2_t ppt2_black;
    ST_C10_PptKey2_t ppt2_laser;
};

struct ST_C10_BootState_t {
    bool safe_mode;
    uint8_t fail_count;
    bool pending;
};
