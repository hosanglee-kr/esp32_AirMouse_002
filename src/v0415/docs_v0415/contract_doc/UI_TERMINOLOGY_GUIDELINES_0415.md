# Elite AirMouse 웹 UI 한/영 표준 용어 가이드라인 (v0415)

본 문서는 Elite AirMouse 프론트엔드(`data_v0415_www/`) 개발 및 유지보수 시 일관된 사용자 경험을 제공하기 위한 **한글/영문 UI 표준 용어 사전 및 명명 규칙**입니다.  
v0415 이상 버전에서 UI 요소를 추가하거나 변경할 때 반드시 본 문서의 표준 용어와 원칙을 준수해야 합니다.

---

## 1. 3대 핵심 표준화 원칙

1. **도메인 표준 용어 준수 (Platform Domain Conventions)**:
   - PC 제어: Windows OS 표준 용어(포인터 속도, 작업 보기, 바탕 화면 보기 등)
   - 마우스 설정: 마우스 설정 및 센서 공학 표준(DPI, 스크롤 속도, 손떨림 보정 EMA, 스냅 보정 등)
   - 프레젠테이션: PowerPoint / Keynote 표준(슬라이드 쇼 시작, 쇼 종료, 이전/다음 슬라이드, 레이저 포인터 등)
   - 스마트 TV: TV 리모컨 및 CEC/소비자 가전 표준(전원, 외부 입력, 홈, 뒤로 가기, 채널, 볼륨, 재생/일시 정지 등)

2. **단일 기능-단일 용어 원칙 (Strict Consistency)**:
   - 동일한 기능에 대해 화면 위치나 모듈에 따라 다른 단어를 혼용하지 않습니다.
   - ❌ 혼용 금지: "프로필" vs "프로파일" → **"프로필 (Profile)"** 통일
   - ❌ 혼용 금지: "재부팅" vs "다시 시작" → **"다시 시작 (Restart)"** 통일 (Windows 표준)
   - ❌ 혼용 금지: "영점 보정" vs "영점 맞추기" → **"자이로 영점 조절 (Gyro Zero Calibration)"** 통일
   - ❌ 혼용 금지: "발표" vs "PPT" vs "프레젠테이션" → **"프레젠테이션 (Presentation)"** 통일
   - ❌ 혼용 금지: "켜짐/꺼짐" vs "ON/OFF" vs "사용/사용 안 함" → **"사용 (Enabled)" / "사용 안 함 (Disabled)"** 통일

3. **영문/한글 1:1 대응 및 번역 누락 방지 (i18n Parity)**:
   - 모든 텍스트는 `lib/am_i18n_0415.js`의 `I18N_DICT` 사전에 `ko`, `en` 키가 100% 동일하게 존재해야 합니다.
   - HTML 내 텍스트는 `data-i18n`, `data-i18n-title`, `data-i18n-placeholder` 속성을 통해 동적 주입되어야 합니다.

---

## 2. 도메인별 표준 용어 매핑 테이블

### A. 시스템 동작 모드 (Operating Modes)
| 모드 | 표준 한글 | 표준 영문 | 축약 |
| :--- | :--- | :--- | :--- |
| 모드 1 | **모드 1: PC (마우스)** | **Mode 1: PC (Mouse)** | `PC` / `모드 1` |
| 모드 2 | **모드 2: 프레젠테이션** | **Mode 2: Presentation** | `발표` / `모드 2` |
| 모드 3 | **모드 3: 스마트 TV** | **Mode 3: Smart TV** | `TV` / `모드 3` |

### B. Windows OS & 일반 PC 기능
| 단축키 | 표준 한글 | 표준 영문 |
| :--- | :--- | :--- |
| Win | 시작 메뉴 | Start Menu |
| Win + Up | 창 최대화 | Maximize Window |
| Win + Left/Right | 화면 좌측/우측 분할 | Snap Left / Right |
| Win + D | 바탕 화면 보기 | Show Desktop |
| Win + Tab | 작업 보기 | Task View |
| Win + Shift + S | 화면 캡처 | Screenshot |
| Win + L | PC 잠금 (화면 잠금) | Lock PC |
| Ctrl + C/V/X | 복사 / 붙여넣기 / 잘라내기 | Copy / Paste / Cut |
| Ctrl + Z/Y | 실행 취소 / 다시 실행 | Undo / Redo |
| Ctrl + A/F | 모두 선택 / 찾기 | Select All / Find |
| Ctrl + T/W | 새 탭 / 탭 닫기 | New Tab / Close Tab |

### C. 마우스 및 센서 동작
| 기능 | 표준 한글 | 표준 영문 |
| :--- | :--- | :--- |
| **DPI / Pointer Speed** | 포인터 속도 | Pointer Speed |
| **Precision** | 포인터 정밀도 | Pointer Precision |
| **Hard Click Lock** | (v0415 삭제) | (removed) |
| **Click-Freeze** | 클릭 시 포인터 고정 | Click-Freeze |
| **Adaptive EMA** | 적응형 손떨림 보정 | Adaptive EMA Filter |
| **Snap-to-Axis** | 직선 이동 보정 (스냅) | Linear Axis Snap |
| **Scroll Sensitivity** | 휠 스크롤 감도 | Scroll Sensitivity |
| **Scroll Dampening** | 스크롤 중 감속 비율 | Scroll Dampening |
| **Zero Calibration** | 자이로 영점 조절 | Gyro Zero Calibration |

