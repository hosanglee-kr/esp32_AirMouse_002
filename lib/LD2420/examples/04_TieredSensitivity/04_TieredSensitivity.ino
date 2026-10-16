// =======================================================
// File: examples/04_TieredSensitivity/04_TieredSensitivity.ino
// 모듈명: 거리별 차등 감도 설정 (근거리 고감도, 원거리 저감도)
// =======================================================

/**
 * [예제 04] 04_TieredSensitivity
 * - FMCW 레이더는 거리가 멀어질수록 반사 신호의 감쇠 및 주변 잡음(다중 경로 반사)의
 *   영향을 크게 받습니다.
 * - 본 예제는 거리에 따라 3단계로 감도를 차등 적용하는 기법을 시연합니다:
 *   1. 근거리(0~2.8m, 게이트 0~4) : 손가락 미세 움직임이나 호흡까지 감지하도록 초고감도(임계값 5) 설정
 *   2. 중거리(3.5~6.3m, 게이트 5~9): 통상적인 보행이나 움직임을 안정적으로 감지(임계값 move=20, still=15)
 *   3. 원거리(7.0~10.5m, 게이트 10~15): 창문 밖 차량, 문 너머 등 원거리 노이즈에 반응하지 않도록 둔감화(임계값 move=80, still=60)
 *
 * [배선 안내]
 * - LD2420 VCC  <-> ESP32 3.3V (5V 인가 금지)
 * - LD2420 GND  <-> ESP32 GND
 * - LD2420 TX   <-> ESP32 GPIO16 (RX2)
 * - LD2420 RX   <-> ESP32 GPIO17 (TX2)
 */

#include <LD2420.h>

HardwareSerial ldSerial(2);
LD2420         radar(ldSerial, 16, 17, 115200);

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n========================================");
    Serial.println("  [04] LD2420 TieredSensitivity Example");
    Serial.println("========================================");

    // 센서 및 FreeRTOS 백그라운드 태스크 초기화
    if (!radar.begin()) {
        Serial.println("[오류] LD2420 센서 초기화 실패!");
        while (1) delay(1000);
    }

    // 16개 게이트 임계값을 저장할 설정 구조체 준비
    LD2420GateConfig cfg;

    // ----------------------------------------------------
    // 1단계: 근거리 구역 (게이트 0~4, 0.0m ~ 2.8m)
    // 책상 앞 작업자나 의자 착석자의 미세 호흡까지 감지하도록 최저 임계값(5) 적용
    // ----------------------------------------------------
    for (int g = 0; g <= 4; g++) {
        cfg.move_threshold[g]  = 5; // 움직임 감지 임계값
        cfg.still_threshold[g] = 5; // 정지 미세 움직임(호흡 등) 감지 임계값
    }

    // ----------------------------------------------------
    // 2단계: 중거리 구역 (게이트 5~9, 3.5m ~ 6.3m)
    // 방 안을 걸어 다니는 일반적 활동 감지, 중간 수준 감도 적용
    // ----------------------------------------------------
    for (int g = 5; g <= 9; g++) {
        cfg.move_threshold[g]  = 20;
        cfg.still_threshold[g] = 15;
    }

    // ----------------------------------------------------
    // 3단계: 원거리 구역 (게이트 10~15, 7.0m ~ 10.5m)
    // 원거리 벽면 진동, 커튼 흔들림 등에 의한 오탐을 방지하기 위해 둔감하게 설정
    // ----------------------------------------------------
    for (int g = 10; g < LD2420_MAX_GATES; g++) {
        cfg.move_threshold[g]  = 80;
        cfg.still_threshold[g] = 60;
    }

    // 설정 구조체를 센서에 일괄 반영 요청 (비동기 처리)
    radar.requestAllGateThresholds(cfg);
    Serial.println("[설정] 3단계 거리별 차등 감도 임계값 적용 완료:");
    Serial.println("  - 0~2.8m  (Gate 0~4) : 초고감도 (move=5, still=5)");
    Serial.println("  - 3.5~6.3m(Gate 5~9) : 표준감도 (move=20, still=15)");
    Serial.println("  - 7.0~10.5m(Gate 10~15): 저감도  (move=80, still=60)");
}

void loop() {
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint > 1000) {
        lastPrint = millis();

        // 현재 인체 재실 여부 및 거리 출력
        if (radar.isPresent()) {
            Serial.printf("[감지] 인체 감지됨 | 거리: %3d cm\n", radar.getDistance());
        } else {
            Serial.println("[대기] 인체 미감지");
        }

        // 16개 게이트별 실시간 수신 신호 강도(Energy) 출력
        // (에너지 모드가 아니더라도 센서가 지원하는 최근 에너지 레벨 표시)
        Serial.print("  └ Gate 에너지(0..15): ");
        for (int g = 0; g < LD2420_MAX_GATES; g++) {
            Serial.printf("%d ", radar.getGateEnergy(g));
        }
        Serial.println();
    }
}
