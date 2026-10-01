# Elite AirMouse S3 — v0410 기술 사양서
- 00.SPEC_0410_002.md

> ESP32-S3-Zero + MPU6050 기반 3-Mode Air Mouse 플랫폼
> BLE HID Composite (Mouse + Keyboard + Consumer) / WS2812 LED / Light-sleep WoM
> **v0410: 다중 프로파일(최대 5개) × 매크로 라이브러리(8×8) × Global+Override 2단 슬롯 매트릭스(27 트리거) × Motion Advanced(Click-Freeze / Adaptive EMA / Snap-to-Axis) × P20 전원관리(Light/Deep Sleep) × C20 버튼 타이밍 고도화**

**소스 버전 접미사: `_0410` (백엔드) / `_0411` (프론트엔드)**
**API 버전: `G_W10_API_VER = 410`**
**문서 대상: 개발자, 유지보수자, AI 어시스턴트**
> 최종 갱신: 2026-10-02 (rev4 — Dead Code 정리 + LED suspend/resume 실구현)  

---

## 📌 AI 어시스턴트용 컨텍스트 (Read First)

이 프로젝트를 수정하기 전 반드시 알아야 할 12가지:

1. **모듈 약어**: `A40`(공용 유틸), `B20`(BLE), `C10`(Config), `C20`(Action), `D10`(로거), `E10`(AirMouse), `L10`(LED), `M10`(모션엔진), `M20`(BiasTracker), `M30`(제스처), `P20`(전원), `W10`(웹서버). 접두사가 파일/클래스/심볼에 그대로 반영.
2. **명명 규칙**: 전역 상수 `G_`, 전역 변수 `g_`, 클래스 `CL_`, struct `ST_`, enum `EN_`, private `_`접두사, 로컬 `v_`, 파라미터 `p_`. **위반 시 리뷰 반려.**
3. **ArduinoJson v7 정책**: `JsonDocument` 단일 타입만. `containsKey`·`createNestedArray/Object` **금지**. `doc["a"]["b"].to<JsonObject>()` 패턴 사용.
4. **E10은 7파일 분할**: `.h` + `Core/Hid/Motion/Diag/Task/Action_0410.cpp`.
5. **태스크 & 상태 소유권**: sensorTask(Core 1)와 commTask(Core 0)가 큐로 통신. **웹 태스크는 HID 직접 접근 금지**. **Special 액션은 sensorTask 단독 실행**.
6. **다중 프로파일 & 슬롯 리졸버**: 최대 5개 독립 프로파일(0~4). 각 프로파일은 27개 고정 트리거(버튼 15, Flick 4, Linear 4, Tilt 4). `_resolveSlot(mode, trigIdx)`에서 Global 기본값 + Mode별 Override(M1/M2/M3) 2단 리졸빙.
7. **매크로 시퀀서 (8×8)**: 프로파일당 최대 8개 매크로, 매크로당 최대 8 step. `EN_C20_ACT_MACRO (10)`. **`_macroAbortToken` 카운터 기반 취소**, BLE 해제/모드 변경/세이프모드 시 즉시 안전 취소. `commTask` 블로킹 없이 `_startMacro`(스냅샷) + `_tickMacro`(상태머신). 실행 중 재호출 시 **조용히 대체(replace)**.
8. **4대 필수 슬롯 잠금 (🔒)**: S1(Top L Click), S5(Top M Hold), S12(Side C Double), S13(Side C 2s Hold)은 브릭 방지를 위해 서버/웹 양측에서 수정 원천 차단.
9. **Action Registry**: 슬롯 값은 `{k, h, p16, p32}` 4-tuple. `k`=kind, `h`=holdMode.
10. **프론트엔드 7-모듈 구조**: `_0411` 파일명 규칙 준수. `am_base` → `am_offline` → `am_profile` → `am_macro` → `am_config` → `am_status` → `app_0411_0001` 순 로드.
11. **[Phase 11.5] Motion Advanced 파이프라인**: `[M10: Adaptive EMA] → [E10: Front Hold] → [E10: Move Gate] → [E10: Precision] → [E10: Snap-to-Axis] → [E10: Click-Freeze]` 순. **Click-Freeze는 HID 전송 직전 최종 게이트**. Reversal Reset은 부호 역전 시 `_lpf = 0.0f` 완전 리셋.
12. **[Phase 11.6 & 11.7] 전원/버튼 라이프사이클**: Sleep 진입 시 `_led.suspend()`, wake 시 `_led.fadein()` + `_led.resume()`, MPU 온도 드리프트 보정을 위해 `_biasTracker.startFastRecalibrate(300ms)` 실행. C20 버튼은 비대칭 디바운스(Press 32ms / Release 16ms), 2-stage 시간+틱 판정, 접점 시간(`rawDownMs`) 보존, 16ms 미만 글리치(`min_click_ms`) 폐기.

---

## 🎯 프로젝트 개요

ESP32-S3-Zero + MPU6050 기반 **자이로 전용 공간 포인팅 에어마우스**. BLE HID Composite로 호스트(PC/macOS/Linux/Android TV/iOS)에 연결되며, 웹 UI로 실시간 커스터마이징을 제공.

### 핵심 특징

- **다중 프로파일 시스템 (v0410)**: 최대 5개 독립 프로파일(작업/게임/발표/미디어 등) 지원, 인덱스 기반 원자적 스위칭
- **Global + Override 2단 슬롯 매트릭스**: 27개 고정 트리거 대상 Global 공통 기본값 + Mode 1~3(PC/PPT/TV) 오버라이드 마스크
- **8×8 매크로 라이브러리**: 최대 8개 매크로, 각 8단계(키/마우스/소비자키 + 0~2000ms 딜레이), 비동기 Live Test 지원
- **4대 필수 기능 잠금 🔒**: 좌클릭/포인팅게이트/모드순환/BLE페어링 수정 차단으로 조작 불능(브릭) 원천 방지
- **3-Mode 시스템**: PC / Presentation / Smart TV — 상황별 슬롯 매트릭스
- **자이로 전용**: 상보 필터 + 시그모이드 가속 + **Adaptive Variable EMA** + Zero Snap
- **제스처 3계층**: Flick (P2P) / Linear (임펄스) / Tilt Hold (자세 유지, **Mode 3 전용**)
- **Front Hold 스크롤**: Side F 누름 중 커서 감쇠 + 수직 휠 + 수평 팬
- **Action Registry**: ~40종 액션 + 매크로 시퀀스, 사용자 자유 배정
- **Zero-rate Bias Tracking**: 드리프트 자동 흡수
- **SafeBoot / OTA Guard**: 브릭 방지 (SafeMode method-aware 게이트)
- **WS2812 LED**: Mode 색상 / Flash / Blink / Fadeout(500ms) / Fadein(300ms) / Suspend & Resume 상태 복원
- **전원 관리 (P20)**: Light-sleep (Mode별/BLE/Pairing 타임아웃) + Deep-sleep (600s) + MPU6050 WoM(800mg) 및 버튼 wake, 슬립 해제 후 자이로 고속 재수집(Fast Recalibrate 300ms)
- **버튼 정밀 디바운스 (C20)**: 비대칭 누름(32ms)/뗌(16ms) 디바운스, 2-Stage 하이브리드(시간+틱) 판정, 16ms 미만 초단 글리치 필터링, 웹 UI 동적 타이밍 제어
- **Multi-Host**: 최대 3 peer bond, disconnect-only 재연결
- **Atomic Config (Schema v5)**: `/json/profiles/` 디렉토리 기반 원자적 IO (`.tmp` → `.old` → rename)
- **AP Fallback**: STA 실패 시 자동 AP (브릭 방지)
- **[Phase 11.5] 포인팅 정밀도 고도화**:
  - **Click-Freeze**: 클릭 순간 커서 동결 (4-state FSM, 150ms 캡 + 2px 이탈)
  - **Adaptive EMA**: Smoothstep 연속 α + Reversal Reset (좌-우 스냅 위상 지연 제거)
  - **Snap-to-Axis**: Mode별 축 스냅 (Soft Snap + Confirm Frames, Chattering 방지)
- **[Phase 11.6 & 11.7] 전원 및 버튼 타이밍 고도화**:
  - **P20 전원 관리**: Light/Deep sleep 독립 타이머, WoM 물리 가속도(25=800mg) 인터럽트, LED 페이드 복원
  - **C20 버튼 타이밍**: 비대칭 디바운스 및 초단 클릭 방지 필터링으로 노후 스위치 채터링 오탐 대폭 감소

---

## 🏗️ 아키텍처

### main.cpp 부팅 시퀀스

```cpp
void setup() {
    // 1) 로깅 초기화
    CL_D10_Logger::begin(Serial);

    // 2) Config 매니저 초기화
    g_cfg.begin(false);

    // 3) Boot-time Factory Reset (Side C 6초 hold)
    if (_holdAtBoot(E10_CONST::PIN_BTN_MODE, 6000)) {
        (void)g_cfg.factoryReset(true);
        ESP.restart();
    }

    // 4) SafeMode 스냅샷
    const bool v_safe = g_cfg.isSafeMode();

    // 5) E10 초기화
    g_e10.begin(&g_cfg);
    if (v_safe) g_e10.setSafeMode(true);

    // 6) W10-E10 브릿지 바인딩 (함수 포인터 테이블)
    g_w10E10If.ctx = (void*)&g_e10;
    // ... 20개 콜백 등록

    // 7) W10 웹 서버 시작 (유일한 E10 진입: e10if)
    g_w10.begin(&g_cfg, &g_w10E10If);
}

void loop() {
    // grace 통과 시 boot ok 마킹 (1회)
    if (!g_bootOkDone && g_cfg.bootMarkOkIfGracePassed(8500)) {
        g_bootOkDone = true;
    }

    // BLE dirty → 프로파일 저장 (200ms cadence)
    g_e10.tickConfigSave();
    delay(200);
}
```

- **`E10_W10Apply` legacy hook 제거** (rev3): W10은 `_e10if` 함수 포인터 테이블만 사용
- **`G_BTN_MODE` 상수 제거**: `E10_CONST::PIN_BTN_MODE` 참조로 통일


### 태스크 모델

| Task | Core | Prio | 주기 | Stack | 역할 |
|---|---|---|---|---|---|
| `_sensorTask` | 1 | 3 | 8ms (125Hz) | 8192 | IMU 읽기, FSM, 제스처, **Adaptive EMA**, **Snap-to-Axis**, **Click-Freeze**, 프레임/커맨드 생산, Special 액션 |
| `_commTask` | 0 | 2 | 7ms | 4096 | 큐 소비 → HID 전송, 액션 실행, 매크로 상태머신 tick |
| `_ledTask` | 0 | 1 | 50ms | 2048 | LED 상태머신 **단독** tick |
| `AsyncWebServer` | (ESP-IDF 내부) | - | 이벤트 | - | HTTP, E10 API는 enqueue만 |
| main loop | 0 | - | 200ms | - | grace 판정, tickConfigSave |

