// =======================================================
// File: src/v0271/C10_Config_0271.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : C10_Config_0271.h
 * 모듈약어 : C10
 * 모듈명 : Config Manager (Atomic Save, .bak Rollback, Export/Import, Validate/Normalize, PPT Keymap v2)
 * ------------------------------------------------------
 * 기능 요약
 *  - P0: Atomic 저장(tmp→rename) + .bak 백업/롤백
 *  - Export/Import + patch(Partial) + UI 편집용 Validate/Normalize
 *  - (v0271) WiFi/E10 전체 편집 UI 대응(/api/config/ui)
 *  - (v023~) ppt_keys2: Keyboard/Consumer(Media) 공통 키맵 유지
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
 *   - 클래스 private 멤버   : _ 접두사
 *   - 클래스 멤버(함수/변수) : 모듈약어 접두사 미사용
 *   - 클래스 정적 멤버      : s_ 접두사
 *   - 함수 로컬 변수        : v_ 접두사
 *   - 함수 인자             : p_ 접두사
 * ------------------------------------------------------
 */

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <string.h>

static constexpr uint16_t G_C10_CFG_VER = 271;

enum EN_C10_WIFI_MODE_t : uint8_t {
    EN_C10_WIFI_AUTO = 0,
    EN_C10_WIFI_AP   = 1,
    EN_C10_WIFI_STA  = 2
};

struct ST_C10_WiFiConfig_t {
    uint8_t mode;          // wifi.mode
    char sta_ssid[33];     // wifi.sta.ssid
    char sta_pass[65];     // wifi.sta.pass
    char ap_ssid[33];      // wifi.ap.ssid
    char ap_pass[65];      // wifi.ap.pass
    char mdns_host[33];    // wifi.mdns.host
};

// (기존) ppt_keys.*
struct ST_C10_PptKey_t {
    uint8_t  mod;
    uint16_t key;
};

// (신규) ppt_keys2.*
enum EN_C10_KEYPAGE_t : uint8_t {
    EN_C10_KEYPAGE_KB = 0,       // Keyboard page(0x07), code=usage id(0x00~0xE7)
    EN_C10_KEYPAGE_CONSUMER = 1  // Consumer(Media), code=32-bit mask
};

struct ST_C10_PptKey2_t {
    uint8_t  page;   // EN_C10_KEYPAGE_t
    uint8_t  mod;    // kb 전용(Modifier byte mask)
    uint32_t code;   // kb=usage(<=0xE7), consumer=mask(32bit)
};

struct ST_C10_E10Config_t {
    uint8_t dpi_level;          // e10.dpi_level (1~3)
    bool hard_click_lock;       // e10.hard_click_lock

    float scale_base[3];        // e10.scale_base[3]
    float accel_gain[3];        // e10.accel_gain[3]
    float accel_threshold;      // e10.accel_threshold

    float wheel_threshold_deg;  // e10.wheel.threshold_deg
    uint8_t wheel_step_max;     // e10.wheel.step_max

    float gesture_flick_deg;    // e10.gesture.flick_deg
    uint16_t gesture_cooldown_ms;// e10.gesture.cooldown_ms

    float scroll_cursor_damp;   // e10.scroll_cursor_damp

    bool  precision_enable;     // e10.precision.enable
    float precision_deadzone;   // e10.precision.deadzone
    float precision_gain;       // e10.precision.gain
    float precision_accel;      // e10.precision.accel
    uint8_t precision_max_step; // e10.precision.max_step
    float precision_smooth;     // e10.precision.smooth

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

// UI에서 범위/가이드 제공용(고정 값)
struct ST_C10_E10Ranges_t {
    uint8_t dpi_min, dpi_max;

    float scale_min, scale_max;
    float accel_gain_min, accel_gain_max;
    float accel_th_min, accel_th_max;

    float wheel_th_min, wheel_th_max;
    uint8_t wheel_step_min, wheel_step_max;

    float flick_min, flick_max;
    uint16_t cooldown_min, cooldown_max;

    float damp_min, damp_max;

