# CONTRACT.md — 모듈 경계 및 인터페이스 계약

> 대상 버전: `v0410` (ESP32-S3-Zero + MPU6050 AirMouse)  
> 위치: `src/v0410/docs_v0410/contract_doc/CONTRACT.md`
> 최종 갱신: 2026-10-01 (rev3 — 레거시 Hook 제거 + main.cpp 검증)  

---

## 1. 개요 및 설계 원칙

본 문서는 `v0410` 펌웨어의 태스크(`sensorTask`, `commTask`, `ledTask`, `webTask`) 간 호출 경계, 통신 큐, 공유 자원 접근 규칙을 고정 정의합니다. 모든 신규 코드 작성 및 코드 리뷰는 본 계약의 준수 여부를 1차 기준으로 검증합니다.

### 4대 절대 금지 규칙
1. **Web Task의 HID 직접 접근 금지**: `_mouse`, `_keyboard` 인스턴스는 오직 `commTask`만 제어하며, Web/Sensor는 반드시 `_qHidCmd` 또는 `_qActionExec`를 경유해야 합니다.
2. **Special 액션의 Context 격리**: `EN_C20_ACT_SPECIAL`은 FreeRTOS 큐에 인큐하지 않고 **오직 `sensorTask` 컨텍스트에서만 동기 실행**합니다.
3. **LED 상태머신의 단독 Tick**: `_led.tick()`은 오직 `_ledTask`(50ms 주기)에서만 호출하며 타 태스크에서 tick을 호출하지 않습니다.
4. **블로킹 금지**: 모든 큐 인큐(`_enqueueAction`, `_enqueueHidCmd`, `_pushFrame`)는 `timeout = 0` (non-blocking) 원칙을 유지합니다.

---

## 2. 함수 인터페이스 계약

| 함수 심볼 | 권한 소유 호출자 | 실행 컨텍스트 제한 | 주요 부수효과 | 실패/경합 시 처리 |
|---|---|---|---|---|
| `_startMacro(idx)` | `commTask` only | 논블로킹 (상태머신 초기화) | `_macroSnapshot` 갱신, `_macroState` 활성화 | ① 인덱스 범위 초과 시 `WARN` 로그 후 무시<br>② **이미 실행 중이면 `WARN` 로그 후 조용히 대체(replace)** — 이전 매크로는 abort 토큰 불일치로 종료되지 않고 즉시 덮어씀 (STATE.md §2.5) |
| `_tickMacro()` | `commTask` only | 매 프레임 논블로킹 호출 | 딜레이 경과 시 액션 실행, 스텝 전진 | 토큰 불일치(`_macroAbortToken`) 시 즉시 중단 + WARN 로그 |
| `_resolveSlot(mode, trig)` | `sensorTask`, `commTask`, `webTask` | Read-only | 없음 (Global + Mode Override O(1) 해석) | 비정상 인덱스 시 `EN_C20_ACT_NONE` 반환 |
| `switchProfile(idx)` | `webTask` only | 진행 중 재진입 차단 (`_cfgProfileValid`) | `_macroAbortToken++`, 큐 드레인, 리셋 위임, HID release | 전환 중이면 `false` 반환 및 조기 종료 |
| `_handleSpecial(special)` | `sensorTask` only | 타 태스크 호출 금지 | 모드 변경, 페어링 시작, 캘리브레이션 요청 등 | 알 수 없는 코드 시 무시 |
| `forceReleaseButtons()` | `any` (web, sensor, comm) | 논블로킹 | `_qHidCmd`에 `RELEASE_ALL` 인큐 + `_macroAbortToken++` | 큐 Full 시 drop (false) |
| `_applyClickFreeze(...)` | `sensorTask` only | 파이프라인 최종단 | `_freezeState` 전이 및 커서 좌표 0 클램프 | 없음 |
| `_applySnapToAxis(...)` | `sensorTask` only | Click-Freeze 직전 | 활성 축 외 성분 감쇠(Soft Snap) | 미활성화 시 원본 유지 |
| `_recoverI2C()` | `sensorTask` only | I2C 버스 재초기화 | `Wire.end()`, `Wire.begin()`, `_mpu.begin()` | 실패 시 `_consecutiveRecoverFail++` |
| `_runGyroCalibration()` | `sensorTask` only | 초기 부팅 또는 정지 시 1000ms 측정 | 자이로 오프셋 산출, `_gyroCalibDone = true` | 측정 중 큰 움직임 시 재시도. 루프 내 `_ble.tick()` 유지 |
| `_snapshotRuntimeToE10Config(out)` | `sensorTask`/`commTask`/`webTask` | `_lock()` 하에 호출 필수 | 런타임 → config 구조체 반영 | `motion_adv`/`power`/`button` 백업 후 재적용 |
| `_applyE10ToRuntime(e10)` | `sensorTask`/`webTask` | `_applyRuntimeLocked()` 경유 (락 보유) | 런타임 필드 일괄 반영 | 범위 클램프 다수 |
| `_enqueueAction(slot, isDown)` | `sensorTask`, `webTask` | 논블로킹 | `_qActionExec` 인큐 | 큐 Full 시 drop (false) |
| `_enqueueHidCmd(cmd)` | `webTask`, `sensorTask` | 논블로킹 | `_qHidCmd` 인큐 | 큐 Full 시 drop (false) |
| `_pushFrame(fr)` | `sensorTask` only | 논블로킹 Overwrite | `_qFrame` 최신 프레임 유지 | size=1 Overwrite로 항상 성공 |
| `applyRuntimeE10(e10)` | `W10` (`/api/control` apply-only) | `_lock()` 하 `_applyRuntimeLocked` | E10 파라미터만 런타임 반영 (persist X) | 항상 true (범위 클램프 다수) |
| `execLiveTest(kind, h, p16, p32)` | `webTask` | SPECIAL은 위임, 그 외 큐 경유 | `_reqSpecialAction` 또는 `_qActionExec` | SafeMode/OTA 차단 시 false |

