# Elite AirMouse (E10) 사용자 매뉴얼 요약 (v0306 기준)

> 목적: 버튼별 기능, 모드별/제스처별 동작, 마우스 이동/휠/클릭 정책을 사용자가 이해하기 쉽게 “표”로 정리  
> 전제: BLE 연결된 PC/태블릿에서 **마우스/키보드(Consumer 포함)** 로 동작

---

## 1) 물리 버튼 구성(핀)과 역할 요약

| 버튼 | 핀 | 입력 | 기본 의미 | 비고 |
|---|---:|---|---|---|
| 좌클릭(L) | 12 | LOW=눌림 | 마우스 좌클릭 | 클릭 시 `notifyClick()` 호출(엔진 내부 반응 강화용) |
| 우클릭(R) | 15 | LOW=눌림 | 마우스 우클릭 | |
| 중클릭(M) | 16 | LOW=눌림 | 마우스 중클릭 | |
| 모드(MODE) | 13 | LOW=눌림 | **짧게**: DPI 변경 / **길게**: PPT 토글 | 길게 기준: 1000ms 초과 |
| 스크롤(SCROLL) | 14 | LOW=눌림 | SCROLL 모드 강제(최우선) | 누르는 동안만 SCROLL |

---

## 2) 모드(FSM) 동작 요약 (우선순위 포함)

| FSM 모드 | 진입 조건(우선순위) | 커서 이동 | 휠 | 제스처(PPT 키) | 클릭(버튼) | 비고 |
|---|---|---|---|---|---|---|
| **SCROLL** | `SCROLL 버튼 누름` (최우선) | 이동은 **감쇠**(damp) 적용 | `gyro.y`로 휠 생성 | 없음 | 버튼 마스크 그대로 전송 | 스크롤 중 커서가 덜 흔들리게 damp 적용 |
| **PRECISION** | `precision_enable=true` AND `precision_mode=true` | **정밀(joystick-like) shaping + smoothing** | 0 | 없음(기본) | 버튼 마스크 그대로 전송 | entry/track/exit 서브상태로 안정화 |
| **PPT** | `PPT 토글 ON`(MODE 길게) & SCROLL/PREC 우선 제외 | 일반 커서 이동 | 0 | `gyro.z` flick으로 prev/next | 버튼 마스크 전송(단, hard_click_lock 정책 적용 가능) | 제스처는 쿨다운 존재 |
| **AIR** | 기본(나머지) | 일반 커서 이동 | 0 | 없음 | 버튼 마스크 전송 | 기본 에어마우스 |

**FSM 우선순위 규칙**
1. SCROLL 버튼이 눌리면 무조건 SCROLL
2. 그 외, precision_mode가 켜져 있고 precision_enable이면 PRECISION
3. 그 외, PPT 토글이 켜져 있으면 PPT
4. 나머지는 AIR

---

## 3) MODE 버튼: 짧게/길게 동작

| 조작 | 판정 기준 | 동작 | 영향 범위 |
|---|---|---|---|
| MODE 짧게 누름 | 눌림→뗌까지 `<= 1000ms` | DPI 순환: 1 → 2 → 3 → 1 | 커서 이동 스케일/가속감에 영향 |
| MODE 길게 누름 | 눌림→뗌까지 `> 1000ms` | PPT 토글 ON/OFF | FSM이 AIR↔PPT로 전환(단, SCROLL/PREC 우선) |

---

## 4) PPT 제스처(손목 플릭) 동작

| 상태 | 입력 축 | 조건(임계) | 동작 | 쿨다운 |
|---|---|---|---|---|
| PPT 모드에서 플릭 | `gyro.z (deg/s)` | `gz > +gesture_flick_deg` | `ppt2_prev` 키 전송 | `gesture_cooldown_ms` |
| PPT 모드에서 플릭 | `gyro.z (deg/s)` | `gz < -gesture_flick_deg` | `ppt2_next` 키 전송 | `gesture_cooldown_ms` |

> `ppt2_prev/next`는 설정(C10)에서 **Keyboard(usage) 또는 Consumer(32-bit mask)** 로 매핑 가능  
> 예: KB PageDown/RightArrow, Consumer NextTrack 등

---

## 5) PRECISION(정밀) 모드 상세(서브 상태)

| 서브상태 | 진입 | 유지/전환 규칙 | 목적 |
|---|---|---|---|
| ENTRY | precision_mode ON 시 시작 | `gyroAbs <= prec_entry_still_deg` 상태가 `prec_entry_ms` 이상 유지되면 TRACK | 정밀모드 진입 안정화(흔들림 억제) |
| TRACK | ENTRY 이후 | `gyroAbs >= prec_exit_move_deg`면 EXIT로 | 정밀 추적(조이스틱처럼 부드럽게) |
| EXIT | TRACK에서 큰 움직임 감지 시 | 다시 안정되면 TRACK로 복귀(시간/임계 기반) | “큰 흔들림” 동안 과민 반응 완화 |

