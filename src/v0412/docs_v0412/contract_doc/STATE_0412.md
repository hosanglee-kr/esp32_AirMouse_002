# STATE_0412.md — 상태 소유권 및 상태머신(FSM) 명세

> 대상 버전: `v0412` (ESP32-S3-Zero + MPU6050 AirMouse)  
> 위치: `src/v0412/docs_v0412/contract_doc/STATE_0412.md`
> 최종 갱신: 2026-10-07 (rev6 — Button Dispatcher resetButton 타임스탬프 리셋 규약 및 HW_Def SSOT 반영)

---

## 1. 상태 변수 소유권 매트릭스

각 상태 변수에 대한 단독 쓰기(Write) 권한 및 읽기/동기화 규칙을 명시합니다.

| 상태 변수 / 객체 | 단독 소유자(Writer) | 허용된 Reader | 동기화 방식 | 원칙 및 라이프사이클 |
|---|---|---|---|---|
| `_freezeState` | `sensorTask` | `sensorTask` | 독점 (No sync) | Click-Freeze 전용 FSM. `motion_adv.click_freeze`는 `_lock()` 하 스냅샷 read [R2-C-1] |
| `_snapActiveAxis` | `sensorTask` | `sensorTask` | 독점 (No sync) | Snap-to-Axis 활성 축 추적. `motion_adv.snap`은 `_lock()` 하 스냅샷 read [R2-C-1] |
| `_snapCandidate` / `_snapCandidateFrames` | `sensorTask` | `sensorTask` | 독점 (No sync) | 축 확정 대기 (Chattering 방지) |
| `_precSub` | `sensorTask` | `sensorTask` | 독점 (No sync) | Precision FSM 3단계 (ENTRY/TRACK/EXIT) |
| `_macroState` | `commTask` (start/tick) | `commTask`, `sensorTask` (power) | `volatile bool active` | 스텝 단위 매크로 상태머신 |
| `_macroAbortToken` | `any` (취소 트리거) | `commTask` (`_tickMacro`) | `volatile uint32_t` | 단조 증가 카운터, 취소 판정 |
| `_hid` (BleCompositeHID) | `commTask` | `sensorTask`, `webTask` (`isConnected()`만) | Thread-safe API | 마우스/키보드 전송은 `commTask` 전용 |
| `_mpu`, `Wire` | `sensorTask` | `sensorTask` | 독점 (No sync) | I2C 센서 샘플링 및 복구 전용 |
| `_biasTracker` | `sensorTask` | `sensorTask` | 독점 (No sync) | 정지 상태 감지 및 자이로 바이어스 보정 |
| `_gesture` | `sensorTask` | `sensorTask` | 독점 (No sync) | Flick/Linear/Tilt 제스처 감지기 |
| `_btnDisp` | `sensorTask` | `sensorTask` | 독점 (No sync) | 물리 버튼 디바운스 및 더블/롱클릭 이벤트 |
| `_actExec` | `commTask` | `commTask` | 독점 (No sync) | HID 키/마우스 상태 유지 및 반복 액션 관리 |
| `_led` | `_ledTask` (tick) | `any` (setModeColor, flash 등) | 내부 상태머신 | 50ms 주기 전용 태스크에서 단독 갱신 |
| `_power` | `sensorTask` | `sensorTask` | 독점 (No sync) | Light-sleep 진입 및 WoM 인터럽트 처리 (Phase 11.6 단일 FSM) |
| `_activeMode` | `sensorTask`, `webTask` | `any` | `volatile uint8_t` + `_lock()` | 3-Mode (1=PC, 2=PPT, 3=TV) 시스템 상태 |
| `_reqSpecialAction` | `webTask` | `sensorTask` | `volatile uint8_t` | Special 액션 비동기 실행 위임 플래그 |
| `_reqCommReleaseAll` | `webTask` | `commTask` | `volatile bool` | 프로파일 전환 시 HID 안전 Release 위임 플래그 |
| `_reqResetBtnDisp` | `webTask` | `sensorTask` | `volatile bool` | 프로파일 전환 시 디스패처 리셋 위임 |
| `_reqResetGesture` | `webTask` | `sensorTask` | `volatile bool` | 프로파일 전환 시 제스처 리셋 위임 |
| `_reqGyroCalib` | `webTask`/`any` | `sensorTask` | `volatile bool` | 캘리브레이션 위임 |
| `_reqI2CRecover` | `webTask`/`any` | `sensorTask` | `volatile bool` | I2C 복구 위임 |
| `_reqClearDiag` | `webTask` | `sensorTask` | `volatile bool` | 진단 초기화 위임 |
| `_reqSaveCfg` | `any` (BLE dirty, tickConfigSave) | `main loop` | `volatile bool` | 프로파일 저장 요청 |
| `_powerNotifyPending` | `sensorTask` | `sensorTask` | `volatile bool` | 커서 이동 기반 deferred activity |
| `_btnLDown` | `sensorTask` (Top L 이벤트) | `sensorTask` (`_applyClickFreeze`) | `volatile bool` | 좌클릭 물리 눌림 플래그 (Phase 1) |
| `_moveGateHeld` | `sensorTask` (Top M 이벤트) | `sensorTask` (모션 파이프라인) | `volatile bool` | Middle Hold 커서 이동 허용 게이트 |
| `_frontHoldActive` | `sensorTask` (Side F 이벤트) | `sensorTask` (스크롤 처리) | `volatile bool` | Front Hold 스크롤 전용 상태 플래그 |
| `_cfgProfile` | `webTask` (write), `sensorTask` (init) | `sensorTask`, `commTask`, `webTask` | `_mutex` (Recursive Mutex) | 프로파일 변경/저장 시 반드시 `_lock()` 하에 접근 |
| `_state` | `sensorTask` (모션/에러 기록) | `webTask` (`getStatus`) | `_mutex` (`pdMS_TO_TICKS(2)`) | 웹 관측용 상태 구조체. 타임아웃 초과 시 miss 카운터 증가 |
| `_errHist`, `_spikes` | `sensorTask`, `commTask` | `webTask` | `_pushErr`, `_pushSpike` 내부 `_lock()` | 링버퍼 오버플로 방지 및 인덱스 정합성 보호 |
| `_profileSwitchInProgress` | `webTask` (`switchProfile`) | `webTask`, `sensorTask` | `volatile bool` | 프로파일 전환 재진입 차단 |
| `_safeMode` / `_otaGuard` | `sensorTask`, `webTask` | `any` | `volatile bool` + `_lock()` | HID 실행 게이트 |