    float prec_dead_min, prec_dead_max;
    float prec_gain_min, prec_gain_max;
    float prec_acc_min, prec_acc_max;
    uint8_t prec_step_min, prec_step_max;
    float prec_smooth_min, prec_smooth_max;
};

class CL_C10_Config {
  private:
    static constexpr const char* s_path_cfg = "/json/config_0271.json";
    static constexpr const char* s_path_tmp = "/json/config_0271.json.tmp";
    static constexpr const char* s_path_bak = "/json/config_0271.json.bak";

  public:
    void begin(bool p_formatOnFail = true) {
        (void)LittleFS.begin(p_formatOnFail);
        if (!LittleFS.exists("/json")) (void)LittleFS.mkdir("/json");

        if (!LittleFS.exists(s_path_cfg)) {
            ST_C10_WiFiConfig_t v_w;
            ST_C10_E10Config_t  v_e;
            makeDefaultsWiFi(v_w);
            makeDefaultsE10(v_e);
            normalizeWiFi(v_w);
            normalizeE10(v_e);
            (void)saveAll(v_w, v_e);
        } else {
            // 파일은 있는데 깨졌을 수 있으니 검증 실패면 defaults로 복구
            if (!verifyJsonFile(s_path_cfg)) {
                ST_C10_WiFiConfig_t v_w;
                ST_C10_E10Config_t  v_e;
                makeDefaultsWiFi(v_w);
                makeDefaultsE10(v_e);
                normalizeWiFi(v_w);
                normalizeE10(v_e);
                (void)saveAll(v_w, v_e);
            }
        }
    }

    void makeDefaultsWiFi(ST_C10_WiFiConfig_t& p_out) {
        memset(&p_out, 0, sizeof(p_out));
        p_out.mode = (uint8_t)EN_C10_WIFI_AUTO;
        strlcpy(p_out.ap_ssid, "EliteAirMouse", sizeof(p_out.ap_ssid));
        strlcpy(p_out.ap_pass, "12345678", sizeof(p_out.ap_pass));
        strlcpy(p_out.mdns_host, "elite-airmouse", sizeof(p_out.mdns_host));
    }

    void makeDefaultsE10(ST_C10_E10Config_t& p_out) {
        memset(&p_out, 0, sizeof(p_out));

        p_out.dpi_level = 2;
        p_out.hard_click_lock = true;

        p_out.scale_base[0] = 0.55f; p_out.scale_base[1] = 0.75f; p_out.scale_base[2] = 1.00f;
        p_out.accel_gain[0] = 0.35f; p_out.accel_gain[1] = 0.55f; p_out.accel_gain[2] = 0.85f;
        p_out.accel_threshold = 8.0f;

        p_out.wheel_threshold_deg = 90.0f;
        p_out.wheel_step_max = 6;

        p_out.gesture_flick_deg = 200.0f;
        p_out.gesture_cooldown_ms = 600;

        p_out.scroll_cursor_damp = 0.25f;

        p_out.precision_enable = false;
        p_out.precision_deadzone = 1.2f;
        p_out.precision_gain = 0.65f;
        p_out.precision_accel = 0.25f;
        p_out.precision_max_step = 18;
        p_out.precision_smooth = 0.85f;

        // (기존) defaults
        p_out.ppt_start = {0x02, 0x3E}; // Shift+F5
        p_out.ppt_exit  = {0x00, 0x29}; // Esc
        p_out.ppt_next  = {0x00, 0x4E}; // PageDown
        p_out.ppt_prev  = {0x00, 0x4B}; // PageUp
        p_out.ppt_black = {0x00, 0x05}; // B
        p_out.ppt_laser = {0x01, 0x0F}; // Ctrl+L

        // (신규) defaults: kb로 mirror
        p_out.ppt2_start = { (uint8_t)EN_C10_KEYPAGE_KB, p_out.ppt_start.mod, (uint32_t)p_out.ppt_start.key };
        p_out.ppt2_exit  = { (uint8_t)EN_C10_KEYPAGE_KB, p_out.ppt_exit.mod,  (uint32_t)p_out.ppt_exit.key };
        p_out.ppt2_next  = { (uint8_t)EN_C10_KEYPAGE_KB, p_out.ppt_next.mod,  (uint32_t)p_out.ppt_next.key };
        p_out.ppt2_prev  = { (uint8_t)EN_C10_KEYPAGE_KB, p_out.ppt_prev.mod,  (uint32_t)p_out.ppt_prev.key };
        p_out.ppt2_black = { (uint8_t)EN_C10_KEYPAGE_KB, p_out.ppt_black.mod, (uint32_t)p_out.ppt_black.key };
        p_out.ppt2_laser = { (uint8_t)EN_C10_KEYPAGE_KB, p_out.ppt_laser.mod, (uint32_t)p_out.ppt_laser.key };
    }

