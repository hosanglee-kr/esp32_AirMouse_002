// main.cpp

#include <Arduino.h>
#include "v010/E10_EliteAirMouse_010.h"


E10_::CL_E10_EliteAirMouse g_E10_airMouse;

void setup() {
    Serial.begin(115200);
    g_E10_airMouse.begin();
}

void loop() {
    vTaskDelete(NULL);
}