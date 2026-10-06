# LD2420 Examples (예제 가이드)

HLK-LD2420 24GHz FMCW 레이더 센서 라이브러리를 위한 아두이노/PlatformIO 공식 예제 모음입니다.  
모든 예제는 **완전 비동기 논블로킹(Non-blocking)** 구조로 설계되었으며, ESP32의 **FreeRTOS 듀얼 태스크(rxTask, cmdTask - Core 0 고정)** 기반으로 구동됩니다.

---

## 📌 공통 하드웨어 배선 안내

| LD2420 레이더 | ESP32 GPIO | 설명 / 주의사항 |
|---|---|---|
| **VCC** | **3.3V** | ⚠️ **5V 직접 인가 금지** (센서 모듈 파손 위험) |
| **GND** | **GND** | 공통 접지 |
| **TX (OT1)** | **GPIO16 (RX2)** | 센서 송신 -> ESP32 수신 (하드웨어 UART2) |
| **RX** | **GPIO17 (TX2)** | 센서 수신 <- ESP32 송신 (하드웨어 UART2) |
| OUT (OT2) | (선택) GPIO18 | 하드웨어 인체 감지 디지털 출력 (존재 시 HIGH) |

> **통신 속도**: 펌웨어 **v1.5.3 이상** 기준 기본 **115200 bps**를 사용합니다.  
> (구형 v1.5.2 이하 모듈은 256000 bps)

---

## 📋 예제 목록 요약

| # | 예제 폴더 / 소스 | 주요 API | 핵심 기능 및 적용 시나리오 |
|---|---|---|---|
| **01** | [`01_BasicPresence`](01_BasicPresence/01_BasicPresence.ino) | `begin()` `isPresent()` `getDistance()` | 최소 기본 사용법 (재실 여부 및 거리 cm 논블로킹 폴링) |
| **02** | [`02_MaxGateLimit`](02_MaxGateLimit/02_MaxGateLimit.ino) | `requestMinMaxDistance()` `requestAllGateThresholds()` | 방 크기(4m)에 맞춘 최대 거리 제한 및 벽/가구 반사 오탐 차단 |
| **03** | [`03_ZoneDetection`](03_ZoneDetection/03_ZoneDetection.ino) | `requestMinMaxDistance()` `requestAllGateThresholds()` | 특정 구간(2.8m~5.6m)만 선별 감지 (근거리 통행/원거리 배경 차단) |
| **04** | [`04_TieredSensitivity`](04_TieredSensitivity/04_TieredSensitivity.ino) | `requestAllGateThresholds()` `getGateEnergy()` | 거리별 3단계 차등 감도 (근거리 초고감도, 중거리 표준, 원거리 저감도) |
| **05** | [`05_BathroomLightFan`](05_BathroomLightFan/05_BathroomLightFan.ino) | `requestTimeout()` `requestAllGateThresholds()` | 스마트 욕실 조명(GPIO25)·환풍기(GPIO26) 제어 및 30초 유지 타임아웃 |
| **06** | [`06_CeilingFanFilter`](06_CeilingFanFilter/06_CeilingFanFilter.ino) | `requestAllGateThresholds()` `getGateEnergy()` | 천장 선풍기(실링팬)/에어컨 바람 흔들림 위치 게이트(5~7) 마스킹 |
| **07** | [`07_AutoCalibration`](07_AutoCalibration/07_AutoCalibration.ino) | `startCalibration()` `applyCalibration()` | 무인 환경 5분(64샘플) 노이즈 플로어 자동 수집 및 최적 임계값 적용 |
| **08** | [`08_ManualCalibration`](08_ManualCalibration/08_ManualCalibration.ino) | 시리얼 콘솔 대화형 인터페이스 | 시리얼 명령어(g, r, t, p, c)를 통한 실시간 수동 게이트 튜닝 |
| **09** | [`09_FreeRTOS_MultiTask`](09_FreeRTOS_MultiTask/09_FreeRTOS_MultiTask.ino) | `onData()` `onEvent()` `getLatestData()` | Core 0(통신) / Core 1(텔레메트리, 고속 GPIO 제어) 멀티태스크 아키텍처 |
| **10** | [`10_AllFeatures_Demo`](10_AllFeatures_Demo/10_AllFeatures_Demo.ino) | 라이브러리 전체 공개 API | 시리얼 숫자 메뉴(1~0, m) 기반 통합 종합 기능 검증 콘솔 |

---

## 🔍 예제별 상세 설명