### 데이터 흐름 (반드시 준수)

```
sensorTask ──push──> _qFrame (size=1, overwrite) ──recv──> commTask ──> HID
     │                                                            ▲
     │ (관측용)                                                    │
     └──lock──> _state ──read──> getStatus() (웹)                 │
                                                                  │
sensorTask ──enqueue──> _qActionExec (size=8) ──recv─────────────┤
web/sensorTask ──enqueue──> _qHidCmd (size=4) ──recv─────────────┘

Special 액션: sensorTask에서 _handleSpecial() 직접 호출 (commTask 안 거침)
매크로:      commTask에서 _startMacro() 스냅샷 + _tickMacro() 상태머신 (블로킹 없음)
LED tick:    _ledTask(50ms) 단독
```

**모션 파이프라인 (sensorTask 내부, 단방향 10단계)**:

```
[MPU6050 Raw Gyro]
       │
       ▼
[NaN Guard] ──────────────── (NaN 감지 시 I2C 복구 + Skip)
       │
       ▼
[M20: BiasTracker 보정]
       │
       ▼
[M10: Adaptive Variable EMA + Reversal Reset]   ← Phase 2 (v0411)
       │
       ▼
[M10: Zero Snap + Sigmoid 가속]
       │
       ▼
[E10: DPI Scaling + accel shaping]
       │
       ▼
[E10: Front Hold (Side F) 감쇠 + Wheel/Pan 산출]  ← 전 Mode 일관
       │
       ▼
[E10: Move Gate (Top M) — 미누름 시 fx=fy=0]
       │
       ▼
[E10: Precision FSM]
       │
       ▼
[E10: Snap-to-Axis]                              ← Phase 3 (v0411)
       │
       ▼
[E10: Click-Freeze 게이트]                       ← Phase 1 (v0411)
       │
       ▼
[BLE HID Mouse Report]
```

**절대 금지:**
- 웹 태스크가 `_mouse`/`_keyboard` 직접 호출 → 반드시 `_qHidCmd` 경유
- sensorTask가 HID 직접 호출 → 반드시 `_qActionExec` 경유
- commTask가 `_state` 접근 → `_qFrame`만 사용
- commTask가 `_handleSpecial()` 실행 → **sensorTask 단독**
- commTask가 `_led.tick()` 호출 → **_ledTask 단독**
- 매크로 실행 블로킹 → **스텝 단위 상태머신 (_tickMacro)**
- web 태스크가 `_biasTracker.reset()` / `_btnDisp.resetAll()` / `_gesture.reset()` 직접 호출 → **sensorTask 위임**

### 상태 소유권

| 상태 | 소유자 | 접근 방식 |
|---|---|---|
| `_state` (ST_E10_State_t) | sensorTask (관측), web (setSafeMode 등) | `_lock()` 하에 read/write |
| `_qFrame` | sensorTask(write), commTask(read) | FreeRTOS queue (lock-free) |
| `_qActionExec` | sensorTask/web(write), commTask(read) | FreeRTOS queue (timeout=0) |
| `_qHidCmd` | web(write), commTask(read) | FreeRTOS queue (timeout=0) |
| `_cfgProfile` (snapshot) | `_lock()` 하에만 | H-3 원자화 경로 |
| motion config | setter: lock, reader: loop 시작 스냅샷 | C-4 |
| `_errHist`, `_spikes` | `_pushErr/_pushSpike` (내부 lock) | C-2 |
| `_hid` (BleCompositeHID) | **commTask 단독** | (예외: `isConnected()` read) |
| `_mpu`, `Wire` | **sensorTask 단독** | recover도 sensorTask |
| `_gesture` | sensorTask 단독 | update만 |
| `_biasTracker` | **sensorTask 단독** | reset은 sensorTask 위임 (D-1) |
| `_ble` | sensorTask (tick), web (pairing) | 락 없음 (volatile 필드) |
| `_power` | sensorTask 단독 | sleepNow는 sensorTask |
| `_led` | **_ledTask 단독** | setModeColor/flash 등은 어느 태스크에서나 (내부 상태머신만 _ledTask가 tick) |
| `_frontHoldActive` | sensorTask (Side F 이벤트) | volatile, 원자적 |
| `_moveGateHeld` | sensorTask (Top M 이벤트) | volatile, 원자적 |
| `_activeMode` | sensorTask (Mode 전환), web(set via apply) | volatile, 원자적 |
| `_macroAbortToken` | 어느 태스크나 (취소 트리거) | `volatile uint32_t` |
| `_macroState.active` | 어느 태스크나 | `volatile bool` |
| `_reqResetBtnDisp` / `_reqResetGesture` | web set / sensorTask consume | `volatile bool` |
| **[Phase 1] `_btnLDown`** | **sensorTask (Top L 이벤트)** | **volatile bool** |
| **[Phase 1] `_freezeState` FSM** | **sensorTask 단독** | – |
| **[Phase 3] `_snapActiveAxis` / `_snapCandidate`** | **sensorTask 단독** | – |
| **[Phase 2] M10 `_lpfX` / `_lpfY` / `_prevSign*_neg`** | **sensorTask 단독 (M10 내부)** | – |
| `_topMDownMs` | **sensorTask (Top M DOWN)** | ⚠️ **Dead Code** — Mode 3 클릭 판정 슬롯 위임 후 미사용 |

### Mutex 정책

- `_mutex`: **recursive mutex** (`xSemaphoreCreateRecursiveMutex`). 이유: `setSafeMode`/`setOtaGuard`가 락 보유 중 `_pushErr` 재진입.
- `_lock()` / `_unlock()`: RAII 아님. 모든 경로에서 짝 맞춤.
- `_qFrame` overwrite: 무조건 성공(size=1).
- `_qHidCmd` / `_qActionExec` enqueue: `timeout=0`. 웹 태스크 블로킹 금지.

### 태스크 간 Special 액션 라우팅

**핵심 규칙:** Special 액션(`EN_C20_ACT_SPECIAL`)은 **sensorTask에서만 실행**.

```
_handleSlotButton / _handleGesture
  ├─ kind == SPECIAL → _handleSpecial() 직접 호출 (sensorTask 컨텍스트)
  └─ kind != SPECIAL → _enqueueAction() → commTask
```

`CL_C20_ActionExec::exec()`의 `EN_C20_ACT_SPECIAL` case는 안전망으로 무시(drop).

### 매크로 실행 (비동기 상태머신)

매크로는 **commTask 블로킹 없이** 스텝 단위로 진행:

```
commTask 큐 드레인
  ├─ MACRO kind → _startMacro(p_idx)  ← 스냅샷만, 즉시 리턴
  │                                    (실행 중이면 WARN 로그 + 조용히 대체)
  └─ 기타       → _actExec.exec()     ← 즉시 실행

commTask 매 루프 후반
  └─ _tickMacro()  ← delay 경과 시 다음 스텝 실행, 커서 프레임 소비 지속
```

**취소**: `_macroAbortToken` 카운터가 `_macroState.startToken`과 다르면 즉시 abort. `switchProfile`, `_setActiveMode`, `forceReleaseButtons`, BLE disconnect edge, SafeMode/OTA gate 진입 시 토큰 증가. **`_startMacro` 자체는 토큰을 증가시키지 않음** (조용히 대체).

---

## 📁 프로젝트 구조

### 백엔드 (`src/v0410/`)

```
src/
├── main.cpp                          부팅 시퀀스 (C10 → Factory Reset → E10 → W10)
└── v0410/
    ├── A40_ComFunc_0410.h              공용 유틸 (JSON/Mutex/IO)
    ├── B20_Ble_0410.h/.cpp             BLE Manager (Pairing/Bonds/Host Cycle)
    ├── C10_Config_0410.h/.cpp          다중 프로파일 & LittleFS 원자적 I/O
    ├── C10_Def_0410.h                  Schema v5, Profile/MacroLib 데이터 구조체
    ├── C20_Action_0410.h               Action Registry (EN_C20_ACT_MACRO 추가)
    ├── C20_ActionExec_0410.h/.cpp      Action 실행기 (HID)
    ├── C20_BtnDispatcher_0410.h/.cpp   버튼 상태머신
    ├── D10_Logger_0410.h               링버퍼 로거
    ├── E10_Def_0410.h                  E10 상수/enum
    ├── E10_AirMouse_0410.h             E10 클래스
    ├── E10_AirMouse_Core_0410.cpp      초기화/런타임 적용/프로파일 스위칭/_resolveSlot
    ├── E10_AirMouse_Hid_0410.cpp       HID primitives
    ├── E10_AirMouse_Motion_0410.cpp    Precision / Snap / Click-Freeze FSM
    ├── E10_AirMouse_Diag_0410.cpp      진단/캘리브/status (biasTracker reset은 sensorTask 위임)
    ├── E10_AirMouse_Task_0410.cpp      sensorTask / commTask / ledTask / MACRO 디스패치
    ├── E10_AirMouse_Action_0410.cpp    27개 트리거 매핑 + _startMacro/_tickMacro + Special
    ├── L10_Led_0410.h/.cpp             WS2812 LED 컨트롤러
    ├── M10_MotionProc_0410.h           물리 엔진 (Roll+Pitch)
    ├── M20_BiasTracker_0410.h          Zero-rate Bias
    ├── M30_Gesture_0410.h/.cpp         Flick/Linear/Tilt (Tilt은 Mode 3 전용)
    ├── P20_Power_0410.h/.cpp           Light-sleep + WoM
    ├── W10_Def_0410.h                  웹 상수/타입 (ST_W10_E10If_t 브릿지)
    ├── W10_Web_0410.h                  웹 클래스
    ├── W10_Web_init_0410.cpp           begin/라우팅/WiFi/mDNS
    ├── W10_Web_Static_0410.cpp         정적서빙/body slot/diag
    ├── W10_WebApi_Com_0410.cpp         API 공통/정책/ETag (SafeMode 게이트 method-aware)
    ├── W10_WebApi_Config_0410.cpp      /api/config/* (프로파일 기반)
    ├── W10_WebApi_Profile_0410.cpp     /api/profiles/*, /api/triggers, /api/action/test*
    ├── W10_WebApi_CtlPpt_0410.cpp      /api/control, /api/ppt/test (deprecated stub)
    ├── W10_WebApi_OtaBoot_0410.cpp     OTA/SafeBoot/Reboot/reboot-check
    ├── W10_WebApi_Status_0410.cpp      /api/status/diag/keycodes (profile_* 필드 포함)
    ├── tools_v0410/pio_gzip_0410.py    gzip 사전 생성
    ├── data_v0410_www/                 웹 소스 (VCS)
    └── data_v0410/                     LittleFS 이미지
        ├── www/                        빌드 산출물
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
        └── json/                       (자동 생성 가능)
            ├── active_profile.json
            ├── boot_state_0410.json
            ├── profiles/
            |      ├── profile_0.json
            |      └── profile_1.json
            └── public/manifest_0410.json
```

