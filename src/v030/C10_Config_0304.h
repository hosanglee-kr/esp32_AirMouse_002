// =======================================================
// File: src/v010/C10_Config_0304.h
// =======================================================
#pragma once
/*
 * (주석 블록은 기존 규칙대로 유지 전제)
 */

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <string.h>
#include <strings.h>
#include <esp_system.h> // esp_reset_reason()

#include "C10_Def_0303.h"

class CL_C10_Config {
  private:
    ST_C10_BootState_t _boot;

  public:
    CL_C10_Config() { 
        memset(&_boot, 0, sizeof(_boot)); 
    }

    void begin(bool p_formatOnFail = true) {
        (void)LittleFS.begin(p_formatOnFail);
        if (!LittleFS.exists("/json")) (void)LittleFS.mkdir("/json");

        // 1) boot state load + mark start (pending)
        _loadBootState(_boot);

        // A-3: reset reason 기록
        _boot.last_reset_reason = _getResetReasonU8();
        
        // 이전 부팅이 pending이었다면 “비정상 리셋”일 때만 실패 카운트 증가
        if (_boot.pending) {
            esp_reset_reason_t v_r = (esp_reset_reason_t)_boot.last_reset_reason;
            if (_isBadResetReason(v_r)) {
                if (_boot.fail_count < 250) _boot.fail_count++;
            }
        }


        // fail threshold 도달 시 safe mode
        if (_boot.fail_count >= C10_DEF::SAFE_FAIL_THRESHOLD) {
            _boot.safe_mode = true;
        }

        // 이번 부팅 pending true로 마킹
        _boot.pending = true;
        // 이번 부팅 pending true + boot_ms 기록
        _boot.boot_ms = (uint32_t)millis();
    
        (void)_saveBootState(_boot);

        // 2) config 존재 보장
        if (!LittleFS.exists(C10_DEF::CFG_PATH)) {
            ST_C10_WiFiConfig_t v_w;
            ST_C10_E10Config_t  v_e;
            makeDefaultsWiFi(v_w);
            makeDefaultsE10(v_e);
            (void)saveAll(v_w, v_e);
        }
    }

    // ---------- Defaults ----------
    void makeDefaultsWiFi(ST_C10_WiFiConfig_t& p_out) {
        memset(&p_out, 0, sizeof(p_out));
        p_out.mode = (uint8_t)EN_C10_WIFI_AUTO;
        strlcpy(p_out.ap_ssid, "EliteAirMouse", sizeof(p_out.ap_ssid));
        strlcpy(p_out.ap_pass, "12345678", sizeof(p_out.ap_pass));
        strlcpy(p_out.mdns_host, "elite-airmouse", sizeof(p_out.mdns_host));
    }

