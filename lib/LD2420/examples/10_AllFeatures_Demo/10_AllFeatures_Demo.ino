// =======================================================
// File: examples/10_AllFeatures_Demo/10_AllFeatures_Demo.ino
// 모듈명: LD2420 라이브러리 전체 기능 시연 통합 대화형 콘솔
// =======================================================

/**
 * [예제 10] 10_AllFeatures_Demo
 * - HLK-LD2420 라이브러리가 제공하는 모든 공개 API를 시리얼 콘솔 메뉴를 통해
 *   직접 테스트하고 동작을 검증할 수 있는 통합 종합 데모 예제입니다.
 *
 * [제공 기능]
 * 1. 실시간 센서 상태 및 16개 게이트 에너지, 노이즈 플로어 조회
 * 2. 내장 프리셋 즉시 적용 (기본, 초고감도, 균형)
 * 3. 감지 최소/최대 거리 게이트 범위 제한
 * 4. 특정 게이트 이동/정지 임계값 개별 수정
 * 5. 인체 부재 유지 타임아웃 변경
 * 6. 자동 캘리브레이션 시작 / 적용 / 취소
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

/**
 * @brief 시리얼 대화형 메뉴 항목 출력
 */
void printMenu() {
    Serial.println("\n================ [ LD2420 전체 기능 데모 메뉴 ] ================");
    Serial.println("  1 : 현재 상태 출력 (재실 여부, 거리, 게이트 에너지, 노이즈 플로어)");
    Serial.println("  2 : 기본 감도 프리셋 적용 (0~5번: 10, 6~15번: 100)");
    Serial.println("  3 : 초고감도 프리셋 적용 (모든 게이트: 5)");
    Serial.println("  4 : 균형 감도 프리셋 적용 (0~7번: 15, 8~15번: 50)");
    Serial.println("  5 : 감지 범위 설정 (사용법: 5 <최소게이트> <최대게이트>, 예: 5 1 6)");
    Serial.println("  6 : 게이트 임계값 설정 (사용법: 6 <게이트> <이동> <정지>, 예: 6 3 15 10)");
    Serial.println("  7 : 부재 유지 타임아웃 설정 (사용법: 7 <초>, 예: 7 30)");
    Serial.println("  8 : 자동 캘리브레이션 시작 (공간 비우기 필수)");
    Serial.println("  9 : 캘리브레이션 결과 적용");
    Serial.println("  0 : 캘리브레이션 취소");
    Serial.println("  m : 메뉴 다시 보기");
    Serial.println("=================================================================\n");
}

/**
 * @brief 현재 센서의 전반적인 상태 모니터링 출력
 */
void printStatus() {
    Serial.println("\n----------------- [ 센서 현재 상태 리포트 ] -----------------");
    Serial.printf("  - 정상 작동(Ready)     : %s\n", radar.isReady() ? "YES (RUN 모드)" : "NO (초기화/대기 중)");
    Serial.printf("  - 인체 감지 여부       : %s\n", radar.isPresent() ? "O (감지됨)" : "X (부재)");
    Serial.printf("  - 감지된 타겟 거리     : %d cm (약 %.2f m)\n", radar.getDistance(), radar.getDistance() / 100.0f);
    Serial.printf("  - 캘리브레이션 상태    : 상태코드 %d | 진행률 %u%%\n", (int)radar.calibrationState(), radar.calibrationProgress());

    // 16개 게이트별 실시간 에너지 레벨 출력
    Serial.print("  - 게이트별 실시간 에너지: [ ");
    for (int g = 0; g < LD2420_MAX_GATES; g++) {
        Serial.printf("%d ", radar.getGateEnergy(g));
    }
    Serial.println("]");

    // 캘리브레이션으로 측정된 노이즈 플로어 수치 출력
    Serial.print("  - 측정된 노이즈 플로어  : [ ");
    for (int g = 0; g < LD2420_MAX_GATES; g++) {
        Serial.printf("%d ", radar.calibrationNoiseFloor(g));
    }
    Serial.println("]");
    Serial.println("-------------------------------------------------------------\n");
}

/**
 * @brief 시리얼 한 줄 입력을 분석하여 해당 API 호출
 */
