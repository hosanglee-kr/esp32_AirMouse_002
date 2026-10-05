// =======================================================
// File: examples/FreeRTOS_MultiTask.ino
// Desc : 
// =======================================================



#include <LD2420_001.h>

HardwareSerial ldSerial(2);
LD2420 radar(ldSerial, 16, 17, 115200);

volatile uint32_t g_presenceCount = 0;

// rxTask에서 직접 호출됨 (코어 0) → 가벼운 처리만
void onData(const LD2420TargetData &d, void *ctx) {
  if (d.presence) g_presenceCount++;
}

void onEvent(const char *evt, void *ctx) {
  Serial.printf("[EVENT] %s\n", evt);
}

void telemetryTask(void *arg) {
  while (true) {
    LD2420TargetData d = radar.getLatestData();  // 뮤텍스 보호
    Serial.printf("[TEL] seq=%u pres=%d dist=%d energy0=%d\n",
                  d.sequence, d.presence, d.distance_cm, d.gate_energy[0]);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void setup() {
  Serial.begin(115200);
  radar.onData(onData);
  radar.onEvent(onEvent);
  radar.begin();

  // 별도 텔레메트리 태스크 (코어 1, loopTask와 함께)
  xTaskCreatePinnedToCore(telemetryTask, "tele", 4096, nullptr, 2, nullptr, 1);
}

void loop() {
  // 설정 변경은 언제든 비동기 요청
  static uint32_t last = 0;
  if (millis() - last > 10000) {
    last = millis();
    // 예: 10초마다 감도 재조정
    radar.requestGateThreshold(3, 8, 6);
  }
}

