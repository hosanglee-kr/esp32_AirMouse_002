# CONTRACT_0415.md — 모듈 경계 및 인터페이스 계약

> 대상 버전: `v0415` (ESP32-S3-Zero + MPU6050 AirMouse)  
> 위치: `src/v0415/docs_v0415/contract_doc/CONTRACT_0415.md`
> 최종 갱신: 2026-10-09 (rev8 — v0415 리팩터: A40 삭제, Consumer mask Descriptor SSOT, Safe/Pairing Wake Mask, BB-1 제스처 확장, `_macroState` lock, L10 mutex, `_bootWifi`, `setPptMode` 삭제, 스키마 411)

---

## 1. 개요 및 설계 원칙

본 문서는 `v0415` 펌웨어의 태스크(`sensorTask`, `commTask`, `ledTask`, `webTask`) 간 호출 경계, 통신 큐, 공유 자원 접근 규칙을 고정 정의합니다. 모든 신규 코드 작성 및 코드 리뷰는 본 계약의 준수 여부를 1차 기준으로 검증합니다.

### 4대 절대 금지 규칙
1. **Web Task의 HID 직접 접근 금지**: `_mouse`, `_keyboard` 인스턴스는 오직 `commTask`만 제어하며, Web/Sensor는 반드시 `_qHidCmd` 또는 `_qActionExec`를 경유해야 합니다.
2. **Special 액션의 Context 격리**: `EN_C20_ACT_SPECIAL`은 FreeRTOS 큐에 인큐하지 않고 **오직 `sensorTask` 컨텍스트에서만 동기 실행**합니다.
3. **LED 상태머신의 단독 Tick**: `_led.tick()`은 오직 `_ledTask`(50ms 주기)에서만 호출하며 타 태스크에서 tick을 호출하지 않습니다. **단, 상태 변경 API는 Recursive Mutex로 보호**되어 어느 태스크에서나 호출 가능합니다 (v0415).
4. **블로킹 금지**: 모든 큐 인큐(`_enqueueAction`, `_enqueueHidCmd`, `_pushFrame`)는 `timeout = 0` (non-blocking) 원칙을 유지합니다.

---

## 2. 함수 인터페이스 계약