### D. 프레젠테이션 도구 (PowerPoint)
| 단축키 | 표준 한글 | 표준 영문 |
| :--- | :--- | :--- |
| F5 | 처음부터 슬라이드 쇼 | Start Show (From Beginning) |
| Shift + F5 | 현재 슬라이드부터 쇼 | Start Show (From Current) |
| ESC | 슬라이드 쇼 종료 | End Slide Show |
| PgUp / Left | 이전 슬라이드 | Previous Slide |
| PgDn / Right / Space | 다음 슬라이드 | Next Slide |
| B | 검은색 화면 토글 | Black Screen Toggle |
| W | 흰색 화면 토글 | White Screen Toggle |
| Ctrl + P | 펜 도구 | Pen Tool |
| Ctrl + L | 레이저 포인터 | Laser Pointer |

### E. 스마트 TV & 미디어 제어
| 기능 | 표준 한글 | 표준 영문 | v0415 상태 |
| :--- | :--- | :--- | :--- |
| **Power** | **전원** | **Power** | **Descriptor 미지원 → Keyboard Page 0x66** |
| Input | 외부 입력 | Input Source | **Descriptor 미지원 → NONE** |
| Home | 홈 | Home | Descriptor 지원 ✓ |
| Back | 뒤로 가기 | Back | Descriptor 지원 ✓ |
| OK / Select | 확인 | OK | – |
| Channel Up/Down | 채널 올리기/내리기 | Channel Up/Down | **Descriptor 미지원 → NONE** |
| Volume Up/Down | 볼륨 올리기/내리기 | Volume Up/Down | Descriptor 지원 ✓ |
| Mute | 음소거 | Mute | Descriptor 지원 ✓ |
| Play / Pause | 재생 / 일시 중지 | Play / Pause | Descriptor 지원 ✓ |
| Next / Prev Track | 다음 트랙 / 이전 트랙 | Next Track / Prev Track | Descriptor 지원 ✓ |

### F. 시스템 공통
| 기능 | 표준 한글 | 표준 영문 |
| :--- | :--- | :--- |
| 저장 | 저장 | Save |
| 적용 | 적용 | Apply |
| 다시 불러오기 | 다시 불러오기 | Reload |
| 되돌리기 | 되돌리기 | Revert |
| 다시 시작 | 다시 시작 | Restart |
| 공장 초기화 | 공장 초기화 | Factory Reset |
| 새로 고침 | 새로 고침 | Refresh |
| 닫기 / 취소 | 닫기 / 취소 | Close / Cancel |
| 접기 ▲ / 펼치기 ▼ | 접기 / 펼치기 | Collapse / Expand |

---

## 3. v0415 변경 사항 (v0414 → v0415)

| 항목 | v0414 | v0415 |
|---|---|---|
| `Hard Click Lock` 라벨 | 유지 | **삭제** (M10 Click-Lock 삭제, Round L) |
| `power` (TV 전원) | Consumer `POWER` | **Keyboard Page `POWER (0x66)`** |
| `input` (외부 입력) | Consumer `TV_INPUT` | **NONE (Descriptor 미지원)** |
| `ch_up` / `ch_down` | Consumer `CH_UP/DOWN` | **NONE (Descriptor 미지원)** |
| AC_Back / AC_Home / AC_Search | 유지 (라벨) | **유지 (C20 이름만 WWW_* 로 재정의, 라벨 동일)** |
| i18n 키 | KO/EN 371개 | **KO/EN 371개 (동일, 값만 일부 재검증)** |
| 오프라인 스키마 | v5 | **v6** |
| API ver | 410 | **411** |

---

## 4. 개발자 준수 가이드라인

1. **새 UI 텍스트 추가 시**:
   - HTML/JS 내부 하드코딩 한글/영문 문자열 금지.
   - `src/v0415/data_v0415_www/lib/am_i18n_0415.js`의 `I18N_DICT.ko`와 `I18N_DICT.en`에 키 동시 추가.
   - 키 명명: `nav.*`, `top.*`, `tab.*`, `dash.*`, `remote.*`, `tools.<mode>.*`, `slots.*`, `macros.*`, `cfg.*`, `diag.*`, `ota.*`, `pop.*`

2. **단축키 라벨**:
   - `Ctrl`, `Alt`, `Shift`, `Win`, `ESC`, `Enter`, `Tab` 등은 한글 모드에서도 알파벳 유지
   - 방향키는 기호(`▲▼◀▶`) 병기

3. **자동화 검증**:
   - 프론트엔드 수정 후 `scratch/check_i18n.py` 실행하여 KO/EN 키 불일치 0건 확인

---

## 5. v0415 새 용어 (TV 모드 Descriptor 제약 반영)

v0415에서 TV Power/입력/채널이 Descriptor 미지원으로 삭제됨에 따라, 사용자 매뉴얼 §7.2 및 Virtual Remote UI를 다음 기준으로 갱신:

| 이전 표기 | v0415 표기 | 비고 |
|---|---|---|
| TV 전원 | (삭제 또는 Keyboard Page Power) | §7.2에 "일부 TV 미지원" 안내 |
| 외부 입력 | (삭제) | §7.2 재작성 |
| 채널 올리기/내리기 | (삭제) | §7.2 재작성 |
| WWW Home / WWW Back / WWW Search | 홈 / 뒤로 가기 / 검색 | 라벨 유지, 값만 재정의 |