### 프론트엔드 (`data_v0410/www/`)

```
www/
├── index_0411.html              메인 UI (v0410)
├── style_0411.css               스타일 (v0411)
├── app_0411_0001.js             UI 바인딩 + Main (7-모듈 로더)
└── lib/
    ├── am_base_0411.js          상수/유틸/API 래퍼/전역 상태/로딩 헬퍼
    ├── am_offline_0411.js       오프라인 시뮬레이터 (localStorage 스키마 v5)
    ├── am_profile_0411.js       프로파일 CRUD/Slots/ActionEditor
    ├── am_macro_0411.js         매크로 편집기 (8×8)
    ├── am_config_0411.js        E10 Config/Control/SafeBoot/Power/Button
    └── am_status_0411.js        Status/Diag/OTA/재부팅 배너/Quick Tuning
```

### E10 7파일 분할 기준

| 파일 | 담당 |
|---|---|
| `Core` | `begin`, ctor, `applyRuntimeE10`, `_resolveSlot`(C-4 락), `reloadActiveProfile`, `saveActiveProfile`, `switchProfile`, `execLiveTest`, `_snapshotRuntimeToE10Config`, `_enqueueHidCmd`, `tickConfigSave` |
| `Hid` | `_tapComboUsageKb`, `_tapUsageKb`, `_tapConsumerMask`, `_sendPptKey2`, `testPptKey2`, `testMouseClick`, `forceReleaseButtons` (macro 토큰 abort), `_doReleaseAllButtons` |
| `Motion` | `_applyPrecision`, `_fsmUpdate`, `_applySnapToAxis`, `_applyClickFreeze` |
| `Diag` | `_pushErr`, `_pushSpike`, `_recoverI2C`, `_runGyroCalibration` (D-2: `_ble.tick()` 포함), `getStatus`, `requestGyroCalibration` (D-1: biasTracker reset 위임), `requestI2CRecover`, `clearDiagnostics` (D-3: 플래그만) |
| `Task` | `_sensorTask`, `_commTask` (매크로 상태머신 tick), `_ledTask` |
| `Action` | `_handleHardcodedButton` (C-1: Top M CLICK 슬롯 위임), `_handleSlotButton`, `_handleGesture`, `_setActiveMode`, `_handleSpecial`, `_startMacro` (H-4 스냅샷), `_tickMacro` (H-1 토큰) |
| `.h` | 선언 + inline (`_lock`, `_unlock`, `_pushFrame`, `_welfordAdd`, `_calcRms`, `_mouseSend`) |

---

## 📐 명명 규칙 & 코드 정책

### 명명 규칙 (엄격)

| 대상 | 접두사/접미사 | 예 |
|---|---|---|
| namespace | `{모듈}_` | `A40_ComFunc`, `C10_DEF`, `E10_CONST` |
| 전역 상수/매크로 | `G_{모듈}_` | `G_C10_CFG_VER`, `G_W10_API_VER` |
| 전역 변수 | `g_{모듈}_` | `g_cfg`, `g_e10` |
| 전역 함수 | `{모듈}_` | `W10_hasVersionToken` |
| typedef | `_t` 접미사 | `ST_C10_WiFiConfig_t` |
| enum 상수 | `EN_{모듈}_` | `EN_C10_WIFI_AUTO` |
| 구조체 | `ST_{모듈}_` | `ST_E10_Status_t` |
| 클래스 | `CL_{모듈}_` | `CL_E10_EliteAirMouse` |
| private 멤버 | `_` 접두사 | `_state`, `_lock()` |
| 클래스 정적 | `s_` 접두사 | `s_buffer`, `s_mux` |
| 함수 로컬 | `v_` 접두사 | `v_dt`, `v_gyroAbs` |
| 함수 인자 | `p_` 접두사 | `p_enable`, `p_code` |

### ArduinoJson v7 정책

**허용:**
```cpp
JsonDocument doc;
doc["a"] = 1;
doc["b"]["c"] = 2;
JsonObject o = doc["x"].to<JsonObject>();
JsonArray arr = doc["y"].to<JsonArray>();
JsonVariant v = doc["z"];
if (!v.isNull()) { ... }
```

**금지:**
```cpp
doc.createNestedObject("x");    // 금지
doc.createNestedArray("y");     // 금지
doc.containsKey("k");           // 금지
StaticJsonDocument<256> d;      // 금지 (v6)
DynamicJsonDocument d(256);     // 금지 (v6)
```

### 문자열/초기화

- `memset` + `strlcpy` 조합 (Arduino String 지양)
- `snprintf` overflow 반드시 검사
- JSON 문자열 export 시 `_appendJsonEscaped` 또는 `_resPrintJsonString`

### 안전 IO

- config 저장: `tmp write → verify → main→old rotate → tmp→main commit → verify → 실패 시 old rollback`
- `.old` 파일은 **성공 시 제거** (프로파일 본체)
- 프로파일 인덱스(`active_profile.json`)는 별도 `.tmp` + rename 방식

---

## 🔌 하드웨어

### 물리 배치

```
      [Top 면]
   [L] [M] [R]      ← 앞쪽 3버튼
   
   ┌─────────────┐
[F]│             │
[C]│             │  ← 좌측면 (Front/Center/Rear)
[R]│             │
   └─────────────┘
```

### GPIO 배정

| 기능 | GPIO | RTC | Wake | 비고 |
|---|---|---|---|---|
| I2C SDA | 4 | ✓ | - | MPU6050 |
| I2C SCL | 5 | ✓ | - | MPU6050 |
| MPU INT1 | 6 | ✓ | WoM | Motion Detection |
| Top L | 12 | ✓ | EXT1 | Mouse L Hold |
| Side C | 13 | ✓ | EXT1 | Mode Cycle / Pairing / Host Cycle |
| Side F | 14 | ✓ | EXT1 | Front Hold (스크롤) / CLICK / LONG |
| Top R | 15 | ✓ | EXT1 | Mouse R |
| Top M | 16 | ✓ | EXT1 | Move Gate |
| Side R | 7 | ✓ | EXT1 | Rear |
| WS2812 DIN | 21 | ✗ | - | 단일 LED |
| Battery ADC | 1 | ✗ | - | (미구현) |

### MPU6050 설정

- Gyro Range: 250°/s
- Accel Range: 2G
- DLPF: 21Hz
- I2C: 400kHz
- INT: open-drain, active-low, latch

### 좌표계 규약 (필수)

| 물리 축 | 사용자 용어 | 용도 |
|---|---|---|
| gx | **Roll** (긴 축) | 커서 Y, Linear U/D, Flick U/D, **Front Hold 수평 팬** |
| gy | **Pitch** (좌우 축) | 휠, Linear L/R, **Front Hold 수직 휠** |
| gz | **Yaw** (수직 축) | 커서 X, Flick L/R |

**상보 필터:**
```cpp
_roll  = 0.98*(_roll  + gy*dt) + 0.02*accelRoll   // atan2(ay, az)
_pitch = 0.98*(_pitch + gx*dt) + 0.02*accelPitch  // atan2(-ax, az)
```

---

## 🎛️ Mode 시스템

### 3 Modes

| Mode | 이름 | LED | 용도 |
|---|---|---|---|
| 1 | PC Air Mouse | Blue | PC 커서/클릭 |
| 2 | Presentation | Green | PPT 슬라이드/제스처 |
| 3 | Smart TV | Orange | TV D-Pad/미디어 |

### 전환 (고정)

| 트리거 | 동작 |
|---|---|
| Side C Double Click | Mode 1 → 2 → 3 → 1 순환 |
| Side C Long (2초) | Pairing Mode 진입 (LED blink) |
| Side C 3초 hold (단독) | Host Cycle (disconnect + 재광고) |

**Mode 전환 시:**
- `_macroAbortToken++` — 진행 중인 매크로 즉시 취소
- `_actExec.releaseAll()` — 진행 중 홀드/repeat 해제
- `_btnDisp.resetAll()` — 클릭 대기 리셋
- `_gesture.reset()` — 제스처 상태 리셋
- `_frontHoldActive = false` — Front Hold 리셋
- `_ble.exitPairing()` — Pairing 중이면 취소
- FSM 리셋 (`_precSub = EN_PREC_OFF`)
- `forceReleaseButtons()` — HID 안전
- LED: 신규 Mode 색 + 흰색 500ms flash

### 2단 슬롯 매트릭스 (Global + Mode Override)

v0410은 모드마다 독립 배열을 중복 저장하지 않고 **Global 기본값(27개)**에 **Mode 1/2/3별 Override 마스크(`mask`) + 슬롯 배열**을 적용하는 2단 매트릭스를 운용합니다.

```cpp
// _resolveSlot(mode, trigIdx):
// 1. Mode가 1~3이고 mode.mask의 (1 << trigIdx)가 켜져 있으면 mode.slots[trigIdx] 반환
// 2. 아니면 global.slots[trigIdx] 반환
// C-4: _lock() 하에 스냅샷 (프로파일 스위치 중 부분 갱신 방지)
```

### 27개 고정 트리거 인덱스 매핑

| Index | ID | 트리거 이름 | 그룹 | 잠금 정책 |
|:---:|:---:|---|---|:---:|
| 0 | S1 | Top L Click | button | 🔒 **잠금** (Mouse Left Hold) |
| 1 | S2 | Top L Double | button | 자유 편집 |
| 2 | S3 | Top L Long | button | 자유 편집 |
| 3 | S4 | Top M Click | button | 자유 편집 |
| 4 | S5 | Top M Hold | button | 🔒 **잠금** (Move Gate) |
| 5 | S6 | Top R Click | button | 자유 편집 |
| 6 | S7 | Top R Double | button | 자유 편집 |
| 7 | S8 | Top R Long | button | 자유 편집 |
| 8 | S9 | Side F Click | button | 자유 편집 |
| 9 | S10 | Side F Long | button | 자유 편집 |
| 10 | S11 | Side C Click | button | 자유 편집 |
| 11 | S12 | Side C Double | button | 🔒 **잠금** (Mode Cycle) |
| 12 | S13 | Side C 2s Hold | button | 🔒 **잠금** (Pairing Mode) |
| 13 | S14 | Side R Click | button | 자유 편집 |
| 14 | S15 | Side R Long | button | 자유 편집 |
| 15 | F1 | Flick LEFT | gesture | 자유 편집 |
| 16 | F2 | Flick RIGHT | gesture | 자유 편집 |
| 17 | F3 | Flick UP | gesture | 자유 편집 |
| 18 | F4 | Flick DOWN | gesture | 자유 편집 |
| 19 | L1 | Linear LEFT | gesture | 자유 편집 |
| 20 | L2 | Linear RIGHT | gesture | 자유 편집 |
| 21 | L3 | Linear UP | gesture | 자유 편집 |
| 22 | L4 | Linear DOWN | gesture | 자유 편집 |
| 23 | T1 | Tilt Hold LEFT | tilt | 자유 편집 (**Mode 3 전용**) |
| 24 | T2 | Tilt Hold RIGHT | tilt | 자유 편집 (**Mode 3 전용**) |
| 25 | T3 | Tilt Hold UP | tilt | 자유 편집 (**Mode 3 전용**) |
| 26 | T4 | Tilt Hold DOWN | tilt | 자유 편집 (**Mode 3 전용**) |

