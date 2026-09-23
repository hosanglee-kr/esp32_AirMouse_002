📝 AirMouse Elite S3 (v0.31.x) 기술 사양서

본 프로젝트는 ESP32-S3-Zero + MPU6050 6축 IMU 기반의 하이엔드 에어마우스 플랫폼입니다.
자이로 전용 공간 포인팅, Precision 안정화 FSM, PPT 제스처, SafeBoot / OTA Guard, 웹 기반 실시간 커스터마이징을 제공합니다.

소스 파일 버전 접미사: _0310(Config/Sensor/Motion), _0315(Web), _0316(Web API). 본 문서는 최신 소스 기준입니다.

---

🚀 핵심 기능 (Key Features)

1️⃣ 하이엔드 물리 엔진 (M10_MotionProc_0310)

단순 자이로 → 좌표 변환이 아닌 사람 손 움직임을 해석하는 물리 엔진을 적용했습니다.

· 상보 필터(Complementary Filter) 기반 기울기 보정
  · 계수: 0.98 * (gyro 적분) + 0.02 * accelRoll
  · 마우스를 비스듬히 잡아도 화면 좌표는 항상 수평/수직 유지
· 시그모이드(Sigmoid) 가변 가속도
  · 감도 계수: dpiGain = 15 + (dpi_level × 7)
  · 미세 움직임: 픽셀 단위 정밀 제어
  · 빠른 스윙: 자연스러운 가속
· 적응형 LPF (Adaptive EMA)
  · delta > 3.0 → alpha = 0.50 (빠른 추종)
  · delta ≤ 3.0 → alpha = 0.12 (떨림 억제)
· Zero Snap: 극저속 영역(< 0.6) 커서 0 고정 → 정지 떨림 제거
· Click-Lock (2모드)
  · hard_click_lock = true → 클릭 후 150ms 완전 고정 (outX/outY=0)
  · hard_click_lock = false → 클릭 후 150ms 95% 감쇠 (Soft 모드)
  · 정밀 드래그·프리젠터 포인팅 최적화

---

2️⃣ Precision Mode (FSM 안정화)

정밀 포인팅을 위한 4단계 상태 머신 + 5단계 프로파일을 지원합니다.

mode 이름 gain alpha accel_limit 특징
0 OFF 1.00 0 0 비활성
1 LOW 0.85 64 0 약한 안정화
2 MED 0.70 128 0 기본 권장
3 HIGH 0.55 180 0 강한 안정화
4 PPT 0.45 210 1.5 프리젠터 최강 안정화

Precision FSM (진입/추적/이탈):

· OFF → ENTRY: mode 활성화 시 자동 진입
· ENTRY → TRACK: gyro 크기 ≤ entry_still_deg가 entry_ms 지속
· TRACK → EXIT: gyro 크기 ≥ exit_move_deg 감지
· EXIT → TRACK: 다시 정지 상태 도달 시

사용자 조정 가능 파라미터: deadzone, gain, accel, max_step, smooth, entry_ms, exit_ms, entry_still_deg, exit_move_deg, profile

---

3️⃣ Gyro 오프셋 자동 캘리브레이션

· 전원 인가 후 1초간 자동 캘리브레이션 (CALIB_MS = 1000)
· 움직임이 큰 샘플 자동 제외 (CALIB_STILL_TH = 3.0 deg/s)
· 손에 들고 전원을 켜도 오차 최소화
· 웹 API로 재캘리브레이션 요청 가능 (/api/control gyro_calib)

---

4️⃣ 멀티코어 RTOS 아키텍처

ESP32-S3 듀얼 코어 구조를 적극 활용합니다.

Core Task 우선순위 주기 Stack
Core 1 Sensor Task 3 8ms (125Hz) 8192
Core 0 Comm Task 2 7ms 4096

· 프레임 큐(Queue size 1, Overwrite): 센서 → 커뮤 태스크는 최신 스냅샷만 전달
· 버튼 diff는 Comm Task에서 처리: updated 여부와 무관하게 버튼 변화는 항상 전송(Stuck 방지)
· mutex miss 시 즉시 재시도 경로: 버튼 stuck 이중 방어