| 함수 심볼 | 권한 소유 호출자 | 실행 컨텍스트 제한 | 주요 부수효과 | 실패/경합 시 처리 |
|---|---|---|---|---|
| `_startMacro(idx)` | `commTask` only | 논블로킹 (`_lock` 하 스냅샷 + state 원자 초기화) | `_macroSnapshot` 갱신, `_macroState` 활성화 (active 마지막 write) | ① 인덱스 범위 초과 시 `WARN` 로그 후 무시<br>② 이미 실행 중이면 `WARN` 로그 후 조용히 대체(replace) — 토큰 불일치 없이 덮어씀 |
| `_tickMacro()` | `commTask` only | 매 프레임 논블로킹 (lock 없음, step 실행만 블로킹) | 딜레이 경과 시 액션 실행, 스텝 전진 | 토큰 불일치(`_macroAbortToken`) 시 즉시 중단 + WARN 로그 |
| `_resolveSlot(mode, trig)` | `sensorTask`, `commTask`, `webTask` | Read-only (`_lock` 하 스냅샷) | 없음 (Global + Mode Override O(1) 해석) | 비정상 인덱스 시 `EN_C20_ACT_NONE` 반환 |
| `switchProfile(idx)` | `webTask` only | 진행 중 재진입 차단 (`_cfgProfileValid` + `_profileSwitchInProgress`) | `_lock` 하 `_macroAbortToken++`, 큐 드레인, 리셋 위임, HID release, `_engine.resetEmaState()` | 전환 중이면 `false` 반환 및 조기 종료 |
| `_handleSpecial(special)` | `sensorTask` only | 타 태스크 호출 금지 | 모드 변경, 페어링, 캘리브, 호스트 순환, **SLEEP_NOW (즉시 진입)** | 알 수 없는 코드 시 무시. **HOST_CYCLE 시 `_ble.exitPairing()` 선행 호출** |
| `forceReleaseButtons()` | `any` (web, sensor, comm) | 논블로킹 (`_lock` 하 매크로 abort + state 리셋) | `_macroAbortToken++` (lock 하) + `_qHidCmd`에 `RELEASE_ALL` 인큐 | **큐 Full 시 `_reqCommReleaseAll = true` 위임 + false 반환** |
| `forceReleaseAllButtons()` | – | – | **v0415 삭제** (Round G, 호출처 없음) | – |
| `_applyClickFreeze(...)` | `sensorTask` only | 파이프라인 최종단 | `_lock()` 하 config 스냅샷 read + `_freezeState` 전이 | `motion_adv.click_freeze` 로컬 복사 |
| `_applySnapToAxis(...)` | `sensorTask` only | Click-Freeze 직전 | `_lock()` 하 config 스냅샷 read + 활성 축 감쇠 | `motion_adv.snap` 로컬 복사 |
| `mpuWr(reg,val)` | `sensorTask` (`_prepareMpuWom`) | 논블로킹 I2C | `Wire.endTransmission()` 반환값 리턴 | WoM prep 실패 시 조기 반환 → sleep 금지 |
| `_recoverI2C()` | `sensorTask` only | I2C 버스 재초기화 | `Wire.end()`, `Wire.begin()`, `_mpu.begin()` | 실패 시 `_consecutiveRecoverFail++` |
| `_runGyroCalibration()` | `sensorTask` only | 초기 부팅 또는 정지 시 1000ms 측정 | 자이로 오프셋 산출, `_gyroCalibDone = true` | 측정 중 움직임 시 재시도. 루프 내 `_ble.tick()` 유지 |
| `_snapshotRuntimeToE10Config(out)` | `sensorTask`/`commTask`/`webTask` | `_lock()` 하 호출 필수 | 런타임 → config 구조체 반영 (**v0415: wipe 제거, 런타임 필드만 갱신**) | `motion_adv`/`power`/`button`/`gyro_bias`/`flick`/`tilt_hold`/`active_mode`/`led_brightness` 등은 호출자가 baseline 유지 |
| `_applyE10ToRuntime(e10)` | `sensorTask`/`webTask` | `_applyRuntimeLocked()` 경유 (락 보유) | 런타임 필드 일괄 반영 | 범위 클램프 다수. **`_engine.setHardClickLock` 호출 없음 (v0415)** |
| `_enqueueAction(slot, isDown)` | `sensorTask`, `webTask` | 논블로킹 | `_qActionExec` 인큐 | 큐 Full 시 drop (false) |
| `_enqueueHidCmd(cmd)` | `webTask`, `sensorTask` | 논블로킹 | `_qHidCmd` 인큐 | 큐 Full 시 drop (false) |
| `_pushFrame(fr)` | `sensorTask` only | 논블로킹 Overwrite | `_qFrame` 최신 프레임 유지 | size=1 Overwrite로 항상 성공 |
| `applyRuntimeE10(e10)` | `W10` (`/api/control` apply-only) | `_lock()` 하 `_applyRuntimeLocked` | E10 파라미터만 런타임 반영 (persist X) | 항상 true (범위 클램프 다수) |
| `execLiveTest(kind, h, p16, p32)` | `webTask` | SPECIAL은 위임, 그 외 큐 경유 | `_reqSpecialAction` 또는 `_qActionExec` | SafeMode/OTA 차단 시 false |
| `_led.suspend(snap)` | `sensorTask` only | **blocking** (≤ `led_fadeout_ms + 200ms`) | 스냅샷 백업 + RED fadeout → OFF | sleep 진입 직전. 실패 시 `resume`으로 원복 |
| `_led.resume(snap)` | `sensorTask` only | **async** (즉시 리턴) | FADEIN 상태 진입 → `_ledTask`가 IDLE 전이 | sleep 복귀 직후 |
| `_led.setFadeTimings(fo, fi)` | `webTask` (`_applyE10ToRuntime`) | 논블로킹 (`_lock` 하) | fade 타이밍 주입 (0 → 1ms 방어) | – |
| `_led.setModeColor/flash/blink/fadeout/fadein/off` | `any` | 논블로킹 (`_lock` 하) | **v0415: Recursive Mutex 보호** | – |
| `sleepNow(now, safeOrPairing)` | `sensorTask` only | **blocking** (wake까지) | Safe/Pairing 마스크 분기 + MPU WoM + light sleep | `_prepareMpuWom` 실패 시 false |
| `deepSleepNow(now, hid, pairing, safe)` | `sensorTask` only | **blocking** (reboot까지) | Safe 분기 마스크 + deep sleep 진입 | `p_pairing` 시 false |
| `_bumpRebootMaskFromBoot()` (W10) | `webTask` | 논블로킹 | `_bootWifi` ↔ 현재 WiFi diff 재계산 | – |

### 2.1 하드웨어 핀 및 Wake Mask SSOT 계약 (`HW_Def_0415.h`)

