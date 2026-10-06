// =======================================================
// File: examples/05_BathroomLightFan/05_BathroomLightFan.ino
// 모듈명: 스마트 욕실/화장실 조명 및 환풍기 릴레이 제어 예제
// =======================================================

/**
 * [예제 05] 05_BathroomLightFan
 * - 화장실 및 욕실 환경에 특화된 재실 센서 자동화 구현 예제입니다.
 * - 주요 과제 해결:
 *   1. 진입 시 빠른 점등: 문 앞(게이트 1~3, 0.7~2.1m)에서 움직임을 즉시 포착하여 조명 점등
 *   2. 샤워/변기 착석 중 꺼짐 방지: 샤워실/양변기 위치(게이트 4~6, 2.8~4.2m)에서 미세 정지 감도(still) 강화
 *   3. 문 너머 복도 오탐 방지: 게이트 7~15번(4.9m 이상)은 임계값 100으로 차단
 *   4. 부재 지연 유지(Timeout): 센서 타임아웃을 30초로 설정하여 순간적인 움직임 멈춤에도 꺼지지 않도록 방지
 *
 * [하드웨어 연결]
 * - LD2420 VCC       <-> ESP32 3.3V
 * - LD2420 GND       <-> ESP32 GND
 * - LD2420 TX        <-> ESP32 GPIO16 (RX2)
 * - LD2420 RX        <-> ESP32 GPIO17 (TX2)
 * - 조명 릴레이 제어   <-> ESP32 GPIO25 (HIGH = 켜짐, LOW = 꺼짐)
 * - 환풍기 릴레이 제어 <-> ESP32 GPIO26 (HIGH = 켜짐, LOW = 꺼짐)
 */

#include <LD2420.h>

HardwareSerial ldSerial(2);
LD2420         radar(ldSerial, 16, 17, 115200);

// 릴레이 제어 GPIO 핀 정의
#define RELAY_LIGHT 25 // 조명 제어 릴레이 핀
#define RELAY_FAN   26 // 환풍기 제어 릴레이 핀

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n========================================");
    Serial.println("  [05] LD2420 BathroomLightFan Example");
    Serial.println("========================================");

    // 1. 릴레이 출력 핀 초기화 (초기 상태: 꺼짐)
    pinMode(RELAY_LIGHT, OUTPUT);
    pinMode(RELAY_FAN, OUTPUT);
    digitalWrite(RELAY_LIGHT, LOW);
    digitalWrite(RELAY_FAN, LOW);

    // 2. LD2420 센서 초기화
    if (!radar.begin()) {
        Serial.println("[오류] LD2420 센서 초기화 실패!");
        while (1) delay(1000);
    }

    // 3. 욕실 맞춤형 게이트 임계값 구성
    LD2420GateConfig cfg = {};

    // [입구 진입 구역]: 게이트 1~3 (약 0.7m ~ 2.1m)
    // 문을 열고 들어오는 순간 즉각적인 움직임 감지
    cfg.move_threshold[1]  = 5;
    cfg.still_threshold[1] = 5;
    cfg.move_threshold[2]  = 5;
    cfg.still_threshold[2] = 5;
    cfg.move_threshold[3]  = 8;
    cfg.still_threshold[3] = 8;

    // [샤워부스 / 양변기 재실 구역]: 게이트 4~6 (약 2.8m ~ 4.2m)
    // 앉아있거나 정지해 있어도 호흡/미세 움직임(still)으로 재실 유지
    cfg.move_threshold[4]  = 15;
    cfg.still_threshold[4] = 10;
    cfg.move_threshold[5]  = 20;
    cfg.still_threshold[5] = 15;
    cfg.move_threshold[6]  = 30;
    cfg.still_threshold[6] = 20;

    // [문 너머 / 욕실 외곽]: 게이트 7~15 (약 4.9m ~ 11.2m)
    // 복도를 지나가는 다른 가족에 의한 오탐을 방지하기 위해 완전 차단
    for (int g = 7; g < LD2420_MAX_GATES; g++) {
        cfg.move_threshold[g]  = 100;
        cfg.still_threshold[g] = 100;
    }

    // 센서에 게이트 감도 일괄 전송
    radar.requestAllGateThresholds(cfg);

    // 유효 감지 범위를 게이트 1 ~ 6으로 제한
    radar.requestMinMaxDistance(1, 6);

    // 인체 부재 판정 유지 타임아웃을 30초로 설정
    // (마지막 감지 후 30초 동안 움직임이 전혀 없을 때 비로소 미감지로 판정)
    radar.requestTimeout(30);

    Serial.println("[설정 완료] 욕실 최적화 모드 구동 시작");
    Serial.println("  - 감지 범위: 게이트 1 ~ 6 (0.7m ~ 4.2m)");
    Serial.println("  - 부재 지연 타임아웃: 30초");
}

void loop() {
    // 논블로킹 재실 확인
    bool isOccupied = radar.isPresent();

    // 재실 상태에 따라 조명 및 환풍기 릴레이 제어
    digitalWrite(RELAY_LIGHT, isOccupied ? HIGH : LOW);
    digitalWrite(RELAY_FAN, isOccupied ? HIGH : LOW);

    // 2초마다 시리얼 로그 출력
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint > 2000) {
        lastPrint = millis();
        Serial.printf("[상태] 재실=%s | 거리=%4d cm | 조명=%s | 환풍기=%s\n",
                      isOccupied ? "재실중(ON) " : "비어있음(OFF)",
                      radar.getDistance(),
                      digitalRead(RELAY_LIGHT) ? "ON " : "OFF",
                      digitalRead(RELAY_FAN) ? "ON " : "OFF");
    }

    // 폴링 루프 지연 (논블로킹)
    delay(100);
}
