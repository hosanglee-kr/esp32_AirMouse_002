# STATE_0415.md — 상태 소유권 및 상태머신(FSM) 명세

> 대상 버전: `v0415` (ESP32-S3-Zero + MPU6050 AirMouse)  
> 위치: `src/v0415/docs_v0415/contract_doc/STATE_0415.md`
> 최종 갱신: 2026-10-11 (rev8 — R-1~R-3 통합 patch + C20 resetButton fix 반영. §2.7 resetButton 규약 통합)

---

## 1. 상태 변수 소유권 매트릭스

각 상태 변수에 대한 단독 쓰기(Write) 권한 및 읽기/동기화 규칙을 명시합니다.

| 상태 변수 / 객체 | 단독 소유자(Writer) | 허용된 Reader | 동기화 방식 | 원칙 및 라이프사이클 |
|---|---|---|---|---|
| `_freezeState` | `sensorTask` | `sensorTask` | 독점 (No sync) | Click-Freeze 전용 FSM. `motion_adv.click_freeze`는 `_lock()` 하 스냅샷 read |
| `_snapActiveAxis` | `sensorTask` | `sensorTask` | 독점 (No sync) | Snap-to-Axis 활성 축 추적 |
| `_snapCandidate` / `_snapCandidateFrames` | `sensorTask` | `sensorTask` | 독점 (No sync) | 축 확정 대기 (Chattering 방지) |
| `_precSub` / `_precT0` | `sensorTask` | `sensorTask` | 독점 (No sync) | Precision FSM 3단계 (ENTRY/TRACK/EXIT) |
| `_fsm` | `sensorTask` | `webTask` (`getStatus`) | `_lock()` (write), read는 관측 | `_activeMode == 2` 기반 파생 (v0415). `_fsmUpdate(gyroAbs)` 단독 write |
| `_macroState` | `commTask` (`_startMacro`/`_tickMacro`) | `commTask`, `sensorTask` (power), `webTask` (관측) | `_lock()` (v0415 정책) + `volatile bool active` | 스텝 단위 매크로 상태머신 |
| `_macroAbortToken` | `any` (취소 트리거) | `commTask` (`_tickMacro`) | `volatile uint32_t` | 단조 증가 카운터, 취소 판정 |
| `_macroSnapshot` | `commTask` (`_startMacro`) | `commTask` (`_tickMacro`) | `_lock()` (v0415) | 실행 시점 매크로 스냅샷 |
| `_hid` (BleCompositeHID) | `commTask` | `sensorTask`, `webTask` (`isConnected()`만) | Thread-safe API | 마우스/키보드 전송은 `commTask` 전용 |
| `_mpu`, `Wire` | `sensorTask` | `sensorTask` | 독점 (No sync) | I2C 센서 샘플링 및 복구 전용 |
| `_biasTracker` | `sensorTask` | `sensorTask` | 독점 (No sync) | 정지 상태 감지 및 자이로 바이어스 보정 |
| `_gesture` | `sensorTask` | `sensorTask` | 독점 (No sync) | Flick/Linear/Tilt 제스처 감지기 |
| `_btnDisp` | `sensorTask` | `sensorTask` | 독점 (No sync) | 물리 버튼 디바운스 및 더블/롱클릭 이벤트 |
| `_actExec` | `commTask` | `commTask` | 독점 (No sync) | HID 키/마우스 상태 유지 및 반복 액션 관리 |
| `_led` | `_ledTask` (tick) | `any` (setModeColor, flash 등) | **Recursive Mutex** (v0415) | 50ms 주기 tick은 `_ledTask` 단독. 상태 변경 API는 mutex 보호 |
| `_power` | `sensorTask` | `sensorTask` | 독점 (No sync) | Light/Deep-sleep 진입 및 WoM 인터럽트 처리 |
| `_activeMode` | `sensorTask`, `webTask` | `any` | `volatile uint8_t` + `_lock()` | 3-Mode (1=PC, 2=PPT, 3=TV) 시스템 상태 |
| `_reqSpecialAction` | `webTask` | `sensorTask` | `volatile uint8_t` | Special 액션 비동기 실행 위임 플래그 |
| `_reqCommReleaseAll` | `webTask`, `any` (enqueue 실패) | `commTask` | `volatile bool` | 프로파일 전환 시 HID 안전 Release 위임 |
| `_reqResetBtnDisp` | `webTask` | `sensorTask` | `volatile bool` | 프로파일 전환 시 디스패처 리셋 위임 |
| `_reqResetGesture` | `webTask` | `sensorTask` | `volatile bool` | 프로파일 전환 시 제스처 리셋 위임 |
| `_reqGyroCalib` | `webTask`/`any` | `sensorTask` | `volatile bool` | 캘리브레이션 위임 |
| `_reqI2CRecover` | `webTask`/`any` | `sensorTask` | `volatile bool` | I2C 복구 위임 |
| `_reqClearDiag` | `webTask` | `sensorTask` | `volatile bool` | 진단 초기화 위임 |
| `_reqSaveCfg` | `any` (BLE dirty) | `main loop` | `volatile bool` | 프로파일 저장 요청 |
| `_powerNotifyPending` | `sensorTask` | `sensorTask` | `volatile bool` | 커서 이동 기반 deferred activity |
| `_btnLDown` | `sensorTask` (Top L 이벤트) | `sensorTask` (`_applyClickFreeze`) | `volatile bool` | 좌클릭 물리 눌림 플래그 |
| `_moveGateHeld` | `sensorTask` (Top M 이벤트), `webTask` (switchProfile) | `sensorTask` (모션 파이프라인) | `volatile bool` | Middle Hold 커서 이동 허용 게이트 |
| `_frontHoldActive` | `sensorTask` (Side F 이벤트) | `sensorTask` (스크롤 처리) | `volatile bool` | Front Hold 스크롤 전용 상태 플래그 |
| `_cfgProfile` | `webTask` (write), `sensorTask` (init) | `sensorTask`, `commTask`, `webTask` | `_mutex` (Recursive Mutex) | 프로파일 변경/저장 시 반드시 `_lock()` 하 |
| `_state` | `sensorTask`, `commTask` (`_doForceReleaseNow`), `webTask` (setSafeMode 등) | `webTask` (`getStatus`) | `_mutex` (`pdMS_TO_TICKS(2)`) | 웹 관측용 상태 구조체. **R-2 LC-D: `btn_mask` 필드 삭제 (항상 0으로 write되어 무의미)**. 타임아웃 초과 시 miss 카운터 증가 |
| `_errHist`, `_spikes` | `sensorTask`, `commTask` | `webTask` | `_pushErr`, `_pushSpike` 내부 `_lock()` | 링버퍼 오버플로 방지 |
| `_profileSwitchInProgress` | `webTask` (`switchProfile`) | `webTask`, `sensorTask` | `volatile bool` | 프로파일 전환 재진입 차단 |
| `_safeMode` / `_otaGuard` | `sensorTask`, `webTask` | `any` | `volatile bool` + `_lock()` | HID 실행 게이트 |
| `_thSensor` / `_thComm` / **`_thLed`** | `begin()` (task create) | `_onPowerWake`, `getStatus` | 읽기 전용 (관측) | **v0415: `_thLed` 추가** (스택 워터마크 관측) |
| **`_bootWifi`** (W10) | `CL_W10_WebConfig::begin()` | `_bumpRebootMaskFromBoot` | 독점 (No sync) | **v0415: W10 부팅 WiFi 스냅샷 (되돌림 감지)** |

