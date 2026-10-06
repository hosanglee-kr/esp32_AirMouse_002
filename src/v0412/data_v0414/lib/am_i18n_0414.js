/* =======================================================
   File: /www/lib/am_i18n_0414.js
   Elite AirMouse WebConfig v0414 — i18n Core (Full)
======================================================= */

const I18N_STORAGE_KEY = "am_lang_0413";
let g_currLang = "ko";

const I18N_DICT = {
  ko: {
    "top.refresh": "새로 고침", "top.reboot": "다시 시작", "top.reboot_now": "지금 다시 시작",
    "top.reboot_needed": "다시 시작 필요:", "top.language": "언어",
    "loading.processing": "처리 중…",
    "prof.label": "프로필", "prof.new": "+ 새로", "prof.rename": "이름", "prof.delete": "삭제",

    "tab.dash": "홈", "tab.slots": "버튼 및 동작 할당", "tab.macros": "매크로 관리",
    "tab.config": "포인터 및 센서 설정", "tab.diag": "진단 및 입력 테스트", "tab.ota": "무선 업데이트",

    "dash.mode_tap_to_change": "탭하여 모드 변경",
    "dash.mode_tab_1": "모드 1", "dash.mode_tab_2": "모드 2", "dash.mode_tab_3": "모드 3",
    "slots.no_action": "동작 없음", "macro.save_ok": "매크로 저장 완료",
    "dash.mode_btn_1": "모드 1 (PC)", "dash.mode_btn_2": "모드 2 (발표)", "dash.mode_btn_3": "모드 3 (TV)",
    "dash.quick_tune": "포인터 빠른 조정", "dash.quick_tune_hint": "즉시 적용 (기기 재시작 전까지 유지)",
    "dash.dpi_label": "포인터 속도:", "dash.dpi_1": "1단계", "dash.dpi_2": "2단계", "dash.dpi_3": "3단계",
    "dash.prec_label": "포인터 정확도:",
    "dash.prec_off": "끔", "dash.prec_low": "낮음", "dash.prec_med": "보통", "dash.prec_high": "높음", "dash.prec_ppt": "발표용",
    "dash.recent_cmds": "최근 명령 로그",
    "dash.uptime": "동작 시간", "dash.heap": "여유 메모리", "dash.wifi": "와이파이", "dash.cur_prof": "현재 프로필",
    "dash.factory_reset": "공장 초기화",

    "remote.title": "가상 컨트롤러",
    "remote.badge_pc": "모드 1: PC", "remote.badge_ppt": "모드 2: 발표", "remote.badge_tv": "모드 3: TV",
    "remote.guide_pc": "링: 단발 방향키 | 스틱 드래그: 방향 반복 | 스틱 탭: Enter",
    "remote.guide_ppt": "링: 이전/다음 쪽 | 스틱 드래그: 도구 반복 | 스틱 탭: 선택",
    "remote.guide_tv": "링: 4방향 이동 | 스틱 드래그: 틸트 홀드 | 스틱 탭: 확인",
    "remote.sub_start": "시작메뉴", "remote.sub_mute": "음소거", "remote.sub_cancel": "취소", "remote.sub_screenshot": "화면캡처",
    "remote.pad_doc": "문서 스크롤", "remote.pad_ppt_vol": "발표 볼륨", "remote.pad_ch": "채널 전환",
    "remote.ppt_f5": "처음쇼", "remote.ppt_shf5": "현재쇼", "remote.ppt_end": "쇼종료",
    "remote.ppt_laser": "레이저", "remote.ppt_prev": "이전쪽", "remote.ppt_next": "다음쪽",
    "remote.ppt_sel": "선택", "remote.ppt_black": "블랙", "remote.ppt_white": "화이트",
    "remote.ppt_pen": "펜도구", "remote.ppt_arrow": "화살표",
    "remote.tv_power": "⏻ 전원", "remote.tv_back": "↩ 뒤로", "remote.tv_home": "🏠 홈",
    "remote.tv_ok": "확인", "remote.tv_input": "외부 입력", "remote.tv_ch_up": "CH ▲", "remote.tv_ch_down": "CH ▼",

    "preview.apply": "전환", "preview.warn_label": "미리보기", "preview.actual_label": "실제",
    "preview.note": "미리보기 모드의 키가 그대로 전송됩니다.",

    "tools.title": "모드별 추가 도구",
    "tools.pc.window": "창 · 화면",
    "tools.pc.win_max": "창 최대화", "tools.pc.win_left": "좌측 분할", "tools.pc.win_right": "우측 분할",
    "tools.pc.win_desk": "바탕화면 보기", "tools.pc.taskview": "작업 보기",
    "tools.pc.edit": "편집 단축키",
    "tools.pc.copy": "복사", "tools.pc.paste": "붙여넣기", "tools.pc.undo": "실행 취소",
    "tools.pc.select": "전체 선택", "tools.pc.find": "찾기",
    "tools.pc.misc": "기타", "tools.pc.newtab": "새 탭", "tools.pc.close": "탭 닫기", "tools.pc.lock": "화면 잠금",
    "tools.ppt.show": "쇼 제어", "tools.ppt.start": "처음부터 재생", "tools.ppt.start_cur": "현재부터 재생",
    "tools.ppt.end": "쇼 종료", "tools.ppt.screen": "화면", "tools.ppt.black": "검은 화면",
    "tools.ppt.white": "흰 화면", "tools.ppt.pen": "펜 도구", "tools.ppt.laser": "레이저 포인터",
    "tools.ppt.nav": "슬라이드 이동", "tools.ppt.home": "첫 장", "tools.ppt.endpg": "마지막 장",
    "tools.tv.channel": "채널", "tools.tv.ch_up": "채널 ▲", "tools.tv.ch_down": "채널 ▼",
    "tools.tv.media": "미디어 재생", "tools.tv.play": "재생/일시정지", "tools.tv.stop": "정지",
    "tools.tv.next": "다음", "tools.tv.prev": "이전", "tools.tv.nav": "탐색",
    "tools.tv.home": "홈", "tools.tv.back": "뒤로", "tools.tv.input": "외부 입력", "tools.tv.search": "검색",
    "tools.tv.vol": "볼륨", "tools.tv.vol_up": "볼륨 +", "tools.tv.vol_down": "볼륨 -", "tools.tv.mute": "음소거",

    "trouble.title": "문제 해결 도구", "trouble.pointer": "포인터", "trouble.conn": "연결",
    "trouble.system": "시스템", "trouble.safeboot": "세이프부트",
    "trouble.calib": "영점 맞추기", "trouble.release": "눌린 키 강제 해제",
    "trouble.host": "호스트 순환", "trouble.pair": "페어링 모드", "trouble.i2c": "센서 연결 초기화",
    "trouble.sleep": "지금 절전 모드", "trouble.safemode": "안전 모드 토글", "trouble.factory": "공장 초기화",
    "trouble.safeinfo": "안전 부팅 상태", "trouble.safeexit": "안전 부팅 해제",

    "slots.title": "버튼 및 동작 맞춤 할당",
    "slots.hint": "27가지 조작 동작에 원하는 키나 기능을 지정함. [기본 공통]은 모든 모드 기본값, 모드별 오버라이드 가능.",
    "slots.view_global": "기본 공통", "slots.view_mode1": "모드 1: PC", "slots.view_mode2": "모드 2: 발표", "slots.view_mode3": "모드 3: TV",
    "slots.save": "저장", "slots.reload": "다시 불러오기",
    "slots.grp_btn": "버튼 조작", "slots.grp_gesture": "공중 제스처", "slots.grp_tilt": "기울여 유지 (TV 전용)",
    "slots.tilt_global_info": "ℹ 기본 공통의 기울여 유지 동작은 모드 3 (TV)에 자동 적용됨.",
    "slots.tilt_disabled_warn": "⚠ 기울여 유지 동작은 모드 3 (TV)에서만 작동함. 기본 공통 또는 모드 3에서 설정 요망.",
    "slots.badge_g_title": "클릭하여 이 모드 전용 재정의 생성",
    "slots.badge_m_title": "클릭하여 기본 공통으로 되돌리기",
    "slots.badge_m3_title": "모드 3 (TV) 전용 — 기본 공통 상속",

    "macros.title": "연속 동작 매크로 관리",
    "macros.hint": "버튼 한 번으로 복잡한 단축키나 연속 입력을 자동 실행함.",
    "macros.add": "+ 새 매크로", "macros.save": "매크로 저장", "macros.reload": "다시 불러오기",
    "macros.name_label": "매크로 이름:", "macros.step_label": "실행 단계:",
    "macros.name_ph": "매크로 이름 입력", "macros.empty_hint": "등록된 매크로가 없음.",
    "macros.select_hint": "왼쪽 목록에서 매크로를 선택하십시오.",
    "macros.add_step": "+ 단계 추가", "macros.btn_del": "삭제",
    "macros.delay_label": "지연 ms (0~2000)",

    "cfg.title": "포인터 및 센서 세부 설정",
    "cfg.hint": "현재 프로필의 포인터 속도, 가속도, 제스처 및 세부 감도 값을 조정함.",
    "cfg.save": "설정 저장", "cfg.reload": "다시 불러오기",
    "cfg.export": "백업 내보내기", "cfg.import": "백업 가져오기",
    "cfg.sec_motion_core": "1. 기본 포인터 속도 및 감도",
    "cfg.sec_motion_modes": "2. 모드별 포인터 이동 특성",
    "cfg.sec_click_freeze": "3. 클릭 프리즈 (Motion Advanced)",
    "cfg.sec_ema": "4. 적응형 손떨림 (Adaptive EMA)",
    "cfg.sec_snap": "5. 직선 보정 (Snap-to-Axis)",
    "cfg.sec_power": "6. 절전 및 전원 관리",
    "cfg.sec_buttons": "7. 버튼 타이밍",
    "cfg.sec_preset": "추천 동작 프리셋",
    "cfg.preset_hint": "프리셋 선택 시 프로필에 권장 설정 즉시 반영됨.",
    "cfg.preset_pc": "일반 PC", "cfg.preset_ppt": "발표 (PPT)", "cfg.preset_tv": "스마트 TV",
    "cfg.preset_gaming": "게임", "cfg.preset_precision": "정밀 작업",
    "cfg.factory_title": "기기 초기화", "cfg.factory_hint": "모든 프로필과 설정이 지워집니다.",
    "cfg.foot": "v0413 · Drawer Nav + Preview Tabs + Full Features",

    "cfg.lbl_dpi": "기본 포인터 속도", "cfg.lbl_hard_click": "클릭 중 포인터 잠금",
    "cfg.lbl_led_bright": "LED 밝기", "cfg.lbl_accel_th": "가속 시작 속도", "cfg.lbl_scroll_damp": "스크롤 중 감속 비율",
    "cfg.lbl_sb": "모드별 기본 속도 비율", "cfg.lbl_ag": "모드별 가속 증가량",
    "cfg.lbl_wheel": "휠 스크롤 민감도", "cfg.lbl_flick": "휘두르기 제스처 감도",
    "cfg.lbl_cf_enable": "클릭 순간 동결", "cfg.lbl_cf_gyro": "정지 판단 회전 속도",
    "cfg.lbl_cf_max": "최대 동결 시간", "cfg.lbl_cf_hold": "뗀 후 동결 유지",
    "cfg.lbl_cf_fadeout": "복귀 시간", "cfg.lbl_cf_move": "강제 해제 이동 (px)",
    "cfg.lbl_cf_freeze_move": "의도적 이동 판단",
    "cfg.lbl_ema_min": "Alpha Min", "cfg.lbl_ema_max": "Alpha Max",
    "cfg.lbl_ema_deadzone": "손떨림 흡수 기준", "cfg.lbl_ema_fast": "고속 추종 전환",
    "cfg.lbl_ema_reversal": "방향 전환 감지", "cfg.lbl_ema_rev_reset": "방향 전환 리셋",
    "cfg.lbl_snap_enable": "직선 스냅", "cfg.lbl_snap_modes": "적용 모드",
    "cfg.lbl_snap_axis": "스냅 방향", "cfg.lbl_snap_ratio": "기울기 비율",
    "cfg.lbl_snap_strength": "고정 강도", "cfg.lbl_snap_frames": "판정 프레임",
    "cfg.lbl_pwr_idle": "1단계 절전 (초)", "cfg.lbl_pwr_deep": "2단계 절전 (초)",
    "cfg.lbl_pwr_wom_th": "흔들어 깨우기 감도", "cfg.lbl_pwr_wom_dur": "흔들림 유지 (ms)",
    "cfg.lbl_pwr_led_fade": "LED 감쇠 시간", "cfg.lbl_pwr_wake_deb": "깨어난 후 안정화",
    "cfg.lbl_btn_deb_press": "누름 디바운스", "cfg.lbl_btn_deb_rel": "뗌 디바운스",
    "cfg.lbl_btn_click": "단일 클릭 최대", "cfg.lbl_btn_dblclick": "더블클릭 간격",
    "cfg.lbl_btn_long": "롱프레스", "cfg.lbl_btn_min_click": "글리치 필터",

    "cfg.dpi_opt_1": "1단계: 정밀 작업용 (느림)", "cfg.dpi_opt_2": "2단계: 일반 사무 / 웹 탐색 (권장)",
    "cfg.dpi_opt_3": "3단계: 대형 화면 / 빠른 이동 (빠름)",
    "cfg.hard_click_on": "사용", "cfg.hard_click_off": "사용 안 함",
    "cfg.cf_on": "사용", "cfg.cf_off": "사용 안 함",
    "cfg.ema_rev_on": "사용", "cfg.ema_rev_off": "사용 안 함",
    "cfg.snap_on": "사용", "cfg.snap_off": "사용 안 함",
    "cfg.snap_axis_both": "가로/세로", "cfg.snap_axis_h": "가로만", "cfg.snap_axis_v": "세로만",

    "diag.title": "진단 및 입력 테스트",
    "diag.clear": "카운터 초기화", "diag.auto": "자동 새로 고침",
    "diag.recent_events": "최근 발생 이벤트",
    "diag.key_test_title": "입력 신호 테스트 (Key Test)",
    "diag.btn_send": "전송", "diag.raw_json": "Raw JSON 보기",
    "diag.kt_kb": "키보드 (KB)", "diag.kt_consumer": "미디어 (Consumer)",

    "ota.title": "무선 소프트웨어 업데이트 (OTA)",
    "ota.guard_label": "업데이트 보안 잠금 (OTA Guard)",
    "ota.guard_desc": "잠금 시 마우스/키보드 입력 일시 중단, 비인가 펌웨어 차단.",
    "ota.btn_upload": "업로드", "ota.btn_status": "상태 확인",

    "btn.apply": "적용", "btn.save": "저장", "btn.cancel": "취소", "btn.close": "닫기", "btn.test": "테스트",

    "pop.mode_switch_confirm": "모드를 {cur} → {tgt}(으)로 전환하시겠습니까?",
    "pop.mode_switch_ok": "모드 {tgt} 전환 완료",
    "pop.calib_ok": "영점 보정 요청 완료", "pop.force_release_ok": "눌린 키 강제 해제 완료",
    "pop.host_cycle_ok": "호스트 순환 요청 완료", "pop.pair_ok": "페어링 모드 진입 요청 완료",
    "pop.i2c_ok": "I2C 복구 요청 완료", "pop.sleep_ok": "절전 모드 예약됨",
    "pop.safemode_on": "안전 모드 ON", "pop.safemode_off": "안전 모드 OFF",
    "pop.factory_confirm": "공장 초기화하시겠습니까? 모든 프로필과 설정이 지워집니다.",
    "pop.factory_ok": "초기화 완료, 새로고침합니다",
    "pop.reboot_confirm": "기기를 다시 시작하시겠습니까?",
    "pop.reboot_requested": "다시 시작 요청 완료", "pop.reboot_fail": "다시 시작 실패:",
    "pop.reboot_delay_warn": "기기 응답 지연. 잠시 후 새로 고침하세요.",
    "pop.dpi_applied": "DPI {v} 적용", "pop.prec_applied": "Precision {v} 적용",
    "pop.key_test_ok": "Key Test OK · {page} mod={mod} code={code}",
    "pop.switch_fail": "프로필 전환 실패:",
    "pop.prof_name_prompt": "새 프로필 이름 입력 (최대 15자):",
    "pop.prof_create_fail": "프로필 생성 실패:",
    "pop.prof_delete_active_confirm": "⚠ 현재 사용 중인 프로필(#{idx})을 삭제함.\n\n삭제 후 다른 프로필로 자동 전환됨.\n계속하시겠습니까?",
    "pop.prof_delete_confirm": "프로필 #{idx} \"{name}\"을(를) 삭제하시겠습니까?",
    "pop.prof_delete_fail": "프로필 삭제 실패:",
    "pop.prof_rename_prompt": "프로필 이름 변경 (최대 15자):",
    "pop.prof_rename_fail": "프로필 이름 변경 실패:",
    "pop.no_action_assigned": "할당된 동작이 없음.",
    "pop.live_test_confirm": "동작 테스트를 실행하시겠습니까? (유형: {kind})",
    "pop.test_fail": "동작 테스트 실패:",
    "pop.slot_save_fail": "동작 할당 저장 실패:",
    "pop.macro_empty": "실행 단계가 비어 있는 매크로임.",
    "pop.macro_test_confirm": "매크로 \"{name}\"을(를) 테스트 실행하시겠습니까?",
    "pop.macro_save_fail": "매크로 저장 실패:",
    "pop.macro_exec_fail": "매크로 실행 실패:",
    "pop.macro_del_confirm": "매크로 \"{name}\"을(를) 삭제하시겠습니까?",
    "pop.macro_new_prompt": "새 매크로 이름 입력 (최대 15자):",
    "pop.safeboot_exit_confirm": "안전 부팅 모드를 종료하고 기기를 다시 시작하시겠습니까?",
    "pop.safeboot_exit_fail": "안전 부팅 종료 실패:",
    "pop.safemode_enter_confirm": "안전 모드로 진입함. 마우스/키보드 입력이 차단됨. 계속?",
    "pop.safemode_exit_confirm": "안전 모드를 종료함. 입력이 재개됨. 계속?",
    "pop.safemode_change_fail": "안전 모드 변경 실패:",
    "pop.factory_reset_fail": "공장 초기화 실패:",
    "pop.preset_apply_confirm": "동작 프리셋 \"{preset}\" 설정을 현재 프로필에 적용하시겠습니까?",
    "pop.preset_applied": "프리셋 \"{preset}\" 적용 및 저장 완료",
    "pop.diag_reset_confirm": "진단 카운터와 이벤트 기록을 초기화하시겠습니까?",
    "pop.diag_reset_fail": "진단 카운터 초기화 실패:",
    "pop.host_cycle_confirm": "등록된 다른 블루투스 기기로 전환하시겠습니까?",
    "pop.host_cycle_fail": "기기 전환 실패:",
    "pop.i2c_recover_confirm": "센서(MPU6050) I2C 통신 버스 복구를 시작하시겠습니까?",
    "pop.i2c_recover_fail": "센서 I2C 복구 요청 실패:",
    "pop.i2c_recover_hardware_warn": "센서 통신 복구 실패. 하드웨어 연결을 점검하세요.",
    "pop.gyro_calib_confirm": "기기를 평평한 바닥에 두고 움직이지 마십시오.\n수평·영점 보정을 시작하시겠습니까?",
    "pop.gyro_calib_fail": "보정 요청 실패:",
    "pop.net_mode_switch_confirm": "현재: {current}\n{target} 모드로 전환하시겠습니까?",
    "pop.file_select_req": "업로드할 펌웨어 파일(.bin)을 선택하십시오.",
    "pop.trouble_fail": "요청 실패:",

    "log.none": "아직 명령 없음", "cfg.save_ok": "설정 저장 (시뮬)", "slot.save_ok": "슬롯 저장 (시뮬)"
  },

  en: {
    "top.refresh": "Refresh", "top.reboot": "Restart", "top.reboot_now": "Restart Now",
    "top.reboot_needed": "Restart Required:", "top.language": "Language",
    "loading.processing": "Processing…",
    "prof.label": "Profile", "prof.new": "+ New", "prof.rename": "Rename", "prof.delete": "Delete",

    "tab.dash": "Home", "tab.slots": "Button Assign", "tab.macros": "Macros",
    "tab.config": "Pointer & Sensor", "tab.diag": "Diagnostics", "tab.ota": "OTA Update",

    "dash.mode_tap_to_change": "Tap to change mode",
    "dash.mode_btn_1": "Mode 1 (PC)", "dash.mode_btn_2": "Mode 2 (Present)", "dash.mode_btn_3": "Mode 3 (TV)",
    "dash.quick_tune": "Quick Pointer Tuning", "dash.quick_tune_hint": "Applied immediately (until restart)",
    "dash.dpi_label": "Speed:", "dash.dpi_1": "L1", "dash.dpi_2": "L2", "dash.dpi_3": "L3",
    "dash.prec_label": "Precision:",
    "dash.prec_off": "Off", "dash.prec_low": "Low", "dash.prec_med": "Med", "dash.prec_high": "High", "dash.prec_ppt": "Present",
    "dash.recent_cmds": "Recent Command Log",
    "dash.uptime": "Uptime", "dash.heap": "Free Mem", "dash.wifi": "Wi-Fi", "dash.cur_prof": "Active Profile",
    "dash.factory_reset": "Factory Reset",

    "remote.title": "Virtual Controller",
    "remote.badge_pc": "Mode 1: PC", "remote.badge_ppt": "Mode 2: Present", "remote.badge_tv": "Mode 3: TV",
    "remote.guide_pc": "Ring: tap arrows | Stick drag: repeat | Stick tap: Enter",
    "remote.guide_ppt": "Ring: Prev/Next | Stick drag: tool repeat | Stick tap: Select",
    "remote.guide_tv": "Ring: 4-way nav | Stick drag: tilt hold | Stick tap: OK",
    "remote.sub_start": "Start", "remote.sub_mute": "Mute", "remote.sub_cancel": "Cancel", "remote.sub_screenshot": "Screenshot",
    "remote.pad_doc": "Doc Scroll", "remote.pad_ppt_vol": "Pres. Volume", "remote.pad_ch": "Channel",
    "remote.ppt_f5": "Start", "remote.ppt_shf5": "From Current", "remote.ppt_end": "End",
    "remote.ppt_laser": "Laser", "remote.ppt_prev": "Prev", "remote.ppt_next": "Next",
    "remote.ppt_sel": "Select", "remote.ppt_black": "Black", "remote.ppt_white": "White",
    "remote.ppt_pen": "Pen", "remote.ppt_arrow": "Arrow",
    "remote.tv_power": "⏻ Power", "remote.tv_back": "↩ Back", "remote.tv_home": "🏠 Home",
    "remote.tv_ok": "OK", "remote.tv_input": "Input", "remote.tv_ch_up": "CH ▲", "remote.tv_ch_down": "CH ▼",

    "preview.apply": "Apply", "preview.warn_label": "Preview", "preview.actual_label": "Actual",
    "preview.note": "Keys of the previewed mode are transmitted as-is.",

    "tools.title": "Mode-specific Tools",
    "tools.pc.window": "Window & Screen",
    "tools.pc.win_max": "Maximize", "tools.pc.win_left": "Snap Left", "tools.pc.win_right": "Snap Right",
    "tools.pc.win_desk": "Show Desktop", "tools.pc.taskview": "Task View",
    "tools.pc.edit": "Edit Shortcuts",
    "tools.pc.copy": "Copy", "tools.pc.paste": "Paste", "tools.pc.undo": "Undo",
    "tools.pc.select": "Select All", "tools.pc.find": "Find",
    "tools.pc.misc": "Misc", "tools.pc.newtab": "New Tab", "tools.pc.close": "Close Tab", "tools.pc.lock": "Lock Screen",
    "tools.ppt.show": "Show Control", "tools.ppt.start": "Start Show", "tools.ppt.start_cur": "From Current",
    "tools.ppt.end": "End Show", "tools.ppt.screen": "Screen", "tools.ppt.black": "Black",
    "tools.ppt.white": "White", "tools.ppt.pen": "Pen Tool", "tools.ppt.laser": "Laser Pointer",
    "tools.ppt.nav": "Slide Navigation", "tools.ppt.home": "First Slide", "tools.ppt.endpg": "Last Slide",
    "tools.tv.channel": "Channel", "tools.tv.ch_up": "CH ▲", "tools.tv.ch_down": "CH ▼",
    "tools.tv.media": "Media", "tools.tv.play": "Play/Pause", "tools.tv.stop": "Stop",
    "tools.tv.next": "Next", "tools.tv.prev": "Previous", "tools.tv.nav": "Navigation",
    "tools.tv.home": "Home", "tools.tv.back": "Back", "tools.tv.input": "Input", "tools.tv.search": "Search",
    "tools.tv.vol": "Volume", "tools.tv.vol_up": "Vol +", "tools.tv.vol_down": "Vol -", "tools.tv.mute": "Mute",

    "trouble.title": "Troubleshoot", "trouble.pointer": "Pointer", "trouble.conn": "Connection",
    "trouble.system": "System", "trouble.safeboot": "SafeBoot",
    "trouble.calib": "Calibrate", "trouble.release": "Release Held Keys",
    "trouble.host": "Host Cycle", "trouble.pair": "Pairing Mode", "trouble.i2c": "I2C Recovery",
    "trouble.sleep": "Sleep Now", "trouble.safemode": "Safe Mode Toggle", "trouble.factory": "Factory Reset",
    "trouble.safeinfo": "SafeBoot Status", "trouble.safeexit": "Exit SafeBoot",

    "slots.title": "Button Assignments",
    "slots.hint": "Assign keys or functions to 27 trigger actions. [Default] applies across all modes and can be overridden per mode.",
    "slots.view_global": "Default", "slots.view_mode1": "Mode 1: PC", "slots.view_mode2": "Mode 2: Present", "slots.view_mode3": "Mode 3: TV",
    "slots.save": "Save", "slots.reload": "Reload",
    "slots.grp_btn": "Button Actions", "slots.grp_gesture": "Air Gestures", "slots.grp_tilt": "Tilt Hold (TV Only)",
    "slots.tilt_global_info": "ℹ Tilt Hold actions in Default are auto-inherited by Mode 3 (TV).",
    "slots.tilt_disabled_warn": "⚠ Tilt Hold works only in Mode 3 (TV). Configure in Default or Mode 3.",
    "slots.badge_g_title": "Click to override for this mode",
    "slots.badge_m_title": "Click to restore to Default",
    "slots.badge_m3_title": "Mode 3 (TV) Only — Inherits Default",

    "macros.title": "Macro Manager",
    "macros.hint": "Automate sequential keystrokes with a single click.",
    "macros.add": "+ New Macro", "macros.save": "Save Macros", "macros.reload": "Reload",
    "macros.name_label": "Macro Name:", "macros.step_label": "Steps:",
    "macros.name_ph": "Enter macro name", "macros.empty_hint": "No macros registered.",
    "macros.select_hint": "Select a macro from the list on the left.",
    "macros.add_step": "+ Add Step", "macros.btn_del": "Delete",
    "macros.delay_label": "Delay ms (0~2000)",

    "cfg.title": "Pointer & Sensor Settings",
    "cfg.hint": "Configure pointer speed, acceleration, gestures, and precision for the selected profile.",
    "cfg.save": "Save Settings", "cfg.reload": "Reload",
    "cfg.export": "Export Backup", "cfg.import": "Import Backup",
    "cfg.sec_motion_core": "1. Core Pointer Speed & Sensitivity",
    "cfg.sec_motion_modes": "2. Mode-specific Pointer Dynamics",
    "cfg.sec_click_freeze": "3. Click-Freeze (Motion Advanced)",
    "cfg.sec_ema": "4. Adaptive EMA",
    "cfg.sec_snap": "5. Snap-to-Axis",
    "cfg.sec_power": "6. Power Management",
    "cfg.sec_buttons": "7. Button Timings",
    "cfg.sec_preset": "Motion Presets",
    "cfg.preset_hint": "Selecting a preset loads recommended settings to the profile.",
    "cfg.preset_pc": "Desktop PC", "cfg.preset_ppt": "Presentation", "cfg.preset_tv": "Smart TV",
    "cfg.preset_gaming": "Gaming", "cfg.preset_precision": "Precision",
    "cfg.factory_title": "Device Reset", "cfg.factory_hint": "All profiles and settings will be erased.",
    "cfg.foot": "v0413 · Drawer Nav + Preview Tabs + Full Features",

    "cfg.lbl_dpi": "Base Pointer Speed", "cfg.lbl_hard_click": "Lock Pointer During Clicks",
    "cfg.lbl_led_bright": "LED Brightness", "cfg.lbl_accel_th": "Acceleration Start Speed",
    "cfg.lbl_scroll_damp": "Damping During Scroll",
    "cfg.lbl_sb": "Mode Base Speed Multiplier", "cfg.lbl_ag": "Mode Acceleration Gain",
    "cfg.lbl_wheel": "Wheel Sensitivity", "cfg.lbl_flick": "Flick Gesture Sensitivity",
    "cfg.lbl_cf_enable": "Click-Freeze", "cfg.lbl_cf_gyro": "Stationary Gyro Threshold",
    "cfg.lbl_cf_max": "Max Freeze Duration", "cfg.lbl_cf_hold": "Post-Release Hold",
    "cfg.lbl_cf_fadeout": "Return Duration", "cfg.lbl_cf_move": "Force-Escape Distance (px)",
    "cfg.lbl_cf_freeze_move": "Intentional Movement Threshold",
    "cfg.lbl_ema_min": "Alpha Min", "cfg.lbl_ema_max": "Alpha Max",
    "cfg.lbl_ema_deadzone": "Tremor Absorption Threshold", "cfg.lbl_ema_fast": "Fast Tracking Transition",
    "cfg.lbl_ema_reversal": "Reversal Detection", "cfg.lbl_ema_rev_reset": "Reversal Reset",
    "cfg.lbl_snap_enable": "Axis Snap", "cfg.lbl_snap_modes": "Applied Modes",
    "cfg.lbl_snap_axis": "Snap Axis", "cfg.lbl_snap_ratio": "Ratio Threshold",
    "cfg.lbl_snap_strength": "Strength", "cfg.lbl_snap_frames": "Confirm Frames",
    "cfg.lbl_pwr_idle": "Stage 1 Sleep (s)", "cfg.lbl_pwr_deep": "Stage 2 Deep Sleep (s)",
    "cfg.lbl_pwr_wom_th": "Wake Sensitivity", "cfg.lbl_pwr_wom_dur": "Shake Duration (ms)",
    "cfg.lbl_pwr_led_fade": "LED Fade Time", "cfg.lbl_pwr_wake_deb": "Wake Debounce",
    "cfg.lbl_btn_deb_press": "Debounce Press", "cfg.lbl_btn_deb_rel": "Debounce Release",
    "cfg.lbl_btn_click": "Click Max", "cfg.lbl_btn_dblclick": "Double-Click Gap",
    "cfg.lbl_btn_long": "Long-Press", "cfg.lbl_btn_min_click": "Glitch Filter",

    "cfg.dpi_opt_1": "L1: Precision (Slow)", "cfg.dpi_opt_2": "L2: Office / Web (Recommended)",
    "cfg.dpi_opt_3": "L3: Large Display / Fast",
    "cfg.hard_click_on": "Enabled", "cfg.hard_click_off": "Disabled",
    "cfg.cf_on": "Enabled", "cfg.cf_off": "Disabled",
    "cfg.ema_rev_on": "Enabled", "cfg.ema_rev_off": "Disabled",
    "cfg.snap_on": "Enabled", "cfg.snap_off": "Disabled",
    "cfg.snap_axis_both": "Both X & Y", "cfg.snap_axis_h": "Horizontal", "cfg.snap_axis_v": "Vertical",

    "diag.title": "Diagnostics & Input Test",
    "diag.clear": "Clear Counters", "diag.auto": "Auto Refresh",
    "diag.recent_events": "Recent Events",
    "diag.key_test_title": "Single Key Test",
    "diag.btn_send": "Send", "diag.raw_json": "View Raw JSON",
    "diag.kt_kb": "Keyboard (KB)", "diag.kt_consumer": "Media (Consumer)",

    "ota.title": "Over-The-Air Update (OTA)",
    "ota.guard_label": "OTA Security Guard",
    "ota.guard_desc": "When ON, mouse/keyboard inputs are suspended and unauthorized flashes are rejected.",
    "ota.btn_upload": "Upload", "ota.btn_status": "Check Status",

    "btn.apply": "Apply", "btn.save": "Save", "btn.cancel": "Cancel", "btn.close": "Close", "btn.test": "Test",

    "pop.mode_switch_confirm": "Switch mode {cur} → {tgt}?",
    "pop.mode_switch_ok": "Mode {tgt} switched",
    "pop.calib_ok": "Calibration requested", "pop.force_release_ok": "All held keys released",
    "pop.host_cycle_ok": "Host cycle requested", "pop.pair_ok": "Pairing mode requested",
    "pop.i2c_ok": "I2C recovery requested", "pop.sleep_ok": "Sleep scheduled",
    "pop.safemode_on": "Safe Mode ON", "pop.safemode_off": "Safe Mode OFF",
    "pop.factory_confirm": "Factory reset? All profiles and settings will be erased.",
    "pop.factory_ok": "Reset complete, reloading",
    "pop.reboot_confirm": "Restart device now?",
    "pop.reboot_requested": "Restart requested", "pop.reboot_fail": "Restart failed:",
    "pop.reboot_delay_warn": "Device is taking longer than expected. Refresh later.",
    "pop.dpi_applied": "DPI {v} applied", "pop.prec_applied": "Precision {v} applied",
    "pop.key_test_ok": "Key Test OK · {page} mod={mod} code={code}",
    "pop.switch_fail": "Profile switch failed:",
    "pop.prof_name_prompt": "Enter new profile name (max 15):",
    "pop.prof_create_fail": "Profile creation failed:",
    "pop.prof_delete_active_confirm": "⚠ Deleting active Profile (#{idx}).\n\nSystem will switch to another profile.\nContinue?",
    "pop.prof_delete_confirm": "Delete Profile #{idx} \"{name}\"?",
    "pop.prof_delete_fail": "Profile deletion failed:",
    "pop.prof_rename_prompt": "Rename profile (max 15):",
    "pop.prof_rename_fail": "Profile rename failed:",
    "pop.no_action_assigned": "No action assigned to this slot.",
    "pop.live_test_confirm": "Execute live test? (kind: {kind})",
    "pop.test_fail": "Test failed:",
    "pop.slot_save_fail": "Failed to save slot assignments:",
    "pop.macro_empty": "This macro has no steps.",
    "pop.macro_test_confirm": "Execute Macro \"{name}\"?",
    "pop.macro_save_fail": "Macro save failed:",
    "pop.macro_exec_fail": "Macro execution failed:",
    "pop.macro_del_confirm": "Delete Macro \"{name}\"?",
    "pop.macro_new_prompt": "Enter new macro name (max 15):",
    "pop.safeboot_exit_confirm": "Exit SafeBoot and restart?",
    "pop.safeboot_exit_fail": "Exit SafeBoot failed:",
    "pop.safemode_enter_confirm": "Enter Safe Mode? Inputs will be suspended.",
    "pop.safemode_exit_confirm": "Exit Safe Mode? Inputs will resume.",
    "pop.safemode_change_fail": "Safe mode change failed:",
    "pop.factory_reset_fail": "Factory reset failed:",
    "pop.preset_apply_confirm": "Apply motion preset \"{preset}\"?",
    "pop.preset_applied": "Preset \"{preset}\" applied",
    "pop.diag_reset_confirm": "Reset diagnostics counters and events?",
    "pop.diag_reset_fail": "Diagnostics reset failed:",
    "pop.host_cycle_confirm": "Switch to next paired Bluetooth device?",
    "pop.host_cycle_fail": "Device switch failed:",
    "pop.i2c_recover_confirm": "Start I2C bus recovery?",
    "pop.i2c_recover_fail": "I2C recovery request failed:",
    "pop.i2c_recover_hardware_warn": "I2C recovery failed. Check hardware wiring.",
    "pop.gyro_calib_confirm": "Keep device stationary on a flat surface.\nStart calibration?",
    "pop.gyro_calib_fail": "Calibration request failed:",
    "pop.net_mode_switch_confirm": "Current: {current}\nSwitch to {target}?",
    "pop.file_select_req": "Please select a firmware file (.bin).",
    "pop.trouble_fail": "Request failed:",

    "log.none": "No commands yet", "cfg.save_ok": "Saved (simulated)", "slot.save_ok": "Slots saved (simulated)"
  }
};