> **W10 → E10 호출 경로 (rev3)**:
> - `W10.begin(cfg, e10if)`의 유일한 E10 진입은 **`_e10if` (`ST_W10_E10If_t` 함수 포인터 테이블)**.
> - Legacy `applyFn`/`applyCtx` 및 `CL_E10_EliteAirMouse::E10_W10Apply`는 **rev3에서 완전 제거됨**.
> - `/api/config/apply`는 `/api/config/save` 별칭으로 통합되어 `_e10if->reloadProfile` 사용.
> - `applyRuntimeE10`은 `/api/control`의 apply-only 요청 (persist X)에서만 사용.
> - **신규 기능 추가 시 `_e10if`에 함수 포인터를 등록하는 방식이 유일한 원칙**.

---

## 3. FreeRTOS 큐 계약

| 큐 식별자 | Producer 태스크 | Consumer 태스크 | 용량(Size) | 타임아웃 | 오버플로 / 경합 정책 |
|---|---|---|---|---|---|
| `_qFrame` | `sensorTask` | `commTask` | 1 (`ST_E10_Frame_t`) | 0 (Overwrite) | `xQueueOverwrite`: 지연 없이 항상 최신 모션 프레임 유지 |
| `_qActionExec` | `sensorTask`, `webTask` | `commTask` | 8 (`ST_ActionCmd_t`) | 0 | Full 시 즉시 drop (커서 및 센서 파이프라인 지연 방지) |
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

