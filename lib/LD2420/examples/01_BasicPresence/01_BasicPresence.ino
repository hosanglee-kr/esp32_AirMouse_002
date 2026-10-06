// =======================================================
// File: examples/01_BasicPresence/01_BasicPresence.ino
// 모듈명: LD2420 최소 사용법 기본 예제
// =======================================================

/**
 * [예제 01] 01_BasicPresence
 * - HLK-LD2420 레이더 센서의 가장 기본적인 인체 감지 예제입니다.
 * - FreeRTOS 듀얼 태스크가 백그라운드(코어 0)에서 동작하므로,
 *   loop()에서는 센서 응답을 기다리는 블로킹(delay 등) 없이
 *   radar.isPresent() 및 radar.getDistance()를 즉시 조회할 수 있습니다.
 *
 * [배선 안내]
 * - LD2420 VCC  <-> ESP32 3.3V (주의: 5V 입력 시 센서 파손 위험)
 * - LD2420 GND  <-> ESP32 GND
 * - LD2420 TX   <-> ESP32 GPIO16 (RX2)
 * - LD2420 RX   <-> ESP32 GPIO17 (TX2)
 */

#include <LD2420.h>

// ESP32 하드웨어 시리얼 2번(UART2) 사용
HardwareSerial ldSerial(2);

// LD2420 인스턴스 생성 (시리얼객체, RX핀, TX핀, 통신속도 115200)
LD2420 radar(ldSerial, 16, 17, 115200);

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n========================================");
    Serial.println("  [01] LD2420 BasicPresence Example");
    Serial.println("========================================");

    // FreeRTOS 백그라운드 태스크(rxTask, cmdTask) 시작
    // 내부적으로 코어 0에 태스크가 생성되며 즉시 반환됩니다.
    if (!radar.begin()) {
        Serial.println("[오류] LD2420 초기화 실패! (배선 및 3.3V 전원 확인)");
        while (1) delay(1000);
    }

    Serial.println("[알림] 센서 부팅 및 감지 대기 중...");
}

void loop() {
    // 논블로킹 조회: 내부적으로 최신 데이터가 즉시 반환됩니다.
    if (radar.isPresent()) {
        Serial.printf("[감지] 인체 감지됨 | 거리: %4d cm (약 %.2f m)\n",
                      radar.getDistance(),
                      radar.getDistance() / 100.0f);
    } else {
        Serial.println("[대기] 인체 미감지 (부재 상태)");
    }

    // 메인 루프 폴링 주기 (500ms)
    delay(500);
}
