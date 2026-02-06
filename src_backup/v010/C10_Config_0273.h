// =======================================================
// File: src/v0273/C10_Config_0273.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : C10_Config_0273.h
 * 모듈약어 : C10
 * 모듈명 : Config Manager (Atomic Save, .bak Rollback, Export/Import, Validate, SafeBoot, FactoryReset)
 * ------------------------------------------------------
 * 기능 요약
 *  - P0: Atomic 저장(tmp→rename) + .bak 백업/롤백
 *  - Export/Import + patchFromJson
 *  - validateE10() / validateWiFi()
 *  - ppt_keys2: Keyboard/Consumer(Media) 공통 키맵 지원
 *  - (v0273) Safe Boot: 부팅 실패 카운터 + 세이프모드 강제 진입
 *  - (v0273) Factory Reset: config 삭제 + 부트상태 삭제 + 재생성
 *  - (v0273) config 파일명: /json/config_0272.json
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

static constexpr uint16_t G_C10_CFG_VER = 272;

// ---- SafeBoot tuning ----
static constexpr uint8_t  G_C10_SAFEBOOT_FAIL_TH = 3;    // 3회 연속 실패 시 safe mode
static constexpr uint32_t G_C10_SAFEBOOT_OK_GRACE_MS = 8000; // 8초 생존하면 성공으로 간주

enum EN_C10_WIFI_MODE_t : uint8_t {
    EN_C10_WIFI_AUTO = 0,
    EN_C10_WIFI_AP   = 1,
    EN_C10_WIFI_STA  = 2
};

enum EN_C10_KEYPAGE_t : uint8_t {
    EN_C10_KEYPAGE_KB = 0,
    EN_C10_KEYPAGE_CONSUMER = 1
};

struct ST_C10_WiFiConfig_t {
    uint8_t mode;
    char sta_ssid[33];
    char sta_pass[65];
    char ap_ssid[33];
    char ap_pass[65];
    char mdns_host[33];
};

struct ST_C10_PptKey_t { uint8_t mod; uint16_t key; };

struct ST_C10_PptKey2_t {
    uint8_t  page;
    uint8_t  mod;
    uint32_t code;
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

    uint16_t prec_entry_ms;
    uint16_t prec_exit_ms;
    float    prec_entry_still_deg;
    float    prec_exit_move_deg;
    uint8_t  prec_profile;

    ST_C10_PptKey_t ppt_start;
    ST_C10_PptKey_t ppt_exit;
    ST_C10_PptKey_t ppt_next;
    ST_C10_PptKey_t ppt_prev;
    ST_C10_PptKey_t ppt_black;
    ST_C10_PptKey_t ppt_laser;

    ST_C10_PptKey2_t ppt2_start;
    ST_C10_PptKey2_t ppt2_exit;
    ST_C10_PptKey2_t ppt2_next;
    ST_C10_PptKey2_t ppt2_prev;
    ST_C10_PptKey2_t ppt2_black;
    ST_C10_PptKey2_t ppt2_laser;
};

// SafeBoot runtime state (W10에서 표시)
struct ST_C10_BootState_t {
    bool safe_mode;
    uint8_t fail_count;
    bool pending;          // boot_pending
    uint32_t last_boot_ms; // last boot mark time (millis snapshot 저장은 의미 약하므로, 단순 카운터용)
};

class CL_C10_Config {
  private:
    // ---- v0273: filename change ----
    static constexpr const char* s_path_cfg = "/json/config_0272.json";
    static constexpr const char* s_path_tmp = "/json/config_0272.json.tmp";
    static constexpr const char* s_path_bak = "/json/config_0272.json.bak";

    // ---- SafeBoot state file ----
    static constexpr const char* s_path_boot = "/json/boot_0272.json";

    ST_C10_BootState_t _boot;

  public:
    CL_C10_Config() { memset(&_boot,0,sizeof(_boot)); }

    void begin(bool p_formatOnFail = true) {
        (void)LittleFS.begin(p_formatOnFail);
        if (!LittleFS.exists("/json")) (void)LittleFS.mkdir("/json");

        // SafeBoot: mark boot start
        (void)bootMarkStart_();

        if (!LittleFS.exists(s_path_cfg)) {
            ST_C10_WiFiConfig_t v_w;
            ST_C10_E10Config_t  v_e;
            makeDefaultsWiFi(v_w);
            makeDefaultsE10(v_e);
            (void)saveAll(v_w, v_e);
        }
    }