    void makeDefaultsE10(ST_C10_E10Config_t& p_out) {
        memset(&p_out, 0, sizeof(p_out));

        p_out.dpi_level       = 2;
        p_out.hard_click_lock = true;

        p_out.scale_base[0] = 0.55f; p_out.scale_base[1] = 0.75f; p_out.scale_base[2] = 1.00f;
        p_out.accel_gain[0] = 0.35f; p_out.accel_gain[1] = 0.55f; p_out.accel_gain[2] = 0.85f;
        p_out.accel_threshold = 8.0f;

        p_out.wheel_threshold_deg = 90.0f;
        p_out.wheel_step_max      = 6;

        p_out.gesture_flick_deg   = 200.0f;
        p_out.gesture_cooldown_ms = 600;

        p_out.scroll_cursor_damp = 0.25f;

        p_out.precision_mode = EN_C10_E10_PREC_OFF;
        // p_out.precision_enable   = false;
        
        p_out.precision_deadzone = 1.2f;
        p_out.precision_gain     = 0.65f;
        p_out.precision_accel    = 0.25f;
        p_out.precision_max_step = 18;
        p_out.precision_smooth   = 0.85f;

        // precision entry/exit defaults
        p_out.prec_entry_ms        = 450;
        p_out.prec_exit_ms         = 300;
        p_out.prec_entry_still_deg = 1.2f;
        p_out.prec_exit_move_deg   = 3.5f;
        p_out.prec_profile         = 0;

        // (기존) defaults
        p_out.ppt_start = {0x02, 0x3E}; // Shift+F5
        p_out.ppt_exit  = {0x00, 0x29}; // Esc
        p_out.ppt_next  = {0x00, 0x4E}; // PageDown
        p_out.ppt_prev  = {0x00, 0x4B}; // PageUp
        p_out.ppt_black = {0x00, 0x05}; // B
        p_out.ppt_laser = {0x01, 0x0F}; // Ctrl+L

        // (신규) defaults: kb로 mirror
        p_out.ppt2_start = {(uint8_t)EN_C10_KEYPAGE_KB, p_out.ppt_start.mod, (uint32_t)p_out.ppt_start.key};
        p_out.ppt2_exit  = {(uint8_t)EN_C10_KEYPAGE_KB, p_out.ppt_exit.mod,  (uint32_t)p_out.ppt_exit.key};
        p_out.ppt2_next  = {(uint8_t)EN_C10_KEYPAGE_KB, p_out.ppt_next.mod,  (uint32_t)p_out.ppt_next.key};
        p_out.ppt2_prev  = {(uint8_t)EN_C10_KEYPAGE_KB, p_out.ppt_prev.mod,  (uint32_t)p_out.ppt_prev.key};
        p_out.ppt2_black = {(uint8_t)EN_C10_KEYPAGE_KB, p_out.ppt_black.mod, (uint32_t)p_out.ppt_black.key};
        p_out.ppt2_laser = {(uint8_t)EN_C10_KEYPAGE_KB, p_out.ppt_laser.mod, (uint32_t)p_out.ppt_laser.key};
    }

    // ---------- Validation ----------
    bool validateWiFi(const ST_C10_WiFiConfig_t& p_w) {
        if (p_w.mode > (uint8_t)EN_C10_WIFI_STA) return false;

        size_t v_apLen = strlen(p_w.ap_pass);
        if (v_apLen > 0 && v_apLen < 8) return false;

        if (strlen(p_w.mdns_host) > C10_DEF::MDNS_MAX) return false;
        return true;
    }

    bool validateE10(const ST_C10_E10Config_t& p_e) {
        // FIX: dpi 1~3
        if (p_e.dpi_level < 1 || p_e.dpi_level > 3) return false;

        if (p_e.accel_threshold < 0.0f || p_e.accel_threshold > 50.0f) return false;
        if (p_e.wheel_threshold_deg < 1.0f || p_e.wheel_threshold_deg > 360.0f) return false;
        if (p_e.wheel_step_max < 1 || p_e.wheel_step_max > 50) return false;

        if (p_e.gesture_flick_deg < 10.0f || p_e.gesture_flick_deg > 2000.0f) return false;
        if (p_e.gesture_cooldown_ms > 20000) return false;

        if (p_e.scroll_cursor_damp < 0.0f || p_e.scroll_cursor_damp > 1.0f) return false;

        if (p_e.precision_mode >= (uint8_t)EN_C10_E10_PREC_MAX) return false;
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

        const ST_C10_PptKey2_t* v_keys[] = {
            &p_e.ppt2_start, &p_e.ppt2_exit, &p_e.ppt2_next,
            &p_e.ppt2_prev, &p_e.ppt2_black, &p_e.ppt2_laser
        };

        for (size_t v_i = 0; v_i < (sizeof(v_keys) / sizeof(v_keys[0])); v_i++) {
            const ST_C10_PptKey2_t& v_k = *v_keys[v_i];
            if (v_k.page > (uint8_t)EN_C10_KEYPAGE_CONSUMER) return false;

            if (v_k.page == (uint8_t)EN_C10_KEYPAGE_KB) {
                if (v_k.code > 0xE7) return false;
            } else {
                if (v_k.mod != 0) return false;
            }
        }
        return true;
    }

    // ---------- SafeBoot APIs ----------
    bool isSafeMode() const { return _boot.safe_mode; }

    void getBootState(ST_C10_BootState_t& p_out) { p_out = _boot; }

