# 🤖 Agent Guide & Project Instructions (agent.md)

이 문서는 **esp32_AirMouse_002** 프로젝트를 분석, 설계, 구현, 리팩토링, 코드 리뷰 및 검증하는 AI 코딩 에이전트(Claude Code, Antigravity 등)와 개발자를 위한 **단일 진실 공급원(Single Source of Truth, SSOT)**이자 필수 행동 지침서입니다.
프로젝트 작업을 시작하기 전 반드시 본 문서의 규칙과 아키텍처 원칙을 숙지하고 엄격히 준수해야 합니다.

---

## 1. 📌 핵심 프로젝트 개요

- **프로젝트 명**: Elite AirMouse S3 (`esp32_AirMouse_002`)
- **하드웨어 사양 (SSOT: [HW_Def_0415.h](file:///d:/95.2540_PJT/80.Platformio_PJTs/esp32_AirMouse_002/src/v0415/HW_Def_0415.h))**:
  - **MCU**: Waveshare ESP32-S3-Zero (Dual-Core Xtensa LX7 @ 240MHz, 4MB Flash, BLE 5.0, Native USB)
  - **IMU 센서**: MPU6050 6축 모션 센서
    - **I2C 버스**: `PIN_I2C_SDA` = GPIO 4, `PIN_I2C_SCL` = GPIO 5 (400kHz Fast Mode)
    - **인터럽트 (WoM)**: `PIN_MPU_INT` = GPIO 6 (Wake-on-Motion 인터럽트, EXT1 웨이크 소스)
  - **입력 버튼 (마이크로 스위치 6개)**: `HW_DEF::BTN_PINS` 및 `EN_C20_BtnId_t` 1:1 매핑
    - Top 면 (3개):
      - `PIN_BTN_TOP_L` (GPIO 9): 마우스 좌클릭 / 드래그
      - `PIN_BTN_TOP_M` (GPIO 8): Move Gate (커서 이동 활성화 게이트)
      - `PIN_BTN_TOP_R` (GPIO 7): 마우스 우클릭
    - Side 면 (3개):
      - `PIN_BTN_SIDE_F` (GPIO 12): Front Hold (스크롤/팬 진입 감쇠)
      - `PIN_BTN_SIDE_C` (GPIO 13): Mode / Pairing / Host Cycle / 6초 롱프레스 공장초기화
      - `PIN_BTN_SIDE_R` (GPIO 11): 보조 기능 (Quick Back 등)
  - **배터리 센싱**: `PIN_BATT_ADC` = GPIO 1 (ADC1_CH0 분압 저항 아날로그 측정)
  - **인디케이터**: `PIN_LED_WS2812` = GPIO 21 (온보드 RGB NeoPixel DIN, RMT 구동)
  - **UART0 디버그**: TX = GPIO 43, RX = GPIO 44 (115200 bps)
  - **EXT1 웨이크업 마스크**: `HW_DEF::buildWakeMaskNormal()`, `buildWakeMaskSafe()`, `buildWakeMaskButtons()` (constexpr 자동 계산)
  - **포인팅 방식**: 자이로 전용 공간 포인팅 (상보 필터 + 적응형 LPF + 시그모이드 가속 + 제로 스냅 + 클릭 프리즈)
  - **저장소**: LittleFS (JSON 설정 원자적 5단계 트랜잭션 회전 저장)
- **주요 핵심 기능**:
  - **3-Mode 프로파일**: Mode 1(PC AirMouse), Mode 2(Presentation/PPT), Mode 3(Smart TV)
  - **BLE HID Composite**: NimBLE 기반 Mouse + Keyboard + Consumer Control 복합 디바이스
  - **3계층 제스처 엔진**: Flick(P2P), Linear(방향성 충격량), Tilt Hold(자세 유지)
  - **Front Hold 스크롤**: Side F 누른 상태에서 커서 감쇠 + 수직 휠 + 수평 팬
  - **Action Registry**: ~40종 사전 정의 액션 슬롯 매트릭스 자유 매핑
  - **Zero-rate Bias Tracking**: 정지 상태 실시간 자이로 바이어스 누적 흡수
  - **Two-Phase Commit 로거**: [D10_Logger_0415](file:///d:/95.2540_PJT/80.Platformio_PJTs/esp32_AirMouse_002/src/v0415/D10_Logger_0415.h) 링버퍼 예약(`reserve`) 및 확정(`commit`)으로 멀티태스킹 로그 경합 제거
  - **전원 관리 (Light-sleep / Deep-sleep)**: 60초 미동작 시 절전, 버튼/모션 인터럽트 즉각 복귀
  - **비동기 웹 UI 커스터마이징**: AP Fallback 및 웹 브라우저 기반 실시간 설정/프로파일/진단 제어

---

## 2. 📂 활성 소스 코드 및 디렉터리 체계

> [!IMPORTANT]
> **현재 활성(Active) 릴리스 버전은 `v0415` 입니다.**
> 모든 신규 기능 구현, 버그 수정, 설정 관리는 `src/v0415/` 디렉터리 내의 파일들을 대상으로 해야 합니다. (레거시 `v001` ~ `v0410` 폴더는 참조용이며 빌드에서 제외됨)

```text
esp32_AirMouse_002/
├── platformio.ini           # PlatformIO 빌드 환경 설정 (default_envs: esp32-s3-zero)
├── default_4MB.csv          # 4MB Flash 파티션 테이블
├── agent.md                 # [본 파일] AI 에이전트 지침서 (Single Source of Truth)
├── CLAUDE.md                # Claude Code용 참조 포인터 (agent.md 위임)
├── README.md                # 전체 프로젝트 사양 및 설명서
├── src/
│   ├── main.cpp             # 펌웨어 진입점, W10-E10 브릿지 콜백, RTOS 스케줄러 시작
│   └── v0415/               # [★ 최신 활성 코드베이스]
│       ├── HW_Def_0415.h                # [HW SSOT] GPIO 핀 정의 및 EXT1 웨이크업 마스크
│       ├── A40_ComFunc_0415.h           # 공용 유틸리티, JSON/LittleFS/Mutex 헬퍼
│       ├── B20_Ble_0415.h / .cpp        # NimBLE 기반 HID Composite 제어기
│       ├── C10_Def_0415.h               # 설정 데이터 구조체 (ST_C10_Config_t 등)
│       ├── C10_Config_0415.h / .cpp     # LittleFS 원자적 JSON 저장/로드/백업 회전
│       ├── C20_Action_0415.h            # 액션 ID, 레지스트리, 버튼 슬롯 정의
│       ├── C20_ActionExec_0415.h / .cpp # 액션 실행기 (HID 커맨드 생성)
│       ├── C20_BtnDispatcher_0415.h/.cpp# 물리 버튼 상태 디바운싱 및 슬롯 디스패치
│       ├── D10_Logger_0415.h / .cpp     # Two-Phase Commit 기반 고속 링버퍼 로거
│       ├── E10_Def_0415.h               # 에어마우스 상태/큐/모드 상수 정의
│       ├── E10_AirMouse_0415.h          # 에어마우스 메인 클래스 선언
│       ├── E10_AirMouse_Core_0415.cpp   # 초기화, FSM 생명주기 관리
│       ├── E10_AirMouse_Hid_0415.cpp    # BLE HID 큐 소비 및 전송
│       ├── E10_AirMouse_Motion_0415.cpp # IMU 읽기, 필터링 및 좌표 계산 파이프라인
│       ├── E10_AirMouse_Diag_0415.cpp   # 진단, 바이어스 캘리브레이션
│       ├── E10_AirMouse_Task_0415.cpp   # RTOS 태스크 진입점 및 스케줄링
│       ├── E10_AirMouse_Action_0415.cpp # Special 액션 실행기 (모드 전환, 리셋 등)
│       ├── L10_Led_0415.h / .cpp        # WS2812 NeoPixel 비동기 제어기
│       ├── M10_MotionProc_0415.h        # 자이로 필터링 및 가속 곡선 엔진
│       ├── M20_BiasTracker_0415.h       # 실시간 제로 레이트 바이어스 추적기
│       ├── M30_Gesture_0415.h / .cpp    # 3계층 제스처 인식 엔진
│       ├── P20_Power_0415.h / .cpp      # Light-sleep, Deep-sleep, WoM 전원 관리자
│       ├── W10_Def_0415.h               # 웹 서버 데이터 구조체 및 상수
│       ├── W10_Web_0415.h               # AsyncWebServer 관리자 클래스 선언
│       ├── W10_Web_init_0415.cpp        # WiFi AP/STA 및 비동기 웹 서버 초기화
│       ├── W10_Web_Static_0415.cpp      # Gzip 정적 웹 파일 라우팅
│       ├── W10_WebApi_Com_0415.cpp      # 일반 REST API 라우트
│       ├── W10_WebApi_Config_0415.cpp   # 설정 조회 및 변경 REST API
│       ├── W10_WebApi_CtlPpt_0415.cpp   # PPT/프레젠테이션 제어 API
│       ├── W10_WebApi_OtaBoot_0415.cpp  # OTA 펌웨어 업데이트 및 재부팅 API
│       ├── W10_WebApi_Profile_0415.cpp  # 3-Mode 프로파일 전환 API
│       ├── W10_WebApi_Status_0415.cpp   # 실시간 텔레메트리/센서 상태 API
│       ├── data_v0415/                  # LittleFS에 플래시될 최종 파일 (index_0415.html.gz 등)
│       ├── data_v0415_www/              # 웹 UI 원본 소스 (HTML/CSS/JS/i18n)
│       ├── docs_v0415/                  # 기술 사양서 및 검증 스크립트
│       │   ├── contract_doc/            # [5대 지속 산출물] CONTRACT, STATE, FLOW, BUDGET, SPEC
│       │   ├── Req_ImplPlan/            # 요구사항 정의서 & 구현 계획서
│       │   ├── SPEC_Manual/             # 00.SPEC_0415_001.md, 00.UserManual_0415_001.md
│       │   ├── check_i18n_0415.py       # 다국어 리소스 무결성 검증기
│       │   └── check_schema_0415.py     # 설정 JSON 스키마 검증기
│       └── tools_v0415/                 # 빌드 보조 도구 (pio_gzip_0415.py)
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
- **시리얼 모니터 모니터링 (115200 bps)**:
  ```powershell
  pio device monitor -b 115200
  ```

### 🌐 웹 에셋 및 스키마 검증 스크립트
- **웹 리소스 Gzip 압축 및 스테이징**:
  ```powershell
  python src/v0415/tools_v0415/pio_gzip_0415.py
  ```
- **다국어(i18n) 정합성 검증**:
  ```powershell
  python src/v0415/docs_v0415/check_i18n_0415.py
  ```
- **설정 스키마(schema) 정합성 검증**:
  ```powershell
  python src/v0415/docs_v0415/check_schema_0415.py
  ```

### 🧠 지식 그래프 (Graphify) 기반 탐색 원칙 (Strict Invariant)

> [!CAUTION]
> **전체 소스/문서 직접 열람 및 무차별 Grep 절대 금지 (Graph-First Rule)**:
> AI 에이전트는 코드 분석, 수정, 버그 추적, 리팩토링, 미사용 코드 점검 등 **모든 작업 시 소스 코드(`.h`, `.cpp`)나 문서(`.md`, `.json`)를 직접 열거나(`view_file`) 무차별 검색(`grep_search`)을 먼저 수행하는 것을 엄격히 금지**합니다.

#### 📌 2단계 필수 탐색 프로토콜 (Two-Step Navigation Protocol)
1. **[1단계] Graphify 선행 질의 (Mandatory First Step)**:
   - 작업 시작 전 반드시 아래 명령어로 지식 그래프(`graphify-out/graph.json`)를 먼저 질의하여 대상 심볼, 호출 관계, 소속 커뮤니티, 정확한 파일 경로 및 라인 번호를 파악해야 합니다:
     - `graphify query "<목적/질문/심볼>"`: 관련 노드, 커뮤니티, 정확한 파일 경로(`src=...`) 및 라인(`loc=...`) 조회 (CLI `--budget` 조절 가능)
     - `graphify path "<출발노드>" "<도착노드>"`: 두 심볼/모듈 간 호출 관계 및 최단 의존 경로 추적
     - `graphify explain "<심볼/함수/클래스명>"`: 특정 심볼의 상세 명세 및 연결 노드 확인
2. **[2단계] 타겟 라인 정밀 열람 (Pinpoint View)**:
   - 1단계 Graphify 질의 결과로 확인된 **정확한 파일과 필요한 최소 라인 범위(`StartLine`, `EndLine`)에 한해서만** `view_file`을 실행합니다.
   - 전체 파일을 한 번에 크게 열거나, 목적 없이 파일들을 순차 열람하는 행위는 금지됩니다.

#### 🔄 지식 그래프 갱신
코드나 설계를 변경한 후에는 지식 그래프 최신화를 위해 반드시 다음 명령어를 실행합니다 (AST 전용, API 비용 없음):
```powershell
graphify update .
```
*(참고: `graphify --help` 명령은 절대 실행하지 않으며 문법은 정적 가이드를 참조합니다.)*

---

## 4. 📐 엄격한 코딩 및 문서화 규칙 (Strict Rules)

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

### ④ 원자적 스토리지 저장 정책 (Atomic Storage 5-Phase)
LittleFS에 파일 저장 시 갑작스러운 전원 차단에 대비하여 다음 5단계를 거쳐야 합니다:
1. 임시 파일(`.tmp`) 생성 및 내용 기록
2. `flush()` 및 `close()` 수행
3. 기존 백업 파일(`.bak`) 존재 시 삭제
4. 현재 파일(`.json`)을 백업 파일(`.bak`)로 이름 변경
5. 임시 파일(`.tmp`)을 현재 파일(`.json`)로 이름 변경

### ⑤ Mermaid 다이어그램 렌더링 무결성 규칙 (GitHub & Obsidian 완벽 호환)
마크다운 문서(`FLOW_0415.md` 등) 내에 Mermaid 다이어그램을 작성하거나 수정할 때는 GitHub 웹 마크다운 뷰어 및 Obsidian 내장 파서에서 구문 오류(`Syntax error in graph`)가 발생하지 않도록 다음 5대 규칙을 반드시 준수합니다:
1. **노드 라벨 큰따옴표 래핑**: 괄호 `()`, 대괄호 `[]`, 중괄호 `{}`, 쉼표 `,`, 콜론 `:`, 슬래시 `/` 등이 포함된 모든 노드 텍스트는 반드시 큰따옴표로 감쌉니다 (`id["..."]` / `id{"..."}`).
2. **특수 문자 및 연산자 격리**: C++ 연산자(`++`, `*=`, `==`, `!=`, `<=`, `>=`) 및 Mermaid 예약어(`&s_mux`의 `&`)는 큰따옴표 내부에 안전하게 격리합니다.
3. **분기 에지(Edge) 라벨 큰따옴표 래핑**: 슬래시나 공백, 한글이 포함된 조건 텍스트는 `-- "조건" -->` 형태로 큰따옴표로 감쌉니다.
4. **라벨 내부 화살표 기호 유니코드 치환**: 텍스트 내부의 `->`는 연결선 구문과 혼동되지 않도록 유니코드 화살표 `➔`로 치환합니다.
5. **그래프 연결성(Connectivity) 보장**: 모든 노드는 선행 에지와 유기적으로 연결되어야 하며, 선행 연결이 없는 고립 노드(Unrooted/Disconnected Node)를 생성하지 않습니다.

### ⑥ 4대 절대 금지 계약 규칙 (CONTRACT Invariants)
1. **Web Task의 HID 직접 접근 금지**: `_mouse`, `_keyboard`는 오직 `commTask`만 제어하며, Web/Sensor는 `_qHidCmd` 또는 `_qActionExec`를 경유해야 합니다.
2. **Special 액션의 Context 격리**: `EN_C20_ACT_SPECIAL`은 큐에 넣지 않고 **오직 `sensorTask` 컨텍스트에서만 동기 실행**합니다.
3. **LED 상태머신의 단독 Tick**: `_led.tick()`은 오직 `_ledTask`(50ms 주기)에서만 단독 실행합니다.
4. **블로킹 금지**: 모든 큐 인큐(`_enqueueAction`, `_enqueueHidCmd`, `_pushFrame`)는 `timeout = 0` (non-blocking)을 유지해야 합니다.

---

## 5. 🧵 멀티태스킹 & 동기화 아키텍처 (Concurrency Rules)

ESP32-S3의 듀얼 코어를 극대화하기 위해 다음과 같은 태스크 분할 모델을 유지합니다.

| Task | Core | 우선순위 | 주기 | 역할 및 데이터 경계 |
|---|:---:|:---:|:---:|---|
| **`_sensorTask`** | **Core 1** | 3 (High) | 8ms (125Hz) | IMU 센서 폴링, 좌표/가속도 필터링, 제스처 감지, **Special 액션 단독 실행**, 큐 생산 |
| **`_commTask`** | **Core 0** | 2 (Mid) | 7ms | 모션/커맨드 큐 소비, BLE HID 패킷 호스트 전송, 일반 액션 실행, 비동기 매크로 FSM |
| **`_ledTask`** | **Core 0** | 1 (Low) | 50ms | WS2812 LED 상태머신 전용 Tick (타 태스크에서 직접 구동 금지) |
| **`AsyncWebServer`** | Core 0 | 이벤트 기반 | 비동기 | HTTP 수신, 설정 갱신. **HID 드라이버 직접 호출 절대 금지 (반드시 큐 Enqueue)** |

### ⚠️ 동기화 필수 준수 사항
1. **HID 직접 접근 금지**: Web 태스크나 메인 루프에서 HID 함수를 직접 부르면 패닉이 발생할 수 있습니다. 반드시 커맨드 큐를 경유해야 합니다.
2. **Special 액션 단독 실행 권한**: 모드 변경, 센서 캘리브레이션, 세이프모드 전환 등 시스템 상태를 변경하는 작업은 오직 `_sensorTask`에서만 처리합니다.
3. **E10 모듈 분할 보존**: E10 관련 코드는 기능에 따라 분할된 7개 파일(`Core`, `Hid`, `Motion`, `Diag`, `Task`, `Action`, `_0415.cpp`)에 맞추어 작성되어야 하며, 임의로 단일 거대 파일로 합치지 마십시오.
4. **공유 상태 보호**: 태스크 간 공유되는 상태는 계약된 동기화 수단(`volatile`, `portENTER_CRITICAL / portEXIT_CRITICAL`, `_macroAbortToken` 원자적 증분)을 준수합니다.

---

## 6. 🚨 계약 기반 개발 및 리뷰 프로토콜 (Contract-Based Protocol)

본 프로젝트의 소스 코드를 수정, 신규 기능 추가 또는 리뷰할 때는 **반드시** 아래 경로에 위치한 5대 지속 산출물(Persistent Artifacts)을 대조 기준으로 삼아 정합성을 검증해야 합니다.

- 계약 문서 경로: [src/v0415/docs_v0415/contract_doc/](file:///d:/95.2540_PJT/80.Platformio_PJTs/esp32_AirMouse_002/src/v0415/docs_v0415/contract_doc/)
  1. [CONTRACT_0415.md](file:///d:/95.2540_PJT/80.Platformio_PJTs/esp32_AirMouse_002/src/v0415/docs_v0415/contract_doc/CONTRACT_0415.md): 모듈 경계, 호출 권한, FreeRTOS 큐 계약, 공유 변수 동기화 및 원자성 계약
  2. [STATE_0415.md](file:///d:/95.2540_PJT/80.Platformio_PJTs/esp32_AirMouse_002/src/v0415/docs_v0415/contract_doc/STATE_0415.md): 상태 변수 단독 소유권(Writer/Reader) 및 FSM(Click-Freeze, Snap, Precision, Macro) 명세
  3. [FLOW_0415.md](file:///d:/95.2540_PJT/80.Platformio_PJTs/esp32_AirMouse_002/src/v0415/docs_v0415/contract_doc/FLOW_0415.md): 모션 파이프라인(8ms), 프로파일 전환 시퀀스, 액션/제스처 디스패치 흐름
  4. [BUDGET_0415.md](file:///d:/95.2540_PJT/80.Platformio_PJTs/esp32_AirMouse_002/src/v0415/docs_v0415/contract_doc/BUDGET_0415.md): FreeRTOS 태스크 주기(8ms/7ms/50ms), Worst-case 실행 시간, 블로킹 금지 규칙
  5. [SPEC_0415.md](file:///d:/95.2540_PJT/80.Platformio_PJTs/esp32_AirMouse_002/src/v0415/docs_v0415/contract_doc/SPEC_0415.md): 요구사항 ID ↔ 구현 소스 추적성 매트릭스

### ⚡ 필수 검토 및 준수 6대 원칙 (Invariants)
1. **[CONTRACT] 4대 절대 금지 규칙 준수**:
   - Web Task의 HID 직접 접근 금지, Special 액션 Context 격리, LED tick 단독 실행, 큐 non-blocking (timeout=0).
2. **[STATE] 상태 소유권 침해 금지**:
   - `STATE.md` 소유권 매트릭스에 명시된 단독 Writer 외 태스크 직접 쓰기 금지. 동기화 수단 준수.
3. **[FLOW] 파이프라인 정합성 유지**:
   - 모션 파이프라인 필터 순서(Bias → M10 → FrontHold/MoveGate → Precision → Snap → Click-Freeze) 임의 변경 금지. 프로파일 변경 시 드레인/리셋 위임 준수.
4. **[BUDGET] 시간 예산 및 블로킹 초과 금지**:
   - `sensorTask`(8ms), `commTask`(7ms) 내 블로킹 I/O/sleep 금지. 매크로는 `_tickMacro` 비동기 상태머신으로 처리.
5. **[SPEC] 추적성 유지**:
   - 신규 기능/변경 시 `SPEC.md`의 요구사항 ID 및 구현 파일 링크 갱신.
6. **[DOCS] 코드 변경 후 문서 동기화**:
   - 계약/상태/예산 영향 시 `contract_doc/` 문서 동시 동기화. Mermaid 다이어그램 작성 시 GitHub/Obsidian 무결성 규칙 엄수.

### 📋 일관된 리뷰 및 검토 출력 포맷 (Standard Review Response Format)
코드 작성 전 검토 또는 코드 리뷰 응답 시 아래 6개 표준 카테고리를 고정 적용합니다:

```markdown
### A. 계약 위반 검토 (CONTRACT.md 기준)
- 위반 여부, 위치, 심각도, 조치 사항

### B. 상태 소유권 위반 검토 (STATE.md 기준)
- Writer/Reader 위반, 동기화 누락 여부

### C. 시간 예산 및 블로킹 검토 (BUDGET.md 기준)
- 최악 실행 시간 영향, 태스크 데드라인 준수 여부

### D. 흐름 및 파이프라인 간섭 검토 (FLOW.md 기준)
- 기존 파이프라인 순서 및 분기 영향

### E. 요구사항 정합성 검토 (SPEC.md 기준)
- 요구사항 ID 충돌 여부 및 추적성

### F. 커버리지 및 최종 판정
- 종합 판정 (PASS / WARN / FAIL)
```

---

## 7. 🔄 버전 업그레이드 시 동기화 체크리스트 (Version Bump Protocol)

펌웨어 버전(예: `v0410` → `v0415`)이 변경될 때는 소스 코드뿐만 아니라 아래 산출물을 반드시 함께 갱신해야 합니다:
1. **GitHub Actions CI/CD 워크플로우**:
   - [`.github/workflows/ci_1_build_010.yml`](file:///d:/95.2540_PJT/80.Platformio_PJTs/esp32_AirMouse_002/.github/workflows/ci_1_build_010.yml)의 paths 트리거, staging 디렉터리 검증, gzip 스크립트 경로를 새 버전 경로(`src/v{VER}/...`)로 현행화.
2. **에이전트 규칙 및 스킬/워크플로우 문서**:
   - [`.agents/rules/contract_review.md`](file:///d:/95.2540_PJT/80.Platformio_PJTs/esp32_AirMouse_002/.agents/rules/contract_review.md) 내 계약 산출물 경로 및 파일명(`*_0415.md`).
   - 스킬 및 에이전트 지침서([`agent.md`](file:///d:/95.2540_PJT/80.Platformio_PJTs/esp32_AirMouse_002/agent.md), [`CLAUDE.md`](file:///d:/95.2540_PJT/80.Platformio_PJTs/esp32_AirMouse_002/CLAUDE.md))의 활성 디렉터리, 파일 테이블, 계약 문서 목록.
3. **계약 문서 (5대 산출물)**:
   - `src/v{VER}/docs_v{VER}/contract_doc/` 내부 파일명(`*_0415.md`) 및 파일 간 상호 참조 링크 동기화.
4. **지식 베이스(Graphify) 갱신**:
   - `graphify update .`를 실행하여 새로운 버전 트리 및 변경된 AST 노드 반영.

---

## 8. 🚀 v0415 릴리스 상태 요약 (Current Status)

| 항목 | 대상 모듈 | 정리 및 현행화 내용 | 상태 |
|---|---|---|:---:|
| **디렉터리 및 소스** | `src/v0415/` | `v0415` 버전 업그레이드 및 41개 소스/헤더/스크립트 심볼, 인클루드, HW SSOT 전수 현행화 | ✅ 완료 |
| **빌드 환경** | `platformio.ini`, `main.cpp` | `data_dir = ./src/v0415/data_v0415`, `src_filter = +<v0415/> -<v0410/>`, `extra_scripts` 및 `main.cpp` 최신 연동 | ✅ 완료 |
| **Web UI & 번들** | `data_v0415_www/`, `data_v0415/` | HTML/CSS/JS 및 manifest/schema v0415 동기화, Gzip 사전 스테이징 검증 완료 | ✅ 완료 |
| **계약 산출물** | `docs_v0415/contract_doc/` | 5대 문서(CONTRACT, STATE, FLOW, BUDGET, SPEC) 최신 하드닝 및 Mermaid 14개 다이어그램 렌더링 무결성 전수 현행화 | ✅ 완료 |
| **검증 & 빌드** | PlatformIO / Graphify | `pio run -e esp32-s3-zero` 빌드 성공 (RAM 33.2%, Flash 46.2%), 지식 그래프 갱신 완료 | ✅ 완료 |