### 1.1 v0415 주요 변경

- **삭제 필드**: `_isPptMode` (E10), `_modeToggleCooldownMs`, `_lastModeToggleMs`, `_topMDownMs` (Dead), **`_state.btn_mask`** (R-2 LC-D), **`ST_E10_Status_t.btn_mask`** (R-2 LC-D)
- **삭제 심볼**: `setPptMode()` (R-1), `buildWakeMaskAll()` (R-2 LC-F), `_detectLinear(p_gx, p_gy, ...)` 시그니처 (R-3 LC-G)
- **추가 필드**: `_thLed`, `ST_E10_Status_t::active_mode`, `ST_E10_Status_t::task_stack_led_min_words`, `_bootWifi` (W10)
- **정책 변경**: `_macroState` → `_lock()` 하 원자적 갱신 (v0412는 부분 volatile)
- **L10 thread-safety**: Recursive Mutex (v0412는 무보호)
- **`_fsm` 판정**: `_isPptMode` → `_activeMode == 2` (v0415 `_fsmUpdate(gyroAbs)` 단일 param)

---

## 2. 상태머신(FSM) 상세 명세

### 2.1 Click-Freeze FSM (`_applyClickFreeze`)

물리 버튼(좌클릭)을 누를 때 손가락 반동으로 인한 커서 튐(Click-Jitter)을 방지하는 최종 게이트 FSM입니다.

