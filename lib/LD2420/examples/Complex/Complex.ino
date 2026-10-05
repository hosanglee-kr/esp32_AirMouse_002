/**
 * 02_MaxGateLimit
 * - 벽/가구 반사로 인한 오탐 방지
 * - 최대 감지 게이트를 실제 방 크기로 제한
 */
#include <LD2420_001.h>

HardwareSerial ldSerial(2);
LD2420 radar(ldSerial, 16, 17, 115200);

// 방 길이: 약 4m → 게이트 6 (0.7m × 6 = 4.2m)까지
const uint8_t MIN_GATE = 1;
const uint8_t MAX_GATE = 6;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[02] MaxGateLimit");

  radar.begin();

  // 감지 범위 제한: 게이트 1 ~ 6
  radar.requestMinMaxDistance(MIN_GATE, MAX_GATE);
  Serial.printf("Range set: gate %u ~ %u (%.1fm ~ %.1fm)\n",
                MIN_GATE, MAX_GATE,
                MIN_GATE * 0.7f, MAX_GATE * 0.7f);

  // 나머지 게이트(7~15)는 임계값 100으로 완전 무시
  LD2420GateConfig cfg;
  cfg.setDefault();
  for (int g = MAX_GATE + 1; g < 16; g++) {
    cfg.move_threshold[g]  = 100;
    cfg.still_threshold[g] = 100;
  }
  radar.requestAllGateThresholds(cfg);

  Serial.println("Wall/furniture reflection filtering enabled.");
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last > 1000) {
    last = millis();
    Serial.printf("Presence: %s @ %d cm\n",
                  radar.isPresent() ? "O" : "X", radar.getDistance());
  }
}




///// *******
/**
 * 03_ZoneDetection
 * - 복도 특정 구역(2.8m ~ 5.6m)만 감지
 * - 센서 바로 앞 통행은 무시
 */
#include <LD2420_001.h>

HardwareSerial ldSerial(2);
LD2420 radar(ldSerial, 16, 17, 115200);

// 감지 구역: 게이트 4 (2.8m) ~ 게이트 8 (5.6m)
const uint8_t ZONE_MIN = 4;
const uint8_t ZONE_MAX = 8;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[03] ZoneDetection");

  radar.begin();

  // 감지 범위 지정
  radar.requestMinMaxDistance(ZONE_MIN, ZONE_MAX);
  Serial.printf("Zone: gate %u ~ %u (%.1fm ~ %.1fm)\n",
                ZONE_MIN, ZONE_MAX,
                ZONE_MIN * 0.7f, ZONE_MAX * 0.7f);

  // 구역 밖 게이트는 완전 무시
  LD2420GateConfig cfg;
  cfg.setDefault();
  for (int g = 0; g < ZONE_MIN; g++) {
    cfg.move_threshold[g]  = 100;
    cfg.still_threshold[g] = 100;
  }
  for (int g = ZONE_MAX + 1; g < 16; g++) {
    cfg.move_threshold[g]  = 100;
    cfg.still_threshold[g] = 100;
  }
  radar.requestAllGateThresholds(cfg);
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last > 500) {
    last = millis();
    if (radar.isPresent()) {
      uint16_t d = radar.getDistance();
      // 감지 구역 내에 있는지 확인
      if (d >= ZONE_MIN * 70 && d <= ZONE_MAX * 70) {
        Serial.printf("Zone hit @ %d cm\n", d);
      }
    }
  }
}

///// *******
/**
 * 04_TieredSensitivity
 * - 가까운 거리: 민감 (낮은 임계값)
 * - 먼 거리: 둔감 (높은 임계값)
 * - 게이트별 개별 임계값 설정
 */
#include <LD2420_001.h>