    void getRangesE10(ST_C10_E10Ranges_t& p_out) {
        memset(&p_out, 0, sizeof(p_out));
        p_out.dpi_min = 1; p_out.dpi_max = 3;

        p_out.scale_min = 0.10f; p_out.scale_max = 2.50f;
        p_out.accel_gain_min = 0.00f; p_out.accel_gain_max = 2.50f;
        p_out.accel_th_min = 0.50f; p_out.accel_th_max = 40.0f;

        p_out.wheel_th_min = 10.0f; p_out.wheel_th_max = 720.0f;
        p_out.wheel_step_min = 1;   p_out.wheel_step_max = 30;

        p_out.flick_min = 50.0f;    p_out.flick_max = 800.0f;
        p_out.cooldown_min = 0;     p_out.cooldown_max = 5000;

        p_out.damp_min = 0.0f;      p_out.damp_max = 1.0f;

        p_out.prec_dead_min = 0.0f; p_out.prec_dead_max = 20.0f;
        p_out.prec_gain_min = 0.05f;p_out.prec_gain_max = 3.0f;
        p_out.prec_acc_min = 0.0f;  p_out.prec_acc_max = 3.0f;
        p_out.prec_step_min = 1;    p_out.prec_step_max = 60;
        p_out.prec_smooth_min = 0.0f;p_out.prec_smooth_max = 0.99f;
    }

    // ---------- Validate/Normalize ----------
    void normalizeWiFi(ST_C10_WiFiConfig_t& p_wifi) {
        if (p_wifi.mode > (uint8_t)EN_C10_WIFI_STA) p_wifi.mode = (uint8_t)EN_C10_WIFI_AUTO;

        // 최소 AP 보안: pass 8자리 미만이면 기본값으로
        if (strlen(p_wifi.ap_ssid) == 0) strlcpy(p_wifi.ap_ssid, "EliteAirMouse", sizeof(p_wifi.ap_ssid));
        if (strlen(p_wifi.ap_pass) < 8) strlcpy(p_wifi.ap_pass, "12345678", sizeof(p_wifi.ap_pass));

        // mdns: 비어있으면 기본값
        if (strlen(p_wifi.mdns_host) == 0) strlcpy(p_wifi.mdns_host, "elite-airmouse", sizeof(p_wifi.mdns_host));

        // mdns host: 허용 문자만 남기기(영문/숫자/하이픈), 선행/후행 하이픈 제거
        char v_buf[33];
        memset(v_buf, 0, sizeof(v_buf));
        size_t v_j = 0;
        for (size_t i = 0; i < strlen(p_wifi.mdns_host) && v_j < (sizeof(v_buf) - 1); i++) {
            char c = p_wifi.mdns_host[i];
            bool ok = ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (c == '-'));
            if (ok) v_buf[v_j++] = (char)tolower((int)c);
        }
        // trim hyphen
        while (v_j > 0 && v_buf[0] == '-') {
            memmove(v_buf, v_buf + 1, v_j);
            v_j--;
        }
        while (v_j > 0 && v_buf[v_j - 1] == '-') {
            v_buf[v_j - 1] = '\0';
            v_j--;
        }
        if (v_j == 0) strlcpy(v_buf, "elite-airmouse", sizeof(v_buf));
        strlcpy(p_wifi.mdns_host, v_buf, sizeof(p_wifi.mdns_host));
    }