    bool clearSafeMode() {
        _boot.safe_mode  = false;
        _boot.fail_count = 0;
        _boot.pending    = false;
        return _saveBootState(_boot);
    }
    
    bool bootMarkOkIfGracePassed(uint32_t p_graceMs) {
        // boot_ms가 0이면(구버전 파일) 지금 시점으로 보정
        if (_boot.boot_ms == 0) _boot.boot_ms = (uint32_t)millis();
    
        const uint32_t v_now = (uint32_t)millis();
        const uint32_t v_aliveMs = (v_now >= _boot.boot_ms) ? (v_now - _boot.boot_ms) : 0;
    
        if (v_aliveMs < p_graceMs) {
            return false; // 아직 그레이스 미통과
        }
    
        // 그레이스 통과 => 이번 부팅 정상 판정
        _boot.pending = false;
    
        // safe_mode가 아니면 fail_count 리셋
        if (!_boot.safe_mode) {
            _boot.fail_count = 0;
        }
    
        // safe_mode 리셋은 사용자가 clearSafeMode()로만 하게 유지(정책 고정)
        // 원하면 자동 해제 옵션을 여기서 추가 가능
    
        return _saveBootState(_boot);
    }


    // ---------- Config Load/Save ----------
    bool loadAll(ST_C10_WiFiConfig_t& p_wifi, ST_C10_E10Config_t& p_e10) {
        File v_f = LittleFS.open(C10_DEF::CFG_PATH, "r");
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
        _buildJson(p_wifi, p_e10, v_doc);

        File v_tmp = LittleFS.open(C10_DEF::CFG_TMP, "w");
        if (!v_tmp) return false;

        if (serializeJson(v_doc, v_tmp) == 0) {
            v_tmp.close();
            (void)LittleFS.remove(C10_DEF::CFG_TMP);
            return false;
        }
        v_tmp.flush();
        v_tmp.close();

        if (!_verifyJsonFile(C10_DEF::CFG_TMP)) {
            (void)LittleFS.remove(C10_DEF::CFG_TMP);
            return false;
        }

        if (LittleFS.exists(C10_DEF::CFG_BAK)) (void)LittleFS.remove(C10_DEF::CFG_BAK);
        if (LittleFS.exists(C10_DEF::CFG_PATH)) {
            if (!LittleFS.rename(C10_DEF::CFG_PATH, C10_DEF::CFG_BAK)) {
                (void)_copyFile(C10_DEF::CFG_PATH, C10_DEF::CFG_BAK);
            }
        }

        if (!LittleFS.rename(C10_DEF::CFG_TMP, C10_DEF::CFG_PATH)) {
            (void)LittleFS.remove(C10_DEF::CFG_PATH);
            if (LittleFS.exists(C10_DEF::CFG_BAK)) (void)LittleFS.rename(C10_DEF::CFG_BAK, C10_DEF::CFG_PATH);
            (void)LittleFS.remove(C10_DEF::CFG_TMP);
            return false;
        }
        return true;
    }

    bool rollbackFromBak() {
        if (!LittleFS.exists(C10_DEF::CFG_BAK)) return false;
        if (!_verifyJsonFile(C10_DEF::CFG_BAK)) return false;
        if (LittleFS.exists(C10_DEF::CFG_PATH)) (void)LittleFS.remove(C10_DEF::CFG_PATH);
        return LittleFS.rename(C10_DEF::CFG_BAK, C10_DEF::CFG_PATH);
    }

    bool exportJson(String& p_out) {
        File v_f = LittleFS.open(C10_DEF::CFG_PATH, "r");
        if (!v_f) return false;
        p_out = v_f.readString();
        v_f.close();
        return (p_out.length() > 0);
    }

