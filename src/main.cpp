// ======================================================
// File: src/main.cpp (정리 버전: W10_Apply 함수 없음)
// ======================================================
#include <Arduino.h>

#include "v010/C10_Config_015.h"
#include "v010/E10_EliteAirMouse_015.h"
#include "v010/W10_WebConfig_015.h"

CL_C10_Config        g_c10;
CL_E10_EliteAirMouse g_e10;
CL_W10_WebConfig     g_w10;

void setup() {
  g_c10.begin(true);
  g_e10.begin(&g_c10);

  // ✅ main에 W10_Apply 필요 없음
  g_w10.begin(&g_c10, CL_E10_EliteAirMouse::E10_W10Apply, &g_e10);
}

void loop() {
  vTaskDelete(NULL);
}