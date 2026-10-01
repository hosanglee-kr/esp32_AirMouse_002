# BUDGET.md — 시간 및 자원 예산 명세

> 대상 버전: `v0410` (ESP32-S3-Zero + MPU6050 AirMouse)  
> 위치: `src/v0410/docs_v0410/contract_doc/BUDGET.md`
> 최종 갱신: 2026-10-02 (rev4 — LED suspend blocking 추가)  

---

## 1. 태스크 주기 및 시간 예산

FreeRTOS 태스크별 데드라인, 목표 주기 및 허용 최대 실행 시간입니다.

| 태스크명 | CPU Core | 우선순위 (Priority) | 할당 주기 (Period) | 스택 크기 | 최악 실행 시간 (Worst-Case) | 여유 시간 (Margin) |
|---|---|---|---|---|---|---|
| `_sensorTask` | Core 1 | 3 | 8 ms (125 Hz) | 8192 바이트 | ~3.8 ms (IMU I2C + 파이프라인) | 4.2 ms |
| `_commTask` | Core 0 | 2 | 7 ms (~142 Hz) | 4096 바이트 | ~2.5 ms (큐 처리 + BLE HID) | 4.5 ms |
| `_ledTask` | Core 0 | 1 | 50 ms (20 Hz) | 2048 바이트 | ~0.8 ms (WS2812 RMT 전송) | 49.2 ms || `main loop` | Core 0 | - | 200 ms (5 Hz, `main.cpp` 확인됨) | - | ~1.5 ms (설정 저장 체크) | 198.5 ms |
| `main loop` | Core 0 | - | 200 ms (5 Hz, `main.cpp` 확인됨) | - | ~1.5 ms (설정 저장 체크) | 198.5 ms |
| `setup()` | Core 0 | - | 1회 (부팅) | - | ~6.5 초 (worst: boot key hold 시) | – |

> **참고**: `main loop` 주기는 `main.cpp`의 `delay(200)`로 확인됨. `tickConfigSave()`가 main loop에서 호출되어 BLE dirty 플래그를 소비한다. `setup()`은 부팅 시 1회 실행되며, 최악의 경우 Side C 6초 hold(Fatory Reset 트리거)로 ~6.5초 소요.

---

## 2. 잠재적 블로킹 지점 및 제한 규칙

| 함수 / 기능 | 실행 컨텍스트 | 최대 블로킹 / 지연 | 시스템 영향 | 해결 및 완화 조치 |
|---|---|---|---|---|
| `_runGyroCalibration` | `sensorTask` | 1000 ms (`CALIB_MS`) | 센서 루프 일시 정지, 캘리브 중 `_ble.tick()`만 유지 | ① 부팅 1회 (`!_gyroCalibDone`) ② `_reqGyroCalib` 플래그 수신 시. 블로킹 중 버튼 상태는 프레임에 반영하나 이동/휠은 0 |
| `_recoverI2C` | `sensorTask` | ~15 ms | 센서 1~2 프레임 누락 | 하드웨어 멈춤 복구를 위한 최소 지연, 복구 후 카운터 기록 |
| `vTaskDelay` (키 스트로크 딜레이) | `commTask` (`_actExec`) | 최대 12~28 ms | 커서 프레임 전달 지연 | 매크로의 경우 `_tickMacro` 비동기 상태머신으로 완전 분리 (Zero Blocking) |
| 프로파일 I/O (`LittleFS`) | `webTask` | ~35 ms | Web API 응답 지연 | Core 0 백그라운드 처리, sensorTask와 독립 |
| Mutex 획득 (`_lock`) | `sensorTask` (`_state`) | 2 ms 제한 (`pdMS_TO_TICKS(2)`) | 초과 시 `_errMutexMiss` 증가 | 락 대기 시간 엄격 제한으로 센서 주기(8ms) 보장 |
| `_holdAtBoot(E10_CONST::PIN_BTN_MODE, 6000)` | `setup()` (부팅 1회) | 최대 6000 ms | 부팅 지연 (런타임 무관) | Factory Reset 트리거. 조기 릴리즈 시 즉시 탈출 (`digitalRead != LOW` → return false) |
| `_led.suspend(snap)` | `sensorTask` (sleep 진입) | 최대 ~700 ms (`led_fadeout_ms + 200ms`) | 센서 루프 지연, 이어서 즉시 sleep 진입 | LED RED fadeout 완료 대기. deadline 초과 시 강제 OFF. `_ledTask`(Core 0) 병렬 tick |

---

## 3. 데드라인 위반 조건 및 감시 체계

1. **sensorTask Overrun**:
   - `vTaskDelayUntil`의 실제 대기 지연이 0인 경우 8ms 초과로 판정 (`_errTaskOverrun++`).
   - 10회 누적 시 `EN_E10_ERR_TASK_OVERRUN` 시스템 에러 히스토리에 기록.
2. **commTask Overrun**:
   - 프레임 간 지연(`commDt`)이 20.0ms 초과 시 `_commOverrunCount++` 증가.
3. **Queue Full Dropping**:
   - `_qActionExec`(8개 상한) 또는 `_qHidCmd`(4개 상한) 큐가 꽉 찼을 경우 즉시 Drop하여 호출 태스크 블로킹 원천 방지.

---

## 4. 큐 용량 및 오버플로 정책 (참조)

| 큐 식별자 | 용량 | Producer | Consumer | 오버플로 정책 |
|---|---|---|---|---|
| `_qFrame` | 1 (`ST_E10_Frame_t`) | `sensorTask` | `commTask` | `xQueueOverwrite` (최신 프레임 유지) |
| `_qActionExec` | 8 (`ST_ActionCmd_t`) | `sensorTask`, `webTask` | `commTask` | `xQueueSend(timeout=0)` 실패 시 drop |
| `_qHidCmd` | 4 (`ST_E10_HidCmd_t`) | `webTask`, `sensorTask` | `commTask` | `xQueueSend(timeout=0)` 실패 시 drop |

---

## 5. 상태 스냅샷 통계 (참고)

`getStatus()`가 노출하는 관측성 카운터:

| 필드 | 의미 |
|---|---|
| `sensor_dt_max_ms` | sensorTask 최대 프레임 간격 |
| `sensor_overrun_count` | 16ms 초과 누적 |
| `comm_dt_avg_ms` / `comm_dt_max_ms` | commTask dt 통계 |
| `comm_overrun_count` | commTask 20ms 초과 누적 |
| `task_stack_sensor_min_words` / `task_stack_comm_min_words` | 스택 high-watermark |
| `failsafe_release_count` | SafeMode/OTA/연결해제로 인한 강제 release |
| `err_mutex_miss` | `_state` 락 2ms 타임아웃 |
| `err_task_overrun` | sensorTask overrun 누적 |