    void normalizeE10(ST_C10_E10Config_t& p_e10) {
        ST_C10_E10Ranges_t r; getRangesE10(r);

        if (p_e10.dpi_level < r.dpi_min) p_e10.dpi_level = r.dpi_min;
        if (p_e10.dpi_level > r.dpi_max) p_e10.dpi_level = r.dpi_max;

        for (int i = 0; i < 3; i++) {
            if (p_e10.scale_base[i] < r.scale_min) p_e10.scale_base[i] = r.scale_min;
            if (p_e10.scale_base[i] > r.scale_max) p_e10.scale_base[i] = r.scale_max;
            if (p_e10.accel_gain[i] < r.accel_gain_min) p_e10.accel_gain[i] = r.accel_gain_min;
            if (p_e10.accel_gain[i] > r.accel_gain_max) p_e10.accel_gain[i] = r.accel_gain_max;
        }

        if (p_e10.accel_threshold < r.accel_th_min) p_e10.accel_threshold = r.accel_th_min;
        if (p_e10.accel_threshold > r.accel_th_max) p_e10.accel_threshold = r.accel_th_max;

        if (p_e10.wheel_threshold_deg < r.wheel_th_min) p_e10.wheel_threshold_deg = r.wheel_th_min;
        if (p_e10.wheel_threshold_deg > r.wheel_th_max) p_e10.wheel_threshold_deg = r.wheel_th_max;

        if (p_e10.wheel_step_max < r.wheel_step_min) p_e10.wheel_step_max = r.wheel_step_min;
        if (p_e10.wheel_step_max > r.wheel_step_max) p_e10.wheel_step_max = r.wheel_step_max;

        if (p_e10.gesture_flick_deg < r.flick_min) p_e10.gesture_flick_deg = r.flick_min;
        if (p_e10.gesture_flick_deg > r.flick_max) p_e10.gesture_flick_deg = r.flick_max;

        if (p_e10.gesture_cooldown_ms < r.cooldown_min) p_e10.gesture_cooldown_ms = r.cooldown_min;
        if (p_e10.gesture_cooldown_ms > r.cooldown_max) p_e10.gesture_cooldown_ms = r.cooldown_max;

        if (p_e10.scroll_cursor_damp < r.damp_min) p_e10.scroll_cursor_damp = r.damp_min;
        if (p_e10.scroll_cursor_damp > r.damp_max) p_e10.scroll_cursor_damp = r.damp_max;

        if (p_e10.precision_deadzone < r.prec_dead_min) p_e10.precision_deadzone = r.prec_dead_min;
        if (p_e10.precision_deadzone > r.prec_dead_max) p_e10.precision_deadzone = r.prec_dead_max;

        if (p_e10.precision_gain < r.prec_gain_min) p_e10.precision_gain = r.prec_gain_min;
        if (p_e10.precision_gain > r.prec_gain_max) p_e10.precision_gain = r.prec_gain_max;

        if (p_e10.precision_accel < r.prec_acc_min) p_e10.precision_accel = r.prec_acc_min;
        if (p_e10.precision_accel > r.prec_acc_max) p_e10.precision_accel = r.prec_acc_max;

        if (p_e10.precision_max_step < r.prec_step_min) p_e10.precision_max_step = r.prec_step_min;
        if (p_e10.precision_max_step > r.prec_step_max) p_e10.precision_max_step = r.prec_step_max;

        if (p_e10.precision_smooth < r.prec_smooth_min) p_e10.precision_smooth = r.prec_smooth_min;
        if (p_e10.precision_smooth > r.prec_smooth_max) p_e10.precision_smooth = r.prec_smooth_max;

        // ppt_keys2 호환/정규화
        normalizePpt2_(p_e10.ppt2_start);
        normalizePpt2_(p_e10.ppt2_exit);
        normalizePpt2_(p_e10.ppt2_next);
        normalizePpt2_(p_e10.ppt2_prev);
        normalizePpt2_(p_e10.ppt2_black);
        normalizePpt2_(p_e10.ppt2_laser);

        // ppt_keys2가 비정상일 때 ppt_keys mirror로 안전값 유지
        // (특히 page=kb인데 code=0xFFFFFFFF 같은 경우 방지)
        mirrorPpt2IfInvalid_(p_e10);
    }

    // ---------- Load/Save ----------
    bool loadAll(ST_C10_WiFiConfig_t& p_wifi, ST_C10_E10Config_t& p_e10) {
        File v_f = LittleFS.open(s_path_cfg, "r");
        if (!v_f) return false;

        JsonDocument v_doc;
        DeserializationError v_err = deserializeJson(v_doc, v_f);
        v_f.close();
        if (v_err) return false;

        makeDefaultsWiFi(p_wifi);
        makeDefaultsE10(p_e10);

        // patch
        patchFromDocWiFi(v_doc["wifi"], p_wifi);
        patchFromDocE10(v_doc["e10"], p_e10);

        // normalize
        normalizeWiFi(p_wifi);
        normalizeE10(p_e10);
        return true;
    }

