# BUDGET.md — 시간 및 자원 예산 명세

> 대상 버전: `v0410` (ESP32-S3-Zero + MPU6050 AirMouse)  
> 최종 갱신: 2026-10-01  
> 위치: `src/v0410/docs_v0410/contract_doc/BUDGET.md`

---

## 1. 태스크 주기 및 시간 예산

FreeRTOS 태스크별 데드라인, 목표 주기 및 허용 최대 실행 시간입니다.

| 태스크명 | CPU Core | 우선순위 (Priority) | 할당 주기 (Period) | 스택 크기 | 최악 실행 시간 (Worst-Case) | 여유 시간 (Margin) |
|---|---|---|---|---|---|---|
| `_sensorTask` | Core 1 | 3 | 8 ms (125 Hz) | 8192 바이트 | ~3.8 ms (IMU I2C + 파이프라인) | 4.2 ms |
| `_commTask` | Core 0 | 2 | 7 ms (~142 Hz) | 4096 바이트 | ~2.5 ms (큐 처리 + BLE HID) | 4.5 ms |
| `_ledTask` | Core 0 | 1 | 50 ms (20 Hz) | 2048 바이트 | ~0.8 ms (WS2812 RMT 전송) | 49.2 ms |
| `main loop` | Core 0 | - | 200 ms (5 Hz) | - | ~1.5 ms (설정 저장 체크) | 198.5 ms |

---

## 2. 잠재적 블로킹 지점 및 제한 규칙

| 함수 / 기능 | 실행 컨텍스트 | 최대 블로킹 / 지연 | 시스템 영향 | 해결 및 완화 조치 |
|---|---|---|---|---|
| `_runGyroCalibration` | `sensorTask` | 1000 ms | 센서 루프 일시 정지 | 부팅 시 또는 명시적 정지 요청 시에만 실행 (동작 중 진입 금지) |
| `_recoverI2C` | `sensorTask` | ~15 ms | 센서 1~2 프레임 누락 | 하드웨어 멈춤 복구를 위한 최소 지연, 복구 후 카운터 기록 |
| `vTaskDelay` (키 스트로크 딜레이) | `commTask` (`_actExec`) | 최대 12~28 ms | 커서 프레임 전달 지연 | 매크로의 경우 `_tickMacro` 비동기 상태머신으로 완전 분리 (Zero Blocking) |
| 프로파일 I/O (`LittleFS`) | `webTask` | ~35 ms | Web API 응답 지연 | Core 0 백그라운드 처리, sensorTask와 독립 |
| Mutex 획득 (`_lock`) | `sensorTask` (`_state`) | 2 ms 제한 (`pdMS_TO_TICKS(2)`) | 초과 시 `_errMutexMiss` 증가 | 락 대기 시간 엄격 제한으로 센서 주기(8ms) 보장 |

---

## 3. 데드라인 위반 조건 및 감시 체계

1. **sensorTask Overrun**:
   - `vTaskDelayUntil`의 실제 대기 지연이 0인 경우 8ms 초과로 판정 (`_errTaskOverrun++`).
   - 10회 누적 시 `EN_E10_ERR_TASK_OVERRUN` 시스템 에러 히스토리에 기록.
2. **commTask Overrun**:
   - 프레임 간 지연(`commDt`)이 20.0ms 초과 시 `_commOverrunCount++` 증가.
3. **Queue Full Dropping**:
   - `_qActionExec`(8개 상한) 또는 `_qHidCmd`(4개 상한) 큐가 꽉 찼을 경우 즉시 Drop하여 호출 태스크 블로킹 원천 방지.
