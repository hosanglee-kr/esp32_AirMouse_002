// =======================================================
// File: examples/01.BasicPresence.ino
// Desc : 
// =======================================================


/**
 * 01_BasicPresence
 * - 라이브러리 최소 사용법
 * - loop()는 논블로킹. 최신값만 조회
 */
 
#include <LD2420_001.h>

HardwareSerial ldSerial(2);
LD2420 radar(ldSerial, 16, 17, 115200);

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[01] BasicPresence");

  // FreeRTOS 태스크 생성 후 즉시 반환
  if (!radar.begin()) {
    Serial.println("LD2420 init failed (check wiring / 3.3V)");
    while (1) delay(1000);
  }
}

void loop() {
  // 절대 블로킹하지 않음. 최신값만 조회
  if (radar.isPresent()) {
    Serial.printf("Present @ %d cm\n", radar.getDistance());
  } else {
    Serial.println("No presence");
  }
  delay(500);
}
