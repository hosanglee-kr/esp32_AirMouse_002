// =======================================================
// File: examples/08_ManualCalibration/08_ManualCalibration.ino
// 모듈명: 시리얼 대화형 실시간 수동 튜닝 및 캘리브레이션 예제
// =======================================================

/**
 * [예제 08] 08_ManualCalibration
 * - 시리얼 모니터 명령어를 통해 실시간으로 특정 게이트의 감도, 감지 범위,
 *   타임아웃 등을 수정하고 즉시 센서 동작을 확인할 수 있는 엔지니어링 튜닝 도구입니다.
 *
 * [지원 시리얼 명령어 목록]
 * - g <gate> <move> <still> : 특정 게이트의 이동/정지 임계값 변경 (예: g3 10 8)
 * - r <min> <max>           : 감지 최소/최대 게이트 범위 설정 (예: r 1 6)
 * - t <초>                  : 인체 부재 판정 유지 타임아웃 초 설정 (예: t 30)
 * - p                       : 현재 16개 게이트의 거리별 감도 설정 테이블 출력
 * - c                       : 자동 캘리브레이션 수집 시작
 * - h                       : 명령어 도움말 출력
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

// 로컬 게이트 설정 보관용 구조체
LD2420GateConfig g_cfg;

/**
 * @brief 시리얼 도움말 출력 함수
 */
void printHelp() {
    Serial.println("\n----------------- [ 명령어 도움말 ] -----------------");
    Serial.println("  g <gate> <move> <still> : 특정 게이트 임계값 변경 (예: g3 15 10)");
    Serial.println("  r <min> <max>           : 감지 범위 게이트 설정 (예: r 1 6)");
    Serial.println("  t <초>                  : 부재 유지 타임아웃 설정 (예: t 15)");
    Serial.println("  p                       : 현재 16개 게이트 설정 상태 출력");
    Serial.println("  c                       : 자동 캘리브레이션 시작");
    Serial.println("  h                       : 이 도움말 다시 보기");
    Serial.println("----------------------------------------------------\n");
}

/**
 * @brief 현재 16개 게이트의 임계값 및 거리 환산 테이블 출력
 */
void printConfig() {
    Serial.println("\n=== [ 현재 16개 게이트 임계값 설정 ] ===");
    for (int g = 0; g < LD2420_MAX_GATES; g++) {
        Serial.printf("  게이트 %2d (약 %4.1f m) | 이동(move): %3u | 정지(still): %3u\n",
                      g,
                      g * LD2420_GATE_DISTANCE_M,
                      g_cfg.move_threshold[g],
                      g_cfg.still_threshold[g]);
    }
    Serial.println("=========================================\n");
}

/**
 * @brief 시리얼로부터 입력받은 문자열 명령어 파싱 및 실행
 * @param line 시리얼 1라인 문자열
 */
void handleCommand(String line) {
    line.trim();
    if (line.length() == 0) return;

    // 도움말
    if (line == "h" || line == "help") {
        printHelp();
        return;
    }

    // 현재 설정 출력
    if (line == "p") {
        printConfig();
        return;
    }

    // 자동 캘리브레이션 시작
    if (line == "c") {
        Serial.println("[알림] 자동 캘리브레이션을 시작합니다. 공간을 비워주세요...");
        radar.startCalibration();
        return;
    }

    // 게이트 임계값 변경 (예: g3 10 5)
    if (line.startsWith("g")) {
        int g, m, s;
        if (sscanf(line.c_str(), "g%d %d %d", &g, &m, &s) == 3 ||
            sscanf(line.c_str(), "g %d %d %d", &g, &m, &s) == 3) {
            if (g < 0 || g >= LD2420_MAX_GATES) {
                Serial.printf("[오류] 게이트 번호는 0 ~ %d 범위여야 합니다.\n", LD2420_MAX_GATES - 1);
                return;
            }
            // 센서에 비동기 명령 전송
            radar.requestGateThreshold((uint8_t)g, (uint16_t)m, (uint16_t)s);
            g_cfg.move_threshold[g]  = m;
            g_cfg.still_threshold[g] = s;
            Serial.printf("[성공] 게이트 %d (약 %.1fm) 설정 완료 -> 이동:%d, 정지:%d\n",
                          g, g * LD2420_GATE_DISTANCE_M, m, s);
        } else {
            Serial.println("[형식 오류] 사용법: g <gate 0~15> <move 0~100> <still 0~100>");
        }
        return;
    }

    // 감지 범위 설정 (예: r 1 6)
    if (line.startsWith("r")) {
        int mn, mx;
        if (sscanf(line.c_str(), "r %d %d", &mn, &mx) == 2) {
            if (mn < 0 || mx >= LD2420_MAX_GATES || mn > mx) {
                Serial.println("[오류] 올바르지 않은 게이트 범위입니다.");
                return;
            }
            radar.requestMinMaxDistance((uint8_t)mn, (uint8_t)mx);
            Serial.printf("[성공] 감지 범위 변경 완료: 게이트 %d ~ %d (%.1fm ~ %.1fm)\n",
                          mn, mx, mn * LD2420_GATE_DISTANCE_M, mx * LD2420_GATE_DISTANCE_M);
        } else {
            Serial.println("[형식 오류] 사용법: r <min> <max>");
        }
        return;
    }

    // 타임아웃 설정 (예: t 30)
    if (line.startsWith("t")) {
        int sec;
        if (sscanf(line.c_str(), "t %d", &sec) == 1) {
            radar.requestTimeout((uint16_t)sec);
            Serial.printf("[성공] 인체 부재 유지 타임아웃 변경 완료: %d 초\n", sec);
        } else {
            Serial.println("[형식 오류] 사용법: t <초>");
        }
        return;
    }

    Serial.println("[알 수 없는 명령] 도움말을 보려면 'h'를 입력하세요.");
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n========================================");
    Serial.println("  [08] LD2420 ManualCalibration Example");
    Serial.println("========================================");

    // 기본 권장값 로드
    g_cfg.setDefault();

    radar.begin();
    radar.requestAllGateThresholds(g_cfg);
    radar.requestTimeout(30);

    printHelp();
}

void loop() {
    // 1. 시리얼 콘솔 입력 처리
    if (Serial.available()) {
        String inputLine = Serial.readStringUntil('\n');
        handleCommand(inputLine);
    }

    // 2. 센서 실시간 상태 1초 주기 모니터링 출력
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint > 1000) {
        lastPrint = millis();

        if (radar.isReady()) {
            Serial.printf("[모니터] 재실: %s | 거리: %4d cm | 캘리브레이션 진행: %3u%%\n",
                          radar.isPresent() ? "O (감지)" : "X (부재)",
                          radar.getDistance(),
                          radar.calibrationProgress());
        }
    }
}