> 🔒 **4대 필수 슬롯 잠금**: S1, S5, S12, S13은 브릭 방지를 위해 서버(`G_C10_TRIG_LOCKED[]`) 및 클라이언트(`am_profile_0411.js`) 양측에서 편집 차단.

> 🎯 **Tilt Mode 제한**: Tilt Hold는 오작동 방지를 위해 **Mode 3 (TV)** 에서만 실제 발동. 웹 UI의 Mode 1/2 뷰에서는 해당 그룹이 회색 처리되어 편집 비활성화 (Global 상속 뷰에서는 편집 가능, 값은 Mode 3에 상속됨).

### 하드코딩 규칙 (`_handleHardcodedButton`)

- **Top L**: DOWN → Mouse Hold press, UP → release, CLICK → 스킵(중복 방지)
- **Top M**: DOWN → `_moveGateHeld=true`, UP → false. **CLICK/LONG은 슬롯 매핑(S4) 위임** (C-1 수정)
- **Side C**: DOUBLE → Mode Cycle, HOLD_2S → Pairing, **HOLD_3S → Host Cycle (단독)**
- **Side F**: DOWN → `_frontHoldActive=true`, UP → false (CLICK/LONG은 슬롯 매핑 위임)

### 버튼 디스패처 및 타이밍 (C20, Phase 11.7)

#### 설정 구조체 (`ST_C10_ButtonConfig_t`)

| 필드 | 기본값 | 검증 범위 | 설명 |
|---|---|---|---|
| `debounce_press_ms` | 32 | 8~100 (8ms 배수) | DOWN 디바운스 (Press ≥ Release 강제) |
| `debounce_release_ms` | 16 | 8~100 (8ms 배수) | UP 디바운스 (노후 스위치 릴리즈 바운스 방지) |
| `long_delay_ms` | 800 | 300~2000 | Long press 판정 임계 (실제 접점 시점 기준) |
| `double_delay_ms` | 320 | 150~800 | Double click 최대 윈도우 |
| `hold_2s_ms` | 2000 | 1000~5000 | Hold 2s 판정 (Side C Pairing) |
| `hold_3s_ms` | 3000 | 2000~8000 | Hold 3s 판정 (Side C Host Cycle) |
| `min_click_ms` | 16 | 0~100 (8ms 배수) | 초단 클릭 필터 (DISCARD) |
| `debounce_min_ticks` | 3 | 1~10 | 2-Stage 하이브리드 카운터 조건 |

#### 핵심 동작 알고리즘
1. **비대칭 디바운스 (실제 정책: Release < Press)**:
   - 누름(Press): 32ms — 손가락 접촉 안정화 우선 (빠른 클릭 오탐 방지)
   - 뗌(Release): 16ms — 릴리즈 즉시 CLICK/DOUBLE 판정 (지연 최소화)
   - ※ SPEC 초안의 "Press 20 / Release 30"은 폐기. `validateE10`에서 `press ≥ release` 강제.
2. **2-Stage 하이브리드 검증 (Hybrid Time + Tick)**:
   - 시간 조건: `(now - lastRawChangeMs) >= debounce_ms`
   - 카운터 조건: `stableCount >= debounce_min_ticks` (기본 3)
   - `v_timeOk && v_countOk && (v_raw != b.stableState)` 동시 충족 시에만 상태 전이
3. **최초 접점 시점 보존 (`rawDownMs`)**:
   - 최초 누름 에지(`v_raw && b.rawDownMs == 0`) 시각을 고정하여 중간 바운스로 인한 시간 측정 왜곡 원천 방지
   - 디바운스 전 글리치 해제 시 `rawDownMs` 즉시 초기화
4. **초단 클릭 필터링 (`DISCARD`)**:
   - `heldMs < min_click_ms` (기본 16ms) 시 CLICK 대기 진입 취소 → `PHASE_IDLE`
   - `rawDownMs`(최초 접점) 기준으로 측정하여 바운스 중 타임스탬프 왜곡 방지
5. **동적 타이밍 반영**:
   - `_btnDisp.setTimings()`를 통해 웹 UI 및 프로파일 설정 변경 즉시 원자적 적용

---

## 🎬 Action Registry & 매크로 라이브러리

### Action Kind

| Kind | 이름 | 설명 | 파라미터 규약 |
|:---:|---|---|---|
| 0 | `NONE` | 동작 없음 | - |
| 1 | `MOUSE_CLICK` | 마우스 버튼 탭 | `p16`: 버튼 마스크 (1=L, 2=R, 4=M, 8=Back, 16=Forward) |
| 2 | `MOUSE_HOLD` | 마우스 버튼 홀드/토글 | `p16`: 버튼 마스크 |
| 3 | `MOUSE_WHEEL` | 마우스 휠 스크롤 | `p16`: `(axis << 8) \| dir` (0=Y, 1=X) |
| 4 | `KB_TAP` | 키보드 단일 탭 | `p16`: HID usage, `p32`: modifier |
| 5 | `KB_COMBO` | 키보드 콤보 입력 | `p32`: `mod \| (u1<<8) \| (u2<<16) \| (u3<<24)` |
| 6 | `KB_REPEAT` | 키보드 연속 반복 | `p16`: HID usage, `p32`: modifier |
| 7 | `CONSUMER_TAP` | 멀티미디어 키 탭 | `p32`: Consumer usage mask |
| 8 | `CONSUMER_REPEAT`| 멀티미디어 키 반복 | `p32`: Consumer usage mask |
| 9 | `SPECIAL` | 펌웨어 내부 특수 기능 | `p16`: Special Action ID (sensorTask 단독) |
| 10 | `MACRO` | **[v0410] 매크로 실행** | `p32`: 매크로 라이브러리 인덱스 (0~7) |

### 매크로 시퀀서 규칙 (8×8)
- 프로파일당 최대 8개 매크로, 매크로당 최대 8개 Step
- 각 Step: 기본 액션(`k, h, p16, p32`) + `d` (0~2000ms delay)
- 중첩 방지: 매크로 Step 내 `SPECIAL` / `MACRO` 사용 불가
- **안전 취소**: `_macroAbortToken` 카운터 방식. `switchProfile`, `_setActiveMode`, `forceReleaseButtons`, BLE disconnect, SafeMode/OTA gate 진입 시 즉시 abort
- **대체(Replace)**: 실행 중 `_startMacro(newIdx)` 호출 시 `WARN` 로그 후 조용히 대체. `startToken = _macroAbortToken`(현재 값)으로 재설정
- **비동기 실행**: `_startMacro`(스냅샷) + `_tickMacro`(commTask 매 루프). 커서 프레임 소비 블로킹 없음

### Special Action

| 값 | 이름 | 처리 |
|---|---|---|
| 0 | NONE | - |
| 1 | GYRO_RECALIB | `requestGyroCalibration()` |
| 2 | SLEEP_NOW | `_led.off()`, sleep 예약 |
| 3 | MODE_CYCLE | `_setActiveMode(next)` |
| 4 | PAIRING | `_ble.enterPairing(30000)` + LED blink |
| 5 | HOST_CYCLE | `_ble.cycleActivePeer()` + `reconnectToActivePeer(10000)` |

**모든 SPECIAL은 sensorTask에서 실행** (commTask로 enqueue 안 됨).

### Slot 구조

```cpp
struct ST_C20_ActionSlot_t {
    uint8_t  kind;      // EN_C20_ActionKind_t
    uint8_t  holdMode;  // 0=NONE, 1=PRESS_HOLD, 2=REPEAT
    uint16_t param16;
    uint32_t param32;
};  // 8 bytes
```

### Consumer Mask (`EN_C20_Consumer_t`)

| 이름 | mask |
|---|---|
| Volume Up | 0x0001 |
| Volume Down | 0x0002 |
| Mute | 0x0004 |
| Play/Pause | 0x0008 |
| Stop | 0x0010 |
| Next Track | 0x0020 |
| Prev Track | 0x0040 |
| FF | 0x0080 |
| Rewind | 0x0100 |
| AC Back | 0x0200 |
| AC Home | 0x0400 |
| AC Search | 0x0800 |
| Power | 0x1000 |
| TV Input | 0x2000 |
| CH Up | 0x4000 |
| CH Down | 0x8000 |

### KB Modifier

| 이름 | mask |
|---|---|
| LCtrl | 0x01 |
| LShift | 0x02 |
| LAlt | 0x04 |
| LMeta | 0x08 |
| RCtrl | 0x10 |
| RShift | 0x20 |
| RAlt | 0x40 |
| RMeta | 0x80 |

---

## 🎮 제스처 시스템 (M30)

### 활성 조건 (Mode별)

| 제스처 | 활성 Mode | 활성 조건 |
|---|---|---|
| Flick L/R | 모든 Mode | 항상 (gz 기반 회전) |
| Flick U/D | 모든 Mode | Middle Hold 해제 시 |
| Linear | 모든 Mode | Middle Hold 중 |
| **Tilt Hold** | **Mode 3만** | Middle Hold 해제 + 자세 유지 |

**Tilt가 Mode 3 전용인 이유**: PC/PPT에서 자연스러운 손목 기울임이 오작동 유발(무한 방향키 반복). TV 리모컨 D-Pad 대체 용도에 부합.

### Flick (Peak-to-Peak)

- 200ms 슬라이딩 윈도우, `p2p > p2p_th` → 부호 판정
- 쿨다운 600ms
- 좌/우: `gz`, 상/하: `gx`

### Linear (임펄스 적분)

- 중력 성분 제거 → `|a_lin| > th` → 300ms 윈도우 적분
- Middle Hold 중만

### Tilt Hold

- Roll/Pitch 자세 > `angle_deg` 유지 300ms → 방향 판정
- 반복 `repeat_hz` Hz
- **Mode 3 + Middle Hold 해제 시만**

---

## 🌀 Front Hold 스크롤

- **트리거**: Side F 누름 중 (`_frontHoldActive = true`)
- **축**: `gy`(수직 휠) / `gx`(수평 팬)
- **커서 감쇠**: `scroll_cursor_damp` (기본 0.25)
- **모든 Mode 일관**
- **CLICK/LONG 관계**: Hold 중엔 슬롯 발동 안 함. 놓을 때 CLICK/LONG 판정

---

## ⚙️ 물리 엔진 (M10)

- **상보 필터**: Roll + Pitch 2축
- **시그모이드 가속**: `dpiGain = 15 + dpi_level*7`, `out = dpiGain / (1 + exp(-0.8*(|in| - 2.0)))`
- **적응형 EMA (Phase 11.5)**: Smoothstep 기반 연속 α + Reversal Reset
  - `alpha_min = 0.05`, `alpha_max = 0.80`
  - `deadzone_th = 3.0`, `fast_th = 15.0`, `reversal_th = 8.0`
