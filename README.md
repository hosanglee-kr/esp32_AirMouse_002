# 📝 AirMouse Elite S3 기술 사양서 (Updated Technical Specification)

본 프로젝트는 **ESP32-S3 + MPU6050** 조합을 기반으로,  
일반 센서 마우스의 한계를 넘어 **프리젠테이션·거실 PC·HTPC·스마트TV** 환경까지 고려한  
**하이엔드 오픈소스 에어마우스 플랫폼**입니다.

---

## 🚀 핵심 기능 (Key Features)

### 1️⃣ 하이엔드 물리 엔진 (Advanced Motion Engine)

단순 자이로 → 좌표 변환이 아닌 **사람 손 움직임을 해석하는 물리 엔진**을 적용했습니다.

- **상보 필터(Complementary Filter) 기반 기울기 보정**
  - 마우스를 비스듬히 잡아도 화면 좌표는 항상 수평/수직 유지
  - ±45° 이상에서도 자연스러운 조작감

- **시그모이드(Sigmoid) 가변 가속도**
  - 미세 움직임: 픽셀 단위 정밀 제어
  - 빠른 스윙: 자연스러운 가속
  - Windows / macOS 커서 가속 감각과 유사

- **Click-Lock (완전 고정 옵션)**
  - 클릭 순간 발생하는 반동·떨림 완전 차단
  - 150ms 동안 커서 이동량을 **0으로 강제 고정**
  - 정밀 드래그·프리젠터 포인팅 최적화

- **적응형 LPF (Adaptive Low Pass Filter)**
  - 정지 상태: 떨림 억제
  - 빠른 이동: 지연 최소화

---

### 2️⃣ Gyro 오프셋 자동 캘리브레이션

- 전원 인가 후 약 **1초간 자동 캘리브레이션**
- 평균 기반 Gyro bias 계산
- **움직임이 큰 샘플 자동 제외**
  - 손에 들고 전원을 켜도 오차 최소화
- 장시간 사용 시 드리프트 현저히 감소

---

### 3️⃣ 멀티코어 RTOS 아키텍처

ESP32-S3 듀얼 코어 구조를 적극 활용

| Core | Task | 역할 |
|----|----|----|
| Core 1 | Sensor Task | MPU6050 125Hz 샘플링 + 물리 엔진 계산 |
| Core 0 | Comm Task | BLE Composite HID 전송 |

✔ 센서 지터 제거  
✔ BLE 스택 부하로 인한 커서 끊김 방지  

---

### 4️⃣ 사용자 UX 기능

- **실시간 DPI 전환**
  - 버튼 Short Press로 3단계 감도 변경
  - (저속 / 표준 / 고속)

- **스크롤 전용 버튼(BTN_SCROLL)**
  - 커서 이동과 완전 분리
  - 버튼을 누른 동안만 스크롤 동작
  - 스크롤 중 커서 이동 자동 억제

- **PPT 제스처 모드**
  - Gyro Flick 제스처
    - 좌/우 슬라이드 이동
  - 스크롤 버튼 누를 때는 제스처 자동 차단 (오동작 방지)

---

## 📊 기술 스펙 (Technical Specifications)

| 항목 | 사양 |
|----|----|
| MCU | ESP32-S3 (Dual-Core, 240MHz) |
| Sensor | MPU6050 (6-axis IMU) |
| Connectivity | BLE 5.0 |
| HID | Composite HID (Mouse + Keyboard) |
| Polling Rate | 125Hz (8ms) |
| DPI | 3단계 가변 (소프트웨어 가속 포함) |
| Calibration | 자동 Gyro bias 캘리브레이션 |
| Scroll | Gyro 기반 스크롤 |
| OS | Windows / macOS / Linux / Android / iOS |
| Driver | 불필요 (표준 HID) |

---

## 🔌 하드웨어 결선도 & 핀맵

### 📐 기본 결선 구조
ESP32-S3        MPU6050
3V3   --------> VCC 
GND   --------> GND 
GPIO4 --------> SDA 
GPIO5 --------> SCL

---

### 📍 GPIO 핀맵 표

| 기능 | GPIO | 설명 |
|----|----|----|
| I2C SDA | GPIO 4 | MPU6050 데이터 |
| I2C SCL | GPIO 5 | MPU6050 클럭 |
| BTN_L | GPIO 12 | 좌클릭 |
| BTN_MODE | GPIO 13 | DPI / PPT 모드 |
| BTN_SCROLL | GPIO 14 | 스크롤 전용 버튼 |

> ⚠️ GPIO 번호는 DevKitC 기준 예시이며, 실제 보드에 맞게 변경 가능

---

## 🖱️ 에어마우스 사용법 (User Guide)

### 1️⃣ 기본 사용
- 전원 인가 → 자동 BLE 연결
- 손을 움직이면 커서 이동
- BTN_L 클릭 = 마우스 좌클릭

---

### 2️⃣ DPI 변경
- **BTN_MODE 짧게 누르기**
  - 저속 → 표준 → 고속 → 반복
- 해상도 높은 화면에서는 고속 권장

---

### 3️⃣ 스크롤 사용
- **BTN_SCROLL 누른 상태에서 위/아래 기울이기**
- 커서 이동 없이 스크롤만 동작
- 웹, PDF, PPT 슬라이드 탐색에 최적

---

### 4️⃣ PPT 모드
- **BTN_MODE 길게 누르기(1초)**
  - PPT 제스처 모드 ON/OFF
- 좌우로 빠르게 휘두르기(Flick)
  - 다음/이전 슬라이드

---

### 5️⃣ Click-Lock 활용
- 클릭 순간 커서가 완전히 고정됨
- 버튼 클릭 시 포인터가 흔들리지 않음
- 프리젠터 레이저 대용으로 적합

---

## ⚠️ 주의사항

- 부팅 직후 **1초간 정지 상태 유지** 권장 (캘리브레이션 정확도 향상)
- 스크롤 반응이 과하면 코드 상수 튜닝 가능
- PC 환경에 따라 휠 방향이 반대일 경우  
  `mouseMove(x,y,scrollX,scrollY)` 인자 순서 변경 가능

---

## 🎯 권장 활용 시나리오

- 프리젠테이션 리모컨
- 거실 PC / HTPC
- 스마트TV 입력 장치
- 산업용 HMI 무선 포인팅
- 커스텀 게임 컨트롤러 확장

---

✅ **AirMouse Elite S3는 단순 프로젝트가 아닌,  
상용급 입력 디바이스 아키텍처를 목표로 설계되었습니다.**