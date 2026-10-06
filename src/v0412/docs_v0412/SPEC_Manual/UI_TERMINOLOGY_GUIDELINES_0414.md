# Elite AirMouse 웹 UI 한/영 표준 용어 가이드라인 (v0414)

본 문서는 Elite AirMouse 프론트엔드(`data_v0414_www/`) 개발 및 유지보수 시 일관된 사용자 경험을 제공하기 위한 **한글/영문 UI 표준 용어 사전 및 명명 규칙(Terminology Guideline)**입니다.  
향후 버전(v0415 이상)에서 UI 요소를 추가하거나 변경할 때 반드시 본 문서의 표준 용어와 원칙을 준수해야 합니다.

---

## 1. 3대 핵심 표준화 원칙

1. **도메인 표준 용어 준수 (Platform Domain Conventions)**:
   - PC 제어: Windows OS 표준 용어(포인터 속도, 포인터 정밀도 향상, 작업 보기, 바탕 화면 보기 등) 준수.
   - 마우스 설정: 마우스 설정 및 센서 공학 표준(DPI, 스크롤 속도, 손떨림 보정 EMA, 스냅 보정 등) 준수.
   - 프레젠테이션: PowerPoint / Keynote 표준(슬라이드 쇼 시작, 쇼 종료, 이전/다음 슬라이드, 레이저 포인터 등) 준수.
   - 스마트 TV: TV 리모컨 및 CEC/소비자 가전 표준(전원, 외부 입력, 홈, 뒤로 가기, 채널, 볼륨, 재생/일시 정지 등) 준수.

2. **단일 기능-단일 용어 원칙 (Strict Consistency)**:
   - 동일한 기능에 대해 화면 위치나 모듈에 따라 다른 단어를 혼용하지 않습니다.
   - ❌ 혼용 금지: "프로필" vs "프로파일" ➔ **"프로필 (Profile)"** 통일
   - ❌ 혼용 금지: "재부팅" vs "다시 시작" ➔ **"다시 시작 (Restart)"** 통일 (Windows 표준)
   - ❌ 혼용 금지: "영점 보정" vs "영점 맞추기" vs "자이로 리셋" ➔ **"자이로 영점 조절 (Gyro Zero Calibration)"** 통일
   - ❌ 혼용 금지: "발표" vs "PPT" vs "프레젠테이션" ➔ **"프레젠테이션 (Presentation)"** 통일
   - ❌ 혼용 금지: "이전쪽/다음쪽" vs "이전/다음" ➔ 프레젠테이션 모드에서는 **"이전 슬라이드 / 다음 슬라이드 (Previous / Next Slide)"** 통일
   - ❌ 혼용 금지: "켜짐/꺼짐" vs "ON/OFF" vs "사용/사용 안 함" ➔ 드롭다운 및 토글 라벨은 **"사용 (Enabled)" / "사용 안 함 (Disabled)"** 통일

3. **영문/한글 1:1 대응 및 번역 누락 방지 (i18n Parity)**:
   - 모든 텍스트는 `lib/am_i18n_0414.js`의 `I18N_DICT` 사전에 `ko`, `en` 키가 100% 동일하게 존재해야 합니다.
   - HTML 내 텍스트는 `data-i18n`, `data-i18n-title`, `data-i18n-placeholder` 속성을 통해 동적 주입되어야 합니다.

---

## 2. 도메인별 표준 용어 매핑 테이블

### A. 시스템 동작 모드 (Operating Modes)
| 모드 번호 | 대상 기기 | 표준 한글 명칭 | 표준 영문 명칭 | 축약 표기 (버튼/배지) |
| :--- | :--- | :--- | :--- | :--- |
| **모드 1** | 데스크톱 / 노트북 | **모드 1: PC (마우스)** | **Mode 1: PC (Mouse)** | `PC` / `모드 1` (Mode 1) |
| **모드 2** | 슬라이드 쇼 / 발표 | **모드 2: 프레젠테이션** | **Mode 2: Presentation** | `발표` / `모드 2` (Presentation) |
| **모드 3** | TV / OTT 셋톱박스 | **모드 3: 스마트 TV** | **Mode 3: Smart TV** | `TV` / `모드 3` (Smart TV) |

---