- **Zero Snap**: `|out| < 0.6 → 0`
- **Click-Lock (150ms)**: `hard_click_lock=true` 완전 고정 / false 시 95% 감쇠

---

## 🧠 Zero-rate Bias Tracking (M20)

- **정지 판정**: `mag = |gx| + |gy| + |gz| < still_th`
- **연속 정지**: `still_win_ms` 유지 시 bias 추종
- **파라미터**: `still_th=2.0`, `still_win_ms=250`, `alpha=0.001`
- **재캘리브**: `requestGyroCalibration()` → **sensorTask가 biasTracker.reset() 실행** (D-1)
- **Fast Recalibrate (Phase 11.6)**:
  - 목적: 슬립 중 MPU6050 센서 온도 변화로 발생하는 Gyro bias drift를 웨이크업 직후 빠르게 흡수
  - API: `startFastRecalibrate(uint16_t p_durationMs = 300)`
  - 동작: 웨이크업 시 300ms 동안 집중 수집, 정지(`mag < still_th`) 감지 시 샘플 평균으로 bias 즉시 갱신 (움직임 시 기존 bias 유지)

---

## 💡 LED (L10)

### 상태머신

`IDLE` (Mode 색 점등) / `FLASH` (단발성 점등) / `BLINK` (주기 점멸) / `FADEOUT` (감쇠 소등) / `FADEIN` (점진 점등) / `OFF`

### 색상

| 이름 | RGB | 용도 |
|---|---|---|
| OFF | (0,0,0) | - |
| BLUE | (0,0,255) | Mode 1 |
| GREEN | (0,255,0) | Mode 2 |
| ORANGE | (255,128,0) | Mode 3 |
| WHITE | (255,255,255) | flash |
| RED | (255,0,0) | 경고/슬립 |

### 피드백

| 이벤트 | 반응 |
|---|---|
| 부팅 완료 | Mode 색 solid |
| Mode 전환 | 신규 색 + 흰색 500ms flash |
| Pairing 진입 | 현재 Mode 색 1Hz blink |
| Host Cycle | 흰색 500ms flash |
| Sleep 진입 | `suspend(snap)` → **blocking** `fadeout(RED, led_fadeout_ms=500)` → OFF (최대 700ms 대기) |
| Wake 복귀 | `resume(snap)` → **async** `fadein(ModeColor, led_fadein_ms=300)` → IDLE (원상태 복원) |
| Sleep 실패/취소 | 즉시 `resume(snap)` 복구 (영구 소등 방지) |

### 슬립 연동 수명주기 API (Phase 11.6)
- `setFadeTimings(uint16_t p_fadeoutMs, uint16_t p_fadeinMs)`: 프로파일의 `power.led_fadeout_ms`/`led_fadein_ms` 주입 (0 방어 → 1ms)
- `suspend(ST_LedSnapshot_t& p_out)`: 스냅샷 백업 + **blocking** RED fadeout → OFF (deadline = `_fadeoutMs + 200ms`)
- `resume(const ST_LedSnapshot_t& p_in)`: OFF → **async** base color fadein (원본 FADEIN/BLINK 등 지속 상태는 sleep과 무관하므로 IDLE로 복원)
- `fadein(EN_L10_Color_t p_color, uint16_t p_ms = 300)`: 지정 색상으로 점진적 밝기 증가 (비동기)

**실행자**: `_ledTask` 단독 tick (50ms)

---

## 📡 BLE (B20)

### Pairing Mode
- 트리거: Side C 2초 hold
- 30초 타임아웃, 연결 시 자동 종료
- LED: 현재 Mode 색 1Hz blink

### Bond 관리
- 최대 peer 3개 (`CONFIG_BT_NIMBLE_MAX_BONDS = 3`)
- NVS FIFO

### Host Cycle (disconnect-only)
- 트리거: Side C 3초 hold (단독)
- `_activePeerIndex` 순환 → 모든 connection disconnect → 재광고
- 재연결 윈도우 10초
- NimBLE 2.5.1 제약으로 whitelist 필터 없음 → OS 자동 재연결 위임

### Composite HID
- Report 1: Mouse (5버튼 + X/Y + Wheel + AC Pan)
- Report 2: Keyboard (mod + 6KRO)
- Report 3: Consumer (16-bit)

---

## 🔋 전원 관리 (P20)

### 설정 구조체 (`ST_C10_PowerConfig_t`)

| 필드 | 기본값 | 검증 범위 | 설명 |
|---|---|---|---|
| `idle_timeout_ms[0]` | 60000 | 5000~3600000 | Mode 1 (PC) 유휴 슬립 대기 (ms) |
| `idle_timeout_ms[1]` | 120000 | 5000~3600000 | Mode 2 (PPT) 유휴 슬립 대기 (ms) |
| `idle_timeout_ms[2]` | 300000 | 5000~3600000 | Mode 3 (TV) 유휴 슬립 대기 (ms) |
| `idle_timeout_ble_ms` | 300000 | 60000~3600000 | BLE 연결 중 유휴 슬립 대기 (ms) |
| `pairing_idle_timeout_ms` | 30000 | 10000~60000 | Pairing 모드 중 유휴 슬립 대기 (ms) |
| `deep_idle_timeout_ms` | 600000 | 0 (비활성) 또는 300000~7200000 | Deep-sleep 대기 (ms) |
| `wake_min_active_ms` | 500 | 100~2000 | Wake 후 최소 활성 시간 (thrashing 방지) |
| `wom_threshold` | 25 | 5~100 (32mg/LSB) | MPU6050 WoM 가속도 임계 (25 = 800mg) |
| `wom_duration` | 4 | 1~50 (ms) | WoM 모션 지속 판정 시간 |
| `fast_recalib_ms` | 300 | 100~1000 | Wake 후 Bias Fast Recalib 수집 시간 |
| `led_fadeout_ms` | 500 | 0~1000 | 슬립 진입 시 LED Fadeout |
| `led_fadein_ms`  | 300 | 0~1000 | Wake 복귀 시 LED Fadein |

### 1. Light-sleep 조건 (모두 만족 시 진입)
- 유휴 시간이 Mode/BLE/Pairing별 `idle_timeout_ms` 초과
- SafeMode 아님 (`!isSafeMode()`)
- OTA Guard 아님 (`!isOtaGuard()`)
- Move Gate 미누름 (`!_moveGateHeld`)
- Front Hold 미누름 (`!_frontHoldActive`)
- Action 실행 큐 비어있음 (`_qActionExec` isEmpty) + 매크로 미실행

### 2. Light-sleep 진입 및 복귀 시퀀스
```
[유휴 판정] ──> LED suspend(snap): blocking fadeout(RED, led_fadeout_ms) ──> OFF
                                                                             │
[Wake 이벤트: WoM 또는 버튼] <── ESP32 Light-Sleep (esp_light_sleep_start) <──┘
           │
           ├──> LED resume(snap): async fadein(base, led_fadein_ms)  [FADEIN → IDLE]
           ├──> M20 BiasTracker startFastRecalibrate(fast_recalib_ms=300)
           ├──> Wake Debounce (wake_min_active_ms=500 백오프)
           └──> commTask 지연 통지 (_onPowerWake → xTaskNotifyGive)

```

### 3. Deep-sleep 조건 및 동작
- `deep_idle_timeout_ms > 0` 이며 누적 유휴 시간이 도달
- MPU6050 슬립 모드 전환 및 WS2812 LED 완전 차단
- ESP32 RTC 딥슬립 (`esp_deep_sleep_start()`) 진입 → Wake 시 리셋 부팅

### 4. MPU6050 Motion Interrupt (WoM) 사양
- **가속도 임계 (`MOT_THR`)**: 25 (32mg/LSB 단위 → 25 × 32mg = 800mg = 0.8g)
- **지속 카운터 (`MOT_DUR`)**: 4ms (1ms 단위)
- **인터럽트 핀 설정**: LATCH_EN = 1, INT_OPEN = 1, INT_LEVEL = 1 (Active-LOW Open Drain)
- **저전력 주기**: PWR_MGMT_1 = 0x20 (CYCLE), PWR_MGMT_2 = 0xC7 (LP_WAKE 5Hz)

### 5. Wake 소스 (EXT1, LOW Active)
- MPU6050 INT (GPIO 6)
- 버튼 6종: Side R (GPIO 7) + Top L (GPIO 12) + Side C (GPIO 13) + Side F (GPIO 14) + Top R (GPIO 15) + Top M (GPIO 16)

---

## 🛡️ SafeBoot / OTA Guard

### Boot State (`/json/boot_state_0410.json`)

```json
{
  "safe_mode": false,
  "fail_count": 0,
  "pending": false,
  "boot_ms": 0,
  "last_reset_reason": 0
}
```

### 절차
1. `begin()` → boot state 로드
2. 이전 `pending=true` + bad reset reason → `fail_count++`
3. `fail_count >= 2` → `safe_mode=true`
4. `pending=true` 마킹
5. `bootMarkOkIfGracePassed(8500ms)` → `pending=false`
6. 실패 시 30초 백오프

### Bad Reset Reason
`PANIC` / `INT_WDT` / `TASK_WDT` / `WDT` / `BROWNOUT`

### SafeMode 정책 (method-aware, M-2)

**허용 API (GET/POST)**:
- `/api/status`, `/api/diag`, `/api/diag/clear`, `/api/keycodes`
- `/api/safeboot`, `/api/ota`, `/api/ota/status`
- `/api/factory_reset`, `/api/reboot`, `/api/reboot/check`
- `GET /api/config`, `/api/config/export`, `/api/export`
- `/api/config/rollback`

**허용 API (GET만, method 검사)**:
- `/api/profiles/active` — GET만 허용, POST 차단
- `/api/profiles` — GET만 허용
- `/api/triggers` — GET만

**차단 API**:
- `/api/config/save`, `/api/config/apply`, `/api/config/import`
- `/api/control`, `/api/ppt/*`
- `/api/profiles/switch`, `/api/profiles/create`, `/api/profiles/delete`, `/api/profiles/rename`
- `/api/action/test`, `/api/action/test_macro`

### OTA Guard
- `/api/control {cmd:set_ota_guard, enable}` → HID 차단 + OTA 업로드 거부
- Stale 회수: `_otaStartedMs` 30초 경과 시 강제 해제
- 웹 UI: OTA 탭의 Guard 스위치 (`otaGuardManual`)

### WiFi 브릭 방지
- `mode=STA` + 연결 실패 시 **AP fallback**

---

## 📝 Config & Profile System (Schema v5, `G_C10_CFG_VER = 410`)

### LittleFS 저장 구조

```
/json/
├── active_profile.json           # {active_index, profile_count}
├── active_profile.json.tmp
├── boot_state_0410.json
├── boot_state_0410.json.tmp
├── profiles/
│   ├── profile_0.json
│   ├── profile_0.json.tmp
│   ├── profile_0.json.old
│   └── profile_N.json            # 최대 5개 (0~4)
└── public/
    └── manifest_0410.json
```