    bool importJson(const String& p_json, bool& p_saved, bool& p_applied) {
        p_saved = false;
        p_applied = false;

        { JsonDocument v_doc; if (deserializeJson(v_doc, p_json)) return false; }

        ST_C10_WiFiConfig_t v_w;
        ST_C10_E10Config_t  v_e;
        makeDefaultsWiFi(v_w);
        makeDefaultsE10(v_e);
        (void)loadAll(v_w, v_e);

        bool v_ok = true;
        v_ok = v_ok && patchFromJsonWiFi(p_json, v_w);
        v_ok = v_ok && patchFromJsonE10(p_json, v_e);

        v_ok = v_ok && validateWiFi(v_w);
        v_ok = v_ok && validateE10(v_e);
        if (!v_ok) return false;

        p_saved = saveAll(v_w, v_e);
        p_applied = p_saved; // 현재 정책상 저장=적용 가능 상태로 표기
        return p_saved;
    }

    // ---------- Patchers ----------
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

        if (!v_e10["dpi_level"].isNull())        p_e10.dpi_level = (uint8_t)v_e10["dpi_level"];
        if (!v_e10["hard_click_lock"].isNull())  p_e10.hard_click_lock = (bool)v_e10["hard_click_lock"];

        JsonVariant v_sb = v_e10["scale_base"];
        if (v_sb.is<JsonArray>()) {
            JsonArray v_a = v_sb.as<JsonArray>();
            if (v_a.size() >= 3) {
                p_e10.scale_base[0] = (float)v_a[0];
                p_e10.scale_base[1] = (float)v_a[1];
                p_e10.scale_base[2] = (float)v_a[2];
            }
        }

        JsonVariant v_ag = v_e10["accel_gain"];
        if (v_ag.is<JsonArray>()) {
            JsonArray v_a = v_ag.as<JsonArray>();
            if (v_a.size() >= 3) {
                p_e10.accel_gain[0] = (float)v_a[0];
                p_e10.accel_gain[1] = (float)v_a[1];
                p_e10.accel_gain[2] = (float)v_a[2];
            }
        }

        if (!v_e10["accel_threshold"].isNull()) p_e10.accel_threshold = (float)v_e10["accel_threshold"];

        JsonVariant v_wh = v_e10["wheel"];
        if (!v_wh.isNull()) {
            if (!v_wh["threshold_deg"].isNull()) p_e10.wheel_threshold_deg = (float)v_wh["threshold_deg"];
            if (!v_wh["step_max"].isNull())      p_e10.wheel_step_max = (uint8_t)v_wh["step_max"];
        }

        JsonVariant v_g = v_e10["gesture"];
        if (!v_g.isNull()) {
            if (!v_g["flick_deg"].isNull())     p_e10.gesture_flick_deg = (float)v_g["flick_deg"];
            if (!v_g["cooldown_ms"].isNull())   p_e10.gesture_cooldown_ms = (uint16_t)v_g["cooldown_ms"];
        }

        if (!v_e10["scroll_cursor_damp"].isNull()) p_e10.scroll_cursor_damp = (float)v_e10["scroll_cursor_damp"];

        JsonVariant v_p = v_e10["precision"];
        if (!v_p.isNull()) {
            
            if (!v_p["mode"].isNull())              p_e10.precision_mode = (uint8_t)v_p["mode"];
            // if (!v_p["enable"].isNull())            p_e10.precision_enable = (bool)v_p["enable"];
            if (!v_p["deadzone"].isNull())          p_e10.precision_deadzone = (float)v_p["deadzone"];
            if (!v_p["gain"].isNull())              p_e10.precision_gain = (float)v_p["gain"];
            if (!v_p["accel"].isNull())             p_e10.precision_accel = (float)v_p["accel"];
            if (!v_p["max_step"].isNull())          p_e10.precision_max_step = (uint8_t)v_p["max_step"];
            if (!v_p["smooth"].isNull())            p_e10.precision_smooth = (float)v_p["smooth"];

            if (!v_p["entry_ms"].isNull())          p_e10.prec_entry_ms = (uint16_t)v_p["entry_ms"];
            if (!v_p["exit_ms"].isNull())           p_e10.prec_exit_ms  = (uint16_t)v_p["exit_ms"];
            if (!v_p["entry_still_deg"].isNull())   p_e10.prec_entry_still_deg = (float)v_p["entry_still_deg"];
            if (!v_p["exit_move_deg"].isNull())     p_e10.prec_exit_move_deg   = (float)v_p["exit_move_deg"];
            if (!v_p["profile"].isNull())           p_e10.prec_profile = (uint8_t)v_p["profile"];
        }

