# 📝 AirMouse Elite S3 (v0410) 종합 가이드 & 기술 사양서

> **ESP32-S3-Zero** 및 **MPU6050 6축 IMU** 기반 에어마우스 플랫폼의 **v0410** 정식 배포 기준 기술 사양 및 개발 가이드라인입니다.
> 
> **핵심 아키텍처:** 다중 프로파일(최대 5개) × 8×8 매크로 라이브러리 × Global/Mode 2단 슬롯 매트릭스(27개 고정 트리거) × Web UI 3-View 통합

* **소스 파일 버전 접미사**: 백엔드 `_0410` 통일 / 프론트엔드 `_0411` (7-모듈 구조)
* **스키마 버전**: Schema v5 (**`G_C10_CFG_VER = 410`**)
* **API 버전**: `G_W10_API_VER = 410`

---

## 📑 목차 (Table of Contents)

1. [AI 어시스턴트를 위한 핵심 요약 (Read First)](#1-ai-어시스턴트를-위한-핵심-요약-read-first)
2. [프로젝트 개요 & 핵심 특징](#2-프로젝트-개요--핵심-특징)
3. [시스템 아키텍처](#3-시스템-아키텍처)
   - [FreeRTOS 태스크 모델](#31-freertos-태스크-모델)
   - [데이터 흐름 및 파이프라인](#32-데이터-흐름-및-파이프라인)
   - [상태 소유권 및 동기화 매트릭스](#33-상태-소유권-및-동기화-매트릭스)
   - [Mutex 동기화 정책](#34-mutex-동기화-정책)
4. [프로젝트 디렉터리 구조](#4-프로젝트-디렉터리-구조)
   - [E10 모듈 7파일 분할 기준](#41-e10-모듈-7파일-분할-기준)
   - [프론트엔드 7-모듈 구조](#42-프론트엔드-7-모듈-구조)
5. [빌드 시스템 (PlatformIO CLI)](#5-빌드-시스템-platformio-cli)
6. [명명 규칙 & 코드 정책](#6-명명-규칙--코드-정책)
   - [명명 규칙 (Strict Naming Conventions)](#61-명명-규칙-strict-naming-conventions)
   - [ArduinoJson v7 전용 코딩 정책](#62-arduinojson-v7-전용-코딩-정책)
   - [안전 원자적 IO 정책 (Atomic Storage)](#63-안전-원자적-io-정책-atomic-storage)
7. [v0410 핵심 기능 상세 사양](#7-v0410-핵심-기능-상세-사양)
   - [7.1. 3-Mode 시스템 (PC / PPT / TV)](#71-3-mode-시스템-pc--ppt--tv)
   - [7.2. 2단 슬롯 매트릭스 (Global + Mode Override)](#72-2단-슬롯-매트릭스-global--mode-override)
   - [7.3. 4대 필수 슬롯 잠금 (🔒)](#73-4대-필수-슬롯-잠금-)
   - [7.4. 8×8 매크로 라이브러리 & 안전 취소 시퀀서](#74-88-매크로-라이브러리--안전-취소-시퀀서)
   - [7.5. M10 물리 모션 엔진 & M30 제스처](#75-m10-물리-모션-엔진--m30-제스처)
   - [7.6. Front Hold 스크롤](#76-front-hold-스크롤)
   - [7.7. SafeBoot & OTA Guard (method-aware)](#77-safeboot--ota-guard-method-aware)
   - [7.8. 다중 프로파일 시스템 (Schema v5)](#78-다중-프로파일-시스템-schema-v5)
   - [7.9. REST API 사양](#79-rest-api-사양)
8. [Web UI 3-View 프론트엔드 가이드](#8-web-ui-3-view-프론트엔드-가이드)
9. [하드웨어 사양 및 핀맵](#9-하드웨어-사양-및-핀맵)
10. [리팩터링 이력 (Phase 1 ~ Phase 11)](#10-리팩터링-이력-phase-1--phase-11)

---

## 1. 📌 AI 어시스턴트를 위한 핵심 요약 (Read First)

> [!IMPORTANT]
> 본 프로젝트의 코드를 수정하거나 기능을 추가하기 전, 반드시 다음 **10가지 핵심 규칙**을 숙지해야 합니다.

1. **모듈 약어 체계**:
   - `A40`(공용 유틸), `B20`(BLE), `C10`(설정/프로파일), `C20`(Action/버튼), `D10`(로거/진단), `E10`(에어마우스 핵심), `L10`(LED), `M10`(모션 물리엔진), `M20`(BiasTracker), `M30`(제스처), `P20`(전원), `W10`(웹서버/API).
   - 각 접두사는 파일명, 클래스명, 전역 심볼에 일관되게 반영되어야 합니다.
2. **엄격한 명명 규칙 준수**:
   - 전역 상수 `G_`, 전역 변수 `g_`, 클래스 `CL_`, 구조체 `ST_`, 열거형 `EN_`, private 멤버 `_` 접두사, 로컬 변수 `v_` 접두사, 매개변수 `p_` 접두사.
   - 명명 규칙 위반 시 리뷰 반려 대상입니다.
3. **ArduinoJson v7 단일화 정책**:
   - 오직 `JsonDocument` 단일 타입만 허용합니다.
   - `containsKey`, `createNestedArray`, `createNestedObject`, `StaticJsonDocument`, `DynamicJsonDocument` 사용 절대 금지.
   - 중첩 구조 접근 시 `doc["a"]["b"].to<JsonObject>()` 패턴을 사용합니다.
4. **E10 모듈 7파일 분할 유지**:
   - 헤더(`.h`) + `Core`, `Hid`, `Motion`, `Diag`, `Task`, `Action_0410.cpp`로 분할되어 있습니다. 기능 수정/추가 시 반드시 역할에 맞는 cpp 파일을 선택해야 합니다.
5. **태스크 및 상태 소유권 절대 준수**:
   - `_sensorTask`(Core 1)와 `_commTask`(Core 0)는 FreeRTOS 큐로만 통신합니다.
   - 웹 태스크(AsyncWebServer)는 HID 드라이버에 직접 접근할 수 없으며 반드시 커맨드 큐(`_qHidCmd`)를 경유해야 합니다.
   - `Special` 액션은 오직 `_sensorTask`에서만 단독 실행합니다.
6. **다중 프로파일 시스템 (v0410)**:
   - 최대 5개 프로파일(0~4). 단일 `config_0410.json`은 폐기되었으며, `/json/active_profile.json` (인덱스) 및 `/json/profiles/profile_N.json` 디렉토리 기반 원자적 IO로 동작합니다.
7. **2단 슬롯 매트릭스 & 리졸버**:
   - 27개 고정 트리거(Button 15개, Flick 4개, Linear 4개, Tilt Hold 4개)에 대해 `Global` 공통 기본값과 `Mode 1~3` 모드별 오버라이드를 분리하여 `_resolveSlot(mode, trigIdx)`로 해석합니다. **`_resolveSlot`은 `_lock()` 하에 스냅샷** (프로파일 스위치 중 부분 갱신 방지).
8. **8×8 매크로 라이브러리 & 비동기 상태머신**:
   - 프로파일당 최대 8개 매크로 × 8개 Step(0~2000ms delay). **commTask 블로킹 없이** `_startMacro()` (스냅샷) + `_tickMacro()` (상태머신) 방식으로 실행.
   - **취소는 `_macroAbortToken` 카운터 방식** (bool 재실행 초기화 경합 제거).
9. **4대 필수 슬롯 잠금 (🔒)**:
   - S1(좌클릭), S5(무브게이트), S12(모드순환), S13(BLE페어링)은 브릭 방지를 위해 서버 및 클라이언트 양측에서 수정이 원천 차단됩니다.
10. **프론트엔드 7-모듈 구조 (`_0411`)**:
    - `am_base` → `am_offline` → `am_profile` → `am_macro` → `am_config` → `am_status` → `app_0411_0001` 순 로드. 모든 파일명은 `_NNNN` 규칙 준수(immutable 캐시).

---

## 2. 🎯 프로젝트 개요 & 핵심 특징

ESP32-S3-Zero와 MPU6050을 기반으로 마우스패드 없이 공중에서 3차원 움직임을 감지하는 자이로 전용 공간 포인팅 에어마우스입니다. BLE HID Composite(Mouse + Keyboard + Consumer) 인터페이스를 통해 PC/Mac/스마트TV에 연결되며, 내장 웹 서버를 통해 3-View 실시간 커스터마이징을 제공합니다.

### 🌟 핵심 특징
* **다중 프로파일 시스템 (Schema v5, `ver=410`)**: 최대 5개 독립 프로파일(작업/게임/발표/미디어 등) 지원, 인덱스 기반 원자적 스위칭.
* **Global + Override 2단 슬롯 매트릭스**: 27개 고정 트리거 대상 Global 공통 기본값 + Mode 1~3(PC/PPT/TV) 오버라이드 마스크 운용. `_resolveSlot`은 락 보호.
* **8×8 매크로 라이브러리 (비동기)**: 최대 8개 매크로, 각 8단계(키/마우스/소비자키 + 0~2000ms 딜레이). commTask 블로킹 없는 상태머신 + 토큰 기반 취소.
* **4대 필수 기능 잠금 🔒**: 좌클릭/포인팅게이트/모드순환/BLE페어링 수정 차단으로 조작 불능(브릭) 원천 방지.
* **3-Mode 시스템**: PC(1: 파랑) / Presentation(2: 초록) / Smart TV(3: 주황) 상황별 슬롯 매트릭스 자동 매핑.
* **자이로 기반 공간 포인팅**: 상보 필터 + 시그모이드(Sigmoid) 가속 곡선 + 적응형 LPF + Zero Snap 탑재.
* **제스처 3계층 & Front Hold**: Flick (P2P) / Linear (임펄스) / Tilt Hold (Mode 3 전용 D-Pad) 및 Side F 스크롤(커서 감쇠 + 휠/팬).
* **SafeBoot / OTA Guard**: 부팅 실패 카운트 기반 브릭 방지, method-aware API 게이트, OTA 중 HID 차단.
* **Web UI 3-View 프론트엔드**: Profile Bar, Slot Editor, Macro Editor, Config, Diag/OTA 통합 (7-모듈 + 오프라인 시뮬레이터).

---

## 3. 🏗️ 시스템 아키텍처

### 3.1. FreeRTOS 태스크 모델

| 태스크명 | 할당 Core | 우선순위 (Priority) | 실행 주기 | 스택 크기 (Stack) | 주요 역할 |
|:---|:---:|:---:|:---:|:---:|:---|
| `_sensorTask` | Core 1 | 3 (High) | 8ms (125Hz) | 8192 B | IMU 읽기, FSM 처리, 모션 계산, 프레임/커맨드 생성, **Special 액션 단독 실행**, `_reqReset*` 플래그 소비 |
| `_commTask` | Core 0 | 2 (Mid) | 7ms | 4096 B | 큐 소비 → BLE HID 전송, **`_startMacro()` + `_tickMacro()` 비동기 상태머신**, repeat tick |
| `_ledTask` | Core 0 | 1 (Low) | 50ms | 2048 B | WS2812 LED 상태머신 **단독** Tick |
| `AsyncWebServer` | Core 0 | - | 이벤트 구동 | - | HTTP 요청 처리, E10 제어 요청 큐잉 (`_qHidCmd`, `_qActionExec`) |
| `main loop` | Core 0 | - | 200ms | - | Grace time 통과 판정, `tickConfigSave()` (BLE dirty 저장) |

### 3.2. 데이터 흐름 및 파이프라인

```
sensorTask (Core 1) ──push──> _qFrame (size=1, overwrite) ──recv──> commTask (Core 0) ──> BLE HID
       │                                                                  ▲
       │ (상태 관측)                                                      │
       └──lock──> _state ──read──> getStatus() (웹 API)                   │
                                                                          │
sensorTask ──enqueue──> _qActionExec (size=8) ──recv──────────────────────┤
web/sensorTask ──enqueue──> _qHidCmd (size=4) ──recv──────────────────────┘

Special 액션:  sensorTask에서 _handleSpecial() 직접 호출 (commTask 경유 금지)
매크로:        commTask에서 _startMacro() (스냅샷, 즉시 리턴) + _tickMacro() (매 루프 진행)
LED 상태머신:  _ledTask(50ms) 단독 구동
Bias reset:    sensorTask가 _reqGyroCalib 플래그 처리 시 _biasTracker.reset() 실행
BtnDisp/Gest:  web 태스크는 _reqReset* 플래그만 설정 → sensorTask가 실제 리셋
```

**절대 금지:**
- 웹 태스크가 `_mouse`/`_keyboard` 직접 호출 → 반드시 `_qHidCmd` 경유
- sensorTask가 HID 직접 호출 → 반드시 `_qActionExec` 경유
- commTask가 `_state` 접근 → `_qFrame`만 사용
- commTask가 `_handleSpecial()` 실행 → **sensorTask 단독**
- commTask가 `_led.tick()` 호출 → **_ledTask 단독**
- 매크로 실행 블로킹 → **스텝 단위 상태머신 (`_tickMacro`)**
- web 태스크가 `_biasTracker.reset()` / `_btnDisp.resetAll()` / `_gesture.reset()` 직접 호출 → **sensorTask 위임**

### 3.3. 상태 소유권 및 동기화 매트릭스

| 상태 / 리소스 | 소유자 | 접근 방식 |
|---|---|---|
| `_state` (`ST_E10_State_t`) | sensorTask (관측), web (setSafeMode 등) | `_lock()` / `_unlock()` 하에 read/write |
| `_qFrame` | sensorTask (write), commTask (read) | FreeRTOS queue (size=1, overwrite, lock-free) |
| `_qActionExec` | sensorTask / web (write), commTask (read) | FreeRTOS queue (timeout=0, 블로킹 금지) |
| `_qHidCmd` | web (write), commTask (read) | FreeRTOS queue (timeout=0, 블로킹 금지) |
| `_cfgProfile` | `_lock()` 보호 | 프로파일 로드/저장/스냅샷 (C-4) |
| `_macroAbortToken` | 모든 태스크 (증가만) | `volatile uint32_t` (H-1) |
| `_macroState.active` | 모든 태스크 | `volatile bool` |
| `_macroSnapshot` | `_startMacro` 스냅샷 | 락 하 스냅샷 후 참조 (H-4) |
| `_reqResetBtnDisp` / `_reqResetGesture` | web set / sensorTask consume | `volatile bool` (H-3) |
| `_reqGyroCalib` | web set / sensorTask consume | `volatile bool`, sensorTask가 `_biasTracker.reset()` 실행 (D-1) |
| `_hid` (`BleCompositeHID`) | **commTask 단독 접근** | web/sensorTask는 직접 호출 절대 금지 |
| `_mpu`, `Wire` | **sensorTask 단독 접근** | I2C 복구도 sensorTask 단독 |
| `_biasTracker` | **sensorTask 단독** | reset도 sensorTask |
| `_led` | **_ledTask 단독 tick** | 색상 설정/플래시는 모든 태스크에서 가능 |
| `_frontHoldActive` / `_moveGateHeld` / `_activeMode` | sensorTask | `volatile`, 원자적 |
| `_ble._pairing` / `_dirty` | sensorTask + web | `volatile bool` (M-1) |

### 3.4. Mutex 동기화 정책
- `_mutex`: **Recursive Mutex** (`xSemaphoreCreateRecursiveMutex`). 이유: `setSafeMode`/`setOtaGuard`/`_recoverI2C`가 락 보유 중 `_pushErr` 재진입.
- `_lock()` / `_unlock()`: RAII 아님. 모든 경로에서 짝 맞춤.
- 웹 태스크 큐 인큐 시 `timeout=0`으로 설정하여 웹 워커 스레드가 블로킹되지 않도록 보장.

---

## 4. 📁 프로젝트 디렉터리 구조

### 4.0. 백엔드 (`src/v0410/`)

```
src/
├── main.cpp                            # Setup, Loop, W10-E10 브릿지 바인딩
└── v0410/
    ├── A40_ComFunc_0410.h              # 공용 유틸 (JSON, Mutex, IO)
    ├── B20_Ble_0410.h / .cpp           # BLE Manager (Pairing, Multi-Host Cycle)
    ├── C10_Config_0410.h / .cpp        # 다중 프로파일 관리 & LittleFS 원자적 I/O
    ├── C10_Def_0410.h                  # Schema v5, Profile/MacroLib 데이터 구조체
    ├── C20_Action_0410.h               # Action Registry (EN_C20_ACT_MACRO 포함)
    ├── C20_ActionExec_0410.h / .cpp    # Action 실행기 (HID 레포트 패킷 조립)
    ├── C20_BtnDispatcher_0410.h / .cpp # 버튼 디바운스 & 더블/롱클릭 상태머신
    ├── D10_Logger_0410.h               # 링버퍼 진단 로거
    ├── E10_Def_0410.h                  # E10 상수 및 데이터 타입
    ├── E10_AirMouse_0410.h             # E10 클래스 선언
    ├── E10_AirMouse_Core_0410.cpp      # 초기화, 프로파일 스위칭, _resolveSlot(락)
    ├── E10_AirMouse_Hid_0410.cpp       # HID primitives & forceRelease (macro 토큰)
    ├── E10_AirMouse_Motion_0410.cpp    # Precision FSM & 적응형 필터
    ├── E10_AirMouse_Diag_0410.cpp      # 진단/캘리브(bias reset 위임, ble.tick 포함)
    ├── E10_AirMouse_Task_0410.cpp      # sensorTask, commTask, ledTask (+매크로 tick)
    ├── E10_AirMouse_Action_0410.cpp    # 27개 트리거 매핑, _startMacro/_tickMacro, Special
    ├── L10_Led_0410.h / .cpp           # WS2812 NeoPixel 비동기 제어기
    ├── M10_MotionProc_0410.h           # 자이로 6축 물리 엔진 (Roll/Pitch/Yaw)
    ├── M20_BiasTracker_0410.h          # 실시간 Zero-rate Bias 자동 보정기
    ├── M30_Gesture_0410.h / .cpp       # Flick / Linear / Tilt Hold (Tilt은 Mode 3 전용)
    ├── P20_Power_0410.h / .cpp         # Light-sleep & WoM(Wake-on-Motion) 전원 관리
    ├── W10_Def_0410.h                  # 웹 서버 데이터 구조체 (ST_W10_E10If_t)
    ├── W10_Web_0410.h                  # AsyncWebServer 관리자
    ├── W10_Web_init_0410.cpp           # WiFi AP/STA 및 라우트 초기화
    ├── W10_Web_Static_0410.cpp         # 정적 웹 파일(Gzip) 서빙 & Diag 이벤트
    ├── W10_WebApi_Com_0410.cpp         # API 게이트웨이, SafeMode (method-aware)
    ├── W10_WebApi_Config_0410.cpp      # /api/config/* (프로파일 기반 패치)
    ├── W10_WebApi_Profile_0410.cpp     # /api/profiles/*, /api/triggers, /api/action/test*
    ├── W10_WebApi_CtlPpt_0410.cpp      # /api/control, /api/ppt/test
    ├── W10_WebApi_OtaBoot_0410.cpp     # OTA, SafeBoot, Reboot, reboot/check
    ├── W10_WebApi_Status_0410.cpp      # /api/status/diag/keycodes (+ config.profile_*)
    ├── tools_v0410/pio_gzip_0410.py    # Gzip 빌드 스크립트
    ├── data_v0410_www/                 # 웹 프론트엔드 원본 소스 (VCS 대상)
    └── data_v0410/                     # LittleFS 플래시 이미지 디렉토리
        ├── www/                        # 압축된 Gzip 웹 산출물 (.gz)
        │   ├── index_0411.html.gz
        │   ├── style_0411.css.gz
        │   ├── app_0411_0001.js.gz
        │   └── lib/
        │       ├── am_base_0411.js.gz
        │       ├── am_offline_0411.js.gz
        │       ├── am_profile_0411.js.gz
        │       ├── am_macro_0411.js.gz
        │       ├── am_config_0411.js.gz
        │       └── am_status_0411.js.gz
        └── json/                       # (자동 생성 가능)
            ├── active_profile.json
            ├── boot_state_0410.json
            ├── profiles/
            │   ├── profile_0.json
            │   └── profile_N.json
            └── public/
                └── manifest_0410.json
```

### 4.1. E10 모듈 7파일 분할 기준

| 파일 | 역할 및 분할 기준 |
|---|---|
| `Core` | `begin`, 프로파일 스위칭/재로드, `_resolveSlot`(**C-4 락**), `saveActiveProfile`, `execLiveTest`, setter 원자적 RMW, `tickConfigSave` |
| `Hid` | 마우스/키보드/소비자키 탭 및 홀드 프리미티브, `forceReleaseButtons` (**`_macroAbortToken++`**), `_doReleaseAllButtons` |
| `Motion` | Precision Mode FSM, 적응형 LPF 가속 계산 |
| `Diag` | 자이로 캘리브(**D-1 bias reset 위임, D-2 `_ble.tick()`**), I2C 복구, `getStatus`, `clearDiagnostics`(**D-3 플래그만**) |
| `Task` | `sensorTask` (**H-3 리셋 플래그 소비**), `commTask` (`_startMacro`/`_tickMacro`), `ledTask` (단독 구동) |
| `Action` | `_handleHardcodedButton` (**C-1 Top M CLICK 슬롯 위임**), `_handleSlotButton`, `_handleGesture`, `_setActiveMode`, `_handleSpecial`, `_startMacro` (**H-4 스냅샷**), `_tickMacro` (**H-1 토큰**) |
| `.h` | 클래스 선언, 인라인 유틸 (`_lock`, `_unlock`, `_pushFrame`, `_mouseSend`, `_welfordAdd`, `_calcRms`) |

### 4.2. 프론트엔드 7-모듈 구조

```
www/
├── index_0411.html              # 메인 UI
├── style_0411.css               # 스타일 (배너/스피너/Quick Tuning/Tilt)
├── app_0411_0001.js             # UI 바인딩 + Main (7-모듈 로더)
└── lib/
    ├── am_base_0411.js          # 상수/유틸/API 래퍼/전역/로딩 헬퍼
    ├── am_offline_0411.js       # 오프라인 시뮬레이터 (localStorage 스키마 v2)
    ├── am_profile_0411.js       # 프로파일 CRUD/Slots/ActionEditor
    ├── am_macro_0411.js         # 매크로 편집기 (8×8)
    ├── am_config_0411.js        # E10 Config/Control/SafeBoot/Factory
    └── am_status_0411.js        # Status/Diag/OTA/재부팅 배너/Quick Tuning
```

**로드 순서 (index_0411.html)**:
```html
<script src="./lib/am_base_0411.js"></script>
<script src="./lib/am_offline_0411.js"></script>
<script src="./lib/am_profile_0411.js"></script>
<script src="./lib/am_macro_0411.js"></script>
<script src="./lib/am_config_0411.js"></script>
<script src="./lib/am_status_0411.js"></script>
<script src="./app_0411_0001.js"></script>
```

**캐시 정책 (`W10_hasVersionToken`)**:
- HTML, `/json/public/*`, `/api/*`: `no-store`
- `_NNNN` 규칙 준수 파일(`am_base_0411.js` 등): `immutable` (max-age=31536000)
- 나머지 정적: `short` (1h)

---

## 5. 🛠️ 빌드 시스템 (PlatformIO CLI)

### 필수 명령어

- **펌웨어 컴파일**:
  ```powershell
  pio run -e esp32-s3-zero
  ```
- **펌웨어 플래시 업로드**:
  ```powershell
  pio run -e esp32-s3-zero -t upload
  ```
- **LittleFS 파일시스템 빌드 & 업로드**:
  ```powershell
  pio run -e esp32-s3-zero -t buildfs
  pio run -e esp32-s3-zero -t uploadfs
  ```
- **빌드 캐시 클린**:
  ```powershell
  pio run -e esp32-s3-zero -t clean
  ```

### 리소스 사용 (v0410 최종)

| 항목 | 값 |
|---|---|
| RAM | ~40% (131 KB / 320 KB) |
| Flash | ~45% (1.42 MB / 3.14 MB) |

---

## 6. 📐 명명 규칙 & 코드 정책

### 6.1. 명명 규칙 (Strict Naming Conventions)

| 구분 | 접두사 / 규칙 | 예시 |
|---|---|---|
| **네임스페이스** | `모듈명_` | `A40_ComFunc`, `C10_DEF` |
| **전역 상수 / 매크로** | `G_모듈명_` | `G_C10_CFG_VER`, `G_W10_API_VER` |
| **전역 변수** | `g_모듈명_` | `g_cfg`, `g_e10`, `g_w10E10If` |
| **구조체 타입** | `ST_모듈명_` / `_t` | `ST_C10_ProfileConfig_t` |
| **열거형 상수** | `EN_모듈명_` | `EN_C20_ACT_MACRO` |
| **클래스명** | `CL_모듈명_` | `CL_E10_EliteAirMouse` |
| **private 멤버** | `_` 접두사 | `_macroAbortToken`, `_resolveSlot` |
| **로컬 변수** | `v_` 접두사 | `v_idx`, `v_doc`, `v_ok` |
| **함수 매개변수** | `p_` 접두사 | `p_idx`, `p_name`, `p_out` |

### 6.2. ArduinoJson v7 전용 코딩 정책
- 오직 `JsonDocument doc;` 단일 인스턴스만 사용.
- `containsKey`, `createNestedArray`, `createNestedObject`, `StaticJsonDocument`, `DynamicJsonDocument` 사용 절대 금지.
- 중첩 객체/배열은 `doc["slots"]["global"].to<JsonArray>()` 패턴 준수.

### 6.3. 안전 원자적 IO 정책 (Atomic Storage)
- `.tmp` 파일 생성 → 파싱 및 무결성 검증 → 기존 파일 `.old` 백업 → `.tmp`를 원본으로 원자적 Rename → 검증 → `.old` 제거.
- 쓰기 실패 시 자동으로 `.old`에서 복원.
- 프로파일 인덱스(`active_profile.json`)는 별도 `.tmp` + rename 방식.

---

## 7. 🚀 v0410 핵심 기능 상세 사양

### 7.1. 3-Mode 시스템 (PC / PPT / TV)

| Mode | 이름 | LED 기본 색상 | 주요 용도 |
|:---:|---|:---:|---|
| **1** | **PC Air Mouse** | 🔵 Blue | 데스크톱 마우스 포인팅, 드래그, 윈도우 단축키, 스크롤 |
| **2** | **Presentation** | 🟢 Green | 파워포인트/슬라이드 넘기기, 레이저 포인터, 블랙아웃 |
| **3** | **Smart TV** | 🟠 Orange | 스마트 TV 리모컨, D-Pad 네비게이션, 볼륨/채널 제어 |

- **모드 전환**: Side C 더블클릭 시 `1 → 2 → 3 → 1` 순환. 전환 시 흰색 500ms Flash 점등.
- **모드 전환 시 안전 처리**: `_macroAbortToken++` (매크로 즉시 취소) + `_macroState.active = false`, 모든 키/버튼 Release, FSM 리셋, `_frontHoldActive = false`, 페어링 취소, LED 재설정.

### 7.2. 2단 슬롯 매트릭스 (Global + Mode Override)

27개 고정 트리거에 대해 **Global 기본값**과 **Mode 1~3별 Override 마스크(`mask`)**를 운용하여 메모리를 절약하고 일관성을 보장합니다.

```cpp
// E10_AirMouse_Core_0410.cpp: _resolveSlot (C-4 락 보호)
ST_C20_ActionSlot_t CL_E10_EliteAirMouse::_resolveSlot(uint8_t p_mode, uint8_t p_trig) const {
    auto* v_self = const_cast<CL_E10_EliteAirMouse*>(this);
    v_self->_lock();

    ST_C20_ActionSlot_t v_out;
    if (!_cfgProfileValid) {
        v_out = { EN_C20_ACT_NONE, EN_C20_HOLD_NONE, 0, 0 };
    } else {
        v_out = C10_ResolveSlot(_cfgProfile.slots, p_mode, p_trig);
    }

    v_self->_unlock();
    return v_out;
}
```

### 7.3. 4대 필수 슬롯 잠금 (🔒)

기기 조작 불능(브릭)을 방지하기 위해 다음 4개 트리거는 서버(`G_C10_TRIG_LOCKED[]`) 및 웹 클라이언트(`am_profile_0411.js`) 양측에서 수정을 원천 차단합니다:

| 슬롯 Index | 트리거 이름 | 기능 | 잠금 사유 |
|:---:|---|---|---|
| **0 (S1)** | Top L Click | 마우스 좌클릭 홀드 | 기본 선택 및 드래그 보장 |
| **4 (S5)** | Top M Hold | Move Gate | 포인팅 활성화 게이트 보장 |
| **11 (S12)** | Side C Double | Mode Cycle | PC ↔ PPT ↔ TV 모드 순환 보장 |
| **12 (S13)** | Side C 2s Hold | BLE Pairing Mode | 페어링 모드 진입 보장 |

### 7.4. 8×8 매크로 라이브러리 & 안전 취소 시퀀서

- **규격**: 프로파일당 최대 8개 매크로 정의 가능, 매크로당 최대 8개 Step 실행.
- **Step 구성**: Action Kind(키보드/마우스/소비자키) + 파라미터 + `delayMs` (0~2000ms).
- **중첩 방지**: 매크로 Step 내에는 `SPECIAL` 및 `MACRO` 사용 불가 (validateMacroStep).
- **비동기 상태머신 실행 (C-3 개선)**:
  - `commTask`는 큐에서 MACRO kind 수신 시 `_startMacro(idx)` 호출 → 스냅샷만 취하고 **즉시 리턴**.
  - 매 루프 후반에 `_tickMacro()` 호출 → delay 경과 시 다음 step 실행. **커서 프레임 소비 지속** (블로킹 없음).
- **토큰 기반 취소 (H-1 개선)**:
  - `_macroAbortToken` 카운터와 `_macroState.startToken` 비교.
  - 취소 트리거: `switchProfile`, `_setActiveMode`, `forceReleaseButtons`, BLE disconnect edge, SafeMode/OTA gate 진입.
  - **재실행 시 초기화 경합 제거** (bool 재실행 문제 해결).

### 7.5. M10 물리 모션 엔진 & M30 제스처
- **상보 필터**: 가속도계와 자이로스코프를 융합하여 Roll/Pitch/Yaw 3축 각속도 추출.
- **시그모이드 가속 곡선**: 손목의 미세한 떨림은 흡수하고 빠른 회전에는 높은 배율 적용.
- **적응형 LPF**: `alpha = (delta > 3.0) ? 0.50 : 0.12`
- **Zero Snap**: `|out| < 0.6 → 0`
- **3계층 제스처** (Mode별 활성 조건):

| 제스처 | 활성 Mode | 조건 |
|---|---|---|
| Flick L/R | 모든 Mode | 항상 (gz 기반 회전) |
| Flick U/D | 모든 Mode | Middle Hold 해제 시 |
| Linear | 모든 Mode | Middle Hold 중 |
| **Tilt Hold** | **Mode 3 전용** | Middle Hold 해제 + 자세 유지 300ms |

### 7.6. Front Hold 스크롤
- **동작**: Side F 버튼을 누르고 있는 동안 마우스 커서 속도가 25%로 감쇠되며, 상하 틸트로 수직 휠, 좌우 틸트로 수평 팬 스크롤을 수행합니다.
- **모든 Mode 일관**.

### 7.7. SafeBoot & OTA Guard (method-aware)

- 부팅 실패 카운트가 2회 이상이면 SafeMode로 진입하여 AP SSID에 `-SAFE`를 붙이고 안전 API만 허용.
- OTA 펌웨어 업로드 중에는 HID 입력을 원천 차단하여 벽돌 방지.

**SafeMode 허용 API (method-aware)**:
- 전체 허용: `/api/status`, `/api/diag`, `/api/diag/clear`, `/api/keycodes`, `/api/safeboot`, `/api/ota`, `/api/ota/status`, `/api/factory_reset`, `/api/reboot`, `/api/reboot/check`, `/api/config/export`, `/api/config/rollback`
- **GET만 허용**: `/api/profiles/active`, `/api/profiles`, `/api/triggers`
- **차단**: `/api/config/save`, `/api/config/apply`, `/api/config/import`, `/api/control`, `/api/ppt/*`, `/api/profiles/switch`, `/api/profiles/create`, `/api/profiles/delete`, `/api/profiles/rename`, `/api/action/test`, `/api/action/test_macro`

### 7.8. 다중 프로파일 시스템 (Schema v5)

- **저장 위치**: LittleFS `/json/active_profile.json` (인덱스) 및 `/json/profiles/profile_N.json`.
- **프로파일 용량**: 최대 5개 독립 프로파일.
- **원자적 저장**: `.tmp` → 검증 → `.old` → 원자적 Rename → 검증 → `.old` 제거.
- **인덱스 스키마**:
  ```json
  { "active_index": 0, "profile_count": 1 }
  ```
- **프로파일 스키마**:
  ```json
  {
    "ver": 410,
    "name": "Default",
    "wifi": { "mode": 0, "sta": {...}, "ap": {...}, "mdns": {...} },
    "e10": { /* DPI, 감도, 제스처, precision, bias, ... */ },
    "slots": {
      "global": [ /* 27개 슬롯 */ ],
      "modes": [
        { "mask": 0, "slots": [] },
        { "mask": 134217727, "slots": [ /* 27개 */ ] },
        { "mask": 134217727, "slots": [ /* 27개 */ ] }
      ]
    },
    "macros": [ /* 최대 8개 */ ]
  }
  ```

### 7.9. REST API 사양

| Method | Endpoint | 설명 |
|---|---|---|
| `GET` | `/api/status` | 전체 시스템/센서/프로파일 상태 스냅샷 (**config.profile_idx/name/count 포함**) |
| `GET` | `/api/diag` | 통신 에러 카운터 및 최근 이벤트 로그 |
| `POST`| `/api/diag/clear` | 진단 카운터 리셋 |
| `GET` | `/api/keycodes` | 키코드/메타데이터 (action_kinds, specials, consumer, triggers, slots_meta, groups, directions) |
| `GET` | `/api/triggers` | 27개 고정 트리거 및 4대 잠금(locked) 메타데이터 |
| `GET` | `/api/profiles` | 프로파일 목록 및 활성 인덱스 조회 |
| `GET` | `/api/profiles/active` | 현재 활성 프로파일 전체 JSON 조회 |
| `POST`| `/api/profiles/active` | 활성 프로파일 설정 패치 및 즉시 저장 |
| `POST`| `/api/profiles/switch` | `{"idx": N}` 활성 프로파일 전환 |
| `POST`| `/api/profiles/create` | `{"name": "..."}` 신규 프로파일 생성 (최대 5개) |
| `POST`| `/api/profiles/delete` | `{"idx": N}` 프로파일 삭제 (최소 1개 보장) |
| `POST`| `/api/profiles/rename` | `{"idx": N, "name": "..."}` 프로파일 이름 변경 |
| `POST`| `/api/action/test` | Action Live Test 비동기 실행 |
| `POST`| `/api/action/test_macro` | Macro Live Test 비동기 실행 |
| `POST`| `/api/ppt/test` | 단발 키 테스트 (KB/Consumer) |
| `GET` | `/api/config/export` | 활성 프로파일 JSON 다운로드 |
| `POST`| `/api/config/import` | 프로파일 JSON 업로드 |
| `POST`| `/api/control` | 빠른 제어 명령 (PPT/DPI/Precision/SafeMode/OTA Guard/I2C/Gyro) |
| `POST`| `/api/ota` | 백그라운드 OTA 펌웨어 업로드 |
| `GET` | `/api/ota/status` | OTA 진행 상태 폴링 |
| `GET` | `/api/safeboot` | SafeBoot 상태 확인 |
| `POST`| `/api/safeboot` | `{exit:true}` SafeMode 해제 + 재부팅 |
| `POST`| `/api/factory_reset` | 공장 초기화 |
| `POST`| `/api/reboot` | 안전 재부팅 |
| `GET` | `/api/reboot/check` | 재부팅 필요 확인 (`required`, `mask`, `reasons`) |

---

## 8. 🖥️ Web UI 3-View 프론트엔드 가이드

* **상단 배너**: 재부팅 필요 시 조건부 표시 (`policy.reboot_required` 감지)
* **Profile Bar (상단 고정)**:
  - 프로파일 드롭다운 전환 (busy 플래그로 재진입 방지), `+ 새로`, `이름`, `삭제` (1개 남았을 때 보호 + 활성 경고), `재로드(↻)`
* **Dashboard 탭**:
  - 시스템 Uptime, Heap, WiFi, BLE 상태, 활성 프로파일 표시 (`config.profile_*`)
  - Quick Control: PPT ON/OFF, 자이로 캘리브 (RMS/Bias 피드백), 강제 릴리즈, I2C 복구 (성공/실패 피드백)
  - **Quick Tuning** (apply-only): DPI 1/2/3, Precision OFF/LOW/MED/HIGH/PPT
  - **SafeMode 진입/해제** (세션 유지), **Host Cycle**, **SafeBoot 해제**, **Factory Reset** (진행 표시)
* **Slots 탭 (Slot Editor)**:
  - 뷰 토글: `Global` / `Mode 1 · PC` / `Mode 2 · PPT` / `Mode 3 · TV`
  - 27개 고정 트리거 대상 액션 종류 지정
  - **KB 드롭다운** (usage 숫자 대신 이름 그룹화: 없음/알파벳/숫자/기본/펑션/네비/기타)
  - `[G]` 기본값 ↔ `[Mx]` 오버라이드 뱃지 원클릭 토글
  - **Tilt 그룹 Mode 3 제한**: Mode 1/2 뷰에서 회색 + 사선 + 편집 비활성 (경고 배너)
  - **slots_meta 부가 라벨**: `Top L Click (TOP_L / CLICK)` 형태
  - 4대 필수 슬롯 🔒 잠금 표시 및 편집 차단
  - 각 슬롯별 즉시 시험 실행 `Test` 버튼 (최신 배열 재조회로 stale 회피)
* **Macros 탭 (Macro Editor)**:
  - 좌측: 등록된 매크로 리스트 (최대 8개)
  - 우측: 매크로 편집기 (이름, 최대 8개 Step, 파라미터 빌더, 0~2000ms 딜레이)
  - 스텝 순서 변경(`▲`/`▼`), 삭제(`✕`), 새 스텝 추가
  - 매크로 즉시 실행 `Test` 버튼 (저장 후 test_macro API 호출)
* **Config 탭**:
  - E10 파라미터 (DPI, 가속도, 휠 틸트 각도, 스크롤 커서 감쇠) 조정
  - Export (서버 파일 사용), Import
  - **Rollback (비활성 안내)** — v0410 폐기, 프로파일 스위치 또는 Factory Reset 권장
* **Diagnostics & OTA 탭**:
  - 통신 오류 카운터, 이벤트 로그, **카운터 초기화** 버튼
  - **Key Test** (KB/Consumer 단발 키 테스트)
  - 무선 펌웨어 업데이트 (OTA Guard 스위치 포함)
* **전역 로딩 오버레이**: 프로파일 전환/삭제/캘리브/Factory 진행 중 스피너 표시

---

## 9. 🔌 하드웨어 사양 및 핀맵

```
      [Top 면]
   [L] [M] [R]      ← 앞쪽 3버튼
   
   ┌─────────────┐
[F]│             │
[C]│             │  ← 좌측면 3버튼 (Front/Center/Rear)
[R]│             │
   └─────────────┘
```

| 버튼 / 핀 | GPIO | 기본 하드코딩 동작 |
|---|:---:|---|
| **Top L** | GPIO 12 | 마우스 좌클릭 홀드 (선택/드래그) |
| **Top M** | GPIO 16 | Move Gate (누르고 있을 때 커서 활성화). CLICK/LONG은 슬롯 매핑(S4) 위임 |
| **Top R** | GPIO 15 | 마우스 우클릭 (슬롯 설정 가능) |
| **Side F** | GPIO 14 | Front Hold 스크롤 (누르고 있을 때 틸트 스크롤) |
| **Side C** | GPIO 13 | 더블클릭: Mode 순환 / 2초 홀드: BLE 페어링 / 3초+Top L: 호스트 순환 |
| **Side R** | GPIO 7 | 보조 기능 (볼륨 다운 등 슬롯 설정 가능) |
| **I2C SDA** | GPIO 4 | MPU6050 센서 통신 |
| **I2C SCL** | GPIO 5 | MPU6050 센서 통신 |
| **MPU INT1** | GPIO 6 | Motion Detection (WoM wake) |
| **WS2812 LED** | GPIO 21 | Mode 표시등 (Core 0 ledTask 전용 구동) |

**MPU6050 설정**: Gyro Range 250°/s, Accel Range 2G, DLPF 21Hz, I2C 400kHz, INT open-drain/active-low/latch.

**좌표계**:
| 물리 축 | 사용자 용어 | 용도 |
|---|---|---|
| gx | Roll (긴 축) | 커서 Y, Linear U/D, Flick U/D, Front Hold 수평 팬 |
| gy | Pitch (좌우 축) | 휠, Linear L/R, Front Hold 수직 휠 |
| gz | Yaw (수직 축) | 커서 X, Flick L/R |

---

## 10. 🗂️ 리팩터링 이력 (Phase 1 ~ Phase 11)

- **Phase 1~4 (E10 핵심 안정화)**: 큐 인큐 누락 방지, recursive mutex 도입, 캘리브레이션 비차단 샘플링, 웹 태스크 직접 호출 차단, 공유 변수 락, 전용 에러코드.
- **Phase 5 (3-Mode & 슬롯 매핑)**: PC / PPT / TV 3-Mode 시스템 구축.
- **Phase 6-J (LED 서브시스템)**: WS2812 전용 `_ledTask` 도입.
- **Phase 7 (제스처 엔진)**: M30 Flick / Linear / Tilt Hold 제스처 3계층 완성.
- **Phase 8 (전원 관리)**: Light-sleep + WoM (EXT1 wake) 및 저전력 모드 연동.
- **Phase 9~10 (BLE Multi-Host)**: 페어링 모드 및 3개 호스트 순환 재연결.
- **Front Hold 스크롤**: Side F 누름 시 커서 감쇠 및 틸트 기반 2축 스크롤.

- **Phase 11 (v0410 정식 배포: 다중 프로파일 & 매크로 라이브러리)**:
  - 다중 프로파일(Schema v5, `ver=410`, 최대 5개) 및 LittleFS 원자적 디렉토리 I/O.
  - 27개 고정 트리거 대상 Global 공통 기본값 + Mode 1~3 오버라이드 매트릭스 및 슬롯 리졸버.
  - 8×8 매크로 라이브러리 및 안전 취소 시퀀서.
  - 4대 필수 슬롯 잠금 🔒 원천 차단.
  - Web UI 3-View (Profile Bar, Slot Editor, Macro Editor, Config, Diag/OTA) 전면 개편.

- **Phase 11 후속 (프론트엔드 안정성 + 누락 기능 + 백엔드 크리티컬 패치)**:

  **백엔드 크리티컬 수정 (C-1~C-4, H-1~H-4, M-1, M-2, D-1~D-3)**
  - C-1: Top M CLICK 슬롯(S4) 도달 (하드코딩 제거)
  - C-2: `switchProfile` HID 직접 호출 제거 → `forceReleaseButtons()` 큐 경유
  - C-3: 매크로 블로킹 제거 → `_startMacro` + `_tickMacro` 상태머신
  - C-4: `_resolveSlot` 락 보호
  - H-1: `_macroAbortToken` 카운터 (bool 재실행 초기화 경합 제거)
  - H-2: `switchProfile` 큐 드레인
  - H-3: `_reqResetBtnDisp` / `_reqResetGesture` 플래그 위임
  - H-4: `_startMacro` 스냅샷 (OOB 방지)
  - M-1: `_pairing` / `_dirty` volatile
  - M-2: SafeMode 게이트 method-aware
  - D-1: `_biasTracker.reset()` sensorTask 위임
  - D-2: 캘리브 루프에 `_ble.tick()` 포함
  - D-3: `clearDiagnostics`는 플래그만 설정

  **프론트엔드 안정성 (C-01~C-07)**
  - C-01: `renderActionEditor` `const` → `let` (오프라인 폴백 재할당)
  - C-02: `/api/status config`에 `profile_idx/name/count` 추가 (백엔드)
  - C-03: `getSlotsArray()` 배열 27 강제
  - C-04: `profileSwitch` busy 플래그
  - C-05: 오프라인 `G_OFFLINE_KEYCODES` 확장 (directions/groups/slots_meta)
  - C-06: `_kbUsageLabel` "None (없음)" 처리
  - C-07: `setInterval` 오버랩 방지

  **누락 기능 21종 (N-1~N-21)**
  - Phase 1: SafeBoot 해제(N-1), OTA Guard 토글(N-2), Diag 초기화(N-3)
  - Phase 2: SafeMode 토글(N-4), 재부팅 배너(N-5), DPI/Precision(N-7), Host Cycle(N-8), I2C 피드백(N-18)
  - Phase 3: reboot check(N-6), Key Test(N-9), 서버 Export(N-10), Rollback 안내(N-11), 캘리브 피드백(N-19), Factory 진행(N-20)
  - Phase 4: 로딩 오버레이(N-13), slots_meta 라벨(N-14), reboot 폴링(N-16), Export 별칭(N-17), 삭제 경고(N-21)

  **SPEC/UserManual 문서 현행화**
  - SPEC §7.9 API 표 갱신, schema ver=410 확정, method-aware SafeMode, Tilt Mode 3 제한 명시
  - UserManual §3 Slot Editor 상세 가이드 (27개 트리거 표, Global/Mode Override, 10가지 Action Kind별 예시)
  - UserManual §4 Macro Editor 상세 가이드 (편집 절차, 실전 매크로 7가지 예시)

---

## 📎 부록: 주요 상수

| 항목 | 값 |
|---|---|
| `G_C10_CFG_VER` | **410** (Schema v5) |
| `G_W10_API_VER` | **410** |
| `G_W10_BODY_MAX` | 8192 |
| `G_W10_BODY_SLOTS` | 4 |
| `G_W10_DEFAULT_INDEX_PATH` | `/www/index_0411.html` |
| `SAFE_FAIL_THRESHOLD` | 2 |
| `G_BOOT_GRACE_MS` | 8500 |
| `CALIB_MS` | 1000 |
| `CALIB_STILL_TH` | 3.0 |
| `SPIKE_TH_DEG` | 650.0 |
| `ERR_HIST_CAP` | 16 |
| Sensor 주기 | 8ms (125Hz) |
| Comm 주기 | 7ms |
| LED tick 주기 | 50ms |
| `_qFrame` size | 1 (overwrite) |
| `_qHidCmd` size | 4 |
| `_qActionExec` size | 8 |
| `CONFIG_BT_NIMBLE_MAX_BONDS` | 3 |
| 오프라인 스키마 (`_v`) | 2 |

---

## 🎯 프로젝트 철학

AirMouse Elite S3는 **상용급 입력 디바이스 아키텍처**입니다.

- 태스크/상태 소유권 명확
- 락 정책 일관 (recursive, 짝맞춤)
- Special 액션 sensorTask 단독 실행
- 매크로 비동기 시퀀서 (블로킹 제로, 토큰 취소)
- LED `_ledTask` 단독 tick
- 브릭 방지 (SafeBoot / Atomic Config / OTA Guard / AP fallback)
- 관측성 (RMS / 스택 / dt / 오류 이력)
- 정책 일관성 (SafeMode API 게이트 method-aware, Action 큐 일원화)
- 프론트엔드 7-모듈 구조 (로드 순서 명확, 캐시 immutable)
- 오프라인 시뮬레이터 (localStorage 스키마 버전 관리)

**새 기능 추가 시 위 원칙 위반 여부를 먼저 판단.**
