# LD2420 Arduino Library

[![Arduino Library](https://img.shields.io/badge/Arduino-Library-blue)](https://www.arduino.cc/reference/en/libraries/)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-Compatible-orange)](https://platformio.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![ESP32](https://img.shields.io/badge/ESP32-FreeRTOS-red)]()

**HLK-LD2420 24GHz FMCW 레이더 센서**를 위한 완전한 객체지향 Arduino / PlatformIO 라이브러리.  
ESPHome의 LD2420 컴포넌트 아키텍처를 Arduino에 맞게 재구현하여, **비동기 논블로킹(Non-blocking)**, **FreeRTOS 태스크 기반(Core 0 고정)**, **자동 노이즈 캘리브레이션**을 지원합니다.

---

## 목차

- [주요 특징](#주요-특징)
- [지원 하드웨어](#지원-하드웨어)
- [설치](#설치)
- [배선](#배선)
- [빠른 시작](#빠른-시작)
- [API 레퍼런스](#api-레퍼런스)
- [게이트 시스템](#게이트-시스템)
- [캘리브레이션 원리](#캘리브레이션-원리)
- [예제 목록](#예제-목록)
- [FAQ](#faq)
- [라이선스 및 참고 자료](#라이선스-및-참고-자료)

---

## 주요 특징

| 특징 | 설명 |
|---|---|
| **완전 논블로킹** | `delay()`나 블로킹 `while` 루프가 전혀 없음. 모든 작업은 FreeRTOS 틱 지연 및 FSM 타이머로 처리 |
| **객체지향 설계** | 파사드(`LD2420`), 프로토콜(`LD2420Protocol`), 캘리브레이터(`LD2420Calibration`), 타입(`LD2420_Types_001`) 분리 |
| **FreeRTOS 지원** | 통신 태스크(`rxTask`, `cmdTask`)를 코어 0에 고정하여, 코어 1의 메인 루프(AirMouse, UI, 센서 제어)의 지연을 원천 차단 |
| **비동기 명령 큐** | `request*()` 계열 API는 명령 큐에 인큐 후 즉시 반환되며, `cmdTask`가 순차 송신 및 ACK 대기/재전송 처리 |
| **자동 캘리브레이션** | 무인 공간에서 16개 게이트의 노이즈 플로어를 5분간 자동 수집하여 최적 임계값 도출 (ESPHome과 동일 공식) |
| **스레드 안전성** | 최신 측정 데이터(`_latest`)는 FreeRTOS 뮤텍스 보호, 상태 통지는 EventGroup 비트 플래그로 동기화 |
| **ESPHome 호환** | 명령 코드, 레지스터 오프셋 매핑, 프레임 형식, 타이밍 규격이 ESPHome `ld2420`과 100% 동일 |
| **16게이트 개별 제어** | 각 0.7m 간격(최대 11.2m)의 16개 거리 게이트별 움직임(move) 및 정지(still) 감도 개별 설정 |

---

## 지원 하드웨어

| MCU | 지원 상태 | 비고 |
|---|---|---|
| **ESP32** (모든 클래식 변형) | ✅ 완전 지원 | 듀얼코어 FreeRTOS (Core 0 고정 최적화) |
| **ESP32-S3** | ✅ 완전 지원 | 듀얼코어 FreeRTOS 지원 |
| **ESP32-S2** | ✅ 지원 | 싱글코어 FreeRTOS |
| **ESP32-C3 / C6** | ✅ 지원 | RISC-V 싱글코어 FreeRTOS |
| Arduino AVR (Uno/Nano) | ❌ 미지원 | FreeRTOS 및 멀티태스킹 메모리 부족 |
| STM32 (FreeRTOS 구동 시) | ⚠️ 이식 가능 | FreeRTOS 및 HardwareSerial 인터페이스 호환 필요 |

### LD2420 펌웨어 호환성

| 펌웨어 버전 | 기본 통신속도 | 핀 연결 (TX/RX) |
|---|---|---|
| v1.5.2 이하 | 256000 bps | OT1 = TX, OT2 = RX |
| **v1.5.3 이상** | **115200 bps** | **OT1 = TX, RX = RX (배선 핀 명칭 변경됨)** |

> 본 라이브러리는 현재 출하되는 **펌웨어 v1.5.3 이상 (115200 bps)** 을 기본 표준으로 합니다.

---

## 설치

### PlatformIO

`platformio.ini`의 `lib_deps`에 추가:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_deps =
    https://github.com/yourname/LD2420.git
```

또는 로컬 라이브러리 경로 지정:
```ini
lib_extra_dirs = lib
```

### Arduino IDE

1. GitHub 저장소에서 `.zip` 파일 다운로드
2. Arduino IDE: `스케치 → 라이브러리 포함하기 → .ZIP 라이브러리 추가...`
3. ESP32 보드 패키지 버전이 최신인지 확인 (`툴 → 보드 → 보드 매니저`)

---

## 배선

| LD2420 핀 | ESP32 GPIO | 설명 |
|---|---|---|
| **VCC** | **3.3V** | ⚠️ **5V 직접 연결 금지** (센서 모듈 파손 위험) |
| **GND** | **GND** | 공통 그라운드 |
| **TX (OT1)** | **GPIO16 (RX2)** | 센서 송신 -> ESP32 수신 (하드웨어 UART2) |
| **RX** | **GPIO17 (TX2)** | 센서 수신 <- ESP32 송신 (하드웨어 UART2) |
| OUT (OT2) | (선택) GPIO18 | 인체 존재 시 HIGH 출력되는 하드웨어 디지털 핀 |

```
        LD2420 모듈                     ESP32 개발보드
       ┌───────────┐                  ┌──────────────┐
       │   VCC     ├─── 3.3V ─────────┤ 3V3          │
       │   GND     ├─── GND ──────────┤ GND          │
       │   TX(OT1) ├──────────────────┤ GPIO16 (RX2) │
       │   RX      ├──────────────────┤ GPIO17 (TX2) │
       │   OUT(선택)├─────────────────┤ GPIO18       │
       └───────────┘                  └──────────────┘
```

> **주의**: 반드시 하드웨어 UART2(`HardwareSerial(2)`)를 사용하세요. SoftwareSerial은 115200bps 통신 시 프레임 손실이 발생할 수 있습니다.

---

## 빠른 시작

```cpp
#include <LD2420.h>

// ESP32 하드웨어 UART2 사용
HardwareSerial ldSerial(2);
LD2420 radar(ldSerial, /*rxPin=*/16, /*txPin=*/17, /*baud=*/115200);

void setup() {
    Serial.begin(115200);

    // FreeRTOS 백그라운드 태스크(Core 0) 생성 및 센서 통신 시작
    if (!radar.begin()) {
        Serial.println("LD2420 초기화 실패! 배선을 확인하세요.");
        while (1) delay(1000);
    }
    Serial.println("LD2420 정상 가동 시작!");
}

void loop() {
    // 논블로킹 최신 감지 데이터 조회
    if (radar.isPresent()) {
        Serial.printf("인체 감지됨! 거리: %d cm\n", radar.getDistance());
    } else {
        Serial.println("인체 없음");
    }
    delay(500);
}
```

---

## API 레퍼런스

### 1. 생성자 및 수명주기 초기화

```cpp
// 생성자: 사용할 시리얼 포트, 핀, 통신속도 설정
LD2420(HardwareSerial &serial, uint8_t rxPin, uint8_t txPin, uint32_t baud = 115200);

// 라이브러리 시작: FreeRTOS 듀얼 태스크를 Core 0에 생성
bool begin(uint32_t rxStackSize = 4096, uint32_t cmdStackSize = 3072,
           UBaseType_t rxPriority = 5, UBaseType_t cmdPriority = 4);

// 라이브러리 종료: 태스크 안전 삭제 및 세마포어/큐 리소스 해제
void end();
```

### 2. 상태 조회 API (논블로킹, 즉시 반환)

| 메서드 | 반환 타입 | 설명 |
|---|---|---|
| `isReady()` | `bool` | 센서가 정상 감지 실행 모드(`RUN`)인지 여부 |
| `isPresent()` | `bool` | 인체 존재 감지 여부 (`true`: 감지됨, `false`: 부재) |
| `getDistance()` | `uint16_t` | 감지된 타겟의 거리 (단위: cm) |
| `getGateEnergy(gate)` | `uint16_t` | 특정 게이트(0~15)의 실시간 수신 신호 강도 |
| `getLatestData()` | `LD2420TargetData` | 뮤텍스 보호 하에 전체 타겟 데이터 스냅샷 복사 취득 |
| `state()` | `LD2420State` | 센서 메인 FSM 상태 (`IDLE`, `RUN`, `CONFIG`, `CALIBRATE` 등) |

### 3. 비동기 설정 명령 API (즉시 반환, Non-blocking)

호출 시 큐(`_cmdQueue`)에 적재된 후 즉시 반환되며, `cmdTask`가 순차적으로 센서에 바이너리 명령을 전송하고 ACK 응답을 처리합니다.

| 메서드 | 파라미터 | 설명 |
|---|---|---|
| `requestGateThreshold(g, move, still)` | 게이트(0~15), 이동감도, 정지감도 | 특정 게이트 1개의 임계값 설정 |
| `requestAllGateThresholds(cfg)` | `LD2420GateConfig` 구조체 | 16개 전체 게이트 임계값 일괄 설정 |
| `requestMinMaxDistance(min, max)` | 최소게이트, 최대게이트 | 센서의 물리적 감지 거리 범위 제한 |
| `requestTimeout(sec)` | 초 단위 시간 | 인체 부재(미감지) 판정 유지 지연 시간 설정 |
| `requestMode(mode)` | `LD2420Mode` enum | `SIMPLE`(텍스트) / `ENERGY`(바이너리 에너지) 모드 전환 |
| `requestFirmwareVersion()` | - | 센서 펌웨어 버전 정보 조회 요청 |
| `requestRestart()` | - | 센서 모듈 소프트웨어 리셋/재시작 |
| `requestConfigMode(enable)` | `bool` | 센서 설정 모드 진입(`true`) 또는 종료(`false`) |

### 4. 감도 프리셋 편의 함수

| 메서드 | 임계값 설정 내용 | 사용 권장 시나리오 |
|---|---|---|
| `requestDefaultThresholds()` | Gate 0~5: 10, Gate 6~15: 100 | 일반 거실/방 표준 설정 |
| `requestSensitiveThresholds()` | 모든 게이트(0~15): 5 | 수면실, 미세 호흡 감지, 정밀 감지 |
| `requestBalancedThresholds()` | Gate 0~7: 15, Gate 8~15: 50 | 복도 및 균형 잡힌 실내 환경 |

### 5. 자동 캘리브레이션 API

| 메서드 | 반환 타입 | 설명 |
|---|---|---|
| `startCalibration()` | `bool` | 비동기 캘리브레이션 시작 (ENERGY 모드 자동 전환, 약 5분 수집) |
| `applyCalibration()` | `bool` | 수집 완료 후 계산된 최적 임계값을 센서에 기록하고 SIMPLE 모드로 복귀 |
| `cancelCalibration()` | `void` | 진행 중인 캘리브레이션 중단 및 RUN 모드 복귀 |
| `calibrationProgress()` | `uint8_t` | 수집 진행률 (0 ~ 100%) |
| `calibrationState()` | `LD2420CalState` | 캘리브레이션 FSM 상태 (`COLLECTING`, `READY` 등) |
| `calibrationNoiseFloor(g)` | `uint16_t` | 특정 게이트의 측정된 평균 배경 노이즈 수치 |

### 6. 콜백 등록 API

```cpp
void onData(LD2420DataCallback cb, void *ctx = nullptr);
void onEvent(LD2420EventCallback cb, void *ctx = nullptr);
```
> ⚠️ **주의**: 콜백은 Core 0의 `rxTask` 내부에서 직접 실행되므로 `delay()`나 무거운 I/O를 수행하지 마세요.

---

## 게이트 시스템

LD2420은 거리를 **16개의 게이트(Gate 0 ~ 15)** 로 분할 관리하며, **1개 게이트는 약 0.7m**를 담당합니다. (최대 감지 범위 약 11.2m)

| 게이트 | 거리 범위 | 게이트 | 거리 범위 |
|:---:|:---:|:---:|:---:|
| **0** | 0.0m ~ 0.7m | **8** | 5.6m ~ 6.3m |
| **1** | 0.7m ~ 1.4m | **9** | 6.3m ~ 7.0m |
| **2** | 1.4m ~ 2.1m | **10** | 7.0m ~ 7.7m |
| **3** | 2.1m ~ 2.8m | **11** | 7.7m ~ 8.4m |
| **4** | 2.8m ~ 3.5m | **12** | 8.4m ~ 9.1m |
| **5** | 3.5m ~ 4.2m | **13** | 9.1m ~ 9.8m |
| **6** | 4.2m ~ 4.9m | **14** | 9.8m ~ 10.5m |
| **7** | 4.9m ~ 5.6m | **15** | 10.5m ~ 11.2m |

- **임계값 범위**: 0 ~ 100 (값이 **낮을수록 민감**, **높을수록 둔감**)
- **move_threshold**: 걷기, 팔 흔들기 등 이동 움직임 감지 민감도
- **still_threshold**: 착석, 수면, 호흡 등 미세 움직임 감지 민감도

---

## 캘리브레이션 원리

ESPHome 컴포넌트의 공식 수식을 적용하여 동작합니다:

1. `radar.startCalibration()` 실행 시 센서가 바이너리 에너지 모드로 전환
2. **5초 주기**로 16개 게이트의 신호 강도를 샘플링하여 총 **64개 샘플** 누적 (약 5.3분 소요)
3. 누적 합산 평균을 통해 각 게이트별 배경 노이즈 플로어(Noise Floor) 산출
4. 노이즈 기반 최적 임계값 자동 연산:
   - **이동 임계값 (move_threshold)** = `clamp(노이즈 × 5배, 10, 100)`
   - **정지 임계값 (still_threshold)** = `clamp(노이즈 × 3배, 5, 80)`
5. `radar.applyCalibration()` 호출 시 계산된 임계값을 센서에 기록하고 표준 모드로 복귀

---

## 예제 목록

모든 예제는 상세 한글 주석을 포함하며, 아두이노 IDE 표준에 맞는 독립 폴더 구조로 제공됩니다:

| # | 예제 폴더 | 주요 내용 | 소스 코드 |
|:---:|---|---|:---:|
| **01** | `01_BasicPresence` | 최소 사용법 (비동기 초기화, 논블로킹 재실/거리 폴링) | [`01_BasicPresence.ino`](examples/01_BasicPresence/01_BasicPresence.ino) |
| **02** | `02_MaxGateLimit` | 실제 방 크기(4m)에 맞춘 최대 거리 제한 및 벽면 반사파 오탐 차단 | [`02_MaxGateLimit.ino`](examples/02_MaxGateLimit/02_MaxGateLimit.ino) |
| **03** | `03_ZoneDetection` | 복도 특정 구간(2.8m~5.6m)만 선별 감지 및 근거리/원거리 동시 마스킹 | [`03_ZoneDetection.ino`](examples/03_ZoneDetection/03_ZoneDetection.ino) |
| **04** | `04_TieredSensitivity` | 거리별 3단계 차등 감도 (근거리 초고감도, 중거리 표준, 원거리 저감도) | [`04_TieredSensitivity.ino`](examples/04_TieredSensitivity/04_TieredSensitivity.ino) |
| **05** | `05_BathroomLightFan` | 스마트 욕실 조명(GPIO25)·환풍기(GPIO26) 릴레이 제어 및 30초 유지 타임아웃 | [`05_BathroomLightFan.ino`](examples/05_BathroomLightFan/05_BathroomLightFan.ino) |
| **06** | `06_CeilingFanFilter` | 천장 실링팬/에어컨 바람 흔들림 위치 게이트(5~7) 마스킹 필터 | [`06_CeilingFanFilter.ino`](examples/06_CeilingFanFilter/06_CeilingFanFilter.ino) |
| **07** | `07_AutoCalibration` | 완전 비동기 자동 캘리브레이션 (5분간 노이즈 수집 후 최적값 자동 적용) | [`07_AutoCalibration.ino`](examples/07_AutoCalibration/07_AutoCalibration.ino) |
| **08** | `08_ManualCalibration` | 시리얼 콘솔 대화형 실시간 수동 게이트 튜닝 및 설정 확인 도구 | [`08_ManualCalibration.ino`](examples/08_ManualCalibration/08_ManualCalibration.ino) |
| **09** | `09_FreeRTOS_MultiTask` | Core 0(통신)과 Core 1(텔레메트리, 고속 GPIO) 멀티태스크 아키텍처 | [`09_FreeRTOS_MultiTask.ino`](examples/09_FreeRTOS_MultiTask/09_FreeRTOS_MultiTask.ino) |
| **10** | `10_AllFeatures_Demo` | 모든 공개 API를 시리얼 숫자 메뉴로 테스트하는 통합 종합 데모 콘솔 | [`10_AllFeatures_Demo.ino`](examples/10_AllFeatures_Demo/10_AllFeatures_Demo.ino) |

더 자세한 사용 방법은 [`examples/README.md`](examples/README.md) 가이드를 참조하세요.

---

## FAQ

**Q. 센서가 전혀 감지하지 못합니다.**
- VCC 전원이 3.3V인지 확인하세요. (5V 인가 시 센서 손상 위험)
- TX/RX 핀이 교차(ESP32 RX -> 센서 TX, ESP32 TX -> 센서 RX) 연결되었는지 확인하세요.
- 센서 모듈의 펌웨어 보레이트를 확인하세요. (v1.5.3 이상은 115200bps, 구형은 256000bps)

**Q. 사람이 없는데도 자꾸 감지(오탐)됩니다.**
- 자동 캘리브레이션([예제 07](examples/07_AutoCalibration/07_AutoCalibration.ino))을 약 5분간 실행하여 환경 노이즈를 반영하세요.
- [예제 02](examples/02_MaxGateLimit/02_MaxGateLimit.ino)를 참고하여 방 크기에 맞게 `requestMinMaxDistance()`로 감지 범위를 줄이세요.
- 선풍기나 커튼이 있는 위치의 게이트는 [예제 06](examples/06_CeilingFanFilter/06_CeilingFanFilter.ino)처럼 임계값을 100으로 높이세요.

**Q. 의자에 앉아 있으면 인체 감지가 자꾸 꺼집니다.**
- 해당 거리에 위치한 게이트의 정지 감도(`still_threshold`)를 5~10 수준으로 낮추세요.
- `radar.requestTimeout(30)`을 호출하여 부재 유지 시간을 30초 이상으로 늘리세요.

**Q. Wi-Fi / BLE / 메인 루프와 함께 사용할 수 있나요?**
- 가능합니다. 본 라이브러리는 UART 통신 태스크를 Core 0에 고정시키므로, Core 1에서 실행되는 Wi-Fi, BLE, 마우스/키보드 HID 태스크의 실시간성을 해치지 않습니다. ([예제 09](examples/09_FreeRTOS_MultiTask/09_FreeRTOS_MultiTask.ino) 참조)

---

## 라이선스 및 참고 자료

- **라이선스**: MIT License ([LICENSE](LICENSE) 참조)
- **참고 문서**:
  - [ESPHome LD2420 공식 컴포넌트 문서](https://esphome.io/components/sensor/ld2420.html)
  - [HLK-LD2420 하이링크 제조사 공식 페이지](https://www.hlktech.net/)
  - [LD2420 프로토콜 역공학 자료 (JoaoSandrini)](https://github.com/JoaoSandrini/ld2420-radar)
