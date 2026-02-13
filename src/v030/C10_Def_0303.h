// =======================================================
// File: src/v010/C10_Def_0303.h
// =======================================================
#pragma once
#include <Arduino.h>
#include <string.h>

// -------------------------------------------------------
// [Global constants / macros] (규칙: G_모듈약어_)
// -------------------------------------------------------
static constexpr uint16_t G_C10_CFG_VER = 303; // 스키마 버전(권장: 파일명과 분리)

namespace C10_DEF {
    // ---------- Paths ----------
    static constexpr const char* CFG_PATH = "/json/config_301.json";
    static constexpr const char* CFG_TMP  = "/json/config_0301.json.tmp";
    static constexpr const char* CFG_BAK  = "/json/config_0301.json.bak";

    static constexpr const char* BOOT_PATH     = "/json/boot_state_0300.json";
    static constexpr const char* BOOT_TMP_PATH = "/json/boot_state_0300.json.tmp";

    static constexpr uint8_t SAFE_FAIL_THRESHOLD = 2; // 2회 연속 실패 시 safe_mode

    // ---------- Limits ----------
    static constexpr size_t STA_SSID_MAX = 32;
    static constexpr size_t STA_PASS_MAX = 64;
    static constexpr size_t AP_SSID_MAX  = 32;
    static constexpr size_t AP_PASS_MAX  = 64;
    static constexpr size_t MDNS_MAX     = 32;
}

// ---------- Enums / Structs ----------
enum EN_C10_WIFI_MODE_t : uint8_t {
    EN_C10_WIFI_AUTO = 0,
    EN_C10_WIFI_AP   = 1,
    EN_C10_WIFI_STA  = 2
};

// (NEW) E10 precision mode contract (owned by C10 config schema)
enum EN_C10_E10PrecisionMode_t : uint8_t {
    EN_C10_E10_PREC_OFF  = 0,    // 정밀 기능 OFF (기본)
    EN_C10_E10_PREC_LOW  = 1,     // 약하게 안정화
    EN_C10_E10_PREC_MED  = 2,    // 기본 정밀(권장)
    EN_C10_E10_PREC_HIGH = 3,    // 강한 안정화(손떨림 큰 사용자)
    EN_C10_E10_PREC_PPT  = 4,    // PPT 포인터 특화(최강 안정화 + 가속 억제)
    EN_C10_E10_PREC_MAX
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
    EN_C10_KEYPAGE_KB       = 0,
    EN_C10_KEYPAGE_CONSUMER = 1
};

struct ST_C10_PptKey2_t {
    uint8_t  page;   // EN_C10_KEYPAGE_t
    uint8_t  mod;    // kb 전용 (policy: consumer면 0)
    uint32_t code;   // kb=usage(<=0xE7), consumer=mask(32bit)
};

struct ST_C10_E10Config_t {
    uint8_t dpi_level;
    bool    hard_click_lock;

    float scale_base[3];
    float accel_gain[3];
    float accel_threshold;

    float   wheel_threshold_deg;
    uint8_t wheel_step_max;

    float    gesture_flick_deg;
    uint16_t gesture_cooldown_ms;

    float scroll_cursor_damp;

    uint8_t precision_mode; // EN_C10_E10PrecisionMode_t
    //bool    precision_enable;
    
    float   precision_deadzone;
    float   precision_gain;
    float   precision_accel;
    uint8_t precision_max_step;
    float   precision_smooth;

    uint16_t prec_entry_ms;
    uint16_t prec_exit_ms;
    float    prec_entry_still_deg;
    float    prec_exit_move_deg;
    uint8_t  prec_profile;

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
    bool    safe_mode;
    uint8_t fail_count;
    bool    pending;
    
    // (NEW) 이번 부팅 시작 시각(업타임 기준 millis)
    uint32_t boot_ms;
    uint8_t  last_reset_reason;
};