### 01. 기본 재실 감지 ([01_BasicPresence.ino](01_BasicPresence/01_BasicPresence.ino))
- **목적**: 센서 연결 후 가장 빠르게 인체 감지 동작을 확인하는 기본 예제.
- **핵심 동작**:
  - `radar.begin()` 호출 시 FreeRTOS `rxTask`, `cmdTask`가 백그라운드에서 자동 구동됩니다.
  - `loop()` 내부에서 `radar.isPresent()`(존재 여부) 및 `radar.getDistance()`(거리 cm)를 락프리로 즉시 읽어옵니다.

### 02. 최대 게이트 거리 제한 ([02_MaxGateLimit.ino](02_MaxGateLimit/02_MaxGateLimit.ino))
- **목적**: 방 크기(약 4m)를 초과하는 원거리 반사파 및 벽 너머 복도 통행으로 인한 오탐 방지.
- **핵심 동작**:
  - `radar.requestMinMaxDistance(1, 6)`: 게이트 1(0.7m)부터 게이트 6(4.2m)까지만 센서 하드웨어 감지 범위로 설정.
  - 게이트 7~15번은 임계값 100(최대 둔감)으로 올려 반사 신호를 완전 무시.

### 03. 특정 구역 필터링 ([03_ZoneDetection.ino](03_ZoneDetection/03_ZoneDetection.ino))
- **목적**: 센서 바로 앞 통행이나 원거리 배경 움직임을 배제하고, 복도 중앙이나 특정 침대/책상 구역만 선별 감지.
- **핵심 동작**:
  - 목표 구간인 게이트 4(2.8m) ~ 8(5.6m)만 정상 감도 유지.
  - 구역 이전(게이트 0~3)과 구역 이후(게이트 9~15)는 임계값 100으로 마스킹.

### 04. 거리별 차등 감도 설정 ([04_TieredSensitivity.ino](04_TieredSensitivity/04_TieredSensitivity.ino))
- **목적**: FMCW 레이더의 거리별 감쇠 및 노이즈 특성을 고려하여 3단계로 감도를 차등 부여.
- **핵심 동작**:
  - 근거리(0~2.8m, Gate 0~4): 호흡/미세 움직임 포착을 위해 임계값 5(초고감도).
  - 중거리(3.5~6.3m, Gate 5~9): 통상 보행 감지를 위해 move=20, still=15(표준).
  - 원거리(7.0~10.5m, Gate 10~15): 창문 밖 차량 등 잡음 차단을 위해 move=80, still=60(저감도).

### 05. 스마트 욕실 조명 및 환풍기 제어 ([05_BathroomLightFan.ino](05_BathroomLightFan/05_BathroomLightFan.ino))
- **목적**: 화장실/욕실 환경에서 문 열림 진입 즉시 점등, 착석/샤워 중 꺼짐 방지, 문 닫힘 후 지연 꺼짐 구현.
- **핵심 동작**:
  - 입구(Gate 1~3): move=5~8로 신속 진입 감지.
  - 샤워/변기(Gate 4~6): still=10~20으로 미세 호흡 감지 유지.
  - `radar.requestTimeout(30)`: 마지막 움직임 이후 30초간 재실 신호 유지.
  - GPIO 25(조명 릴레이), GPIO 26(환풍기 릴레이) 자동 제어.

### 06. 천장 선풍기 오탐 필터링 ([06_CeilingFanFilter.ino](06_CeilingFanFilter/06_CeilingFanFilter.ino))
- **목적**: 천장 실링팬, 선풍기 회전 날개, 에어컨 바람에 날리는 커튼으로 인한 허위 재실 감지 제거.
- **핵심 동작**:
  - 선풍기 설치 거리(Gate 5~7, 3.5m~4.9m)의 임계값을 100으로 올려 회전체 신호 무시.
  - 사람이 위치한 게이트 1~4는 고감도를 유지하며, 선풍기 게이트의 실시간 에너지를 모니터링.

### 07. 완전 비동기 자동 캘리브레이션 ([07_AutoCalibration.ino](07_AutoCalibration/07_AutoCalibration.ino))
- **목적**: 설치된 공간의 고유한 배경 노이즈(Noise Floor)를 측정하여 최적의 이동/정지 임계값을 자동 도출.
- **핵심 동작**:
  - `radar.startCalibration()`: 실시간 에너지 모드로 전환 후 5초 주기로 64개 샘플(약 5.3분) 누적 수집.
  - 수집 완료(`READY` 상태) 시 `radar.applyCalibration()`을 호출하여 노이즈 대비 이동 5배(10~100), 정지 3배(5~80) 임계값을 자동 산출하여 센서에 주입 후 일반 모드로 복귀.

