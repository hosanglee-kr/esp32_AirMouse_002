// =======================================================
// File: examples/07_AutoCalibration/07_AutoCalibration.ino
// 모듈명: 완전 비동기 자동 캘리브레이션 예제
// =======================================================

/**
 * [예제 07] 07_AutoCalibration
 * - 설치된 공간의 고유한 배경 노이즈(벽면, 가구, 전자기기 반사파)를 자동으로 측정하여
 *   최적의 감도 임계값을 산출하고 센서에 자동 적용하는 예제입니다.
 * - ESPHome의 ld2420 캘리브레이션 알고리즘과 동일하게 동작합니다:
 *   1. 센서를 실시간 에너지 출력 모드(ENERGY, 0x0004)로 자동 전환
 *   2. 5초마다 16개 게이트의 신호 강도를 샘플링 (총 64회 = 약 5.3분 소요)
 *   3. 수집 완료 시 평균값(Noise Floor)을 산출하여 최적 임계값 계산:
 *      - 움직임 감도(move) = clamp(노이즈 × 5배, 10, 100)
 *      - 정지 감도(still)  = clamp(노이즈 × 3배, 5, 80)
 *   4. 센서에 16개 게이트 설정을 일괄 기록하고 표준 감지 모드(SIMPLE)로 복귀
 *
 * [주의사항]
 * - 캘리브레이션 진행 중에는 **센서 감지 구역 내에 사람이 전혀 없어야 합니다.**
 * - 선풍기, 로봇청소기, 움직이는 커튼 등도 멈춘 상태여야 합니다.
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

// 캘리브레이션 완료 후 1회만 적용하기 위한 플래그
bool g_calApplied = false;

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n========================================");
    Serial.println("  [07] LD2420 AutoCalibration Example");
    Serial.println("========================================");

    if (!radar.begin()) {
        Serial.println("[오류] LD2420 센서 초기화 실패!");
        while (1) delay(1000);
    }

    // 센서 내부 이벤트 발생 시 알림을 받을 콜백 등록
    radar.onEvent([](const char* evt, void* ctx) {
        Serial.printf(">> [이벤트 발생] %s\n", evt);
    });

    Serial.println("\n[안내] 자동 캘리브레이션을 시작합니다.");
    Serial.println("       약 5분간 측정 공간을 비워주세요 (사람 및 움직임 금지)...");

    // 캘리브레이션 시작 (완전 논블로킹, 백그라운드 태스크에서 진행)
    radar.startCalibration();
}

void loop() {
    LD2420CalState calState = radar.calibrationState();

    // ----------------------------------------------------
    // 1. 샘플 수집 진행 중인 경우 (COLLECTING)
    // ----------------------------------------------------
    if (calState == LD2420CalState::COLLECTING) {
        static uint32_t lastPrint = 0;
        if (millis() - lastPrint > 5000) {
            lastPrint = millis();

            Serial.printf("[수집 중] 진행률: %3u%% | 게이트별 노이즈: ", radar.calibrationProgress());
            for (int g = 0; g < LD2420_MAX_GATES; g++) {
                Serial.printf("%d ", radar.calibrationNoiseFloor(g));
            }
            Serial.println();
        }
    }
    // ----------------------------------------------------
    // 2. 64개 샘플 수집 완료 및 임계값 계산 완료 (READY)
    // ----------------------------------------------------
    else if (calState == LD2420CalState::READY && !g_calApplied) {
        g_calApplied = true;
        Serial.println("\n========================================");
        Serial.println("  [완료] 노이즈 수집 완료! 계산된 최적값 센서 반영");
        Serial.println("========================================");

        // 연산된 임계값을 센서에 전송하고 SIMPLE 모드로 복귀
        if (radar.applyCalibration()) {
            Serial.println("[성공] 캘리브레이션 임계값 반영 완료. 정상 감지 모드로 전환되었습니다.");
        } else {
            Serial.println("[실패] 임계값 반영 중 오류 발생");
        }
    }
    // ----------------------------------------------------
    // 3. 정상 감지 실행 모드 (RUN)
    // ----------------------------------------------------
    else if (radar.isReady() && g_calApplied) {
        static uint32_t lastPrint = 0;
        if (millis() - lastPrint > 1000) {
            lastPrint = millis();

            if (radar.isPresent()) {
                Serial.printf("[정상 탐지] 인체 감지됨 | 거리: %4d cm\n", radar.getDistance());
            } else {
                Serial.println("[정상 탐지] 인체 미감지 (부재)");
            }
        }
    }

    // 메인 루프 주기
    delay(100);
}
