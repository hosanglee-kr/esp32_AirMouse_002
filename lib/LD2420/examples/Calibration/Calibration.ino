// =======================================================
// File: examples/Calibration.ino
// Desc : 
// =======================================================



#include <LD2420_001.h>

HardwareSerial ldSerial(2);
LD2420 radar(ldSerial, 16, 17, 115200);

bool calDone = false;

void setup() {
  Serial.begin(115200);
  radar.begin();

  Serial.println("Calibration start - keep area empty for ~5 min");
  radar.startCalibration();   // 즉시 반환
}

void loop() {
  if (radar.calibrationState() == LD2420CalState::COLLECTING) {
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint > 3000) {
      lastPrint = millis();
      Serial.printf("Cal progress: %u%%\n", radar.calibrationProgress());
    }
  }
  else if (radar.calibrationState() == LD2420CalState::READY && !calDone) {
    calDone = true;
    Serial.println("Applying calibration...");
    radar.applyCalibration();   // 비동기로 게이트 설정 명령 큐잉
    Serial.println("Calibration applied. Back to simple mode.");
  }

  delay(200);
}