---

## 2. 상태머신(FSM) 상세 명세

### 2.1 Click-Freeze FSM (`_applyClickFreeze`)

물리 버튼(좌클릭)을 누를 때 손가락 반동으로 인한 커서 튐(Click-Jitter)을 방지하는 최종 게이트 FSM입니다.

| 상태 (State) | 진입 조건 | 이탈 조건 | 동작 및 부수효과 |
|---|---|---|---|
| **E10_FREEZE_IDLE** | 초기 상태, Move Gate 해제(`!_moveGateHeld`) 또는 타임아웃/이탈 | `btnDown && \|gyro\| < gyro_th && _moveGateHeld` | 정상 커서 출력 통과 (Move Gate 누름 중만 활성) |
| **E10_FREEZE_LOCKED** | IDLE에서 좌클릭 눌림 감지 | 1) 버튼 뗌 (`!btnDown`) → `HOLD` 전이<br>2) `freezeTimer > max_ms` → `IDLE`<br>3) `\|gyro\| > freeze_move_th` 또는 `!_moveGateHeld` → `IDLE` | `fx = 0`, `fy = 0` (완전 고정)<br>`freezeTimer += 8ms` |
| **E10_FREEZE_HOLD** | LOCKED 상태에서 버튼 릴리즈 | 1) `accumDistSq > thSq`, `btnDown`, `!_moveGateHeld` → `IDLE`<br>2) `holdTimer >= hold_ms` → `FADEOUT` | `fx = 0`, `fy = 0` (반동 안정화 대기)<br>`holdTimer += 8ms`, 누적 변위 추적 |
| **E10_FREEZE_FADEOUT** | HOLD 타이머 만료 | 1) `accumDistSq > thSq`, `btnDown`, `!_moveGateHeld` → `IDLE`<br>2) `fadeTimer >= fadeout_ms` → `IDLE` | `scale = fadeTimer / fadeout_ms`<br>`fx *= scale`, `fy *= scale` (선형 감쇠) |