| 상태 | 진입 조건 | 이탈 조건 | 동작 |
|---|---|---|---|
| **E10_FREEZE_IDLE** | 초기, Move Gate 해제, 타임아웃/이탈 | `btnDown && \|gyro\| < gyro_th && _moveGateHeld` | 정상 커서 출력 통과 |
| **E10_FREEZE_LOCKED** | IDLE에서 좌클릭 눌림 감지 | 1) 버튼 뗌 → HOLD<br>2) `freezeTimer > max_ms` → IDLE<br>3) `\|gyro\| > freeze_move_th` 또는 `!_moveGateHeld` → IDLE | `fx = 0`, `fy = 0` |
| **E10_FREEZE_HOLD** | LOCKED에서 버튼 릴리즈 | 1) `accumDistSq > thSq` 또는 `btnDown` → IDLE<br>2) `holdTimer >= hold_ms` → FADEOUT | `fx = 0`, `fy = 0` |
| **E10_FREEZE_FADEOUT** | HOLD 타이머 만료 | 1) `accumDistSq > thSq` 또는 `btnDown` → IDLE<br>2) `fadeTimer >= fadeout_ms` → IDLE | `scale = fadeTimer / fadeout_ms`<br>`fx *= scale`, `fy *= scale` |

> **활성 조건**: `cfg.enable == true` 이면서 `_moveGateHeld == true`. Move Gate 해제 시 즉시 IDLE 강제 리셋.

---

### 2.2 Snap-to-Axis FSM (`_applySnapToAxis`)

직선 드래그, 표 작업, 슬라이더 조절 시 수평/수직 축을 고정(Soft Snap)해주는 FSM입니다.

| 상태 | 조건 | 동작 |
|---|---|---|
| **E10_SNAP_NONE** | 초기 또는 큰 대각선 움직임 | 후보 축 탐색 (수평/수직 비율 및 최소 이동량 검사) |
| **후보 프레임 축적** | 한쪽 축 우세 (`\|major\| > ratio * \|minor\|`) | `candidateFrames++`, `confirm_frames` 도달 시 활성축 확정 |
| **E10_SNAP_HORIZ** | 수평 스냅 활성화 | `fy *= (1.0 - strength)` |
| **E10_SNAP_VERT** | 수직 스냅 활성화 | `fx *= (1.0 - strength)` |

> **활성 조건**: `cfg.enable == true` 이면서 `(cfg.mode_mask & (1 << (_activeMode - 1))) != 0`.  
> **이탈 조건**: 활성 축의 반대축 성분이 `\|minor\| > \|major\| / ratio_enter` 초과 시 후보 리셋.

---

### 2.3 Precision FSM (`_fsmUpdate` / `_applyPrecision`)

저속 미세 조작 시 커서 해상도를 비선형 완화하는 3단계 상태머신입니다.

| 상태 | 진입 조건 | 이탈 조건 | 동작 |
|---|---|---|---|
| **EN_PREC_OFF** | 정밀 모드 비활성 | 정밀 모드 설정 시 | 일반 시그모이드 가속 |
| **EN_PREC_ENTRY** | 모드 진입 직후 | `gyro <= entry_still_deg` 유지 `entry_ms` 이상 → TRACK | 조이스틱 감쇠 완화 대기 |
| **EN_PREC_TRACK** | ENTRY 조건 만족 | `gyro >= exit_move_deg` 발생 시 → EXIT | 데드존 + 게인 + 스무딩 필터 적용 |
| **EN_PREC_EXIT** | 고속 움직임 감지 | `gyro <= entry_still_deg` 재안정화 시 → TRACK | 일반 가속으로 점진 복귀 |

**v0415 변경**: `_fsmUpdate(float p_gyroAbs)` — 1-param. FSM 판정은 `_activeMode == 2` 기반:
- `_fsm = (_activeMode == 2) ? EN_FSM_PPT : EN_FSM_AIR;`
- `EN_FSM_SCROLL` 폐기 (Front Hold로 재설계)
- `p_btnScroll` / `p_btnModeLongToggle` 파라미터 삭제 (Dead)

---

### 2.4 Macro Sequencer FSM (`_tickMacro`)

`commTask` 루프 내에서 프레임 지연 없이 스텝별 딜레이와 실행을 제어하는 비동기 FSM입니다.

