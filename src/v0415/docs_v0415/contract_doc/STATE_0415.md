# STATE_0415.md — 상태 소유권 및 상태머신(FSM) 명세

> 대상 버전: `v0415` (ESP32-S3-Zero + MPU6050 AirMouse)  
> 위치: `src/v0415/docs_v0415/contract_doc/STATE_0415.md`
> 최종 갱신: 2026-10-14 (rev16 — Multi-Host Reject-Until-Target FSM 및 _reqHostOp 위임 소유권 명세)

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
| **`_diag`** (ST_E10_DiagCounters) | `sensorTask` (write), `_reqClearDiag` 시 `_diag.reset()` | `getStatus` (read) | `_lock()` 스냅샷 (관측용) | **rev18**: 8개 진단 카운터 통합 (`errMpuNan` / `errMutexMiss` / `errTaskOverrun` / `i2cRecoverCount` / `i2cRecoverLastOk` / `consecutiveFail` / `consecutiveRecoverFail` / `consecutiveI2cFail`). 루프 내 단조 증가는 무경합 원자적 갱신, 스냅샷 읽기는 `_lock()` 동기화 |
| **`_commPrevConn`** | `commTask` | `commTask` | 독점 (No sync) | **rev18**: 이전 루프의 BLE 연결 상태 추적 (로컬 static → 멤버 승격) |
| **`_commReleasedOnSafe`** | `commTask` | `commTask` | 독점 (No sync) | **rev18**: Safe/OTA Gate 진입 시 1회성 release 추적 |
| **`_connEdgePrev`** | `sensorTask` (`_notifyConnectionEdge`) | `sensorTask` | 독점 (No sync) | **rev18**: BLE 연결 edge 감지용 상태 추적 (로컬 static → 멤버 승격) |
| **`s_corruptCount`** (D10) | `_pushToRing` (Phase 2) | `getCorruptCount()` | `volatile` | **rev18**: Phase 4-A 슬롯 무결성 관측 (정상 시 0) |
| `NimBLEDevice::isInitialized()` | `B20::begin()` | `B20::getBondCount()` 등 | NimBLE 내부 mutex | 최대 500ms 동기화 대기. 미초기화 시 API 접근 차단(NULL Mutex 방지) |
| `_biasTracker` | `sensorTask` | `sensorTask` | 독점 (No sync) | 정지 상태 감지 및 자이로 바이어스 보정 |
| `_gesture` | `sensorTask` | `sensorTask` | 독점 (No sync) | Flick/Linear/Tilt 제스처 감지기 |
| `_btnDisp` | `sensorTask` | `sensorTask` | 독점 (No sync) | 물리 버튼 디바운스 및 더블/롱클릭 이벤트 |
| `_actExec` | `commTask` | `commTask` | 독점 (No sync) | HID 키/마우스 상태 유지 및 반복 액션 관리 |
| `_led` | `_ledTask` (tick) | `any` (setModeColor, flash 등) | **Recursive Mutex** (v0415) | 50ms 주기 tick은 `_ledTask` 단독. 상태 변경 API는 mutex 보호 |
| `_power` | `sensorTask` | `sensorTask` | 독점 (No sync) | Light/Deep-sleep 진입 및 WoM 인터럽트 처리 |
| `_bootMutex` (C10) | `C10` | `C10` | `SemaphoreHandle_t` | `boot.json` 읽기/쓰기/SafeMode 클리어 전역 동기화 |
| `_dirsEnsured` (C10) | `C10::begin()` | `C10` | 독점 (No sync) | `/json`, `/json/profiles` 디렉토리 생성 완료 플래그 캐시 |
| `s_buffer[64]` (D10) | `_pushToRing` (Phase 2) | (write-only) | Two-Phase Locking | critical 외부 96B 복사 (무경합 슬롯 write) |
| `s_head`/`s_count` (D10) | `_pushToRing` (Phase 1) | `_pushToRing` (Phase 1) | `portENTER_CRITICAL` | 링버퍼 인덱스 할당 임계구역 |
| `s_overwriteCount`/`s_truncCount` (D10) | `_pushToRing` | `getOverwriteCount`/`getTruncCount` | critical / volatile | 오버라이트 및 메시지 잘림 카운터 |
| `_logLevel` (D10) | `setLevel` | `log` | `volatile` | 런타임 로그 레벨 |
| **`_reqHostOp`** | `webTask` (`/api/hosts/select`, `cancel`) | `sensorTask` (루프 헤드) | `volatile EN_C20_ReqHostOp_t` | **rev16: Last-Write-Wins 위임 (Web의 BLE 직접 제어 배제)** |
| **`_advMode`** (B20) | `sensorTask` (`selectHostIdx`, `cancelSelect`, `tick`) | `sensorTask`, `webTask` (`getHostState`) | `volatile EN_B20_AdvMode_t` | **rev16: IDLE / CYCLE / TARGET 광고 FSM** |
| **`_targetPeerIdx`** (B20) | `sensorTask` | `sensorTask`, `webTask` | `volatile uint8_t` | **rev16: 목표 호스트 인덱스 (0~2 또는 0xFF)** |
| **`_advModeStartMs` / `_advModeTimeoutMs`** (B20) | `sensorTask` | `sensorTask` (`tick`) | `volatile uint32_t` | **rev16: Strict 10초 타임아웃 추적** |
| **`last_host_addr[18]`** (C10 Boot) | `sensorTask` (BLE 연결 시) | `sensorTask` (부팅 우선 시도), `webTask` | `_bootMutex` | **rev16: 부팅 시 마지막 기기 우선 연결 주소** |
| **`hosts_0415.json`** (C10) | `webTask` (`/api/hosts/label` 경유) | `webTask` (`/api/hosts`), `C10` | `_lock()` + 3-tier rotate | **rev16: 호스트 라벨/색상 영구 저장소** |
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
| `_safeMode` / `_otaGuard` | `sensorTask`, `webTask` | `any` | `volatile bool` + `_lock()` | HID 실행 게이트. SafeBoot 시 SoftAP SSID `-SAFE` 표기 일원화 |
| `_thSensor` / `_thComm` / **`_thLed`** | `begin()` (task create) | `_onPowerWake`, `getStatus` | 읽기 전용 (관측) | **v0415: `_thLed` 추가** (스택 워터마크 관측) |
| **`_stackLedMinWords`** (E10) | `_ledTask` (50ms) | `getStatus` | 독점 (No sync) | **rev13 (B-2)**: LED 태스크 워터마크. `_thLed` 저장과 함께 완결 |
| **`_bootWifi`** (W10) | `CL_W10_WebConfig::begin()` | `_bumpRebootMaskFromBoot` | 독점 (No sync) | **v0415: W10 부팅 WiFi 스냅샷 (되돌림 감지)** |
| `sta_ssid`, `sta_pass` (W10/C10) | `C10::makeDefaultsWiFi()` / `W10` | `CL_W10_WebConfig::begin()` | 독점 (No sync) | **rev12: 실 자격증명 embed 제거 → 기본값 공백("")**. 빈 문자열 시 8초 STA 연결 시도 생략 즉시 SoftAP 구동. 사용자가 UI에서 명시 입력 시에만 STA 시도 |
| `_taskReboot` (W10) | `webTask` (`apiRebootPost`) | FreeRTOS Scheduler | `BaseType_t` 반환값 검증 | **rev14 (B-4)**: 태스크 생성 실패 시 동기식 `ESP.restart()` 안전 폴백 |

