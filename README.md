# 📝 AirMouse Elite S3 기술 사양서 (Technical Specification)

본 프로젝트는 **ESP32-S3**와 **MPU6050**을 결합하여 일반적인 센서 마우스의 한계를 극복하고, 상용 하이엔드 제품 수준의 부드러움과 정밀도를 구현한 오픈소스 에어마우스 솔루션입니다.

---

## 🚀 핵심 기능 (Key Features)

### 1. 하이엔드 물리 엔진 (Advanced Motion Engine)
단순한 좌표 변환을 넘어 인간의 미세한 움직임을 해석하는 4대 알고리즘이 적용되었습니다.

* **상보 필터(Complementary Filter) 기반 기울기 보정**: 마우스를 쥐는 각도(Roll)에 상관없이 사용자가 체감하는 수평/수직 방향으로 커서를 정확히 이동시킵니다.
* **시그모이드(Sigmoid) 가변 가속도**: 윈도우/macOS의 커서 조작감과 유사하게, 미세한 움직임은 정밀하게(Pixel-perfect), 빠른 움직임은 시원하게 가속됩니다.
* **지능형 클릭 안정화(Click-Lock)**: 클릭 순간 발생하는 손가락의 반동과 떨림을 감지하여 150ms 동안 커서를 고정함으로써 미스클릭을 방지합니다.
* **적응형 LPF(Adaptive Low Pass Filter)**: 정지 상태에서는 떨림을 억제하고, 빠른 이동 시에는 지연 시간(Latency)을 최소화하도록 필터 계수를 실시간 변경합니다.



### 2. 멀티코어 RTOS 아키텍처
ESP32-S3의 듀얼 코어를 활용하여 통신과 연산을 분리했습니다.

* **Core 1 (Sensor Task)**: 125Hz(8ms) 주기로 센서 데이터를 읽고 물리 엔진을 계산하여 연산 지터(Jitter)를 제거합니다.
* **Core 0 (Comm Task)**: 블루투스 HID 스택을 관리하며 데이터 패킷을 비동기 전송하여 통신 부하가 조작감에 영향을 주지 않도록 설계되었습니다.

### 3. 사용자 편의 기능 (UX)
* **실시간 DPI 전환**: 전용 버튼을 통해 3단계 감도(저속/표준/고속)를 즉시 변경할 수 있습니다.
* **지능형 스크롤 모드**: 스크롤 버튼을 누른 상태에서 마우스를 상하로 움직이면 화면 스크롤로 동작합니다.
* **제스처 인식**: 순간적인 가속(Flick)을 감지하여 브라우저 뒤로 가기 등의 명령을 수행할 수 있습니다.

---

## 📊 기술 스펙 (Technical Specifications)

| 구분 | 상세 사양 | 비고 |
| :--- | :--- | :--- |
| **MCU** | ESP32-S3 (Dual-Core, 240MHz) | Xtensa® 32-bit LX7 |
| **Sensor** | MPU6050 (6-Axis IMU) | Accelerometer + Gyroscope |
| **Connectivity** | Bluetooth Low Energy (BLE) 5.0 | HID Profile 지원 |
| **Polling Rate** | 125Hz (8ms) | 상용 유선 마우스 표준 주사율 |
| **DPI Range** | 3단계 가변 (1200 / 2400 / 3600 DPI 상당) | 소프트웨어 가속 포함 |
| **Axis Compensation** | ±45도 기울기 보정 지원 | 상보 필터 알고리즘 |
| **Algorithm** | Adaptive EMA + Sigmoid Acceleration | 자체 개발 물리 엔진 |
| **OS Compatibility** | Windows, macOS, Android, iOS, Linux | 별도 드라이버 불필요 |

---

## 🛠 시스템 아키텍처 (Software Architecture)

### 클래스 설계 (OOP)
1.  **`AdvancedMotionProcessor`**: 수학적 모델링 담당. 기울기 보정, 가속도 곡선, 필터링 등 모든 물리 연산이 캡슐화되어 있습니다.
2.  **`EliteAirMouse`**: 하드웨어 추상화 계층. MPU6050 초기화, 버튼 인터럽트 처리, BLE HID 통신 및 FreeRTOS 태스크 관리를 수행합니다.

### 데이터 흐름 (Data Flow)
1.  **Sensing**: MPU6050에서 6축 데이터 추출 (8ms 주기)
2.  **Orientation**: 가속도/자이로 데이터를 결합하여 현재 롤(Roll) 각도 추정 (상보 필터)
3.  **Correction**: 회전 행렬을 통한 좌표축 보정 및 클릭 떨림 억제
4.  **Scaling**: 시그모이드 함수를 통한 비선형 가속도 적용
5.  **Transmission**: Mutex로 보호된 공유 자원을 통해 BLE 스택으로 데이터 전달 후 PC 송신



---

## ⚠️ 설치 및 주의사항
* **라이브러리 의존성**: `ESP32 BLE Mouse`, `MPU6050` 라이브러리가 필요합니다.
* **초기 교정(Calibration)**: 전원을 켠 직후 약 1초간 마우스를 평평한 곳에 두어 자이로 오프셋을 자동으로 잡도록 설계되었습니다.
* **핀 맵 설정**: `platformio.ini` 및 `EliteAirMouse.h`에서 실제 사용 중인 GPIO 번호를 반드시 확인하십시오.