    // (0273) 부팅이 일정 시간 이상 정상 유지되면 성공 처리
    bool bootMarkOkIfGracePassed(uint32_t p_uptime_ms){
        if(p_uptime_ms < G_C10_SAFEBOOT_OK_GRACE_MS) return false;
        return bootMarkOk_();
    }

    // (0273) SafeMode 여부
    bool isSafeMode() const { return _boot.safe_mode; }

    // (0273) 부트상태 조회
    void getBootState(ST_C10_BootState_t& p_out) const { p_out = _boot; }

    // (0273) SafeMode 해제(카운터 리셋) - UI에서 "Exit Safe Boot" 용
    bool clearSafeMode(){
        _boot.safe_mode=false;
        _boot.fail_count=0;
        _boot.pending=false;
        return writeBoot_(_boot);
    }

    // (0273) Factory Reset: config 삭제 + boot 삭제 + 재생성
    bool factoryReset(bool p_recreate = true){
        (void)LittleFS.remove(s_path_tmp);
        (void)LittleFS.remove(s_path_bak);
        (void)LittleFS.remove(s_path_cfg);
        (void)LittleFS.remove(s_path_boot);

        memset(&_boot,0,sizeof(_boot));
        (void)writeBoot_(_boot);

        if(p_recreate){
            ST_C10_WiFiConfig_t w; ST_C10_E10Config_t e;
            makeDefaultsWiFi(w); makeDefaultsE10(e);
            return saveAll(w,e);
        }
        return true;
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

        p_out.prec_entry_ms = 180;
        p_out.prec_exit_ms  = 160;
        p_out.prec_entry_still_deg = 2.2f;
        p_out.prec_exit_move_deg   = 7.5f;
        p_out.prec_profile = 1;

        // legacy ppt_keys defaults
        p_out.ppt_start = {0x02, 0x3E};
        p_out.ppt_exit  = {0x00, 0x29};
        p_out.ppt_next  = {0x00, 0x4E};
        p_out.ppt_prev  = {0x00, 0x4B};
        p_out.ppt_black = {0x00, 0x05};
        p_out.ppt_laser = {0x01, 0x0F};

        // ppt_keys2 mirror
        p_out.ppt2_start = { (uint8_t)EN_C10_KEYPAGE_KB, p_out.ppt_start.mod, (uint32_t)p_out.ppt_start.key };
        p_out.ppt2_exit  = { (uint8_t)EN_C10_KEYPAGE_KB, p_out.ppt_exit.mod,  (uint32_t)p_out.ppt_exit.key };
        p_out.ppt2_next  = { (uint8_t)EN_C10_KEYPAGE_KB, p_out.ppt_next.mod,  (uint32_t)p_out.ppt_next.key };
        p_out.ppt2_prev  = { (uint8_t)EN_C10_KEYPAGE_KB, p_out.ppt_prev.mod,  (uint32_t)p_out.ppt_prev.key };
        p_out.ppt2_black = { (uint8_t)EN_C10_KEYPAGE_KB, p_out.ppt_black.mod, (uint32_t)p_out.ppt_black.key };
        p_out.ppt2_laser = { (uint8_t)EN_C10_KEYPAGE_KB, p_out.ppt_laser.mod, (uint32_t)p_out.ppt_laser.key };
    }

    bool loadAll(ST_C10_WiFiConfig_t& p_wifi, ST_C10_E10Config_t& p_e10) {
        File v_f = LittleFS.open(s_path_cfg, "r");
        if (!v_f) return false;

        JsonDocument v_doc;
        DeserializationError v_err = deserializeJson(v_doc, v_f);
        v_f.close();
        if (v_err) return false;

        makeDefaultsWiFi(p_wifi);
        makeDefaultsE10(p_e10);

        String v_json;
        serializeJson(v_doc, v_json);

        (void)patchFromJsonWiFi(v_json, p_wifi);
        (void)patchFromJsonE10(v_json, p_e10);
        return true;
    }

