# 🤖 Agent Guide & Project Instructions (agent.md)

이 문서는 **esp32_AirMouse_002** 프로젝트를 분석, 수정, 확장하는 AI 코딩 에이전트와 개발자를 위한 필수 행동 지침서입니다.
프로젝트 작업을 시작하기 전 반드시 본 문서의 규칙과 아키텍처 원칙을 숙지하고 엄격히 준수해야 합니다.

---

## 1. 📌 핵심 프로젝트 개요

- **프로젝트 명**: Elite AirMouse S3 (`esp32_AirMouse_002`)
- **하드웨어 사양**:
  - **MCU**: ESP32-S3-Zero (Dual-Core Xtensa LX7, 240MHz, 4MB Flash, BLE 5.0, Native USB)
  - **센서**: MPU6050 6축 IMU (I2C: SDA=GPIO7, SCL=GPIO8)
  - **포인팅 방식**: 자이로 전용 공간 포인팅 (상보 필터 + 적응형 LPF + 시그모이드 가속 + 제로 스냅)
  - **입력 버튼**: 마이크로 스위치 6개 (GPIO 4, 5, 6, 9, 10, 11) + 레이저 제어 (GPIO 3)
  - **인디케이터**: 온보드 WS2812 RGB LED (GPIO 21)
  - **저장소**: LittleFS (설정 JSON 원자적 저장 관리)
- **주요 기능**:
  - **3-Mode 시스템**: Mode 1(PC), Mode 2(Presentation/PPT), Mode 3(Smart TV)
  - **BLE HID Composite**: Mouse + Keyboard + Consumer Control (NimBLE 기반)
  - **3계층 제스처**: Flick(P2P), Linear(방향성 충격량), Tilt Hold(자세 유지)
  - **Front Hold 스크롤**: Side F 누른 상태에서 커서 감쇠 + 수직 휠 + 수평 팬
  - **Action Registry**: ~40종 사전 정의 액션 슬롯 매트릭스 자유 매핑
  - **Zero-rate Bias Tracking**: 실시간 자이로 정지 상태 자동 바이어스 흡수
  - **SafeBoot & OTA Guard**: 부팅 실패 감지 복구 및 OTA 도중 브릭 방지
  - **Light-sleep & WoM**: 60초 미동작 시 절전, 버튼/모션 인터럽트 즉각 복귀
  - **웹 UI 커스터마이징**: AP Fallback 및 웹 브라우저 기반 실시간 설정 변경

---

## 2. 📂 활성 소스 코드 및 디렉터리 체계

> [!IMPORTANT]
> **현재 활성(Active) 릴리스 버전은 `v0410` 입니다.**
> 모든 신규 기능 구현, 버그 수정, 설정 관리는 `src/v0410/` 디렉터리 내의 파일들을 대상으로 해야 합니다. (레거시 `v001` ~ `v0400` 폴더는 참조용이며 빌드에서 제외됨)

