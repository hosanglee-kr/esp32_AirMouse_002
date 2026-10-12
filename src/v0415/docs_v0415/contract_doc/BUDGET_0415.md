# BUDGET_0415.md — 시간 및 자원 예산 명세

> 대상 버전: `v0415` (ESP32-S3-Zero + MPU6050 AirMouse)
> 위치: `src/v0415/docs_v0415/contract_doc/BUDGET_0415.md`
> 최종 갱신: 2026-10-12 (rev10 — LED 태스크 스택 관측 실측화, _stackLedMinWords 멤버 + _ledTask 50ms 워터마크 훅 + getStatus 반영, §5 표기 갱신)

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
| `setup()` | Core 0 | – | 1회 (부팅) | – | ~9.0 초 (worst: boot key hold 6초 + I2C 진단 ~1.5초 + USB CDC 대기 최대 1.5초) | – |

> **참고**: `main loop`는 `main.cpp`의 `delay(200)`로 확인. `setup()` 최악 시간 = Side C 6초 hold(Factory Reset) + MPU6050 I2C 진단/복구(~1.5초) + USB CDC 열림 대기(최대 1.5초) + 모듈 초기화. STA 미설정 시 Wi-Fi SoftAP 즉시 구동(0ms 지연)으로 기존 8초 blocking이 완전 제거됨.

---

## 2. 잠재적 블로킹 지점 및 제한 규칙

### 2.1 sensorTask 컨텍스트

| 함수 / 기능 | 최대 블로킹 | 시스템 영향 | 완화 조치 |
|---|---|---|---|
| `_runGyroCalibration` | 1000 ms (`CALIB_MS`) | 센서 루프 일시 정지, 커서 1초 정지 | ① 부팅 1회 ② `_reqGyroCalib` 수신 시. 루프 내 `_ble.tick()` 유지 |
| `_recoverI2C` | ~15 ms | 센서 1~2 프레임 누락 | 하드웨어 멈춤 복구 최소 지연. 연속 10회 실패 시 호출 간격 제한 |
| `I2C Pre-probe` | ~0.1 ms | 없음 | `Wire.beginTransmission / endTransmission` 경량 ACK 확인 |
| `_diagAndRecoverMpu` | ~1500 ms | 부팅 setup 1회 | 9-펄스 버스 언락 + 100kHz 3라운드 스캔 (setup 전용) |
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
| step delay | **논블로킹** | `v_m._stepStartMs` + `v_now >= delayMs` 비동기 검사 |
| step 실행 (`execSingleAction`) | **블로킹 허용** (최대 28 ms) | HID tap/release의 `vTaskDelay`는 commTask에만 한정. 8ms 주기 sensorTask는 영향 없음 |

---

## 3. 동기화 원시 타입 비용

| 원시 타입 | 사용 위치 | 용도 | 시간 오버헤드 | 메모리 |
|---|---|---|---|---|
| `portENTER_CRITICAL` | `_macroAbortToken` | 매크로 즉시 abort | ~50 ns | 0 |
| `xSemaphoreCreateRecursiveMutex` | `_mutex` (E10) | `_state`, `_cfgProfile` 보호 | ~1 μs (uncontended) | 80 B |
| `xSemaphoreCreateMutex` | `_mutex` (L10) | WS2812 RMT + 애니메이션 보호 | ~1 μs (uncontended) | 80 B |

---

## 4. 큐 깊이 및 드롭 정책

| 큐 | 깊이 / 타입 | 송신 태스크 | 수신 태스크 | 오버플로 정책 |
|---|---|---|---|---|
| `_qFrame` | 1 (`ST_E10_Frame_t`) | sensorTask | commTask | `xQueueOverwrite` (최신 프레임 우선) |
| `_qActionExec` | 8 (`ST_ActionCmd_t`) | sensorTask | commTask | `xQueueSend(timeout=0)` 실패 시 drop + log |
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
| `task_stack_sensor_min_words` / `task_stack_comm_min_words` / **`task_stack_led_min_words`** | 스택 high-watermark (v0415 rev10: LED 실측 반영, _ledTask 50ms 주기 관측) |
| `failsafe_release_count` | SafeMode/OTA/연결해제 강제 release |
| `err_mutex_miss` | `_state` 락 2ms 타임아웃 |
| `err_task_overrun` | sensorTask overrun 누적 |
| `health_score` | 1000 기준 감점 방식. 클램프 헬퍼로 곱셈 오버플로 방어 |