    bool saveAll(const ST_C10_WiFiConfig_t& p_wifi, const ST_C10_E10Config_t& p_e10) {
        JsonDocument v_doc;
        buildJson(p_wifi, p_e10, v_doc);

        File v_tmp = LittleFS.open(s_path_tmp, "w");
        if (!v_tmp) return false;
        if (serializeJson(v_doc, v_tmp) == 0) { v_tmp.close(); (void)LittleFS.remove(s_path_tmp); return false; }
        v_tmp.flush();
        v_tmp.close();

        if (!verifyJsonFile(s_path_tmp)) { (void)LittleFS.remove(s_path_tmp); return false; }

        if (LittleFS.exists(s_path_bak)) (void)LittleFS.remove(s_path_bak);
        if (LittleFS.exists(s_path_cfg)) {
            if (!LittleFS.rename(s_path_cfg, s_path_bak)) (void)copyFile(s_path_cfg, s_path_bak);
        }

        if (!LittleFS.rename(s_path_tmp, s_path_cfg)) {
            (void)LittleFS.remove(s_path_cfg);
            if (LittleFS.exists(s_path_bak)) (void)LittleFS.rename(s_path_bak, s_path_cfg);
            (void)LittleFS.remove(s_path_tmp);
            return false;
        }
        return true;
    }

    bool rollbackFromBak() {
        if (!LittleFS.exists(s_path_bak)) return false;
        if (!verifyJsonFile(s_path_bak)) return false;
        if (LittleFS.exists(s_path_cfg)) (void)LittleFS.remove(s_path_cfg);
        return LittleFS.rename(s_path_bak, s_path_cfg);
    }

    bool exportJson(String& p_out) {
        File v_f = LittleFS.open(s_path_cfg, "r");
        if (!v_f) return false;
        p_out = v_f.readString();
        v_f.close();
        return (p_out.length() > 0);
    }

    bool importJson(const String& p_json, bool& p_saved) {
        p_saved = false;

        // validate json text
        { JsonDocument v_doc; if (deserializeJson(v_doc, p_json)) return false; }

        ST_C10_WiFiConfig_t v_w;
        ST_C10_E10Config_t  v_e;
        makeDefaultsWiFi(v_w);
        makeDefaultsE10(v_e);
        (void)loadAll(v_w, v_e); // current -> patch

        // patch by parsing again (단일 JsonDocument 규칙 준수: 이 함수 scope에서 1개만)
        JsonDocument v_doc2;
        if (deserializeJson(v_doc2, p_json)) return false;

        patchFromDocWiFi(v_doc2["wifi"], v_w);
        patchFromDocE10(v_doc2["e10"], v_e);

        normalizeWiFi(v_w);
        normalizeE10(v_e);

        p_saved = saveAll(v_w, v_e);
        return p_saved;
    }

    // ---------- Patch helpers (Doc Variant 기반: W10에서 JsonDocument 재생성 방지) ----------
    void patchFromDocWiFi(JsonVariant p_wifi, ST_C10_WiFiConfig_t& p_out) {
        if (p_wifi.isNull()) return;

        if (!p_wifi["mode"].isNull()) p_out.mode = (uint8_t)p_wifi["mode"];

        JsonVariant v_sta = p_wifi["sta"];
        if (!v_sta.isNull()) {
            if (!v_sta["ssid"].isNull()) strlcpy(p_out.sta_ssid, (const char*)v_sta["ssid"], sizeof(p_out.sta_ssid));
            if (!v_sta["pass"].isNull()) strlcpy(p_out.sta_pass, (const char*)v_sta["pass"], sizeof(p_out.sta_pass));
        }

        JsonVariant v_ap = p_wifi["ap"];
        if (!v_ap.isNull()) {
            if (!v_ap["ssid"].isNull()) strlcpy(p_out.ap_ssid, (const char*)v_ap["ssid"], sizeof(p_out.ap_ssid));
            if (!v_ap["pass"].isNull()) strlcpy(p_out.ap_pass, (const char*)v_ap["pass"], sizeof(p_out.ap_pass));
        }

        JsonVariant v_mdns = p_wifi["mdns"];
        if (!v_mdns.isNull()) {
            if (!v_mdns["host"].isNull()) strlcpy(p_out.mdns_host, (const char*)v_mdns["host"], sizeof(p_out.mdns_host));
        }
    }