### 08. 시리얼 대화형 실시간 수동 튜닝 ([08_ManualCalibration.ino](08_ManualCalibration/08_ManualCalibration.ino))
- **목적**: 시리얼 터미널에서 즉석으로 명령어를 입력하여 현장 환경에 맞는 게이트 파라미터를 실시간 튜닝.
- **명령어 예시**:
  - `g3 15 10`: 게이트 3번의 이동 감도 15, 정지 감도 10으로 변경
  - `r 1 6`: 감지 범위를 게이트 1~6으로 설정
  - `t 30`: 부재 유지 타임아웃을 30초로 변경
  - `p`: 현재 16개 게이트의 거리 및 임계값 테이블 출력
  - `c`: 자동 캘리브레이션 시작

### 09. FreeRTOS 멀티태스크 아키텍처 ([09_FreeRTOS_MultiTask.ino](09_FreeRTOS_MultiTask/09_FreeRTOS_MultiTask.ino))
- **목적**: ESP32의 코어 0과 코어 1을 분리 활용하는 대규모/고신뢰성 임베디드 펌웨어 구조 제시.
- **핵심 동작**:
  - **Core 0**: 라이브러리 내부 태스크(`rxTask`, `cmdTask`)가 UART 통신 전담.
  - **Core 1**: 사용자 `telemetryTask`(뮤텍스 보호 하에 `getLatestData()` 호출) 및 `controlTask`(200ms 주기 고속 GPIO2 LED 제어) 실행.
  - `onData()`, `onEvent()` 콜백의 논블로킹 작성 수칙 준수.

### 10. 전체 기능 종합 데모 ([10_AllFeatures_Demo.ino](10_AllFeatures_Demo/10_AllFeatures_Demo.ino))
- **목적**: 라이브러리가 제공하는 모든 프리셋, 범위, 임계값, 타임아웃, 캘리브레이션 제어 API를 한눈에 검증하는 통합 콘솔.
- **메뉴 구성**:
  - `1`: 현재 센서 전체 상태(재실, 거리, 16개 게이트 에너지, 노이즈 플로어) 리포트
  - `2~4`: 내장 감도 프리셋(기본, 초고감도, 균형) 즉시 적용
  - `5`: 감지 범위 설정 (`5 <min> <max>`)
  - `6`: 게이트 임계값 개별 설정 (`6 <gate> <move> <still>`)
  - `7`: 타임아웃 설정 (`7 <sec>`)
  - `8~0`: 캘리브레이션 시작, 적용, 취소

---

## 🛠️ 권장 게이트 튜닝 절차

실제 현장 설치 시 가장 안정적인 감지 결과를 얻는 표준 절차:

1. **물리적 범위 제한 (`requestMinMaxDistance`)**
   - 방이나 감지 구역의 실제 크기에 맞춰 최대 게이트를 제한합니다. (벽 너머 간섭 방지)
2. **자동 캘리브레이션 실행 (`startCalibration`)**
   - 사람이 없는 빈 방 상태를 만든 후 약 5분간 노이즈 플로어를 측정합니다.
   - 측정이 끝나면 `applyCalibration()`으로 공간 맞춤형 임계값을 자동 반영합니다.
3. **오탐 발생원 국소 튜닝 (`requestGateThreshold`)**
   - 선풍기, 커튼, 에어컨 등 특정 위치에서 간헐적 오탐이 발생할 경우 해당 거리의 게이트만 임계값을 상향(예: 80~100) 조정합니다.
4. **부재 유지 타임아웃 설정 (`requestTimeout`)**
   - 정지 인체의 미세 움직임 누락을 방지하기 위해 15초~30초 수준의 유지 시간을 설정합니다.

---

## ⏱️ 캘리브레이션 체크리스트

- [ ] 감지 공간 내에 사람이 완전히 퇴실하였는가?
- [ ] 선풍기, 로봇청소기, 커튼 등 주기적 이동 물체가 정지해 있는가?
- [ ] 약 5.3분 (64샘플 × 5초 = 320초) 동안 공간이 비워져 있는가?
- [ ] `calibrationProgress() == 100` 및 `calibrationState() == READY`에 도달했는가?
- [ ] `applyCalibration()`을 호출하여 센서에 정상 반영되었는가?

---

## ⚠️ 콜백(Callback) 함수 작성 시 주의사항

`radar.onData()` 및 `radar.onEvent()` 콜백은 **Core 0의 `rxTask` 컨텍스트에서 직접 호출**됩니다.

- ❌ **금지 사항**: `delay()`, `Serial.print` 다량 호출, HTTP/MQTT 통신, SD카드 쓰기 등 긴 블로킹 작업
- ✅ **권장 사항**: 플래그 설정, 카운터 증가, 또는 FreeRTOS Queue(`xQueueSendFromISR` 또는 `xQueueSend`)를 통한 Core 1 태스크로의 데이터 전달