    bool saveAll(const ST_C10_WiFiConfig_t& p_wifi, const ST_C10_E10Config_t& p_e10) {
        if (!validateWiFi(p_wifi)) return false;
        if (!validateE10(p_e10)) return false;

        JsonDocument v_doc;
        buildJson_(p_wifi, p_e10, v_doc);

        File v_tmp = LittleFS.open(s_path_tmp, "w");
        if (!v_tmp) return false;
        if (serializeJson(v_doc, v_tmp) == 0) { v_tmp.close(); (void)LittleFS.remove(s_path_tmp); return false; }
        v_tmp.flush();
        v_tmp.close();

        if (!verifyJsonFile_(s_path_tmp)) { (void)LittleFS.remove(s_path_tmp); return false; }

        if (LittleFS.exists(s_path_bak)) (void)LittleFS.remove(s_path_bak);
        if (LittleFS.exists(s_path_cfg)) {
            if (!LittleFS.rename(s_path_cfg, s_path_bak)) (void)copyFile_(s_path_cfg, s_path_bak);
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
        if (!verifyJsonFile_(s_path_bak)) return false;
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

    // 저장(import) 전용
    bool importJson(const String& p_json, bool& p_saved, bool& p_applied) {
        p_saved = false; p_applied = false;
        { JsonDocument v_doc; if (deserializeJson(v_doc, p_json)) return false; }

        ST_C10_WiFiConfig_t v_w; ST_C10_E10Config_t v_e;
        makeDefaultsWiFi(v_w); makeDefaultsE10(v_e);
        (void)loadAll(v_w, v_e);

        (void)patchFromJsonWiFi(p_json, v_w);
        (void)patchFromJsonE10(p_json, v_e);

        p_saved = saveAll(v_w, v_e);
        return p_saved;
    }

    bool validateWiFi(const ST_C10_WiFiConfig_t& w) {
        if (w.ap_pass[0] != '\0' && strlen(w.ap_pass) < 8) return false;
        if (w.sta_pass[0] != '\0' && strlen(w.sta_pass) < 8) return false;
        for (size_t i=0;i<strlen(w.mdns_host);i++){
            char c=w.mdns_host[i];
            if (c==' ' || c=='/') return false;
        }
        return true;
    }

    bool validateE10(const ST_C10_E10Config_t& e) {
        if (e.dpi_level < 1 || e.dpi_level > 3) return false;
        if (e.wheel_step_max < 1 || e.wheel_step_max > 12) return false;
        if (e.gesture_cooldown_ms < 50 || e.gesture_cooldown_ms > 3000) return false;
        if (e.precision_smooth < 0.0f || e.precision_smooth > 0.99f) return false;
        if (e.precision_max_step > 80) return false;

        auto chkPpt2=[&](const ST_C10_PptKey2_t& k)->bool{
            if (k.page == (uint8_t)EN_C10_KEYPAGE_CONSUMER) return true;
            if (k.code > 0xE7) return false;
            return true;
        };
        return chkPpt2(e.ppt2_start) && chkPpt2(e.ppt2_exit) && chkPpt2(e.ppt2_next)
            && chkPpt2(e.ppt2_prev) && chkPpt2(e.ppt2_black) && chkPpt2(e.ppt2_laser);
    }

    bool patchFromJsonWiFi(const String& p_json, ST_C10_WiFiConfig_t& p_wifi) {
        JsonDocument v_doc;
        if (deserializeJson(v_doc, p_json)) return false;

        JsonVariant v_wifi = v_doc["wifi"];
        if (v_wifi.isNull()) return true;

        if (!v_wifi["mode"].isNull()) p_wifi.mode = (uint8_t)v_wifi["mode"];

        JsonVariant v_sta = v_wifi["sta"];
        if (!v_sta.isNull()) {
            if (!v_sta["ssid"].isNull()) strlcpy(p_wifi.sta_ssid, (const char*)v_sta["ssid"], sizeof(p_wifi.sta_ssid));
            if (!v_sta["pass"].isNull()) strlcpy(p_wifi.sta_pass, (const char*)v_sta["pass"], sizeof(p_wifi.sta_pass));
        }

        JsonVariant v_ap = v_wifi["ap"];
        if (!v_ap.isNull()) {
            if (!v_ap["ssid"].isNull()) strlcpy(p_wifi.ap_ssid, (const char*)v_ap["ssid"], sizeof(p_wifi.ap_ssid));
            if (!v_ap["pass"].isNull()) strlcpy(p_wifi.ap_pass, (const char*)v_ap["pass"], sizeof(p_wifi.ap_pass));
        }

        JsonVariant v_mdns = v_wifi["mdns"];
        if (!v_mdns.isNull()) {
            if (!v_mdns["host"].isNull()) strlcpy(p_wifi.mdns_host, (const char*)v_mdns["host"], sizeof(p_wifi.mdns_host));
        }
        return true;
    }

    bool patchFromJsonE10(const String& p_json, ST_C10_E10Config_t& p_e10) {
        JsonDocument v_doc;
        if (deserializeJson(v_doc, p_json)) return false;

        JsonVariant v_e10 = v_doc["e10"];
        if (v_e10.isNull()) return true;

        if (!v_e10["dpi_level"].isNull()) p_e10.dpi_level = (uint8_t)v_e10["dpi_level"];
        if (!v_e10["hard_click_lock"].isNull()) p_e10.hard_click_lock = (bool)v_e10["hard_click_lock"];

        JsonVariant v_sb = v_e10["scale_base"];
        if (v_sb.is<JsonArray>()) {
            JsonArray a = v_sb.as<JsonArray>();
            if (a.size() >= 3) { p_e10.scale_base[0]=(float)a[0]; p_e10.scale_base[1]=(float)a[1]; p_e10.scale_base[2]=(float)a[2]; }
        }
        JsonVariant v_ag = v_e10["accel_gain"];
        if (v_ag.is<JsonArray>()) {
            JsonArray a = v_ag.as<JsonArray>();
            if (a.size() >= 3) { p_e10.accel_gain[0]=(float)a[0]; p_e10.accel_gain[1]=(float)a[1]; p_e10.accel_gain[2]=(float)a[2]; }
        }
        if (!v_e10["accel_threshold"].isNull()) p_e10.accel_threshold = (float)v_e10["accel_threshold"];

        JsonVariant v_wh = v_e10["wheel"];
        if (!v_wh.isNull()) {
            if (!v_wh["threshold_deg"].isNull()) p_e10.wheel_threshold_deg = (float)v_wh["threshold_deg"];
            if (!v_wh["step_max"].isNull()) p_e10.wheel_step_max = (uint8_t)v_wh["step_max"];
        }

        JsonVariant v_g = v_e10["gesture"];
        if (!v_g.isNull()) {
            if (!v_g["flick_deg"].isNull()) p_e10.gesture_flick_deg = (float)v_g["flick_deg"];
            if (!v_g["cooldown_ms"].isNull()) p_e10.gesture_cooldown_ms = (uint16_t)v_g["cooldown_ms"];
        }
        if (!v_e10["scroll_cursor_damp"].isNull()) p_e10.scroll_cursor_damp = (float)v_e10["scroll_cursor_damp"];

        JsonVariant v_p = v_e10["precision"];
        if (!v_p.isNull()) {
            if (!v_p["enable"].isNull()) p_e10.precision_enable = (bool)v_p["enable"];
            if (!v_p["deadzone"].isNull()) p_e10.precision_deadzone = (float)v_p["deadzone"];
            if (!v_p["gain"].isNull()) p_e10.precision_gain = (float)v_p["gain"];
            if (!v_p["accel"].isNull()) p_e10.precision_accel = (float)v_p["accel"];
            if (!v_p["max_step"].isNull()) p_e10.precision_max_step = (uint8_t)v_p["max_step"];
            if (!v_p["smooth"].isNull()) p_e10.precision_smooth = (float)v_p["smooth"];

            if (!v_p["entry_ms"].isNull()) p_e10.prec_entry_ms = (uint16_t)v_p["entry_ms"];
            if (!v_p["exit_ms"].isNull())  p_e10.prec_exit_ms  = (uint16_t)v_p["exit_ms"];
            if (!v_p["entry_still_deg"].isNull()) p_e10.prec_entry_still_deg = (float)v_p["entry_still_deg"];
            if (!v_p["exit_move_deg"].isNull())   p_e10.prec_exit_move_deg   = (float)v_p["exit_move_deg"];
            if (!v_p["profile"].isNull())         p_e10.prec_profile = (uint8_t)v_p["profile"];
        }

        JsonVariant v_pk2 = v_e10["ppt_keys2"];
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

            loadKey2("start", p_e10.ppt2_start);
            loadKey2("exit",  p_e10.ppt2_exit);
            loadKey2("next",  p_e10.ppt2_next);
            loadKey2("prev",  p_e10.ppt2_prev);
            loadKey2("black", p_e10.ppt2_black);
            loadKey2("laser", p_e10.ppt2_laser);
        } else {
            // ppt_keys2 없으면 legacy mirror (최소)
            p_e10.ppt2_start = { (uint8_t)EN_C10_KEYPAGE_KB, p_e10.ppt_start.mod, (uint32_t)p_e10.ppt_start.key };
            p_e10.ppt2_exit  = { (uint8_t)EN_C10_KEYPAGE_KB, p_e10.ppt_exit.mod,  (uint32_t)p_e10.ppt_exit.key };
            p_e10.ppt2_next  = { (uint8_t)EN_C10_KEYPAGE_KB, p_e10.ppt_next.mod,  (uint32_t)p_e10.ppt_next.key };
            p_e10.ppt2_prev  = { (uint8_t)EN_C10_KEYPAGE_KB, p_e10.ppt_prev.mod,  (uint32_t)p_e10.ppt_prev.key };
            p_e10.ppt2_black = { (uint8_t)EN_C10_KEYPAGE_KB, p_e10.ppt_black.mod, (uint32_t)p_e10.ppt_black.key };
            p_e10.ppt2_laser = { (uint8_t)EN_C10_KEYPAGE_KB, p_e10.ppt_laser.mod, (uint32_t)p_e10.ppt_laser.key };
        }

        return true;
    }

  private:
    // ---------- SafeBoot internals ----------
    bool bootMarkStart_(){
        // read boot state
        readBoot_(_boot);

        // if previous boot was pending, count as failure
        if(_boot.pending){
            if(_boot.fail_count < 250) _boot.fail_count++;
        }

        // decide safe mode
        if(_boot.fail_count >= G_C10_SAFEBOOT_FAIL_TH){
            _boot.safe_mode=true;
        }

        // set pending=true for current boot
        _boot.pending=true;
        _boot.last_boot_ms = (uint32_t)millis();
        return writeBoot_(_boot);
    }

    bool bootMarkOk_(){
        // 성공: pending 해제 + fail_count 감소/리셋
        readBoot_(_boot);
        _boot.pending=false;
        _boot.safe_mode=false;
        _boot.fail_count=0;
        return writeBoot_(_boot);
    }

    bool readBoot_(ST_C10_BootState_t& st){
        memset(&st,0,sizeof(st));
        if(!LittleFS.exists(s_path_boot)) return writeBoot_(st);

        File f=LittleFS.open(s_path_boot,"r");
        if(!f) return false;
        JsonDocument d;
        if(deserializeJson(d,f)){ f.close(); return false; }
        f.close();

        if(!d["safe_mode"].isNull()) st.safe_mode=(bool)d["safe_mode"];
        if(!d["fail_count"].isNull()) st.fail_count=(uint8_t)d["fail_count"];
        if(!d["pending"].isNull()) st.pending=(bool)d["pending"];
        if(!d["last_boot_ms"].isNull()) st.last_boot_ms=(uint32_t)d["last_boot_ms"];
        return true;
    }

    bool writeBoot_(const ST_C10_BootState_t& st){
        JsonDocument d;
        d["safe_mode"]=st.safe_mode;
        d["fail_count"]=st.fail_count;
        d["pending"]=st.pending;
        d["last_boot_ms"]=st.last_boot_ms;

        File f=LittleFS.open(s_path_boot,"w");
        if(!f) return false;
        bool ok = (serializeJson(d,f)>0);
        f.flush(); f.close();
        return ok;
    }

    // ---------- JSON build ----------
    void buildJson_(const ST_C10_WiFiConfig_t& w, const ST_C10_E10Config_t& e, JsonDocument& doc) {
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
        p["entry_ms"] = e.prec_entry_ms;
        p["exit_ms"]  = e.prec_exit_ms;
        p["entry_still_deg"] = e.prec_entry_still_deg;
        p["exit_move_deg"]   = e.prec_exit_move_deg;
        p["profile"] = e.prec_profile;

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

    bool verifyJsonFile_(const char* p_path) {
        File f = LittleFS.open(p_path, "r");
        if (!f) return false;
        JsonDocument d;
        bool ok = !deserializeJson(d, f);
        f.close();
        return ok;
    }

    bool copyFile_(const char* src, const char* dst) {
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
};

