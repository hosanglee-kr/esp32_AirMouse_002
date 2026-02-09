// ======================================================
// File: src/main.cpp
// ======================================================
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

#include "v010/A40_ComFunc_070.h" // 내부에서 D10_Logger_060.h 포함

#include "v030/C10_Config_0302.h"
#include "v030/E10_EliteAirMouse_0302.h"
#include "v030/W10_WebConfig_0300.h"

static CL_C10_Config        g_cfg;
static CL_E10_EliteAirMouse g_e10;
static CL_W10_WebConfig     g_w10;

// 버튼 핀(기존 E10과 일치 가정)
static constexpr int 		G_BTN_MODE 		= 13;

// grace
static constexpr uint32_t 	G_BOOT_GRACE_MS = 8500;

// 내부: 부팅 시 N ms 이상 계속 눌림이면 true
static bool holdAtBoot_(int pin, uint32_t ms) {
    pinMode(pin, INPUT_PULLUP);
    const uint32_t t0 = millis();
    while (millis() - t0 < ms) {
        if (digitalRead(pin) != LOW) return false;
        delay(10);
    }
    return true;
}

static bool     			g_bootOkDone 	= false;
static uint32_t 			g_tStart     	= 0;

void setup() {
    Serial.begin(115200);

    CL_D10_Logger::begin(Serial);
    CL_D10_Logger::setLevel(EN_D10_LOG_INFO);
    CL_D10_Logger::enableTimestamp(true);
    CL_D10_Logger::enableMemUsage(false);

    // 0) FS mount (1회만)
    if (!LittleFS.begin(true)) {
        D10_LOGE("LittleFS begin failed");
    } else {
        D10_LOGI("LittleFS mounted");
    }

    // 1) C10 begin (내부에서 /json 보장 + 기본 config 생성 + SafeBoot pending 마킹)
    g_cfg.begin(true);

    // 2) 부팅 중 Factory Reset (MODE 6초)
    //    - C10 begin 이후 호출: 경로/디렉토리 보장
    if (holdAtBoot_(G_BTN_MODE, 6000)) {
        (void)g_cfg.factoryReset(true); // true면 즉시 기본 config 재생성
        D10_LOGW("[0274] FactoryReset by boot key. rebooting...");
        delay(200);
        ESP.restart();
    }

    // 3) Safe Boot 상태 확인
    //    - Safe mode일 때는 W10.setupWiFi_()가 강제로 AP 타야 함
    if (g_cfg.isSafeMode()) {
        D10_LOGW("[0274] SAFE BOOT MODE ACTIVE");
    }

    // 4) 모듈 시작
    g_e10.begin(&g_cfg);
    g_w10.begin(&g_cfg, CL_E10_EliteAirMouse::E10_W10Apply, (void*)&g_e10);

    D10_LOGI("[0274] started");

    // 5) grace 타이머 시작
    g_tStart     = millis();
    g_bootOkDone = false;
}

void loop() {
    // 6) 8.5초 생존 → boot ok 처리(벽돌 방지 핵심)
    //    - 1회만 수행
    if (!g_bootOkDone) {
        const uint32_t up = millis() - g_tStart;
        if (up >= G_BOOT_GRACE_MS) {
            (void)g_cfg.bootMarkOkIfGracePassed(G_BOOT_GRACE_MS);
            g_bootOkDone = true;
            D10_LOGI("[0274] boot grace passed -> boot ok marked");
        }
    }

    delay(200);
}