```text
esp32_AirMouse_002/
├── platformio.ini           # PlatformIO 빌드 환경 설정 (default_envs: esp32-s3-zero)
├── default_4MB.csv          # 4MB Flash 파티션 테이블
├── agent.md                 # [본 파일] AI 에이전트 지침서
├── README.md                # 전체 프로젝트 사양 및 설명서
├── src/
│   ├── main.cpp             # 펌웨어 진입점, W10-E10 브릿지 콜백, 루프 감시
│   └── v0410/               # [★ 최신 활성 코드베이스]
│       ├── A40_ComFunc_0410.h           # 공용 유틸리티 및 헬퍼 함수
│       ├── B20_Ble_0410.h / .cpp        # NimBLE 기반 HID Composite 제어
│       ├── C10_Def_0410.h               # 설정 데이터 구조체 (ST_C10_Config_t 등)
│       ├── C10_Config_0410.h / .cpp     # LittleFS 원자적 JSON 저장/로드
│       ├── C20_Action_0410.h            # 액션 ID, 레지스트리 정의
│       ├── C20_ActionExec_0410.h / .cpp # 액션 실행기 (HID 커맨드 생성)
│       ├── C20_BtnDispatcher_0410.h/.cpp# 물리 버튼 상태 처리 및 슬롯 매핑
│       ├── D10_Logger_0410.h            # 고속 링버퍼 기반 로거 & 시스템 진단
│       ├── E10_Def_0410.h               # 에어마우스 상태/큐/모드 정의
│       ├── E10_AirMouse_0410.h          # 에어마우스 메인 클래스 선언
│       ├── E10_AirMouse_Core_0410.cpp   # 초기화, FSM, 생명주기 관리
│       ├── E10_AirMouse_Hid_0410.cpp    # BLE HID 큐 소비 및 전송
│       ├── E10_AirMouse_Motion_0410.cpp # IMU 읽기 및 좌표 계산
│       ├── E10_AirMouse_Diag_0410.cpp   # 진단, 바이어스 캘리브레이션
│       ├── E10_AirMouse_Task_0410.cpp   # RTOS 태스크 진입점 및 스케줄링
│       ├── E10_AirMouse_Action_0410.cpp # 특수 액션 실행 (모드 전환 등)
│       ├── L10_Led_0410.h / .cpp        # WS2812 NeoPixel 비동기 제어
│       ├── M10_MotionProc_0410.h        # 자이로 필터링 및 가속 곡선 엔진
│       ├── M20_BiasTracker_0410.h       # 실시간 제로 레이트 바이어스 추적기
│       ├── M30_Gesture_0410.h / .cpp    # 3계층 제스처 인식 엔진
│       ├── P20_Power_0410.h / .cpp      # Light-sleep, WoM 전원 관리
│       ├── W10_Def_0410.h               # 웹 서버 데이터 구조체/상수
│       ├── W10_Web_0410.h               # AsyncWebServer 관리자
│       ├── W10_Web_init_0410.cpp        # WiFi AP/STA 및 서버 초기화
│       ├── W10_Web_Static_0410.cpp      # 정적 웹 파일(Gzip) 라우팅
│       ├── W10_WebApi_*.cpp             # 모듈별 REST API 라우트
│       ├── data_v0410/                  # LittleFS에 플래시될 파일 (index.html.gz 등)
│       ├── docs_v0410/                  # v0410 상세 요구사항 및 기술 사양서
│       │   ├── Req_ImplPlan/            # 요구사항 정의서 & 구현 계획서
│       │   └── SPEC_Manual/             # 00.SPEC_0410_001.md, 사용자 매뉴얼
│       └── tools_v0410/                 # 빌드 보조 스크립트 (pio_gzip_0410.py)
```

---

## 3. ⚡ 개발 환경 및 필수 명령어

### 🛠️ PlatformIO CLI 명령어
- **펌웨어 컴파일**:
  ```powershell
  pio run -e esp32-s3-zero
  ```
- **펌웨어 업로드**:
  ```powershell
  pio run -e esp32-s3-zero -t upload
  ```
- **LittleFS 파일시스템 빌드 및 업로드**:
  ```powershell
  pio run -e esp32-s3-zero -t buildfs
  pio run -e esp32-s3-zero -t uploadfs
  ```
- **빌드 캐시 클린**:
  ```powershell
  pio run -e esp32-s3-zero -t clean
  ```
- **시리얼 모니터 (115200 bps)**:
  ```powershell
  pio device monitor -b 115200
  ```

### 🧠 지식 그래프 (Graphify) 갱신
코드나 설계를 변경한 후에는 지식 그래프의 최신화를 위해 반드시 다음 명령어를 실행합니다:
```powershell
graphify update .
```

---

## 4. 📐 엄격한 코딩 규칙 (Strict Rules)

코드 작성 시 아래 규칙을 위반하면 즉시 반려 또는 빌드/런타임 실패로 이어집니다.