- 모든 GPIO 핀 번호(`PIN_BTN_*`, `PIN_I2C_*`, `PIN_LED_*`, `PIN_BATT_ADC`) 및 EXT1 RTC Deep Sleep Wakeup 마스크는 `HW_Def_0415.h`의 `HW_DEF` 네임스페이스를 **단일 진실 공급원(SSOT)**으로 삼는다.
- 타 모듈(`C20`, `L10`, `E10`, `P20`, `main.cpp`)에서 GPIO 핀 번호를 로컬 매크로나 독립 상수로 중복 정의하는 행위는 엄격히 금지된다.
- RTC Deep Sleep 웨이크업 마스크는 다음 **3종 constexpr 함수**를 통해서만 획득:

| 함수 | 마스크 소스 | 사용 시나리오 |
|---|---|---|
| `HW_DEF::buildWakeMaskNormal()` | MPU INT + 6버튼 | Light-sleep 일반 모드 |
| `HW_DEF::buildWakeMaskSafe()` | Side C 단독 (GPIO 13) | Safe/Pairing (Light/Deep 모두) |
| `HW_DEF::buildWakeMaskButtons()` | 6버튼 (MPU 제외) | Deep-sleep 일반 모드 |

- `BTN_PINS[]` 순서는 `EN_C20_BtnId_t` 순서와 1:1 유지. `C20_BtnDispatcher::G_PINS`는 본 배열을 참조해야 한다.

### 2.2 Consumer Mask Descriptor SSOT 계약 (v0415 신규)

- HID Consumer Report의 비트 순서는 **ESP32-BLE-CompositeHID 라이브러리의 `_mediakeysHIDReportDescriptor`** (Report ID 0x43, 24-bit)가 **유일한 진실 공급원**이다.
- `EN_C20_Consumer_t` (C20_Action_0415.h)와 `G_W10_CONSUMER[]` (W10_Def_0415.h)는 **동일한 비트 체계**(Bit 0 = Play, Bit 11 = PlayPause, Bit 12 = Mute, Bit 13 = Vol+ 등)를 사용해야 한다.
- Consumer Report Count = 24 → **Bit 24 이상은 전송되지 않음**. `validateSlot`/`validateMacroStep`에서 `G_C20_CONSUMER_MASK_MAX = 0x00FFFFFFu` 상한 검증 필수.
- v0412 스키마 프로파일의 16-bit Consumer 값은 **`G_C20_LEGACY_410_TO_415[16]` 매핑 테이블**로 자동 변환 (`C10_Config_0415::_migrateProfileV410ToV411`).
- **POWER/TV_INPUT/CH_UP/CH_DOWN은 Descriptor 미지원** → 마이그레이션 시 `NONE (0)` 처리. Power는 Keyboard Page `EN_C20_KB_POWER (0x66)` 로 대체 가능.

### 2.3 순간(Momentary) 트리거 지속성 액션 단발화 계약 (BB-1, v0415 확장)

- 순간 이벤트(`EVT_CLICK`, `EVT_DBLCLICK`, `EVT_LONGPRESS`) **및 제스처 발동(Flick/Linear/Tilt)** 에 할당된 지속성 액션(`MOUSE_HOLD`, `KB_REPEAT`, `CONSUMER_REPEAT`)은 단발 액션(`MOUSE_CLICK`, `KB_TAP`, `CONSUMER_TAP`)으로 인플레이스(in-place) 변환된다.
- 변환은 namespace 정적 헬퍼 **`_sanitizeMomentarySlot(ST_C20_ActionSlot_t&)`** 에서 수행하며, 다음 두 경로에서 공통 사용:
  - `_handleSlotButton()` — 버튼 CLICK/DOUBLE/LONG
  - `_handleGesture()` — 제스처 발동 (v0415 추가)
- 정제된 액션은 `isDown = true`로 1회만 `_enqueueAction()`되며, 짝 맞춤 UP 이벤트가 발생하지 않는 순간 이벤트 특성상 키/버튼이 영구 홀드(Stuck) 상태에 빠지는 현상을 원천 방지한다.
- **v0412 문제**: `_handleGesture`에 sanitization 미적용 → Tilt Hold를 KB_REPEAT로 매핑 시 키 stuck. **v0415 해결**: 공통 헬퍼 확장.

### 2.4 Side F 이벤트 소비 및 Front Hold 스크롤 계약 (BB-6)