### 1.1 v0415 주요 변경

- **삭제 필드**: `_isPptMode` (E10), `_modeToggleCooldownMs`, `_lastModeToggleMs`, `_topMDownMs` (Dead), **`_state.btn_mask`** (R-2 LC-D), **`ST_E10_Status_t.btn_mask`** (R-2 LC-D)
- **삭제 심볼**: `setPptMode()` (R-1), `buildWakeMaskAll()` (R-2 LC-F), `_detectLinear(p_gx, p_gy, ...)` 시그니처 (R-3 LC-G)
- **추가 필드**: `_thLed`, `ST_E10_Status_t::active_mode`, `ST_E10_Status_t::task_stack_led_min_words`, `_bootWifi` (W10), `_mpuAddr` (동적 감지 0x68/0x69), `_consecutiveI2cFail` (E10), `_bootMutex` (C10), `_dirsEnsured` (C10), `_serialMutex` (D10), `_dropQueueCount` 외 관측 카운터 (D10)
- **정책 변경**: `_macroState` → `_lock()` 하 원자적 갱신 (v0412는 부분 volatile)
- **L10 thread-safety**: Recursive Mutex (v0412는 무보호)
- **`_fsm` 판정**: `_isPptMode` → `_activeMode == 2` (v0415 `_fsmUpdate(gyroAbs)` 단일 param)
- **마이그레이션 정책**: v410→v411 로드 시 POWER→KB_POWER(0x66), TV_INPUT→WWW_SEARCH, 미지원 코드 NONE 치환 및 validate 통과 즉시 `saveProfile()` Eager Persist

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