### B. Windows OS & 일반 PC 기능
| 기능 / 단축키 | 표준 한글 명칭 | 표준 영문 명칭 | 설명 / 비고 |
| :--- | :--- | :--- | :--- |
| **Win 키** | 시작 메뉴 | Start Menu | Windows 시작 메뉴 열기 |
| **Win + Up** | 창 최대화 | Maximize Window | 현재 활성 창 전체 화면 최대화 |
| **Win + Left / Right** | 화면 좌측/우측 분할 | Snap Left / Right | Windows Aero Snap 화면 분할 |
| **Win + D** | 바탕 화면 보기 | Show Desktop | 모든 창 최소화 및 바탕 화면 표시 |
| **Win + Tab** | 작업 보기 | Task View | 가상 데스크톱 및 열린 작업 보기 |
| **Win + Shift + S** | 화면 캡처 | Screenshot | 윈도우 캡처 도구 실행 |
| **Win + L** | PC 잠금 (화면 잠금) | Lock PC | Windows 세션 잠금 |
| **Ctrl + C / V / X** | 복사 / 붙여넣기 / 잘라내기 | Copy / Paste / Cut | 클립보드 편집 |
| **Ctrl + Z / Y** | 실행 취소 / 다시 실행 | Undo / Redo | 작업 취소 및 복원 |
| **Ctrl + A / F** | 모두 선택 / 찾기 | Select All / Find | 문서/화면 전체 선택 및 검색 |
| **Ctrl + T / W** | 새 탭 / 탭 닫기 | New Tab / Close Tab | 브라우저 탭 관리 |

---

### C. 마우스 및 센서 동작 (Mouse & Sensor Settings)
| 기능 항목 | 표준 한글 명칭 | 표준 영문 명칭 | 세부 옵션 / 단계 표준 |
| :--- | :--- | :--- | :--- |
| **DPI / Pointer Speed** | 포인터 속도 | Pointer Speed | • 1단계: 정밀 작업용 (느림) / Level 1: Precision (Slow)<br>• 2단계: 일반 사무 (권장) / Level 2: Standard (Recommended)<br>• 3단계: 대형 화면 (빠름) / Level 3: Fast (Large Screen) |
| **Precision** | 포인터 정밀도 | Pointer Precision | 끔 (Off) / 낮음 (Low) / 보통 (Medium) / 높음 (High) / 프레젠테이션 (Presentation) |
| **Hard Click Lock** | 하드 클릭 방지 (클릭 중 잠금) | Hard Click Lock | 클릭 누름 순간 마우스 튐 방지 고정 (사용 / 사용 안 함) |
| **Click-Freeze** | 클릭 시 포인터 고정 | Click-Freeze | 마우스 클릭 순간의 흔들림 동결 (사용 / 사용 안 함) |
| **Adaptive EMA** | 적응형 손떨림 보정 | Adaptive EMA Filter | 저속 정밀 제어 및 고속 추종 필터 (불감대, 급반전 감지) |
| **Snap-to-Axis** | 직선 이동 보정 (스냅) | Linear Axis Snap | 가로(X축) / 세로(Y축) 직선 이동 보정 (사용 / 사용 안 함) |
| **Scroll Sensitivity** | 휠 스크롤 감도 | Scroll Sensitivity | 휠 1회 회전당 이동량 |
| **Scroll Dampening** | 스크롤 중 감속 비율 | Scroll Dampening | 휠 스크롤 중 포인터 이동 감속 |
| **Zero Calibration** | 자이로 영점 조절 | Gyro Zero Calibration | 수평 바닥에 정지 상태로 자이로 오프셋 보정 |

---

### D. 프레젠테이션 도구 (Presentation / PowerPoint)
| 기능 / 단축키 | 표준 한글 명칭 | 표준 영문 명칭 | 리모컨 간결 표기 |
| :--- | :--- | :--- | :--- |
| **F5** | 처음부터 슬라이드 쇼 | Start Show (From Beginning) | 처음부터 |
| **Shift + F5** | 현재 슬라이드부터 쇼 | Start Show (From Current) | 현재부터 |
| **ESC** | 슬라이드 쇼 종료 | End Slide Show | 쇼 종료 |
| **PgUp / Left** | 이전 슬라이드 | Previous Slide | 이전 슬라이드 |
| **PgDn / Right / Space** | 다음 슬라이드 | Next Slide | 다음 슬라이드 |
| **Home / End** | 첫 슬라이드 / 마지막 슬라이드 | First Slide / Last Slide | 첫 장 / 끝 장 |
| **B** | 검은색 화면 토글 | Black Screen Toggle | 검은 화면 |
| **W** | 흰색 화면 토글 | White Screen Toggle | 흰색 화면 |
| **Ctrl + P** | 펜 도구 | Pen Tool | 펜 |
| **Ctrl + L** | 레이저 포인터 | Laser Pointer | 레이저 |
| **Ctrl + A** | 화살표 포인터 | Arrow Pointer | 화살표 |