### 5.1 D10 Logger 관측 카운터 (v0415 rev9 신규)

`getStatus()` 미노출, 필요 시 개발자 콘솔에서 직접 조회:

| API | 의미 |
|---|---|
| `getOverwriteCount()` | 링버퍼(64) 덮어쓰기 누적 (`s_dropCount` → `s_overwriteCount` 네이밍 정정) |
| `getTruncCount()` | 메시지 96B 초과 잘림 누적 (신규 관측) |

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

### 6.6 I2C 버스 복구 및 센서 진단 예산 (v0415)

- **부팅 진단**: `_diagAndRecoverMpu()`는 부팅 시 1회 최대 ~1.5초 소요 (GPIO 검사 + 9-펄스 버스 언락 + 100kHz 3라운드 스캔 + WHO_AM_I 확인).
- **런타임 오버헤드**: `sensorTask` 매 틱 I2C pre-probe는 ~0.1ms 미만으로 8ms 주기 데드라인에 영향 없음.
- **연속 실패 가드**: 연속 10회 실패 시에만 복구 시도하여 I2C 리셋 플러딩으로 인한 태스크 지연 방지.

### 6.7 Wi-Fi 부팅 시간 최적화 (v0415)

- **STA 미설정 시 즉시 SoftAP**: `sta_ssid`가 비어있는 경우 기존 8초간의 STA 타임아웃 대기를 완전히 생략.
- **시스템 기여**: 에어마우스 부팅 시간이 8초 이상 대폭 단축되어 즉각적인 페어링 및 웹 설정 가능.

### 6.8 웹 소켓 풀 보존 및 API 메모리 절감 예산 (v0415 rev8)

- **JS 번들링을 통한 소켓 풀 고갈 원천 차단**: 9개 JS 모듈을 `app_bundle_0415.js.gz`로 1개 단일화하여 전체 HTTP 요청 수를 11개 ➔ 3개로 압축. ESP32 lwIP TCP PCB 소켓 한계(8개) 초과로 인한 5분 타임아웃/연결 끊김을 0.5초 이내 고속 응답으로 안정화.
- **`apiKeycodes` 메모리 복제 0바이트 최적화**: 15KB 크기의 JSON AST 딥카피 제거 및 유효 키코드(55개) Flash 리터럴 zero-copy 스트리밍으로 순간 힙 점유를 30KB 이상에서 ~3KB로 **90% 절감**, OOM에 의한 TCP RST(Connection Reset) 완벽 차단.

### 6.9 D10 Logger SRAM 및 지연 최적화 (v0415 rev9 신규)

- **RingBuffer 축소**: `BUFFER_SIZE 256→64`, `ST_D10_LogEntry 136B→104B` (`G_D10_MSG_MAX 128→96`).
  - 총량: **34.8 KB → 6.7 KB** (−28.1 KB, −81%)
  - Dead 조회 API 삭제(Round L) 이후 write-only 상태였으므로 축소 정당성 확보
- **Two-Phase Locking**: 인덱스 확보만 `portENTER_CRITICAL`, 96B `memcpy`는 critical 외부.
  - critical 점유 ~300~600ns → ~50~100ns
  - ISR 지연 리스크 최소화 + 향후 ISR 로그 확장 대비
- **Serial 단일 write**: 라인 전체를 스택 버퍼(256B)에 조립 후 `_serial->write(buf, len)` 1회.
  - 기존 2회 printf(HWCDC 내부 분할 + 태스크 인터리빙) → 1회 원자적 write
- **delay(30) 제거**: USB CDC enumeration 대기는 `setup()` 위임.
  - Native USB(ESP32-S3): delay(30)로는 enumeration 완료 보장 못 함
  - UART 브리지(CH340 등): 즉시 열거 → delay 불필요
- **.h inline → .cpp 이관**: C++17 inline variable 이식성 이슈 회피 (ESP32 Core 2.x gnu++11 / 3.x gnu++17 양쪽 대응)

### 6.10 C10 원자적 저장 rotate 및 idx 가드 (v0415 rev9 신규)