**v0415 Safe/Pairing 분기 및 센서 복원**:
- `sleepNow(now, safeOrPairing)` — Safe 또는 Pairing 시 `buildWakeMaskSafe()`
- `deepSleepNow(now, hid, pairing, safe)` — Safe 시 Side C 단독, Pairing 시 sleep 금지
- **Wake 직후 I2C 복원**: 슬립 복귀 시 `Wire.end()` / `Wire.begin()` 후 `_restoreMpuAfterWake(true)`를 호출하여 MPU6050 전원/WoM 레지스터를 즉시 정상화

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
- **Deep Sleep 전제조건 (v0415 하드닝)**: `deepSleepNow` 진입 전 `_led.suspend(snap)` 호출 필수 (WS2812 래칭 차단).

> **[rev13] 스택 관측**: `_ledTask` 매 tick 종료 시 `uxTaskGetStackHighWaterMark(nullptr)`
> 호출 → `_stackLedMinWords` 단조 최소 갱신 → `getStatus().task_stack_led_min_words`.

---

### 2.9 D10 Logger 상태머신 (Two-Phase Locking)

| 위상 | 보호 수단 | 대상 리소스 | 실행 특성 | 동작 내용 |
|---|---|---|---|---|
| **Phase 1: Index 확보** | `portENTER_CRITICAL(&s_mux)` | `s_head`, `s_count`, `s_overwriteCount` | ISR-safe 초단기 spinlock | 고유 슬롯 인덱스(`v_idx`) 예약 할당 및 오버라이트 판정 |
| **Phase 2: 슬롯 Write** | 없음 (고유 인덱스 격리) | `s_buffer[v_idx]` (96B) | Lock-Free 무경합 | critical 외부에서 메시지 복사 및 `s_truncCount++` (잘림 방어) |
| **Serial 출력** | 스택 버퍼 단일 조립 | 하드웨어 UART / USB CDC | 단일 I/O 전송 | 256B 스택 버퍼에 라인 전체 조립 후 `_serial->write()` 1회 호출 (인터리빙 방지) |

- Task 전용 (ISR/타이머 콜백 호출 금지).


---

### 2.10 C10 Boot State FSM (Mutex 보호)

- `_bootMutex` (`SemaphoreHandle_t`): `getBootState()`, `_saveBootState()`, `clearSafeMode()`, `bootMarkOkIfGracePassed()` 실행 시 단독 획득.
- `boot.json` 회전 저장 (`BOOT_TMP_PATH` -> `BOOT_PATH` / `BOOT_OLD_PATH`) 중 다중 태스크 경쟁 상태 원천 차단.

---

### 2.11 Multi-Host Reject-Until-Target FSM (B20 BLE Manager)

다중 호스트 환경에서 사용자가 지정한 단일 목표 호스트만 연결되도록 비대상 기기의 접속을 강제 거절하는 광고 상태머신입니다.

| 상태 (Enum) | 진입 조건 | 동작 내용 | 이탈 조건 및 다음 상태 |
|---|---|---|---|
| **EN_B20_ADV_IDLE** (0) | 부팅 완료, 타임아웃 만료, 취소 요청, 타깃 연결 완료 | 일반 BLE 광고 및 임의 호스트 접속 허용 | `selectHostIdx()` / `selectHostAddr()` 호출 시 → **TARGET** |
| **EN_B20_ADV_CYCLE** (1) | Side C 3초 Hold (`cycleActivePeer()`) | 80ms Settle 후 다음 인덱스로 전환 광고 | 인덱스 지정 완료 시 → **TARGET** |
| **EN_B20_ADV_TARGET** (2) | `selectHostIdx()`, 부팅 `last_host_addr` 우선 연결 | 1) `isSameBleAddress()` 비교<br>2) 비대상 접속 시 0x16 즉시 Disconnect<br>3) Sleep Gate 활성화 (절전 유예) | 1) 목표 호스트 접속 성공 → **IDLE**<br>2) 10초 타임아웃 만료 (Option B: Strict Stop) → **IDLE** (적색 3회 점멸)<br>3) `cancelSelect()` 호출 → **IDLE** |

---

## 3. 위임(Delegation) 패턴 정리

Web 태스크가 sensorTask/commTask 소유 상태를 직접 조작하지 않고 플래그로 위임하는 패턴입니다.