---

### E. 스마트 TV & 미디어 제어 (Smart TV & Media Control)
| 기능 항목 | 표준 한글 명칭 | 표준 영문 명칭 | 비고 |
| :--- | :--- | :--- | :--- |
| **Power** | 전원 | Power | 기기 켜기 / 끄기 |
| **Input** | 외부 입력 | Input Source | HDMI / TV 입력 소스 전환 |
| **Home** | 홈 | Home | 스마트 TV 홈 화면 |
| **Back** | 뒤로 가기 | Back | 이전 화면으로 돌아가기 |
| **OK / Select** | 확인 | OK | 선택 및 확인 |
| **Channel Up / Down** | 채널 올리기 / 내리기 | Channel Up / Down | CH ▲ / CH ▼ |
| **Volume Up / Down** | 볼륨 올리기 / 내리기 | Volume Up / Down | Vol + / Vol - |
| **Mute** | 음소거 | Mute | 음소거 켜기 / 끄기 |
| **Play / Pause** | 재생 / 일시 중지 | Play / Pause | 미디어 재생 상태 전환 |
| **Stop** | 정지 | Stop | 미디어 재생 정지 |
| **Next / Prev Track** | 다음 트랙 / 이전 트랙 | Next Track / Prev Track | 곡/영상 건너뛰기 |

---

### F. 시스템 공통 버튼 및 동작 (Common UI Actions)
| 기능 | 표준 한글 명칭 | 표준 영문 명칭 | 설명 / 사용처 |
| :--- | :--- | :--- | :--- |
| **저장** | 저장 (설정 저장 / 매크로 저장) | Save (Save Settings / Save Macros) | 플래시 메모리(NVS)에 영구 기록 |
| **적용** | 적용 | Apply | 임시 런타임 적용 또는 보안 잠금 적용 |
| **다시 불러오기** | 다시 불러오기 | Reload | 서버/기기에서 현재 저장된 값 재조회 |
| **되돌리기** | 되돌리기 | Revert | 저장되지 않은 변경사항을 원래대로 복원 |
| **다시 시작** | 다시 시작 | Restart | 하드웨어 웜 부팅 (재부팅 ❌) |
| **공장 초기화** | 공장 초기화 | Factory Reset | NVS 영구 영역 완전 소거 및 기본값 복원 |
| **새로 고침** | 새로 고침 | Refresh | 웹 화면 상태 재조회 |
| **닫기 / 취소** | 닫기 / 취소 | Close / Cancel | 팝업 또는 드로어 닫기 |
| **접기 / 펼치기** | 접기 ▲ / 펼치기 ▼ | Collapse ▲ / Expand ▼ | 가상 컨트롤러 패널 토글 |

---

## 3. 개발자 준수 가이드라인 (프론트엔드 변경 시)

1. **새로운 UI 텍스트 추가 시**:
   - 절대로 HTML이나 JS 내부에 하드코딩 한글/영문 문자열을 직접 넣지 마십시오.
   - 반드시 `src/v0412/data_v0414_www/lib/am_i18n_0414.js`의 `I18N_DICT.ko`와 `I18N_DICT.en`에 키를 동시에 추가해야 합니다.
   - 키 명명 규칙:
     - 네비게이션: `nav.*`, 상단 헤더: `top.*`
     - 탭 패널: `tab.*`, 대시보드/홈: `dash.*`
     - 가상 리모컨: `remote.*`, 모드별 도구: `tools.<mode>.*`
     - 슬롯 할당: `slots.*`, 매크로: `macros.*`
     - 세부 설정: `cfg.*`, 진단: `diag.*`, OTA: `ota.*`
     - 팝업/알림/확인창: `pop.*`

2. **단축키 및 키 라벨**:
   - `Ctrl`, `Alt`, `Shift`, `Win`, `ESC`, `Enter`, `Tab` 등 범용적인 약어는 한글 모드에서도 알파벳 고유 명칭을 유지합니다.
   - 방향키는 기호(`▲`, `▼`, `◀`, `▶`)와 함께 병기하거나 간결하게 표기합니다.

3. **자동화 검증 스크립트 실행 필수**:
   - 프론트엔드 수정 후에는 반드시 진단 스크립트(`scratch/check_i18n.py`)를 실행하여 한/영 키 불일치 및 누락이 0건인지 검증하십시오.
