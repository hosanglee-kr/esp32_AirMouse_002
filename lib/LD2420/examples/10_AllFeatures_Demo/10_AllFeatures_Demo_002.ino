
/**
 * 10_AllFeatures_Demo
 * - 시리얼 메뉴로 모든 기능 시연
 * - 자동/수동 캘리브레이션, 프리셋, 게이트 설정, 범위, 타임아웃
 */
 
#include <LD2420_001.h>

HardwareSerial ldSerial(2);
LD2420 radar(ldSerial, 16, 17, 115200);

void printMenu() {
  Serial.println("\n===== LD2420 Full Feature Demo =====");
  Serial.println(" 1  Print current status");
  Serial.println(" 2  Apply default thresholds");
  Serial.println(" 3  Apply sensitive thresholds");
  Serial.println(" 4  Apply balanced thresholds");
  Serial.println(" 5  Set range (min max)");
  Serial.println(" 6  Set gate threshold (g m s)");
  Serial.println(" 7  Set timeout (sec)");
  Serial.println(" 8  Start auto calibration");
  Serial.println(" 9  Apply calibration");
  Serial.println(" 0  Cancel calibration");
  Serial.println(" m  Show this menu");
  Serial.println("====================================\n");
}

void printStatus() {
  Serial.println("\n--- Status ---");
  Serial.printf("Ready=%d Presence=%d Dist=%dcm\n",
                radar.isReady(), radar.isPresent(), radar.getDistance());
  Serial.printf("Cal state=%d Progress=%u%%\n",
                (int)radar.calibrationState(), radar.calibrationProgress());
  Serial.print("Gate energy: ");
  for (int g = 0; g < 16; g++) Serial.printf("%d ", radar.getGateEnergy(g));
  Serial.println();
  Serial.print("Noise floor: ");
  for (int g = 0; g < 16; g++) Serial.printf("%d ", radar.calibrationNoiseFloor(g));
  Serial.println();
}

void handleMenu(String line) {
  line.trim();
  if (line.length() == 0) return;

  if (line == "m") { printMenu(); return; }
  if (line == "1") { printStatus(); return; }

  if (line == "2") { radar.requestDefaultThresholds();  Serial.println("Default applied."); return; }
  if (line == "3") { radar.requestSensitiveThresholds();Serial.println("Sensitive applied."); return; }
  if (line == "4") { radar.requestBalancedThresholds(); Serial.println("Balanced applied."); return; }

  if (line == "8") {
    Serial.println("Start auto calibration (empty area!)...");
    radar.startCalibration();
    return;
  }
  if (line == "9") {
    if (radar.applyCalibration()) Serial.println("Calibration applied.");
    else Serial.println("Calibration not ready.");
    return;
  }
  if (line == "0") { radar.cancelCalibration(); Serial.println("Cancelled."); return; }

  if (line.startsWith("5")) {
    int mn, mx;
    if (sscanf(line.c_str(), "5 %d %d", &mn, &mx) == 2) {
      radar.requestMinMaxDistance((uint8_t)mn, (uint8_t)mx);
      Serial.printf("Range set: %d~%d\n", mn, mx);
    } else Serial.println("Usage: 5 <min> <max>");
    return;
  }
  if (line.startsWith("6")) {
    int g, m, s;
    if (sscanf(line.c_str(), "6 %d %d %d", &g, &m, &s) == 3) {
      radar.requestGateThreshold((uint8_t)g, (uint16_t)m, (uint16_t)s);
      Serial.printf("Gate %d: move=%d still=%d\n", g, m, s);
    } else Serial.println("Usage: 6 <gate> <move> <still>");
    return;
  }
  if (line.startsWith("7")) {
    int sec;
    if (sscanf(line.c_str(), "7 %d", &sec) == 1) {
      radar.requestTimeout((uint16_t)sec);
      Serial.printf("Timeout: %d\n", sec);
    } else Serial.println("Usage: 7 <sec>");
    return;
  }

  Serial.println("Unknown command. 'm' for menu.");
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[10] AllFeatures_Demo");

  radar.onEvent([](const char *evt, void *ctx) {
    Serial.printf("[EVENT] %s\n", evt);
  });

  if (!radar.begin()) {
    Serial.println("LD2420 init failed");
    while (1) delay(1000);
  }

  printMenu();
}

void loop() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    handleMenu(line);
  }

  static uint32_t last = 0;
  if (millis() - last > 3000) {
    last = millis();
    if (radar.isReady()) {
      Serial.printf("[%lus] P=%d D=%dcm Cal=%u%%\n",
                    millis() / 1000,
                    radar.isPresent(), radar.getDistance(),
                    radar.calibrationProgress());
    }
  }
}