    void patchFromDocE10(JsonVariant p_e10, ST_C10_E10Config_t& p_out) {
        if (p_e10.isNull()) return;

        if (!p_e10["dpi_level"].isNull()) p_out.dpi_level = (uint8_t)p_e10["dpi_level"];
        if (!p_e10["hard_click_lock"].isNull()) p_out.hard_click_lock = (bool)p_e10["hard_click_lock"];

        JsonVariant v_sb = p_e10["scale_base"];
        if (v_sb.is<JsonArray>()) {
            JsonArray a = v_sb.as<JsonArray>();
            if (a.size() >= 3) { p_out.scale_base[0]=(float)a[0]; p_out.scale_base[1]=(float)a[1]; p_out.scale_base[2]=(float)a[2]; }
        }

        JsonVariant v_ag = p_e10["accel_gain"];
        if (v_ag.is<JsonArray>()) {
            JsonArray a = v_ag.as<JsonArray>();
            if (a.size() >= 3) { p_out.accel_gain[0]=(float)a[0]; p_out.accel_gain[1]=(float)a[1]; p_out.accel_gain[2]=(float)a[2]; }
        }

        if (!p_e10["accel_threshold"].isNull()) p_out.accel_threshold = (float)p_e10["accel_threshold"];

        JsonVariant v_wh = p_e10["wheel"];
        if (!v_wh.isNull()) {
            if (!v_wh["threshold_deg"].isNull()) p_out.wheel_threshold_deg = (float)v_wh["threshold_deg"];
            if (!v_wh["step_max"].isNull()) p_out.wheel_step_max = (uint8_t)v_wh["step_max"];
        }

        JsonVariant v_g = p_e10["gesture"];
        if (!v_g.isNull()) {
            if (!v_g["flick_deg"].isNull()) p_out.gesture_flick_deg = (float)v_g["flick_deg"];
            if (!v_g["cooldown_ms"].isNull()) p_out.gesture_cooldown_ms = (uint16_t)v_g["cooldown_ms"];
        }

        if (!p_e10["scroll_cursor_damp"].isNull()) p_out.scroll_cursor_damp = (float)p_e10["scroll_cursor_damp"];

        JsonVariant v_p = p_e10["precision"];
        if (!v_p.isNull()) {
            if (!v_p["enable"].isNull()) p_out.precision_enable = (bool)v_p["enable"];
            if (!v_p["deadzone"].isNull()) p_out.precision_deadzone = (float)v_p["deadzone"];
            if (!v_p["gain"].isNull()) p_out.precision_gain = (float)v_p["gain"];
            if (!v_p["accel"].isNull()) p_out.precision_accel = (float)v_p["accel"];
            if (!v_p["max_step"].isNull()) p_out.precision_max_step = (uint8_t)v_p["max_step"];
            if (!v_p["smooth"].isNull()) p_out.precision_smooth = (float)v_p["smooth"];
        }

        // (기존) ppt_keys
        JsonVariant v_pk = p_e10["ppt_keys"];
        if (!v_pk.isNull()) {
            auto loadKey = [&](const char* n, ST_C10_PptKey_t& k){
                JsonVariant o = v_pk[n];
                if (o.isNull()) return;
                if (!o["mod"].isNull()) k.mod = (uint8_t)o["mod"];
                if (!o["key"].isNull()) k.key = (uint16_t)o["key"];
            };
            loadKey("start", p_out.ppt_start);
            loadKey("exit",  p_out.ppt_exit);
            loadKey("next",  p_out.ppt_next);
            loadKey("prev",  p_out.ppt_prev);
            loadKey("black", p_out.ppt_black);
            loadKey("laser", p_out.ppt_laser);
        }

        // (신규) ppt_keys2
        JsonVariant v_pk2 = p_e10["ppt_keys2"];
        if (!v_pk2.isNull()) {
            auto loadKey2 = [&](const char* n, ST_C10_PptKey2_t& k){
                JsonVariant o = v_pk2[n];
                if (o.isNull()) return;

                if (!o["page"].isNull()) {
                    if (o["page"].is<const char*>()) {
                        const char* s = (const char*)o["page"];
                        if (s && strcasecmp(s, "consumer") == 0) k.page = (uint8_t)EN_C10_KEYPAGE_CONSUMER;
                        else k.page = (uint8_t)EN_C10_KEYPAGE_KB;
                    } else {
                        k.page = (uint8_t)o["page"];
                    }
                }
                if (!o["mod"].isNull())  k.mod  = (uint8_t)o["mod"];
                if (!o["code"].isNull()) k.code = (uint32_t)o["code"];
            };
            loadKey2("start", p_out.ppt2_start);
            loadKey2("exit",  p_out.ppt2_exit);
            loadKey2("next",  p_out.ppt2_next);
            loadKey2("prev",  p_out.ppt2_prev);
            loadKey2("black", p_out.ppt2_black);
            loadKey2("laser", p_out.ppt2_laser);
        } else {
            // 없으면 mirror
            p_out.ppt2_start = { (uint8_t)EN_C10_KEYPAGE_KB, p_out.ppt_start.mod, (uint32_t)p_out.ppt_start.key };
            p_out.ppt2_exit  = { (uint8_t)EN_C10_KEYPAGE_KB, p_out.ppt_exit.mod,  (uint32_t)p_out.ppt_exit.key };
            p_out.ppt2_next  = { (uint8_t)EN_C10_KEYPAGE_KB, p_out.ppt_next.mod,  (uint32_t)p_out.ppt_next.key };
            p_out.ppt2_prev  = { (uint8_t)EN_C10_KEYPAGE_KB, p_out.ppt_prev.mod,  (uint32_t)p_out.ppt_prev.key };
            p_out.ppt2_black = { (uint8_t)EN_C10_KEYPAGE_KB, p_out.ppt_black.mod, (uint32_t)p_out.ppt_black.key };
            p_out.ppt2_laser = { (uint8_t)EN_C10_KEYPAGE_KB, p_out.ppt_laser.mod, (uint32_t)p_out.ppt_laser.key };
        }
    }

