// =======================================================
// File: examples/03_ZoneDetection/03_ZoneDetection.ino
// 모듈명: 특정 구간(Zone) 국한 감지 예제
// =======================================================

/**
 * [예제 03] 03_ZoneDetection
 * - 복도의 특정 구간(예: 2.8m ~ 5.6m)만 선별 감지하는 구역(Zone) 필터링 예제입니다.
 * - 센서 바로 앞(근거리)을 지나가는 통행인이나 원거리 배경 움직임은 무시하고,
 *   지정된 중간 영역(게이트 4 ~ 8)에 진입한 대상만 정밀하게 감지합니다.
 *
 * [게이트 거리 매핑 (0.7m 단위)]
 * - 게이트 0~3: 0.0m ~ 2.8m (무시 구역: 임계값 100)
 * - 게이트 4~8: 2.8m ~ 5.6m (목표 감지 구역: 기본 감도)
 * - 게이트 9~15: 5.6m ~ 11.2m (원거리 무시 구역: 임계값 100)
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

// 감지할 특정 구역 정의: 게이트 4(2.8m) ~ 게이트 8(5.6m)
const uint8_t ZONE_MIN = 4;
const uint8_t ZONE_MAX = 8;

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n========================================");
    Serial.println("  [03] LD2420 ZoneDetection Example");
    Serial.println("========================================");

    radar.begin();

    // 1. 센서 감지 게이트 범위를 목표 구역으로 제한
    radar.requestMinMaxDistance(ZONE_MIN, ZONE_MAX);
    Serial.printf("[설정] 목표 구역: 게이트 %u ~ %u (%.1fm ~ %.1fm)\n",
                  ZONE_MIN,
                  ZONE_MAX,
                  ZONE_MIN * LD2420_GATE_DISTANCE_M,
                  ZONE_MAX * LD2420_GATE_DISTANCE_M);

    // 2. 구역 밖의 게이트는 임계값을 100으로 올려 확실하게 차단
    LD2420GateConfig cfg;
    cfg.setDefault();

    // 목표 구역 이전(근거리 0 ~ 3번 게이트) 차단
    for (int g = 0; g < ZONE_MIN; g++) {
        cfg.move_threshold[g]  = 100;
        cfg.still_threshold[g] = 100;
    }
    // 목표 구역 이후(원거리 9 ~ 15번 게이트) 차단
    for (int g = ZONE_MAX + 1; g < LD2420_MAX_GATES; g++) {
        cfg.move_threshold[g]  = 100;
        cfg.still_threshold[g] = 100;
    }

    radar.requestAllGateThresholds(cfg);
    Serial.println("[알림] 구역 외 게이트 마스킹 완료.");
}

void loop() {
    static uint32_t lastCheck = 0;
    if (millis() - lastCheck > 500) {
        lastCheck = millis();

        if (radar.isPresent()) {
            uint16_t dist_cm = radar.getDistance();

            // 감지된 거리가 목표 구역 내(280cm ~ 560cm)인지 소프트웨어적으로 2차 검증
            if (dist_cm >= (ZONE_MIN * 70) && dist_cm <= (ZONE_MAX * 70)) {
                Serial.printf("[타겟 진입] 목표 구역(Zone) 내 타겟 포착! | 거리: %d cm\n", dist_cm);
            } else {
                Serial.printf("[경계 신호] 구역 경계 반사 감지 (거리: %d cm)\n", dist_cm);
            }
        }
    }
}
