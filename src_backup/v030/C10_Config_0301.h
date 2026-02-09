// =======================================================
// File: src/v010/C10_Config_0301.h
// =======================================================
#pragma once
/*
 * ------------------------------------------------------
 * 소스명 : C10_Config_0301.h
 * 모듈약어 : C10
 * 모듈명 : Config Manager (Atomic Save, .bak Rollback, Export/Import, PPT Keymap v2, SafeBoot/FactoryReset)
 * ------------------------------------------------------
 * 기능 요약
 *  - P0: Atomic 저장(tmp→rename) + .bak 백업/롤백
 *  - Export/Import + patchFromJson
 *  - ppt_keys2: Keyboard/Consumer(Media) 공통 키맵 지원
 *  - validateWiFi/validateE10 제공(서버 입력 검증)
 *  - SafeBoot(벽돌 방지): boot state pending/fail_count/safe_mode
 *  - Factory Reset: config/boot 초기화 + 기본값 재생성
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
#include <strings.h>

#include "C10_Def_0301.h"

class CL_C10_Config {
  private:
    // (요구) config_0272.json
    static constexpr const char* s_path_cfg = "/json/config_0272.json";
    static constexpr const char* s_path_tmp = "/json/config_0272.json.tmp";
    static constexpr const char* s_path_bak = "/json/config_0272.json.bak";

    // SafeBoot state
    static constexpr const char*   s_path_boot           = "/json/boot_state_0272.json";
    static constexpr const char*   s_path_boot_tmp       = "/json/boot_state_0272.json.tmp";
    static constexpr const uint8_t s_safe_fail_threshold = 2; // 2회 연속 실패 시 safe_mode

    ST_C10_BootState_t _boot;

  public:
    CL_C10_Config(){
        memset(&_boot, 0, sizeof(_boot));
    }

    void begin(bool p_formatOnFail = true){
        (void)LittleFS.begin(p_formatOnFail);
        if(!LittleFS.exists("/json")) (void)LittleFS.mkdir("/json");

        // 1) boot state load + mark start (pending)
        loadBootState_(_boot);
        // 이전 부팅이 pending 상태였으면 실패 카운트 증가
        if(_boot.pending){
            if(_boot.fail_count < 250) _boot.fail_count++;
        }
        // fail threshold 도달 시 safe mode
        if(_boot.fail_count >= s_safe_fail_threshold){
            _boot.safe_mode = true;
        }
        // 이번 부팅 pending true로 마킹
        _boot.pending = true;
        (void)saveBootState_(_boot);

        // 2) config 존재 보장
        if(!LittleFS.exists(s_path_cfg)){
            ST_C10_WiFiConfig_t v_w;
            ST_C10_E10Config_t  v_e;
            makeDefaultsWiFi(v_w);
            makeDefaultsE10(v_e);
            (void)saveAll(v_w, v_e);
        }
    }

    // ---------- Defaults ----------
    void makeDefaultsWiFi(ST_C10_WiFiConfig_t& p_out){
        memset(&p_out, 0, sizeof(p_out));
        p_out.mode = (uint8_t)EN_C10_WIFI_AUTO;
        strlcpy(p_out.ap_ssid, "EliteAirMouse", sizeof(p_out.ap_ssid));
        strlcpy(p_out.ap_pass, "12345678", sizeof(p_out.ap_pass));
        strlcpy(p_out.mdns_host, "elite-airmouse", sizeof(p_out.mdns_host));
    }

    void makeDefaultsE10(ST_C10_E10Config_t& p_out){
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

        // (0272~) precision entry/exit defaults
        p_out.prec_entry_ms = 450;          // 추천: 350~700
        p_out.prec_exit_ms  = 300;          // 추천: 200~600
        p_out.prec_entry_still_deg = 1.2f;  // 추천: 0.8~2.0
        p_out.prec_exit_move_deg   = 3.5f;  // 추천: 2.0~6.0
        p_out.prec_profile = 0;

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

    // ---------- Validation ----------
    bool validateWiFi(const ST_C10_WiFiConfig_t& p_w){
        if(p_w.mode > (uint8_t)EN_C10_WIFI_STA) return false;

        // string is already bounded by struct, but check pass policy
        // AP pass: empty(open) OR >=8
        size_t apLen = strlen(p_w.ap_pass);
        if(apLen > 0 && apLen < 8) return false;

        // mdns host: empty ok, else length <=32
        if(strlen(p_w.mdns_host) > 32) return false;

        // STA pass length <=64 already, no extra
        return true;
    }

    bool validateE10(const ST_C10_E10Config_t& p_e){
        if(p_e.dpi_level > 2) return false;

        if(p_e.accel_threshold < 0.0f || p_e.accel_threshold > 50.0f) return false;
        if(p_e.wheel_threshold_deg < 1.0f || p_e.wheel_threshold_deg > 360.0f) return false;
        if(p_e.wheel_step_max < 1 || p_e.wheel_step_max > 50) return false;

        if(p_e.gesture_flick_deg < 10.0f || p_e.gesture_flick_deg > 2000.0f) return false;
        if(p_e.gesture_cooldown_ms > 20000) return false;

        if(p_e.scroll_cursor_damp < 0.0f || p_e.scroll_cursor_damp > 1.0f) return false;

        if (p_e.precision_deadzone < 0.0f || p_e.precision_deadzone > 50.0f) return false;
        if (p_e.precision_gain < 0.0f || p_e.precision_gain > 5.0f) return false;
        if (p_e.precision_accel < 0.0f || p_e.precision_accel > 5.0f) return false;
        if (p_e.precision_max_step < 1 || p_e.precision_max_step > 200) return false;
        if (p_e.precision_smooth < 0.0f || p_e.precision_smooth > 1.0f) return false;

        if (p_e.prec_entry_ms < 50 || p_e.prec_entry_ms > 5000) return false;
        if (p_e.prec_exit_ms < 50 || p_e.prec_exit_ms > 5000) return false;
        if (p_e.prec_entry_still_deg < 0.1f || p_e.prec_entry_still_deg > 20.0f) return false;
        if (p_e.prec_exit_move_deg < 0.1f || p_e.prec_exit_move_deg > 50.0f) return false;
        if (p_e.prec_profile > 5) return false;

        // ppt2 constraints
        const ST_C10_PptKey2_t* keys[] = { &p_e.ppt2_start,&p_e.ppt2_exit,&p_e.ppt2_next,&p_e.ppt2_prev,&p_e.ppt2_black,&p_e.ppt2_laser };
        for(size_t i=0;i<sizeof(keys)/sizeof(keys[0]);i++){
            const ST_C10_PptKey2_t& k = *keys[i];
            if(k.page > (uint8_t)EN_C10_KEYPAGE_CONSUMER) return false;
            if(k.page == (uint8_t)EN_C10_KEYPAGE_KB){
                if(k.code > 0xE7) return false;
            }else{
                // consumer: mod must be 0 (UI에서 0 강제)
                if(k.mod != 0) return false;
            }
        }
        return true;
    }

    // ---------- SafeBoot APIs ----------
    bool isSafeMode() const { return _boot.safe_mode; }

    void getBootState(ST_C10_BootState_t& p_out){
        p_out = _boot;
    }

    bool clearSafeMode(){
        _boot.safe_mode = false;
        _boot.fail_count = 0;
        _boot.pending = false;
        return saveBootState_(_boot);
    }

    // setup에서 grace 통과 후 호출: pending 해제
    bool bootMarkOkIfGracePassed(uint32_t p_graceMs){
        (void)p_graceMs;

        // 이번 부팅 성공 표시
        _boot.pending = false;

        // safe_mode가 false인 경우에만 fail_count 리셋
        if(!_boot.safe_mode){
            _boot.fail_count = 0;
        }
        // safe_mode는 유지(사용자 해제까지)
        return saveBootState_(_boot);
    }

    /*
    // setup에서 grace 통과 후 호출: pending 해제 + fail_count reset
    bool bootMarkOkIfGracePassed(uint32_t p_graceMs){
        (void)p_graceMs; // 호출자에서 uptime 확인 후 호출
        _boot.pending = false;
        _boot.fail_count = 0;
        // safe_mode는 유지(사용자가 해제할 때까지) 정책
        return saveBootState_(_boot);
    }
    */

    // ---------- Config Load/Save ----------
    bool loadAll(ST_C10_WiFiConfig_t& p_wifi, ST_C10_E10Config_t& p_e10){
        File v_f = LittleFS.open(s_path_cfg, "r");
        if(!v_f) return false;

        JsonDocument v_doc;
        DeserializationError v_err = deserializeJson(v_doc, v_f);
        v_f.close();
        if(v_err) return false;

        makeDefaultsWiFi(p_wifi);
        makeDefaultsE10(p_e10);

        String v_json;
        serializeJson(v_doc, v_json);

        (void)patchFromJsonWiFi(v_json, p_wifi);
        (void)patchFromJsonE10(v_json, p_e10);

        return true;
    }

    bool saveAll(const ST_C10_WiFiConfig_t& p_wifi, const ST_C10_E10Config_t& p_e10){
        if(!validateWiFi(p_wifi)) return false;
        if(!validateE10(p_e10)) return false;

        JsonDocument v_doc;
        buildJson_(p_wifi, p_e10, v_doc);

        File v_tmp = LittleFS.open(s_path_tmp, "w");
        if(!v_tmp) return false;
        if(serializeJson(v_doc, v_tmp) == 0){
            v_tmp.close();
            (void)LittleFS.remove(s_path_tmp);
            return false;
        }
        v_tmp.flush();
        v_tmp.close();

        if(!verifyJsonFile_(s_path_tmp)){
            (void)LittleFS.remove(s_path_tmp);
            return false;
        }

        if(LittleFS.exists(s_path_bak)) (void)LittleFS.remove(s_path_bak);
        if(LittleFS.exists(s_path_cfg)){
            if(!LittleFS.rename(s_path_cfg, s_path_bak)){
                (void)copyFile_(s_path_cfg, s_path_bak);
            }
        }

        if(!LittleFS.rename(s_path_tmp, s_path_cfg)){
            (void)LittleFS.remove(s_path_cfg);
            if(LittleFS.exists(s_path_bak)) (void)LittleFS.rename(s_path_bak, s_path_cfg);
            (void)LittleFS.remove(s_path_tmp);
            return false;
        }
        return true;
    }

    bool rollbackFromBak(){
        if(!LittleFS.exists(s_path_bak)) return false;
        if(!verifyJsonFile_(s_path_bak)) return false;
        if(LittleFS.exists(s_path_cfg)) (void)LittleFS.remove(s_path_cfg);
        return LittleFS.rename(s_path_bak, s_path_cfg);
    }

    bool exportJson(String& p_out){
        File v_f = LittleFS.open(s_path_cfg, "r");
        if(!v_f) return false;
        p_out = v_f.readString();
        v_f.close();
        return (p_out.length() > 0);
    }

    // import body:
    // - full config.json OR {wifi:{...}, e10:{...}} partial patch 모두 허용
    bool importJson(const String& p_json, bool& p_saved, bool& p_applied){
        p_saved=false; p_applied=false;
        { JsonDocument v_doc; if(deserializeJson(v_doc, p_json)) return false; }

        ST_C10_WiFiConfig_t v_w; ST_C10_E10Config_t v_e;
        makeDefaultsWiFi(v_w); makeDefaultsE10(v_e);
        (void)loadAll(v_w, v_e);

        bool ok=true;
        ok = ok && patchFromJsonWiFi(p_json, v_w);
        ok = ok && patchFromJsonE10(p_json, v_e);

        ok = ok && validateWiFi(v_w);
        ok = ok && validateE10(v_e);

        if(!ok) return false;

        p_saved = saveAll(v_w, v_e);
        return p_saved;
    }

    // ---------- Patchers ----------
    bool patchFromJsonWiFi(const String& p_json, ST_C10_WiFiConfig_t& p_wifi){
        JsonDocument v_doc;
        if(deserializeJson(v_doc, p_json)) return false;

        JsonVariant v_wifi = v_doc["wifi"];
        if(v_wifi.isNull()) return true;

        if(!v_wifi["mode"].isNull()) p_wifi.mode = (uint8_t)v_wifi["mode"];

        JsonVariant v_sta = v_wifi["sta"];
        if(!v_sta.isNull()){
            if(!v_sta["ssid"].isNull()) strlcpy(p_wifi.sta_ssid, (const char*)v_sta["ssid"], sizeof(p_wifi.sta_ssid));
            if(!v_sta["pass"].isNull()) strlcpy(p_wifi.sta_pass, (const char*)v_sta["pass"], sizeof(p_wifi.sta_pass));
        }

        JsonVariant v_ap = v_wifi["ap"];
        if(!v_ap.isNull()){
            if(!v_ap["ssid"].isNull()) strlcpy(p_wifi.ap_ssid, (const char*)v_ap["ssid"], sizeof(p_wifi.ap_ssid));
            if(!v_ap["pass"].isNull()) strlcpy(p_wifi.ap_pass, (const char*)v_ap["pass"], sizeof(p_wifi.ap_pass));
        }

        JsonVariant v_mdns = v_wifi["mdns"];
        if(!v_mdns.isNull()){
            if(!v_mdns["host"].isNull()) strlcpy(p_wifi.mdns_host, (const char*)v_mdns["host"], sizeof(p_wifi.mdns_host));
        }
        return true;
    }

    bool patchFromJsonE10(const String& p_json, ST_C10_E10Config_t& p_e10){
        JsonDocument v_doc;
        if(deserializeJson(v_doc, p_json)) return false;

        JsonVariant v_e10 = v_doc["e10"];
        if(v_e10.isNull()) return true;

        if(!v_e10["dpi_level"].isNull()) p_e10.dpi_level = (uint8_t)v_e10["dpi_level"];
        if(!v_e10["hard_click_lock"].isNull()) p_e10.hard_click_lock = (bool)v_e10["hard_click_lock"];

        JsonVariant v_sb = v_e10["scale_base"];
        if(v_sb.is<JsonArray>()){
            JsonArray a = v_sb.as<JsonArray>();
            if(a.size() >= 3){
                p_e10.scale_base[0]=(float)a[0];
                p_e10.scale_base[1]=(float)a[1];
                p_e10.scale_base[2]=(float)a[2];
            }
        }

        JsonVariant v_ag = v_e10["accel_gain"];
        if(v_ag.is<JsonArray>()){
            JsonArray a = v_ag.as<JsonArray>();
            if(a.size() >= 3){
                p_e10.accel_gain[0]=(float)a[0];
                p_e10.accel_gain[1]=(float)a[1];
                p_e10.accel_gain[2]=(float)a[2];
            }
        }

        if(!v_e10["accel_threshold"].isNull()) p_e10.accel_threshold = (float)v_e10["accel_threshold"];

        JsonVariant v_wh = v_e10["wheel"];
        if(!v_wh.isNull()){
            if(!v_wh["threshold_deg"].isNull()) p_e10.wheel_threshold_deg = (float)v_wh["threshold_deg"];
            if(!v_wh["step_max"].isNull()) p_e10.wheel_step_max = (uint8_t)v_wh["step_max"];
        }

        JsonVariant v_g = v_e10["gesture"];
        if(!v_g.isNull()){
            if(!v_g["flick_deg"].isNull()) p_e10.gesture_flick_deg = (float)v_g["flick_deg"];
            if(!v_g["cooldown_ms"].isNull()) p_e10.gesture_cooldown_ms = (uint16_t)v_g["cooldown_ms"];
        }

        if(!v_e10["scroll_cursor_damp"].isNull()) p_e10.scroll_cursor_damp = (float)v_e10["scroll_cursor_damp"];

        JsonVariant v_p = v_e10["precision"];
        if(!v_p.isNull()){
            if(!v_p["enable"].isNull()) p_e10.precision_enable = (bool)v_p["enable"];
            if(!v_p["deadzone"].isNull()) p_e10.precision_deadzone = (float)v_p["deadzone"];
            if(!v_p["gain"].isNull()) p_e10.precision_gain = (float)v_p["gain"];
            if(!v_p["accel"].isNull()) p_e10.precision_accel = (float)v_p["accel"];
            if(!v_p["max_step"].isNull()) p_e10.precision_max_step = (uint8_t)v_p["max_step"];
            if(!v_p["smooth"].isNull()) p_e10.precision_smooth = (float)v_p["smooth"];

			if (!v_p["entry_ms"].isNull())          p_e10.prec_entry_ms = (uint16_t)v_p["entry_ms"];
			if (!v_p["exit_ms"].isNull())           p_e10.prec_exit_ms  = (uint16_t)v_p["exit_ms"];
			if (!v_p["entry_still_deg"].isNull())   p_e10.prec_entry_still_deg = (float)v_p["entry_still_deg"];
			if (!v_p["exit_move_deg"].isNull())     p_e10.prec_exit_move_deg   = (float)v_p["exit_move_deg"];
			if (!v_p["profile"].isNull())           p_e10.prec_profile = (uint8_t)v_p["profile"];
        }

        // (기존) ppt_keys
        JsonVariant v_pk = v_e10["ppt_keys"];
        if(!v_pk.isNull()){
            auto loadKey = [&](const char* n, ST_C10_PptKey_t& k){
                JsonVariant o = v_pk[n];
                if(o.isNull()) return;
                if(!o["mod"].isNull()) k.mod = (uint8_t)o["mod"];
                if(!o["key"].isNull()) k.key = (uint16_t)o["key"];
            };
            loadKey("start", p_e10.ppt_start);
            loadKey("exit",  p_e10.ppt_exit);
            loadKey("next",  p_e10.ppt_next);
            loadKey("prev",  p_e10.ppt_prev);
            loadKey("black", p_e10.ppt_black);
            loadKey("laser", p_e10.ppt_laser);
        }

        // (신규) ppt_keys2
        JsonVariant v_pk2 = v_e10["ppt_keys2"];
        if(!v_pk2.isNull()){
            auto loadKey2 = [&](const char* n, ST_C10_PptKey2_t& k){
                JsonVariant o = v_pk2[n];
                if(o.isNull()) return;

                if(!o["page"].isNull()){
                    if(o["page"].is<const char*>()){
                        const char* s = (const char*)o["page"];
                        if(s && strcasecmp(s, "consumer")==0) k.page = (uint8_t)EN_C10_KEYPAGE_CONSUMER;
                        else k.page = (uint8_t)EN_C10_KEYPAGE_KB;
                    }else{
                        k.page = (uint8_t)o["page"];
                    }
                }
                if(!o["mod"].isNull())  k.mod  = (uint8_t)o["mod"];
                if(!o["code"].isNull()) k.code = (uint32_t)o["code"];
            };

            loadKey2("start", p_e10.ppt2_start);
            loadKey2("exit",  p_e10.ppt2_exit);
            loadKey2("next",  p_e10.ppt2_next);
            loadKey2("prev",  p_e10.ppt2_prev);
            loadKey2("black", p_e10.ppt2_black);
            loadKey2("laser", p_e10.ppt2_laser);
        }else{
            // ppt_keys2 없으면 ppt_keys mirror
            p_e10.ppt2_start = { (uint8_t)EN_C10_KEYPAGE_KB, p_e10.ppt_start.mod, (uint32_t)p_e10.ppt_start.key };
            p_e10.ppt2_exit  = { (uint8_t)EN_C10_KEYPAGE_KB, p_e10.ppt_exit.mod,  (uint32_t)p_e10.ppt_exit.key };
            p_e10.ppt2_next  = { (uint8_t)EN_C10_KEYPAGE_KB, p_e10.ppt_next.mod,  (uint32_t)p_e10.ppt_next.key };
            p_e10.ppt2_prev  = { (uint8_t)EN_C10_KEYPAGE_KB, p_e10.ppt_prev.mod,  (uint32_t)p_e10.ppt_prev.key };
            p_e10.ppt2_black = { (uint8_t)EN_C10_KEYPAGE_KB, p_e10.ppt_black.mod, (uint32_t)p_e10.ppt_black.key };
            p_e10.ppt2_laser = { (uint8_t)EN_C10_KEYPAGE_KB, p_e10.ppt_laser.mod, (uint32_t)p_e10.ppt_laser.key };
        }

        return true;
    }

    // ---------- Factory Reset ----------
    bool factoryReset(bool p_recreateDefault){
        // config files
        if(LittleFS.exists(s_path_tmp)) (void)LittleFS.remove(s_path_tmp);
        if(LittleFS.exists(s_path_bak)) (void)LittleFS.remove(s_path_bak);
        if(LittleFS.exists(s_path_cfg)) (void)LittleFS.remove(s_path_cfg);

        // boot state
        if(LittleFS.exists(s_path_boot_tmp)) (void)LittleFS.remove(s_path_boot_tmp);
        if(LittleFS.exists(s_path_boot)) (void)LittleFS.remove(s_path_boot);

        // reset boot state in memory
        memset(&_boot, 0, sizeof(_boot));
        // factory reset 직후엔 pending을 false로 저장(다음 begin에서 다시 true로 마킹됨)
        _boot.pending = false;
        //  _boot.pending = true; // 이번 부팅을 pending으로 다시 찍어두는 편이 안전
        (void)saveBootState_(_boot);

        if(p_recreateDefault){
            ST_C10_WiFiConfig_t v_w;
            ST_C10_E10Config_t  v_e;
            makeDefaultsWiFi(v_w);
            makeDefaultsE10(v_e);
            return saveAll(v_w, v_e);
        }
        return true;
    }

  private:
    // ---------- JSON build ----------
    void buildJson_(const ST_C10_WiFiConfig_t& w, const ST_C10_E10Config_t& e, JsonDocument& doc){
        doc["ver"] = (uint16_t)G_C10_CFG_VER;

        JsonObject jw = doc["wifi"].to<JsonObject>();
        jw["mode"] = w.mode;

        JsonObject jsta = jw["sta"].to<JsonObject>();
        jsta["ssid"] = w.sta_ssid;
        jsta["pass"] = w.sta_pass;

        JsonObject jap = jw["ap"].to<JsonObject>();
        jap["ssid"] = w.ap_ssid;
        jap["pass"] = w.ap_pass;

        JsonObject jmd = jw["mdns"].to<JsonObject>();
        jmd["host"] = w.mdns_host;

        JsonObject je = doc["e10"].to<JsonObject>();
        je["dpi_level"] = e.dpi_level;
        je["hard_click_lock"] = e.hard_click_lock;

        JsonArray sb = je["scale_base"].to<JsonArray>();
        sb.add(e.scale_base[0]); sb.add(e.scale_base[1]); sb.add(e.scale_base[2]);

        JsonArray ag = je["accel_gain"].to<JsonArray>();
        ag.add(e.accel_gain[0]); ag.add(e.accel_gain[1]); ag.add(e.accel_gain[2]);

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

        // (기존) ppt_keys
        JsonObject pk = je["ppt_keys"].to<JsonObject>();
        auto putK = [&](const char* n, const ST_C10_PptKey_t& k){
            JsonObject o = pk[n].to<JsonObject>();
            o["mod"] = k.mod;
            o["key"] = k.key;
        };
        putK("start", e.ppt_start);
        putK("exit",  e.ppt_exit);
        putK("next",  e.ppt_next);
        putK("prev",  e.ppt_prev);
        putK("black", e.ppt_black);
        putK("laser", e.ppt_laser);

        // (신규) ppt_keys2
        JsonObject pk2 = je["ppt_keys2"].to<JsonObject>();
        auto putK2 = [&](const char* n, const ST_C10_PptKey2_t& k){
            JsonObject o = pk2[n].to<JsonObject>();
            o["page"] = (k.page == (uint8_t)EN_C10_KEYPAGE_CONSUMER) ? "consumer" : "kb";
            o["mod"]  = k.mod;
            o["code"] = k.code;
        };
        putK2("start", e.ppt2_start);
        putK2("exit",  e.ppt2_exit);
        putK2("next",  e.ppt2_next);
        putK2("prev",  e.ppt2_prev);
        putK2("black", e.ppt2_black);
        putK2("laser", e.ppt2_laser);
    }

    // ---------- Boot state ----------
    bool loadBootState_(ST_C10_BootState_t& p_out){
        memset(&p_out, 0, sizeof(p_out));

        if(!LittleFS.exists(s_path_boot)){
            // default: not safe, fail_count=0, pending=false
            return saveBootState_(p_out);
        }

        File f = LittleFS.open(s_path_boot, "r");
        if(!f) return false;

        JsonDocument d;
        DeserializationError err = deserializeJson(d, f);
        f.close();
        if(err) return false;

        if(!d["safe_mode"].isNull()) p_out.safe_mode = (bool)d["safe_mode"];
        if(!d["fail_count"].isNull()) p_out.fail_count = (uint8_t)d["fail_count"];
        if(!d["pending"].isNull()) p_out.pending = (bool)d["pending"];
        return true;
    }

    bool saveBootState_(const ST_C10_BootState_t& p_in){
        JsonDocument d;
        d["safe_mode"] = p_in.safe_mode;
        d["fail_count"] = p_in.fail_count;
        d["pending"] = p_in.pending;

        File tmp = LittleFS.open(s_path_boot_tmp, "w");
        if(!tmp) return false;
        if(serializeJson(d, tmp) == 0){
            tmp.close();
            (void)LittleFS.remove(s_path_boot_tmp);
            return false;
        }
        tmp.flush();
        tmp.close();

        if(!verifyJsonFile_(s_path_boot_tmp)){
            (void)LittleFS.remove(s_path_boot_tmp);
            return false;
        }

        if(LittleFS.exists(s_path_boot)) (void)LittleFS.remove(s_path_boot);
        return LittleFS.rename(s_path_boot_tmp, s_path_boot);
    }

    // ---------- file utils ----------
    bool verifyJsonFile_(const char* p_path){
        File f = LittleFS.open(p_path, "r");
        if(!f) return false;
        JsonDocument d;
        bool ok = !deserializeJson(d, f);
        f.close();
        return ok;
    }

    bool copyFile_(const char* src, const char* dst){
        File s = LittleFS.open(src, "r");
        if(!s) return false;
        File d = LittleFS.open(dst, "w");
        if(!d){ s.close(); return false; }

        uint8_t buf[256];
        while(true){
            int n = s.read(buf, sizeof(buf));
            if(n <= 0) break;
            if(d.write(buf, (size_t)n) != (size_t)n){
                s.close(); d.close();
                return false;
            }
        }
        d.flush();
        s.close(); d.close();
        return true;
    }
};