- `BTN_SIDE_F`의 `EVT_DOWN` 및 `EVT_UP` 이벤트는 `_frontHoldActive` 플래그를 토글한 후 즉시 `return true`로 이벤트를 완전 소비한다.
- 이를 통해 Front Hold 조작 중 의도치 않은 슬롯 액션 디스패치를 방지하며, Side F의 `CLICK` 및 `LONGPRESS`만 등록된 슬롯 매핑(S9/S10)으로 정상 전달된다.

### 2.5 Side C LONG 이벤트 계약

- `BTN_SIDE_C`의 `EVT_LONG` (800ms)은 **하드코딩 미처리 + 슬롯 매핑 미등록** → **명시적 무시** (v0415).
- 800ms EVT_LONG은 2000ms HOLD_2S(Pairing) 진입 과정에서 자연 발생하므로 사용자 체감 없음.
- `_handleHardcodedButton` 내부에서 `if (p_evt == EN_C20_EVT_LONG) return true;` 로 명시.

### 2.6 W10 → E10 호출 경로 계약

- `W10.begin(cfg, e10if)`의 유일한 E10 진입은 **`_e10if` (`ST_W10_E10If_t` 함수 포인터 테이블)**.
- **v0415: `ST_W10_E10If_t::setPptMode` 콜백 삭제**. PPT 판정은 `active_mode == 2` 로 일원화.
- **v0415: `W10_Web_0415.h`의 4-arg `begin()` 오버로드 삭제**. 2-arg(cfg, e10if) 단일 진입.
- `/api/config/apply`는 `/api/config/save` 별칭으로 통합.
- `applyRuntimeE10`은 `/api/control`의 apply-only 요청에서만 사용.
- **신규 기능 추가 시 `_e10if`에 함수 포인터를 등록하는 방식이 유일한 원칙**.

### 2.7 enqueue 실패 안전망

- 모든 큐 enqueue(`_enqueueHidCmd`, `_enqueueAction`)는 `timeout=0`으로 즉시 반환.
- **HID `RELEASE_ALL` 요청**은 큐 Full 시 반드시 `_reqCommReleaseAll` 위임 플래그를 set해야 한다.
- **이유**: HID stuck(키/버튼 눌림 고정)은 사용자 체감 치명적. 큐 상태와 무관하게 release 100% 보장 필요.
- 소비: `commTask` 루프 진입부에서 `_reqCommReleaseAll` 체크 → `_actExec.releaseAll()` + `_doReleaseAllButtons()`.
- `_enqueueAction`(액션 슬롯)은 drop 허용 (다음 프레임 재시도 가능).
- `_pushFrame`은 `size=1 Overwrite`이므로 실패 없음.
- `_reqSpecialAction` / `_reqResetBtnDisp` / `_reqResetGesture` / `_reqGyroCalib` / `_reqI2CRecover` / `_reqClearDiag` / `_reqSaveCfg` / `_reqCommReleaseAll`은 **위임 플래그** (volatile, 재시도 없이 다음 소비 시점에 처리).

### 2.8 웹 API (`/api/profiles/active`) 부분 패치 무결성 계약

- 웹 클라이언트가 매크로(`macros`) 또는 슬롯(`slots`)을 부분 패치(POST)할 때, 백엔드 `validateProfile()`는 모든 슬롯에 대해 매크로 인덱스 경계(`param32 < macroCount`)를 원자적으로 검증한다.
- 매크로 삭제/재정렬 시 클라이언트는 해당 매크로를 참조하던 슬롯을 `EN_C20_ACT_NONE (0)`으로 초기화하고, **반드시 `patch.macros`와 `patch.slots`를 단일 요청에 포함하여 원자적으로 전송**해야 한다.
- 위반 시 백엔드는 400 Bad Request (`validation_failed`)를 반환하고 프로파일 수정을 원천 거부한다.
- **v0415 추가**: Consumer kind의 `param32`는 `G_C20_CONSUMER_MASK_MAX (0x00FFFFFF)` 상한 검증. 초과 시 `validation_failed`.

### 2.9 매크로 step 블로킹 명확화 (v0415)

SPEC §"매크로 실행 (비동기 상태머신)"의 "블로킹 없음" 표현은 **delay에 한정**한다:

| 구간 | 블로킹 |
|---|:---:|
| `_startMacro` | ✗ (스냅샷만) |
| `_tickMacro` delay 대기 | ✗ (다음 tick까지 리턴) |
| **`_tickMacro` step 실행 (`_actExec.exec`)** | **✓ (~28ms)** |
| `_tickMacro` 종료 | ✗ |

