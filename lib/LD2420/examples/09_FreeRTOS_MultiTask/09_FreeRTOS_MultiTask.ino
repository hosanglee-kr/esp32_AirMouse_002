// =======================================================
// File: examples/09_FreeRTOS_MultiTask/09_FreeRTOS_MultiTask.ino
// 모듈명: FreeRTOS 듀얼코어 멀티태스크 아키텍처 연동 예제
// =======================================================

/**
 * [예제 09] 09_FreeRTOS_MultiTask
 * - ESP32의 듀얼코어(Dual Core) 및 FreeRTOS 멀티태스킹 환경에서
 *   LD2420 라이브러리를 안전하고 효율적으로 연동하는 고급 아키텍처 예제입니다.
 *
 * [태스크 분리 및 코어 할당 구조]
 * - [코어 0 (Core 0)]: 라이브러리 내부 전담 (사용자 개입 불필요)
 *   1. rxTask  : 5ms 주기 UART 배치 수신 및 바이너리 프레임 파싱
 *   2. cmdTask : 비동기 명령 큐 처리, ACK 타임아웃 감시 및 재전송
 * - [코어 1 (Core 1)]: 사용자 애플리케이션 전담
 *   1. telemetryTask : 2초마다 뮤텍스로 안전하게 데이터를 읽어 원격/시리얼 전송
 *   2. controlTask   : 200ms마다 재실 상태를 확인하여 하드웨어(LED/릴레이) 제어
 *   3. loopTask      : 메인 루프 (주기적 동적 감도 재조정 등)
 *
 * [콜백 함수 주의사항]
 * - onData() 및 onEvent() 콜백은 **rxTask(코어 0) 내부에서 직접 호출**됩니다.
 * - 따라서 콜백 내부에서는 delay(), 긴 문자열 출력, 네트워크 송신 등
 *   블로킹 작업을 절대 피하고, 카운터 증가나 FreeRTOS 큐 전달 등 가벼운 처리만 수행해야 합니다.
 *
 * [배선 안내]
 * - LD2420 VCC       <-> ESP32 3.3V
 * - LD2420 GND       <-> ESP32 GND
 * - LD2420 TX        <-> ESP32 GPIO16 (RX2)
 * - LD2420 RX        <-> ESP32 GPIO17 (TX2)
 * - 상태 표시 LED/출력 <-> ESP32 GPIO2 (내장 LED)
 */

#include <LD2420.h>

HardwareSerial ldSerial(2);
LD2420         radar(ldSerial, 16, 17, 115200);

// 태스크 간 통계 공유용 volatile 카운터 변수
volatile uint32_t g_eventCount = 0;
volatile uint32_t g_dataCount  = 0;

/**
 * @brief 타겟 감지 데이터 수신 콜백
 * @warning rxTask(코어 0)에서 직접 실행되므로 블로킹 작업 금지!
 */
void onData(const LD2420TargetData& d, void* ctx) {
    g_dataCount++;
    // 필요 시 FreeRTOS Queue로 다른 태스크에 복사 전달 가능
}

/**
 * @brief 센서 이벤트 발생 콜백
 */
void onEvent(const char* evt, void* ctx) {
    g_eventCount++;
    Serial.printf("[이벤트 알림] %s\n", evt);
}

/**
 * @brief [사용자 태스크 1] 텔레메트리 전송 태스크 (코어 1 실행)
 * - 2초 주기로 뮤텍스 보호를 통해 최신 센서 스냅샷을 획득하여 출력합니다.
 */
void telemetryTask(void* arg) {
    while (true) {
        // getLatestData()는 내부 뮤텍스로 보호되어 데이터 일관성이 보장됩니다.
        LD2420TargetData d = radar.getLatestData();

        Serial.printf("[텔레메트리] 시퀀스=%u | 재실=%d | 거리=%4d cm | 데이터패킷수=%u | 이벤트수=%u\n",
                      d.sequence,
                      d.presence,
                      d.distance_cm,
                      g_dataCount,
                      g_eventCount);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/**
 * @brief [사용자 태스크 2] 고속 하드웨어 제어 태스크 (코어 1 실행)
 * - 200ms 주기로 재실 여부를 확인하여 LED 출력을 신속하게 제어합니다.
 */
void controlTask(void* arg) {
    const uint8_t LED_PIN = 2; // ESP32 보드 내장 LED (GPIO 2)
    pinMode(LED_PIN, OUTPUT);

    while (true) {
        // isPresent()는 락프리로 즉시 반환되므로 고속 제어에 적합합니다.
        bool isOccupied = radar.isPresent();
        digitalWrite(LED_PIN, isOccupied ? HIGH : LOW);

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n========================================");
    Serial.println("  [09] LD2420 FreeRTOS MultiTask Example");
    Serial.println("========================================");

    // 1. 센서 콜백 등록
    radar.onData(onData);
    radar.onEvent(onEvent);

    // 2. 센서 라이브러리 시작 (코어 0에 rxTask, cmdTask 자동 생성)
    if (!radar.begin()) {
        Serial.println("[오류] LD2420 초기화 실패!");
        while (1) delay(1000);
    }

    // 3. 사용자 태스크를 코어 1에 명시적으로 생성 (loopTask와 함께 코어 1 공유)
    xTaskCreatePinnedToCore(telemetryTask, "telemetryTask", 4096, nullptr, 2, nullptr, 1);
    xTaskCreatePinnedToCore(controlTask,   "controlTask",   2048, nullptr, 3, nullptr, 1);

    // 4. 초기 기본 권장 감도 비동기 적용
    radar.requestDefaultThresholds();

    Serial.println("[알림] 멀티태스크 시스템 정상 가동 중 (Core 0: 통신, Core 1: 앱/제어)");
}

void loop() {
    // Arduino loop() 역시 코어 1에서 실행되며 다른 작업에 방해받지 않습니다.
    // 예: 10초마다 주기적으로 게이트 3번 감도를 미세 재조정
    static uint32_t lastAdjust = 0;
    if (millis() - lastAdjust > 10000) {
        lastAdjust = millis();
        Serial.println("[메인루프] 게이트 3번 감도 미세 조정 명령 비동기 전송...");
        radar.requestGateThreshold(3, 8, 6);
    }

    delay(1000);
}
