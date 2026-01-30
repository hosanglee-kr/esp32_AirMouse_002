# AirMouse Elite S3 (v0.1.0) — Release Package

ESP32-S3 + MPU6050 기반 **에어마우스 + 프리젠터** 프로젝트입니다.  
BLE **Composite HID(Mouse + Keyboard)** 로 동작하며, 드리프트 억제를 위한 **Gyro 자동 캘리브레이션**,  
UX 충돌을 없앤 **스크롤 전용 버튼**, 클릭 순간 커서 흔들림을 없애는 **Click-Lock(완전 고정)**을 포함합니다.

> ✅ Joystick(PSP1000) 기능은 **최후순위**이며, v0.1.0에서는 **유무 설정(빌드 플래그)만 제공**하고 실제 포인팅은 미구현입니다.

---

## 1) Key Features

- **Air Mouse (MPU6050 Gyro)**
  - 기울기 보정(Complementary Filter)
  - Sigmoid 기반 가변 가속
  - Adaptive LPF(정지 떨림 억제 / 이동 반응성 강화)

- **Gyro Bias Auto Calibration**
  - 부팅 후 약 **1초 동안 평균**
  - **움직임 큰 샘플 제외**
  - 샘플 부족 시 fallback(짧은 구간 완화 평균)

- **Scroll Mode (BTN_SCROLL 전용)**
  - 스크롤 버튼을 누르는 동안만 휠 동작
  - 스크롤 중 커서 이동 억제(UX 충돌 제거)

- **Click-Lock (Hard Lock Option)**
  - 클릭 후 150ms 동안 outX/outY = 0 (완전 고정)

- **Dual-Core FreeRTOS**
  - Core 1: Sensor + Motion (125Hz)
  - Core 0: BLE Comm

---

## 2) Software Architecture

### Task Split
- **E10_Sensor (Core 1)**
  - MPU6050 읽기(8ms)
  - 캘리브/모션엔진/제스처/스크롤 계산
  - 공유 상태 업데이트(뮤텍스)

- **E10_Comm (Core 0)**
  - 공유 상태를 HID report로 전송

### Data Flow
1) MPU6050 read  
2) Gyro bias 제거  
3) Orientation update (roll)  
4) MotionEngine.process() → tx,ty  
5) DPI 스케일 + 가속  
6) (ScrollMode) wheel 생성 + cursor damp  
7) HID report send (mouseMove / keyPress)

---

## 3) Hardware Wiring

### 3-1) MPU6050 I2C 결선(기본)

- ESP32-S3        MPU6050
- 3V3   --------> VCC 
- GND   --------> GND 
- GPIO4 --------> SDA 
- GPIO5 --------> SCL

> I2C는 400kHz로 동작합니다(Wire.setClock(400000))

---

## 4) Pin Map (Default: ESP32-S3 DevKitC Example)

> ⚠️ GPIO는 예시이며, 실제 하드웨어에 맞게 수정하세요.

| 기능 | 핀 | 설명 |
|---|---:|---|
| I2C SDA | GPIO4 | MPU6050 SDA |
| I2C SCL | GPIO5 | MPU6050 SCL |
| BTN_L | GPIO12 | 좌클릭 |
| BTN_MODE | GPIO13 | 짧게=DPI 변경 / 길게=PPT 토글 |
| BTN_SCROLL | GPIO14 | 스크롤 전용(누르는 동안만) |

### 4-1) (옵션) Joystick 핀(후순위)
- v0.1.0: **유무 설정만 제공**, 기능 미구현
- E10_HAS_JOYSTICK=1일 때만 핀 정의 포함

| 기능 | 핀(예시) | 비고 |
|---|---:|---|
| JOY_X | GPIO1 | ⚠ ADC 지원핀인지 보드별 확인 필요 |
| JOY_Y | GPIO2 | ⚠ ADC 지원핀인지 보드별 확인 필요 |

---

## 5) Usage Guide

### 5-1) 전원 ON & 초기 캘리브레이션
- 부팅 직후 **약 1초간** 캘리브레이션 수행
- 가능하면 **평평한 곳에 놓고** 켜면 가장 안정적
- 손에 들고 켜도 “움직임 샘플 제외 + fallback” 로 최대한 안정화

### 5-2) 기본 커서 이동
- 손 움직임(자이로 회전)에 따라 커서 이동

### 5-3) 좌클릭
- **BTN_L** 누르면 좌클릭(Press/Release)

### 5-4) DPI 변경
- **BTN_MODE 짧게 클릭**
- 1 → 2 → 3 → 1 순환

### 5-5) PPT 모드 토글
- **BTN_MODE 길게(1초 이상)**
- PPT 모드 ON/OFF

### 5-6) PPT 제스처 (Flick)
- PPT 모드 ON일 때:
  - z축 회전 속도 기반 Flick
  - 오른쪽 휘두름 → NEXT(PageDown)
  - 왼쪽 휘두름 → PREV(PageUp)
- **스크롤 버튼 누르는 동안은 제스처 차단**(오동작 방지)

### 5-7) 스크롤 모드
- **BTN_SCROLL 누르는 동안**
  - gyro.y 기반 wheel 생성
  - 스크롤 중 커서 이동 억제(0.25배)

---

## 6) Build (PlatformIO)

### 6-1) platformio.ini 핵심 예시
```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino

monitor_speed = 115200
monitor_filters = esp32_exception_decoder

lib_archive = yes
lib_ldf_mode = chain+
build_unflags = -std=gnu++11
build_flags =
  -std=gnu++17
  -D ARDUINO_USB_MODE=1
  -D ARDUINO_USB_CDC_ON_BOOT=1
  -D CONFIG_BT_NIMBLE_ENABLED=1
  -D CONFIG_BT_BLE_ENABLED=1

  ; Joystick 옵션 (v0.1.0: 스텁만)
  -D E10_HAS_JOYSTICK=0

lib_deps =
  adafruit/Adafruit MPU6050@^2.2.6
  h2zero/NimBLE-Arduino @ ^2.3.7
  https://github.com/Mystfit/ESP32-BLE-CompositeHID.git
  