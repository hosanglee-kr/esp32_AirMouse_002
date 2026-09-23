# 📝 AirMouse Elite S3 (v0320) 기술 사양서 & AI 컨텍스트

본 문서는 **ESP32-S3-Zero** 및 **MPU6050 6축 IMU** 기반 에어마우스 플랫폼의 최신(**v0320**) 소스 코드 기준 기술 사양 및 개발 가이드라인입니다.

* **소스 파일 버전 접미사**: `_0320` 통일 (Config, Sensor, Motion, Web, API 전체 일괄 적용)

---

## 📑 목차 (Table of Contents)

1. [AI 어시스턴트를 위한 핵심 요약 (Read First)](#1-ai-어시스턴트를-위한-핵심-요약-read-first)
2. [프로젝트 개요](#2-프로젝트-개요)
3. [시스템 아키텍처](#3-시스템-아키텍처)
   - [FreeRTOS 태스크 모델](#31-freertos-태스크-모델)
   - [데이터 흐름 및 파이프라인](#32-데이터-흐름-및-파이프라인)
   - [상태 소유권 및 동기화 매트릭스](#33-상태-소유권-및-동기화-매트릭스)
   - [Mutex 동기화 정책](#34-mutex-동기화-정책)
4. [프로젝트 디렉터리 구조](#4-프로젝트-디렉터리-구조)
   - [E10 모듈 6파일 분할 기준](#41-e10-모듈-6파일-분할-기준)
5. [빌드 시스템 (PlatformIO)](#5-빌드-시스템-platformio)
6. [명명 규칙 & 코드 정책](#6-명명-규칙--코드-정책)
   - [명명 규칙 (Strict Naming Conventions)](#61-명명-규칙-strict-naming-conventions)
   - [ArduinoJson v7 전용 코딩 정책](#62-arduinojson-v7-전용-코딩-정책)
   - [문자열 및 버퍼 관리 정책](#63-문자열-및-버퍼-관리-정책)
   - [안전 원자적 IO 정책 (Atomic Storage)](#64-안전-원자적-io-정책-atomic-storage)
7. [핵심 기능 상세 사양](#7-핵심-기능-상세-사양)
   - [1. 물리 모션 엔진 (`M10_MotionProc_0320`)](#71-물리-모션-엔진-m10_motionproc_0320)
   - [2. Precision Mode FSM](#72-precision-mode-fsm)
   - [3. 자이로 캘리브레이션](#73-자이로-캘리브레이션)
   - [4. RTOS 큐 파이프라인](#74-rtos-큐-파이프라인)
   - [5. PPT 제스처 및 Keymap v2](#75-ppt-제스처-및-keymap-v2)
   - [6. SafeBoot 및 OTA Guard](#76-safeboot-및-ota-guard)
   - [7. Config 영속화 메커니즘](#77-config-영속화-메커니즘)
   - [8. 웹 커스터마이징 및 REST API](#78-웹-커스터마이징-및-rest-api)
8. [하드웨어 사양 및 핀맵](#8-하드웨어-사양-및-핀맵)
9. [운용 및 사용 가이드](#9-운용-및-사용-가이드)
   - [9.1. 하드웨어 버튼 및 제스처 운용](#91-하드웨어-버튼-및-제스처-운용)
   - [9.2. 웹 UI 기능 및 원격 제어 가이드](#92-웹-ui-기능-및-원격-제어-가이드)
10. [시스템 진단 지표](#10-시스템-진단-지표)
11. [주의사항](#11-주의사항)
12. [프로젝트 구조 요약 테이블](#12-프로젝트-구조-요약-테이블)
13. [리팩터링 이력 (Phase 1 ~ Track 5)](#13-리팩터링-이력-phase-1--track-5)
14. [알려진 유보 이슈 (Known Deferred Issues)](#14-알려진-유보-이슈-known-deferred-issues)
15. [버전별 사양 변경 비교 (v0.0.6 vs v0320)](#15-버전별-사양-변경-비교-v006-vs-v0320)
16. [프로젝트 핵심 설계 철학](#16-프로젝트-핵심-설계-철학)

---

## 1. 📌 AI 어시스턴트를 위한 핵심 요약 (Read First)

> [!IMPORTANT]
> 본 프로젝트의 코드를 수정하거나 기능을 추가하기 전, 반드시 다음 **6가지 핵심 규칙**을 숙지해야 합니다.

1. **모듈 약어 체계**:
   - `A40`(공용 유틸), `C10`(설정 관리), `D10`(로거/진단), `E10`(에어마우스 핵심), `M10`(모션 물리엔진), `W10`(웹서버/API).
   - 각 접두사는 파일명, 클래스명, 전역 심볼에 일관되게 반영되어야 합니다.
2. **엄격한 명명 규칙 준수**:
   - 전역 상수 `G_`, 전역 변수 `g_`, 클래스 `CL_`, 구조체 `ST_`, 열거형 `EN_`, private 멤버 `_` 접두사, 로컬 변수 `v_` 접두사, 매개변수 `p_` 접두사.
   - 명명 규칙 위반 시 리뷰 반려 대상입니다.
3. **ArduinoJson v7 단일화 정책**:
   - 오직 `JsonDocument` 단일 타입만 허용합니다.
   - `containsKey`, `createNestedArray`, `createNestedObject`, `StaticJsonDocument`, `DynamicJsonDocument` 사용 절대 금지.
   - 중첩 구조 접근 시 `doc["a"]["b"].to<JsonObject>()` 패턴을 사용합니다.
4. **E10 모듈 6파일 분할**:
   - 헤더(`.h`) + `Core` / `Hid` / `Motion` / `Diag` / `Task_0320.cpp`로 분할되어 있습니다. 기능 수정/추가 시 반드시 역할에 맞는 cpp 파일을 선택해야 합니다.
5. **태스크 및 상태 소유권 절대 준수**:
   - `_sensorTask`(Core 1)와 `_commTask`(Core 0)는 FreeRTOS 큐로만 통신합니다.
   - 웹 태스크(AsyncWebServer)는 HID 드라이버에 직접 접근할 수 없으며 반드시 커맨드 큐(`_qHidCmd`)를 경유해야 합니다.
6. **리팩터링 컨텍스트 유지**:
   - Phase 1~4 (E10 핵심 수정), Track 2 (JSON/JS 정합), Track 3 (W10 점검)을 완료한 상태입니다. 신규 변경 사항은 반드시 기존 리팩터링 설계 위에서 진행되어야 합니다.

---

## 2. 🎯 프로젝트 개요

ESP32-S3-Zero와 MPU6050을 기반으로 마우스패드 없이 공중에서 3차원 움직임을 감지하는 자이로 전용 공간 포인팅 에어마우스입니다. BLE HID Composite(Mouse + Keyboard) 인터페이스를 통해 호스트 기기에 연결되며, 내장 웹 서버를 통해 실시간 파라미터 튜닝 및 진단 기능을 제공합니다.

### 🌟 핵심 특징
* **자이로 기반 공간 포인팅**: 상보 필터 + 시그모이드(Sigmoid) 가속 곡선 + 적응형 LPF 탑재.
* **Precision 안정화 FSM**: 미세 포인팅 보정을 위한 5개 프로파일 × 4개 상태 머신.
* **PPT 제스처 & Keymap v2**: 손목 Yaw Flick 감지 및 커스텀 키맵(Keyboard / Consumer 추상화) 지원.
* **SafeBoot / OTA Guard**: 부팅 실패 카운트 기반 브릭 방지 및 펌웨어 업데이트 중 오동작 차단.
* **실시간 웹 설정/진단 UI**: 30개 이상의 REST API를 통한 파라미터 실시간 조정 및 모니터링.
* **원자적(Atomic) 설정 영속화**: 임시 파일 검증 및 `.bak` 자동 롤백을 통한 Flash 무결성 보장.

---

## 3. 🏗️ 시스템 아키텍처

### 3.1. FreeRTOS 태스크 모델

| 태스크명 | 할당 Core | 우선순위 (Priority) | 실행 주기 | 스택 크기 (Stack) | 주요 역할 |
|:---|:---:|:---:|:---:|:---:|:---|
| `_sensorTask` | Core 1 | 3 | 8ms (125Hz) | 8192 B | IMU 읽기, FSM 처리, 모션 계산, 프레임/커맨드 생성 |
| `_commTask` | Core 0 | 2 | 7ms | 4096 B | 큐 소비 $\rightarrow$ BLE HID 전송, 특수 커맨드 실행 |
| `AsyncWebServer` | Core 0/1 | (IDF 내부) | 이벤트 구동 | - | HTTP 요청 처리, E10 제어 요청 큐잉 |
| `main loop` | Core 0 | - | 200ms | - | 시스템 Grace time 통과 판정 및 백그라운드 관리 |

---

### 3.2. 데이터 흐름 및 파이프라인

```
┌─────────────────┐           overwrite (size=1)           ┌──────────────┐
│   sensorTask    ├─────push─────> _qFrame ─────recv──────>│   commTask   ├────> BLE HID
└────────┬────────┘                                        └──────┬───────┘
         │                                                        ▲
         │ (관측 갱신)                                            │
         └────lock────> _state ──read──> getStatus() [Web]        │
                                                                  │
┌────────────────────────┐      enqueue (size=4, timeout=0)       │
│ Web API / sensorTask   ├────────────────────────────────────────┘
└────────────────────────┘
```

> [!CAUTION]
> #### 아키텍처 절대 금지 사항
> 1. **웹 태스크의 HID 직접 접근 금지**: `_mouse` 및 `_keyboard` 인스턴스를 직접 호출하지 말고, 반드시 `_qHidCmd` 큐를 통해 위임해야 합니다.
> 2. **`sensorTask`의 HID 직접 호출 금지**: 센서 태스크에서 HID 전송을 직접 수행하지 않고 `_qHidCmd`를 경유합니다.
> 3. **`commTask`의 `_state` 무단 접근 금지**: 상태 변수에 직접 접근하지 않고 `_qFrame` 데이터만 사용합니다.

---

### 3.3. 상태 소유권 및 동기화 매트릭스

| 상태 / 리소스 | 소유자 (Writer) | 접근 방식 (Reader) | 동기화 메커니즘 및 정책 |
|:---|:---|:---|:---|
| `_state` (`ST_E10_State_t`) | `sensorTask` (상태 갱신), `web` (`setSafeMode` 등) | `getStatus()` 등 (웹) | `_lock()` / `_unlock()` 뮤텍스 보호 |
| `_qFrame` | `sensorTask` (write) | `commTask` (read) | FreeRTOS Queue (`size=1`, lock-free overwrite) |
| `_qHidCmd` | `web`, `sensorTask` (write) | `commTask` (read) | FreeRTOS Queue (`size=4`, `timeout=0`) |
| `_cfgE10Runtime` | 설정 변경 경로 | 각 모듈 | `_lock()` 하에만 접근 (H-3 원자화 경로) |
| Motion Config | Setter (외부) | `sensorTask` | Setter는 lock 후 기록, Reader는 루프 시작 시 스냅샷 (C-4) |
| `_errHist`, `_spikes` | 내부 로직 | 진단 모듈 | `_pushErr` / `_pushSpike` 내부에서 mutex lock (C-2) |
| `_hid` (`BleCompositeHID`) | `commTask` 단독 | `commTask` (예외: `isConnected()` 웹 조회) | 단일 태스크 전담 소유 |
| `_mpu`, `Wire` | `sensorTask` 단독 | `sensorTask` 단독 | I2C 복구(`_recoverI2C`)도 `sensorTask` 내부에서만 실행 |

---

### 3.4. Mutex 동기화 정책

* **Recursive Mutex 채택**: `_mutex`는 `xSemaphoreCreateRecursiveMutex()`로 생성됩니다.
  * *이유*: `setSafeMode()`나 `setOtaGuard()`가 이미 락을 보유한 상태에서 `_pushErr()`를 호출하는 등 재진입 상황 대응.
* **수동 락 관리**: `_lock()`과 `_unlock()`은 RAII 형태가 아니므로 모든 예외/분기 경로에서 엄격하게 쌍을 맞추어야 합니다.
* **Non-blocking Enqueue**: `_qHidCmd` enqueue는 `timeout=0`으로 호출되어 웹 태스크가 블로킹되지 않도록 합니다. 큐가 가득 차면 패킷을 드롭합니다.

---

## 4. 📁 프로젝트 디렉터리 구조

```
src/
├── main.cpp                              # 메인 진입점 (setup / loop, Grace 감시)
└── v032/                                 # v0320 모듈 루트
    ├── A40_ComFunc_0320.h                # 공용 유틸 (JSON 직렬화, Mutex, 원자적 IO)
    ├── C10_Config_0320.h                 # 설정 로드/저장/검증 및 Boot State (Header-only)
    ├── C10_Def_0320.h                    # Config 스키마 상수, 구조체, 파일 경로 정의
    ├── D10_Logger_0320.h                 # 링버퍼 로거 및 진단 카운터 (Header-only)
    ├── M10_MotionProc_0320.h             # 모션 물리 엔진 (상보 필터, LPF, 가속) (Header-only)
    │
    ├── E10_Def_0320.h                    # E10 모듈 상수, 열거형, 구조체 정의
    ├── E10_AirMouse_0320.h               # E10 메인 클래스 선언 및 인라인 유틸
    ├── E10_AirMouse_Core_0320.cpp        # 라이프사이클 초기화, 런타임 설정 적용, setter
    ├── E10_AirMouse_Hid_0320.cpp         # HID 패킷 송출, 테스트 클릭, 강제 릴리즈
    ├── E10_AirMouse_Motion_0320.cpp      # Precision FSM 및 Motion FSM 연산
    ├── E10_AirMouse_Diag_0320.cpp        # 오류/스파이크 로깅, I2C 복구, 자이로 캘리브레이션
    ├── E10_AirMouse_Task_0320.cpp        # _sensorTask 및 _commTask 루프 구현
    │
    ├── W10_Def_0320.h                    # 웹서버 상수, 응답 타입, 키코드 매핑 테이블
    ├── W10_Web_0320.h                    # 웹서버 메인 클래스 선언
    ├── W10_Web_init_0320.cpp             # 서버 초기화, 라우팅 등록, WiFi/mDNS 구성
    ├── W10_Web_Static_0320.cpp           # 정적 파일 서빙, LittleFS 바디 슬롯 관리
    ├── W10_WebApi_Com_0320.cpp           # API 공통 처리, CORS, 보안 정책, ETag 생성
    ├── W10_WebApi_Config_0320.cpp        # /api/config/* 라우트 핸들러
    ├── W10_WebApi_CtlPpt_0320.cpp        # /api/control, /api/ppt 핸들러
    ├── W10_WebApi_OtaBoot_0320.cpp       # /api/ota, /api/safeboot, /api/reboot 핸들러
    ├── W10_WebApi_Status_0320.cpp        # /api/status, /api/diag, /api/keycodes 핸들러
    │
    ├── tools_v032/
    │   └── pio_gzip_0320.py              # 빌드 전 www 정적 파일 gzip 압축 및 LittleFS 동기화 스크립트
    │
    ├── data_v032_www/                    # [VCS 소스] 웹 프론트엔드 원본 리소스 (Git 추적 대상)
    │   ├── index_0320.html               # SPA 메인 HTML (Quick Ctl, Dashboard, Diag, OTA UX)
    │   ├── app_0320.js                   # SPA 프론트엔드 컨트롤러 (실시간 폴링, API 통신, XHR OTA)
    │   ├── style_0320.css                # 반응형 다크 테마 UI 스타일시트
    │   └── images/
    │
    └── data_v032/                        # LittleFS 업로드 이미지 디렉터리 (빌드 타깃)
        ├── www/                          # [빌드 파생물] Gzip 압축된 웹 파일 (Git 제외: .gitignore)
        │   ├── index_0320.html.gz
        │   ├── app_0320.js.gz
        │   ├── style_0320.css.gz
        │   └── images/
        └── json/                         # 시스템 설정 및 메타데이터
            └── public/
                ├── manifest_0320.json
                └── schema_0320.json
```

---

### 4.1. E10 모듈 6파일 분할 기준

E10 모듈의 유지보수 시 변경할 기능에 따라 아래 담당 파일로 이동하여 작업합니다.

| 소스 파일명 | 담당 핵심 역할 |
|:---|:---|
| `E10_AirMouse_Core_0320.cpp` | `begin()`, 생성자, `applyRuntimeE10()`, `set*` 계열 함수(`PptMode`, `Dpi`, `Precision`, `HardClick`, `SafeMode`, `OtaGuard`), `_applyFromConfig()`, `_applyE10ToRuntime()`, `_snapshotRuntimeToE10Config()`, `_enqueueHidCmd()`, `_applyRuntimeLocked()` |
| `E10_AirMouse_Hid_0320.cpp` | `_tapComboUsageKb()`, `_tapUsageKb()`, `_tapConsumerMask()`, `_sendPptKey2()`, `_processGesturesDeg()`, `testPptKey2()`, `testMouseClick()`, `forceReleaseButtons()`, `_doReleaseAllButtons()`, `_doTestMouseClick()`, `_doForceReleaseNow()` |
| `E10_AirMouse_Motion_0320.cpp` | `_applyPrecision()`, `_fsmUpdate()` |
| `E10_AirMouse_Diag_0320.cpp` | `_pushErr()`, `_pushSpike()`, `_recoverI2C()`, `_runGyroCalibration()`, `getStatus()`, `requestGyroCalibration()`, `requestI2CRecover()`, `clearDiagnostics()` |
| `E10_AirMouse_Task_0320.cpp` | `_sensorTask()`, `_commTask()` |
| `E10_AirMouse_0320.h` | 클래스 선언, 인라인 유틸 함수(`_lock`, `_unlock`, `_pushFrame`, `_welfordAdd`, `_calcRms`, `_mouseSend`) |

---

## 5. ⚙️ 빌드 시스템 (PlatformIO)

### `platformio.ini` 환경 설정

```ini
[platformio]
default_envs    = esp32-s3-zero
build_cache_dir = .pio/cache
data_dir        = ./src/v032/data_v032

[ESP32_common]
platform                = espressif32
framework               = arduino
board_build.filesystem  = littlefs
monitor_speed           = 115200
monitor_filters         = esp32_exception_decoder, colorize

src_filter =
    +<main.cpp>
    +<v032/>
    -<v001/> -<v010/> -<v030/> -<v031/> -<src_backup/>

lib_archive = yes
lib_ldf_mode = chain+
lib_deps =
    bblanchon/ArduinoJson @ ^7.4.3
    h2zero/NimBLE-Arduino @ ^2.3.7
    https://github.com/Mystfit/ESP32-BLE-CompositeHID.git
    adafruit/Adafruit MPU6050@^2.2.9
    esp32async/ESPAsyncWebServer @ ^3.10.0

build_unflags = -std=gnu++11
build_flags =
    -std=gnu++17
    -D CONFIG_BT_NIMBLE_ENABLED=1
    -D CONFIG_BT_BLE_ENABLED=1
    -D E10_HAS_JOYSTICK=0

[env:esp32-s3-zero]
extends                 = ESP32_common
board                   = esp32-s3-devkitc-1
board_build.mcu         = esp32s3
board_build.f_cpu       = 240000000L
board_build.f_flash     = 80000000L
board_build.flash_mode  = qio
board_upload.flash_size = 4MB
board_build.partitions  = default_4MB.csv

build_flags =
    ${ESP32_common.build_flags}
    -Desp_cpu_get_cycle_count=xthal_get_ccount
    -DBOARD_HAS_PSRAM
    -mfix-esp32-psram-cache-issue
    -D ARDUINO_USB_MODE=1
    -D ARDUINO_USB_CDC_ON_BOOT=1

extra_scripts = pre:src/v032/tools_v032/pio_gzip_0320.py
```

> [!NOTE]
> * `src_filter`에 `+<v032/>`가 지정되어 있어 신규 소스 파일 추가 시 `platformio.ini`를 수정할 필요가 없습니다.
> * `tools_v032/pio_gzip_0320.py` 스크립트가 빌드 전 `data_v032/www/` 내부 정적 에셋의 `.gz` 압축 파일을 자동 생성합니다.

---

## 6. 📐 명명 규칙 & 코드 정책

### 6.1. 명명 규칙 (Strict Naming Conventions)

| 대상 | 접두사 / 접미사 | 네이밍 예시 |
|:---|:---|:---|
| 네임스페이스 (Namespace) | `{모듈}_` | `A40_ComFunc`, `C10_DEF`, `E10_CONST` |
| 전역 상수 / 매크로 | `G_{모듈}_` | `G_C10_CFG_VER`, `G_W10_API_VER` |
| 전역 변수 | `g_{모듈}_` | `g_cfg`, `g_e10` |
| 전역 함수 | `{모듈}_` | `W10_hasVersionToken()` |
| 구조체 / 타입 정의 | `ST_{모듈}_`, 접미사 `_t` | `ST_C10_WiFiConfig_t`, `ST_E10_Status_t` |
| 열거형 (Enum) 상수 | `EN_{모듈}_` | `EN_C10_WIFI_AUTO` |
| 클래스명 | `CL_{모듈}_` | `CL_E10_EliteAirMouse` |
| Private 멤버 변수/함수 | `_` 접두사 | `_state`, `_lock()` |
| 클래스 정적(Static) 멤버 | `s_` 접두사 | `s_buffer`, `s_mux` |
| 함수 로컬 변수 | `v_` 접두사 | `v_dt`, `v_gyroAbs` |
| 함수 매개변수 (Parameter) | `p_` 접두사 | `p_enable`, `p_code` |

---

### 6.2. ArduinoJson v7 전용 코딩 정책

> [!WARNING]
> 본 프로젝트는 ArduinoJson v7을 표준으로 사용합니다. 이전 v6 스타일 API는 컴파일 에러를 발생시키거나 메모리 누수를 유발하므로 절대 사용하지 마십시오.

#### ✅ 권장 패턴
```cpp
JsonDocument doc;
doc["a"] = 1;
doc["b"]["c"] = 2;

JsonObject o = doc["x"].to<JsonObject>();
JsonArray arr = doc["y"].to<JsonArray>();
JsonVariant v = doc["z"];

if (!v.isNull()) { /* 처리 */ }
if (!v["k"].isNull()) { /* 처리 */ }
```

#### ❌ 금지 패턴
```cpp
JsonObject o = doc.createNestedObject("x");     // 금지: v7 지원 중단
JsonArray a = doc.createNestedArray("y");       // 금지: v7 지원 중단
if (doc.containsKey("k")) { ... }               // 금지: isNull() 패턴 사용
StaticJsonDocument<256> d;                      // 금지: v6 전용
DynamicJsonDocument d(256);                     // 금지: v6 전용
```

---

### 6.3. 문자열 및 버퍼 관리 정책
* **메모리 안정성**: 가변 `String` 객체 생성을 지양하고, `memset` + `strlcpy` 고정 버퍼 조합을 사용합니다.
* **버퍼 오버플로우 방지**: 포맷팅 시 `snprintf`를 사용하고 반환값 및 오버플로우 여부를 필수 검증합니다.
* **JSON 이스케이프**: JSON 직렬화 시 `_appendJsonEscaped` 또는 `_resPrintJsonString`을 통하여 제어 문자를 안전하게 인코딩합니다.

---

### 6.4. 안전 원자적 IO 정책 (Atomic Storage)
* **저장 5단계 프로세스**:
  1. `.tmp` 파일 생성 및 쓰기
  2. `.tmp` 파싱을 통한 무결성 검증 (`verify`)
  3. 기존 메인 설정 파일을 `.bak`로 회전 (`rename` 우선, 실패 시 `copy`)
  4. `.tmp`를 메인 경로로 커밋
  5. 최종 파일 재검증 (실패 시 `.bak`에서 롤백)
* **백업 파일 보존**: 쓰기가 성공하더라도 복구 여지를 위해 `.bak` 파일을 유지합니다.
* **복구 정책**: `rollbackFromBak()` 수행 시 파일 보존을 위해 `rename` 대신 `copy` 방식을 우선 적용합니다.

---

## 7. 🚀 핵심 기능 상세 사양

### 7.1. 물리 모션 엔진 (`M10_MotionProc_0320`)
단순 센서 값 변환이 아닌, 인체공학적 손 떨림 해석 및 정밀 제어를 제공합니다.

* **상보 필터 (Complementary Filter)**:
  $$\text{roll} = 0.98 \times (\text{roll} + \text{gyro} \times dt) + 0.02 \times \text{accelRoll}$$
* **시그모이드(Sigmoid) 비선형 가속**:
  $$\text{dpiGain} = 15 + (\text{dpi\_level} \times 7)$$
  $$\text{out} = \frac{\text{dpiGain}}{1 + \exp(-0.8 \times (|\text{in}| - 2))}$$
* **적응형 LPF (Low-Pass Filter)**:
  * 모션 변화량 $\Delta > 3.0$ 일 때: $\alpha = 0.50$ (반응성 우선)
  * 정적/미세 움직임 시: $\alpha = 0.12$ (떨림 완화 우선)
* **Zero Snap**: $|v| < 0.6^\circ/\text{s}$ 이하의 미세 노이즈는 강제로 0으로 스냅.
* **Click-Lock 메커니즘 (150ms)**:
  * `hard_click_lock = true`: 클릭 순간 좌표 완전 고정 ($\text{outX} = \text{outY} = 0$)
  * `hard_click_lock = false`: 클릭 순간 좌표 95% 감쇠 ($\times 0.05$)

---

### 7.2. Precision Mode FSM

미세 조준/포인팅을 위한 5단계 프로파일과 4단계 FSM 상태 머신을 운영합니다.

#### 프로파일 매트릭스
| 모드 (ID) | 프로파일 명 | Gain | Alpha ($\alpha$) | 가속 제한 (accel_limit) |
|:---:|:---|:---:|:---:|:---:|
| `0` | **OFF** | 1.00 | 0 | 0 |
| `1` | **LOW** | 0.85 | 64 | 0 |
| `2` | **MED** | 0.70 | 128 | 0 |
| `3` | **HIGH** | 0.55 | 180 | 0 |
| `4` | **PPT** | 0.45 | 210 | 1.5 |

#### FSM 상태 전이
$$\text{OFF} \longrightarrow \text{ENTRY} \longrightarrow \text{TRACK} \rightleftarrows \text{EXIT}$$
* **ENTRY $\rightarrow$ TRACK**: $\text{gyro} \le \text{entry\_still\_deg}$ 상태가 $\text{entry\_ms}$ 동안 유지될 때 진입.
* **TRACK $\rightarrow$ EXIT**: $\text{gyro} \ge \text{exit\_move\_deg}$ 감지 시 이탈 준비.
* **EXIT $\rightarrow$ TRACK**: 다시 안정화되어 $\text{exit\_ms}$ 경과 시 복귀.

---

### 7.3. 자이로 캘리브레이션
* 부팅 직후 1초간(`CALIB_MS = 1000`) 자이로 정지 샘플링(`CALIB_STILL_TH = 3.0 deg/s`)을 수집하여 Offset을 연산합니다.
* **논블로킹 버튼 샘플링**: 캘리브레이션 중에도 버튼 입력이 멈추지 않도록 프레임을 주기적으로 push합니다.
* 웹 API(`/api/control` `{"cmd":"gyro_calib"}`)를 통해 런타임 재보정이 가능합니다.

---

### 7.4. RTOS 큐 파이프라인
* **`_qFrame` (Size: 1, Overwrite Mode)**: 최신 센서 데이터 프레임만 유지하여 커서 랙을 원천 차단합니다.
* **버튼 상태 디바운싱**: `commTask`에서 이전 프레임과의 차분(diff)을 비교하여 처리하므로 패킷 드롭 시에도 버튼 고착(stuck)이 방지됩니다.
* **`_qHidCmd` (Size: 4, Non-blocking Enqueue)**: 웹이나 센서 태스크에서 발행한 특수 명령(`RELEASE_ALL`, `TEST_CLICK`, `TEST_PPT`)을 `commTask`가 안전하게 직렬 실행합니다.

---

### 7.5. PPT 제스처 및 Keymap v2
* **Z축(Yaw) Flick 제스처**:
  * $gz > +\text{flick\_deg} \rightarrow \text{이전 슬라이드(Prev)}$
  * $gz < -\text{flick\_deg} \rightarrow \text{다음 슬라이드(Next)}$
  * 쿨다운: `gesture_cooldown_ms` (기본 600ms)
  * 스크롤 버튼 눌림 상태 또는 SafeMode/OTA Guard 활성화 시 제스처 자동 무시
* **Keymap v2 추상화 구조**:
  * 키보드 페이지: `page="kb"` (Usage ID + Modifier 키)
  * 컨슈머 페이지: `page="consumer"` (32-bit Usage Mask)
  * 지원 액션 6종: `start`, `exit`, `next`, `prev`, `black`, `laser`

---

### 7.6. SafeBoot 및 OTA Guard

부팅 상태 정보는 `/json/boot_state_0320.json`에 영속화됩니다.

#### 동작 알고리즘
1. `begin()` 호출 시 부팅 상태 JSON 로드
2. 이전 부팅이 완료되지 않고(`pending=true`) 재부팅 원인이 비정상일 경우 `fail_count` 증가
   * *비정상 원인*: `PANIC`, `INT_WDT`, `TASK_WDT`, `WDT`, `BROWNOUT`
3. `fail_count >= SAFE_FAIL_THRESHOLD(2)` 도달 시 자동으로 **SafeMode** 활성화
4. 정상 진입 후 유예 시간(`bootMarkOkIfGracePassed`, 8500ms) 경과 시 `pending=false` 처리
5. 부팅 실패 시 플래시 마모 방지를 위해 30초 백오프 적용

#### SafeMode 및 OTA Guard 진입 시 시스템 동작
* **AP SSID 변경**: 네트워크 식별을 위해 AP SSID 뒤에 `-SAFE` 접미사 자동 부여.
* **HID 출력 완전 차단**: 마우스 커서 및 키 입력이 호스트로 방출되지 않음.
* **API 정책 제한 (H-1)**:
  * **허용 엔드포인트**: `/api/status`, `/api/diag`, `/api/diag/clear`, `/api/keycodes`, `/api/safeboot`, `/api/ota`, `/api/ota/status`, `/api/factory_reset`, `/api/reboot`, `/api/reboot/check`, `GET /api/config`, `/api/config/export`, `/api/export`, `/api/config/rollback`
  * **차단 엔드포인트**: `/api/config/save`, `/api/config/apply`, `/api/config/import`, `/api/control`, `/api/ppt`, `/api/ppt/test`
* **OTA Guard**: 펌웨어 전송 중 키 고착을 방지하며, 비정상 중단 시 30초 후 stale 상태를 자동 회수합니다.

---

### 7.7. Config 영속화 메커니즘

#### 시스템 파일 경로 (`C10_Def_0320.h`)
```cpp
CFG_PATH  = "/json/config_0320.json";
CFG_TMP   = "/json/config_0320.json.tmp";
CFG_BAK   = "/json/config_0320.json.bak";
BOOT_PATH = "/json/boot_state_0320.json";
```

* **무결성 캐싱**: FNV-1a 32-bit 알고리즘 기반 ETag를 생성하여 웹 클라이언트 캐시 무효화를 제어합니다.

---

### 7.8. 웹 커스터마이징 및 REST API

* **접속 프로토콜**: AP 및 STA 동시 지원, mDNS 도메인(`http://elite-airmouse.local`) 지원.
* **정적 파일 서빙**: HTML, CSS, JS에 대한 Gzip 자동 서빙 및 버전 토큰(`_0320`) 기반 Immutable 캐싱.
* **보안**: 파일 확장자 화이트리스트 검사 및 경로 탐색(`..`) 차단.

#### 주요 REST API 엔드포인트
| HTTP Method | URI Endpoint | 기능 설명 | 주요 파라미터 / 페이로드 |
|:---|:---|:---|:---|
| `GET` | `/api/status` | 시스템, E10 모듈, 정책 플래그, 센서 진단 스냅샷 반환 | `?compact=1` (실시간 경량 모니터링) |
| `GET` | `/api/diag` | 시스템 진단 카운터, RTOS 태스크 진단, E10 센서 에러 링버퍼 로그 반환 | - |
| `POST` | `/api/diag/clear` | 진단 카운터 및 이벤트 로그 초기화 | - |
| `GET` | `/api/keycodes` | 키보드 Modifiers, Usage ID, Consumer 코드 목록 반환 | - |
| `GET` | `/api/config` | 전체 JSON 설정값 조회 (ETag 지원) | - |
| `POST` | `/api/config/save` | 변경 설정 검증, 영속화 및 런타임 즉시 적용 | 전체 JSON Body |
| `POST` | `/api/config/apply` | 플래시 저장 없이 런타임에만 임시 적용 | 전체 JSON Body |
| `GET` | `/api/config/export` | 현재 설정을 JSON 파일로 다운로드 | - |
| `POST` | `/api/config/import` | 외부 JSON 설정을 검증 후 가져오기 | Multi-part 파일 |
| `POST` | `/api/config/rollback` | `.bak` 백업 파일로부터 설정 복원 | - |
| `POST` | `/api/control` | 모드 및 비상 복구 제어 | `dpi_level`(1/2/3), `gyro_calibrate`(true), `force_release`(true), `i2c_recover`(true), `ppt_mode`, `precision_profile`, `safe_mode`, `ota_guard` |
| `GET` | `/api/ppt` | PPT 액션별 키 매핑 테이블 조회 | - |
| `POST` | `/api/ppt` | PPT 액션별 키 매핑 테이블 저장 | PPT 액션 JSON 매핑 |
| `POST` | `/api/ppt/test` | 특정 PPT 단일 키 송출 테스트 | `{"action": "next"}` 등 |
| `POST` | `/api/ota` | 펌웨어 바이너리 멀티파트 업로드 | Multi-part 바이너리 (`.bin`) |
| `GET` | `/api/ota/status` | OTA 진행률 및 성공/실패 상태 폴링 | - |
| `GET` | `/api/safeboot` | SafeBoot 카운터 및 활성화 상태 확인 | - |
| `POST` | `/api/safeboot` | SafeMode 강제 해제 (`{"exit": true}`) | `{"exit": true}` |
| `POST` | `/api/factory_reset`| 플래시 설정을 초기 기본값으로 리셋 | - |
| `POST` | `/api/reboot` | 시스템 소프트 리셋 (`reason_mask` 검증) | `{"reason": 1}` |
| `GET` | `/api/reboot/check` | 설정 변경에 따른 재부팅 요구 여부 조회 | - |

---

## 8. 🔌 하드웨어 사양 및 핀맵

### 8.1. 주요 하드웨어 제원
| 구분 | 상세 사양 |
|:---|:---|
| **MCU** | ESP32-S3 (Dual-Core Xtensa LX7, 최대 240MHz) |
| **메모리** | Flash 4MB (QIO, 80MHz), PSRAM 내장 지원 (`BOARD_HAS_PSRAM`) |
| **센서** | InvenSense MPU6050 (자이로 $\pm 250^\circ/\text{s}$, 가속도 $\pm 2\text{G}$, DLPF 21Hz) |
| **통신 버스** | I2C Fast Mode (400kHz) |
| **무선 인터페이스** | BLE 5.0 (NimBLE 스택 기반 HID over GATT) |
| **HID 인터페이스** | Composite HID (마우스 5버튼/휠 + 키보드 멀티미디어) |
| **센서 샘플링** | 125Hz (8ms 고정 인터벌) |
| **HID 리포트 주기**| BLE 연결 간격(Connection Interval) 종속 (통상 7.5ms ~ 15ms) |
| **파일 시스템** | LittleFS (내장 플래시 파티션 기반) |
| **호환 OS** | Windows, macOS, Linux, Android, iOS (표준 드라이버 불필요) |

---

### 8.2. 배선도 (Wiring Diagram)

```
┌──────────────────┐               ┌──────────────────┐
│  ESP32-S3-Zero   │               │     MPU6050      │
│                  │               │                  │
│             3V3  ├───────────────┤  VCC             │
│             GND  ├───────────────┤  GND             │
│           GPIO4  ├───────────────┤  SDA             │
│           GPIO5  ├───────────────┤  SCL             │
└──────────────────┘               └──────────────────┘
```

---

### 8.3. GPIO 핀 할당표 (`E10_Def_0320.h`)

| 기능 명칭 | GPIO 핀 번호 | 입출력 특성 및 상세 설명 |
|:---|:---:|:---|
| **I2C SDA** | `GPIO 4` | MPU6050 센서 데이터 라인 (400kHz) |
| **I2C SCL** | `GPIO 5` | MPU6050 센서 클럭 라인 (400kHz) |
| **BTN_L** | `GPIO 12` | 마우스 좌클릭 입력 (Click-Lock 제어 트리거) |
| **BTN_MODE** | `GPIO 13` | 짧게: DPI 순환 (1$\rightarrow$2$\rightarrow$3), 길게(1초): PPT 모드 토글 |
| **BTN_SCROLL** | `GPIO 14` | 누른 상태 유지: 수직 스크롤 모드 진입 |
| **BTN_R** | `GPIO 15` | 마우스 우클릭 입력 |
| **BTN_M** | `GPIO 16` | 마우스 휠(미들) 클릭 입력 |

> [!NOTE]
> 물리 레이저 포인터 모듈은 안전 및 반응 속도를 위해 MCU를 거치지 않고 하드웨어 스위치로 직결 구동됩니다.

---

## 9. 🖱️ 운용 및 사용 가이드

### 9.1. 하드웨어 버튼 및 제스처 운용
* **기본 커서 동작**:
  * 기기 전원을 켜면 BLE 페어링 대기 후 자동으로 연결됩니다. 공중에서 기기를 움직이면 커서가 이동합니다.
  * `BTN_L`을 누르면 기본 좌클릭이 수행됩니다.
* **DPI 단계 변경**:
  * `BTN_MODE` 버튼을 짧게(0.3초 미만) 누르면 DPI 단계가 `1` $\rightarrow$ `2` $\rightarrow$ `3` $\rightarrow$ `1` 순서로 즉시 순환 변경됩니다.
* **화면 스크롤**:
  * `BTN_SCROLL`을 누른 채로 기기를 위아래로 기울이면 휠 스크롤이 발생합니다.
  * 기울기 민감도는 `wheel_threshold_deg`(기본 90°), 가속 상한은 `wheel_step_max`(기본 6), 커서 움직임 억제율은 `scroll_cursor_damp`로 튜닝합니다.
* **프레젠테이션(PPT) 제스처 모드**:
  * `BTN_MODE`를 1초 이상 길게 누르면 PPT 모드가 활성화됩니다.
  * 손목을 왼쪽으로 가볍게 채면(Yaw Flick) **이전 슬라이드**, 오른쪽으로 채면 **다음 슬라이드** 명령이 전송됩니다.
* **정밀 조준 (Precision Mode)**:
  * 웹 UI 혹은 API를 통해 원하는 감도 프로파일(LOW, MED, HIGH, PPT)을 활성화하면 미세 조준 시 커서 떨림이 완벽히 제어됩니다.

### 9.2. 웹 UI 기능 및 원격 제어 가이드
* **원클릭 퀵 컨트롤 (Quick Control)**:
  * 웹 대시보드 상단의 퀵 컨트롤 패널을 통해 마우스 기기를 만지지 않고도 실시간 튜닝 및 긴급 제어가 가능합니다:
    * `[DPI 1]` / `[DPI 2]` / `[DPI 3]`: 원클릭으로 런타임 DPI 레벨 즉시 전환 (현재 활성 단계는 강조 색상으로 자동 하이라이트).
    * `[자이로 보정]`: 공중 또는 거치 상태에서 커서 흐름(Drift) 발생 시 정지 상태에서 즉시 오프셋 재계측.
    * `[버튼 강제 릴리즈]`: 통신 지연이나 조작 실수로 마우스 버튼 또는 키보드 키가 눌린 채 고착되었을 때 원격 비상 해제.
    * `[I2C 복구]`: MPU6050 버스 정체나 데이터 이상 징후 감지 시 소프트웨어 I2C 버스 리셋 및 센서 재초기화 트리거.
* **실시간 대시보드 모니터링 (Dashboard Status)**:
  * 현재 활성 DPI 단계(`DPI Level`), 실시간 손떨림 지표(`Cursor RMS`), 센서 다이 내부 온도(`Sensor Temp` ℃), 센서 실제 샘플링 인터벌 및 주파수(`Sampling` ms / Hz)를 실시간 관측합니다.
* **통합 진단 및 오류 로그 뷰어 (Diagnostics)**:
  * 웹 서버 내부 에러 외에도 MPU NaN 에러, 뮤텍스 획득 실패, RTOS 태스크 타임 슬라이스 오버런, I2C 복구 횟수, 비상 릴리즈 횟수, 센서/통신 태스크 잔여 스택 워드를 실시간 배지로 확인 가능합니다 (0 초과 시 붉은색 경고 표시).
  * 진단 탭 하단에 E10 센서 링버퍼 에러 로그(`diagErrHist`) 전용 뷰어를 제공하여 최근 발생한 하드웨어 예외를 타임스탬프와 함께 열람할 수 있습니다.
* **무선 펌웨어 업데이트 (OTA Update UX)**:
  * 웹 OTA 탭에서 펌웨어 바이너리(`.bin`)를 선택하고 업로드를 시작하면 실시간 XHR 전송 진행률 바(0~100% 및 전송 KB)가 표시됩니다.
  * 업데이트 진행 중에는 마우스 오동작 방지를 위해 `OTA Guard` 경고 배너가 표시되며 모든 HID 입력이 차단됩니다. 업로드 완료 후 기기가 자동으로 안전하게 재부팅됩니다.
* **SafeBoot 복구 및 공장 초기화**:
  * 비정상 부팅 반복으로 SafeMode 진입 시 `POST /api/safeboot {"exit":true}` 호출 후 재부팅하면 일반 모드로 복구됩니다.
  * 복구 불가 오류 발생 시 `POST /api/factory_reset`을 통해 설정을 기본값으로 초기화할 수 있습니다.

---

## 10. 🔬 시스템 진단 지표

`/api/status?compact=1` 및 `/api/diag` 엔드포인트를 통해 실시간 시스템 상태를 모니터링할 수 있습니다.

### 진단 데이터 그룹
* **E10 상태 정보**:
  * `ble_connected`: 호스트와의 BLE HID 연결 여부
  * `ppt_mode`, `dpi_level`, `precision_mode`: 현재 동작 모드 플래그
  * `fsm_state`, `fsm_sub`: Precision 머신 현재 상태
  * `safe_mode`, `gate.*`, `health.*`: 시스템 보호 게이트 동작 여부
* **센서 및 모션 정보**:
  * `gyro.*`: 3축 자이로 각속도 실측값
  * `cursor_rms`: 커서 떨림 정도를 나타내는 RMS 지표
  * `temp_c`: 센서 다이 온도 (℃)
  * `sampling.*`: 실제 센서 샘플링 주기(dt, ms) 및 주파수(Hz), 지터 모니터링
* **하드웨어 및 RTOS 에러 카운터 (7종)**:
  * `err.mpu_nan`: MPU 센서 데이터 비정상(NaN) 발생 횟수
  * `err.mutex_miss`: FreeRTOS 뮤텍스 획득 경합 실패 횟수
  * `err.task_overrun`: 태스크 타임 슬라이스(데드라인) 초과 카운트
  * `err.i2c_recover`: I2C 버스 락 발생에 따른 버스 리셋 복구 발동 횟수
  * `err.failsafe_rel`: 버튼 누름 고착 방지를 위한 강제 릴리즈 발동 횟수
  * `err.stack_sensor`: `_sensorTask` 최소 잔여 스택 워드 (워터마크)
  * `err.stack_comm`: `_commTask` 최소 잔여 스택 워드 (워터마크)
* **센서 링버퍼 에러 로그 (`diagErrHist` / `e10.err_hist`)**:
  * 최근 발생한 센서/통신 하드웨어 오류 내역(시간, 오류 코드, 상세 메시지)을 순환 링버퍼로 보관 및 웹 뷰어 표출
* **웹서버 진단 카운터**:
  * `body_too_large`, `body_no_slot`: 요청 바디 슬롯 부족 현황
  * `bad_json`: JSON 파싱 실패 건수
  * `safe_blocked`, `ota_blocked`: 보호 모드에 의해 차단된 API 호출 수

---

## 11. ⚠️ 주의사항

* **부팅 시 1초 정치 유지**: 부팅 시 센서 오프셋을 자동 계산하므로 기기를 평평한 곳에 약 1초간 정지 상태로 두어야 합니다.
* **스크롤 과민 반응 조치**: 스크롤이 지나치게 민감할 경우 `wheel_threshold_deg`를 높이거나 `wheel_step_max`를 낮추십시오.
* **스크롤 중 커서 흔들림**: 스크롤 도중 커서가 함께 움직이지 않게 하려면 `scroll_cursor_damp`를 `0`으로 설정하십시오.
* **정지 상태 미세 떨림**: 손떨림으로 인해 커서가 떨리는 경우 Precision Mode를 켜거나 `zero_snap` 임계값을 상향 조정하십시오.
* **네트워크 설정 적용**: WiFi 접속 정보(SSID/PW)를 변경한 후에는 반드시 시스템을 재부팅해야 합니다 (`/api/reboot/check`).

---

## 12. 🗂️ 프로젝트 구조 요약 테이블

| 파일 경로 | 담당 역할 및 주요 기능 | 관리 모듈 |
|:---|:---|:---:|
| `main.cpp` | 하드웨어 셋업, `sensorTask`/`commTask` 생성, Grace Time 감시 | Core |
| `src/v032/A40_ComFunc_0320.h` | JSON 보조 함수, 뮤텍스 래퍼, 원자적 파일 I/O | `A40` |
| `src/v032/C10_Config_0320.h` | 설정 로드, 저장, 무결성 검증, 부팅 상태 관리 | `C10` |
| `src/v032/C10_Def_0320.h` | 설정 스키마 정의, 시스템 경로 및 열거형 정의 | `C10` |
| `src/v032/D10_Logger_0320.h` | 링버퍼 메모리 로거 및 진단 이벤트 카운터 관리 | `D10` |
| `src/v032/E10_Def_0320.h` | E10 상수, 제어 열거형, 런타임 상태 구조체 | `E10` |
| `src/v032/E10_AirMouse_0320.h` | 에어마우스 클래스 선언 및 인라인 연산 유틸 | `E10` |
| `src/v032/E10_AirMouse_Core_0320.cpp` | 시스템 초기화, 런타임 파라미터 적용, Setter 로직 | `E10` |
| `src/v032/E10_AirMouse_Hid_0320.cpp` | BLE HID 패킷 출력, 제스처 번역, 버튼 안전 릴리즈 | `E10` |
| `src/v032/E10_AirMouse_Motion_0320.cpp` | Precision FSM 상태 처리 및 모션 가속 필터링 | `E10` |
| `src/v032/E10_AirMouse_Diag_0320.cpp` | 자이로 오프셋 보정, I2C 복구 루틴, 상태 모니터링 | `E10` |
| `src/v032/E10_AirMouse_Task_0320.cpp` | 센서 취득 루프(`_sensorTask`), 통신 전송 루프(`_commTask`) | `E10` |
| `src/v032/M10_MotionProc_0320.h` | 상보 필터, 적응형 LPF, 시그모이드 가속 물리 엔진 | `M10` |
| `src/v032/W10_Def_0320.h` | 웹서버 상수, MIME 타입 매핑, HID 키코드 테이블 | `W10` |
| `src/v032/W10_Web_0320.h` | 비동기 웹서버 메인 클래스 인터페이스 정의 | `W10` |
| `src/v032/W10_Web_init_0320.cpp` | 서버 초기화, URL 라우팅 등록, WiFi/mDNS 시작 | `W10` |
| `src/v032/W10_Web_Static_0320.cpp` | LittleFS 기반 정적 리소스 서빙, 업로드 버퍼 관리 | `W10` |
| `src/v032/W10_WebApi_Com_0320.cpp` | API 공통 인증, 응답 포맷터, ETag 계산 모듈 | `W10` |
| `src/v032/W10_WebApi_Config_0320.cpp` | `/api/config/*` 설정 조회, 저장, 백업, 롤백 라우트 | `W10` |
| `src/v032/W10_WebApi_CtlPpt_0320.cpp` | `/api/control`, `/api/ppt` 장치 제어 라우트 | `W10` |
| `src/v032/W10_WebApi_OtaBoot_0320.cpp` | `/api/ota`, `/api/safeboot`, `/api/reboot` 관리 라우트 | `W10` |
| `src/v032/W10_WebApi_Status_0320.cpp` | `/api/status`, `/api/diag`, `/api/keycodes` 진단 라우트 | `W10` |
| `src/v032/tools_v032/pio_gzip_0320.py` | 웹 프론트엔드 정적 파일(`.gz`) 사전 압축 및 LittleFS 빌드 동기화 스크립트 | Build |
| `src/v032/data_v032_www/*` | SPA 프론트엔드 원본 소스코드 (HTML/JS/CSS, Git 버전관리 대상) | Web Source |
| `src/v032/data_v032/www/*` | 빌드 시 사전 압축 생성되는 파생물 (LittleFS 패킹 대상, Git 제외) | Web Dist |
| `src/v032/data_v032/json/public/*` | 브라우저 설정 스키마 및 매니페스트 | Web |

---

## 13. 📚 참고: 리팩터링 이력 (Phase 1 ~ Track 5)

### Phase 1 — E10 치명 버그 수정
* **C-1**: `sensorTask` 정상 경로에서 `_pushFrame()` 누락으로 인해 HID 큐가 초기 1회만 채워지고 이후 마우스 좌표/버튼이 전송되지 않던 치명적 결함 수정.
* **C-2**: `_pushErr` 및 `_pushSpike` 함수에 대한 다중 태스크 무보호 접근 문제를 재귀적 뮤텍스(Recursive Mutex) 기반 내부 락으로 해결.
* **C-5**: 자이로 보정 중 1초간 전체 태스크가 멈추던 문제를 개선하여 캘리브레이션 중에도 버튼 샘플링 프레임을 지속 push하도록 수정.
* **모듈 분할**: 단일 거대 파일이던 `E10` 소스를 6개 전문 파일로 분할.

### Phase 2 — HID 전송 파이프라인 일원화
* **C-3**: 웹 태스크에서 `_mouse` 및 `_keyboard` 객체를 직접 호출하던 위험 요소를 제거하고 `_qHidCmd` 큐를 도입하여 `commTask` 전담 실행 구조로 전환.
* **H-2**: 중복 선언되었던 `forceReleaseButtons()`와 `forceReleaseAll()`을 통합 단일화.
* **H-4**: 웹 태스크에서 발생하던 250ms 블로킹 클릭 테스트를 논블로킹 enqueue 방식으로 전면 개편.

### Phase 3 — 원자성 보장 및 API 정책 정립
* **H-3**: `setDpiLevel` 등에서 발생하던 비원자적 Read-Modify-Write 문제를 `_applyRuntimeLocked()` 기반 원자적 커밋으로 변경.
* **C-4**: `sensorTask` 루프 시작 시 동작 파라미터(DPI, 가속 계수, 휠 설정)를 안전하게 로컬 스냅샷으로 캡처하도록 분리.
* **H-1**: SafeMode 활성화 시 위조 설정 주입 방지를 위해 `/api/config/import` 엔드포인트 차단.

### Phase 4 — 플래시 마모 방지 및 복구 신뢰성 확보
* **M-1**: `bootMarkOkIfGracePassed` 실패 시 플래시 마모를 막기 위해 30초 백오프 지연 적용.
* **M-2**: `_recoverI2C` 카운터 접근에 뮤텍스 보호를 통일 적용.
* **M-4**: SAFE 및 OTA 게이트 진입/이탈 추적을 위한 전용 시스템 에러 코드 추가.
* **M-5**: `rollbackFromBak()` 실행 시 파일 삭제 방지를 위해 복사(`copy`) 방식을 우선 적용.

### Track 2 — JSON 스키마 및 프론트엔드 일관성 확보
* **J-1**: `boot_state_0320.json`의 키 명칭을 `boot_ms` 및 `last_reset_reason`으로 통일.
* **J-2**: `app_0320.js`에서 참조하는 스키마 파일명을 `schema_0320.json`으로 갱신.
* **J-3**: UI 및 설정 스키마 전반에 걸쳐 DPI 레벨을 1~3 정수 범위로 통일.
* **J-4 / J-4b**: 레거시 `precision.enable`을 제거하고 `precision.mode` 키로 통일.
* **J-5**: `config_0320.json` 내부의 불필요한 `meta` 노드 정리 및 `index_0320.html`의 중복 선택자 제거.

### Track 3 — W10 웹서버 안정화
* **H-W1**: WiFi 설정 스키마의 모드 열거형을 `AUTO` / `AP` / `STA`로 명확히 규정.
* **H-W2**: OTA 진행 중 연결이 끊겼을 때 상태 플래그(`_otaInProgress`)가 잠기는 문제를 30초 타임아웃 회수 로직으로 해결.
* **R-1**: OTA 파일 업로드 콜백 내 괄호 불일치 버그 수정.

### Track 4 — 빌드 파이프라인(Gzip) 동기화, CI 및 Git 추적 정상화
* **B-1 (`pio_gzip_0320.py`)**: SCons에서 `buildfs` 실행 시 `littlefs.bin`이 먼저 빌드되고 뒤늦게 동기화되어 웹 파일이 누락되거나 구버전이 패킹되던 치명적 타이밍 버그 해결 (SCons 이미지 타깃 PreAction 및 CLI 타깃 즉시 동기화 등록).
* **B-2 (`.gitignore` & `git rm --cached`)**: 빌드 시 자동 생성되는 `src/**/data_*/www/` 파생물 10건을 Git 인덱스 추적에서 안전하게 제외하고, `compile_commands.json`, `.clangd/`, 런타임 임시파일(`*.tmp`, `*.bak`), Python/OS 캐시 일괄 제외.
* **B-3 (`ci_1_build_009.yml`)**: PR 트리거 `paths` 필터에서 소스인 `src/v032/data_v032/json/**`이 제외(`!`)되어 CI가 무시되던 결함 수정 및 매트릭스 환경별 `clean` 명령 보완.

### Track 5 — 프론트엔드 UI/UX 고도화 및 백엔드 전 기능 연동 (4단계)
* **FE-1 (Quick Control)**: 대시보드에서 런타임 즉시 감도를 바꾸는 `[DPI 1/2/3]` 원클릭 버튼(현재 DPI 자동 하이라이트), 커서 드리프트 즉시 보정 `[자이로 보정]`, 키 고착 비상 해제 `[버튼 강제 릴리즈]`, 버스 이상 복구 `[I2C 복구]` 버튼 추가 및 `/api/control` 완벽 연동.
* **FE-2 (Dashboard Status)**: 대시보드 상태 요약 카드에 `DPI Level`, 커서 손떨림 지표 `Cursor RMS`, 센서 다이 온도 `Sensor Temp` (℃), 실제 센서 샘플링 주기 `Sampling` (ms/Hz) 실시간 모니터링 연동.
* **FE-3 (Diagnostics)**: 웹 카운터 외에 하드웨어/RTOS 에러 카운터 7종(`mpu_nan`, `mutex_miss`, `task_overrun`, `i2c_recover`, `failsafe_rel`, `stack_sensor`, `stack_comm`) 배지 추가, 0 초과 시 붉은색 경고 하이라이트 적용, E10 센서 링버퍼 에러 로그(`diagErrHist` / `e10.err_hist`) 전용 로그 뷰어 구축.
* **FE-4 (OTA Update UX)**: XHR 기반 실시간 진행률 프로그레스 바(0~100% 및 전송 KB), 펌웨어 업로드 중 마우스 입력 차단 안내를 위한 `OTA Guard` 경고 배너 추가.

---

## 14. 🔧 알려진 유보 이슈 (Known Deferred Issues)

향후 추가 고도화 작업 시 AI 어시스턴트가 참고할 미해결/유보 이슈 목록입니다.

| 식별자 | 내용 및 증상 | 관련 파일 | 추후 트리거 조건 |
|:---:|:---|:---|:---|
| **H-5** | `commTask`가 HID 테스트 커맨드 실행 중(최대 250ms) 프레임 처리를 일시 정지함 | `E10_AirMouse_Task_0320.cpp` | `test_click` 실행 중 커서가 순간 멈추는 현상이 체감될 때 |
| **M-6** | `_qFrame` 큐가 크기 1 덮어쓰기 모드이므로 극단적인 고속 스크롤 시 휠 패킷 누락 가능성 | `E10_AirMouse_Core_0320.cpp` | 고속 스크롤 시 휠 입력 손실이 실제 보고될 때 |
| **M-7** | `_state.updated` 변수가 쓰기만 되고 읽히지 않는 미사용 상태로 남아있음 | 여러 파일 | 소스 정리 및 가독성 개선 작업 시 |
| **M-W2** | `mode=STA`로 지정되었으나 `sta_ssid`가 비어있을 때 폴백 모드 없이 무선 연결 불가 | `W10_Web_init_0320.cpp` | WiFi 자동 AP 폴백 정책 수립 시 |
| **M-W1** | `apiKeycodes` 응답 시 캐시가 없어 매 요청마다 232개 엔트리를 순회 생성함 | `W10_WebApi_Status_0320.cpp` | 웹 응답 레이턴시 최적화 요구 시 |
| **L-1** | `getStatus()` 함수가 락을 잡은 상태에서 `_hid.isConnected()`를 호출함 | `E10_AirMouse_Diag_0320.cpp` | 락 점유 시간 단축 최적화 시 |
| **L-4** | `_sendPptKey2FromCfg` 함수가 호출되지 않는 데드 코드로 남아있음 | `E10_AirMouse_Hid_0320.cpp` | 데드 코드 정리 시점 |
| **L-5** | `_getE10RuntimeConfig` 함수가 구현되었으나 현재 사용되지 않음 | `E10_AirMouse_Core_0320.cpp` | 데드 코드 정리 시점 |

---

## 15. 📌 부록: 버전별 사양 변경 비교 (v0.0.6 vs v0320)

| 항목 | 레거시 버전 (v0.0.6) | 현행 버전 (v0320) |
|:---|:---|:---|
| **물리 버튼 수** | 3버튼 (`BTN_L`, `MODE`, `SCROLL`) | **5버튼** (`BTN_L`, `BTN_R`, `BTN_M`, `MODE`, `SCROLL`) |
| **GPIO 할당** | 우클릭 / 휠클릭 핀 미할당 | **`GPIO 15` (R), `GPIO 16` (M)** 신규 추가 |
| **스크롤 동작** | 커서 완전 분리 (100% 락) | 감쇠율 조정 가능 (**기본 25% 보존**) |
| **Click-Lock** | Hard 모드만 지원 | **Hard(완전 고정) / Soft(95% 감쇠)** 2가지 모드 지원 |
| **정밀 조준** | 미지원 | **5단계 프로파일 × 4상태 FSM** 지원 |
| **PPT 제스처 키맵** | 고정 키코드 사용 | **Keymap v2** (Keyboard Usage + Consumer Mask 추상화) |
| **SafeBoot** | 미지원 | 부팅 실패 누적(`fail_count`) 감지 및 **SafeMode 자동 진입** |
| **OTA 보호** | 미지원 | 펌웨어 업로드 중 **OTA Guard** HID 차단 게이트 작동 |
| **웹 관리 API** | 초기 프로토타입 수준 | **30개 이상의 RESTful 엔드포인트** 완비 |
| **설정 영속화** | 단순 직접 파일 덮어쓰기 | **원자적 저장 (tmp $\rightarrow$ bak $\rightarrow$ verify $\rightarrow$ rollback)** |
| **시스템 진단** | 단순 디버그 로그 출력 | **커서 RMS, 에러 이력, FreeRTOS 태스크 스택/dt 모니터링** |
| **샘플링 주기** | 단순 125Hz 표기 | 센서 취득 **125Hz 고정**, HID 전송 **BLE 간격 동기화** |
| **블루투스 스택** | 레거시 BLE 라이브러리 | **NimBLE-Arduino** 기반 고효율 HID over GATT |
| **E10 코드 구조** | 단일 거대 cpp 파일 | **기능별 6개 파일 모듈화 분할** |
| **HID 실행 권한** | 다중 태스크 무분별 직접 접근 | **`commTask` 단일 태스크 전담 실행** |
| **HID 명령 전달** | 인스턴스 직접 호출 | FreeRTOS **`_qHidCmd` 큐를 통한 비동기 위임** |

---

## 16. ✅ 프로젝트 핵심 설계 철학

AirMouse Elite S3는 단순 아두이노 예제를 넘어 상용 입력 디바이스 수준의 견고성과 신뢰성을 지향합니다.

1. **엄격한 태스크 및 상태 소유권 분리**: 각 리소스는 명확한 단일 소유 태스크를 가지며, 태스크 간 데이터 교환은 FreeRTOS 큐를 통합니다.
2. **일관성 있는 동기화 규칙**: 재귀적 뮤텍스(`recursive mutex`)와 명확한 락/언락 짝맞춤을 보장합니다.
3. **다중 브릭 방지 시스템**: SafeBoot, 원자적 설정 쓰기, 백업 롤백, OTA Guard를 통해 어떤 상황에서도 장치가 복구 불능 상태에 빠지지 않도록 합니다.
4. **철저한 시스템 관측성(Observability)**: 커서 RMS, 센서 드리프트, 태스크별 스택 워터마크, 에러 카운터를 노출하여 문제 발생 시 원인을 즉각 추적할 수 있도록 합니다.
5. **정책 기반 API 게이트**: 안전 모드 또는 업데이트 중에는 위험한 설정 변경 및 HID 입력을 원천 차단합니다.

> [!TIP]
> 향후 본 코드베이스에 새로운 기능을 추가하거나 수정할 때는 반드시 위 설계 원칙에 위배되지 않는지 검토한 후 작업을 진행하십시오.