> ⚠️ **구현 참고 (개념적 서술)**: 아래 상태 이름은 문서적 이해를 돕기 위한 것이며, 실제 코드에는 **enum으로 존재하지 않습니다**. 실 구현은 `_macroState.active` (bool) + `_macroState.stepIdx` (uint8) + `s.delayMs` 경과 여부로 판정합니다. "WAIT_DELAY"와 "EXEC_STEP"은 `_tickMacro()` **한 호출 안에서 순차 처리**됩니다 (별개 프레임 아님).

| 개념 상태 | 진입 조건 | 이탈/전진 조건 | 동작 |
|---|---|---|---|
| **INACTIVE** | 초기 또는 실행 완료 | `_startMacro(idx)` 호출 | `active == false` |
| **WAIT_DELAY** | `s.delayMs > 0` 스텝 진입 | `(now - stepStartMs) >= delayMs` | 논블로킹 대기 |
| **EXEC_STEP** | 딜레이 경과 또는 delay=0 | 스텝 실행 완료 시 | `_actExec.exec(step, true)`, `stepIdx++` |
| **ABORTED** | 토큰 불일치 (`startToken != _macroAbortToken`) | 즉시 | `active = false` 강제 종료, WARN 로그 |

**v0415 정책 변경**:
- `_startMacro`: `_lock()` 하에 snapshot + state 초기화. **`active = true`를 마지막에 write** (재정렬 방지).
- `_tickMacro`: lock 없이 진행 (성능). `active`/`startToken` read는 volatile 원자.
- `switchProfile` / `_setActiveMode` / `forceReleaseButtons`: `_lock()` 하 `_macroAbortToken++` + `active = false`.
- disconnect edge / gate 진입: 동일하게 `_lock()` 하 abort.

---

### 2.5 Macro 대체(Replace) 및 취소(Abort) 정책

| 시나리오 | 동작 | 취소 방식 | 로그 |
|---|---|---|---|
| 실행 중 `_startMacro(newIdx)` 호출 | **조용히 대체** (이전 스냅샷 무효화) | 토큰 증가 없음, `_macroSnapshot` 덮어씀 | `WARN: macro replace: prev idx=N step=M` |
| `switchProfile` 호출 | 즉시 취소 | `_lock()` 하 `_macroAbortToken++` | – |
| `_setActiveMode` 호출 | 즉시 취소 | `_lock()` 하 `_macroAbortToken++` | – |
| `forceReleaseButtons()` 호출 | 즉시 취소 | `_lock()` 하 `_macroAbortToken++` | – |
| BLE disconnect edge (commTask) | 즉시 취소 | `_lock()` 하 `_macroAbortToken++` | – |
| SafeMode / OTA Gate 진입 | 즉시 취소 | `_lock()` 하 `_macroAbortToken++` | – |
| 매크로 자연 종료 | `active = false` | stepIdx >= stepCount | `INFO: macro end: idx=N` |

> **대체 vs 취소 구분**: 사용자 의도가 "새 매크로 실행"이면 대체, "안전 정지"면 명시적 abort. `_startMacro`가 토큰을 증가시키지 않는 이유는 새 매크로 시작 시 `startToken = _macroAbortToken`으로 재설정되기 때문입니다.

---

### 2.6 전원 관리 FSM (`sensorTask` 내부)

| 상태 | 진입 조건 | 이탈 조건 | 동작 |
|---|---|---|---|
| **ACTIVE** | 초기 / Wake 복귀 | 유휴 시간 초과 | 정상 동작, `notifyActivity()`로 타이머 리셋 |
| **LED_SUSPEND** | Sleep 조건 모두 만족 | fadeout 완료 (≤ `led_fadeout_ms + 200ms`) | `_led.suspend(snap)` — **blocking** RED fadeout → OFF |
| **LIGHT_SLEEP** | `sleepNow(now, safeOrPair)` 진입 | EXT1 Wake (Safe: Side C 단독 / 일반: MPU + 6버튼) | `esp_light_sleep_start()` (RAM 보존) |
| **WAKE_RESUME** | Light-sleep 복귀 | Fast Recalib 완료 | `_led.resume(snap)` — **async** base fadein + `biasTracker.startFastRecalibrate(fast_recalib_ms)` |
| **DEEP_SLEEP** | `deep_idle_timeout_ms` 초과 | 버튼 Wake | `esp_deep_sleep_start()` (재부팅). Safe 시 Side C 단독 마스크 |
| **SLEEP_NOW** (즉시) | `_handleSpecial(SLEEP_NOW)` | wake 이벤트 | **v0415 Q4-a**: sensorTask 컨텍스트 blocking. LED suspend → sleepNow → LED resume → Fast Recalib |