> **활성 조건**: `cfg.enable == true` 이면서 `_moveGateHeld == true`인 경우에만 FSM 진입. Move Gate 해제 시 즉시 `IDLE` 강제 리셋.

---

### 2.2 Snap-to-Axis FSM (`_applySnapToAxis`)

직선 드래그, 표 작업, 슬라이더 조절 시 수평/수직 축을 고정(Soft Snap)해주는 FSM입니다.

| 상태 (State) | 조건 | 동작 및 부수효과 |
|---|---|---|
| **E10_SNAP_NONE** | 초기 상태 또는 큰 대각선 움직임 | 후보 축 탐색 (수평/수직 비율 및 최소 이동량 검사) |
| **후보 프레임 축적** | 한쪽 축 우세 (`\|major\| > ratio * \|minor\|`) | `candidateFrames++`, `confirm_frames` 도달 시 활성축 확정 |
| **E10_SNAP_HORIZ** | 수평 스냅 활성화 | `fy *= (1.0 - strength)` (수직 성분 소프트 감쇠) |
| **E10_SNAP_VERT** | 수직 스냅 활성화 | `fx *= (1.0 - strength)` (수평 성분 소프트 감쇠) |

> **활성 조건**: `cfg.enable == true` 이면서 `(cfg.mode_mask & (1 << (_activeMode - 1))) != 0`.  
> **이탈 조건**: 활성 축의 반대축 성분이 `\|minor\| > \|major\| / ratio_enter` 초과 시 후보 리셋.

---

### 2.3 Precision FSM (`_fsmUpdate` / `_applyPrecision`)

저속 미세 조작 시 커서 해상도를 비선형 완화하는 3단계 상태머신입니다.

| 상태 (State) | 진입 조건 | 이탈 조건 | 동작 및 부수효과 |
|---|---|---|---|
| **EN_PREC_OFF** | 정밀 모드 비활성 | 정밀 모드 설정 시 | 일반 시그모이드 가속 적용 |
| **EN_PREC_ENTRY** | 모드 진입 직후 | `gyro <= entry_still_deg` 유지 `entry_ms` 이상 → `TRACK` | 조이스틱 감쇠 완화 대기 |
| **EN_PREC_TRACK** | ENTRY 조건 만족 | `gyro >= exit_move_deg` 발생 시 → `EXIT` | 데드존 + 게인 + 스무딩 필터 적용 |
| **EN_PREC_EXIT** | 고속 움직임 감지 | `gyro <= entry_still_deg` 재안정화 시 → `TRACK` | 일반 가속으로 점진 복귀 |

---

### 2.4 Macro Sequencer FSM (`_tickMacro`)

`commTask` 루프 내에서 프레임 지연 없이 스텝별 딜레이와 실행을 제어하는 비동기 FSM입니다.

> ⚠️ **구현 참고 (개념적 서술)**: 아래 상태 이름은 문서적 이해를 돕기 위한 것이며, 실제 코드에는 **enum으로 존재하지 않는다**. 실 구현은 `_macroState.active` (bool) + `_macroState.stepIdx` (uint8) + `s.delayMs` 경과 여부로 판정한다. "WAIT_DELAY"와 "EXEC_STEP"은 `_tickMacro()` **한 호출 안에서 순차 처리**된다 (별개 프레임 아님).