HardwareSerial ldSerial(2);
LD2420 radar(ldSerial, 16, 17, 115200);

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[04] TieredSensitivity");

  radar.begin();

  LD2420GateConfig cfg;

  // 게이트 0~4 (0 ~ 2.8m): 민감
  for (int g = 0; g <= 4; g++) {
    cfg.move_threshold[g]  = 5;
    cfg.still_threshold[g] = 5;
  }

  // 게이트 5~9 (3.5m ~ 6.3m): 중간 감도
  for (int g = 5; g <= 9; g++) {
    cfg.move_threshold[g]  = 20;
    cfg.still_threshold[g] = 15;
  }

  // 게이트 10~15 (7m ~ 10.5m): 둔감
  for (int g = 10; g < 16; g++) {
    cfg.move_threshold[g]  = 80;
    cfg.still_threshold[g] = 60;
  }

  radar.requestAllGateThresholds(cfg);
  Serial.println("Tiered thresholds applied (near=sensitive, far=desensitized).");
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last > 1000) {
    last = millis();
    Serial.print("Gates: ");
    for (int g = 0; g < 16; g++) Serial.printf("%d ", radar.getGateEnergy(g));
    Serial.println();
  }
}

///// *******
/**
 * 05_BathroomLightFan
 * - 진입 감지: 게이트 1~3 (0.7~2.1m) 민감
 * - 재실 유지: 게이트 4~6 (2.8~4.2m) 정지 감지
 * - 문 너머(게이트 7~15): 완전 무시
 * - presence_timeout 30초
 */
#include <LD2420_001.h>

HardwareSerial ldSerial(2);
LD2420 radar(ldSerial, 16, 17, 115200);

#define RELAY_LIGHT 25
#define RELAY_FAN   26

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[05] BathroomLightFan");

  pinMode(RELAY_LIGHT, OUTPUT);
  pinMode(RELAY_FAN, OUTPUT);
  digitalWrite(RELAY_LIGHT, LOW);
  digitalWrite(RELAY_FAN, LOW);

  radar.begin();

  LD2420GateConfig cfg = {};

  // 진입 감지 (게이트 1~3): 움직임에 민감
  cfg.move_threshold[1] = 5;  cfg.still_threshold[1] = 5;
  cfg.move_threshold[2] = 5;  cfg.still_threshold[2] = 5;
  cfg.move_threshold[3] = 8;  cfg.still_threshold[3] = 8;

  // 재실 유지 (게이트 4~6): 미세 움직임 감지
  cfg.move_threshold[4] = 15; cfg.still_threshold[4] = 10;
  cfg.move_threshold[5] = 20; cfg.still_threshold[5] = 15;
  cfg.move_threshold[6] = 30; cfg.still_threshold[6] = 20;

  // 게이트 7~15: 문 너머 오탐 방지
  for (int g = 7; g < 16; g++) {
    cfg.move_threshold[g]  = 100;
    cfg.still_threshold[g] = 100;
  }

  radar.requestAllGateThresholds(cfg);
  radar.requestMinMaxDistance(1, 6);
  radar.requestTimeout(30);   // 30초 유지

  Serial.println("Bathroom mode ready. Timeout: 30s");
}

void loop() {
  // 재실 여부에 따라 조명/환풍기 제어
  bool occupied = radar.isPresent();

  digitalWrite(RELAY_LIGHT, occupied ? HIGH : LOW);
  digitalWrite(RELAY_FAN,   occupied ? HIGH : LOW);

  static uint32_t last = 0;
  if (millis() - last > 2000) {
    last = millis();
    Serial.printf("Occupied=%d dist=%dcm light=%d fan=%d\n",
                  occupied, radar.getDistance(),
                  digitalRead(RELAY_LIGHT), digitalRead(RELAY_FAN));
  }

  delay(100);
}

///// *******

/**
 * 06_CeilingFanFilter
 * - 천장 선풍기의 움직임을 게이트 5~7에서 완전 무시
 * - 사람이 있는 게이트 1~4는 민감하게 유지
 */
#include <LD2420_001.h>

HardwareSerial ldSerial(2);
LD2420 radar(ldSerial, 16, 17, 115200);

