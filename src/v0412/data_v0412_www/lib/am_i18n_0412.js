/* =======================================================
   File: /www/lib/am_i18n_0412.js
   Elite AirMouse WebConfig v0412 — Lightweight i18n Core
   - 한국어(ko) / 영어(en) 다국어 지원 엔진
   - 로드 순서: 2 (am_base 다음, offline 이전)
   - data-i18n, data-i18n-title, data-i18n-placeholder 속성 일괄 번역
   - 브라우저 언어 자동 감지 및 localStorage 영구 기억
   ======================================================= */

const I18N_STORAGE_KEY = "am_lang_0412";
let g_currLang = "ko";

const I18N_DICT = {
  ko: {
    /* Header & Global */
    "top.brand_sub": "v0412",
    "top.refresh": "새로고침",
    "top.reboot": "재부팅",
    "top.reboot_needed": "재부팅 필요:",
    "top.reboot_now": "지금 재부팅",
    "prof.label": "Profile",
    "prof.new": "+ 새로",
    "prof.rename": "이름",
    "prof.delete": "삭제",
    "prof.reload_title": "재로드",
    "loading.processing": "처리 중…",

    /* Tabs */
    "tab.dash": "Dashboard",
    "tab.slots": "버튼 및 제스처 동작 배정 (Slots)",
    "tab.macros": "Macros 동작 설정",
    "tab.config": "기기 세부 환경 설정 (Device Config)",
    "tab.diag": "문제 진단 및 로그 (Diagnostics)",
    "tab.ota": "펌웨어 업데이트 (OTA)",

    /* Dashboard */
    "dash.status_title": "현재 상태 (Status)",
    "dash.uptime": "가동 시간 (Uptime)",
    "dash.heap": "여유 메모리 (Heap Free)",
    "dash.wifi": "와이파이 연결 (Wi-Fi)",
    "dash.cur_prof": "현재 Profile",
    "dash.quick_ctl": "빠른 제어 (Quick Control)",
    "dash.ppt_on": "발표 모드 ON",
    "dash.ppt_off": "발표 모드 OFF",
    "dash.calib": "센서 영점 맞추기 (자이로 캘리브레이션)",
    "dash.force_rel": "키 누름 강제 해제",
    "dash.i2c_rec": "센서 연결 초기화 (I2C 버스 복구)",
    "dash.quick_tune": "빠른 튜닝 (Quick Tuning)",
    "dash.quick_tune_hint": "(임시 적용 · 기기 재부팅 시 원래대로 복구됨)",
    "dash.dpi_label": "커서 속도 (DPI):",
    "dash.dpi_1": "1 (느림)",
    "dash.dpi_2": "2 (표준)",
    "dash.dpi_3": "3 (빠름)",
    "dash.prec_label": "정밀 모드 (Precision):",
    "dash.prec_off": "OFF",
    "dash.prec_low": "약함",
    "dash.prec_med": "중간",
    "dash.prec_high": "강함",
    "dash.prec_ppt": "발표용",
    "dash.safe_enter": "안전 모드 (SafeMode) 진입",
    "dash.safe_exit": "안전 모드 (SafeMode) 해제",
    "dash.host_cycle": "연결 기기 전환 (호스트 순환)",
    "dash.safeboot_info": "SafeBoot 상태",
    "dash.safeboot_exit": "SafeBoot 해제 (재부팅)",
    "dash.factory_reset": "공장 초기화 (Factory Reset)",
    "dash.profile_label": "동작 모드:",
    "dash.mode_1": "모드 1 (일반)",
    "dash.mode_2": "모드 2 (발표)",
    "dash.mode_3": "모드 3 (TV)",

    /* Slots */
    "slots.title": "버튼 및 제스처 동작 배정 (Slots)",
    "slots.view_global": "기본 공통 (Global)",
    "slots.view_mode1": "모드 1: 일반 마우스",
    "slots.view_mode2": "모드 2: 프레젠테이션",
    "slots.view_mode3": "모드 3: 스마트 TV / 미디어",
    "slots.th_trigger": "트리거 (입력 신호)",
    "slots.th_action": "실행할 동작 (Action)",
    "slots.th_test": "테스트",
    "slots.save": "동작 설정 저장",
    "slots.reload": "되돌리기 (재로드)",
    "slots.grp_btn": "버튼 트리거 (Button Triggers)",
    "slots.grp_gesture": "제스처 (공중 털기 / 직선 이동)",
    "slots.grp_tilt": "기울기 유지 (Tilt Hold · Mode 3 TV 전용)",
    "slots.tilt_global_info": "ℹ 여기서 설정한 기본 공통(Global) 값은 Mode 3 (TV)에 자동 상속됩니다.",
    "slots.tilt_disabled_warn": "⚠ Tilt Hold(기울기 유지)는 Mode 3 (TV)에서만 실제 발동합니다. 이 뷰에서는 편집할 수 없습니다. 기본 공통(Global) 또는 Mode 3에서 설정하세요.",
    "slots.badge_g_title": "클릭하여 이 모드 전용 설정(Override) 생성",
    "slots.badge_m_title": "클릭하여 기본 공통(Global)으로 복원",
    "slots.badge_m3_title": "Mode 3 (TV) 전용 — 기본 공통(Global) 값 상속",

    /* Macros */
    "macros.title": "Macros 관리",
    "macros.add": "+ 새 Macro 생성",
    "macros.save": "Macro 저장",
    "macros.reload": "되돌리기 (재로드)",
    "macros.name_label": "Macro 이름:",
    "macros.step_label": "실행 단계 (Steps):",
    "macros.name_ph": "Macro 이름",
    "macros.empty_hint": "등록된 Macro가 없습니다.",
    "macros.select_hint": "좌측 목록에서 Macro를 선택하세요.",
    "macros.add_step": "+ Step 추가",
    "macros.btn_del": "삭제",
    "macros.delay_label": "지연 대기 ms (0~2000)",

    /* Config Sections */
    "cfg.title": "기기 세부 환경 설정 (Device Config)",
    "cfg.save": "설정 저장하기",
    "cfg.reload": "되돌리기",
    "cfg.export": "설정 파일로 백업 (JSON)",
    "cfg.import": "백업 파일 불러오기",
    "cfg.rollback": "이전 설정 복원 (폐기)",
    "cfg.rollback_hint": "설정 복구는 상단 Profile 전환 또는 공장 초기화(Factory Reset)를 이용해 주세요.",
    "cfg.sec_motion_core": "1. 기본 마우스 감도 (Core Motion)",
    "cfg.sec_motion_modes": "2. 모드별 세부 동작 특성 (3 Modes Motion)",
    "cfg.sec_click_freeze": "3. 클릭 순간 커서 동결 (Click-Freeze)",
    "cfg.sec_ema": "4. 지능형 손떨림 억제 (Adaptive EMA)",
    "cfg.sec_snap": "5. 발표용 직선 보정 (Snap-to-Axis)",
    "cfg.sec_power": "6. 절전 및 전원 관리 (Power Management)",
    "cfg.sec_buttons": "7. 버튼 디바운스 및 타이밍 (Button Timings)",
    "cfg.sec_preset": "모션 프리셋 (빠른 적용)",
    "cfg.preset_hint": "프리셋 클릭 시 현재 편집 중인 Profile에 즉시 적용됩니다.",
    "cfg.preset_pc": "PC",
    "cfg.preset_ppt": "PPT",
    "cfg.preset_tv": "TV",
    "cfg.preset_gaming": "게임",
    "cfg.preset_precision": "정밀",
    "slots.hint": "27가지 조작 동작(버튼 15개, 제스처 8개, 틸트 4개)에 원하는 키나 기능을 짝지어 줍니다. [전체 공통]은 모든 모드 기본값이며, 모드 탭에서 해당 모드 전용 기능으로 덮어쓸 수 있습니다.",
    "macros.hint": "한 번의 버튼 조작으로 복잡한 단축키나 연속 동작을 자동으로 실행합니다. (Profile당 최대 8개 Macro, Macro당 최대 8단계 지정 가능)",
    "cfg.hint": "현재 선택된 Profile의 마우스 속도, 가속도, 제스처 및 세부 감도 파라미터를 편집합니다.",
    "cfg.foot": "v0412 · Profile + Global/Mode Override Slots + Macro Lib + Motion Advanced",

    /* Config Select Options */
    "cfg.dpi_opt_1": "1단계: 정밀 작업용 (느림)",
    "cfg.dpi_opt_2": "2단계: 일반 사무/웹서핑 (권장 · 표준)",
    "cfg.dpi_opt_3": "3단계: 대화면/빠른 이동 (빠름)",
    "cfg.hard_click_on": "사용함 (ON · 클릭 순간 위치 잠금)",
    "cfg.hard_click_off": "사용 안 함 (OFF · 자유 이동 허용)",
    "cfg.cf_on": "사용함 (ON · 권장)",
    "cfg.cf_off": "사용 안 함 (OFF · 자유 이동)",
    "cfg.ema_rev_on": "사용함 (ON · 권장 · 랙/지연 제거)",
    "cfg.ema_rev_off": "사용 안 함 (OFF · 연속 필터 유지)",
    "cfg.snap_on": "사용함 (ON · 직선 보정)",
    "cfg.snap_off": "사용 안 함 (OFF · 완전 자유 이동)",
    "cfg.snap_axis_both": "가로 + 세로 모두 지원 (Both · 권장)",
    "cfg.snap_axis_h": "가로 직선만 보정 (Horizontal Only)",
    "cfg.snap_axis_v": "세로 직선만 보정 (Vertical Only)",

    /* Config Field Labels (flbl) */
    "cfg.lbl_dpi": "커서 기본 속도 (DPI)",
    "cfg.lbl_hard_click": "클릭 중 커서 흔들림 잠금",
    "cfg.lbl_led_bright": "상태 표시등 (LED) 밝기",
    "cfg.lbl_accel_th": "가속 시작 속도 (임계값)",
    "cfg.lbl_scroll_damp": "스크롤 중 커서 감속 비율",
    "cfg.lbl_sb": "모드별 기본 속도 배율",
    "cfg.sb0_title": "PC 모드 (권장: 0.55)",
    "cfg.sb1_title": "발표 모드 (권장: 0.75)",
    "cfg.sb2_title": "TV 모드 (권장: 1.00)",
    "cfg.lbl_ag": "모드별 가속 곡선 강도",
    "cfg.ag0_title": "PC 모드 (권장: 0.35)",
    "cfg.ag1_title": "발표 모드 (권장: 0.55)",
    "cfg.ag2_title": "TV 모드 (권장: 0.85)",
    "cfg.lbl_wheel": "스크롤 휠 민감도",
    "cfg.wheel_th_title": "시작 기울기 각도 (권장: 90도)",
    "cfg.wheel_step_title": "최대 스텝 수 (권장: 6)",
    "cfg.lbl_flick": "공중 털기 제스처 감도",
    "cfg.flick_deg_title": "인식 각속도 (권장: 200도/초)",
    "cfg.flick_cd_title": "재발생 방지 대기시간 (권장: 600ms)",
    "cfg.lbl_cf_enable": "클릭 순간 커서 동결",
    "cfg.lbl_cf_gyro": "정지 판정 각속도",
    "cfg.lbl_cf_max": "동결 유지 최대 시간",
    "cfg.lbl_cf_hold": "손 뗀 후 동결 유지",
    "cfg.lbl_cf_fadeout": "감쇠 복귀 시간",
    "cfg.lbl_cf_move": "강제 이탈 변위 (픽셀)",
    "cfg.lbl_cf_freeze_move": "이동 의도 판정 각속도",
    "cfg.lbl_ema_min": "저속 부드러움 (Alpha Min)",
    "cfg.lbl_ema_max": "고속 반응성 (Alpha Max)",
    "cfg.lbl_ema_deadzone": "저속 필터링 기준 속도",
    "cfg.lbl_ema_fast": "고속 추종 전환 속도",
    "cfg.lbl_ema_reversal": "급격한 방향 전환 감도",
    "cfg.lbl_ema_rev_reset": "급격한 방향 전환 리셋",
    "cfg.lbl_snap_enable": "축 스냅 기능",
    "cfg.lbl_snap_modes": "적용 모드 선택",
    "cfg.lbl_snap_axis": "스냅 판정 축",
    "cfg.lbl_snap_ratio": "축 판정 기울기 비율",
    "cfg.lbl_snap_strength": "직선 고정 강도",
    "cfg.lbl_snap_frames": "축 판정 안정화 프레임",
    "cfg.lbl_pwr_idle": "1단계 라이트 슬립 (초)",
    "cfg.lbl_pwr_deep": "2단계 초절전 딥 슬립 (초)",
    "cfg.lbl_pwr_wom_th": "흔듦 감지 깨어남 감도 (WoM)",
    "cfg.lbl_pwr_wom_dur": "흔듦 지속시간 (ms)",
    "cfg.lbl_pwr_led_fade": "절전 진입 불빛 서서히 끄기",
    "cfg.lbl_pwr_wake_deb": "깨어남 센서 안정화 시간",
    "cfg.lbl_btn_deb_press": "누름 떨림 방지 (Debounce Press)",
    "cfg.lbl_btn_deb_rel": "뗌 튕김 방지 (Debounce Release)",
    "cfg.lbl_btn_click": "단일 클릭 최대 누름 시간",
    "cfg.lbl_btn_dblclick": "더블클릭 최대 허용 간격",
    "cfg.lbl_btn_long": "길게 누름 (Long-Press) 인식 시간",
    "cfg.lbl_btn_min_click": "미세 글리치(스침) 무시 시간",

    /* Config Field Help Guides (fhelp) */
    "cfg.help_dpi": "<b>설정 범위: 1~3단계</b> | 기본값: <b>2 (표준)</b><br>공간에서 손목을 움직일 때 마우스 화살표의 이동 민감도를 선택합니다.",
    "cfg.help_hard_click": "<b>권장값: ON (사용함)</b><br>공중에서 마우스 버튼을 딸깍 누를 때 손가락 압력으로 화살표가 빗나가는 현상을 방지합니다.",
    "cfg.help_led_bright": "<b>설정 범위: 0~255</b> | 권장값: <b>100~150</b> (기본값: 128)<br>본체 중앙 WS2812 RGB 불빛의 밝기를 조절합니다. (0 = 완전 꺼짐, 배터리 절약)",
    "cfg.help_accel_th": "<b>설정 범위: 1.0~30.0</b> | 권장값: <b>5.0~12.0</b> (기본값: 8.0)<br>손목을 빠르게 스냅할 때 가속 곡선이 발동하기 시작하는 기준 속도입니다.",
    "cfg.help_scroll_damp": "<b>설정 범위: 0.05~1.00</b> | 권장값: <b>0.20~0.30</b> (기본값: 0.25)<br>Side F 버튼을 누른 채 화면을 스크롤할 때 화살표 튐을 줄이기 위해 속도를 줄여주는 감속 비율입니다. (0.25 = 평소 속도의 25%로 감속)",
    "cfg.help_sb": "<b>권장값: [0.55, 0.75, 1.00]</b> (범위: 0.20~2.00)<br>순서대로 1번(PC) / 2번(발표) / 3번(스마트TV) 모드의 기본 이동 배율입니다.",
    "cfg.help_ag": "<b>권장값: [0.35, 0.55, 0.85]</b> (범위: 0.10~2.00)<br>빠른 손목 스냅 시 추가로 화살표를 멀리 보내는 가속 증폭 강도입니다.",
    "cfg.help_wheel": "<b>기본값: 90도 / 6스텝</b> (권장: 각도 60~120도, 스텝 3~10)<br>스크롤이 작동하기 시작하는 기기 기울기 각도(도) 및 1회 최대 스크롤 양입니다.",
    "cfg.help_flick": "<b>기본값: 200도/초 / 600ms</b> (권장: 150~300도/초, 400~800ms)<br>손목을 휙 털 때 명령으로 인식할 회전 속도 및 중복 방지 대기 시간입니다.",
    "cfg.help_cf_enable": "<b>권장값: ON (사용함)</b><br>클릭 버튼을 누르는 순간 손가락 압력으로 화살표가 튀거나 빗나가는 현상을 정밀하게 잡아줍니다.",
    "cfg.help_cf_gyro": "<b>설정 범위: 5.0~30.0 deg/s</b> | 권장값: <b>10.0~20.0</b> (기본값: 15.0)<br>손목의 회전 속도가 이 값 이하일 때만 의도된 클릭으로 판정하여 커서를 동결합니다.",
    "cfg.help_cf_max": "<b>설정 범위: 50~300 ms</b> | 권장값: <b>100~200 ms</b> (기본값: 150ms)<br>이 시간을 초과하여 누르고 있으면 드래그(끌기) 동작으로 판단하여 동결을 자동으로 해제합니다.",
    "cfg.help_cf_hold": "<b>설정 범위: 0~50 ms</b> | 권장값: <b>10~30 ms</b> (기본값: 20ms)<br>버튼에서 손가락을 뗄 때 발생하는 미세 반동으로 화살표가 튀는 현상을 막기 위해 추가 유지하는 시간입니다.",
    "cfg.help_cf_fadeout": "<b>설정 범위: 0~80 ms</b> | 권장값: <b>20~40 ms</b> (기본값: 30ms)<br>동결이 끝난 후 정상 화살표 이동 속도로 서서히 부드럽게 복귀하는 전환 시간입니다.",
    "cfg.help_cf_move": "<b>설정 범위: 1.0~10.0 px</b> | 권장값: <b>1.5~3.0 px</b> (기본값: 2.0px)<br>동결 중이라도 손목을 강하게 움직여 이 거리 이상 이동하면 즉시 동결을 강제 해제합니다.",
    "cfg.help_cf_freeze_move": "<b>설정 범위: 10~60 deg/s</b> | 권장값: <b>25~35 deg/s</b> (기본값: 30 deg/s)<br>동결 도중 사용자가 의도적인 빠른 이동을 시작했다고 판단하여 잠금을 즉시 푸는 회전 속도입니다.",
    "cfg.help_ema_min": "<b>설정 범위: 0.01~0.20</b> | 권장값: <b>0.03~0.08</b> (기본값: 0.05)<br>정지 및 느린 조작 시의 필터 강도입니다. (수치가 낮을수록 손떨림을 완벽히 흡수하여 부드러움)",
    "cfg.help_ema_max": "<b>설정 범위: 0.50~0.95</b> | 권장값: <b>0.75~0.85</b> (기본값: 0.80)<br>빠른 손목 이동 시 지연 없이 반응하는 강도입니다. (1.0에 가까울수록 지연 없는 직결 조작)",
    "cfg.help_ema_deadzone": "<b>설정 범위: 1.0~10.0 rad/s</b> | 권장값: <b>2.0~4.0</b> (기본값: 3.0 rad/s)<br>이 속도 이하의 미세 움직임에서 최대 손떨림 억제 필터를 적용합니다.",
    "cfg.help_ema_fast": "<b>설정 범위: 10~30 rad/s</b> | 권장값: <b>12~18</b> (기본값: 15 rad/s)<br>이 속도 이상으로 손을 빠르게 휘두르면 필터 지연 없이 100% 즉각 추종합니다.",
    "cfg.help_ema_reversal": "<b>설정 범위: 5.0~20.0 rad/s</b> | 권장값: <b>6.0~10.0</b> (기본값: 8.0 rad/s)<br>좌↔우 급회전으로 인식할 반대 방향 각속도 기준값입니다.",
    "cfg.help_ema_rev_reset": "<b>권장값: ON (사용함)</b><br>손목 방향을 반대로 갑자기 꺾을 때 뒤따라오는 위상 지연(무게감/랙)을 순간 리셋하여 즉각 반전합니다.",
    "cfg.help_snap_enable": "<b>권장값: 발표 모드에서 ON</b><br>슬라이드 밑줄 긋기나 도표 지칭 시 손목이 흔들려도 수평/수직 직선으로 자동 정렬합니다.",
    "cfg.help_snap_modes": "<b>권장: 2번 발표 모드만 체크</b><br>일반 PC 작업이나 TV 제어 시에는 직선 보정을 끄고, 프레젠테이션 시에만 작동하도록 권장합니다.",
    "cfg.help_snap_axis": "<b>권장값: 가로 + 세로 모두 지원</b><br>직선 보정을 허용할 이동 방향 범위를 선택합니다.",
    "cfg.help_snap_ratio": "<b>설정 범위: 2.0~10.0</b> | 권장값: <b>3.0~5.0</b> (기본값: 4.0 = 4:1)<br>한쪽 축으로의 움직임이 다른 축 대비 4배 우세할 때 직선으로 판단하여 자석처럼 고정합니다.",
    "cfg.help_snap_strength": "<b>설정 범위: 0.50~1.00</b> | 권장값: <b>0.80~0.90</b> (기본값: 0.85)<br>1.00은 완전 고정된 직선, 0.85는 살짝 빗겨 나갈 수 있는 부드러운 직선 유도입니다.",
    "cfg.help_snap_frames": "<b>설정 범위: 1~10 프레임</b> | 권장값: <b>2~4 프레임</b> (기본값: 3프레임 = 24ms)<br>대각선 흔들림으로 인한 스냅 오작동을 방지하기 위해 축 판정을 유지하는 최소 프레임 수입니다.",
    "cfg.help_pwr_idle": "<b>설정 범위: 0~3600 초</b> | 권장값: <b>60초</b> (0=기능 끄기)<br>블루투스 미연결 시 라이트 슬립 진입 대기 시간입니다. (가볍게 흔들거나 클릭 시 즉시 0.3초 내로 깨어남)",
    "cfg.help_pwr_deep": "<b>설정 범위: 0~86400 초</b> | 권장값: <b>600초 (10분)</b> (0=기능 끄기)<br>배터리 방전을 완벽히 방지하기 위해 센서와 불빛을 완전히 차단하는 초절전 모드 진입 시간입니다. (버튼을 누르면 전원 켜짐)",
    "cfg.help_pwr_wom_th": "<b>설정 범위: 5~150</b> | 권장값: <b>20~35</b> (기본값: 25 = 약 0.8g 흔들림)<br>잠든 마우스를 손으로 흔들어 깨울 때의 가속도 민감도입니다. (숫자가 낮을수록 살짝만 흔들어도 쉽게 깨어남)",
    "cfg.help_pwr_wom_dur": "<b>설정 범위: 1~50 ms</b> | 권장값: <b>2~5 ms</b> (기본값: 2ms)<br>흔들림이 단순 충격인지 실제 깨우려는 동작인지 판정하는 최소 지속 시간입니다.",
    "cfg.help_pwr_led_fade": "<b>설정 범위: 100~3000 ms</b> | 권장값: <b>500~1000 ms</b> (기본값: 800ms)<br>슬립 모드로 들어갈 때 빨간색 표시등이 부드럽게 페이드아웃되며 꺼지는 연출 시간입니다.",
    "cfg.help_pwr_wake_deb": "<b>설정 범위: 50~1000 ms</b> | 권장값: <b>100~200 ms</b> (기본값: 150ms)<br>슬립에서 깨어난 직후 센서 영점을 재수집하고 손떨림으로 인한 연속 재슬립을 방지하는 안정화 대기 시간입니다.",
    "cfg.help_btn_deb_press": "<b>설정 범위: 5~100 ms</b> | 권장값: <b>15~25 ms</b> (기본값: 20ms)<br>버튼을 누르는 순간 금속 접점의 물리적 떨림을 무시하는 시간입니다. (낮을수록 클릭 반응이 즉각적임)",
    "cfg.help_btn_deb_rel": "<b>설정 범위: 5~100 ms</b> | 권장값: <b>25~40 ms</b> (기본값: 30ms)<br>손가락을 뗄 때 발생하는 스위치 튕김으로 인해 의도치 않은 더블클릭이 일어나는 노후 스위치 오작동을 방지합니다.",
    "cfg.help_btn_click": "<b>설정 범위: 50~1000 ms</b> | 권장값: <b>200~300 ms</b> (기본값: 250ms)<br>버튼을 누르고 있는 시간이 이 값 이하일 때만 1회 단독 클릭으로 판정합니다.",
    "cfg.help_btn_dblclick": "<b>설정 범위: 100~1000 ms</b> | 권장값: <b>250~400 ms</b> (기본값: 300ms)<br>첫 번째 클릭 후 두 번째 클릭이 인정되는 최대 시간 간격입니다. (손이 느린 사용자는 400ms 추천)",
    "cfg.help_btn_long": "<b>설정 범위: 200~3000 ms</b> | 권장값: <b>500~800 ms</b> (기본값: 600ms)<br>버튼을 꾹 누르고 있을 때 '길게 누름' 동작으로 판정하기 시작하는 최소 시간입니다.",
    "cfg.help_btn_min_click": "<b>설정 범위: 0~100 ms</b> | 권장값: <b>20~40 ms</b> (기본값: 30ms)<br>옷깃 스침이나 정전기로 인한 30ms 미만의 극히 짧은 전기적 펄스는 무효 신호로 처리하여 완전 무시합니다.",

    /* Common */
    "btn.apply": "적용",
    "btn.save": "저장",
    "btn.cancel": "취소",
    "btn.close": "닫기",
    "btn.test": "테스트",

    /* Diagnostics & OTA */
    "diag.title": "문제 진단 및 로그 (Diagnostics)",
    "diag.clear": "카운터 초기화",
    "diag.auto": "자동 갱신 (Auto)",
    "diag.filter_ph": "필터 검색 (예: bad_json, i2c)",
    "diag.recent_events": "최근 발생 이벤트 (Recent Events)",
    "diag.key_test_title": "단발 키 전송 테스트 (Key Test)",
    "diag.key_test_hint": "(블루투스 연결 상태에서 즉시 신호 전송)",
    "diag.btn_send": "테스트 전송",
    "diag.key_test_sub": "키보드: usage 키코드 (10진수), 미디어: 비트 마스크 (10진수)",
    "diag.raw_json": "진단 원본 데이터 보기 (Raw JSON)",
    "diag.kt_kb": "키보드 일반 (KB)",
    "diag.kt_consumer": "미디어 키 (Consumer)",
    "diag.kt_mod_none": "조합키 없음 (None)",
    "diag.kt_mod_lctrl": "Ctrl (좌)",
    "diag.kt_mod_lshift": "Shift (좌)",
    "diag.kt_mod_lalt": "Alt (좌)",
    "diag.kt_mod_lgui": "Win/Cmd (좌)",
    "diag.kt_mod_rctrl": "Ctrl (우)",
    "diag.kt_mod_rshift": "Shift (우)",
    "diag.kt_mod_ralt": "Alt (우)",
    "diag.kt_mod_rgui": "Win/Cmd (우)",
    "ota.title": "무선 펌웨어 업데이트 (OTA)",
    "ota.guard_label": "OTA 보안 보호 (OTA Guard - 마우스 입력 차단 및 무단 업데이트 방지)",
    "ota.guard_desc": "보호 모드(Guard ON) 시 마우스/키보드 입력이 차단되며, 외부에서의 원치 않는 펌웨어 덮어쓰기가 거부됩니다.",
    "ota.btn_upload": "펌웨어 업로드",
    "ota.btn_status": "진행 상태 확인",

    /* Popups / Alerts / Prompts (사용자 알림 및 확인) */
    "pop.switch_fail": "Profile 전환 실패:",
    "pop.prof_name_prompt": "새 Profile 이름 (최대 15자):",
    "pop.prof_create_fail": "Profile 생성 실패:",
    "pop.prof_delete_active_confirm": "⚠ 현재 활성 Profile(#{idx})을 삭제합니다.\n\n삭제 후 다른 Profile로 자동 전환됩니다.\n계속하시겠습니까?",
    "pop.prof_delete_confirm": "Profile #{idx} \"{name}\"을(를) 삭제하시겠습니까?",
    "pop.prof_delete_fail": "Profile 삭제 실패:",
    "pop.prof_rename_prompt": "새 Profile 이름 (최대 15자):",
    "pop.prof_rename_fail": "Profile 이름 변경 실패:",
    "pop.no_action_assigned": "할당된 동작(Action)이 없습니다.",
    "pop.live_test_confirm": "테스트를 실행하시겠습니까? (종류: {kind})",
    "pop.test_fail": "테스트 실행 실패:",
    "pop.slot_save_fail": "동작 배정(Slots) 저장 실패:",
    "pop.macro_empty": "동작 단계(Steps)가 비어 있는 Macro입니다.",
    "pop.macro_test_confirm": "Macro \"{name}\"을(를) 실행하시겠습니까?",
    "pop.macro_save_fail": "Macro 저장 실패:",
    "pop.macro_exec_fail": "Macro 실행 실패:",
    "pop.macro_del_confirm": "Macro \"{name}\"을(를) 삭제하시겠습니까?",
    "pop.macro_new_prompt": "새 Macro 이름 (최대 15자):",
    "pop.force_release_ok": "모든 버튼 및 키 누름이 정상적으로 해제되었습니다.",
    "pop.safeboot_exit_confirm": "SafeBoot 모드를 해제하고 기기를 재부팅하시겠습니까?",
    "pop.safeboot_exit_fail": "SafeBoot 해제 실패:",
    "pop.reboot_delay_warn": "재부팅 응답이 지연되고 있습니다. 잠시 후 웹페이지를 새로고침 해주세요.",
    "pop.safemode_enter_confirm": "안전 모드(SafeMode)로 진입합니다.\nHID(마우스/키보드) 입력이 차단됩니다 (웹 설정은 정상 가능).\n계속하시겠습니까?",
    "pop.safemode_exit_confirm": "안전 모드(SafeMode)를 해제합니다.\nHID(마우스/키보드) 입력이 정상 재개됩니다.\n계속하시겠습니까?",
    "pop.safemode_change_fail": "안전 모드(SafeMode) 상태 변경 실패:",
    "pop.factory_reset_confirm": "⚠ 공장 초기화 (Factory Reset)를 진행하시겠습니까?\n\n모든 사용자 Profile 및 설정이 영구 삭제되고 초기화된 상태로 재부팅됩니다.",
    "pop.factory_reset_fail": "공장 초기화 (Factory Reset) 실패:",
    "pop.reboot_confirm": "기기를 재부팅하시겠습니까?",
    "pop.reboot_requested": "기기 재부팅 요청 완료 (약 3초 후 재연결됩니다)",
    "pop.reboot_fail": "재부팅 요청 실패:",
    "pop.preset_apply_confirm": "모션 프리셋 \"{preset}\"을(를) 현재 Profile에 적용하시겠습니까?\n(설정 창에 반영되며 [기기에 저장]을 눌러야 영구 적용됩니다)",
    "pop.preset_applied": "프리셋 \"{preset}\" 적용 및 저장 완료",
    "pop.diag_reset_confirm": "진단 카운터 및 오류 로그를 초기화하시겠습니까?",
    "pop.diag_reset_fail": "진단 카운터 초기화 실패:",
    "pop.host_cycle_confirm": "등록된 다른 블루투스 기기(호스트)로 전환하시겠습니까?\n(현재 블루투스 연결이 잠시 끊긴 후 재연결됩니다)",
    "pop.host_cycle_fail": "호스트 순환 전환 실패:",
    "pop.i2c_recover_confirm": "센서(MPU6050) I2C 통신 버스 복구를 진행하시겠습니까?",
    "pop.i2c_recover_fail": "센서(MPU6050) I2C 복구 요청 실패:",
    "pop.i2c_recover_hardware_warn": "I2C 버스 복구에 실패했습니다.\n센서 연결 상태나 하드웨어 전원을 점검해 주세요.",
    "pop.gyro_calib_confirm": "기기를 흔들림 없는 평평한 바닥에 내려놓고 정지 상태를 유지해 주세요.\n자이로 영점 재보정을 시작하시겠습니까?",
    "pop.gyro_calib_fail": "자이로 영점 재보정 요청 실패:",
    "pop.net_mode_switch_confirm": "현재 모드: {current}\n{target} 모드로 전환하시겠습니까?",
    "pop.file_select_req": "업로드할 펌웨어 파일을 먼저 선택해 주세요."
  },
  en: {
    /* Header & Global */
    "top.brand_sub": "v0412",
    "top.refresh": "Refresh",
    "top.reboot": "Reboot",
    "top.reboot_needed": "Reboot Required:",
    "top.reboot_now": "Reboot Now",
    "prof.label": "Profile",
    "prof.new": "+ New",
    "prof.rename": "Rename",
    "prof.delete": "Delete",
    "prof.reload_title": "Reload",
    "loading.processing": "Processing…",

    /* Tabs */
    "tab.dash": "Dashboard",
    "tab.slots": "Button / Gesture Slots",
    "tab.macros": "Macro Configuration",
    "tab.config": "Device Configuration",
    "tab.diag": "Diagnostics",
    "tab.ota": "OTA Update",

    /* Dashboard */
    "dash.status_title": "System Status",
    "dash.uptime": "Uptime",
    "dash.heap": "Heap Free",
    "dash.wifi": "WiFi Status",
    "dash.cur_prof": "Active Profile",
    "dash.quick_ctl": "Quick Control",
    "dash.ppt_on": "Presenter Mode ON",
    "dash.ppt_off": "Presenter Mode OFF",
    "dash.calib": "Calibrate Gyro Zero",
    "dash.force_rel": "Force Release Keys",
    "dash.i2c_rec": "Reset I2C Bus",
    "dash.quick_tune": "Quick Tuning",
    "dash.quick_tune_hint": "(Temporary · Reverts on device reboot)",
    "dash.dpi_label": "Cursor Speed (DPI):",
    "dash.dpi_1": "1 (Slow)",
    "dash.dpi_2": "2 (Normal)",
    "dash.dpi_3": "3 (Fast)",
    "dash.prec_label": "Precision Mode:",
    "dash.prec_off": "OFF",
    "dash.prec_low": "Low",
    "dash.prec_med": "Med",
    "dash.prec_high": "High",
    "dash.prec_ppt": "Presenter",
    "dash.safe_enter": "Enter SafeMode",
    "dash.safe_exit": "Exit SafeMode",
    "dash.host_cycle": "Cycle Host (BT)",
    "dash.safeboot_info": "SafeBoot Status",
    "dash.safeboot_exit": "Exit SafeBoot (Reboot)",
    "dash.factory_reset": "Factory Reset",
    "dash.profile_label": "Active Mode:",
    "dash.mode_1": "Mode 1 (Standard)",
    "dash.mode_2": "Mode 2 (Presenter)",
    "dash.mode_3": "Mode 3 (TV)",

    /* Slots */
    "slots.title": "Button & Gesture Assignments (Slots)",
    "slots.view_global": "Default (Global)",
    "slots.view_mode1": "Mode 1: Standard Mouse",
    "slots.view_mode2": "Mode 2: Presentation",
    "slots.view_mode3": "Mode 3: Smart TV / Media",
    "slots.th_trigger": "Trigger (Input Signal)",
    "slots.th_action": "Action to Execute",
    "slots.th_test": "Test",
    "slots.save": "Save Slots",
    "slots.reload": "Reload",
    "slots.grp_btn": "Button Triggers",
    "slots.grp_gesture": "Gesture (Flick / Linear Move)",
    "slots.grp_tilt": "Tilt Hold (Mode 3 · TV Only)",
    "slots.tilt_global_info": "ℹ Global values configured here are inherited by Mode 3 (TV).",
    "slots.tilt_disabled_warn": "⚠ Tilt Hold only activates in Mode 3 (TV). It cannot be edited in this view. Please configure in Global or Mode 3.",
    "slots.badge_g_title": "Click to override in this Mode",
    "slots.badge_m_title": "Click to revert to Global",
    "slots.badge_m3_title": "Mode 3 (TV) Only — Inherits Global",

    /* Macros */
    "macros.title": "Macro Sequence Manager",
    "macros.add": "+ New Macro",
    "macros.save": "Save Macros",
    "macros.reload": "Reload",
    "macros.name_label": "Macro Name:",
    "macros.step_label": "Steps:",
    "macros.name_ph": "Macro Name",
    "macros.empty_hint": "No macros registered.",
    "macros.select_hint": "Select a macro from the list on the left.",
    "macros.add_step": "+ Add Step",
    "macros.btn_del": "Delete",
    "macros.delay_label": "Delay ms (0~2000)",

    /* Config Sections */
    "cfg.title": "Device Configuration",
    "cfg.save": "Save Configuration",
    "cfg.reload": "Reload",
    "cfg.export": "Backup to File (JSON)",
    "cfg.import": "Restore from Backup",
    "cfg.rollback": "Restore Previous (Deprecated)",
    "cfg.rollback_hint": "Please use Profile Switch or Factory Reset for recovery.",
    "cfg.sec_motion_core": "1. Core Motion Sensitivity",
    "cfg.sec_motion_modes": "2. Mode-Specific Motion Parameters",
    "cfg.sec_click_freeze": "3. Anti-Shake Click-Freeze Filter",
    "cfg.sec_ema": "4. Tremor Filter & Smoothing (Adaptive EMA)",
    "cfg.sec_snap": "5. Straight Line Assist (Snap-to-Axis)",
    "cfg.sec_power": "6. Power & Sleep Management",
    "cfg.sec_buttons": "7. Button Response Timings",
    "cfg.sec_preset": "Motion Presets (Quick Apply)",
    "cfg.preset_hint": "Preset applies immediately to the profile currently being edited.",
    "cfg.preset_pc": "PC",
    "cfg.preset_ppt": "PPT",
    "cfg.preset_tv": "TV",
    "cfg.preset_gaming": "Gaming",
    "cfg.preset_precision": "Precision",
    "slots.hint": "Assign functions to 27 trigger actions (15 buttons, 8 gestures, 4 tilts). [Global] applies by default, and can be overridden per mode.",
    "macros.hint": "Automate complex hotkeys or sequential actions with a single click. (Up to 8 macros per profile, up to 8 steps each)",
    "cfg.hint": "Configure cursor speeds, accelerations, gestures, and precision parameters for the active profile.",
    "cfg.foot": "v0412 · Profile + Global/Mode Override Slots + Macro Lib + Motion Advanced",

    /* Config Select Options */
    "cfg.dpi_opt_1": "Level 1: Precision Work (Slow)",
    "cfg.dpi_opt_2": "Level 2: Standard Office/Web (Recommended)",
    "cfg.dpi_opt_3": "Level 3: Large Screen/Fast Move (Fast)",
    "cfg.hard_click_on": "Enabled (ON · Lock cursor position on click)",
    "cfg.hard_click_off": "Disabled (OFF · Allow free cursor motion)",
    "cfg.cf_on": "Enabled (ON · Recommended)",
    "cfg.cf_off": "Disabled (OFF · Free motion)",
    "cfg.ema_rev_on": "Enabled (ON · Recommended · Eliminate lag)",
    "cfg.ema_rev_off": "Disabled (OFF · Continuous filter)",
    "cfg.snap_on": "Enabled (ON · Assist straight lines)",
    "cfg.snap_off": "Disabled (OFF · Completely free motion)",
    "cfg.snap_axis_both": "Support Both X & Y (Recommended)",
    "cfg.snap_axis_h": "Horizontal Lines Only",
    "cfg.snap_axis_v": "Vertical Lines Only",

    /* Config Field Labels (flbl) */
    "cfg.lbl_dpi": "Base Cursor Speed (DPI)",
    "cfg.lbl_hard_click": "Cursor Lock While Clicking",
    "cfg.lbl_led_bright": "Status Indicator (LED) Brightness",
    "cfg.lbl_accel_th": "Acceleration Start Speed (Threshold)",
    "cfg.lbl_scroll_damp": "Cursor Damping Ratio While Scrolling",
    "cfg.lbl_sb": "Mode-Specific Base Speed Scale",
    "cfg.sb0_title": "PC Mode (Rec: 0.55)",
    "cfg.sb1_title": "Presenter Mode (Rec: 0.75)",
    "cfg.sb2_title": "TV Mode (Rec: 1.00)",
    "cfg.lbl_ag": "Mode-Specific Acceleration Gain",
    "cfg.ag0_title": "PC Mode (Rec: 0.35)",
    "cfg.ag1_title": "Presenter Mode (Rec: 0.55)",
    "cfg.ag2_title": "TV Mode (Rec: 0.85)",
    "cfg.lbl_wheel": "Scroll Wheel Sensitivity",
    "cfg.wheel_th_title": "Start Tilt Angle (Rec: 90 deg)",
    "cfg.wheel_step_title": "Max Step Count (Rec: 6)",
    "cfg.lbl_flick": "Air Flick Gesture Sensitivity",
    "cfg.flick_deg_title": "Detection Angular Velocity (Rec: 200 deg/s)",
    "cfg.flick_cd_title": "Repeat Cooldown (Rec: 600 ms)",
    "cfg.lbl_cf_enable": "Cursor Freeze On Click",
    "cfg.lbl_cf_gyro": "Stationary Gyro Threshold",
    "cfg.lbl_cf_max": "Maximum Freeze Duration",
    "cfg.lbl_cf_hold": "Post-Release Freeze Hold Time",
    "cfg.lbl_cf_fadeout": "Damping Fadeout Duration",
    "cfg.lbl_cf_move": "Forced Escape Displacement (px)",
    "cfg.lbl_cf_freeze_move": "Intentional Movement Gyro Threshold",
    "cfg.lbl_ema_min": "Low-Speed Smoothness (Alpha Min)",
    "cfg.lbl_ema_max": "High-Speed Responsiveness (Alpha Max)",
    "cfg.lbl_ema_deadzone": "Low-Speed Filtering Base Threshold",
    "cfg.lbl_ema_fast": "Fast Tracking Transition Threshold",
    "cfg.lbl_ema_reversal": "Sharp Direction Reversal Sensitivity",
    "cfg.lbl_ema_rev_reset": "Sharp Direction Reversal Reset",
    "cfg.lbl_snap_enable": "Axis Snap Feature",
    "cfg.lbl_snap_modes": "Applied Modes Selection",
    "cfg.lbl_snap_axis": "Snap Target Axis",
    "cfg.lbl_snap_ratio": "Axis Dominance Slope Ratio",
    "cfg.lbl_snap_strength": "Straight Line Snap Strength",
    "cfg.lbl_snap_frames": "Axis Confirmation Stabilization Frames",
    "cfg.lbl_pwr_idle": "Stage 1 Light Sleep (sec)",
    "cfg.lbl_pwr_deep": "Stage 2 Ultra-Low Deep Sleep (sec)",
    "cfg.lbl_pwr_wom_th": "Shake Wake-up Sensitivity (WoM)",
    "cfg.lbl_pwr_wom_dur": "Shake Minimum Duration (ms)",
    "cfg.lbl_pwr_led_fade": "Sleep Fade-out Duration",
    "cfg.lbl_pwr_wake_deb": "Wake-up Sensor Stabilization Time",
    "cfg.lbl_btn_deb_press": "Debounce Press (Switch Chatter Reject)",
    "cfg.lbl_btn_deb_rel": "Debounce Release (Bounce Reject)",
    "cfg.lbl_btn_click": "Single Click Max Duration",
    "cfg.lbl_btn_dblclick": "Double Click Max Gap Window",
    "cfg.lbl_btn_long": "Long-Press Detection Time",
    "cfg.lbl_btn_min_click": "Micro Glitch Rejection Time",

    /* Config Field Help Guides (fhelp) */
    "cfg.help_dpi": "<b>Range: 1~3</b> | Default: <b>2 (Standard)</b><br>Selects cursor pointer sensitivity when moving the mouse in free space.",
    "cfg.help_hard_click": "<b>Recommended: ON</b><br>Locks cursor movement momentarily during physical clicks to avoid accidental drift.",
    "cfg.help_led_bright": "<b>Range: 0~255</b> | Recommended: <b>100~150</b> (Default: 128)<br>Adjusts RGB WS2812 brightness. (0 = completely off for maximum battery life)",
    "cfg.help_accel_th": "<b>Range: 1.0~30.0</b> | Recommended: <b>5.0~12.0</b> (Default: 8.0)<br>Threshold speed at which acceleration curves begin ramping up during quick snaps.",
    "cfg.help_scroll_damp": "<b>Range: 0.05~1.00</b> | Recommended: <b>0.20~0.30</b> (Default: 0.25)<br>Reduces pointer speed while holding Side F to prevent cursor jitter during scrolling.",
    "cfg.help_sb": "<b>Recommended: [0.55, 0.75, 1.00]</b> (Range: 0.20~2.00)<br>Base multiplier for Mode 1 (PC), Mode 2 (Presenter), and Mode 3 (Smart TV).",
    "cfg.help_ag": "<b>Recommended: [0.35, 0.55, 0.85]</b> (Range: 0.10~2.00)<br>Acceleration boost gain when snapping the wrist quickly.",
    "cfg.help_wheel": "<b>Default: 90 deg / 6 steps</b> (Recommended: 60~120 deg, 3~10 steps)<br>Tilt angle threshold and maximum step displacement per scroll tick.",
    "cfg.help_flick": "<b>Default: 200 deg/s / 600 ms</b> (Recommended: 150~300 deg/s, 400~800 ms)<br>Angular velocity threshold and cooldown to trigger flick gesture commands.",
    "cfg.help_cf_enable": "<b>Recommended: ON</b><br>Eliminates cursor tremor caused by finger button pressure when clicking in the air.",
    "cfg.help_cf_gyro": "<b>Range: 5.0~30.0 deg/s</b> | Recommended: <b>10.0~20.0</b> (Default: 15.0)<br>Freezes cursor only when wrist angular velocity is below this threshold during clicks.",
    "cfg.help_cf_max": "<b>Range: 50~300 ms</b> | Recommended: <b>100~200 ms</b> (Default: 150 ms)<br>Releases freeze automatically if button is held longer, transitioning to drag mode.",
    "cfg.help_cf_hold": "<b>Range: 0~50 ms</b> | Recommended: <b>10~30 ms</b> (Default: 20 ms)<br>Extra freeze hold time after release to prevent switch spring recoil wobble.",
    "cfg.help_cf_fadeout": "<b>Range: 0~80 ms</b> | Recommended: <b>20~40 ms</b> (Default: 30 ms)<br>Smooth transition period returning from frozen state back to full speed.",
    "cfg.help_cf_move": "<b>Range: 1.0~10.0 px</b> | Recommended: <b>1.5~3.0 px</b> (Default: 2.0 px)<br>Immediate release threshold if wrist moves beyond this distance during freeze.",
    "cfg.help_cf_freeze_move": "<b>Range: 10~60 deg/s</b> | Recommended: <b>25~35 deg/s</b> (Default: 30 deg/s)<br>Wrist rotational speed threshold indicating intentional rapid motion.",
    "cfg.help_ema_min": "<b>Range: 0.01~0.20</b> | Recommended: <b>0.03~0.08</b> (Default: 0.05)<br>Filtering weight at standstill or slow movements. Lower values absorb hand tremors.",
    "cfg.help_ema_max": "<b>Range: 0.50~0.95</b> | Recommended: <b>0.75~0.85</b> (Default: 0.80)<br>Filtering weight at high velocities. Values closer to 1.0 ensure zero-latency response.",
    "cfg.help_ema_deadzone": "<b>Range: 1.0~10.0 rad/s</b> | Recommended: <b>2.0~4.0</b> (Default: 3.0 rad/s)<br>Speed boundary below which maximum tremor reduction filtering is applied.",
    "cfg.help_ema_fast": "<b>Range: 10~30 rad/s</b> | Recommended: <b>12~18</b> (Default: 15 rad/s)<br>Velocity boundary above which filtering latency is eliminated completely.",
    "cfg.help_ema_reversal": "<b>Range: 5.0~20.0 rad/s</b> | Recommended: <b>6.0~10.0</b> (Default: 8.0 rad/s)<br>Threshold to detect rapid wrist reversal direction changes.",
    "cfg.help_ema_rev_reset": "<b>Recommended: ON</b><br>Instantly clears filter phase lag on rapid directional snap reversals.",
    "cfg.help_snap_enable": "<b>Recommended: ON in Mode 2</b><br>Snaps cursor into clean horizontal/vertical straight lines for presentation slides.",
    "cfg.help_snap_modes": "<b>Recommended: Checked only for Mode 2</b><br>Leave disabled for normal desktop tasks, enable for presentation use.",
    "cfg.help_snap_axis": "<b>Recommended: Both X & Y</b><br>Select allowed straight line guidance orientations.",
    "cfg.help_snap_ratio": "<b>Range: 2.0~10.0</b> | Recommended: <b>3.0~5.0</b> (Default: 4.0 = 4:1)<br>Ratio of dominant axis motion required before magnetic axis locking engages.",
    "cfg.help_snap_strength": "<b>Range: 0.50~1.00</b> | Recommended: <b>0.80~0.90</b> (Default: 0.85)<br>1.00 is rigid straight line, 0.85 allows smooth slight deviations.",
    "cfg.help_snap_frames": "<b>Range: 1~10 frames</b> | Recommended: <b>2~4 frames</b> (Default: 3 frames = 24 ms)<br>Consecutive frames required to lock onto an axis cleanly.",
    "cfg.help_pwr_idle": "<b>Range: 0~3600 sec</b> | Recommended: <b>60 sec</b> (0 = disabled)<br>Idle timeout before entering light sleep when disconnected from Bluetooth.",
    "cfg.help_pwr_deep": "<b>Range: 0~86400 sec</b> | Recommended: <b>600 sec (10 min)</b> (0 = disabled)<br>Deep sleep timeout to completely power down sensor and LED to preserve battery.",
    "cfg.help_pwr_wom_th": "<b>Range: 5~150</b> | Recommended: <b>20~35</b> (Default: 25 = ~0.8g)<br>Accelerometer shake sensitivity to wake up sleeping mouse.",
    "cfg.help_pwr_wom_dur": "<b>Range: 1~50 ms</b> | Recommended: <b>2~5 ms</b> (Default: 2 ms)<br>Minimum duration shake must persist to distinguish from incidental bumps.",
    "cfg.help_pwr_led_fade": "<b>Range: 100~3000 ms</b> | Recommended: <b>500~1000 ms</b> (Default: 800 ms)<br>Smooth fadeout duration of the red indicator before sleeping.",
    "cfg.help_pwr_wake_deb": "<b>Range: 50~1000 ms</b> | Recommended: <b>100~200 ms</b> (Default: 150 ms)<br>Stabilization wait window upon wake-up before re-evaluating sleep conditions.",
    "cfg.help_btn_deb_press": "<b>Range: 5~100 ms</b> | Recommended: <b>15~25 ms</b> (Default: 20 ms)<br>Contact chatter rejection window on initial button depression.",
    "cfg.help_btn_deb_rel": "<b>Range: 5~100 ms</b> | Recommended: <b>25~40 ms</b> (Default: 30 ms)<br>Contact bounce rejection window on button release.",
    "cfg.help_btn_click": "<b>Range: 50~1000 ms</b> | Recommended: <b>200~300 ms</b> (Default: 250 ms)<br>Maximum hold duration to qualify as an isolated single click.",
    "cfg.help_btn_dblclick": "<b>Range: 100~1000 ms</b> | Recommended: <b>250~400 ms</b> (Default: 300 ms)<br>Maximum time allowed between consecutive clicks to form a double click.",
    "cfg.help_btn_long": "<b>Range: 200~3000 ms</b> | Recommended: <b>500~800 ms</b> (Default: 600 ms)<br>Minimum duration a button must stay held to trigger long-press actions.",
    "cfg.help_btn_min_click": "<b>Range: 0~100 ms</b> | Recommended: <b>20~40 ms</b> (Default: 30 ms)<br>Pulses shorter than this duration are treated as ESD glitches and discarded.",

    /* Common */
    "btn.apply": "Apply",
    "btn.save": "Save",
    "btn.cancel": "Cancel",
    "btn.close": "Close",
    "btn.test": "Test",

    /* Diagnostics & OTA */
    "diag.title": "Diagnostics & Event Logs",
    "diag.clear": "Clear Counters",
    "diag.auto": "Auto Refresh",
    "diag.filter_ph": "Filter search (e.g. bad_json, i2c)",
    "diag.recent_events": "Recent Events",
    "diag.key_test_title": "Single Key Test",
    "diag.key_test_hint": "(Transmits immediately over Bluetooth connection)",
    "diag.btn_send": "Send Test",
    "diag.key_test_sub": "Keyboard: usage code (dec), Media: bitmask (dec)",
    "diag.raw_json": "View Raw JSON Data",
    "diag.kt_kb": "Standard Keyboard (KB)",
    "diag.kt_consumer": "Media Keys (Consumer)",
    "diag.kt_mod_none": "No Modifiers (None)",
    "diag.kt_mod_lctrl": "Ctrl (Left)",
    "diag.kt_mod_lshift": "Shift (Left)",
    "diag.kt_mod_lalt": "Alt (Left)",
    "diag.kt_mod_lgui": "Win/Cmd (Left)",
    "diag.kt_mod_rctrl": "Ctrl (Right)",
    "diag.kt_mod_rshift": "Shift (Right)",
    "diag.kt_mod_ralt": "Alt (Right)",
    "diag.kt_mod_rgui": "Win/Cmd (Right)",
    "ota.title": "Over-The-Air Firmware Update (OTA)",
    "ota.guard_label": "OTA Security Guard (Block mouse inputs & unauthorized flash)",
    "ota.guard_desc": "When Guard is ON, HID inputs are blocked and unexpected firmware flash attempts will be rejected.",
    "ota.btn_upload": "Upload Firmware",
    "ota.btn_status": "Check Progress",

    /* Popups / Alerts / Prompts */
    "pop.switch_fail": "Profile switch failed:",
    "pop.prof_name_prompt": "New Profile name (max 15 chars):",
    "pop.prof_create_fail": "Profile creation failed:",
    "pop.prof_delete_active_confirm": "⚠ Deleting active Profile (#{idx}).\n\nSystem will automatically switch to another profile.\nContinue?",
    "pop.prof_delete_confirm": "Delete Profile #{idx} \"{name}\"?",
    "pop.prof_delete_fail": "Profile deletion failed:",
    "pop.prof_rename_prompt": "New Profile name (max 15 chars):",
    "pop.prof_rename_fail": "Profile rename failed:",
    "pop.no_action_assigned": "No action assigned to this slot.",
    "pop.live_test_confirm": "Execute Live Test? (kind: {kind})",
    "pop.test_fail": "Test execution failed:",
    "pop.slot_save_fail": "Failed to save slot assignments:",
    "pop.macro_empty": "This macro has no steps defined.",
    "pop.macro_test_confirm": "Execute Macro \"{name}\"?",
    "pop.macro_save_fail": "Macro save failed:",
    "pop.macro_exec_fail": "Macro execution failed:",
    "pop.macro_del_confirm": "Delete Macro \"{name}\"?",
    "pop.macro_new_prompt": "New Macro name (max 15 chars):",
    "pop.force_release_ok": "All held buttons and keys have been released.",
    "pop.safeboot_exit_confirm": "Exit SafeBoot mode and reboot device?",
    "pop.safeboot_exit_fail": "Failed to exit SafeBoot:",
    "pop.reboot_delay_warn": "Reboot is taking longer than expected. Please refresh page.",
    "pop.safemode_enter_confirm": "Entering SafeMode.\nHID inputs will be blocked (web configuration remains accessible).\nContinue?",
    "pop.safemode_exit_confirm": "Exiting SafeMode.\nHID inputs will be resumed.\nContinue?",
    "pop.safemode_change_fail": "Failed to update SafeMode:",
    "pop.factory_reset_confirm": "⚠ Perform Factory Reset?\n\nAll profiles and settings will be permanently erased, and device will reboot to default state.",
    "pop.factory_reset_fail": "Factory Reset failed:",
    "pop.reboot_confirm": "Reboot device now?",
    "pop.reboot_requested": "Reboot requested (reconnecting in ~3 seconds)",
    "pop.reboot_fail": "Reboot request failed:",
    "pop.preset_apply_confirm": "Apply motion preset \"{preset}\" to current Profile?\n(Loaded to UI, click [Save] to persist to device)",
    "pop.preset_applied": "Preset \"{preset}\" applied and saved",
    "pop.diag_reset_confirm": "Reset diagnostics counters and event logs?",
    "pop.diag_reset_fail": "Failed to reset diagnostics:",
    "pop.host_cycle_confirm": "Cycle to next paired Bluetooth host?\n(Connection will drop and reconnect)",
    "pop.host_cycle_fail": "Host cycle failed:",
    "pop.i2c_recover_confirm": "Perform MPU6050 sensor I2C bus recovery?",
    "pop.i2c_recover_fail": "Sensor I2C recovery request failed:",
    "pop.i2c_recover_hardware_warn": "I2C recovery failed.\nPlease check MPU6050 wiring and power supply.",
    "pop.gyro_calib_confirm": "Keep device stationary on a flat, level surface.\nStart Gyro zero calibration now?",
    "pop.gyro_calib_fail": "Gyro calibration request failed:",
    "pop.net_mode_switch_confirm": "Current mode: {current}\nSwitch to {target} mode?",
    "pop.file_select_req": "Please select a firmware binary file first."
  }
};

