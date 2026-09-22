📝 AirMouse Elite S3 (v0320) 기술 사양서 + AI 컨텍스트

본 문서는 ESP32-S3-Zero + MPU6050 6축 IMU 기반 에어마우스 플랫폼의 최신(v0320) 소스 기준 사양입니다. 

소스 파일 버전 접미사: _0320 통일 (Config/Sensor/Motion/Web/API 전부).

---

📌 AI 어시스턴트를 위한 요약 (Read First)

이 프로젝트를 수정하기 전 반드시 알아야 할 6가지:

1. 모듈 약어 체계: A40(공용 유틸), C10(설정), D10(로거), E10(에어마우스), M10(모션엔진), W10(웹서버). 각 접두사가 파일명·클래스명·전역 심볼에 그대로 반영됨.
2. 명명 규칙 강제: 전역 상수 G_, 전역 변수 g_, 클래스 CL_, struct ST_, enum EN_, private 멤버 _접두사, 로컬 v_접두사, 파라미터 p_접두사. 위반 시 리뷰 반려 대상.
3. ArduinoJson v7 정책: JsonDocument 단일 타입만. containsKey·createNestedArray/Object 금지. 대신 doc["a"]["b"].to<JsonObject>() 패턴.
4. E10은 6개 파일로 분할: .h + Core/Hid/Motion/Diag/Task_0320.cpp. 기능 추가 시 어느 cpp인지 먼저 판단.
5. 태스크 & 상태 소유권: sensorTask(Core 1)와 commTask(Core 0)가 큐로 통신. 웹 태스크는 HID 직접 접근 금지. 이 규칙 위반은 즉시 버그.
6. 최근 리팩터 이력: Phase 1~4 (E10 소스 크리티컬 수정), Track 2 (JSON/JS 정합), Track 3 (W10 점검). 이후 새 수정은 이 컨텍스트 위에서.

---

🎯 프로젝트 개요

ESP32-S3-Zero + MPU6050 기반 자이로 전용 공간 포인팅 에어마우스. BLE HID Composite(Mouse + Keyboard)로 호스트에 연결되며, 웹 UI로 실시간 커스터마이징을 제공합니다.

핵심 특징:

· 자이로 전용(마우스패드 불필요), 상보 필터 + 시그모이드 가속 + 적응형 LPF
· Precision 안정화 FSM (5 프로파일 × 4 상태)
· PPT 제스처(Yaw Flick) + Keymap v2 (kb/consumer 추상화)
· SafeBoot / OTA Guard 브릭 방지
· 웹 기반 실시간 편집 + 진단 (30+ API)
· Atomic config 저장 + .bak rollback

---

🏗️ 아키텍처 (AI 컨텍스트 핵심)

태스크 모델

Task Core Prio 주기 Stack 역할
_sensorTask 1 3 8ms (125Hz) 8192 IMU 읽기, FSM, 모션계산, 프레임/커맨드 생산
_commTask 0 2 7ms 4096 큐 소비 → HID 전송, 특수 커맨드 실행
AsyncWebServer (ESP-IDF 내부) - 이벤트 - HTTP 처리, E10 제어 API는 enqueue만
main loop 0 - 200ms - grace 통과 판정만

데이터 흐름 (반드시 준수)

```
sensorTask ──push──> _qFrame (size=1, overwrite) ──recv──> commTask ──> HID
     │                                                            ▲
     │ (관측용)                                                    │
     └──lock──> _state ──read──> getStatus() (웹)                 │
                                                                  │
web/sensorTask ──enqueue──> _qHidCmd (size=4) ──recv─────────────┘
```

절대 금지:

· 웹 태스크가 _mouse/_keyboard 직접 호출 → 반드시 _qHidCmd 경유
· sensorTask가 HID 직접 호출 → 반드시 _qHidCmd 경유
· commTask가 _state 접근 → _qFrame만 사용

상태 소유권

