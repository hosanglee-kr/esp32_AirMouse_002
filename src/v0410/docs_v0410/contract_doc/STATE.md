# STATE.md — 상태 소유권 및 상태머신(FSM) 명세

> 대상 버전: `v0410` (ESP32-S3-Zero + MPU6050 AirMouse)  
> 최종 갱신: 2026-10-01  
> 위치: `src/v0410/docs_v0410/contract_doc/STATE.md`

---

## 1. 상태 변수 소유권 매트릭스

각 상태 변수에 대한 단독 쓰기(Write) 권한 및 읽기/동기화 규칙을 명시합니다.

| 상태 변수 / 객체 | 단독 소유자(Writer) | 허용된 Reader | 동기화 방식 | 원칙 및 라이프사이클 |
|---|---|---|---|---|
| `_freezeState` | `sensorTask` | `sensorTask` | 독점 (No sync) | Click-Freeze 전용 내부 FSM 상태 |
| `_snapActiveAxis` | `sensorTask` | `sensorTask` | 독점 (No sync) | Snap-to-Axis 활성 축 추적 상태 |
| `_precSub` | `sensorTask` | `sensorTask` | 독점 (No sync) | Precision FSM 3단계 (ENTRY/TRACK/EXIT) |
| `_macroState` | `commTask` (start/tick) | `commTask`, `sensorTask` (power) | `volatile bool active` | 스텝 단위 매크로 상태머신 |
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

---

## 2. 상태머신(FSM) 상세 명세

### 2.1 Click-Freeze FSM (`_applyClickFreeze`)
물리 버튼(좌클릭)을 누를 때 손가락 반동으로 인한 커서 튐(Click-Jitter)을 방지하는 최종 게이트 FSM입니다.

| 상태 (State) | 진입 조건 | 이탈 조건 | 동작 및 부수효과 |
|---|---|---|---|
| **E10_FREEZE_IDLE** | 초기 상태 또는 타임아웃/이탈 | `btnDown && \|gyro\| < gyro_th` | 정상 커서 출력 통과 |
| **E10_FREEZE_LOCKED** | IDLE에서 좌클릭 눌림 감지 | 1) 버튼 뗌 (`!btnDown`) → `HOLD` 전이<br>2) `freezeTimer > max_ms` → `IDLE`<br>3) `\|gyro\| > freeze_move_th` → `IDLE` | `fx = 0`, `fy = 0` (완전 고정)<br>`freezeTimer += 8ms` |
| **E10_FREEZE_HOLD** | LOCKED 상태에서 버튼 릴리즈 | 1) `accumDistSq > thSq` 또는 `btnDown` → `IDLE`<br>2) `holdTimer >= hold_ms` → `FADEOUT` | `fx = 0`, `fy = 0` (반동 안정화 대기)<br>`holdTimer += 8ms`, 누적 변위 추적 |
| **E10_FREEZE_FADEOUT** | HOLD 타이머 만료 | 1) `accumDistSq > thSq` 또는 `btnDown` → `IDLE`<br>2) `fadeTimer >= fadeout_ms` → `IDLE` | `scale = fadeTimer / fadeout_ms`<br>`fx *= scale`, `fy *= scale` (선형 감쇠) |

---

### 2.2 Snap-to-Axis FSM (`_applySnapToAxis`)
직선 드래그, 표 작업, 슬라이더 조절 시 수평/수직 축을 고정(Soft Snap)해주는 FSM입니다.

| 상태 (State) | 조건 | 동작 및 부수효과 |
|---|---|---|
| **E10_SNAP_NONE** | 초기 상태 또는 큰 대각선 움직임 | 후보 축 탐색 (수평/수직 비율 및 최소 이동량 검사) |
| **후보 프레임 축적** | 한쪽 축 우세 (`\|major\| > ratio * \|minor\|`) | `candidateFrames++`, 기준치 도달 시 활성축 확정 |
| **E10_SNAP_HORIZ** | 수평 스냅 활성화 | `fy *= (1.0 - strength)` (수직 성분 소프트 감쇠) |
| **E10_SNAP_VERT** | 수직 스냅 활성화 | `fx *= (1.0 - strength)` (수평 성분 소프트 감쇠) |

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

| 상태 (State) | 진입 조건 | 이탈 / 전진 조건 | 동작 및 부수효과 |
|---|---|---|---|
| **INACTIVE** | 초기 상태 또는 실행 완료 | `_startMacro(idx)` 호출 | `active = false`, 아무 동작 안 함 |
| **WAIT_DELAY** | `s.delayMs > 0` 스텝 진입 | `(now - stepStartMs) >= delayMs` | 논블로킹 대기 (커서 전송 지속) |
| **EXEC_STEP** | 딜레이 경과 또는 delay=0 | 스텝 실행 완료 시 | `_actExec.exec(step)` 호출, `stepIdx++` |
| **ABORTED** | 토큰 불일치(`startToken != _macroAbortToken`) | 즉시 | `active = false` 강제 종료, 로그 기록 |
