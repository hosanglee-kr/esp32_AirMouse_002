# LD2420 Examples

ESP32 + LD2420 (24GHz FMCW 레이더) 예제 모음.
모든 예제는 **비동기 논블로킹** 이며 **FreeRTOS 태스크** 기반입니다.

## 공통 배선

| LD2420 | ESP32 | 비고 |
|---|---|---|
| VCC | 3.3V | **5V 금지** |
| GND | GND | |
| TX (OT1) | GPIO16 (RX2) | 펌웨어 v1.5.3+ |
| RX | GPIO17 (TX2) | 펌웨어 v1.5.3+ |
| OUT (OT2) | (선택) GPIO18 | 존재 시 HIGH |

## 예제 목록

| # | 파일 | 주요 API | 설명 |
|---|---|---|---|
| 01 | BasicPresence | `begin()` `isPresent()` `getDistance()` | 최소 사용법 |
| 02 | MaxGateLimit | `requestMinMaxDistance()` `requestAllGateThresholds()` | 벽·가구 오탐 방지 |
| 03 | ZoneDetection | `requestMinMaxDistance()` | 특정 구역만 감지 |
| 04 | TieredSensitivity | `requestAllGateThresholds()` | 거리별 감도 차등 |
| 05 | BathroomLightFan | `requestTimeout()` `requestAllGateThresholds()` | 진입+재실 조명/환풍기 |
| 06 | CeilingFanFilter | `requestAllGateThresholds()` | 천장 선풍기 필터 |
| 07 | AutoCalibration | `startCalibration()` `applyCalibration()` | 자동 캘리브레이션 |
| 08 | ManualCalibration | 시리얼 + `requestGateThreshold()` | 수동 조정 |
| 09 | FreeRTOS_MultiTask | `onData()` `onEvent()` | 태스크 분리 |
| 10 | AllFeatures_Demo | 모든 API | 전체 기능 시연 |

## 실행 방법

### Arduino IDE

1. 예제 파일 열기: `File → Examples → LD2420 → 01_BasicPresence`
2. 보드 선택: `Tools → Board → ESP32 Dev Module`
3. 포트 선택 후 업로드
4. `Tools → Serial Monitor` (115200 baud)

### PlatformIO

```bash
cd examples/01_BasicPresence
pio run -t upload
pio device monitor
```

## 게이트 튜닝 순서

대부분의 예제에서 권장하는 튜닝 순서:

1. **물리 범위 제한**: `requestMinMaxDistance(min, max)`
   - 실제 방 크기에 맞춰 최대 게이트 제한
2. **자동 캘리브레이션**: `startCalibration()` → `applyCalibration()`
   - 빈 환경에서 30초 이상 수집
3. **특정 게이트 미세 조정**: `requestGateThreshold(gate, move, still)`
   - 천장 선풍기, 문 너머 등 특정 오탐원 무시

## 캘리브레이션 체크리스트

- [ ] 대상 공간에 사람이 없음
- [ ] 움직이는 물체(선풍기, 커튼 등) 정지
- [ ] 최소 30초 이상 대기
- [ ] `calibrationProgress() == 100`
- [ ] `calibrationState() == READY`
- [ ] `applyCalibration()` 호출

## 문제 해결

| 증상 | 원인 | 해결 |
|---|---|---|
| 초기화 실패 | 전원 3.3V 아님 | 전원 재확인 |
| 데이터 없음 | 펌웨어 보레이트 불일치 | 115200 또는 256000 시도 |
| 오탐 많음 | 캘리브레이션 미실시 | `startCalibration()` 실행 |
| 정지 인체 미감지 | still 임계값 높음 | `still_threshold` 낮춤 |
| 천장 선풍기 오탐 | 해당 게이트 민감 | 게이트 임계값 100 |
| 문 너머 오탐 | 범위 미제한 | `requestMinMaxDistance()` |

## 콜백 사용 시 주의

`onData`/`onEvent` 콜백은 **rxTask 내부에서 직접 호출**됩니다.
무거운 작업(Serial.print, delay, Wi-Fi 호출 등)은 금지.
필요 시 FreeRTOS Queue로 다른 태스크에 전달하세요.

```cpp
// 좋은 예: 가벼운 플래그만
void onData(const LD2420TargetData &d, void *ctx) {
  g_presenceCount++;
}

// 나쁜 예: 블로킹 작업
void onData(const LD2420TargetData &d, void *ctx) {
  Serial.printf(...);   // 무거움
  delay(100);           // 절대 금지
  httpClient.POST(...); // 절대 금지
}
```