// 천장 선풍기가 위치한 게이트 (실측 후 조정)
const uint8_t FAN_GATE_START = 5;
const uint8_t FAN_GATE_END   = 7;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[06] CeilingFanFilter");

  radar.begin();

  LD2420GateConfig cfg;
  cfg.setDefault();

  // 사람 감지 구역 (게이트 1~4): 민감
  cfg.move_threshold[1] = 8;  cfg.still_threshold[1] = 6;
  cfg.move_threshold[2] = 8;  cfg.still_threshold[2] = 6;
  cfg.move_threshold[3] = 10; cfg.still_threshold[3] = 8;
  cfg.move_threshold[4] = 15; cfg.still_threshold[4] = 10;

  // 선풍기 위치 게이트: 완전 무시
  for (int g = FAN_GATE_START; g <= FAN_GATE_END; g++) {
    cfg.move_threshold[g]  = 100;
    cfg.still_threshold[g] = 100;
  }

  radar.requestAllGateThresholds(cfg);

  Serial.printf("Ceiling fan filter: gate %u~%u muted.\n",
                FAN_GATE_START, FAN_GATE_END);
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last > 1000) {
    last = millis();
    Serial.printf("Presence=%d  FanGates[%u..%u] energy: ",
                  radar.isPresent(), FAN_GATE_START, FAN_GATE_END);
    for (int g = FAN_GATE_START; g <= FAN_GATE_END; g++) {
      Serial.printf("%d ", radar.getGateEnergy(g));
    }
    Serial.println();
  }
}

///// *******
/**
 * 07_AutoCalibration
 * - startCalibration() → 30초(64샘플×5초) 수집
 * - applyCalibration() → 노이즈 플로어 × 5(트리거), × 3(유지)
 * - 모든 과정 비동기 (loop 블로킹 없음)
 */
#include <LD2420_001.h>

HardwareSerial ldSerial(2);
LD2420 radar(ldSerial, 16, 17, 115200);

bool applied = false;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[07] AutoCalibration");
  Serial.println("Keep area empty during calibration...");

  radar.begin();

  // 이벤트 콜백으로 캘리브레이션 상태 수신
  radar.onEvent([](const char *evt, void *ctx) {
    Serial.printf("[EVENT] %s\n", evt);
  });

  // 캘리브레이션 시작 (즉시 반환)
  radar.startCalibration();
}

void loop() {
  if (radar.calibrationState() == LD2420CalState::COLLECTING) {
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint > 5000) {
      lastPrint = millis();
      Serial.printf("Cal progress: %u%%\n", radar.calibrationProgress());

      Serial.print("Noise floor: ");
      for (int g = 0; g < 16; g++) {
        Serial.printf("%d ", radar.calibrationNoiseFloor(g));
      }
      Serial.println();
    }
  }
  else if (radar.calibrationState() == LD2420CalState::READY && !applied) {
    applied = true;
    Serial.println("\n=== Calibration ready ===");
    radar.applyCalibration();
    Serial.println("Applied. Normal mode resumed.");
  }

  // 일반 감지 상태
  if (radar.isReady()) {
    static uint32_t last = 0;
    if (millis() - last > 1000) {
      last = millis();
      Serial.printf("Presence=%d dist=%dcm\n",
                    radar.isPresent(), radar.getDistance());
    }
  }

  delay(100);
}

///// *******
/**
 * 08_ManualCalibration
 * - 시리얼 명령으로 게이트별 임계값 실시간 조정
 * - g<gate> <move> <still>  예: g3 5 3
 * - r                       감지 범위 (min max)
 * - t <sec>                 타임아웃
 * - p                       현재 설정 출력
 * - c                       자동 캘리브레이션 시작
 */
#include <LD2420_001.h>

HardwareSerial ldSerial(2);
LD2420 radar(ldSerial, 16, 17, 115200);

LD2420GateConfig g_cfg;

void printHelp() {
  Serial.println("\n--- Commands ---");
  Serial.println(" g<gate> <move> <still>   set gate threshold");
  Serial.println(" r <min> <max>            set min/max gate");
  Serial.println(" t <seconds>              set presence timeout");
  Serial.println(" p                        print current config");
  Serial.println(" c                        start auto calibration");
  Serial.println(" h                        help");
}

void printConfig() {
  Serial.println("\n--- Current Gate Config ---");
  for (int g = 0; g < 16; g++) {
    Serial.printf("  Gate %2d (%.1fm): move=%3u still=%3u\n",
                  g, g * 0.7f,
                  g_cfg.move_threshold[g], g_cfg.still_threshold[g]);
  }
}

