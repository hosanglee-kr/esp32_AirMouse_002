// ======================================================
// File: src/main.cpp
// ======================================================
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

#include "v031/A40_ComFunc_070.h" // 내부에서 D10_Logger_060.h 포함
#include "v031/C10_Config_0310.h"
#include "v031/E10_AirMouse_0310.h"
#include "v031/W10_WebCfg_0314.h"

static CL_C10_Config        g_cfg;
static CL_E10_EliteAirMouse g_e10;
static CL_W10_WebConfig     g_w10;


static ST_W10_E10If_t       g_w10E10If;

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



static bool _w10_setPptMode(void* ctx, bool en) {
    if (!ctx) return false;
    return ((CL_E10_EliteAirMouse*)ctx)->setPptMode(en);
}
static bool _w10_setDpiLevel(void* ctx, uint8_t lv) {
    if (!ctx) return false;
    return ((CL_E10_EliteAirMouse*)ctx)->setDpiLevel(lv);
}
static bool _w10_setHardClickLock(void* ctx, bool en) {
    if (!ctx) return false;
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
// 버튼 핀(기존 E10과 일치 가정)
static constexpr int      G_BTN_MODE       = 13;

// grace
static constexpr uint32_t G_BOOT_GRACE_MS  = 8500;

// 내부: 부팅 시 p_ms 이상 계속 눌림이면 true
static bool _holdAtBoot(int p_pin, uint32_t p_ms) {
    pinMode(p_pin, INPUT_PULLUP);

    const uint32_t v_t0 = (uint32_t)millis();
    while (((uint32_t)millis() - v_t0) < p_ms) {
        if (digitalRead(p_pin) != LOW) return false;
        delay(10);
    }
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

    // 2) Factory Reset (MODE 6초)
    if (_holdAtBoot(G_BTN_MODE, 6000)) {
        (void)g_cfg.factoryReset(true);
        D10_LOGW("[0274] FactoryReset by boot key. rebooting...");
        delay(200);
        ESP.restart();
    }

    // 3) Safe Boot 상태 확인
    if (g_cfg.isSafeMode()) {
        D10_LOGW("[0274] SAFE BOOT MODE ACTIVE");
    
        // A-4: SAFE 진입 시 config.bak 자동 롤백 1회 시도
        bool v_rb = g_cfg.rollbackFromBak();
        if (v_rb) {
            D10_LOGW("[0274] rollbackFromBak OK (auto)");
        } else {
            D10_LOGW("[0274] rollbackFromBak skipped/failed (no bak or invalid)");
        }
    }


    // 4) 모듈 시작
    g_e10.begin(&g_cfg);
    g_e10.setSafeMode(g_cfg.isSafeMode());
    
        // W10-E10 interface bind
    g_w10E10If.ctx            = (void*)&g_e10;
    g_w10E10If.getStatus       = _w10_getStatus;
    g_w10E10If.applyRuntimeE10 = _w10_applyRuntimeE10;
    g_w10E10If.setPrecisionMode= _w10_setPrecisionMode;
    g_w10E10If.setSafeMode     = _w10_setSafeMode;
    g_w10E10If.testPptKey2     = _w10_testPptKey2;
    g_w10E10If.setPptMode     = _w10_setPptMode;
    g_w10E10If.setDpiLevel    = _w10_setDpiLevel;
    g_w10E10If.setHardClickLock= _w10_setHardClickLock;
    g_w10E10If.forceReleaseButtons = _w10_forceReleaseButtons;
    g_w10E10If.requestI2CRecover   = _w10_requestI2CRecover;
    g_w10E10If.requestGyroCalibration = _w10_requestGyroCalibration;
    g_w10E10If.clearDiagnostics    = _w10_clearDiagnostics;
    g_w10E10If.setOtaGuard     = _w10_setOtaGuard;

    g_w10.begin(&g_cfg, CL_E10_EliteAirMouse::E10_W10Apply, (void*)&g_e10, &g_w10E10If);

    D10_LOGI("[0274] started");

    g_bootOkDone = false;
}

void loop() {
    // 5) grace 통과 시 boot ok 처리 (1회)
    if (!g_bootOkDone) {
        if (g_cfg.bootMarkOkIfGracePassed(G_BOOT_GRACE_MS)) {
            g_bootOkDone = true;
            D10_LOGI("[0274] boot grace passed -> boot ok marked");
        }
    }

    delay(200);
}