### 프로파일 본체 스키마 (`profile_N.json`)

```json
{
  "ver": 410,
  "name": "Default",
  "wifi": {
    "mode": 0,
    "sta":  { "ssid": "", "pass": "" },
    "ap":   { "ssid": "EliteAirMouse", "pass": "12345678" },
    "mdns": { "host": "elite-airmouse" }
  },
  "e10": {
    /* DPI, 감도, 제스처, precision, bias, ... */
    "motion_adv": {
      "click_freeze": { "enable": true, "gyro_th": 15.0, "max_ms": 150, "hold_ms": 20, "fadeout_ms": 30, "move_th": 2.0, "freeze_move_th": 30.0 },
      "ema": { "alpha_min": 0.05, "alpha_max": 0.80, "deadzone_th": 3.0, "fast_th": 15.0, "reversal_th": 8.0, "reversal_reset": true },
      "snap": { "enable": true, "mode_mask": 2, "axis_mode": 0, "confirm_frames": 3, "ratio_enter": 4.0, "strength": 0.85 }
    },
    "power": {
      "idle_timeout_ms":      [60000, 120000, 300000],
      "idle_timeout_ble_ms":  300000,
      "pairing_idle_timeout_ms": 30000,
      "deep_idle_timeout_ms": 600000,
      "wake_min_active_ms":   500,
      "wom_threshold":        25,
      "wom_duration":         4,
      "fast_recalib_ms":      300,
      "led_fadeout_ms":       500,
      "led_fadein_ms":        300
    },
    "button": {
      "debounce_press_ms":   32,
      "debounce_release_ms": 16,
      "long_delay_ms":       800,
      "double_delay_ms":     320,
      "hold_2s_ms":          2000,
      "hold_3s_ms":          3000,
      "min_click_ms":        16,
      "debounce_min_ticks":  3
    }
  },
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

### 검증 규칙
- `profile_count`: 1~5
- `slots.global`: 정확히 27개
- `slots.modes`: 정확히 3개 (각 mask + 27개 slots)
- 4대 잠금 (S1/S5/S12/S13): `G_C10_TRIG_LOCKED[]` 상수로 서버 강제
- `macros`: ≤8개, 각 ≤8 step, delay ≤2000ms
- `power` 범위 검증: `idle_timeout_ms[i]` (5000~3600000), `idle_timeout_ble_ms` (60000~3600000), `pairing_idle_timeout_ms` (10000~60000), `deep_idle_timeout_ms` (0 또는 300000~7200000), `wake_min_active_ms` (100~2000), `wom_threshold` (5~100), `wom_duration` (1~50), `fast_recalib_ms` (100~1000), `led_fadeout_ms`/`led_fadein_ms` (≤1000)
- `button` 범위 검증: `debounce_press_ms` (8~100), `debounce_release_ms` (8~100), `debounce_press_ms >= debounce_release_ms`, `long_delay_ms` (300~2000), `double_delay_ms` (150~800), `hold_2s_ms` (1000~5000), `hold_3s_ms` (2000~8000), `hold_2s_ms < hold_3s_ms`, `min_click_ms` (≤100), `debounce_min_ticks` (1~10)

---

## 🌐 Web API (v0410)

**API ver:** `G_W10_API_VER = 410`

| Method | Endpoint | 용도 | 프론트엔드 사용처 |
|---|---|---|---|
| GET | `/api/status` | 상태 스냅샷 | Dashboard, 자동 폴링 (2.5s) |
| GET | `/api/diag` | 진단 카운터 + 이벤트 | Diagnostics 탭 |
| POST | `/api/diag/clear` | 카운터 리셋 (N-3) | Diag 카운터 초기화 버튼 |
| GET | `/api/keycodes` | 키코드 + triggers_meta + slots_meta (N-14) | Slots/Macros 드롭다운, meta 라벨 |
| GET | `/api/triggers` | 27개 트리거 목록 | Slot Editor |
| GET | `/api/profiles` | 프로파일 목록 | Profile Bar |
| GET | `/api/profiles/active` | 활성 프로파일 본체 | Slot/Macro/Config 로드 |
| POST | `/api/profiles/active` | 활성 프로파일 패치 | Slot/Macro/Config 저장 |
| POST | `/api/profiles/switch` | 프로파일 전환 | Profile Bar 드롭다운 (N-13 로딩) |
| POST | `/api/profiles/create` | 프로파일 생성 | + 새로 |
| POST | `/api/profiles/delete` | 프로파일 삭제 (N-21 경고) | 삭제 |
| POST | `/api/profiles/rename` | 이름 변경 | 이름 |
| POST | `/api/action/test` | Live Test | Slot Test, Host Cycle (N-8) |
| POST | `/api/action/test_macro` | 매크로 테스트 | Macro Test |
| POST | `/api/ppt/test` | 단발 키 테스트 (N-9) | Diag Key Test |
| GET | `/api/config/export` | 프로파일 다운로드 (N-10) | Config Export |
| POST | `/api/config/import` | 프로파일 가져오기 | Config Import |
| POST | `/api/control` | Quick Control | PPT/DPI/Precision/SafeMode/OTA Guard/I2C/Gyro (N-2, N-4, N-7, N-18, N-19) |
| POST | `/api/ota` | OTA 업로드 | OTA 탭 |
| GET | `/api/ota/status` | OTA 상태 | OTA 탭 |
| GET | `/api/safeboot` | SafeBoot 상태 | Dashboard |
| POST | `/api/safeboot {exit:true}` | SafeMode 해제 (N-1) | SafeBoot 해제 |
| POST | `/api/factory_reset` | 공장 초기화 (N-20 진행 표시) | Dashboard |
| POST | `/api/reboot` | 재부팅 | 헤더/배너 |
| GET | `/api/reboot/check` | 재부팅 필요 확인 (N-6) | 배너 tooltip |


### `/api/status` 응답 확장 (C-02)

`config` 객체에 다음 3필드 포함:

```json
{
  "groups": {
    "config": {
      "ver": 410,
      "etag_ok": true,
      "etag": 12345,
      "size": 8042,
      "profile_idx": 0,
      "profile_count": 1,
      "profile_name": "Default",
      "last_apply_ok": true,
      "last_apply_ms": 12345,
      "last_apply_age_ms": 45678,
      "last_apply_code": "...",
      "last_apply_src": "..."
    }
  }
}
```

프론트엔드 `refreshStatus()`:
```js
qs("stProfile").textContent =
  `#${cfg.profile_idx ?? "-"} ${cfg.profile_name ?? ""} / 총 ${cfg.profile_count ?? "-"}개`;
