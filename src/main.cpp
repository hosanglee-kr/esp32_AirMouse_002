// ======================================================
// File: src/main.cpp
// ======================================================
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

// [v0415] A40_ComFunc 삭제 → D10_Logger_0415.h 직접 include
//   - v0412의 "A40 우회 include" 제거
//   - D10 로거를 직접 사용
#include "v0415/D10_Logger_0415.h"
#include "v0415/C10_Config_0415.h"
#include "v0415/E10_AirMouse_0415.h"
#include "v0415/HW_Def_0415.h"
#include "v0415/W10_Web_0415.h"

static CL_C10_Config        g_cfg;
static CL_E10_EliteAirMouse g_e10;
static CL_W10_WebConfig     g_w10;

static ST_W10_E10If_t g_w10E10If;

static bool g_bootOkDone = false;

// ---- W10-E10 bridge callbacks ----
static bool _w10_getStatus(void* ctx, ST_E10_Status_t* out) {
    if (!ctx || !out) return false;
    ((CL_E10_EliteAirMouse*)ctx)->getStatus(*out);
    return true;
}

static bool _w10_applyRuntimeE10(void* ctx, const ST_C10_E10Config_t* e10) {
    if (!ctx || !e10) return false;
    return ((CL_E10_EliteAirMouse*)ctx)->applyRuntimeE10(*e10);
}

static bool _w10_setPrecisionMode(void* ctx, uint8_t mode) {
    if (!ctx) return false;
    return ((CL_E10_EliteAirMouse*)ctx)->setPrecisionMode(mode);
}

static bool _w10_setSafeMode(void* ctx, bool en) {
    if (!ctx) return false;
    return ((CL_E10_EliteAirMouse*)ctx)->setSafeMode(en);
}

static bool _w10_testPptKey2(void* ctx, uint8_t page, uint8_t mod, uint32_t code) {
    if (!ctx) return false;
    return ((CL_E10_EliteAirMouse*)ctx)->testPptKey2(page, mod, code);
}

// [v0415 삭제] _w10_setPptMode — _isPptMode 필드 삭제 (Round G)
//   - PPT 판정은 _activeMode == 2 로 일원화
//   - W10의 set_ppt cmd도 삭제됨 (Round M)
//   - ST_W10_E10If_t::setPptMode 콜백 자체가 삭제됨

static bool _w10_setDpiLevel(void* ctx, uint8_t lv) {
    if (!ctx) return false;
    return ((CL_E10_EliteAirMouse*)ctx)->setDpiLevel(lv);
}

static bool _w10_setHardClickLock(void* ctx, bool en) {
    if (!ctx) return false;
    // [v0415] M10 Click-Lock 삭제 → E10 setHardClickLock은 no-op 스텁 (Round L)
    //   - 호환성을 위해 콜백은 유지 (W10이 요청 시 성공 반환)
    return ((CL_E10_EliteAirMouse*)ctx)->setHardClickLock(en);
}

static bool _w10_forceReleaseButtons(void* ctx) {
    if (!ctx) return false;
    return ((CL_E10_EliteAirMouse*)ctx)->forceReleaseButtons();
}

static bool _w10_requestI2CRecover(void* ctx) {
    if (!ctx) return false;
    return ((CL_E10_EliteAirMouse*)ctx)->requestI2CRecover();
}

static bool _w10_requestGyroCalibration(void* ctx) {
    if (!ctx) return false;
    return ((CL_E10_EliteAirMouse*)ctx)->requestGyroCalibration();
}

static bool _w10_clearDiagnostics(void* ctx) {
    if (!ctx) return false;
    return ((CL_E10_EliteAirMouse*)ctx)->clearDiagnostics();
}

static bool _w10_setOtaGuard(void* ctx, bool en) {
    if (!ctx) return false;
    return ((CL_E10_EliteAirMouse*)ctx)->setOtaGuard(en);
}

// ---- Profile 관리 브릿지 ----
static bool _w10_reloadProfile(void* ctx) {
    if (!ctx) return false;
    return ((CL_E10_EliteAirMouse*)ctx)->reloadActiveProfile();
}

static bool _w10_saveProfile(void* ctx) {
    if (!ctx) return false;
    return ((CL_E10_EliteAirMouse*)ctx)->saveActiveProfile();
}

static bool _w10_getProfileInfo(void* ctx,
                                uint8_t* outIdx, uint8_t* outCount,
                                char* outName, size_t outNameSize) {
    if (!ctx) return false;
    uint8_t v_idx = 0, v_cnt = 0;
    const bool v_ok = ((CL_E10_EliteAirMouse*)ctx)->getActiveProfileInfo(
                          v_idx, v_cnt, outName, outNameSize);
    if (outIdx)   *outIdx   = v_idx;
    if (outCount) *outCount = v_cnt;
    return v_ok;
}

