기능설계 및 구현 계획 (v0.1.0)

목표: **AirMouse Elite S3(에어모드/프리젠터)**를 v0.1.0에서 “제품으로 쓸 수 있는 수준”까지 완성하고, **조이스틱(PSP1000)**은 설정으로 유무를 켤 수 있게만 준비하되 구현은 최후순위로 둡니다.


---

1) v0.1.0 범위 정의

포함(반드시)

BLE Composite HID 기반 Mouse + Keyboard 동작 안정화

Gyro 오프셋 자동 캘리브레이션(부팅 1초 평균 + 움직임 샘플 제외)

Click-Lock 완전 고정 옵션(150ms outX/outY=0)

스크롤 전용 버튼(BTN_SCROLL) 기반 스크롤 모드(커서 이동 억제 포함)

PPT 키맵 기본 세트(Next/Prev/Start/Exit/Black/Laser SW)

멀티코어 태스크 분리(Core1 센서/연산, Core0 통신)

핀맵/빌드 옵션 문서화(PlatformIO)


제외(명시적으로 v0.1.0에서 하지 않음)

Web UI(설정 페이지) 전체

LittleFS 기반 config.json 저장/로딩

고급 사용자 커스텀 키맵(런타임 변경)

조이스틱 기반 포인팅/제스처(후순위)



---

2) 조이스틱 유무 “설정 가능” 설계(구현은 후순위)

v0.1.0에서 조이스틱은 실제로 동작하게 만들지 않아도 됨.
대신 “조이스틱이 있는 하드웨어/없는 하드웨어”를 빌드/런타임에서 선택 가능하도록 설계만 확정합니다.

2-1. 설정 방식(권장 2단)

1단(즉시 적용 / 가장 안정): platformio.ini 빌드 플래그

-D E10_HAS_JOYSTICK=0 또는 1


2단(향후 Web/LittleFS 적용 대비): 런타임 config 필드 준비

예: cfg.presenter.hasJoystick (v0.1.0에서는 “기본값만 사용”)



> v0.1.0에서는 “빌드 플래그만 적용”으로도 충분합니다.
Web/LFS는 v0.2.0 이후에 자연스럽게 붙습니다.



2-2. 코드 구조(미리 자리만 잡기)

E10 내부에 조이스틱 관련 멤버/함수는 조건부 컴파일로 스텁만 준비

예: readJoystick() / applyJoystickToCursor()는 비워두고 TODO 주석만


핀 정의도 조건부로만 포함

E10_HAS_JOYSTICK==1일 때만 ADC 핀 설정 활성화




---

3) 기능 설계(동작 정책) v0.1.0

3-1. 입력 버튼 역할(기본)

BTN_L: 좌클릭

BTN_MODE: 짧게=DPI 순환, 길게=PPT 모드 토글

BTN_SCROLL: 누르는 동안만 스크롤(커서 이동 억제 + PPT 제스처 차단)

(선택/후순위) 레이저 물리 버튼/출력은 v0.2.0 이후


3-2. 우선순위(충돌 해결 규칙)

1. BTN_SCROLL 눌림 → 스크롤 모드(제스처/PPT 명령 차단)


2. PPT 모드 ON → Flick 제스처(Next/Prev)


3. 일반 커서 이동(에어모드)




---

4) 구현 계획(마일스톤)

M0. 베이스라인 고정(완료 수준)

E10_EliteAirMouse_006.h 기준 코드 동작 확인

HID 연결/해제 안정성 확인(PC/맥/안드)


M1. 안정화/튜닝(필수)

캘리브 샘플 제외 임계(G_E10_CALIB_STILL_TH_DEG) 튜닝

스크롤 임계(G_E10_WHEEL_TH_DEG) 및 step 튜닝

Flick 임계(±200 deg/s) 튜닝

장시간 드리프트/지터 관찰 및 보정


M2. 조이스틱 “설정 가능”만 반영(구현은 X)

E10_HAS_JOYSTICK 빌드 플래그 추가

코드에 조이스틱 스텁/핀 정의 자리만 추가

사양서/핀맵에 “옵션”으로 명시


M3. 릴리즈 패키징(v0.1.0)

플랫폼/핀맵 표 정리

기본 사용법(버튼/모드) 문서 포함

PlatformIO 예제 설정 포함



---

5) v0.1.0 산출물(Deliverables)

소스: E10_EliteAirMouse_006.h + M10_MotionProc_004.h + main.cpp

platformio.ini 템플릿(기본/옵션 플래그 포함)

기술사양서(Updated) + 결선도/핀맵 + 사용법

조이스틱 옵션 설계 문서(“후순위 구현” 명시)



---

6) 후속 로드맵(참고)

v0.2.0: LittleFS + config.json 로드/세이브 + 간단 Web UI

v0.3.0: 조이스틱 정밀모드(포인팅) 구현

v0.4.0: 조이스틱 조합 제스처(Up/Down 등) + 사용자 키맵 커스텀


원하시면, v0.1.0 기준으로 **폴더/파일명 체계 + platformio.ini 플래그 설계안(E10_HAS_JOYSTICK 포함)**까지 바로 써드릴게요.