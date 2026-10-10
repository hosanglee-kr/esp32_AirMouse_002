# BUDGET_0415.md — 시간 및 자원 예산 명세

> 대상 버전: `v0415` (ESP32-S3-Zero + MPU6050 AirMouse)
> 위치: `src/v0415/docs_v0415/contract_doc/BUDGET_0415.md`
> 최종 갱신: 2026-10-08 (rev6 — v0415 리팩터 반영: Safe Wake Mask, B20 HOST_CYCLE, 매크로 step 블로킹, L10 mutex)

---

## 1. 태스크 주기 및 시간 예산

FreeRTOS 태스크별 데드라인, 목표 주기 및 허용 최대 실행 시간입니다.

| 태스크명 | CPU Core | 우선순위 | 할당 주기 | 스택 크기 | 최악 실행 시간 (Worst-Case) | 여유 |
|---|---|---|---|---|---|---|
| `_sensorTask` | Core 1 | 3 | 8 ms (125 Hz) | 8192 B | ~3.8 ms (IMU I2C + 10단계 파이프라인) | 4.2 ms |
| `_commTask` | Core 0 | 2 | 7 ms (~142 Hz) | 4096 B | ~2.5 ms (큐 처리 + BLE HID) | 4.5 ms |
| `_ledTask` | Core 0 | 1 | 50 ms (20 Hz) | 2048 B | ~0.8 ms (WS2812 RMT + mutex) | 49.2 ms |
| `AsyncWebServer` | (ESP-IDF 내부) | – | 이벤트 | – | (HTTP body 당 최대 ~30 ms) | – |
| `main loop` | Core 0 | – | 200 ms (5 Hz) | – | ~1.5 ms (boot grace + tickConfigSave) | 198.5 ms |
| `setup()` | Core 0 | – | 1회 (부팅) | – | ~6.5 초 (worst: boot key hold) | – |

> **참고**: `main loop`는 `main.cpp`의 `delay(200)`로 확인. `setup()` 최악 ~6.5초 = C10 begin(~0.3초) + Side C 6초 hold(Fatory Reset) + 모듈 초기화.

---

## 2. 잠재적 블로킹 지점 및 제한 규칙

### 2.1 sensorTask 컨텍스트

| 함수 / 기능 | 최대 블로킹 | 시스템 영향 | 완화 조치 |
|---|---|---|---|
| `_runGyroCalibration` | 1000 ms (`CALIB_MS`) | 센서 루프 일시 정지, 커서 1초 정지 | ① 부팅 1회 ② `_reqGyroCalib` 수신 시. 루프 내 `_ble.tick()` 유지 |
| `_recoverI2C` | ~15 ms | 센서 1~2 프레임 누락 | 하드웨어 멈춤 복구 최소 지연 |
| **`_handleSpecial(SLEEP_NOW)`** | **~700 ms + sleep (wake까지)** | **sensorTask blocking (wake 후 복귀)** | **v0415 Q4-a: sensorTask 컨텍스트 blocking 허용** (sleep 진입 자체가 목적) |
| `_led.suspend(snap)` | ~700 ms (`_fadeoutMs + 200`) | sleep 진입 전 LED fadeout | LED RED fadeout 완료 대기. deadline 초과 시 강제 OFF |
| `_power.sleepNow(now, safeOrPair)` | (wake까지 블로킹) | sensorTask 정지 | `esp_light_sleep_start()` 리턴까지. wake 후 자동 복귀 |
| `_power.deepSleepNow(now, hid, pair, safe)` | (reboot까지) | deep-sleep 진입 | `esp_deep_sleep_start()`는 리턴 안 함 |
| **`_handleSpecial(HOST_CYCLE)`** | **~110 ms** | **sensorTask 14프레임 누락 (커서 112ms 정지)** | **B20 내부 delay(80) + delay(30) — v0415 문서화** (사용자 트리거이므로 즉시성 요구 낮음) |
| `_applyClickFreeze` / `_applySnapToAxis` | ≤ 10 μs | 없음 | config 스냅샷 read (_lock 하) |
| `_lock()` (state read) | ≤ 2 ms (`pdMS_TO_TICKS(2)`) | 초과 시 `_errMutexMiss++` | recursive mutex + 짧은 critical section |