### ① 명명 규칙 (Strict Naming Conventions)
| 구분 | 접두사 / 규칙 | 예시 |
|---|---|---|
| **전역 상수** | `G_` (대문자 스네이크) | `G_BOOT_GRACE_MS`, `G_BTN_MODE` |
| **전역 변수** | `g_` (카멜 표기) | `g_cfg`, `g_e10`, `g_w10` |
| **클래스명** | `CL_` + 모듈약어 | `CL_E10_EliteAirMouse`, `CL_C10_Config` |
| **구조체/타입** | `ST_` + `_t` 접미사 | `ST_E10_Status_t`, `ST_C10_E10Config_t` |
| **열거형** | `EN_` + `_t` 접미사 | `EN_E10_Mode_t`, `EN_M30_GestureType_t` |
| **클래스 private 멤버** | `_` (언더스코어) | `_qMotion`, `_sensorTask`, `_pBle` |
| **로컬 변수** | `v_` (카멜 표기) | `v_dx`, `v_nowMs`, `v_doc` |
| **함수 매개변수** | `p_` (카멜 표기) | `p_mode`, `p_outStatus`, `p_ctx` |

### ② ArduinoJson v7 단일화 정책
- **오직 `JsonDocument` 단일 타입만 사용**:
  - `StaticJsonDocument`, `DynamicJsonDocument` 사용 절대 금지.
  - `containsKey()` 사용 금지 (`if (doc["key"].is<int>())` 또는 `isNull()` 사용).
  - `createNestedArray()`, `createNestedObject()` 사용 금지.
  - 중첩 객체/배열 생성 시: `doc["node"].to<JsonObject>()` 또는 `doc["items"].to<JsonArray>()` 패턴 사용.

### ③ 문자열 및 버퍼 관리 정책
- 힙 단편화(Heap Fragmentation) 방지를 위해 **아두이노 `String`의 무분별한 사용 금지**.
- 로깅 및 포맷팅 시 고정 스택 버퍼(`char buf[N]`)와 `snprintf` 사용 권장.

### ④ 원자적 스토리지 저장 정책 (Atomic Storage)
LittleFS에 파일 저장 시 갑작스러운 전원 차단에 대비하여 다음 5단계를 거쳐야 합니다:
1. 임시 파일(`.tmp`) 생성 및 내용 기록
2. `flush()` 및 `close()` 수행
3. 기존 백업 파일(`.bak`) 존재 시 삭제
4. 현재 파일(`.json`)을 백업 파일(`.bak`)로 이름 변경
5. 임시 파일(`.tmp`)을 현재 파일(`.json`)로 이름 변경

---

## 5. 🧵 멀티태스킹 & 동기화 아키텍처 (Concurrency Rules)

ESP32-S3의 듀얼 코어를 극대화하기 위해 다음과 같은 태스크 분할 모델을 유지합니다.

| Task | Core | 우선순위 | 주기 | 역할 및 데이터 경계 |
|---|:---:|:---:|:---:|---|
| **`_sensorTask`** | **Core 1** | 3 (High) | 8ms (125Hz) | IMU 센서 폴링, 좌표/가속도 필터링, 제스처 감지, **Special 액션 단독 실행**, 큐 생산 |
| **`_commTask`** | **Core 0** | 2 (Mid) | 7ms | 모션/커맨드 큐 소비, BLE HID 패킷 호스트 전송, 일반 액션 실행 |
| **`_ledTask`** | **Core 0** | 1 (Low) | 50ms | WS2812 LED 상태머신 전용 Tick (다른 태스크에서 직접 LED 구동 금지) |
| **`AsyncWebServer`** | Core 0 | 이벤트 기반 | 비동기 | HTTP 수신, 설정 갱신. **HID 드라이버 직접 호출 절대 금지 (반드시 큐 Enqueue)** |