// _qHidCmd
struct ST_E10_HidCmd_t {
    uint8_t  cmd;    // RELEASE_ALL / TEST_CLICK / TEST_PPT
    uint8_t  arg0;
    uint8_t  arg1;
    uint16_t holdMs;
    uint32_t code;
};
```

---

## 4. 공유 변수 및 상태 동기화 계약

| 공유 변수명 | Writer | Reader | 동기화 메커니즘 | 불변식 및 원자성 근거 |
|---|---|---|---|---|
| `_macroAbortToken` | `sensorTask`, `commTask`, `webTask` | `commTask` (`_tickMacro`) | `volatile uint32_t` | 단조 증가 카운터. 토큰 불일치 시 매크로 취소 (32비트 원자적 단일 쓰기) |
| `_macroState.active` | `sensorTask`, `commTask`, `webTask` | `commTask`, `sensorTask` (power) | `volatile bool` | 슬립 및 매크로 스케줄링 플래그 |
| `_reqResetBtnDisp` | `webTask` (`switchProfile`) | `sensorTask` (루프 진입부) | `volatile bool` | 위임 패턴. Web이 요청하고 SensorTask가 실제 resetAll 실행 |
| `_reqResetGesture` | `webTask` (`switchProfile`) | `sensorTask` (루프 진입부) | `volatile bool` | 위임 패턴. Gesture 감지기 상태 리셋 |
| `_powerNotifyPending` | `sensorTask` (파이프라인) | `sensorTask` (전원 루틴) | `volatile bool` | 센서 루프 내 deferred 활동 알림 |
| `_btnLDown` | `sensorTask` | `sensorTask` (`_applyClickFreeze`) | `volatile bool` | 좌클릭 물리 눌림 플래그 |
| `_activeMode` | `sensorTask`, `webTask` | `sensorTask`, `commTask`, `webTask` | `volatile uint8_t` + `_lock()` | 모드 변경 시 원자적 쓰기 (1~3 범위) |
| `_moveGateHeld` | `sensorTask` (Top M) | `sensorTask` (모션 파이프라인) | `volatile bool` | Middle Hold 커서 이동 허용 게이트 |
| `_frontHoldActive` | `sensorTask` (Side F) | `sensorTask` (스크롤 처리) | `volatile bool` | Front Hold 스크롤 전용 상태 플래그 |
| `_cfgProfile` | `webTask` (write), `sensorTask` (init) | `sensorTask`, `commTask`, `webTask` | `_mutex` (Recursive Mutex) | 프로파일 변경/저장 시 반드시 `_lock()` 하에 접근 |
| `_state` | `sensorTask` (모션/에러 기록) | `webTask` (`getStatus`) | `_mutex` (`pdMS_TO_TICKS(2)`) | 웹 관측용 상태 구조체. 타임아웃 초과 시 miss 카운터 증가 |
| `_errHist`, `_spikes` | `sensorTask`, `commTask` | `webTask` | `_pushErr`, `_pushSpike` 내부 `_lock()` | 링버퍼 오버플로 방지 및 인덱스 정합성 보호 |
| `_reqSpecialAction` | `webTask` (`execLiveTest`) | `sensorTask` (루프 진입부) | `volatile uint8_t` | 위임 패턴. Special 액션을 sensorTask 컨텍스트에서 안전하게 실행 |
| `_reqCommReleaseAll` | `webTask` (`switchProfile`) | `commTask` (루프 진입부) | `volatile bool` | 위임 패턴. 프로파일 스위치 시 큐 Drop에 영향받지 않는 HID Release 100% 보장 |
| `_reqGyroCalib` | `webTask`/`any` | `sensorTask` (루프 진입부) | `volatile bool` | 캘리브레이션 + `biasTracker.reset()` 위임 |
| `_reqI2CRecover` | `webTask`/`any` | `sensorTask` (루프 진입부) | `volatile bool` | I2C 복구 위임 |
| `_reqClearDiag` | `webTask` | `sensorTask` (루프 진입부) | `volatile bool` | 진단 카운터 초기화 위임 |
| `_reqSaveCfg` | `any` (BLE dirty) | `main loop` (`tickConfigSave`) | `volatile bool` | 프로파일 저장 요청 |
| `_profileSwitchInProgress` | `webTask` (`switchProfile`) | `webTask`, `sensorTask` | `volatile bool` | 프로파일 전환 재진입 차단 (busy 플래그) |
| `_safeMode` / `_otaGuard` | `sensorTask`, `webTask` | `any` | `volatile bool` + `_lock()` | HID 실행 게이트 |
| `_pairing` / `_dirty` (B20) | `sensorTask` (`_ble.tick`), `webTask` (`enterPairing`) | `sensorTask`, `webTask` | `volatile bool` | Pairing 상태 및 config 저장 필요 플래그 |

---

## 5. 태스크 간 Special 액션 라우팅 계약

**핵심 규칙:** Special 액션(`EN_C20_ACT_SPECIAL`)은 **sensorTask에서만 실행**.

```
_handleSlotButton / _handleGesture
  ├─ kind == SPECIAL → _handleSpecial() 직접 호출 (sensorTask 컨텍스트)
  └─ kind != SPECIAL → _enqueueAction() → commTask

execLiveTest (webTask)
  ├─ kind == SPECIAL → _reqSpecialAction = param16 (위임)
  └─ kind != SPECIAL → _enqueueAction() → commTask

CL_C20_ActionExec::exec()
  └─ EN_C20_ACT_SPECIAL case → 안전망으로 무시(drop)