- 매크로 8 step × 28ms = 최대 224ms 누적 블로킹 (분산 실행).
- 커서 프레임 소비는 각 step 사이에 계속됨.

---

## 3. FreeRTOS 큐 계약

| 큐 식별자 | Producer 태스크 | Consumer 태스크 | 용량(Size) | 타임아웃 | 오버플로 / 경합 정책 |
|---|---|---|---|---|---|
| `_qFrame` | `sensorTask` | `commTask` | 1 (`ST_E10_Frame_t`) | 0 (Overwrite) | `xQueueOverwrite`: 지연 없이 최신 모션 프레임 유지 |
| `_qActionExec` | `sensorTask`, `webTask` | `commTask` | 8 (`ST_ActionCmd_t`) | 0 | Full 시 즉시 drop (커서 지연 방지) |
| `_qHidCmd` | `webTask`, `sensorTask` | `commTask` | 4 (`ST_E10_HidCmd_t`) | 0 | Full 시 즉시 drop (`false` 리턴) |

### 큐 메시지 구조체 (참조)

```cpp
// _qFrame (Overwrite)
struct ST_E10_Frame_t {
    int16_t x, y, wheel, pan;
    uint8_t btn_mask;
    bool    updated;
};

// _qActionExec
struct ST_ActionCmd_t {
    ST_C20_ActionSlot_t slot;   // kind/holdMode/param16/param32
    bool                isDown;
};

// _qHidCmd — [v0415] holdMs 필드 삭제 (TEST_CLICK 삭제 이후 Dead)
struct ST_E10_HidCmd_t {
    uint8_t  cmd;    // RELEASE_ALL / TEST_PPT
    uint8_t  arg0;
    uint8_t  arg1;
    uint32_t code;
};
```

### 큐 드레인 상한 (commTask)

- `_qActionExec` 드레인: **프레임당 최대 4개** (v_drainCount). 다중 액션 시 최대 4 × 28 = 112ms 블로킹.
- `_qHidCmd` 드레인: 무제한 (일반적으로 0~1개).

---

## 4. 공유 변수 및 상태 동기화 계약

