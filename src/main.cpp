// ======================================================
// File: src/main.cpp (정리 버전: W10_Apply 함수 없음)
// ======================================================
#include <Arduino.h>


#include "v010/A40_ComFunc_070.h"   // 내부에서 D10_Logger_060.h 포함

#include "v010/C10_Config_0273.h"
#include "v010/E10_EliteAirMouse_0272.h"
#include "v010/W10_WebConfig_0273.h"


static CL_C10_Config g_cfg;
static CL_E10_EliteAirMouse g_e10;
static CL_W10_WebConfig g_w10;

// 버튼 핀(기존 E10과 일치 가정)
static constexpr int G_BTN_MODE = 13;

static bool holdAtBoot_(int pin, uint32_t ms){
  pinMode(pin, INPUT_PULLUP);
  uint32_t t0=millis();
  while(millis()-t0 < ms){
    if(digitalRead(pin)!=LOW) return false;
    delay(10);
  }
  return true;
}

void setup(){
  Serial.begin(115200);
  
  CL_D10_Logger::begin(Serial);
  CL_D10_Logger::setLevel(EN_D10_LOG_INFO);
  CL_D10_Logger::enableTimestamp(true);
  CL_D10_Logger::enableMemUsage(false);

  // LittleFS
  if (!LittleFS.begin(true)) {
    D10_LOGE("LittleFS begin failed");
  } else {
    D10_LOGI("LittleFS mounted");
  }
  
  

  // 1) 부팅 중 Factory Reset (MODE 6초)
  if(holdAtBoot_(G_BTN_MODE, 6000)){
    (void)LittleFS.begin(true);
    g_cfg.factoryReset(true);
    Serial.println("[0273] FactoryReset by boot key. rebooting...");
    delay(200);
    ESP.restart();
  }

  // 2) config + SafeBoot start mark
  g_cfg.begin(true);

  // 3) Safe Mode 판단
  bool safe = g_cfg.isSafeMode();
  if(safe){
    Serial.println("[0273] SAFE BOOT MODE ACTIVE");
    // Safe mode에서는 WiFi를 무조건 AP로 타게 해야 하므로:
    // - 가장 단순: config에서 wifi.mode를 AP로 강제 저장하지 말고,
    //   W10의 setupWiFi_에서 safe mode면 AP 우선하도록 조건 추가를 권장.
    // (여기서는 E10/W10는 계속 시작)
  }

  g_e10.begin(&g_cfg);
  g_w10.begin(&g_cfg, CL_E10_EliteAirMouse::E10_W10Apply, (void*)&g_e10);

  Serial.println("[0273] started");

  // 4) 8초 생존 → boot ok 처리(벽돌 방지 핵심)
  // loop에서 uptime 체크해도 되는데, setup에서 간단히 딜레이 후 OK 처리
  delay(8500);
  (void)g_cfg.bootMarkOkIfGracePassed(8500);
}

void loop(){
  delay(1000);
}