```

---

## 🖥️ 프론트엔드 (Web UI 3-View + Phase 1~4 확장)

### 파일 구조 (7-모듈)

| 파일 | 역할 | 로드 순서 |
|---|---|---|
| `index_0411.html` | 메인 UI | – |
| `style_0411.css` | 스타일 (+ 로딩/배너/Quick Tuning) | – |
| `am_base_0411.js` | 상수/유틸/API/전역/로딩 헬퍼 | 1 |
| `am_offline_0411.js` | 오프라인 시뮬레이터 (schema v5) | 2 |
| `am_profile_0411.js` | 프로파일/Slots/ActionEditor | 3 |
| `am_macro_0411.js` | 매크로 편집기 | 4 |
| `am_config_0411.js` | E10 Config/Control/SafeBoot/Power/Button | 5 |
| `am_status_0411.js` | Status/Diag/OTA/배너/Quick Tuning | 6 |
| `app_0411_0001.js` | UI 바인딩 + Main | 7 |

### 탭 구성

1. **상단 배너** (N-5): 재부팅 필요 시 조건부 표시
2. **Profile Bar**: 드롭다운 / + 새로 / 이름 / 삭제 / 재로드
3. **Dashboard**:
   - Status (Uptime/Heap/WiFi/Profile)
   - Quick Control (PPT, 자이로, 강제 릴리즈, I2C)
   - **Quick Tuning** (N-7): DPI 1/2/3, Precision OFF/LOW/MED/HIGH/PPT
   - **SafeMode / Host Cycle** (N-4, N-8)
   - **SafeBoot 해제** (N-1), Factory Reset (N-20)
4. **Slots 탭**:
   - Global / Mode 1·2·3 뷰 토글
   - 27개 트리거 (Button/Flick/Linear/Tilt)
   - **Tilt 그룹 Mode 3 제한** (Mode 1/2 뷰에서 회색 + 경고 배너)
   - **slots_meta 부가 라벨** (N-14) — `(TOP_L / CLICK)` 형태
   - 🔒 잠금 트리거 표시
5. **Macros 탭**: 8×8 편집기, Key 드롭다운 (usage 숫자 대신 이름)
6. **Config 탭**:
   - **E10 Core & Arrays**: DPI, Click-Lock, LED 밝기, Accel Gain, Scale Base, Wheel, Flick
   - **Motion Advanced (Phase 11.5)**: Click-Freeze / Adaptive EMA / Snap-to-Axis
   - **Power Management (Phase 11.6)**: Idle Timeout(Mode별), BLE Idle, Pairing Idle, Deep Idle, Wake Min Active, WoM Threshold, WoM Duration, Fast Recalib, LED Fadeout/Fadein
   - **Button Timings (Phase 11.7)**: Debounce Press/Release, Long Delay, Double Delay, Hold 2s/3s, Min Click, Min Ticks
   - **프리셋 5종**: PC / PPT / TV / Gaming / Precision 즉시 적용 및 저장
   - Export / Import / Raw JSON 에디터
7. **Diagnostics 탭**:
   - 카운터 + 이벤트
   - **카운터 초기화** (N-3)
   - **Key Test** (N-9): KB/Consumer 단발 테스트
8. **OTA 탭**:
   - **OTA Guard 스위치** (N-2)
   - 파일 업로드

### UI 개선 (Phase 1~4)

| 요소 | 이슈 | 설명 |
|---|---|---|
| KB usage 드롭다운 | – | 그룹화 (알파벳/숫자/기본/펑션/네비/기타) |
| 로딩 오버레이 | N-13 | 프로파일 전환/삭제/캘리브/Factory 시 표시 |
| 재부팅 배너 | N-5 | `policy.reboot_required` 감지 |
| Quick Tuning | N-7 | apply-only (저장 X), 클릭 시 파란 하이라이트 |
| SafeBoot 해제 | N-1 | 재부팅 + 자동 재접속 폴링 |
| OTA Guard | N-2 | 수동 토글 + 자동 OFF 감지 |
| Key Test | N-9 | Diag 탭 내 즉시 키 전송 |

### 캐시 정책

| 대상 | Cache-Control |
|---|---|
| HTML, `/json/public/*`, `/api/*` | `no-store` |
| 버전 토큰 포함 정적 (`_NNNN`) | `immutable` |
| 나머지 정적 | `short` (1h) |

**파일명 규칙**: `_NNNN` 접미사가 "원본 확장자 바로 앞"에 위치해야 immutable. 예: `am_base_0411.js` ✅, `app_0410.003.js` ❌ (short).

### gzip

- html/css/js → `.gz`만
- `pio_gzip_0410.py`가 buildfs 시 생성

---

## 🚀 빌드

### platformio.ini (주요)

```ini
[platformio]
default_envs    = esp32-s3-zero
build_cache_dir = .pio/cache
data_dir        = ./src/v0410/data_v0410

[ESP32_common]
platform                = espressif32
framework               = arduino
board_build.filesystem  = littlefs
monitor_speed           = 115200

src_filter =
    +<main.cpp>
    +<v0410/>
    -<v0400/> -<v032/> -<v001/> -<v010/>

lib_deps =
    bblanchon/ArduinoJson @ ^7.4.3
    h2zero/NimBLE-Arduino @ ^2.5.1
    https://github.com/Mystfit/ESP32-BLE-CompositeHID.git
    adafruit/Adafruit MPU6050@^2.2.9
    esp32async/ESPAsyncWebServer @ ^3.12.1
    adafruit/Adafruit NeoPixel @ ^1.12.3

build_flags =
    -std=gnu++17
    -D CONFIG_BT_NIMBLE_ENABLED=1
    -D CONFIG_BT_BLE_ENABLED=1

[env:esp32-s3-zero]
board                   = esp32-s3-devkitc-1
board_build.mcu         = esp32s3
board_build.f_cpu       = 240000000L
board_upload.flash_size = 4MB
board_build.partitions  = default_4MB.csv

extra_scripts = pre:src/v0410/tools_v0410/pio_gzip_0410.py
```

### 리소스 (v0410 최종)

| 항목 | 값 |
|---|---|
| RAM | ~40% (131 KB / 320 KB) |
| Flash | ~45% (1.42 MB / 3.14 MB) |

---

## 🗂️ 리팩터 이력

### Phase 1~4 (E10 크리티컬)
- C-1 ~ C-5, H-2 ~ H-4, M-2, M-4

### Phase 5 (Mode + 슬롯 매트릭스)
- 3-Mode 시스템, Action Registry

### Phase 6-J (LED)
- WS2812 + `_ledTask`

### Phase 7 (제스처)
- M30 Flick P2P / Linear / Tilt Hold

### Phase 8 (Sleep)
- P20 Light-sleep + WoM (EXT1 wake)

### Phase 9~10 (BLE)
- Pairing Mode, Host Cycle, disconnect-only 재연결

### Front Hold 스크롤
- Side F Hold → 커서 감쇠 + 수직 휠 + 수평 팬

### Phase 11 (v0410 다중 프로파일 & 매크로 라이브러리)
- 다중 프로파일 시스템 (Schema v5)
- 2단 슬롯 매트릭스 (`_resolveSlot`)
- 매크로 라이브러리 (8×8, 비동기 시퀀서)
- 4대 필수 슬롯 잠금
- Web UI 3-View 통합

### Phase 11 후속 (프론트엔드 안정성 + 누락 기능)

**Critical/High (C-01 ~ C-04)**
- C-01: `renderActionEditor` `const` 재할당 → `let` (오프라인 폴백)
- C-02: `/api/status config`에 `profile_idx/name/count` 추가
- C-03: `getSlotsArray()` 배열 27 강제
- C-04: `profileSwitch` 재진입 방지 (busy 플래그)

**Medium/Low (C-05 ~ C-07)**
- C-05: 오프라인 `G_OFFLINE_KEYCODES` 확장 (directions/groups/slots_meta)
- C-06: `_kbUsageLabel` "None (없음)" 처리
- C-07: `setInterval` 오버랩 방지

**누락 기능 (N-1 ~ N-21)**
- Phase 1: SafeBoot 해제(N-1), OTA Guard(N-2), Diag 초기화(N-3)
- Phase 2: SafeMode 토글(N-4), 재부팅 배너(N-5), DPI/Precision(N-7), Host Cycle(N-8), I2C 피드백(N-18)
- Phase 3: reboot check(N-6), Key Test(N-9), 서버 Export(N-10), Rollback 안내(N-11), 캘리브 피드백(N-19), Factory 진행(N-20)
- Phase 4: 로딩 오버레이(N-13), slots_meta 라벨(N-14), reboot 폴링(N-16), Export 별칭(N-17), 삭제 경고(N-21)

**백엔드 크리티컬 수정 (C-1 ~ C-4, H-1 ~ H-4)**
- C-1: Top M CLICK 슬롯(S4) 도달
- C-2: `switchProfile` HID 큐 경유
- C-3: 매크로 블로킹 제거 (상태머신)
- C-4: `_resolveSlot` 락 보호
- H-1: `_macroAbortToken` 카운터
- H-2: `switchProfile` 큐 드레인
- H-3: 리셋 플래그 위임
- H-4: `_startMacro` 스냅샷
- M-1: `_pairing` volatile
- M-2: SafeMode method-aware 게이트
- D-1: `biasTracker.reset()` sensorTask 위임
- D-2: 캘리브 루프 `_ble.tick()`
- D-3: `clearDiagnostics` 플래그만 설정

### Phase 11.5 (포인팅 정밀도 & 모션 알고리즘 고도화, v0411)

**백엔드 확장 (C10/M10/E10)**
- `C10_Def_0410.h`: `ST_C10_MotionAdv_ClickFreeze_t` / `ST_C10_MotionAdv_Ema_t` / `ST_C10_MotionAdv_Snap_t` 3개 struct + `ST_C10_MotionAdv_t` 통합
- `C10_Config_0410.cpp`: `makeDefaultsE10`, `validateE10`, `_buildE10Json`, `_patchE10Json` 확장
- `M10_MotionProc_0410.h`: **Adaptive EMA 전면 재설계**
  - 기존 이진 LPF 제거
  - `_computeEmaAlpha` (Hermite Smoothstep)
  - `_prevSignX_neg` / `_prevSignY_neg` + Reversal Reset
  - `setEmaConfig()` / `resetEmaState()` 신규
- `E10_AirMouse_0410.h`: `EN_E10_FreezeState_t` (4-state) / `EN_E10_SnapAxis_t` (3-state) + 상태 멤버 + `_btnLDown` volatile
- `E10_AirMouse_Motion_0410.cpp`: `_applyClickFreeze` + `_applySnapToAxis` 신규
- `E10_AirMouse_Task_0410.cpp`: 파이프라인 10단계 (NaN → Bias → EMA → ZeroSnap/Sigmoid → DPI → Front Hold → Move Gate → Precision → Snap → Freeze)
- `E10_AirMouse_Core_0410.cpp`: EMA config 주입 + `motion_adv` 스냅샷 백업/복원
- `E10_AirMouse_Action_0410.cpp`: Top L DOWN/UP 시 `_btnLDown` 갱신 + `_setActiveMode`에 `_resetSnapState()`

**프론트엔드 확장 (Config 탭)**
- `index_0411.html`: Motion Advanced 3섹션 (Click-Freeze / EMA / Snap, 총 19 필드) + 프리셋 버튼 5종
- `am_config_0411.js`: `configToUi` / `uiToConfig` 확장 + `applyMotionPreset` + `MOTION_PRESETS`
- `am_offline_0411.js`: `motion_adv` 기본값 + `OFFLINE_STORE_SCHEMA` 상향
- `app_0411_0001.js`: 프리셋 버튼 5개 바인딩

**특징**
- 3기능 모두 **단방향 파이프라인** 내 순차 적용
- Click-Freeze는 **최종 게이트** (HID 전송 직전)
- 추정값 기반 구현 → Phase 0 실측 후 튜닝

### Phase 11.6 (P20 전원 관리 고도화: Light/Deep Sleep & WoM)

**백엔드 확장 (C10/P20/L10/M20/E10)**
- `C10_Def_0410.h`: `ST_C10_PowerConfig_t` 추가
  - 실제 필드: `idle_timeout_ms[3]`, `idle_timeout_ble_ms`, `pairing_idle_timeout_ms`, `deep_idle_timeout_ms`, `wake_min_active_ms`, `wom_threshold`, `wom_duration`, `fast_recalib_ms`, `led_fadeout_ms`, `led_fadein_ms`
  - 기본값: `{60000,120000,300000}` / 300000 / 30000 / 600000 / 500 / 25 / **4** / 300 / **500** / **300**
  - (초안의 `idle_sleep_sec`/`deep_sleep_sec`/`led_fade_ms`/`wake_debounce_ms` 명칭은 폐기)
- `C10_Config_0410.cpp`: `makeDefaultsE10`, `validateE10`, `_buildE10Json`, `_patchE10Json`에 `power` 객체 반영
- `P20_Power_0410.h/.cpp`:
  - `setConfig()` 동적 반영
  - `sleepNow()`: MPU6050 WoM 설정 (32mg/LSB 물리 단위, 25 = 800mg), EXT1 Wake 소스 등록, 웨이크업 안정화 백오프 (`wake_min_active_ms=500`)
  - `deepSleepNow()`: 센서 및 LED 전원 차단 후 RTC Deep Sleep 진입
  - `_onPowerWake` 콜백을 통한 복귀 이벤트 알림
- `L10_Led_0410.h/.cpp`: `EN_L10_ST_FADEIN`, `fadein()`, `suspend()`, `resume()` 추가 (슬립 전후 LED 무결성 보장 및 실패 시 복원)
- `M20_BiasTracker_0410.h`: `startFastRecalibrate(300ms)` 추가 (슬립 후 센서 온도 변화에 따른 Gyro 드리프트 급속 재수집)
- `E10_AirMouse_Task_0410.cpp`: sensorTask 슬립 평가 및 LED suspend/resume 연동, commTask 지연 통지(`_powerNotifyPending`)
- `E10_AirMouse_Core_0410.cpp`: `_applyE10ToRuntime()`, `_snapshotRuntimeToE10Config()` 동기화

### Phase 11.7 (C20 버튼 디바운스 및 타이밍 고도화)

**백엔드 확장 (C10/C20/E10)**
- `C10_Def_0410.h`: `ST_C10_ButtonConfig_t` 추가
  - 실제 필드: `debounce_press_ms`, `debounce_release_ms`, `long_delay_ms`, `double_delay_ms`, `hold_2s_ms`, `hold_3s_ms`, `min_click_ms`, `debounce_min_ticks`
  - (초안의 `click_ms`/`dblclick_gap_ms`/`long_press_ms` 명칭은 폐기)
  - 기본값: Press=**32**, Release=**16**, Long=**800**, Double=**320**, Hold2s=2000, Hold3s=3000, MinClick=**16**, MinTicks=3
  - 검증: `press >= release`, `hold_2s < hold_3s` 강제
- `C10_Config_0410.cpp`: `makeDefaultsE10`, `validateE10`, `_buildE10Json`, `_patchE10Json`에 `button` 객체 반영
- `C20_BtnDispatcher_0410.h/.cpp`:
  - 비대칭 디바운스: **Press 32ms (접촉 안정화) / Release 16ms (지연 최소화)**
  - 2-Stage 하이브리드 판정: 경과 시간 AND `stableCount >= debounce_min_ticks`(기본 3)
  - 최초 접점 시점 보존(`rawDownMs`): 바운스 중 타임스탬프 덮어쓰기 방지
  - 초단 글리치 필터링: 누름 지속 시간 < `min_click_ms` (기본 16ms) 시 이벤트 폐기 (`DISCARD`)
  - 동적 적용: `setTimings()` 구현
- `E10_AirMouse_Core_0410.cpp`: `_applyE10ToRuntime()`에서 `_btnDisp.setTimings()` 즉시 연동

**프론트엔드 연동 (Config 탭 & Offline)**
- `index_0411.html`: Power Management (12개 필드) 및 Button Timings (8개 필드) 폼 추가
- `am_config_0411.js`: `configToUi` / `uiToConfig`에 `power` 및 `button` 양방향 매핑
- `am_offline_0411.js`: 기본 프로파일에 `power` / `button` 기본값 등록 및 `OFFLINE_STORE_SCHEMA = 5` 상향

### 계약 문서 정합성 보완 (rev1, 2026-10-01)
- **SPEC.md**: Power/Button 표·JSON 스키마·기본값 실제 코드와 일치, Host Cycle 트리거 "Side C 3초 단독", 파이프라인 10단계로 갱신, `G_BOOT_GRACE_MS` 출처 명시
- **CONTRACT.md**: `_startMacro` 대체 정책 명시, `_topMDownMs` Dead Code 표기, SPEC 참조 `_002.md` 갱신
- **STATE.md**: Macro Sequencer FSM "개념적 서술" 경고문 + §2.5 대체/취소 정책 표 신설, `_topMDownMs` 소유권 표 추가
- **FLOW.md**: 매크로 대체 분기 추가, Front Hold/Move Gate 파이프라인 반영
- **BUDGET.md**: main loop `main.cpp 확인 필요` 표기, `_runGyroCalibration` 트리거 조건 명확화


### 계약 문서 정합성 보완 (rev2, 2026-10-01)
- **main.cpp 검증(N-1~N-4)**:
  - N-1: Boot Factory Reset 트리거 문서화 (Side C 6초 hold)
  - N-2: SafeMode 조건부 실행 (`setSafeMode` 재호출 최소화)
  - N-3: `E10_W10Apply` vs `applyRuntimeE10` 이중 경로 정책 명시
  - N-4: 로그 prefix `[0274]` → `[0410]`
- **상수 통일**: `G_BTN_MODE` 제거 → `E10_CONST::PIN_BTN_MODE` 참조

### 레거시 Hook 제거 (rev3, 2026-10-01)
- **`E10_W10Apply` 완전 삭제**: `W10`의 유일한 E10 진입은 `_e10if` (`ST_W10_E10If_t` 함수 포인터 테이블)
- **`W10.begin()` 시그니처 축소**: `begin(cfg, applyFn, applyCtx, e10if)` → `begin(cfg, e10if)` (2인자)
- **제거 심볼**:
  - `CL_E10_EliteAirMouse::E10_W10Apply` (선언 + 정의)
  - `CL_W10_WebConfig::_applyFn` / `_applyCtx` (멤버 변수)
- **main.cpp 호출부**: `g_w10.begin(&g_cfg, &g_w10E10If);`
- **W10 소스 영향**: `W10_Web_0410.h` / `W10_Web_init_0410.cpp` / `main.cpp` / `E10_AirMouse_0410.h` / `E10_AirMouse_Core_0410.cpp` (5개 파일)

### Dead Code 정리 (rev4, 2026-10-02)
- **`_topMDownMs` 제거**: v0410 C-1 수정으로 Mode 3 클릭 판정이 슬롯 매핑으로 이관된 후 잔존한 미사용 변수
- **`apiGetPpt` / `apiPostPpt` 제거**: `/api/ppt` GET/POST는 라우팅 미등록 상태로 스텁만 존재 → 완전 삭제
- `/api/ppt/test`는 유지 (Live Test 용도)

### Dead Code 정리 + SPEC 실구현 (rev4, 2026-10-02)
- **Dead Code 제거 (A 카테고리, A-5 제외)**:
  - A-1: `M10::updateOrientation` 단일축 overload 삭제
  - A-2: SPECIAL 콜백 체인 전체 삭제 (`setSpecialCallback` / `_specialCb` / `_specialCtx` / `_execSpecial` / `_onSpecial`)
  - A-3: W10 미사용 메서드 3종 삭제 (`_wantsEnvelope`, `_resPrintJsonString` ×2, `_addEtagHeadersNoStore`)
  - A-4: E10 `isSafeMode()` public getter 삭제
  - A-6: B20 `clearAllBonds` / `getConnectedCount` / `isWhitelistActive` 삭제
  - A-7: C10 `duplicateProfile` / `getActiveProfile` 삭제
  - A-8: P20 `setIdleTimeout` / `isIdle` / 3개 getter 삭제
  - A-9: L10 `stopBlink` 삭제
- **B 카테고리 (SPEC 실구현)**:
  - B-1: `L10::suspend()` → blocking RED fadeout (SPEC 준수)
  - B-2: `L10::resume()` → async base color fadein (SPEC 준수)
  - `L10::setFadeTimings()` 신규 + `E10::_applyE10ToRuntime`에서 프로파일값 주입
- **C 카테고리 (개선)**:
  - C-1: B20 `_whitelistActive` / `_whitelistUntilMs` → volatile
  - C-2: E10 `_thLed` 핸들 제거 (관측 미사용)
  - C-3: `main.cpp::_holdAtBoot` 1초마다 시리얼 `.` 피드백
  - C-4: `switchProfile` race window 주석 명시
  - C-5: `_reqSaveCfg` single-writer 원칙 주석
- **빌드 영향**: Flash −200 B / RAM −8 B (링커 DCE로 이미 최적화된 상태). 경고/에러 0건


---

## 📎 부록: 주요 상수

| 항목 | 값 | 비고 |
|---|---|---|
| `G_C10_CFG_VER` | 410 (Schema v5) | – |
| `G_W10_API_VER` | 410 | – |
| `G_W10_BODY_MAX` | 8192 | POST body 최대 |
| `G_W10_BODY_SLOTS` | 4 | 동시 body 슬롯 |
| `G_W10_DEFAULT_INDEX_PATH` | `/www/index_0411.html` | – |
| `SAFE_FAIL_THRESHOLD` | 2 | SafeBoot 임계 |
| `G_BOOT_GRACE_MS` | 8500 | (main.cpp 정의, `bootMarkOkIfGracePassed()` 인자로 전달. E10 헤더에는 미정의) |
| `CALIB_MS` | 1000 | – |
| `CALIB_STILL_TH` | 3.0 | – |
| `SPIKE_TH_DEG` | 650.0 | – |
| `ERR_HIST_CAP` | 16 | – |
| `_fadeoutMs` (L10) | 500 (기본), 프로파일 주입 | `power.led_fadeout_ms` |
| `_fadeinMs` (L10) | 300 (기본), 프로파일 주입 | `power.led_fadein_ms` |
| Sensor 주기 | 8ms (125Hz) | – |
| Comm 주기 | 7ms | – |
| LED tick 주기 | 50ms | – |
| `_qFrame` size | 1 (overwrite) | – |
| `_qHidCmd` size | 4 | – |
| `_qActionExec` size | 8 | – |
| `CONFIG_BT_NIMBLE_MAX_BONDS` | 3 | – |
| 오프라인 스키마 (`_v`) | 5 | – |

### Motion Advanced (Phase 11.5) 기본값

| 항목 | 값 |
|---|---|
| **Click-Freeze** | |
| `gyro_th` | 15.0 deg/s |
| `max_ms` | 150 ms |
| `hold_ms` | 20 ms |
| `fadeout_ms` | 30 ms |
| `move_th` | 2.0 px |
| `freeze_move_th` | 30.0 deg/s |
| **Adaptive EMA** | |
| `alpha_min` | 0.05 |
| `alpha_max` | 0.80 |
| `deadzone_th` | 3.0 rad/s |
| `fast_th` | 15.0 rad/s |
| `reversal_th` | 8.0 rad/s |
| `reversal_reset` | true |
| **Snap-to-Axis** | |
| `enable` | true |
| `mode_mask` | 0x02 (Mode 2만) |
| `axis_mode` | 0 (both) |
| `confirm_frames` | 3 |
| `ratio_enter` | 4.0 |
| `strength` | 0.85 |

### Power Management (Phase 11.6) 기본값

| 항목 | 값 |
|---|---|
| `idle_timeout_ms[0]` | 60000 ms (Mode 1) |
| `idle_timeout_ms[1]` | 120000 ms (Mode 2) |
| `idle_timeout_ms[2]` | 300000 ms (Mode 3) |
| `idle_timeout_ble_ms` | 300000 ms |
| `pairing_idle_timeout_ms` | 30000 ms |
| `deep_idle_timeout_ms` | 600000 ms |
| `wake_min_active_ms` | 500 ms |
| `wom_threshold` | 25 (800mg) |
| `wom_duration` | 4 ms |
| `fast_recalib_ms` | 300 ms |
| `led_fadeout_ms` | 500 ms |
| `led_fadein_ms` | 300 ms |

### Button Timings (Phase 11.7) 기본값

| 항목 | 값 |
|---|---|
| `debounce_press_ms` | 32 ms |
| `debounce_release_ms` | 16 ms |
| `long_delay_ms` | 800 ms |
| `double_delay_ms` | 320 ms |
| `hold_2s_ms` | 2000 ms |
| `hold_3s_ms` | 3000 ms |
| `min_click_ms` | 16 ms |
| `debounce_min_ticks` | 3 |

---

## 🎯 프로젝트 철학

AirMouse Elite S3는 **상용급 입력 디바이스 아키텍처**입니다.

- 태스크/상태 소유권 명확
- 락 정책 일관 (recursive, 짝맞춤)
- Special 액션 sensorTask 단독 실행
- 매크로 비동기 시퀀서 (블로킹 제로, 토큰 취소, 조용히 대체)
- 모션 파이프라인 단방향 10단계 (NaN → Bias → EMA → ZeroSnap → DPI → Front Hold → Move Gate → Precision → Snap → Freeze)
- LED `_ledTask` 단독 tick
- 브릭 방지 (SafeBoot / Atomic Config / OTA Guard / AP fallback)
- 관측성 (RMS / 스택 / dt / 오류 이력)
- 정책 일관성 (SafeMode API 게이트 method-aware, Action 큐 일원화)
- 프론트엔드 7-모듈 구조 (로드 순서 명확, 캐시 immutable)
- 오프라인 시뮬레이터 (localStorage 스키마 버전 관리)

**새 기능 추가 시 위 원칙 위반 여부를 먼저 판단.**