void handleCommand(String line) {
  line.trim();
  if (line.length() == 0) return;

  if (line == "h") { printHelp(); return; }
  if (line == "p") { printConfig(); return; }
  if (line == "c") {
    Serial.println("Start calibration (keep area empty)...");
    radar.startCalibration();
    return;
  }

  if (line.startsWith("g")) {
    int g, m, s;
    if (sscanf(line.c_str(), "g%d %d %d", &g, &m, &s) == 3) {
      if (g < 0 || g > 15) { Serial.println("gate must be 0~15"); return; }
      radar.requestGateThreshold((uint8_t)g, (uint16_t)m, (uint16_t)s);
      g_cfg.move_threshold[g] = m;
      g_cfg.still_threshold[g] = s;
      Serial.printf("Gate %d: move=%d still=%d\n", g, m, s);
    }
    return;
  }

  if (line.startsWith("r")) {
    int mn, mx;
    if (sscanf(line.c_str(), "r %d %d", &mn, &mx) == 2) {
      radar.requestMinMaxDistance((uint8_t)mn, (uint8_t)mx);
      Serial.printf("Range: gate %d ~ %d\n", mn, mx);
    }
    return;
  }

  if (line.startsWith("t")) {
    int sec;
    if (sscanf(line.c_str(), "t %d", &sec) == 1) {
      radar.requestTimeout((uint16_t)sec);
      Serial.printf("Timeout: %d sec\n", sec);
    }
    return;
  }

  Serial.println("Unknown command. Type 'h' for help.");
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[08] ManualCalibration");

  // 기본값 로드
  g_cfg.setDefault();

  radar.begin();
  radar.requestAllGateThresholds(g_cfg);
  radar.requestTimeout(30);

  printHelp();
}

void loop() {
  // 시리얼 명령 처리
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    handleCommand(line);
  }

  // 상태 표시 (1초마다)
  static uint32_t last = 0;
  if (millis() - last > 1000) {
    last = millis();
    if (radar.isReady()) {
      Serial.printf("Presence=%d dist=%dcm cal=%u%%\n",
                    radar.isPresent(), radar.getDistance(),
                    radar.calibrationProgress());
    }
  }
}

///// *******

/**
 * 09_FreeRTOS_MultiTask
 * - rxTask(코어0): UART 수신 + 파싱
 * - cmdTask(코어0): 명령 큐 처리 + ACK 재시도
 * - telemetryTask(코어1): 1초마다 상태 출력
 * - controlTask(코어1): 감지 기반 제어 로직
 */
#include <LD2420_001.h>

HardwareSerial ldSerial(2);
LD2420 radar(ldSerial, 16, 17, 115200);

volatile uint32_t g_eventCount = 0;
volatile uint32_t g_dataCount  = 0;

// rxTask에서 직접 호출됨 → 가벼운 처리만
void onData(const LD2420TargetData &d, void *ctx) {
  g_dataCount++;
}

void onEvent(const char *evt, void *ctx) {
  g_eventCount++;
  Serial.printf("[EVENT] %s\n", evt);
}

// 코어 1: 텔레메트리
void telemetryTask(void *arg) {
  while (true) {
    LD2420TargetData d = radar.getLatestData();
    Serial.printf("[TEL] seq=%u pres=%d dist=%dcm data=%u evt=%u\n",
                  d.sequence, d.presence, d.distance_cm,
                  g_dataCount, g_eventCount);
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

// 코어 1: 제어 로직
void controlTask(void *arg) {
  pinMode(2, OUTPUT);
  while (true) {
    bool occ = radar.isPresent();
    digitalWrite(2, occ ? HIGH : LOW);
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[09] FreeRTOS_MultiTask");

  radar.onData(onData);
  radar.onEvent(onEvent);

  if (!radar.begin()) {
    Serial.println("LD2420 init failed");
    while (1) delay(1000);
  }

  // 사용자 태스크 2개 (코어 1에서 loopTask와 함께 실행)
  xTaskCreatePinnedToCore(telemetryTask, "tele", 4096, nullptr, 2, nullptr, 1);
  xTaskCreatePinnedToCore(controlTask,   "ctrl", 4096, nullptr, 3, nullptr, 1);

  // 기본 감도 적용 (비동기)
  radar.requestDefaultThresholds();
}

void loop() {
  // loopTask는 자유롭게 다른 작업 가능
  // 예: 10초마다 감도 재조정
  static uint32_t last = 0;
  if (millis() - last > 10000) {
    last = millis();
    radar.requestGateThreshold(3, 8, 6);
  }
  delay(1000);
}

///// *******

///// *******

///// *******

///// *******

///// *******

///// *******

///// *******


