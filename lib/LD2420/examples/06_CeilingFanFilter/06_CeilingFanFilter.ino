// =======================================================
// File: examples/06_CeilingFanFilter/06_CeilingFanFilter.ino
// 모듈명: 천장 선풍기/에어컨 바람 오탐 필터링 예제
// =======================================================

/**
 * [예제 06] 06_CeilingFanFilter
 * - 천장에 설치된 실링팬(Ceiling Fan), 탁상용 선풍기 날개의 회전, 또는 에어컨 바람에 흔들리는
 *   커튼은 FMCW 레이더에서 가장 흔하게 발생하는 오탐(False Positive)의 원인입니다.
 * - 본 예제는 선풍기가 위치한 특정 거리 게이트 구간만 선택적으로 둔감화(마스킹)하여,
 *   선풍기가 계속 돌아가도 오탐이 발생하지 않으면서 사람이 있는 다른 게이트는 정상 감지하는 방법을 보여줍니다.
 *
 * [게이트 분할]
 * - 게이트 1~4 (0.7m ~ 2.8m): 사람이 주로 머무는 구역 -> 민감도 유지 (move=8~15, still=6~10)
 * - 게이트 5~7 (3.5m ~ 4.9m): 천장 선풍기가 설치된 구역 -> 임계값 100으로 완전 마스킹
 * - 게이트 8~15 (5.6m ~ 11.2m): 원거리 외곽 구역
 *
 * [배선 안내]
 * - LD2420 VCC  <-> ESP32 3.3V
 * - LD2420 GND  <-> ESP32 GND
 * - LD2420 TX   <-> ESP32 GPIO16 (RX2)
 * - LD2420 RX   <-> ESP32 GPIO17 (TX2)
 */

#include <LD2420.h>

HardwareSerial ldSerial(2);
LD2420         radar(ldSerial, 16, 17, 115200);

// 천장 선풍기가 위치한 게이트 범위 (설치 환경에 맞게 실측 후 조정)
const uint8_t FAN_GATE_START = 5; // 약 3.5m
const uint8_t FAN_GATE_END   = 7; // 약 4.9m

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n========================================");
    Serial.println("  [06] LD2420 CeilingFanFilter Example");
    Serial.println("========================================");

    if (!radar.begin()) {
        Serial.println("[오류] LD2420 센서 초기화 실패!");
        while (1) delay(1000);
    }

    // 기본 설정 로드
    LD2420GateConfig cfg;
    cfg.setDefault();

    // 1. 사람이 주로 생활하는 근거리 구역 (게이트 1~4) 감도 최적화
    cfg.move_threshold[1]  = 8;
    cfg.still_threshold[1] = 6;
    cfg.move_threshold[2]  = 8;
    cfg.still_threshold[2] = 6;
    cfg.move_threshold[3]  = 10;
    cfg.still_threshold[3] = 8;
    cfg.move_threshold[4]  = 15;
    cfg.still_threshold[4] = 10;

    // 2. 선풍기가 위치한 게이트 구간(5~7번)의 임계값을 100(최대치)으로 설정하여 무시
    for (int g = FAN_GATE_START; g <= FAN_GATE_END; g++) {
        cfg.move_threshold[g]  = 100;
        cfg.still_threshold[g] = 100;
    }

    // 센서에 새 임계값 전달
    radar.requestAllGateThresholds(cfg);

    Serial.printf("[설정 완료] 선풍기 구역(게이트 %u~%u, %.1fm~%.1fm) 마스킹 필터 활성화\n",
                  FAN_GATE_START,
                  FAN_GATE_END,
                  FAN_GATE_START * LD2420_GATE_DISTANCE_M,
                  FAN_GATE_END * LD2420_GATE_DISTANCE_M);
}

void loop() {
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint > 1000) {
        lastPrint = millis();

        // 인체 감지 상태
        Serial.printf("[상태] 인체 감지 여부: %s (거리: %d cm) | 선풍기 게이트 에너지: [ ",
                      radar.isPresent() ? "감지됨(O)" : "미감지(X)",
                      radar.getDistance());

        // 선풍기가 위치한 게이트(5~7)의 실시간 신호 강도를 모니터링하여 임계값 튜닝 지원
        for (int g = FAN_GATE_START; g <= FAN_GATE_END; g++) {
            Serial.printf("G%u:%-3d ", g, radar.getGateEnergy(g));
        }
        Serial.println("]");
    }
}