- **`_saveIndex` / `_saveBootState` rotate 방식**: `remove(main) → rename(tmp, main)` 패턴 제거.
  - 신규: `main → main.old` rotate → `tmp → main` commit → 성공 시 `main.old` 정리
  - Power-loss 시에도 main 또는 old에서 복구 가능 (기존: index 소실 위험)
- **`_atomicWriteJson` mkdir 캐시**: `_dirsEnsured` 플래그로 begin() 이후 1회만 mkdir. 매 저장 반복 제거.
- **`makeProfilePath` / `makeProfileTmpPath` idx 가드**: `p_idx >= PROFILE_MAX` 조기 반환. Phantom 파일 생성 방지.

### 6.11 Deep-sleep 셧다운 예산 (v0415 rev9 신규)

- **MPU6050 sleep bit**: `deepSleepNow` 진입 시 `PWR_MGMT_1 = 0x40`. deep wake는 버튼 EXT1만 사용하므로 MPU WoM 불필요.
  - 절감: ~4mA 상시 소모 제거
- **WS2812 LED 소등 선행**: `deepSleepNow` 호출 이전 `_led.suspend()` (기존: light-sleep 경로에만).
  - WS2812 latch 특성상 RMT disabled 후에도 마지막 색 무한 유지 → 20mA 방전 방지

---

## 7. v0415 최종 리소스

| 항목 | v0412 | v0415 (rev8) | v0415 (rev9 실측) | 변화 (rev8→rev9) |
|---|---|---|---|---|
| RAM | ~40% (131 KB) | 41.8% (136.8 KB) | **33.2% (108.7 KB / 108,664 B)** | **−28.1 KB** |
| Flash | ~45% (1.42 MB) | 46.1% (1.451 MB) | **46.2% (1.453 MB / 1,453,201 B)** | +2 KB |

**rev9 RAM 감소 요인**: D10 RingBuffer 34.8 KB → 6.7 KB (−28.1 KB), Dead 조회 API 캐시 정리
**rev9 Flash 소폭 증가 요인**: C10 rotate 저장 rollback 로직, 딥슬립 직전 MPU 슬립 bit 및 LED suspend 가드, USB CDC 대기


---

## 8. 개정 이력

| 버전 | 날짜 | 변경 사항 |
|---|---|---|
| rev0 | 2026-09-15 | 최초 작성 (v0412) |
| rev6 | 2026-10-08 | v0415 리팩터 반영: Safe Wake Mask, B20 HOST_CYCLE, 매크로 step 블로킹, L10 mutex |
| rev7 | 2026-10-11 | **I2C 진단 및 Wi-Fi 부팅 최적화 예산 반영**: `setup()` MPU6050 9-펄스 진단/동적감지(~1.5초) 예산 반영, 런타임 pre-probe(~0.1ms) 및 10회 복구 가드, NimBLE 동기화 대기(최대 500ms) 반영, `sta_ssid` 공백 시 8초 STA 타임아웃 제거(0ms 즉시 SoftAP 구동) 부팅 단축 명시. |
| rev8 | 2026-10-11 | **웹 정적 번들링 및 API 힙 메모리 절감 예산 반영**: HTTP 요청 수 11개→3개 압축으로 lwIP TCP PCB 풀(8개) 보호 및 렌더링 지연 단축. `/api/keycodes` AST 딥카피 제거 및 zero-copy 스트리밍으로 순간 힙 점유 90% 절감(30KB→3KB, OOM/RST 방어). |
| rev9 | 2026-10-12 | **D10 Logger 최적화 + 원자적 저장 + Deep-sleep 셧다운**: §5.1 D10 관측 카운터 신규 (`getOverwriteCount`, `getTruncCount`). §6.9 D10 RingBuffer 256→64, MSG_MAX 128→96 (SRAM −28.1KB), Two-Phase Locking, Serial 단일 write, delay(30) 제거, .h inline → .cpp 이관. §6.10 C10 rotate 저장 (`main → main.old`), `_dirsEnsured` 캐시, idx 상한 가드. §6.11 Deep-sleep MPU sleep bit(`0x40`) + WS2812 LED suspend 선행. |
| **rev10** | **2026-10-12** | **LED 태스크 스택 관측 실측화 (B-2)**: `_stackLedMinWords` 멤버 + `_ledTask` 워터마크 훅 + `getStatus` 반영. §5 표기 갱신. |