### 2.2 commTask 컨텍스트

| 함수 / 기능 | 최대 블로킹 | 시스템 영향 | 완화 조치 |
|---|---|---|---|
| `_actExec.exec()` 단일 호출 | ~28 ms (`_consumerTapMs`) | 커서 프레임 소비 지연 | 단발 액션은 프레임당 상한 4개 (아래) |
| **`_qActionExec` 드레인 (4개 상한)** | **최대 4 × 28 = 112 ms** | **커서 지연** | **v0415: 프레임당 상한 4 유지** (BUDGET 명시) |
| `_tickMacro()` step 실행 | step당 최대 28 ms | 매크로 진행 지연 | delay는 논블로킹, **step 실행만 블로킹** (아래 2.3) |
| `_actExec.tickRepeat()` | ~28 ms × N (MAX_RUNTIME=8) | 반복 액션 처리 | 최대 8 × 28 = 224 ms (극히 rare) |
| `_qFrame` receive | ≤ 1 프레임 | (Overwrite size=1) | 논블로킹 |
| `vTaskDelay` (HID tap) | 20~28 ms | – | _kbTapMs=22, _consumerTapMs=28 |

### 2.3 매크로 step 실행 정책 (v0415 명확화)

SPEC §"매크로 실행 (비동기 상태머신)"의 "블로킹 없음" 표현은 **delay에 한정**:

| 구간 | 블로킹 여부 | 근거 |
|---|---|---|
| `_startMacro` | 논블로킹 | 스냅샷만, 즉시 리턴 |
| `_tickMacro` delay 대기 | 논블로킹 | 다음 tick까지 리턴 |
| **`_tickMacro` step 실행 (`_actExec.exec`)** | **블로킹 (~28ms)** | HID tap 물리적 지속 시간 |
| `_tickMacro` 종료 | 논블로킹 | active=false |

**결론**: 매크로 스텝 8개 × 28ms = 최대 224 ms의 누적 블로킹 (분산 실행). 커서 프레임 소비는 각 step 사이에 계속됨.

### 2.4 Web/기타 컨텍스트

| 함수 / 기능 | 최대 블로킹 | 시스템 영향 | 완화 조치 |
|---|---|---|---|
| 프로파일 I/O (LittleFS) | ~35 ms | Web API 응답 지연 | Core 0 백그라운드, sensorTask 독립 |
| `_setupWiFi` STA 시도 | **~8 초 (blocking)** | **setup() 지연** | v0415: BUDGET 명시 (STA 실패 시 AP fallback) |
| `_holdAtBoot(Side C, 6000)` | ~6000 ms | 부팅 지연 | 조기 릴리즈 시 즉시 탈출 |
| `_resetProfile` (Factory) | ~500 ms | FS 재생성 | 재부팅 유도 |

---

## 3. 데드라인 위반 조건 및 감시 체계

1. **sensorTask Overrun**:
   - `vTaskDelayUntil` 실제 대기 지연 0 → 8ms 초과 판정 (`_errTaskOverrun++`)
   - 10회 누적 시 `EN_E10_ERR_TASK_OVERRUN` 히스토리 기록
2. **commTask Overrun**:
   - 프레임 간 지연(`commDt`) > 20.0ms → `_commOverrunCount++`
3. **Queue Full Dropping**:
   - `_qActionExec`(8) / `_qHidCmd`(4) Full 시 즉시 Drop (호출 태스크 블로킹 원천 방지)
4. **HID RELEASE_ALL 안전망**:
   - `_reqCommReleaseAll` 위임으로 큐 Full 상태에서도 release 100% 보장

---

## 4. 큐 용량 및 오버플로 정책