---

5️⃣ PPT 제스처 모드 & Keymap v2

· Z축(Yaw) Flick 제스처
  · gz > +flick_deg → 이전 슬라이드 (prev 키 전송)
  · gz < -flick_deg → 다음 슬라이드 (next 키 전송)
  · gesture_cooldown_ms로 중복 방지 (기본 600ms)
· PPT Keymap v2 (page 추상화)
  · page = "kb": 키보드 usage-id (0x00~0xE7) + modifier mask
  · page = "consumer": 32-bit consumer mask
  · 6개 액션: start / exit / next / prev / black / laser
  · 웹에서 조회·수정·테스트 가능
· 제스처 자동 차단: 스크롤 버튼 누른 상태에서는 제스처 무시(오동작 방지)

---

6️⃣ SafeBoot / OTA Guard (브릭 방지)

· Boot State 파일 (/json/boot_state_0300.json)로 부팅 실패 카운트 추적
· 부팅 시 pending=true 마킹 → 정상 부팅 시 bootMarkOkIfGracePassed()로 해제
· 비정상 리셋 판정: PANIC / INT_WDT / TASK_WDT / WDT / BROWNOUT
· SAFE_FAIL_THRESHOLD = 2 도달 시 Safe Mode 진입
  · AP SSID에 -SAFE 접미사
  · 위험 API 차단(/api/config/save, /api/control 등)
  · 허용 API만 응답(/api/status, /api/safeboot, /api/ota 등)
· OTA Guard: OTA 중 HID 출력 완전 차단 + 버튼 stuck 방지

---

7️⃣ Config 영속화 (Atomic + Recovery)

· 경로 정책
  · Main: /json/config_301.json
  · Temp: /json/config_0301.json.tmp
  · Backup: /json/config_0301.json.bak
· 원자적 저장 절차
  1. tmp write → verify(재파싱)
  2. main → bak rotate (rename 우선, 실패 시 copy)
  3. tmp → main commit (rename 우선, 실패 시 copy)
  4. 최종 verify 실패 시 bak에서 자동 복구
· Config ETag: FNV-1a 32bit, 웹 UI 캐시 무효화용
· Rollback: /api/config/rollback으로 .bak 즉시 복원

---

8️⃣ 웹 커스터마이징 (ESPAsyncWebServer)