| 개념 상태 | 진입 조건 | 이탈 / 전진 조건 | 동작 및 부수효과 |
|---|---|---|---|
| **INACTIVE** | 초기 상태 또는 실행 완료 | `_startMacro(idx)` 호출 | `_macroState.active == false`, 아무 동작 안 함 |
| **WAIT_DELAY** | `s.delayMs > 0` 스텝 진입 | `(now - stepStartMs) >= delayMs` | 논블로킹 대기 (커서 전송 지속) |
| **EXEC_STEP** | 딜레이 경과 또는 delay=0 | 스텝 실행 완료 시 | `_actExec.exec(step, true)` 호출, `stepIdx++` |
| **ABORTED** | 토큰 불일치(`startToken != _macroAbortToken`) | 즉시 | `active = false` 강제 종료, WARN 로그 기록 |

---

### 2.5 Macro 대체(Replace) 및 취소(Abort) 정책

매크로 실행 중 상태 변경 요청이 들어올 때의 정책입니다.

| 시나리오 | 동작 | 취소 방식 | 로그 |
|---|---|---|---|
| 실행 중 `_startMacro(newIdx)` 호출 | **조용히 대체** (이전 스냅샷 무효화) | 토큰 증가 없음, `_macroSnapshot` 덮어씀 | `WARN: macro replace: prev idx=N step=M` |
| `switchProfile` 호출 | 즉시 취소 | `_macroAbortToken++` | – |
| `_setActiveMode` 호출 | 즉시 취소 | `_macroAbortToken++` | – |
| `forceReleaseButtons()` 호출 | 즉시 취소 | `_macroAbortToken++` | – |
| BLE disconnect edge (commTask) | 즉시 취소 | `_macroAbortToken++` | – |
| SafeMode / OTA Gate 진입 | 즉시 취소 | `_macroAbortToken++` | – |
| 매크로 자연 종료 | `active = false` | stepIdx >= stepCount | `INFO: macro end: idx=N` |

> **대체 vs 취소 구분 원칙**: 사용자 의도가 "새 매크로 실행"인 경우(`_startMacro`)는 대체, "안전 정지"인 경우(프로파일 전환, 모드 전환, 게이트)는 명시적 abort. `_startMacro`가 토큰을 증가시키지 않는 이유는 새 매크로 시작 시 `_macroState.startToken = _macroAbortToken`으로 재설정되기 때문이다.

---

### 2.6 전원 관리 FSM (`sensorTask` 내부, Phase 11.6)

| 상태 | 진입 조건 | 이탈 조건 | 동작 |
|---|---|---|---|
| **ACTIVE** | 초기 / Wake 복귀 | 유휴 시간 초과 | 정상 동작, `notifyActivity()`로 타이머 리셋 |
| **LED_SUSPEND** | Sleep 조건 모두 만족 | fadeout 완료 (≤ `led_fadeout_ms + 200ms`) | `_led.suspend(snap)` — **blocking** RED fadeout → OFF (sensorTask 최대 700ms 지연) |
| **LIGHT_SLEEP** | `sleepNow()` 진입 | EXT1 Wake (MPU INT / 버튼) | `esp_light_sleep_start()` (RAM 보존) |
| **WAKE_RESUME** | Light-sleep 복귀 | Fast Recalib 완료 | `_led.resume(snap)` — **async** base fadein (FADEIN → IDLE) + `biasTracker.startFastRecalibrate(fast_recalib_ms)` |
| **DEEP_SLEEP** | `deep_idle_timeout_ms` 초과 | 버튼 Wake | `esp_deep_sleep_start()` (재부팅) |

---

### 2.7 Button Dispatcher Phase 전이 및 리셋 규약 (rev5, rev6)
`PHASE_DOUBLE`은 Long/Hold 타이머 검사에서 **제외** [R2-M-1]. 이유: Side C Double(Mode Cycle) 후 계속 hold 시 Pairing/Host Cycle이 뒤이어 오발화하는 UX 문제 방지.
| Phase | Long/Hold 검사 | Double 대기 | CLICK 발화 |
|---|:---:|:---:|:---:|
| `PHASE_IDLE` | – | – | – |
| `PHASE_PRESSED` | ✓ (`longFired`/`hold2sFired`/`hold3sFired`) | – | – |
| `PHASE_WAIT_CLICK` | – | ✓ (`double_delay_ms`) | ✓ (타임아웃 시) |
| `PHASE_DOUBLE` | **✗ (제외)** | – | – |