---

## 6) 커서 이동/가속/스케일 정책

### 6-1. 센서 → 엔진 → 커서 값 생성 흐름

| 단계 | 입력 | 처리 | 출력 |
|---|---|---|---|
| 센서 읽기 | MPU6050 accel/gyro | bias 보정, NaN 가드 | `gx, gy, gz (deg/s)` |
| 엔진 업데이트 | accel(y,z), gx, dt | orientation update | 내부 상태 갱신 |
| 엔진 process | `-gz, -gx` | motion processing | `tx, ty` (정수) |
| 가속/스케일 | mag 기반 accel | DPI별 `scale_base` + `accel_gain` + `accel_threshold` | `fx, fy` (float) |
| FSM 적용 | scroll/prec/ppt/air | damp or precision shaping | 최종 `fx, fy` |
| 상태 커밋 | mutex 성공 시 | state.x/y/wheel/btn_mask 업데이트 | commTask로 전달 |

### 6-2. DPI별 체감 변화(일반적인 해석)

| DPI 레벨 | 의미 | 주로 변하는 값 | 체감 |
|---:|---|---|---|
| 1 | 느림 | scale_base↓, accel_gain↓ | 작은 손동작에 안정적 |
| 2 | 중간 | 기본 | 일반 사용 |
| 3 | 빠름 | scale_base↑, accel_gain↑ | 큰 화면/프레젠테이션에 유리 |

---

## 7) SCROLL 모드: 휠 생성 규칙

| 항목 | 입력 | 조건 | 출력 |
|---|---|---|---|
| 휠 Up | `gyro.y` | `gy > wheel_threshold_deg` | `wheel = + (1 ~ wheel_step_max)` |
| 휠 Down | `gyro.y` | `gy < -wheel_threshold_deg` | `wheel = - (1 ~ wheel_step_max)` |
| 커서 감쇠 | `fx, fy` | SCROLL 모드 | `fx *= scroll_cursor_damp`, `fy *= scroll_cursor_damp` |

---

## 8) 클릭/버튼 전송 정책(스턱 방지 포함)

| 상황 | 정책 | 설명 |
|---|---|---|
| 버튼 변화(press/release) | **항상 전송** | `updated=false`여도 btn diff는 무조건 처리(스턱 방지 핵심) |
| 커서/휠 이동 | `updated=true`일 때만 전송 | 스팸/지터 방지 목적 |
| BLE disconnect 직전 | 남아있는 pressed 버튼을 release | disconnect edge에서 L/R/M 모두 릴리즈 |
| BLE connect 직후 | 1회 동기화 | v_lastBtnMask를 현재 state.btn_mask로 맞춤 |
| SafeMode 진입 | HID 출력 차단 + 1회 릴리즈 | stuck 예방용, state 소비(updated=false, btn=0) |
| SafeMode 중 | 입력은 읽어도 HID 전송 없음 | 장치 안전/복구 모드 |
| hard_click_lock=true (권장 정책) | PPT에서 클릭 차단 가능 | 발표 중 오클릭 방지(구현 선택) |

---

## 9) SafeMode(안전 모드) 사용자 관점 설명

| 항목 | 동작 |
|---|---|
| 목적 | 문제 상황(부팅 실패 등)에서 **HID 출력 완전 차단**하여 안전한 설정/복구 가능 |
| HID 출력 | 마우스 이동/휠/클릭/키 입력 모두 차단 |
| 버튼 stuck 방지 | SafeMode 진입 순간 L/R/M 릴리즈를 1회 수행 |
| UI 연계 | W10 `/api/safeboot`로 상태 확인/해제 후 재부팅 권장 |

---

## 10) “한 줄 요약” 사용 시나리오

| 하고 싶은 것 | 추천 조작 |
|---|---|
| 일반 에어마우스로 쓰기 | 기본 AIR 모드로 사용(그냥 움직이고 클릭) |
| 스크롤만 하고 싶다 | SCROLL 버튼 누른 채로 위/아래로 기울이기(gyro.y) |
| 발표 넘기기(제스처) | MODE 길게 눌러 PPT ON → 손목 플릭(gyro.z) |
| 더 정밀하게 조작 | UI에서 precision_mode ON (precision_enable 필요) → PRECISION에서 천천히 이동 |
| 커서 속도 바꾸기 | MODE 짧게 눌러 DPI 1~3 순환 |

---
```
