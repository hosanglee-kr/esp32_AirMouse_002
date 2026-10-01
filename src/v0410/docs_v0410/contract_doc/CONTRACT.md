# CONTRACT.md — 모듈 경계 및 인터페이스 계약

> 대상 버전: `v0410` (ESP32-S3-Zero + MPU6050 AirMouse)  
> 최종 갱신: 2026-10-01  
> 위치: `src/v0410/docs_v0410/contract_doc/CONTRACT.md`

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
| `_startMacro(idx)` | `commTask` only | 논블로킹 (상태머신 초기화) | `_macroSnapshot` 갱신, `_macroState` 활성화 | 인덱스 범위 초과 시 로그 후 Drop |
| `_tickMacro()` | `commTask` only | 매 프레임 논블로킹 호출 | 딜레이 경과 시 액션 실행, 스텝 전진 | 토큰 불일치(`_macroAbortToken`) 시 즉시 중단 |
| `_resolveSlot(mode, trig)` | `sensorTask`, `commTask`, `webTask` | Read-only | 없음 (Global + Mode Override O(1) 해석) | 비정상 인덱스 시 `EN_C20_ACT_NONE` 반환 |
| `switchProfile(idx)` | `webTask` only | 진행 중 재진입 차단 (`_cfgProfileValid`) | `_macroAbortToken++`, 큐 드레인, 리셋 위임, HID release | 전환 중이면 `false` 반환 및 조기 종료 |
| `_handleSpecial(special)` | `sensorTask` only | 타 태스크 호출 금지 | 모드 변경, 페어링 시작, 캘리브레이션 요청 등 | 알 수 없는 코드 시 무시 |
| `forceReleaseButtons()` | `any` (web, sensor, comm) | 논블로킹 | `_qHidCmd`에 `RELEASE_ALL` 인큐 | 큐 Full 시 drop (false) |
| `_applyClickFreeze(...)` | `sensorTask` only | 파이프라인 최종단 | `_freezeState` 전이 및 커서 좌표 0 클램프 | 없음 |
| `_applySnapToAxis(...)` | `sensorTask` only | Click-Freeze 직전 | 활성 축 외 성분 감쇠(Soft Snap) | 미활성화 시 원본 유지 |
| `_recoverI2C()` | `sensorTask` only | I2C 버스 재초기화 | `Wire.end()`, `Wire.begin()`, `_mpu.begin()` | 실패 시 `_consecutiveRecoverFail++` |
| `_runGyroCalibration()` | `sensorTask` only | 초기 부팅 또는 정지 시 1000ms 측정 | 자이로 오프셋 산출, `_gyroCalibDone = true` | 측정 중 큰 움직임 시 재시도 |

---

## 3. FreeRTOS 큐 계약

| 큐 식별자 | Producer 태스크 | Consumer 태스크 | 용량(Size) | 타임아웃 | 오버플로 / 경합 정책 |
|---|---|---|---|---|---|
| `_qFrame` | `sensorTask` | `commTask` | 1 (`ST_E10_Frame_t`) | 0 (Overwrite) | `xQueueOverwrite`: 지연 없이 항상 최신 모션 프레임 유지 |
| `_qActionExec` | `sensorTask`, `webTask` | `commTask` | 8 (`ST_ActionCmd_t`) | 0 | Full 시 즉시 drop (커서 및 센서 파이프라인 지연 방지) |
| `_qHidCmd` | `webTask`, `sensorTask` | `commTask` | 4 (`ST_E10_HidCmd_t`) | 0 | Full 시 즉시 drop (`false` 리턴) |

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