```

---

## 6. 매크로 실행 계약 (비동기 상태머신)

매크로는 **commTask 블로킹 없이** 스텝 단위로 진행:

```
commTask 큐 드레인
  ├─ MACRO kind → _startMacro(p_idx)  ← 스냅샷만, 즉시 리턴
  │                                    (실행 중이면 WARN 로그 + 조용히 대체)
  └─ 기타       → _actExec.exec()     ← 즉시 실행

commTask 매 루프 후반
  └─ _tickMacro()  ← delay 경과 시 다음 스텝 실행, 커서 프레임 소비 지속
```

### 취소 정책
- `_macroAbortToken` 카운터가 `_macroState.startToken`과 다르면 즉시 abort (WARN 로그).
- **`switchProfile`, `_setActiveMode`, `forceReleaseButtons`, BLE disconnect edge, SafeMode/OTA gate 진입** 시 토큰 증가.
- **`_startMacro(newIdx)` 자체는 토큰을 증가시키지 않음** (조용히 대체). 새 매크로 시작 시 `startToken = _macroAbortToken`으로 재설정.

### 대체(Replace) 정책
실행 중 매크로가 있을 때 `_startMacro(newIdx)`가 호출되면:
1. `WARN: macro replace: prev idx=N step=M` 로그
2. `_macroSnapshot` 덮어씀
3. `_macroState.stepIdx = 0`, `stepStartMs = now`
4. `startToken = _macroAbortToken` (현재 값)

---

## 7. Mutex 정책

- `_mutex`: **recursive mutex** (`xSemaphoreCreateRecursiveMutex`). 이유: `setSafeMode`/`setOtaGuard`가 락 보유 중 `_pushErr` 재진입.
- `_lock()` / `_unlock()`: RAII 아님. 모든 경로에서 짝 맞춤.
- `_qFrame` overwrite: 무조건 성공(size=1).
- `_qHidCmd` / `_qActionExec` enqueue: `timeout=0`. 웹 태스크 블로킹 금지.
- `_state` 접근: `pdMS_TO_TICKS(2)` 타임아웃. 실패 시 `_errMutexMiss++`.

---

## 8. 계층 경계 요약

```
┌──────────────────────────────────────────────────────────┐
│                      Web Task (W10)                       │
│  - HTTP, LittleFS IO                                      │
│  - E10 API는 ST_W10_E10If_t 함수 포인터로만 호출           │
│  - HID 직접 접근 금지                                      │
└───────────────┬──────────────────────┬────────────────────┘
                │ _qHidCmd             │ _qActionExec
                │ _reqSpecialAction    │ _reqCommReleaseAll
                ▼                      ▼
┌────────────────────────┐  ┌──────────────────────────────┐
│   commTask (Core 0)    │  │   sensorTask (Core 1)         │
│  - HID 단독 실행자       │  │  - MPU I2C 단독               │
│  - 액션 실행             │  │  - FSM/제스처/precision        │
│  - 매크로 상태머신        │  │  - Special 동기 실행           │
│  - _qFrame 소비          │  │  - _qFrame 생산 (Overwrite)   │
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
│  - setModeColor/flash 등은 any task 호출 가능              │
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
| `_macroState` 락 없이 접근 | 코드 리뷰 | 반려. `volatile` + 토큰 기반 재작성 |
| 위임 플래그 read-clear 순서 위반 | 코드 리뷰 | 반려. `read → clear → 처리` 순서 강제 |

---

## 10. 개정 이력

| 버전 | 날짜 | 변경 사항 |
|---|---|---|
| rev0 | 2026-09-15 | 최초 작성 |
| rev1 | 2026-10-01 | 계약 문서 정합성 보완: `_startMacro` 대체 정책 명시, `_topMDownMs` Dead Code 표기, 위임 플래그/매크로 취소 정책 상세화, SPEC 참조 `_002.md` 갱신 |
| rev2 | 2026-10-01 | main.cpp 검증(N-1~N-4): Boot Factory Reset 문서화, SafeMode 조건부 실행, `E10_CONST::PIN_BTN_MODE` 통일, 로그 prefix `[0274]` → `[0410]` |
| rev3 | 2026-10-01 | **레거시 Hook 제거**: `E10_W10Apply` / `_applyFn` / `_applyCtx` 삭제. `W10.begin(cfg, e10if)` 2인자 시그니처. `_e10if` 경유 유일 원칙 명시 |
| rev4 | 2026-10-02 | Dead Code 정리: `_topMDownMs` (E10), `apiGetPpt`/`apiPostPpt` (W10) 제거 |