static bool _w10_switchProfile(void* ctx, uint8_t p_idx) {
    if (!ctx) return false;
    return ((CL_E10_EliteAirMouse*)ctx)->switchProfile(p_idx);
}

static bool _w10_execLiveTest(void* ctx,
                              uint8_t p_kind, uint8_t p_hMode,
                              uint16_t p_p16, uint32_t p_p32) {
    if (!ctx) return false;
    return ((CL_E10_EliteAirMouse*)ctx)->execLiveTest(p_kind, p_hMode, p_p16, p_p32);
}

// grace
static constexpr uint32_t G_BOOT_GRACE_MS = 8500;

// 내부: 부팅 시 p_ms 이상 계속 눌림이면 true
//   - 1초마다 '.' 출력 → 사용자에게 진행 피드백 (LED는 아직 초기화 안 됨)
static bool _holdAtBoot(int p_pin, uint32_t p_ms) {
    pinMode(p_pin, INPUT_PULLUP);

    const uint32_t v_t0 = (uint32_t)millis();
    uint32_t v_lastBeep = v_t0;
    Serial.print("[boot-hold] keep held: ");

    while (((uint32_t)millis() - v_t0) < p_ms) {
        if (digitalRead(p_pin) != LOW) {
            Serial.println(" (released)");
            return false;
        }
        const uint32_t v_now = (uint32_t)millis();
        if ((uint32_t)(v_now - v_lastBeep) >= 1000) {
            v_lastBeep = v_now;
            Serial.print(".");
        }
        delay(10);
    }
    Serial.println(" OK (factory reset)");
    return true;
}

void setup() {
    Serial.begin(115200);

    CL_D10_Logger::begin(Serial);
    CL_D10_Logger::setLevel(EN_D10_LOG_INFO);
    CL_D10_Logger::enableTimestamp(true);
    CL_D10_Logger::enableMemUsage(false);

    // 1) C10 begin
    g_cfg.begin(false);

    // 2) Factory Reset (Side C 6초 hold)
    if (_holdAtBoot(HW_DEF::PIN_BTN_SIDE_C, 6000)) {
        (void)g_cfg.factoryReset(true);
        D10_LOGW("[0415] FactoryReset by boot key. rebooting...");
        delay(200);
        ESP.restart();
    }

    // 3) SafeMode 상태 스냅샷 (begin 이전 확보)
    const bool v_safe = g_cfg.isSafeMode();

    // 4) 모듈 시작
    g_e10.begin(&g_cfg);

    // SafeMode 활성 시에만 errHist 이벤트 기록
    if (v_safe) {
        D10_LOGW("[0415] SAFE BOOT MODE ACTIVE");
        g_e10.setSafeMode(true);
    } else {
        D10_LOGI("[0415] boot normal mode");
    }

    // W10-E10 interface bind
    // [v0415] setPptMode 콜백 삭제 (Round G/M 정합)
    g_w10E10If.ctx                    = (void*)&g_e10;
    g_w10E10If.getStatus              = _w10_getStatus;
    g_w10E10If.applyRuntimeE10        = _w10_applyRuntimeE10;
    g_w10E10If.setPrecisionMode       = _w10_setPrecisionMode;
    g_w10E10If.setSafeMode            = _w10_setSafeMode;
    g_w10E10If.testPptKey2            = _w10_testPptKey2;
    // [v0415 삭제] g_w10E10If.setPptMode = _w10_setPptMode;
    g_w10E10If.setDpiLevel            = _w10_setDpiLevel;
    g_w10E10If.setHardClickLock       = _w10_setHardClickLock;
    g_w10E10If.forceReleaseButtons    = _w10_forceReleaseButtons;
    g_w10E10If.requestI2CRecover      = _w10_requestI2CRecover;
    g_w10E10If.requestGyroCalibration = _w10_requestGyroCalibration;
    g_w10E10If.clearDiagnostics       = _w10_clearDiagnostics;
    g_w10E10If.setOtaGuard            = _w10_setOtaGuard;

    // Profile & Live Test
    g_w10E10If.reloadProfile          = _w10_reloadProfile;
    g_w10E10If.saveProfile            = _w10_saveProfile;
    g_w10E10If.getProfileInfo         = _w10_getProfileInfo;
    g_w10E10If.switchProfile          = _w10_switchProfile;
    g_w10E10If.execLiveTest           = _w10_execLiveTest;

    g_w10.begin(&g_cfg, &g_w10E10If);

    D10_LOGI("[0415] started");

    g_bootOkDone = false;
}

void loop() {
    // 5) grace 통과 시 boot ok 처리 (1회)
    if (!g_bootOkDone) {
        if (g_cfg.bootMarkOkIfGracePassed(G_BOOT_GRACE_MS)) {
            g_bootOkDone = true;
            D10_LOGI("[0415] boot grace passed -> boot ok marked");
        }
    }

    // 6) BLE dirty → config 저장 (rare event, 200ms cadence)
    g_e10.tickConfigSave();

    delay(200);
}
