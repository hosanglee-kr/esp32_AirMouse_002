# 🛠️ DIY High-End Presenter Project Specification

ESP32-S3-Zero와 MPU6050, PSP1000 조이스틱을 결합한 지능형 프리젠터 설계 사양서입니다.

---

## 1. 하드웨어 주요 사양 (H/W Specs)

| 항목 | 상세 내용 |
|:---:|---|
| **MCU** | ESP32-S3-Zero (Dual-Core, WiFi/BLE 5.0, Native USB) |
| **Sensor** | MPU6050 (6축 가속도/자이로 센서) |
| **Pointing** | PSP1000 아날로그 조이스틱 (X, Y 좌표 이동) |
| **Laser** | 650nm Red Laser Module (물리 버튼 직결 제어) |
| **Buttons** | Micro Switch x 6 (레이저 전용 1 + 기능 5) |
| **Storage** | LittleFS (설정값 JSON 저장용) |

---

## 2. 입력 장치 및 기능 매핑 (Input Mapping)

### A. 버튼 배치 및 기능
| 버튼 위치 | 동작 | 기능 (Default) | 비고 |
|:---:|:---:|---|---|
| **우측 상단** | 누름 유지 | **H/W 레이저 포인터 ON** | 물리적 레이저 점등 |
| **중앙 [상]** | 누름 유지 | **에어 마우스 활성화** | MPU6050 기반 이동 |
| | 짧게 클릭 | **마우스 왼쪽 클릭** | - |
| **중앙 [중]** | 짧게 클릭 | **Page Down** | 다음 슬라이드 |
| | 길게 누름 | **Shift + F5** | 슬라이드 쇼 시작 |
| **중앙 [하]** | 짧게 클릭 | **Page Up** | 이전 슬라이드 |
| | 길게 누름 | **Esc** | 슬라이드 쇼 종료 |
| **하단 [좌]** | 짧게 클릭 | **B (Blackout)** | 화면 블랙아웃 |
| **하단 [우]** | 짧게 클릭 | **Ctrl + L** | S/W 레이저 포인터 활성화 |

### B. 조이스틱 및 제스처
- **기본 동작:** 아날로그 스틱 이동량에 비례하여 **마우스 커서 좌표 이동** (가속도 알고리즘 적용).
- **조합 제스처:** **중앙 [상] 버튼을 누른 상태**에서 조이스틱을 특정 방향으로 튕기기
  - **Up:** `Ctrl + P` (펜 도구 변경)
  - **Down:** `Ctrl + I` (형광펜 도구 변경)
  - **Left/Right:** 사용자 커스텀 가능 (예: 볼륨 조절)

---

## 3. 소프트웨어 주요 기능

### 🌐 Web 커스터마이징 (ESPAsyncWebServer)
- **접속 방식:** ESP32가 생성한 AP 모드 또는 공유기 IP 접속.
- **주요 기능:**
  - 각 버튼별 단축키(Keycode) 실시간 변경.
  - 조이스틱/자이로 감도(Sensitivity) 및 데드존(Deadzone) 미세 조정.
  - **ArduinoJson V7.4.x**를 사용하여 설정값을 `config.json`으로 관리.

### 🖱️ 하이브리드 포인팅 시스템
- **정밀 모드:** 조이스틱을 이용한 픽셀 단위 커서 제어.
- **에어 모드:** MPU6050 자이로 센서를 이용한 공간 포인팅 제어.

---

## 4. 핀 맵 구성 (Pin Assignment - ESP32-S3-Zero)

| 부품 | 핀 번호 | 인터페이스 |
|:---:|:---:|:---:|
| **MPU6050** | GPIO 7(SDA), 8(SCL) | I2C |
| **Joystick** | GPIO 1(X), 2(Y) | Analog |
| **Laser** | GPIO 3 | Digital Out |
| **SW1~6** | GPIO 4, 5, 6, 9, 10, 11 | Digital Input (Pull-up) |

---

> **Note:** 본 사양은 `PlatformIO` 환경에서 `Arduino Core`를 사용하여 개발하며, 배터리 효율을 위해 블루투스 저전력(BLE) 스택을 최적화하여 사용합니다.