void handleMenu(String line) {
    line.trim();
    if (line.length() == 0) return;

    if (line == "m" || line == "M") {
        printMenu();
        return;
    }
    if (line == "1") {
        printStatus();
        return;
    }

    // 프리셋 적용
    if (line == "2") {
        radar.requestDefaultThresholds();
        Serial.println("[적용] 기본 권장 감도 프리셋이 적용되었습니다.");
        return;
    }
    if (line == "3") {
        radar.requestSensitiveThresholds();
        Serial.println("[적용] 초고감도 프리셋이 적용되었습니다.");
        return;
    }
    if (line == "4") {
        radar.requestBalancedThresholds();
        Serial.println("[적용] 균형 감도 프리셋이 적용되었습니다.");
        return;
    }

    // 캘리브레이션 제어
    if (line == "8") {
        Serial.println("[알림] 자동 캘리브레이션을 시작합니다. 공간을 비워주세요 (약 5분 소요)...");
        radar.startCalibration();
        return;
    }
    if (line == "9") {
        if (radar.applyCalibration()) {
            Serial.println("[성공] 계산된 캘리브레이션 임계값이 센서에 정상 반영되었습니다.");
        } else {
            Serial.println("[실패] 캘리브레이션이 완료되지 않았거나 적용에 실패했습니다.");
        }
        return;
    }
    if (line == "0") {
        radar.cancelCalibration();
        Serial.println("[알림] 캘리브레이션이 취소되고 정상 모드로 복귀했습니다.");
        return;
    }

    // 범위 설정 (예: 5 1 6)
    if (line.startsWith("5")) {
        int mn, mx;
        if (sscanf(line.c_str(), "5 %d %d", &mn, &mx) == 2) {
            radar.requestMinMaxDistance((uint8_t)mn, (uint8_t)mx);
            Serial.printf("[성공] 감지 범위가 게이트 %d ~ %d (%.1fm ~ %.1fm)로 설정되었습니다.\n",
                          mn, mx, mn * LD2420_GATE_DISTANCE_M, mx * LD2420_GATE_DISTANCE_M);
        } else {
            Serial.println("[오류] 사용법: 5 <최소게이트 0~15> <최대게이트 0~15>");
        }
        return;
    }

    // 게이트 임계값 설정 (예: 6 3 15 10)
    if (line.startsWith("6")) {
        int g, m, s;
        if (sscanf(line.c_str(), "6 %d %d %d", &g, &m, &s) == 3) {
            if (g >= 0 && g < LD2420_MAX_GATES) {
                radar.requestGateThreshold((uint8_t)g, (uint16_t)m, (uint16_t)s);
                Serial.printf("[성공] 게이트 %d 설정 요청 완료: 이동=%d, 정지=%d\n", g, m, s);
            } else {
                Serial.println("[오류] 게이트 번호는 0~15 사이여야 합니다.");
            }
        } else {
            Serial.println("[오류] 사용법: 6 <게이트 0~15> <이동 0~100> <정지 0~100>");
        }
        return;
    }

    // 타임아웃 설정 (예: 7 30)
    if (line.startsWith("7")) {
        int sec;
        if (sscanf(line.c_str(), "7 %d", &sec) == 1) {
            radar.requestTimeout((uint16_t)sec);
            Serial.printf("[성공] 부재 유지 타임아웃이 %d초로 설정되었습니다.\n", sec);
        } else {
            Serial.println("[오류] 사용법: 7 <초>");
        }
        return;
    }

    Serial.println("[알 수 없는 명령] 메뉴를 보려면 'm'을 입력하세요.");
}

void setup() {
    Serial.begin(115200);
    delay(200);

    // 이벤트 콜백 등록
    radar.onEvent([](const char* evt, void* ctx) {
        Serial.printf(">> [센서 이벤트] %s\n", evt);
    });

    if (!radar.begin()) {
        Serial.println("[오류] LD2420 초기화 실패! (배선 및 전원 확인)");
        while (1) delay(1000);
    }

    printMenu();
}

void loop() {
    // 1. 시리얼 입력 모니터링
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        handleMenu(cmd);
    }

    // 2. 주기적 상태 로그 (3초마다)
    static uint32_t lastLog = 0;
    if (millis() - lastLog > 3000) {
        lastLog = millis();

        if (radar.isReady()) {
            Serial.printf("[실시간] 재실: %s | 거리: %4d cm\n",
                          radar.isPresent() ? "O (감지)" : "X (미감지)",
                          radar.getDistance());
        }
    }
}