| 공유 변수명 | Writer | Reader | 동기화 메커니즘 | 불변식 |
|---|---|---|---|---|
| `_macroAbortToken` | `sensorTask`, `commTask`, `webTask` | `commTask` (`_tickMacro`) | `volatile uint32_t` | 단조 증가 카운터 |
| `_macroState.active` | `sensorTask`, `commTask`, `webTask` | `commTask`, `sensorTask` (power) | `volatile bool` + **`_lock()` (v0415)** | 취소/시작 시 lock 하 원자 갱신 |
| `_macroState.macroIdx/stepIdx/stepStartMs/startToken` | `commTask` (`_startMacro`, `_tickMacro`) | `commTask` | **`_lock()` 하 갱신 (v0415)** | startToken은 `_macroAbortToken` 스냅샷 |
| `_macroSnapshot` | `commTask` (`_startMacro`) | `commTask` (`_tickMacro`) | **`_lock()` 하 (v0415)** | 실행 시점 매크로 스냅샷 |
| `_reqResetBtnDisp` | `webTask` (`switchProfile`) | `sensorTask` | `volatile bool` | 위임 패턴 |
| `_reqResetGesture` | `webTask` (`switchProfile`) | `sensorTask` | `volatile bool` | 위임 패턴 |
| `_powerNotifyPending` | `sensorTask` (파이프라인) | `sensorTask` (전원 루틴) | `volatile bool` | 센서 루프 내 deferred |
| `_btnLDown` | `sensorTask` (Top L) | `sensorTask` (`_applyClickFreeze`) | `volatile bool` | 좌클릭 물리 눌림 |
| `_activeMode` | `sensorTask`, `webTask` | `sensorTask`, `commTask`, `webTask` | `volatile uint8_t` + `_lock()` | 1~3 범위 |
| `_moveGateHeld` | `sensorTask` (Top M), `webTask` (switchProfile) | `sensorTask` (모션) | `volatile bool` | Move Gate |
| `_frontHoldActive` | `sensorTask` (Side F) | `sensorTask` (스크롤) | `volatile bool` | Front Hold |
| `_cfgProfile` | `webTask` (write), `sensorTask` (init) | `sensorTask`, `commTask`, `webTask` | `_mutex` (Recursive Mutex) | 프로파일 변경/저장 시 `_lock()` |
| `_state` | `sensorTask`, **`commTask` (`_doForceReleaseNow`)**, `webTask` (setSafeMode 등) | `webTask` (`getStatus`) | `_mutex` (`pdMS_TO_TICKS(2)`) | 관측용. 타임아웃 시 miss 카운터 증가 |
| `_errHist`, `_spikes` | `sensorTask`, `commTask` | `webTask` | `_pushErr`, `_pushSpike` 내부 `_lock()` | 링버퍼 오버플로 방지 |
| `_reqSpecialAction` | `webTask` (`execLiveTest`) | `sensorTask` | `volatile uint8_t` | SPECIAL 위임. 최신 1개만 유효 |
| `_reqCommReleaseAll` | `webTask` (`switchProfile`), `any` (enqueue 실패) | `commTask` | `volatile bool` | release 100% 보장 |
| `_reqGyroCalib` | `webTask`/`any` | `sensorTask` | `volatile bool` | 캘리브 + biasTracker reset 위임 |
| `_reqI2CRecover` | `webTask`/`any` | `sensorTask` | `volatile bool` | I2C 복구 위임 |
| `_reqClearDiag` | `webTask` | `sensorTask` | `volatile bool` | 진단 초기화 위임 |
| `_reqSaveCfg` | `any` (BLE dirty) | `main loop` (`tickConfigSave`) | `volatile bool` | 프로파일 저장 |
| `_profileSwitchInProgress` | `webTask` (`switchProfile`) | `webTask`, `sensorTask` | `volatile bool` | 재진입 차단 |
| `_safeMode` / `_otaGuard` | `sensorTask`, `webTask` | `any` | `volatile bool` + `_lock()` | HID 실행 게이트 |
| `_pairing` / `_dirty` (B20) | `sensorTask` (`_ble.tick`), `webTask` (`enterPairing`) | `sensorTask`, `webTask` | `volatile bool` | Pairing 상태 |
| `_pairingStartMs` / `_pairingTimeoutMs` (B20) | `webTask` (`enterPairing`) | `sensorTask` (`tick`) | `volatile uint32_t` | Pairing 타임아웃 |
| `_whitelistActive` / `_whitelistUntilMs` (B20) | `webTask` (`reconnectToActivePeer`) | `sensorTask` (`tick`) | `volatile bool` / `volatile uint32_t` | 재연결 윈도우 추적 |
| **`_bootWifi`** (W10) | `CL_W10_WebConfig::begin()` | `_bumpRebootMaskFromBoot` | 독점 (No sync) | **v0415: 부팅 WiFi 스냅샷** |
| **`_thLed`** (E10) | `begin()` (task create) | `getStatus` (관측) | 읽기 전용 | **v0415: LED 태스크 핸들** |

---

## 5. 태스크 간 Special 액션 라우팅 계약

**핵심 규칙:** Special 액션(`EN_C20_ACT_SPECIAL`)은 **sensorTask에서만 실행**.

```
_handleSlotButton / _handleGesture
  ├─ kind == SPECIAL → _handleSpecial() 직접 호출 (sensorTask 컨텍스트)
  └─ kind != SPECIAL → _sanitizeMomentarySlot() → _enqueueAction() → commTask

execLiveTest (webTask)
  ├─ kind == SPECIAL → _reqSpecialAction = param16 (위임)
  └─ kind != SPECIAL → _enqueueAction() → commTask

CL_C20_ActionExec::exec()
  └─ EN_C20_ACT_SPECIAL case → 안전망으로 무시(drop)
```

### 5.1 SLEEP_NOW 계약 (v0415 Q4-a)

- `_handleSpecial(EN_C20_SP_SLEEP_NOW)`은 **sensorTask 컨텍스트에서 동기 실행**.
- 시퀀스: `_led.suspend(snap)` (blocking) → `_power.sleepNow(now, safeOrPairing)` (wake까지 blocking) → `_led.resume(snap)` (async) → `biasTracker.startFastRecalibrate(_cfgProfile.e10.power.fast_recalib_ms)`.
- **`fast_recalib_ms`는 E10 config 단독 SSOT** (P20 Config 필드 삭제, Round E).

---

## 6. 매크로 실행 계약 (비동기 상태머신)

매크로는 **commTask 블로킹 없이** 스텝 단위로 진행:

```
commTask 큐 드레인
  ├─ MACRO kind → _startMacro(p_idx)  ← _lock 하 스냅샷 + state 초기화, 즉시 리턴
  │                                    (실행 중이면 WARN 로그 + 조용히 대체)
  └─ 기타       → _actExec.exec()     ← 즉시 실행

commTask 매 루프 후반
  └─ _tickMacro()  ← delay 경과 시 다음 스텝 실행, 커서 프레임 소비 지속
```

