# LD2420 Arduino Library

[![Arduino Library](https://img.shields.io/badge/Arduino-Library-blue)](https://www.arduino.cc/reference/en/libraries/)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-Compatible-orange)](https://platformio.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![ESP32](https://img.shields.io/badge/ESP32-FreeRTOS-red)]()

**HLK-LD2420 24GHz FMCW 레이더 센서**를 위한 완전한 객체지향 Arduino 라이브러리.
ESPHome의 LD2420 컴포넌트 아키텍처를 Arduino에 맞게 재구현하여, **비동기 논블로킹**, **FreeRTOS 태스크 기반**, **자동 캘리브레이션**을 지원합니다.

---

## 목차

- [주요 특징](#주요-특징)
- [지원 하드웨어](#지원-하드웨어)
- [설치](#설치)
- [배선](#배선)
- [빠른 시작](#빠른-시작)
- [API 레퍼런스](#api-레퍼런스)
- [게이트 시스템](#게이트-시스템)
- [캘리브레이션](#캘리브레이션)
- [예제 목록](#예제-목록)
- [FAQ](#faq)
- [라이선스](#라이선스)

---

## 주요 특징

| 특징 | 설명 |
|---|---|
| **완전 논블로킹** | `delay()`·`while` 대기 없음. 모든 대기는 상태 머신 또는 FreeRTOS 큐로 처리 |
| **객체지향 설계** | `LD2420`(파사드), `LD2420Protocol`(프레임), `LD2420Calibration`(캘리브레이션) 분리 |
| **FreeRTOS 지원** | rxTask/cmdTask를 코어 0에 고정, 사용자 loopTask는 코어 1에서 자유 |
| **비동기 API** | `request*()` 계열은 즉시 반환, 명령 큐가 순차 처리 |
| **자동 캘리브레이션** | 노이즈 플로어 수집 → 임계값 자동 산출 (ESPHome과 동일 알고리즘) |
| **스레드 안전** | 뮤텍스 + FreeRTOS Queue + EventGroup |
| **ESPHome 호환** | 명령 코드, 프레임 형식, 상태 머신, 타임아웃이 ESPHome LD2420과 동일 |
| **16게이트 제어** | 게이트별 move/still 임계값 개별 설정 |

---

## 지원 하드웨어

| MCU | 상태 |
|---|---|
| **ESP32** (모든 변형) | ✅ 완전 지원 (듀얼코어 FreeRTOS) |
| ESP32-S2 | ✅ 지원 |
| ESP32-S3 | ✅ 지원 |
| ESP32-C3 | ✅ 지원 |
| ESP32-C6 | ✅ 지원 |
| Arduino AVR (Uno/Nano) | ❌ 미지원 (FreeRTOS 없음) |
| STM32 (FreeRTOS 사용 시) | ⚠️ 이식 가능 |

### LD2420 펌웨어 호환성

| 펌웨어 | 보레이트 | TX/RX 핀 |
|---|---|---|
| v1.5.2 이하 | 256000 | OT1=TX, OT2=RX |
| **v1.5.3 이상** | **115200** | **OT1=TX, RX=RX (교체됨)** |

> 이 라이브러리는 **펌웨어 v1.5.3 이상**을 기준으로 합니다.

---

## 설치

### Arduino IDE

1. **ZIP 다운로드**: GitHub → Code → Download ZIP
2. **Arduino IDE**: `스케치 → 라이브러리 포함하기 → .ZIP 라이브러리 추가`
3. **ESP32 보드 패키지**: `툴 → 보드 → 보드 매니저`에서 `esp32` 검색 후 설치

### PlatformIO

`platformio.ini`에 추가:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_deps =
    https://github.com/yourname/LD2420.git
```

또는 로컬 라이브러리로:

```ini
lib_extra_dirs = ../libraries
```

### 수동 설치

```
~/Arduino/libraries/LD2420/
├── src/
├── examples/
├── library.properties
└── ...
```

---

## 배선

| LD2420 | ESP32 | 비고 |
|---|---|---|
| VCC | 3.3V | **5V 금지 (모듈 손상)** |
| GND | GND | |
| TX (OT1) | GPIO16 (RX2) | 펌웨어 v1.5.3+ |
| RX | GPIO17 (TX2) | 펌웨어 v1.5.3+ |
| OUT (OT2) | (선택) GPIO18 | 존재 시 HIGH |

```
        LD2420              ESP32
       ┌──────┐            ┌──────┐
       │ VCC  ├───3.3V─────┤3V3   │
       │ GND  ├───GND──────┤GND   │
       │ TX   ├────────────┤16 RX2│
       │ RX   ├────────────┤17 TX2│
       │ OUT  ├──(선택)────┤18    │
       └──────┘            └──────┘
```

> **주의**: 하드웨어 UART2 사용. 소프트웨어 시리얼은 115200bps에서 불안정.

---

## 빠른 시작

```cpp
#include <LD2420.h>

HardwareSerial ldSerial(2);
LD2420 radar(ldSerial, /*rx=*/16, /*tx=*/17, /*baud=*/115200);

void setup() {
  Serial.begin(115200);
  radar.begin();     // FreeRTOS 태스크 생성, 즉시 반환
}

void loop() {
  // 논블로킹. 최신값만 조회.
  if (radar.isPresent()) {
    Serial.printf("Present @ %d cm\n", radar.getDistance());
  }
  delay(500);
}
```

**캘리브레이션 예**:

```cpp
void setup() {
  Serial.begin(115200);
  radar.begin();
  radar.startCalibration();   // 30초간 노이즈 수집 (비동기)
}

void loop() {
  if (radar.calibrationState() == LD2420CalState::READY) {
    radar.applyCalibration();  // 임계값 자동 적용
  }
  delay(200);
}
```

---

## API 레퍼런스

### 생성자

```cpp
LD2420(HardwareSerial &serial, uint8_t rxPin, uint8_t txPin, uint32_t baud = 115200);
```

### 초기화

| 메서드 | 설명 |
|---|---|
| `bool begin(...)` | FreeRTOS 태스크 생성 + 초기 상태 진입. 즉시 반환 |
| `void end()` | 태스크 종료 및 리소스 해제 |

### 상태 조회 (논블로킹, 즉시 반환)

| 메서드 | 반환 | 설명 |
|---|---|---|
| `bool isReady()` | bool | RUN 상태 여부 |
| `bool isPresent()` | bool | 인체 존재 |
| `uint16_t getDistance()` | cm | 타겟 거리 |
| `uint16_t getGateEnergy(uint8_t gate)` | 0~ | 게이트별 에너지 |
| `LD2420TargetData getLatestData()` | struct | 전체 스냅샷 (뮤텍스 보호) |
| `LD2420State state()` | enum | 현재 상태 |

### 비동기 설정 명령 (즉시 반환)

| 메서드 | 설명 |
|---|---|
| `bool requestGateThreshold(g, move, still)` | 개별 게이트 임계값 |
| `bool requestAllGateThresholds(cfg)` | 전체 게이트 설정 |
| `bool requestMinMaxDistance(min, max)` | 감지 범위 제한 |
| `bool requestTimeout(sec)` | 감지 유지 시간 |
| `bool requestMode(LD2420Mode)` | SIMPLE/ENERGY/DEBUG |
| `bool requestFirmwareVersion()` | 펌웨어 버전 조회 |
| `bool requestRestart()` | 모듈 재시작 |
| `bool requestConfigMode(bool)` | 설정 모드 진입/종료 |

### 프리셋

| 메서드 | 설명 |
|---|---|
| `requestDefaultThresholds()` | 게이트 0~5: 10, 6~15: 100 |
| `requestSensitiveThresholds()` | 모든 게이트: 5 |
| `requestBalancedThresholds()` | 0~7: 15, 8~15: 50 |

### 캘리브레이션

| 메서드 | 설명 |
|---|---|
| `bool startCalibration()` | 캘리브레이션 시작 (비동기) |
| `bool applyCalibration()` | 계산된 임계값 적용 |
| `void cancelCalibration()` | 취소 |
| `uint8_t calibrationProgress()` | 0~100% |
| `LD2420CalState calibrationState()` | IDLE/COLLECTING/READY/APPLIED |
| `uint16_t calibrationNoiseFloor(gate)` | 게이트별 노이즈 |

### 콜백

```cpp
void onData(LD2420DataCallback cb, void *ctx = nullptr);
void onEvent(LD2420EventCallback cb, void *ctx = nullptr);
```

rxTask에서 직접 호출되므로 **가벼운 처리만** 수행할 것.

---

## 게이트 시스템

LD2420은 **16개의 감지 게이트(0~15)** 를 가지며, 각 게이트는 **약 0.7m** 담당:

| 게이트 | 거리 | 게이트 | 거리 |
|---|---|---|---|
| 0 | 0.0~0.7m | 8 | 5.6~6.3m |
| 1 | 0.7~1.4m | 9 | 6.3~7.0m |
| 2 | 1.4~2.1m | 10 | 7.0~7.7m |
| 3 | 2.1~2.8m | 11 | 7.7~8.4m |
| 4 | 2.8~3.5m | 12 | 8.4~9.1m |
| 5 | 3.5~4.2m | 13 | 9.1~9.8m |
| 6 | 4.2~4.9m | 14 | 9.8~10.5m |
| 7 | 4.9~5.6m | 15 | 10.5~11.2m |

### 임계값

- **값 범위**: 0~100
- **낮을수록 민감**, 높을수록 둔감
- **move_threshold**: 움직임 감지 민감도
- **still_threshold**: 정지 인체(호흡 등) 감지 민감도

### 기본값

```
Gate 0~5  : move=10,  still=10  (민감)
Gate 6~15 : move=100, still=100 (둔감)
```

---

## 캘리브레이션

ESPHome의 캘리브레이션 모드와 동일한 원리:

1. **에너지 출력 모드**로 전환
2. **5초마다** 게이트별 에너지 레벨 수집
3. **64 샘플** 누적 (약 5분)
4. 노이즈 플로어 기반 임계값 계산:
   - **트리거(move)** = 노이즈 × **5** (10~100 clamp)
   - **유지(still)** = 노이즈 × **3** (5~80 clamp)
5. **Simple 모드**로 복귀 + 임계값 적용

### 캘리브레이션 환경

- **사람이 없는 상태** 유지 필수
- 움직이는 물체, 가구 이동 금지
- 전자레인지·Wi-Fi 공진기 근처라면 60초 이상 권장

---

## 예제 목록

| # | 예제 | 설명 |
|---|---|---|
| 01 | BasicPresence | 최소 사용법 |
| 02 | MaxGateLimit | 벽·가구 오탐 방지 |
| 03 | ZoneDetection | 특정 구역만 감지 |
| 04 | TieredSensitivity | 거리별 감도 차등 |
| 05 | BathroomLightFan | 진입+재실 조명/환풍기 |
| 06 | CeilingFanFilter | 천장 선풍기 오탐 필터 |
| 07 | AutoCalibration | 자동 캘리브레이션 |
| 08 | ManualCalibration | 시리얼 수동 조정 |
| 09 | FreeRTOS_MultiTask | 태스크 분리 |
| 10 | AllFeatures_Demo | 전체 기능 시연 |

자세한 내용은 [`examples/README.md`](examples/README.md) 참조.

---

## FAQ

**Q. 감지가 안 됩니다.**
- 3.3V 전원인지 확인 (5V는 손상 위험)
- 펌웨어 버전 확인 (v1.5.3 미만은 256000 baud)
- TX/RX 핀 교차 확인

**Q. 오탐이 많습니다.**
- 자동 캘리브레이션 실행 (`startCalibration()` → `applyCalibration()`)
- `requestMinMaxDistance()`로 범위 제한
- 문제 게이트 임계값 상향 (예: 천장 선풍기 게이트 = 100)

**Q. 정지 인체(앉아 있음)가 감지 안 됩니다.**
- 해당 거리 게이트의 `still_threshold` 낮춤 (5~10)
- `requestTimeout()`을 30초 이상으로 설정

**Q. Wi-Fi/MQTT와 함께 쓰려면?**
- 이 라이브러리는 완전 논블로킹이므로 병행 가능
- 예제 09 참조 (FreeRTOS 멀티태스크)

**Q. ESPHome과 동시 사용 가능한가요?**
- 아니요. ESPHome의 LD2420 컴포넌트와 이 라이브러리는 UART를 공유할 수 없습니다.
- ESPHome 사용 시 ESPHome의 `ld2420` 컴포넌트를 사용하세요.

---

## 라이선스

MIT License. [LICENSE](LICENSE) 참조.

## 참고 자료

- [ESPHome LD2420 문서](https://esphome.io/components/sensor/ld2420.html)
- [HLK-LD2420 제조사](https://www.hlktech.net/)
- [프로토콜 번역 PDF (JoaoSandrini)](https://github.com/JoaoSandrini/ld2420-radar)
- [ESP32-C3 완성 프로젝트 (kiwironz)](https://github.com/kiwironz/esp32c3-ld2420-radar)

## 기여

이슈 및 PR 환영합니다. 버그 리포트 시 다음 정보를 포함해주세요:
- ESP32 보드 종류
- LD2420 펌웨어 버전
- 배선도
- 시리얼 로그