> **`resetButton()` 타임스탬프 초기화 규약 (rev6, BB-3)**:
> `_btnDisp.resetButton(idx)` 및 `resetAll()` 호출 시 상태 Phase(`PHASE_IDLE`) 및 발화 플래그(`longFired`, `hold2sFired`, `hold3sFired`, `downSent`)뿐만 아니라, **시간 계측 타임스탬프(`downMs = 0`, `upMs = 0`, `waitClickStartMs = 0`, `rawDownMs = 0`)를 전수 0으로 완전 초기화**한다. 이전 잔여 타임스탬프가 남아있을 경우 모드/프로파일 전환 직후 버튼 재조작 시 유령 더블클릭이나 의도치 않은 클릭 타임아웃이 발생하는 문제를 원천 방지한다.

---

## 3. 위임(Delegation) 패턴 정리

Web 태스크가 sensorTask/commTask 소유 상태를 직접 조작하지 않고 플래그로 위임하는 패턴입니다.

| 플래그 | 소유자 (Writer) | 소비자 (Consumer) | 목적 |
|---|---|---|---|
| `_reqSpecialAction` | `webTask` | `sensorTask` 루프 진입부 | Special 액션을 sensorTask 컨텍스트에서 동기 실행 |
| `_reqCommReleaseAll` | `webTask` (`switchProfile`), `any` (`forceReleaseButtons` 큐 Full 시) | `commTask` 루프 진입부 | 큐 Drop과 무관하게 HID 안전 Release 100% 보장 [R3-H-2/3] |
| `_reqResetBtnDisp` | `webTask` (`switchProfile`) | `sensorTask` 루프 진입부 | `_btnDisp.resetAll()` 위임 |
| `_reqResetGesture` | `webTask` (`switchProfile`) | `sensorTask` 루프 진입부 | `_gesture.reset()` 위임 |
| `_reqGyroCalib` | `webTask`/`any` | `sensorTask` 루프 진입부 | 캘리브레이션 + `biasTracker.reset()` 위임 |
| `_reqI2CRecover` | `webTask`/`any` | `sensorTask` 루프 진입부 | I2C 복구 위임 |
| `_reqClearDiag` | `webTask` | `sensorTask` 루프 진입부 | 진단 카운터/링버퍼 초기화 |
| `_reqSaveCfg` | `any` (BLE dirty) | `main loop` (`tickConfigSave`) | 프로파일 저장 |

> **원칙**: 위 플래그는 모두 `volatile`로 선언되며, 설정자(Writer)와 소비자(Consumer)는 서로 다른 태스크에서 실행됨. 소비자는 반드시 **flag read → 즉시 clear → 처리** 순서를 유지하여 재진입을 방지한다.


---

## 4. 개정 이력

| 버전 | 날짜 | 변경 사항 |
|---|---|---|
| rev0 | 2026-09-15 | 최초 작성 |
| rev1 | 2026-10-01 | §2.5 Macro 대체/취소 정책 신설, §2.4 Macro FSM 개념적 서술 경고, `_topMDownMs` Dead Code 표기 |
| rev4 | 2026-10-02 | LED suspend/resume FSM 상세화 (blocking/async 구분), Dead Code 정리 반영 |
| rev5 | 2026-10-02 | motion_adv config 스냅샷 락 [R2-C-1], `_reqCommReleaseAll` 확장 소비자 [R3-H-2/3], PHASE_DOUBLE hold 제외 [R2-M-1] |
| rev6 | 2026-10-07 | §2.7 `resetButton()` 타임스탬프(`downMs`, `upMs`, `waitClickStartMs`) 완전 초기화(BB-3) 및 HW_Def SSOT 반영 |