### 6.1 `_startMacro` 원자화 (v0415)

```cpp
_lock();
if (!_cfgProfileValid) { _unlock(); return; }
if (p_idx >= _cfgProfile.macros.count) { _unlock(); return; }

_macroSnapshot = _cfgProfile.macros.macros[p_idx];

_macroState.macroIdx    = p_idx;
_macroState.stepIdx     = 0;
_macroState.stepStartMs = millis();
_macroState.startToken  = _macroAbortToken;
_macroState.active      = true;   // ← 마지막 write (재정렬 방지)
_unlock();
```

### 6.2 취소 정책 (v0415: `_lock` 하 원자화)

- `_macroAbortToken` 카운터가 `_macroState.startToken`과 다르면 즉시 abort (WARN 로그).
- **`switchProfile`, `_setActiveMode`, `forceReleaseButtons`, BLE disconnect edge, SafeMode/OTA gate 진입** 시 `_lock()` 하 `_macroAbortToken++` + `active=false`.
- **`_startMacro(newIdx)` 자체는 토큰을 증가시키지 않음** (조용히 대체).

### 6.3 대체(Replace) 정책
실행 중 매크로가 있을 때 `_startMacro(newIdx)`가 호출되면:
1. `WARN: macro replace: prev idx=N step=M` 로그
2. `_macroSnapshot` 덮어씀
3. `_macroState.stepIdx = 0`, `stepStartMs = now`
4. `startToken = _macroAbortToken` (현재 값)

### 6.4 매크로/런타임 관련 제거 이력 (v0415)

- `_getE10RuntimeConfig` 삭제 (v0412 rev5)
- `testMouseClick` / `_doTestMouseClick` / `EN_E10_HIDCMD_TEST_CLICK` 삭제 (v0412 rev5)
- **`forceReleaseAllButtons` 삭제 (v0415)**
- **`EN_E10_HidCmd_t::holdMs` 필드 삭제 (v0415, TEST_CLICK 삭제 이후 Dead)**
- **`_modeToggleCooldownMs` / `_lastModeToggleMs` 삭제 (v0415, `_fsmUpdate` Dead 분기)**

---

## 7. Mutex 정책

### 7.1 E10 `_mutex` (Recursive Mutex)
- **종류**: `xSemaphoreCreateRecursiveMutex`
- **이유**: `setSafeMode`/`setOtaGuard`가 락 보유 중 `_pushErr` 재진입. `_applyRuntimeLocked` → `_snapshotRuntimeToE10Config` 등 중첩 호출.
- `_lock()` / `_unlock()`: RAII 아님. 모든 경로에서 짝 맞춤.
- `_state` 접근: `pdMS_TO_TICKS(2)` 타임아웃. 실패 시 `_errMutexMiss++`.

### 7.2 L10 `_mutex` (Recursive Mutex, v0415 신규)
- **종류**: `xSemaphoreCreateRecursiveMutex`
- **이유**: `Adafruit_NeoPixel::show()`가 ESP32 RMT semaphore를 take → portMUX critical section 내부 호출 불가.
- **Recursive 선택**: `_enterIdle` → `_applyBase` → `_apply` 중첩 호출 재진입 안전.
- **suspend() blocking 중 mutex 해제**: `_ledTask`가 tick 진행하도록 yield.

### 7.3 큐 정책
- `_qFrame` overwrite: 무조건 성공(size=1).
- `_qHidCmd` / `_qActionExec` enqueue: `timeout=0`. 웹 태스크 블로킹 금지.

---

## 8. 계층 경계 요약

