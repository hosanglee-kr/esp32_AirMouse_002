// ======================================================
// File: src/main.cpp (정리 버전: W10_Apply 함수 없음)
// ======================================================
#include <Arduino.h>


#include "v010/A40_ComFunc_070.h"   // 내부에서 D10_Logger_060.h 포함

#include "v010/C10_Config_0272.h"
#include "v010/E10_EliteAirMouse_0272.h"
#include "v010/W10_WebConfig_0272.h"

CL_C10_Config        g_c10;
CL_E10_EliteAirMouse g_e10;
CL_W10_WebConfig     g_w10;

void setup() {
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
  
  
  g_c10.begin(true);
  g_e10.begin(&g_c10);

  // ✅ main에 W10_Apply 필요 없음
  g_w10.begin(&g_c10, CL_E10_EliteAirMouse::E10_W10Apply, &g_e10);
}

void loop() {
  vTaskDelete(NULL);
}