function t(key, params) {
  const dict = I18N_DICT[g_currLang] || I18N_DICT.ko;
  let s = dict[key] || (I18N_DICT.ko[key]) || key;
  if (params && typeof params === "object") {
    for (const k of Object.keys(params)) s = s.split(`{${k}}`).join(String(params[k]));
  }
  return s;
}

function applyI18nToDom() {
  document.querySelectorAll("[data-i18n]").forEach(el => {
    const s = t(el.getAttribute("data-i18n")); if (s) el.textContent = s;
  });
  document.querySelectorAll("[data-i18n-html]").forEach(el => {
    const s = t(el.getAttribute("data-i18n-html")); if (s) el.innerHTML = s;
  });
  document.querySelectorAll("[data-i18n-title]").forEach(el => {
    const s = t(el.getAttribute("data-i18n-title")); if (s) el.title = s;
  });
  document.querySelectorAll("[data-i18n-placeholder]").forEach(el => {
    const s = t(el.getAttribute("data-i18n-placeholder")); if (s) el.placeholder = s;
  });
}

function setLanguage(lang) {
  if (!I18N_DICT[lang]) lang = "ko";
  g_currLang = lang;
  try { localStorage.setItem(I18N_STORAGE_KEY, lang); } catch (e) { }
  document.documentElement.lang = lang;
  applyI18nToDom();

  /* app 함수 재렌더 (있으면) */
  const m = (typeof g_currentActiveMode === "number") ? g_currentActiveMode : ((typeof g_previewMode === "number") ? g_previewMode : 1);
  if (typeof _renderController === "function") _renderController(m);
  if (typeof _updateModeSelectorSummary === "function") _updateModeSelectorSummary();
  if (typeof _updatePreviewWarn === "function") _updatePreviewWarn();
  if (typeof renderModeTools === "function") renderModeTools();
  if (typeof renderSlotEditor === "function") renderSlotEditor();
  if (typeof macroRenderList === "function") macroRenderList();
  if (typeof macroRenderEditor === "function") macroRenderEditor();
}

function initI18n() {
  let saved = null;
  try { saved = localStorage.getItem(I18N_STORAGE_KEY); } catch (e) { }
  if (!saved) {
    const nav = (navigator.language || "").toLowerCase();
    saved = nav.startsWith("ko") ? "ko" : "en";
  }
  setLanguage(saved);
}