상태 소유자 접근 방식
_state (ST_E10_State_t) sensorTask(관측 갱신), web(setSafeMode 등) _lock() 하에 read/write
_qFrame sensorTask(write), commTask(read) FreeRTOS queue (lock-free)
_qHidCmd web/sensor(write), commTask(read) FreeRTOS queue (timeout=0)
_cfgE10Runtime (snapshot) _lock() 하에만 접근 H-3 원자화 경로
motion config (_dpiLevel, _scaleBase 등) setter: lock, reader: loop 시작 스냅샷 C-4
_errHist, _spikes _pushErr/_pushSpike (내부에서 lock) C-2
_hid (BleCompositeHID) commTask 단독 (예외: isConnected() read만 web 허용)
_mpu, Wire sensorTask 단독 I2C recover도 sensorTask에서만

Mutex 정책

· _mutex: recursive mutex (xSemaphoreCreateRecursiveMutex). 이유: setSafeMode/setOtaGuard가 락 보유 중 _pushErr 재진입.
· _lock() / _unlock()은 RAII 아님. 모든 경로에서 짝 맞춰야 함.
· _qFrame overwrite는 무조건 성공(size=1).
· _qHidCmd enqueue는 timeout=0 — 웹 태스크 블로킹 금지. full이면 drop.

---

📁 프로젝트 구조 (실제 v0320)

```
src/
├── main.cpp
└── v032/
    ├── A40_ComFunc_0320.h              공용 유틸 (JSON/Mutex/IO)
    ├── C10_Config_0320.h               설정 관리 (헤더 온리)
    ├── C10_Def_0320.h                  Config 스키마/enum/경로
    ├── D10_Logger_0320.h               링버퍼 로거 (헤더 온리)
    ├── M10_MotionProc_0320.h           물리 엔진 (헤더 온리)
    │
    ├── E10_Def_0320.h                  E10 상수/enum/struct
    ├── E10_AirMouse_0320.h             E10 클래스 선언
    ├── E10_AirMouse_Core_0320.cpp      초기화/런타임 적용/setter
    ├── E10_AirMouse_Hid_0320.cpp       HID 출력/테스트/강제 릴리즈
    ├── E10_AirMouse_Motion_0320.cpp    Precision FSM / Motion FSM
    ├── E10_AirMouse_Diag_0320.cpp      진단/캘리브/status
    ├── E10_AirMouse_Task_0320.cpp      sensorTask / commTask
    │
    ├── W10_Def_0320.h                  웹 상수/타입/키코드 프리셋
    ├── W10_Web_0320.h                  웹 클래스 선언
    ├── W10_Web_init_0320.cpp           begin/라우팅/WiFi/mDNS
    ├── W10_Web_Static_0320.cpp         정적서빙/body slot/diag
    ├── W10_WebApi_Com_0320.cpp         API 공통/정책/ETag
    ├── W10_WebApi_Config_0320.cpp      /api/config/*
    ├── W10_WebApi_CtlPpt_0320.cpp      /api/control, /api/ppt
    ├── W10_WebApi_OtaBoot_0320.cpp     OTA/SafeBoot/Reboot
    ├── W10_WebApi_Status_0320.cpp      /api/status/diag/keycodes
    │
    ├── tools_v032/
    │   └── pio_gzip_0320.py            www/json gzip 사전 생성
    │
    └── data_v032/                       LittleFS 이미지 (www/json 등)
        ├── www/
        │   ├── index_0320.html
        │   ├── app_0320.js
        │   ├── style_0320.css
        │   └── images/
        └── json/
            └── public/
                ├── manifest_0320.json
                └── schema_0320.json
```

E10 6파일 분할 기준

기능 추가/수정 시 어느 파일인지 판단:

파일 담당
Core begin, ctor, applyRuntimeE10, set* (PptMode/Dpi/Precision/HardClick/SafeMode/OtaGuard), _applyFromConfig, _applyE10ToRuntime, _snapshotRuntimeToE10Config, _enqueueHidCmd, _applyRuntimeLocked
Hid _tapComboUsageKb, _tapUsageKb, _tapConsumerMask, _sendPptKey2, _processGesturesDeg, testPptKey2, testMouseClick, forceReleaseButtons, _doReleaseAllButtons, _doTestMouseClick, _doForceReleaseNow
Motion _applyPrecision, _fsmUpdate
Diag _pushErr, _pushSpike, _recoverI2C, _runGyroCalibration, getStatus, requestGyroCalibration, requestI2CRecover, clearDiagnostics
Task _sensorTask, _commTask
.h 선언 + inline 유틸(_lock, _unlock, _pushFrame, _welfordAdd, _calcRms, _mouseSend)

---

⚙️ 빌드 시스템

platformio.ini 핵심

```ini
[platformio]
default_envs    = esp32-s3-zero
build_cache_dir = .pio/cache
data_dir        = ./src/v032/data_v032

[ESP32_common]
platform                = espressif32
framework               = arduino
board_build.filesystem  = littlefs
monitor_speed           = 115200
monitor_filters         = esp32_exception_decoder, colorize

src_filter =
    +<main.cpp>
    +<v032/>
    -<v001/> -<v010/> -<v030/> -<v031/> -<src_backup/>

lib_archive = yes
lib_ldf_mode = chain+
lib_deps =
    bblanchon/ArduinoJson @ ^7.4.3
    h2zero/NimBLE-Arduino @ ^2.3.7
    https://github.com/Mystfit/ESP32-BLE-CompositeHID.git
    adafruit/Adafruit MPU6050@^2.2.9
    esp32async/ESPAsyncWebServer @ ^3.10.0

build_unflags = -std=gnu++11
build_flags =
    -std=gnu++17
    -D CONFIG_BT_NIMBLE_ENABLED=1
    -D CONFIG_BT_BLE_ENABLED=1
    -D E10_HAS_JOYSTICK=0

[env:esp32-s3-zero]
extends                 = ESP32_common
board                   = esp32-s3-devkitc-1
board_build.mcu         = esp32s3
board_build.f_cpu       = 240000000L
board_build.f_flash     = 80000000L
board_build.flash_mode  = qio
board_upload.flash_size = 4MB
board_build.partitions  = default_4MB.csv

build_flags =
    ${ESP32_common.build_flags}
    -Desp_cpu_get_cycle_count=xthal_get_ccount
    -DBOARD_HAS_PSRAM
    -mfix-esp32-psram-cache-issue
    -D ARDUINO_USB_MODE=1
    -D ARDUINO_USB_CDC_ON_BOOT=1

extra_scripts = pre:src/v032/tools_v032/pio_gzip_0320.py
```

AI 어시스턴트용 메모:

· src_filter는 src/v032/ 전체를 자동 포함 → 새 파일 추가 시 ini 수정 불필요
· data_dir은 data_v032/를 LittleFS 이미지로 사용
· pio_gzip_0320.py가 빌드 전 www/*.html, *.css, *.js의 .gz 사전 생성

---

📐 명명 규칙 & 코드 정책

명명 규칙 (엄격)

대상 접두사/접미사 예
namespace {모듈}_ A40_ComFunc, C10_DEF, E10_CONST
전역 상수/매크로 G_{모듈}_ G_C10_CFG_VER, G_W10_API_VER
전역 변수 g_{모듈}_ g_cfg, g_e10
전역 함수 {모듈}_ W10_hasVersionToken
타입 T_{모듈}_ (드묾)
typedef _t 접미사 ST_C10_WiFiConfig_t
enum 상수 EN_{모듈}_ EN_C10_WIFI_AUTO
구조체 ST_{모듈}_ ST_E10_Status_t
클래스 CL_{모듈}_ CL_E10_EliteAirMouse
private 멤버 _ 접두사 _state, _lock()
클래스 정적 멤버 s_ 접두사 s_buffer, s_mux
함수 로컬 v_ 접두사 v_dt, v_gyroAbs
함수 파라미터 p_ 접두사 p_enable, p_code

ArduinoJson v7 정책

허용:

```cpp
JsonDocument doc;
doc["a"] = 1;
doc["b"]["c"] = 2;
JsonObject o = doc["x"].to<JsonObject>();
JsonArray arr = doc["y"].to<JsonArray>();
JsonVariant v = doc["z"];
if (!v.isNull()) { ... }
if (!v["k"].isNull()) { ... }
```

금지:

```cpp
JsonObject o = doc.createNestedObject("x");     // 금지
JsonArray a = doc.createNestedArray("y");       // 금지
if (doc.containsKey("k")) { ... }               // 금지
StaticJsonDocument<256> d;                      // 금지 (v6)
DynamicJsonDocument d(256);                     // 금지 (v6)
```

문자열/초기화 정책

· memset + strlcpy 조합 (Arduino String 지양)
· snprintf overflow 반드시 검사
· JSON 문자열 export 시 _appendJsonEscaped 또는 _resPrintJsonString 사용

안전 IO 정책

· config 저장: tmp write → verify(재파싱) → main→bak rotate → tmp→main commit → verify → 실패 시 bak rollback
· .bak는 성공해도 유지 (재롤백 여지)
· rollbackFromBak은 copy 우선 (rename 시 bak 상실)

---

🚀 핵심 기능 (수정본)

1️⃣ 물리 엔진 (M10_MotionProc_0320)

단순 자이로→좌표 변환이 아닌 사람 손 움직임 해석 엔진.

· 상보 필터: _roll = 0.98*(_roll + gyro*dt) + 0.02*accelRoll
· 시그모이드 가속: dpiGain = 15 + dpi_level*7, out = gain / (1 + exp(-0.8*(|in|-2)))
· 적응형 LPF: delta > 3.0 → α=0.50, else α=0.12
· Zero Snap: |v| < 0.6 → 0
· Click-Lock (150ms):
  · hard_click_lock=true → 완전 고정 (outX=outY=0)
  · hard_click_lock=false → 95% 감쇠 (*= 0.05)

2️⃣ Precision Mode (5 프로파일 × 4 상태)

mode name gain alpha accel_limit
0 OFF 1.00 0 0
1 LOW 0.85 64 0
2 MED 0.70 128 0
3 HIGH 0.55 180 0
4 PPT 0.45 210 1.5

FSM: OFF → ENTRY → TRACK ⇄ EXIT

· ENTRY→TRACK: gyro ≤ entry_still_deg가 entry_ms 지속
· TRACK→EXIT: gyro ≥ exit_move_deg 감지
· EXIT→TRACK: 다시 정지 (exit_ms 경과)

3️⃣ Gyro 캘리브레이션

· 부팅 후 1초 (CALIB_MS=1000), 정지 샘플만 (CALIB_STILL_TH=3.0 deg/s)
· 캘리브 중에도 버튼 샘플링 프레임 push (Phase 1 C-5)
· 웹 API 재요청: /api/control {"cmd":"gyro_calib"}

4️⃣ RTOS 아키텍처

· _qFrame (size=1, overwrite): 최신 스냅샷만 전달. x/y엔 정답, wheel은 강한 스크롤 시 유실 가능(M-6 유보)
· 버튼 diff는 commTask에서: updated 무관하게 항상 처리 (stuck 방지)
· _qHidCmd (size=4, timeout=0): 특수 커맨드 (RELEASE_ALL / TEST_CLICK / TEST_PPT)
· HID 실행자 = commTask 단독: 웹/sensor는 enqueue만

5️⃣ PPT 제스처 + Keymap v2

· Z축 Flick: gz > +flick_deg → prev, gz < -flick_deg → next
· 쿨다운 gesture_cooldown_ms(600ms)
· 스크롤 버튼 눌린 상태 = 제스처 무시
· SafeMode/OTA Guard 중 = 제스처 무시
· Keymap v2: page="kb" (usage-id + mod) | page="consumer" (32-bit mask)
· 6개 액션: start/exit/next/prev/black/laser

6️⃣ SafeBoot / OTA Guard

Boot State: /json/boot_state_0320.json

절차:

1. begin() → boot state 로드
2. 이전 부팅 pending=true + reset reason이 bad → fail_count++
3. fail_count ≥ SAFE_FAIL_THRESHOLD(2) → safe_mode=true
4. 이번 부팅 pending=true로 마킹
5. 정상 부팅 후 bootMarkOkIfGracePassed(8500ms) → pending=false
6. 실패 시 30초 백오프 (M-1)

bad reset reason: PANIC / INT_WDT / TASK_WDT / WDT / BROWNOUT

SafeMode 진입 시:

· AP SSID에 -SAFE 접미사
· E10 _safeMode=true (HID 차단)
· API 정책 (H-1 정리):
  · 허용: /api/status, /api/diag, /api/diag/clear, /api/keycodes, /api/safeboot, /api/ota, /api/ota/status, /api/factory_reset, /api/reboot, /api/reboot/check, GET /api/config, /api/config/export, /api/export, /api/config/rollback
  · 차단: /api/config/save, /api/config/apply, /api/config/import, /api/control, /api/ppt, /api/ppt/test

OTA Guard:

· /api/control {"cmd":"set_ota_guard","enable":true} → E10 _otaGuard=true
· HID 출력 차단 + 버튼 stuck 방지
· OTA 업로드 중 ota_guard 감지 → ota_guard_blocked (H-W2 stale 회수: 30초)

7️⃣ Config 영속화

경로 (C10_Def_0320.h):

```cpp
CFG_PATH = "/json/config_0320.json";
CFG_TMP  = "/json/config_0320.json.tmp";
CFG_BAK  = "/json/config_0320.json.bak";
BOOT_PATH = "/json/boot_state_0320.json";
```

원자적 저장 (A40_IO 또는 C10_Config::saveAll):

1. tmp write
2. tmp verify (재파싱)
3. main → bak rotate (rename 우선, copy fallback)
4. tmp → main commit
5. 최종 verify 실패 → bak rollback

Rollback (rollbackFromBak): copy 우선 → bak 유지 (M-5)

ETag: FNV-1a 32bit (웹 캐시 무효화)

8️⃣ 웹 커스터마이징

접속: AP/STA/mDNS (elite-airmouse.local)

정적 서빙: /www/* 동적 라우팅, gzip (html/css/js)

Cache-Control 자동:

· no-store: html, /json/public/*, /api/*
· immutable: 파일명에 _NNNN 버전 토큰
· short: 그 외

보안: 화이트리스트 확장자 + .. 트래버설 차단

주요 API (전체):

Method Endpoint 용도
GET /api/status 시스템/E10/정책/진단 스냅샷
GET /api/diag 진단 카운터 + 이벤트
POST /api/diag/clear 카운터 초기화
GET /api/keycodes mods/kb/consumer/precision_modes
GET /api/config 설정 조회 (ETag)
POST /api/config/save 저장 + 적용
POST /api/config/apply 적용만 (E10)
GET /api/config/export JSON 파일 다운로드
POST /api/config/import JSON 가져오기
POST /api/config/rollback .bak 복원
POST /api/control PPT/DPI/Precision/SafeMode/OTA Guard/강제 릴리즈
GET /api/ppt PPT 키맵 조회
POST /api/ppt PPT 키맵 저장
POST /api/ppt/test 단일 키 테스트
POST /api/ota 펌웨어 업로드
GET /api/ota/status OTA 진행 폴링
GET /api/safeboot SafeBoot 상태
POST /api/safeboot SafeMode 해제 (exit:true)
POST /api/factory_reset 공장 초기화
POST /api/reboot 재부팅 (reason_mask 검증)
GET /api/reboot/check 재부팅 필요 여부

---

📊 기술 스펙

항목 사양
MCU ESP32-S3 (Dual-Core, 240MHz)
Flash 4MB, QIO, 80MHz
PSRAM 지원 (BOARD_HAS_PSRAM)
Sensor MPU6050 (Gyro 250°/s, Accel 2G, DLPF 21Hz)
I2C 400kHz
Connectivity BLE (NimBLE), HID over GATT
HID Composite (Mouse + Keyboard)
Sensor Sampling 125Hz (8ms)
HID Report BLE connection interval 종속 (통상 7.5~15ms)
DPI Level 1 ~ 3
Calibration 자동 gyro bias (1초, 정지 샘플)
Scroll Gyro 기반 (cursor damp 옵션)
Storage LittleFS
Web Server ESPAsyncWebServer, HTTP/80
OS 호환 Win / macOS / Linux / Android / iOS
Driver 불필요 (표준 HID)

---

🔌 하드웨어

결선

```
ESP32-S3-Zero        MPU6050
3V3    ────────>     VCC
GND    ────────>     GND
GPIO4  ────────>     SDA
GPIO5  ────────>     SCL
```

GPIO 핀맵 (E10_Def_0320.h)

기능 GPIO 설명
I2C SDA 4 MPU6050 데이터
I2C SCL 5 MPU6050 클럭
BTN_L 12 좌클릭 (Click-Lock 트리거)
BTN_MODE 13 짧게=DPI cycle, 길게(1s)=PPT 토글
BTN_SCROLL 14 누름 유지=스크롤 모드
BTN_R 15 우클릭
BTN_M 16 휠클릭 (Middle)

⚠️ 물리 레이저 포인터는 MCU 미경유 (하드웨어 직결).

---

🖱️ 사용 가이드

기본

· 전원 인가 → 자동 BLE 연결 → 자이로 커서
· BTN_L = 좌클릭

DPI

· BTN_MODE 짧게: 1→2→3→1 순환

스크롤

· BTN_SCROLL 누른 상태에서 위/아래 기울이기
· 커서 감쇠 scroll_cursor_damp (0=완전 분리)
· 반응 임계 wheel_threshold_deg (기본 90°)
· 가속 wheel_step_max (기본 6)

PPT

· BTN_MODE 길게(1초+) → PPT 모드 ON/OFF (쿨다운 1500ms)
· 손목 Yaw Flick: 좌=prev, 우=next
· 웹 PPT Keymap 탭에서 실시간 편집/테스트

Precision

· 웹 또는 /api/control로 set_precision mode 설정
· FSM 자동 ENTRY→TRACK→EXIT

SafeBoot / 공장 초기화

· SafeBoot 해제: POST /api/safeboot {"exit":true} → 재부팅
· Factory Reset: POST /api/factory_reset

OTA

· 웹 UI OTA 탭 → .bin 업로드
· 업로드 중 HID 자동 차단 (버튼 stuck 방지)
· 완료 시 자동 재부팅

---

🔬 진단

/api/status?compact=1 및 /api/diag.

E10 그룹:
ble_connected, ppt_mode, dpi_level, precision_mode, fsm_state, fsm_sub, btn_mask, safe_mode, gate.*, health.*

모션/센서:
gyro.*, cursor_rms, temp_c, sampling.*

오류/복구:
err.mpu_nan, err.mutex_miss, err.task_overrun, i2c.*, anomaly.*

관측:
task_stack_sensor_min_words, task_stack_comm_min_words, sensor_dt_max_ms, sensor_overrun_count, comm_dt_avg_ms, comm_dt_max_ms, comm_overrun_count, failsafe_release_count

웹:
body_too_large, body_no_slot, bad_json, safe_blocked, ota_blocked, last_apply.*

---

⚠️ 주의사항

· 부팅 직후 1초 정지 권장 (캘리브)
· 스크롤 과반응 → wheel_threshold_deg ↑ / wheel_step_max ↓
· 커서 스크롤 중 이동 → scroll_cursor_damp ↓ (0=분리)
· 정지 떨림 → precision_mode 활성 or zero_snap 조정
· WiFi 변경 저장 후 재부팅 필요 (/api/reboot/check)

---

🗂️ 프로젝트 구조 (요약 표)

파일 역할 소유 모듈
main.cpp setup/loop, W10-E10 브릿지, grace 판정 -
A40_ComFunc_0320.h JSON helper / Mutex / Atomic IO A40
C10_Config_0320.h Config load/save/validate, boot state C10
C10_Def_0320.h Config 스키마 / 경로 상수 C10
D10_Logger_0320.h 링버퍼 로거 + 진단 카운터 D10
E10_Def_0320.h E10 상수/enum/struct E10
E10_AirMouse_0320.h E10 클래스 선언 + inline 유틸 E10
E10_AirMouse_Core_0320.cpp 초기화/런타임 적용/setter E10
E10_AirMouse_Hid_0320.cpp HID 출력/테스트/강제 릴리즈 E10
E10_AirMouse_Motion_0320.cpp Precision/Motion FSM E10
E10_AirMouse_Diag_0320.cpp 진단/캘리브/status E10
E10_AirMouse_Task_0320.cpp sensorTask / commTask E10
M10_MotionProc_0320.h 물리 엔진 M10
W10_Def_0320.h 웹 상수/타입/키코드 프리셋 W10
W10_Web_0320.h 웹 클래스 선언 W10
W10_Web_init_0320.cpp begin/라우팅/WiFi/mDNS W10
W10_Web_Static_0320.cpp 정적서빙/body slot/diag W10
W10_WebApi_Com_0320.cpp API 공통/정책/ETag W10
W10_WebApi_Config_0320.cpp /api/config/* W10
W10_WebApi_CtlPpt_0320.cpp /api/control, /api/ppt W10
W10_WebApi_OtaBoot_0320.cpp OTA/SafeBoot/Reboot W10
W10_WebApi_Status_0320.cpp /api/status/diag/keycodes W10
tools_v032/pio_gzip_0320.py 빌드 전 gzip 사전 생성 build
data_v032/www/* HTML/JS/CSS frontend
data_v032/json/public/* manifest/schema frontend

---

📚 참고: 리팩터 이력 (AI 컨텍스트)

Phase 1 — E10 치명 버그

· C-1: sensorTask 정상 경로에서 _pushFrame() 누락 → HID 큐가 초기 1회만 채워짐 → 커서/버튼 전송 안 됨. 수정.
· C-2: _pushErr/_pushSpike 무보호 다중 태스크 접근 → recursive mutex + 내부 lock.
· C-5: 캘리브 1초 블로킹 중 프레임 정지 → 강제 릴리즈 + 버튼 샘플링 프레임 push.
· E10을 6파일로 분할.

Phase 2 — HID 태스크 일원화

· C-3: 웹 태스크가 _mouse/_keyboard 직접 접근 → _qHidCmd 도입, commTask 단독 실행.
· H-2: forceReleaseButtons/All 이원화 → 단일화 (alias + enqueue).
· H-4: 웹 블로킹 (test 250ms) → enqueue-only.

Phase 3 — 원자성/정책

· H-3: setDpiLevel 등 RMW 비원자 → _applyRuntimeLocked + lock 안 commit.
· C-4: sensorTask 매 루프 시작 config 스냅샷 (dpi+scale+accel, wheel 그룹).
· H-1: SafeMode에서 /api/config/import 차단.

Phase 4 — 마모/정합성

· M-1: bootMarkOkIfGracePassed 실패 시 30초 백오프 (flash 마모 방지).
· M-2: _recoverI2C 카운터 lock 통일.
· M-4: SAFE/OTA gate 진입·이탈 전용 에러코드.
· M-5: rollbackFromBak copy 우선, bak 유지.

Track 2 — JSON/Frontend 정합

· J-1: boot_state_0320.json 키 boot_ms, last_reset_reason
· J-2: app_0320.js schema 경로 schema_0320.json
· J-3: DPI 1~3 통일 (schema/UI)
· J-4/J-4b: precision.enable 제거, precision_mode → precision.mode
· J-5: config_0320.json meta 노드 제거
· index_0320.html 중복 precMode select 제거

Track 3 — W10 재점검

· H-W1: schema wifi.mode enum (AUTO/AP/STA)
· H-W2: OTA _otaInProgress stuck → 30초 stale 회수 (_otaStartedMs)
· R-1: OTA 업로드 콜백 중괄호 mismatch

---

🔧 알려진 유보 이슈 (AI가 참고할 것)

# 내용 파일 트리거
H-5 commTask가 HID 커맨드 실행 중(최대 250ms) 프레임 처리 정지 E10_AirMouse_Task_0320.cpp test_click 중 커서 끊김 체감 시
M-6 _qFrame size=1 overwrite → 강한 스크롤 시 wheel 유실 E10_AirMouse_Core_0320.cpp 스크롤 카운트 부정확 확인 시
M-7 _state.updated write-only (dead) 여러 곳 정리 시점
M-W2 mode=STA + sta_ssid 빈 값 → WiFi 없음 (fallback 없음) W10_Web_init_0320.cpp 정책 결정 필요
M-W1 apiKeycodes 캐시 없음 (매 요청 232개 순회) W10_WebApi_Status_0320.cpp 성능 이슈 시
L-1 getStatus()가 락 안에서 _hid.isConnected() E10_AirMouse_Diag_0320.cpp 무해
L-4 _sendPptKey2FromCfg dead code E10_AirMouse_Hid_0320.cpp 정리 시점
L-5 _getE10RuntimeConfig 미사용 E10_AirMouse_Core_0320.cpp 정리 시점

---

🎯 권장 활용 시나리오

· 프레젠테이션 리모컨 (Precision + PPT 제스처)
· 거실 PC / HTPC (자이로 포인팅)
· 스마트TV 입력 장치
· 산업용 HMI 무선 포인팅
· 드로잉/태블릿 보조 입력 (Click-Lock + Precision)

---

📌 부록: 원본 README 대비 변경점

항목 구버전 (v0.0.6) 현행 (v0320)
버튼 3 (L, MODE, SCROLL) 5 (L, R, M, MODE, SCROLL)
GPIO BTN_R/M 없음 15 / 16 추가
스크롤 "완전 분리" 감쇠 (기본 25%)
Click-Lock Hard만 Hard + Soft 2모드
Precision 없음 5단계 FSM + 프로파일
PPT Keymap 없음 v2 (kb/consumer)
SafeBoot 없음 fail_count 기반 자동 진입
OTA Guard 없음 HID 차단 게이트
웹 API 최소 v316 (30+ 엔드포인트)
Config 저장 단순 write Atomic + .bak + rollback
진단 없음 RMS/오류/스택/dt 관측
Polling "125Hz" Sensor 125Hz / HID BLE 종속
BLE "BLE 5.0" BLE (NimBLE), HID over GATT
E10 구조 단일 파일 6파일 분할
HID 실행자 다중 태스크 commTask 단독
HID 커맨드 직접 호출 _qHidCmd 경유

---

✅ 프로젝트 철학

AirMouse Elite S3는 단순 프로젝트가 아닌 상용급 입력 디바이스 아키텍처입니다.

· 태스크/상태 소유권 명확
· 락 정책 일관 (recursive, 짝맞춤)
· 브릭 방지 (SafeBoot / Atomic Config / OTA Guard)
· 관측성 (RMS / 스택 / dt / 오류 이력)
· 정책 일관성 (SafeMode API 게이트, 커맨드 큐 일원화)

새 기능 추가 시 위 원칙 위반 여부를 먼저 판단.

---