```
┌──────────────────────────────────────────────────────────┐
│                      Web Task (W10)                       │
│  - HTTP, LittleFS IO                                      │
│  - E10 API는 ST_W10_E10If_t 함수 포인터로만 호출           │
│    (v0415: setPptMode 콜백 삭제)                           │
│  - HID 직접 접근 금지                                      │
│  - _bootWifi 스냅샷 (되돌림 감지)                          │
└───────────────┬──────────────────────┬────────────────────┘
                │ _qHidCmd             │ _qActionExec
                │ _reqSpecialAction    │ _reqCommReleaseAll
                ▼                      ▼
┌────────────────────────┐  ┌──────────────────────────────┐
│   commTask (Core 0)    │  │   sensorTask (Core 1)         │
│  - HID 단독 실행자       │  │  - MPU I2C 단독               │
│  - 액션 실행             │  │  - FSM/제스처/precision        │
│  - 매크로 상태머신        │  │  - Special 동기 실행           │
│    (_startMacro/_tickMacro)  │  - SLEEP_NOW 즉시 진입         │
│  - _qFrame 소비          │  │  - _qFrame 생산 (Overwrite)   │
│  - gate 드레인           │  │  - Safe/Pairing wake 분기      │
│  - _actExec.tickRepeat   │  │  - _led.suspend/resume         │
└────────┬───────────────┘  └──────────────┬───────────────┘
         │                                  │
         │ _qFrame (size=1)                 │ _pushErr/_pushSpike
         │                                  │ _state (mutex)
         ▼                                  ▼
┌──────────────────────────────────────────────────────────┐
│              Shared: _cfgProfile, _state                  │
│              Guard: _mutex (recursive)                    │
└──────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────┐
│                    ledTask (Core 0, 50ms)                 │
│  - _led.tick() 단독                                        │
│  - v0415: L10 _mutex (Recursive)로 상태 변경 API 보호      │
└──────────────────────────────────────────────────────────┘
```

---

## 9. 위반 시 조치

| 위반 유형 | 감지 방법 | 조치 |
|---|---|---|
| Web → HID 직접 접근 | 코드 리뷰 (`_mouse.` / `_keyboard.` grep) | 반려. `_qHidCmd` 경유로 재작성 |
| CommTask → `_handleSpecial` | 코드 리뷰 | 반려. `_reqSpecialAction` 위임으로 재작성 |
| SensorTask → HID 직접 | 코드 리뷰 | 반려. `_qActionExec` 경유로 재작성 |
| `_led.tick()`을 타 태스크에서 호출 | `_led.tick` grep | 반려. `_ledTask`에서만 호출 |
| 큐 enqueue에 timeout != 0 | `xQueueSend` grep | 반려. `timeout=0` 강제 |
| `_macroState` 락 없이 갱신 (v0415) | 코드 리뷰 | 반려. `_lock()` 하 갱신 강제 |
| GPIO 핀 로컬 재정의 | `PIN_BTN` grep | 반려. `HW_DEF::` 참조 강제 |
| Consumer mask 상한 미검증 | `validateSlot` 리뷰 | 반려. `G_C20_CONSUMER_MASK_MAX` 검증 필수 |
| Momentary sanitization 누락 | `_sanitizeMomentarySlot` 호출 확인 | 반려. 버튼/제스처 양쪽 적용 필수 |
| 위임 플래그 read-clear 순서 위반 | 코드 리뷰 | 반려. `read → clear → 처리` 순서 강제 |
| L10 외부 상태 변경 lock 누락 | `_lock()` 쌍 확인 | 반려. 모든 public API 보호 |

---

## 10. 개정 이력

| 버전 | 날짜 | 변경 사항 |
|---|---|---|
| rev0 | 2026-09-15 | 최초 작성 (v0412) |
| rev1 | 2026-10-01 | 계약 문서 정합성 보완: `_startMacro` 대체 정책, 위임 플래그 상세화 |
| rev2 | 2026-10-01 | main.cpp 검증: Boot Factory Reset 문서화, SafeMode 조건부 실행 |
| rev3 | 2026-10-01 | **레거시 Hook 제거**: `E10_W10Apply` / `_applyFn` / `_applyCtx` 삭제. `_e10if` 경유 유일 원칙 |
| rev4 | 2026-10-02 | Dead Code 정리(A 카테고리), LED suspend blocking + resume async 계약 명시 |
| rev5 | 2026-10-02 | Round 2/3 조치: `forceReleaseButtons` enqueue 안전망, `motion_adv` config 스냅샷 락 |
| rev6 | 2026-10-07 | 웹 API 부분 패치 무결성 계약 |
| rev7 | 2026-10-07 | HW_Def SSOT, Momentary 단발화(BB-1), Host Cycle 페어링 취소(BB-2), Side F DOWN/UP 소비(BB-6) |
| **rev8** | **2026-10-09** | **v0415 리팩터**: A40 파일 삭제, Consumer mask Descriptor SSOT 계약 신규, Safe/Pairing Wake Mask 분기, BB-1 제스처 확장, `_macroState` lock 정책, L10 Recursive Mutex, `_bootWifi` W10 스냅샷, `setPptMode` 콜백 삭제, `forceReleaseAllButtons` 삭제, `ST_E10_HidCmd_t.holdMs` 삭제, `_fsmUpdate` 1-param, M10 Click-Lock 삭제, 스키마 411 |