        JsonVariant v_pk2 = v_e10["ppt_keys2"];
        if (!v_pk2.isNull()) {
            auto v_loadKey2 = [&](const char* p_name, ST_C10_PptKey2_t& p_k) {
                JsonVariant v_o = v_pk2[p_name];
                if (v_o.isNull()) return;

                if (!v_o["page"].isNull()) {
                    if (v_o["page"].is<const char*>()) {
                        const char* v_s = (const char*)v_o["page"];
                        if (v_s && strcasecmp(v_s, "consumer") == 0) p_k.page = (uint8_t)EN_C10_KEYPAGE_CONSUMER;
                        else p_k.page = (uint8_t)EN_C10_KEYPAGE_KB;
                    } else {
                        p_k.page = (uint8_t)v_o["page"];
                    }
                }
                if (!v_o["mod"].isNull())  p_k.mod  = (uint8_t)v_o["mod"];
                if (!v_o["code"].isNull()) p_k.code = (uint32_t)v_o["code"];
            };

            v_loadKey2("start", p_e10.ppt2_start);
            v_loadKey2("exit",  p_e10.ppt2_exit);
            v_loadKey2("next",  p_e10.ppt2_next);
            v_loadKey2("prev",  p_e10.ppt2_prev);
            v_loadKey2("black", p_e10.ppt2_black);
            v_loadKey2("laser", p_e10.ppt2_laser);
        } else {
            // ppt_keys2 없으면 ppt_keys mirror (기존 호환)
            p_e10.ppt2_start = {(uint8_t)EN_C10_KEYPAGE_KB, p_e10.ppt_start.mod, (uint32_t)p_e10.ppt_start.key};
            p_e10.ppt2_exit  = {(uint8_t)EN_C10_KEYPAGE_KB, p_e10.ppt_exit.mod,  (uint32_t)p_e10.ppt_exit.key};
            p_e10.ppt2_next  = {(uint8_t)EN_C10_KEYPAGE_KB, p_e10.ppt_next.mod,  (uint32_t)p_e10.ppt_next.key};
            p_e10.ppt2_prev  = {(uint8_t)EN_C10_KEYPAGE_KB, p_e10.ppt_prev.mod,  (uint32_t)p_e10.ppt_prev.key};
            p_e10.ppt2_black = {(uint8_t)EN_C10_KEYPAGE_KB, p_e10.ppt_black.mod, (uint32_t)p_e10.ppt_black.key};
            p_e10.ppt2_laser = {(uint8_t)EN_C10_KEYPAGE_KB, p_e10.ppt_laser.mod, (uint32_t)p_e10.ppt_laser.key};
        }