  private:
    void buildJson(const ST_C10_WiFiConfig_t& w, const ST_C10_E10Config_t& e, JsonDocument& doc) {
        doc["ver"] = (uint16_t)G_C10_CFG_VER;

        JsonObject jw = doc["wifi"].to<JsonObject>();
        jw["mode"] = w.mode;
        JsonObject jsta = jw["sta"].to<JsonObject>(); jsta["ssid"]=w.sta_ssid; jsta["pass"]=w.sta_pass;
        JsonObject jap  = jw["ap"].to<JsonObject>();  jap["ssid"]=w.ap_ssid;  jap["pass"]=w.ap_pass;
        JsonObject jmd  = jw["mdns"].to<JsonObject>(); jmd["host"]=w.mdns_host;

        JsonObject je = doc["e10"].to<JsonObject>();
        je["dpi_level"] = e.dpi_level;
        je["hard_click_lock"] = e.hard_click_lock;

        JsonArray sb = je["scale_base"].to<JsonArray>(); sb.add(e.scale_base[0]); sb.add(e.scale_base[1]); sb.add(e.scale_base[2]);
        JsonArray ag = je["accel_gain"].to<JsonArray>(); ag.add(e.accel_gain[0]); ag.add(e.accel_gain[1]); ag.add(e.accel_gain[2]);
        je["accel_threshold"] = e.accel_threshold;

        JsonObject wh = je["wheel"].to<JsonObject>();
        wh["threshold_deg"] = e.wheel_threshold_deg;
        wh["step_max"] = e.wheel_step_max;

        JsonObject g = je["gesture"].to<JsonObject>();
        g["flick_deg"] = e.gesture_flick_deg;
        g["cooldown_ms"] = e.gesture_cooldown_ms;

        je["scroll_cursor_damp"] = e.scroll_cursor_damp;

        JsonObject p = je["precision"].to<JsonObject>();
        p["enable"] = e.precision_enable;
        p["deadzone"] = e.precision_deadzone;
        p["gain"] = e.precision_gain;
        p["accel"] = e.precision_accel;
        p["max_step"] = e.precision_max_step;
        p["smooth"] = e.precision_smooth;

        // (기존) ppt_keys
        JsonObject pk = je["ppt_keys"].to<JsonObject>();
        auto putK = [&](const char* n, const ST_C10_PptKey_t& k){
            JsonObject o = pk[n].to<JsonObject>(); o["mod"]=k.mod; o["key"]=k.key;
        };
        putK("start", e.ppt_start); putK("exit", e.ppt_exit); putK("next", e.ppt_next);
        putK("prev", e.ppt_prev); putK("black", e.ppt_black); putK("laser", e.ppt_laser);

        // (신규) ppt_keys2
        JsonObject pk2 = je["ppt_keys2"].to<JsonObject>();
        auto putK2 = [&](const char* n, const ST_C10_PptKey2_t& k){
            JsonObject o = pk2[n].to<JsonObject>();
            o["page"] = (k.page == (uint8_t)EN_C10_KEYPAGE_CONSUMER) ? "consumer" : "kb";
            o["mod"]  = k.mod;
            o["code"] = k.code;
        };
        putK2("start", e.ppt2_start); putK2("exit", e.ppt2_exit); putK2("next", e.ppt2_next);
        putK2("prev", e.ppt2_prev); putK2("black", e.ppt2_black); putK2("laser", e.ppt2_laser);
    }