### ⚠️ 동기화 필수 준수 사항
1. **HID 직접 접근 금지**: Web 태스크나 메인 루프에서 HID 함수를 직접 부르면 패닉이 발생할 수 있습니다. 반드시 커맨드 큐를 경유해야 합니다.
2. **Special 액션 단독 실행 권한**: 모드 변경, 센서 캘리브레이션, 세이프모드 전환 등 시스템 상태를 흔드는 작업은 오직 `_sensorTask`에서만 처리합니다.
3. **E10 모듈 분할 보존**: E10 관련 코드는 기능에 따라 분할된 7개 파일(`Core`, `Hid`, `Motion`, `Diag`, `Task`, `Action`, `_0410.cpp`)에 맞추어 작성되어야 하며, 임의로 단일 거대 파일로 합치지 마십시오.

---

## 6. 🤖 AI 에이전트 수정 워크플로우 (계약 기반 리뷰)

1. **지속 산출물 대조 검토 (Contract Validation)**:
   - 코드 수정 전 `src/v0410/docs_v0410/contract_doc/`의 5대 산출물 대조:
     - `CONTRACT.md` (모듈 경계, HID 단일 제어권, Special 격리, 큐 non-blocking)
     - `STATE.md` (상태 단독 소유권, FSM 규칙)
     - `FLOW.md` (8ms 모션 파이프라인, 프로파일 스위치 흐름)
     - `BUDGET.md` (태스크 주기 8ms/7ms/50ms, 블로킹 금지)
     - `SPEC.md` (요구사항 ID 매핑)
2. **코드 변경 시**:
   - 명명 규칙(`G_`, `g_`, `CL_`, `ST_`, `EN_`, `_`, `v_`, `p_`) 엄수
   - FreeRTOS 태스크 간 데이터 전달 시 큐 경유 여부 점검 (timeout=0)
   - 계약/상태/예산에 영향 주는 수정 시 `contract_doc/` 문서 동시 동기화
3. **검증 및 빌드**:
   - PlatformIO 빌드 명령어로 문법/링크 오류 없음 확인:
     `pio run -e esp32-s3-zero`
4. **지식 베이스 동기화**:
   - 코드 변경 후 `graphify update .` 실행

---

## 7. 🚀 v0410 다중 프로파일 & 매크로 라이브러리 구현 현황 (완료)

| 마일스톤 | 대상 모듈 | 구현 내용 | 상태 |
|---|---|---|:---:|
| **M1: 스키마 & 모델** | `C10_Config`, `C20_Action` | Schema v5 데이터 구조 정의, Profile CRUD(최대 5개), MacroLib(8×8), 27개 고정 트리거(4개 잠금 비트), LittleFS 원자적 I/O | ✅ 완료 |
| **M2: 백엔드 런타임** | `E10_AirMouse_*` | 프로파일 동적 스위칭/재로드, Global+Override 2단 슬롯 리졸버(`_resolveSlot`), 10ms 단위 비동기 안전 취소 매크로 시퀀서(`_runMacro`), Live Test 지원 | ✅ 완료 |
| **M3: REST API & 브릿지** | `W10_Web*`, `main.cpp` | `/api/profiles/*`, `/api/triggers`, `/api/action/test`, `/api/action/test_macro` 구현, SafeMode Guard 연동, E10 브릿지 확장 | ✅ 완료 |
| **M4: Web UI 3-View** | `data_v0410_www/` | `index_0410.html`, `style_0410.css`, `app_0410.002.js` 전면 개편. Online/Offline 모드(상수 및 모킹), Profile Bar, Slot Editor(Global/M1/M2/M3), Macro Editor(8×8, 순서이동, 딜레이), Config/Diag 연동 | ✅ 완료 |
| **검증 & 빌드** | PlatformIO / Graphify | `pio run -e esp32-s3-zero` 빌드 성공 (RAM 40.9%, Flash 45.5%), `graphify update .` 최신화 완료 | ✅ 완료 |