| 플래그 | 소유자 (Writer) | 소비자 (Consumer) | 목적 |
|---|---|---|---|
| **`_reqHostOp`** | `webTask` (`/api/hosts/select`, `cancel`) | `sensorTask` 루프 진입부 | Web API의 다중 호스트 전환 요청을 sensorTask에서 안전하게 소비 (신규 큐 미생성) |
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
| rev8 | 2026-10-11 | **R-1~R-3 통합 patch**: `_state.btn_mask` / `ST_E10_Status_t.btn_mask` 필드 삭제 (R-2 LC-D), `setPptMode()` 삭제 (R-1), `buildWakeMaskAll()` alias 삭제 (R-2 LC-F). **§2.7 resetButton 규약 rev7로 갱신**: downMs/upMs/waitClickStartMs = `millis()` (0 아님, L5-A3-11). **DOUBLE DOWN 흡수** + **LONG/HOLD progressive** + **update() 5~10ms 전제조건** 명시. HW_Def 핀 재배치 (GP7~GP13) 반영. |
| rev9 | 2026-10-11 | **I2C/NimBLE/SoftAP/마이그레이션 소유권 정합**: `_mpuAddr` 동적 감지(0x68/0x69) 및 `_power.setMpuAddr` 전파, 런타임 pre-probe/10회 복구 소유권, 슬립 복귀 `_restoreMpuAfterWake` 복원, `NimBLEDevice::isInitialized()` 동기화 대기 및 `getBondCount()` 0 반환 소유권, SoftAP `sta_ssid` 공백 시 즉시 시작 및 SafeBoot `-SAFE` 상태 일원화, v410→v411 `saveProfile()` Eager Persist 정책 반영. |
| rev11 | 2026-10-11 | **execLiveTest Special 액션 BLE 독립 소유권 및 스토리지 무결성**: `execLiveTest` 내 `EN_C20_ACT_SPECIAL` 판정부를 `_hid.isConnected()` 앞으로 이동하여 Wi-Fi AP 단독 상태에서도 모드 전환/영점/절전의 `_reqSpecialAction` 위임 쓰기 권한 보장. LittleFS `_atomicWriteJson` 및 `begin()`의 `/json/profiles` 디렉토리 생성 보장으로 프로파일 쓰기 소유권 정상화. `makeDefaultsWiFi`에 공유기 자격증명(`NetE89C` / `38D#4FP001`) Fallback 소유권 반영. |
| rev12 | 2026-10-12 | **D10 Two-Phase 뮤텍스 소유권 & C10 _bootMutex 및 딥슬립 LED 차단 상태 전이**: §1 매트릭스에 `_consecutiveI2cFail`, `_bootMutex`, `_dirsEnsured`, D10 `_buf` 및 `_serialMutex`, D10 원자적 카운터 소유권 추가. `sta_ssid/pass` 실 자격증명 제거 및 기본값 공백("") 규약 확정. C10 `_loadIndex` 하위 호환 폴백(`active`/`count`) 및 `active_profile.json` 프로파일 2종(`Default`/`Office`) 동기화. §2.8 Deep Sleep 진입 직전 `_led.suspend(snap)` WS2812 래칭 차단 명시. §2.9 D10 Logger Two-Phase Locking FSM 신설. §2.10 C10 Boot State Mutex FSM 신설. |
| rev13 | 2026-10-12 | **LED 태스크 스택 관측 완결 (B-2)**: `_stackLedMinWords` 소유권 등록. §2.8 L10 LED FSM에 `_ledTask` 관측 훅 명시. |
| **rev14** | **2026-10-12** | **W10 Reboot 태스크 소유권 방어 (B-4) 및 D10 로깅 상태 전이 명세**: `_taskReboot` 생성 반환값(`pdPASS`) 검증 및 실패 시 동기식 `ESP.restart()` 안전 폴백 소유권 등록. D10 로거는 상태 전이 및 치명적 에러 한정 호출 규약 동기화. |
| **rev15** | **2026-10-12** | **스키마 412 gesture 삭제 + Wi-Fi UI 저장 소유권**: §1 매트릭스에서 `_gestureFlickDeg`/`_gestureCooldownMs` 삭제 → `flick.p2p_th`/`flick.cooldown_ms` 단독 소유 명시. `cfgSave()` patch 확장(`{e10, wifi}`)에 따른 W10 `_wifi` 로컬 캐시 갱신 소유권 (§2.13 하위) 반영. |
| **rev16** | **2026-10-14** | **Multi-Host Reject-Until-Target FSM 및 _reqHostOp 위임 소유권 명세**: §1 매트릭스에 `_reqHostOp`, `_advMode`, `_targetPeerIdx`, `_advModeStartMs`, `last_host_addr`, `hosts_0415.json` 소유권 추가. §2.11 Multi-Host FSM(IDLE/CYCLE/TARGET) 신설. §3 위임 패턴에 `_reqHostOp` 등록. |
| **rev18** | **2026-10-15** | **Phase 1·2·3·4 소유권 반영**: §1 매트릭스에 `_diag` 그룹화 (8개 카운터 통합), `_commPrevConn` / `_commReleasedOnSafe` / `_connEdgePrev` (comm/sensor 상태 멤버 승격), `s_corruptCount` (D10 관측) 추가. |
