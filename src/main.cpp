// ======================================================
// File: src/main.cpp  (예시: C10 1개 공유 + E10/W10 연결)
// ======================================================
#include <Arduino.h>

#include "v010/C10_Config_010.h"
#include "v010/E10_EliteAirMouse_012.h"
#include "v010/W10_WebConfig_010.h"

CL_C10_Config        g_c10;
CL_E10_EliteAirMouse g_e10;
CL_W10_WebConfig     g_w10;

// W10 저장 후 즉시 적용 콜백
static bool W10_Apply(void* p_ctx) {
  CL_E10_EliteAirMouse* v_e = (CL_E10_EliteAirMouse*)p_ctx;
  return v_e->reloadConfig();
}

void setup() {
  // 1) FS/Config 준비(1회)
  g_c10.begin(true);

  // 2) E10 시작(주입)
  g_e10.begin(&g_c10);

  // 3) W10 시작(같은 C10 공유 + 저장 후 reloadConfig)
  g_w10.begin(&g_c10, W10_Apply, &g_e10);
}

void loop() {
  vTaskDelete(NULL);
}



/*
// main.cpp

#include <Arduino.h>
#include "v010/E10_EliteAirMouse_011.h"
#include "v001/W10_WebConfig_010.h"


CL_E10_EliteAirMouse g_E10_airMouse;
CL_W10_WebConfig     g_w10;



void setup() {
    Serial.begin(115200);

    g_E10_airMouse.begin();
}

void loop() {
    vTaskDelete(NULL);
}
*/