/**
 * i18n 번역 조회 헬퍼 (파라미터 치환 지원: {var})
 * @param {string} key 번역 키
 * @param {object|string} params 파라미터 맵 또는 기본 반환값
 * @param {string} fallback 기본 반환값
 * @returns {string}
 */
function t(key, params = null, fallback = "") {
  let fb = fallback;
  let p = params;
  if (typeof params === "string") {
    fb = params;
    p = null;
  }
  const langTable = I18N_DICT[g_currLang] || I18N_DICT.ko;
  let str = langTable[key] || (I18N_DICT.ko && I18N_DICT.ko[key]) || fb || key;

  if (p && typeof p === "object") {
    for (const [k, v] of Object.entries(p)) {
      str = str.replace(new RegExp(`\\{${k}\\}`, "g"), String(v));
    }
  }
  return str;
}

/**
 * DOM 전체에서 data-i18n 속성을 찾아 텍스트 교체
 */
function applyI18nToDom() {
  document.querySelectorAll("[data-i18n]").forEach(el => {
    const k = el.getAttribute("data-i18n");
    if (!k) return;
    const txt = t(k);
    if (txt) el.textContent = txt;
  });

  document.querySelectorAll("[data-i18n-title]").forEach(el => {
    const k = el.getAttribute("data-i18n-title");
    if (!k) return;
    const txt = t(k);
    if (txt) el.title = txt;
  });

  document.querySelectorAll("[data-i18n-html]").forEach(el => {
    const k = el.getAttribute("data-i18n-html");
    if (!k) return;
    const txt = t(k);
    if (txt) el.innerHTML = txt;
  });

  document.querySelectorAll("[data-i18n-placeholder]").forEach(el => {
    const k = el.getAttribute("data-i18n-placeholder");
    if (!k) return;
    const txt = t(k);
    if (txt) el.placeholder = txt;
  });

  // 언어 선택 UI 동기화
  const selLang = qs("selLang");
  if (selLang && selLang.value !== g_currLang) {
    selLang.value = g_currLang;
  }
}

/**
 * 언어 변경 및 영구 저장
 * @param {string} langCode "ko" | "en"
 */
function setLanguage(langCode) {
  if (!I18N_DICT[langCode]) langCode = "ko";
  g_currLang = langCode;
  try {
    localStorage.setItem(I18N_STORAGE_KEY, langCode);
  } catch (e) { }

  document.documentElement.lang = langCode;
  applyI18nToDom();

  // 자바스크립트 동적 렌더링 컴포넌트 재호출
  if (typeof renderSlotEditor === "function") renderSlotEditor();
  if (typeof macroRenderList === "function") macroRenderList();
  if (typeof macroRenderEditor === "function") macroRenderEditor();
}

/**
 * i18n 초기화 (페이지 로드 시 자동 호출)
 */
function initI18n() {
  let saved = null;
  try {
    saved = localStorage.getItem(I18N_STORAGE_KEY);
  } catch (e) { }

  if (!saved) {
    const nav = (navigator.language || navigator.userLanguage || "").toLowerCase();
    saved = nav.startsWith("ko") ? "ko" : "en";
  }

  setLanguage(saved);
}