| 큐 식별자 | 용량 | Producer | Consumer | 오버플로 정책 |
|---|---|---|---|---|
| `_qFrame` | 1 (`ST_E10_Frame_t`) | sensorTask | commTask | `xQueueOverwrite` (최신 프레임 유지) |
| `_qActionExec` | 8 (`ST_ActionCmd_t`) | sensorTask, webTask | commTask | `xQueueSend(timeout=0)` 실패 시 drop |
| `_qHidCmd` | 4 (`ST_E10_HidCmd_t`) | webTask, sensorTask | commTask | `xQueueSend(timeout=0)` 실패 시 drop |

**v0415 변경**:
- `_qHidCmd` 메시지에서 `holdMs` 필드 삭제 (Round G, TEST_CLICK 삭제 이후 Dead)

---

## 5. 상태 스냅샷 통계 (관측성)

`getStatus()` 노출 카운터:

| 필드 | 의미 |
|---|---|
| `sensor_dt_max_ms` | sensorTask 최대 프레임 간격 |
| `sensor_overrun_count` | 16ms 초과 누적 |
| `comm_dt_avg_ms` / `comm_dt_max_ms` | commTask dt 통계 |
| `comm_overrun_count` | commTask 20ms 초과 누적 |
| `task_stack_sensor_min_words` / `task_stack_comm_min_words` / **`task_stack_led_min_words`** | 스택 high-watermark (v0415: LED 추가) |
| `failsafe_release_count` | SafeMode/OTA/연결해제 강제 release |
| `err_mutex_miss` | `_state` 락 2ms 타임아웃 |
| `err_task_overrun` | sensorTask overrun 누적 |
| `health_score` | 1000 기준 감점 방식. 클램프 헬퍼로 곱셈 오버플로 방어 |

---

## 6. v0415 리팩터 반영

### 6.1 L10 LED mutex (Round F)

- **변경**: `SemaphoreHandle_t _mutex` (Recursive Mutex)
- **영향**: `_ledTask` tick + 외부 태스크 `flash/setModeColor` 동시 호출 시 안전
- **비용**: mutex take/give 오버헤드 (~1μs)
- **비교**: portMUX 대신 mutex — `Adafruit_NeoPixel::show()`의 RMT semaphore take 때문

### 6.2 P20 Safe/Pairing Wake Mask (Round D/E)

- **변경**: `buildWakeMaskNormal/Safe/Buttons` 3종
- **영향**: Safe 모드 → Side C 단독 wake (SPEC rev8 준수)
- **BUDGET 영향**: 없음 (Wake 시점 판정)

### 6.3 C10 스키마 411 (Round C)

- **변경**: `_migrateProfileV410ToV411` (첫 부팅 시 1회)
- **영향**: 첫 부팅 +~100ms (프로파일 개수 × 매크로 step 수)
- **BUDGET**: 8초 grace (`G_BOOT_GRACE_MS = 8500`) 내 처리 가능

### 6.4 D10 대폭 축소 (Round L)

- **삭제**: Stats/markFail/markSpike/JSON export/saveToFile
- **영향**: Flash ~5KB 절감, RAM ~2KB 절감
- **관측**: 통계는 E10 도메인(`_errMpuNan` 등)이 담당

### 6.5 매크로 라이브러리 구조체 축소

- **변경**: `ST_C10_MacroStep_t` 16B → 12B
- **영향**: `ST_C10_ProfileConfig_t` ~160B 절감
- **BUDGET**: 프로파일 I/O 시간 감소

---

## 7. v0415 최종 리소스

| 항목 | v0412 | v0415 | 변화 |
|---|---|---|---|
| RAM | ~40% (131 KB) | **41.1% (134.5 KB)** | +3.5 KB |
| Flash | ~45% (1.42 MB) | **45.9% (1.443 MB)** | **−45 KB** |

**Flash 감소 원인**: A40 파일 삭제, M10 Click-Lock 삭제, D10 Stats/export 삭제, MacroStep 축소

**RAM 증가 원인**: L10 mutex, `_thLed` handle, `_bootWifi` 스냅샷, `active_mode` + `task_stack_led_min_words` 필드