**v0415 Safe/Pairing 분기**:
- `sleepNow(now, safeOrPairing)` — Safe 또는 Pairing 시 `buildWakeMaskSafe()`
- `deepSleepNow(now, hid, pairing, safe)` — Safe 시 Side C 단독, Pairing 시 sleep 금지

---

### 2.7 Button Dispatcher Phase 전이

`PHASE_DOUBLE`은 Long/Hold 타이머 검사에서 **제외**. 이유: Side C Double(Mode Cycle) 후 계속 hold 시 Pairing/Host Cycle이 뒤이어 오발화하는 UX 문제 방지.

| Phase | Long/Hold 검사 | Double 대기 | CLICK 발화 |
|---|:---:|:---:|:---:|
| `PHASE_IDLE` | – | – | – |
| `PHASE_PRESSED` | ✓ (`longFired`/`hold2sFired`/`hold3sFired`) | – | – |
| `PHASE_WAIT_CLICK` | – | ✓ (`double_delay_ms`) | ✓ (타임아웃 시) |
| `PHASE_DOUBLE` | **✗ (제외)** | – | – |

**이벤트 발화 정책 (C20 patch 반영)**:
1. **DOUBLE 시 2번째 DOWN 흡수**: `PHASE_WAIT_CLICK → PHASE_DOUBLE` 전이 시 DOWN 발화 없이 DOUBLE만 발화.
   시퀀스: `DOWN(1) → UP(1) → DOUBLE → UP(2)`.
   소비처가 첫 DOWN/UP으로 1회 클릭, DOUBLE 슬롯이 2회째 클릭 발화 (Top L 기본 매핑).
2. **LONG/HOLD progressive**: `PHASE_PRESSED` 유지 중 각 임계 도달 시 1회씩 순차 발화.
   `longFired`/`hold2sFired`/`hold3sFired` 플래그로 각 임계당 1회만 발화.
3. **PHASE_DOUBLE 제외**: `_checkTimers`에서 `PHASE_DOUBLE`은 Long/Hold 검사 대상 아님 (R2-M-1).

**v0415 `setTimings` 정책**: 8-param 단일. 구버전 5-param 오버로드 삭제.

> **`resetButton()` 타임스탬프 초기화 규약 (rev7, BB-3 + L5-A3-11)**:
>
> `_btnDisp.resetButton(idx)` 및 `resetAll()` 호출 시 다음을 초기화한다:
>
> - **상태**: `phase = PHASE_IDLE`, `longFired = false`, `hold2sFired = false`, `hold3sFired = false`
> - **시간 계측 타임스탬프 (R-1 fix)**: `downMs = millis()`, `upMs = millis()`, `waitClickStartMs = millis()`, `lastRawChangeMs = millis()`
>   - **이전 rev6**: 0으로 초기화 → **폐기**
>   - **이유**: reset 직후 release edge 발생 시
>     `heldMs = (rawDownMs>0) ? ... : (now - downMs)` 계산에서
>     `downMs=0`이면 거대값 → phase가 PRESSED였을 경우 LONG 오발화 위험
> - **디바운스**: `stableCount = 0`, `rawDownMs = 0`
> - **유지**: `stableState`, `lastRaw` (물리 상태 반영 — 프로파일 전환 중 버튼 눌림 유지 시 debounce 정상 동작)

> **update() 호출 전제조건**:
> - `update()`는 5~10ms 주기로 호출 필수 (sensorTask 8ms 권장)
> - 15ms 이상이면 `_debounceMinTicks(3)`가 시간 조건 지배 → 실효 디바운스 증가 (역효과)
> - 20ms 이상이면 `v_timeOk`가 32ms를 초과하여 디바운스 무력화 위험
> - 콜백 경량 필수 (`_cb()` 내부 블로킹 금지)

---

### 2.8 L10 LED 상태머신 (v0415 Mutex 보호)

| 상태 | 설명 |
|---|---|
| `EN_L10_ST_IDLE` | Mode 색 점등 (base color) |
| `EN_L10_ST_FLASH` | 단발 점등 (일정 시간 후 IDLE) |
| `EN_L10_ST_BLINK` | 주기적 on/off (pairing 등) |
| `EN_L10_ST_FADEOUT` | 밝기 감소 → OFF (sleep 진입) |
| `EN_L10_ST_FADEIN` | 점진 점등 → IDLE (wake 복귀) |
| `EN_L10_ST_OFF` | 완전 소등 |