    bool verifyJsonFile(const char* p_path) {
        File f = LittleFS.open(p_path, "r");
        if (!f) return false;
        JsonDocument d;
        bool ok = !deserializeJson(d, f);
        f.close();
        return ok;
    }

    bool copyFile(const char* src, const char* dst) {
        File s = LittleFS.open(src, "r");
        if (!s) return false;
        File d = LittleFS.open(dst, "w");
        if (!d) { s.close(); return false; }
        uint8_t buf[256];
        while (true) {
            int n = s.read(buf, sizeof(buf));
            if (n <= 0) break;
            if (d.write(buf, (size_t)n) != (size_t)n) { s.close(); d.close(); return false; }
        }
        d.flush();
        s.close(); d.close();
        return true;
    }

    void normalizePpt2_(ST_C10_PptKey2_t& k) {
        if (k.page > (uint8_t)EN_C10_KEYPAGE_CONSUMER) k.page = (uint8_t)EN_C10_KEYPAGE_KB;

        if (k.page == (uint8_t)EN_C10_KEYPAGE_KB) {
            if (k.code > 0xE7) k.code = 0x00;
            // mod는 "modifier byte mask" 그대로: 0x00~0xFF 허용(여러비트 OR)
        } else {
            // consumer: mod는 무시
            k.mod = 0;
            // code는 32-bit mask: 0도 허용(None)
        }
    }

    void mirrorPpt2IfInvalid_(ST_C10_E10Config_t& e) {
        auto safeMirror = [&](ST_C10_PptKey2_t& k2, const ST_C10_PptKey_t& k1){
            bool bad = false;
            if (k2.page == (uint8_t)EN_C10_KEYPAGE_KB && k2.code == 0x00) {
                // 0x00(None)는 허용이지만, 액션키가 전부 None이면 문제일 가능성 큼.
                // 여기서는 강제하지 않고, “page/case가 이상할 때만” 미러.
            }
            if (k2.page > (uint8_t)EN_C10_KEYPAGE_CONSUMER) bad = true;
            if (k2.page == (uint8_t)EN_C10_KEYPAGE_KB && k2.code > 0xE7) bad = true;

            if (bad) {
                k2.page = (uint8_t)EN_C10_KEYPAGE_KB;
                k2.mod  = k1.mod;
                k2.code = (uint32_t)k1.key;
            }
        };

        safeMirror(e.ppt2_start, e.ppt_start);
        safeMirror(e.ppt2_exit,  e.ppt_exit);
        safeMirror(e.ppt2_next,  e.ppt_next);
        safeMirror(e.ppt2_prev,  e.ppt_prev);
        safeMirror(e.ppt2_black, e.ppt_black);
        safeMirror(e.ppt2_laser, e.ppt_laser);
    }
};