        return true;
    }

    // ---------- Factory Reset ----------
    bool factoryReset(bool p_recreateDefault) {
        if (LittleFS.exists(C10_DEF::CFG_TMP)) (void)LittleFS.remove(C10_DEF::CFG_TMP);
        if (LittleFS.exists(C10_DEF::CFG_BAK)) (void)LittleFS.remove(C10_DEF::CFG_BAK);
        if (LittleFS.exists(C10_DEF::CFG_PATH)) (void)LittleFS.remove(C10_DEF::CFG_PATH);

        if (LittleFS.exists(C10_DEF::BOOT_TMP_PATH)) (void)LittleFS.remove(C10_DEF::BOOT_TMP_PATH);
        if (LittleFS.exists(C10_DEF::BOOT_PATH)) (void)LittleFS.remove(C10_DEF::BOOT_PATH);

        memset(&_boot, 0, sizeof(_boot));
        _boot.pending = false;
        (void)_saveBootState(_boot);

        if (p_recreateDefault) {
            ST_C10_WiFiConfig_t v_w;
            ST_C10_E10Config_t  v_e;
            makeDefaultsWiFi(v_w);
            makeDefaultsE10(v_e);
            return saveAll(v_w, v_e);
        }
        return true;
    }
    
    bool _isBadResetReason(esp_reset_reason_t p_r) {
        // “진짜 실패”로 볼 리셋 원인만 true
        switch (p_r) {
            case ESP_RST_PANIC:
            case ESP_RST_INT_WDT:
            case ESP_RST_TASK_WDT:
            case ESP_RST_WDT:
                return true;

            // 브라운아웃을 실패로 볼지 정책 선택:
            // - 현장 전원 불안정이면 safe로 유도하는 게 맞을 수 있음
            case ESP_RST_BROWNOUT:
                return true;

            default:
                return false;
        }
    }

    uint8_t _getResetReasonU8() {
        esp_reset_reason_t v_r = esp_reset_reason();
        return (uint8_t)v_r;
    }
    

  private:
    // ---------- JSON build ----------
    void _buildJson(const ST_C10_WiFiConfig_t& p_w, const ST_C10_E10Config_t& p_e, JsonDocument& p_doc) {
        p_doc["ver"] = (uint16_t)G_C10_CFG_VER;

        JsonObject v_jw = p_doc["wifi"].to<JsonObject>();
        v_jw["mode"] = p_w.mode;

        JsonObject v_jsta = v_jw["sta"].to<JsonObject>();
        v_jsta["ssid"] = p_w.sta_ssid;
        v_jsta["pass"] = p_w.sta_pass;

        JsonObject v_jap = v_jw["ap"].to<JsonObject>();
        v_jap["ssid"] = p_w.ap_ssid;
        v_jap["pass"] = p_w.ap_pass;

        JsonObject v_jmd = v_jw["mdns"].to<JsonObject>();
        v_jmd["host"] = p_w.mdns_host;

        JsonObject v_je = p_doc["e10"].to<JsonObject>();
        v_je["dpi_level"]       = p_e.dpi_level;
        v_je["hard_click_lock"] = p_e.hard_click_lock;

        JsonArray v_sb = v_je["scale_base"].to<JsonArray>();
        v_sb.add(p_e.scale_base[0]); v_sb.add(p_e.scale_base[1]); v_sb.add(p_e.scale_base[2]);

        JsonArray v_ag = v_je["accel_gain"].to<JsonArray>();
        v_ag.add(p_e.accel_gain[0]); v_ag.add(p_e.accel_gain[1]); v_ag.add(p_e.accel_gain[2]);

        v_je["accel_threshold"] = p_e.accel_threshold;

        JsonObject v_wh = v_je["wheel"].to<JsonObject>();
        v_wh["threshold_deg"] = p_e.wheel_threshold_deg;
        v_wh["step_max"]      = p_e.wheel_step_max;

        JsonObject v_g = v_je["gesture"].to<JsonObject>();
        v_g["flick_deg"]   = p_e.gesture_flick_deg;
        v_g["cooldown_ms"] = p_e.gesture_cooldown_ms;

        v_je["scroll_cursor_damp"] = p_e.scroll_cursor_damp;

        JsonObject v_p = v_je["precision"].to<JsonObject>();
        
        v_p["mode"]     = p_e.precision_mode; 
        //v_p["enable"]   = p_e.precision_enable;
        
        v_p["deadzone"] = p_e.precision_deadzone;
        v_p["gain"]     = p_e.precision_gain;
        v_p["accel"]    = p_e.precision_accel;
        v_p["max_step"] = p_e.precision_max_step;
        v_p["smooth"]   = p_e.precision_smooth;

        v_p["entry_ms"]         = p_e.prec_entry_ms;
        v_p["exit_ms"]          = p_e.prec_exit_ms;
        v_p["entry_still_deg"]  = p_e.prec_entry_still_deg;
        v_p["exit_move_deg"]    = p_e.prec_exit_move_deg;
        v_p["profile"]          = p_e.prec_profile;

        JsonObject v_pk2 = v_je["ppt_keys2"].to<JsonObject>();
        auto v_putK2 = [&](const char* p_name, const ST_C10_PptKey2_t& p_k) {
            JsonObject v_o = v_pk2[p_name].to<JsonObject>();
            v_o["page"] = (p_k.page == (uint8_t)EN_C10_KEYPAGE_CONSUMER) ? "consumer" : "kb";
            v_o["mod"]  = p_k.mod;
            v_o["code"] = p_k.code;
        };

        v_putK2("start", p_e.ppt2_start);
        v_putK2("exit",  p_e.ppt2_exit);
        v_putK2("next",  p_e.ppt2_next);
        v_putK2("prev",  p_e.ppt2_prev);
        v_putK2("black", p_e.ppt2_black);
        v_putK2("laser", p_e.ppt2_laser);
    }

    // ---------- Boot state ----------
    bool _loadBootState(ST_C10_BootState_t& p_out) {
        memset(&p_out, 0, sizeof(p_out));

        if (!LittleFS.exists(C10_DEF::BOOT_PATH)) {
            return _saveBootState(p_out);
        }

        File v_f = LittleFS.open(C10_DEF::BOOT_PATH, "r");
        if (!v_f) return false;

        JsonDocument v_d;
        DeserializationError v_err = deserializeJson(v_d, v_f);
        v_f.close();
        if (v_err) return false;

        if (!v_d["safe_mode"].isNull())  p_out.safe_mode  = (bool)v_d["safe_mode"];
        if (!v_d["fail_count"].isNull()) p_out.fail_count = (uint8_t)v_d["fail_count"];
        if (!v_d["pending"].isNull())    p_out.pending    = (bool)v_d["pending"];
        
        // (NEW)
        if (!v_d["boot_ms"].isNull())    p_out.boot_ms    = (uint32_t)v_d["boot_ms"];
        
        if (!v_d["last_reset_reason"].isNull()) p_out.last_reset_reason = (uint8_t)v_d["last_reset_reason"];
        
    
        return true;
    }

    bool _saveBootState(const ST_C10_BootState_t& p_in) {
        JsonDocument v_d;
        v_d["safe_mode"]  = p_in.safe_mode;
        v_d["fail_count"] = p_in.fail_count;
        v_d["pending"]    = p_in.pending;
        
        // (NEW)
        v_d["boot_ms"]    = p_in.boot_ms;
        
        v_d["last_reset_reason"] = p_in.last_reset_reason;
        

        File v_tmp = LittleFS.open(C10_DEF::BOOT_TMP_PATH, "w");
        if (!v_tmp) return false;

        if (serializeJson(v_d, v_tmp) == 0) {
            v_tmp.close();
            (void)LittleFS.remove(C10_DEF::BOOT_TMP_PATH);
            return false;
        }
        v_tmp.flush();
        v_tmp.close();

        if (!_verifyJsonFile(C10_DEF::BOOT_TMP_PATH)) {
            (void)LittleFS.remove(C10_DEF::BOOT_TMP_PATH);
            return false;
        }

        if (LittleFS.exists(C10_DEF::BOOT_PATH)) (void)LittleFS.remove(C10_DEF::BOOT_PATH);
        return LittleFS.rename(C10_DEF::BOOT_TMP_PATH, C10_DEF::BOOT_PATH);
    }

    // ---------- file utils ----------
    bool _verifyJsonFile(const char* p_path) {
        File v_f = LittleFS.open(p_path, "r");
        if (!v_f) return false;

        JsonDocument v_d;
        bool v_ok = !deserializeJson(v_d, v_f);
        v_f.close();
        return v_ok;
    }

    bool _copyFile(const char* p_src, const char* p_dst) {
        File v_s = LittleFS.open(p_src, "r");
        if (!v_s) return false;

        File v_d = LittleFS.open(p_dst, "w");
        if (!v_d) { v_s.close(); return false; }

        uint8_t v_buf[256];
        while (true) {
            int v_n = v_s.read(v_buf, sizeof(v_buf));
            if (v_n <= 0) break;
            if (v_d.write(v_buf, (size_t)v_n) != (size_t)v_n) {
                v_s.close(); v_d.close();
                return false;
            }
        }

        v_d.flush();
        v_s.close();
        v_d.close();
        return true;
    }
};