**Thread-safety (v0415)**:
- 모든 public API는 `_lock()` / `_unlock()` 쌍으로 보호 (Recursive Mutex)
- `_tickInternal()`은 lock 보유 가정
- `suspend()` blocking 루프 중 mutex 해제 → `_ledTask`가 tick 진행 가능
- `portMUX` 대신 mutex 선택 이유: `Adafruit_NeoPixel::show()`의 RMT semaphore take

**Snapshot/Resume**:
- `suspend(snap)`: 상태 스냅샷 + RED fadeout → OFF (blocking)
- `resume(snap)`: base color fadein → IDLE (async)

---

## 3. 위임(Delegation) 패턴 정리

Web 태스크가 sensorTask/commTask 소유 상태를 직접 조작하지 않고 플래그로 위임하는 패턴입니다.

| 플래그 | 소유자 (Writer) | 소비자 (Consumer) | 목적 |
|---|---|---|---|
| `_reqSpecialAction` | `webTask` | `sensorTask` 루프 진입부 | Special 액션을 sensorTask 컨텍스트에서 동기 실행 |
| `_reqCommReleaseAll` | `webTask` (`switchProfile`), `any` (`forceReleaseButtons` 큐 Full 시) | `commTask` 루프 진입부 | 큐 Drop과 무관하게 HID 안전 Release 100% 보장 |
| `_reqResetBtnDisp` | `webTask` (`switchProfile`) | `sensorTask` 루프 진입부 | `_btnDisp.resetAll()` 위임 |
| `_reqResetGesture` | `webTask` (`switchProfile`) | `sensorTask` 루프 진입부 | `_gesture.reset()` 위임 |
| `_reqGyroCalib` | `webTask`/`any` | `sensorTask` 루프 진입부 | 캘리브레이션 + `biasTracker.reset()` 위임 |
| `_reqI2CRecover` | `webTask`/`any` | `sensorTask` 루프 진입부 | I2C 복구 위임 |
| `_reqClearDiag` | `webTask` | `sensorTask` 루프 진입부 | 진단 카운터/링버퍼 초기화 |
| `_reqSaveCfg` | `any` (BLE dirty) | `main loop` (`tickConfigSave`) | 프로파일 저장 |

> **원칙**: 위 플래그는 모두 `volatile`로 선언되며, 설정자(Writer)와 소비자(Consumer)는 서로 다른 태스크에서 실행됨. 소비자는 반드시 **flag read → 즉시 clear → 처리** 순서를 유지하여 재진입을 방지합니다.

---

## 4. 개정 이력

| 버전 | 날짜 | 변경 사항 |
|---|---|---|
| rev0 | 2026-09-15 | 최초 작성 (v0412) |
| rev1 | 2026-10-01 | §2.5 Macro 대체/취소 정책 신설, §2.4 Macro FSM 개념적 서술 경고 |
| rev4 | 2026-10-02 | LED suspend/resume FSM 상세화 (blocking/async 구분) |
| rev5 | 2026-10-02 | motion_adv config 스냅샷 락, `_reqCommReleaseAll` 확장, PHASE_DOUBLE hold 제외 |
| rev6 | 2026-10-07 | `resetButton()` 타임스탬프 완전 초기화, HW_Def SSOT 반영 |
| rev7 | 2026-10-09 | **v0415 리팩터**: `_isPptMode` 삭제 → `active_mode` 일원화, `_fsmUpdate(gyroAbs)` 1-param, `_macroState` lock 정책 강화, L10 Recursive Mutex, `_bootWifi` W10 스냅샷, `_thLed` 추가, `task_stack_led_min_words` 관측 |
| **rev8** | **2026-10-11** | **R-1~R-3 통합 patch**: `_state.btn_mask` / `ST_E10_Status_t.btn_mask` 필드 삭제 (R-2 LC-D), `setPptMode()` 삭제 (R-1), `buildWakeMaskAll()` alias 삭제 (R-2 LC-F). **§2.7 resetButton 규약 rev7로 갱신**: downMs/upMs/waitClickStartMs = `millis()` (0 아님, L5-A3-11). **DOUBLE DOWN 흡수** + **LONG/HOLD progressive** + **update() 5~10ms 전제조건** 명시. HW_Def 핀 재배치 (GP7~GP13) 반영. |
