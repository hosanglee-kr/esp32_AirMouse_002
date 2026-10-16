// =======================================================
// File: examples/02_MaxGateLimit/02_MaxGateLimit.ino
// 모듈명: 벽 및 가구 반사 오탐 방지 (최대 감지 게이트 제한)
// =======================================================

/**
 * [예제 02] 02_MaxGateLimit
 * - 방의 크기에 맞춰 최대 감지 게이트를 제한하여, 벽 너머의 통행인이나
 *   가구 반사파로 인한 허위 감지(오탐)를 방지하는 예제입니다.
 * - LD2420의 1개 게이트는 약 0.7m를 담당합니다.
 *   예: 방 길이가 4m인 경우, 4m / 0.7m = 약 5.7 -> 게이트 6(4.2m)으로 제한
 * - requestMinMaxDistance()로 센서 하드웨어 레벨에서 범위를 제한하고,
 *   유효 범위를 벗어난 게이트(7~15)의 감도 임계값을 100으로 설정하여 완전 무시합니다.
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

// 방 크기 설정: 최소 게이트 1(0.7m) ~ 최대 게이트 6(4.2m)
const uint8_t MIN_GATE = 1;
const uint8_t MAX_GATE = 6;

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n========================================");
    Serial.println("  [02] LD2420 MaxGateLimit Example");
    Serial.println("========================================");

    radar.begin();

    // 1. 센서 감지 게이트 범위 설정 (게이트 1 ~ 6)
    radar.requestMinMaxDistance(MIN_GATE, MAX_GATE);
    Serial.printf("[설정] 감지 범위 지정: 게이트 %u ~ %u (%.1fm ~ %.1fm)\n",
                  MIN_GATE,
                  MAX_GATE,
                  MIN_GATE * LD2420_GATE_DISTANCE_M,
                  MAX_GATE * LD2420_GATE_DISTANCE_M);

    // 2. 게이트별 임계값 설정
    LD2420GateConfig cfg;
    cfg.setDefault(); // 게이트 0~5 기본값 적용

    // 방 크기 밖의 원거리 게이트(7~15번)는 임계값을 100(최대 둔감)으로 올려 반사파 차단
    for (int g = MAX_GATE + 1; g < LD2420_MAX_GATES; g++) {
        cfg.move_threshold[g]  = 100;
        cfg.still_threshold[g] = 100;
    }

    // 변경된 16개 게이트 임계값을 센서에 일괄 전송 (비동기 처리)
    radar.requestAllGateThresholds(cfg);

    Serial.println("[알림] 벽/가구 반사파 필터링 적용 완료.");
}

void loop() {
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint > 1000) {
        lastPrint = millis();

        if (radar.isPresent()) {
            Serial.printf("[감지] 방 내부 인체 감지됨 | 거리: %d cm\n", radar.getDistance());
        } else {
            Serial.println("[대기] 방 내부 비어있음");
        }
    }
}