· 접속 방식: AP 모드 / STA 모드 / mDNS (elite-airmouse.local)
· 정적 서빙: /www/* 동적 라우팅, gzip 지원(html/css/js)
· Cache-Control 자동 분류
  · no-store: HTML, /json/public/*, /api/*
  · immutable: 파일명 버전 토큰(_NNNN) 포함 리소스
  · short: 그 외 정적 리소스
· 보안: 화이트리스트 확장자 + .. 트래버설 차단

주요 API:

Endpoint 용도
GET /api/status 시스템/E10/정책/진단 통합 스냅샷
GET /api/diag 진단 카운터 + 최근 이벤트
GET /api/keycodes mods/kb/consumer/precision_modes 목록
GET /api/config 설정 조회 (ETag 지원)
POST /api/config/save 저장 + 적용
POST /api/config/apply 적용만 (저장 없음, E10만)
POST /api/config/import JSON 가져오기
POST /api/config/rollback .bak에서 복원
POST /api/control PPT/DPI/Precision/SafeMode/OTA Guard/강제 릴리즈
GET /api/ppt PPT 키맵 조회
POST /api/ppt PPT 키맵 저장
POST /api/ppt/test 단일 키 테스트
POST /api/ota 펌웨어 업로드
GET /api/safeboot SafeBoot 상태
POST /api/factory_reset 공장 초기화
POST /api/reboot 재부팅 (reason_mask 검증)

---

📊 기술 스펙 (Technical Specifications)

항목 사양
MCU ESP32-S3 (Dual-Core, 240MHz)
Flash 4MB, QIO, 80MHz
PSRAM 지원 (BOARD_HAS_PSRAM)
Sensor MPU6050 (Gyro 250°/s, Accel 2G, DLPF 21Hz)
I2C 400kHz
Connectivity BLE (NimBLE), HID over GATT
HID Composite HID (Mouse + Keyboard)
Sensor Sampling 125Hz (8ms)
HID Report BLE connection interval 종속 (통상 7.5~15ms)
DPI Level 1 ~ 3 (소프트웨어 가속 포함)
Calibration 자동 Gyro bias (1초, 정지 샘플만)
Scroll Gyro 기반 (커서 감쇠 옵션)
Storage LittleFS (설정 + 부팅 상태)
Web Server ESPAsyncWebServer, HTTP/80
OS 호환 Windows / macOS / Linux / Android / iOS
Driver 불필요 (표준 HID)

---

🔌 하드웨어 결선 & 핀맵

📐 기본 결선 구조

```
ESP32-S3-Zero        MPU6050
3V3    -------->     VCC
GND    -------->     GND
GPIO4  -------->     SDA
GPIO5  -------->     SCL
```

📍 GPIO 핀맵 (E10_Def_0310.h)

기능 GPIO 설명
I2C SDA 4 MPU6050 데이터
I2C SCL 5 MPU6050 클럭
BTN_L 12 마우스 좌클릭 (Click-Lock 트리거)
BTN_MODE 13 짧게 = DPI cycle / 길게(1초) = PPT 토글
BTN_SCROLL 14 누름 유지 = 스크롤 모드
BTN_R 15 마우스 우클릭
BTN_M 16 마우스 휠클릭 (Middle)

⚠️ 물리 레이저 포인터는 MCU를 거치지 않고 버튼 직결 방식으로 제어됩니다(코드 제어 대상 아님).

---

🖱️ 사용 가이드 (User Guide)

1️⃣ 기본 사용

· 전원 인가 → 자동 BLE 연결
· 손을 움직이면 커서 이동 (자이로 기반)
· BTN_L 클릭 = 마우스 좌클릭

2️⃣ DPI 변경

· BTN_MODE 짧게 누르기: 1 → 2 → 3 → 1 순환
· 해상도 높은 화면에서는 고속(3) 권장

3️⃣ 스크롤 사용

· BTN_SCROLL 누른 상태에서 위/아래 기울이기
· 커서 이동은 scroll_cursor_damp 배율로 감쇠 (기본 25%)
  · 완전 분리를 원하면 scroll_cursor_damp = 0.0으로 설정
· wheel_threshold_deg(기본 90°) 이상 기울여야 반응
· wheel_step_max(기본 6)까지 가속

4️⃣ PPT 모드

· BTN_MODE 길게 누르기(1초 이상) → PPT 제스처 모드 ON/OFF
  · 모드 토글 쿨다운: 1500ms
· 좌우로 손목을 빠르게 회전(Yaw Flick)
  · 좌 → 이전 / 우 → 다음 슬라이드
· PPT 키맵은 웹 UI의 PPT Keymap 탭에서 실시간 편집/테스트

5️⃣ Precision (정밀 모드)

· 웹 UI 또는 /api/control로 precision_mode 설정
· 설정 즉시 FSM이 ENTRY → TRACK → EXIT 자동 전이
· 정지 상태에서 커서 흔들림 대폭 감소
· 드로잉/포인팅/PPT에 최적

6️⃣ SafeBoot / 공장 초기화

· SafeBoot 해제: /api/safeboot { "exit": true } → 자동 재부팅
· Factory Reset: /api/factory_reset → 설정/부팅 상태 전부 초기화 후 재부팅

7️⃣ OTA 업데이트

· 웹 UI OTA 탭에서 .bin 파일 업로드
· 업로드 중 HID 자동 차단 → 버튼 stuck 방지
· 완료 시 자동 재부팅

---

🔬 진단 & 관측 (Diagnostics)

/api/status (compact=1) 및 /api/diag에서 다음 항목을 제공합니다.

E10 상태 그룹:

· ble_connected, ppt_mode, dpi_level, precision_mode
· fsm_state, fsm_sub, btn_mask, safe_mode
· gate.ota_guard, gate.ota_guard_count, gate.ota_guard_uptime_ms
· health.state, health.score (0~1000)

모션/센서:

· gyro.bias_x/y/z, gyro.rms, cursor_rms, temp_c
· sampling.ms_target, sampling.ms_avg

오류/복구:

· err.mpu_nan, err.mutex_miss, err.task_overrun
· i2c.recover_count, i2c.recover_last_ok
· anomaly.spike_count_10s, consecutive_fail, consecutive_recover_fail

관측 (Observability):

· task_stack_sensor_min_words, task_stack_comm_min_words
· sensor_dt_max_ms, sensor_overrun_count
· comm_dt_avg_ms, comm_dt_max_ms, comm_overrun_count
· failsafe_release_count

웹 서버 (W10):

· body_too_large, body_no_slot, bad_json, safe_blocked, ota_blocked
· last_apply.ok/ms/code/src

---

⚠️ 주의사항

· 부팅 직후 1초간 정지 상태 유지 권장 (캘리브레이션 정확도 향상)
· 스크롤 반응이 과하면 wheel_threshold_deg / wheel_step_max 조정
· 커서가 스크롤 중에도 움직이면 scroll_cursor_damp를 낮춤 (0 = 완전 분리)
· 정지 떨림이 심하면 precision_mode 활성화 또는 zero_snap 조정
· WiFi 설정 변경은 저장 후 재부팅 필요 (/api/reboot/check로 확인)
· Safe Mode 진입 시 웹 UI에서 원인 리셋 카운터 확인 후 해제

---

🗂️ 프로젝트 구조 (주요 파일)

파일 역할
E10_AirMouse_0310.h 에어마우스 메인 (태스크/FSM/HID/설정 적용)
E10_Def_0310.h E10 상수/열거형/구조체
M10_MotionProc_0310.h 물리 엔진 (상보필터/시그모이드/Click-Lock)
C10_Config_0310.h 설정 로드/저장/검증/부팅 상태
C10_Def_0310.h Config 스키마/enum/경로 상수
A40_ComFunc_070.h 공용 유틸 (JSON/FS/Mutex/Atomic IO)
D10_Logger_061.h 링버퍼 로거 + 진단 카운터
W10_Def_0315.h 웹 서버 공통 상수/타입/키코드 프리셋
W10_Web_0315.h/.cpp 웹 서버 본체
W10_WebApi_*_0316.cpp API 그룹별 구현
tools/pio_gzip_0312.py 빌드 전 www/json gzip 생성 스크립트

---

🎯 권장 활용 시나리오

· 프레젠테이션 리모컨 (Precision + PPT 제스처)
· 거실 PC / HTPC (자이로 포인팅)
· 스마트TV 입력 장치
· 산업용 HMI 무선 포인팅
· 드로잉/태블릿 보조 입력 (Click-Lock + Precision)

---

✅ AirMouse Elite S3는 단순 프로젝트가 아닌,
상용급 입력 디바이스 아키텍처를 목표로 설계되었습니다.

---

📌 부록: 원본 README 대비 주요 변경점

항목 구버전 (v0.0.6) 현행 (v0.31.x)
버튼 개수 3 (L, MODE, SCROLL) 5 (L, R, M, MODE, SCROLL)
GPIO 누락 BTN_R/M 없음 15 / 16 추가
스크롤 "완전 분리" 감쇠(기본 25%)
Click-Lock Hard만 Hard + Soft 2모드
Precision 없음 5단계 FSM + 프로파일
PPT Keymap 없음 v2 (page=kb/consumer)
SafeBoot 없음 fail_count 기반 자동 진입
OTA Guard 없음 HID 차단 게이트
웹 API 최소 v316 (30+ 엔드포인트)
Config 저장 단순 write Atomic + .bak + rollback
진단 없음 RMS/오류/스택/dt 관측
Polling 표기 "125Hz" Sensor 125Hz / HID는 BLE 종속
BLE 표기 "BLE 5.0" BLE (NimBLE), HID over GATT
