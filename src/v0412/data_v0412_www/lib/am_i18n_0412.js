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
    "prof.label": "프로파일 (Profile)",
    "prof.new": "+ 새로",
    "prof.rename": "이름",
    "prof.delete": "삭제",
    "prof.reload_title": "재로드",
    "loading.processing": "처리 중…",

    /* Tabs */
    "tab.dash": "대시보드 (Dashboard)",
    "tab.slots": "동작 배정 (Slots)",
    "tab.macros": "연속 동작 (Macros)",
    "tab.config": "세부 설정 (Config)",
    "tab.diag": "문제 진단 (Diagnostics)",
    "tab.ota": "펌웨어 업데이트 (OTA)",

    /* Dashboard */
    "dash.status_title": "현재 상태 (Status)",
    "dash.uptime": "가동 시간 (Uptime)",
    "dash.heap": "여유 메모리 (Heap Free)",
    "dash.wifi": "와이파이 연결 (WiFi)",
    "dash.cur_prof": "현재 프로파일 (Profile)",
    "dash.quick_ctl": "빠른 제어 (Quick Control)",
    "dash.ppt_on": "발표 모드 ON",
    "dash.ppt_off": "발표 모드 OFF",
    "dash.calib": "센서 영점 맞추기 (자이로 캘리브)",
    "dash.force_rel": "키 누름 강제 해제",
    "dash.i2c_rec": "센서 연결 초기화 (I2C)",
    "dash.quick_tune": "빠른 튜닝 (Quick Tuning)",
    "dash.quick_tune_hint": "(임시 적용 · 기기 재부팅 시 원래대로 복구됨)",
    "dash.dpi_label": "커서 속도 (DPI):",
    "dash.dpi_1": "1 (느림)",
    "dash.dpi_2": "2 (표준)",
    "dash.dpi_3": "3 (빠름)",
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

    /* Macros */
    "macros.title": "연속 동작 관리자 (Macros)",
    "macros.add": "+ 새 매크로 생성",
    "macros.save": "매크로 저장",
    "macros.reload": "되돌리기 (재로드)",
    "macros.name_label": "매크로 이름:",
    "macros.step_label": "실행 단계 (Steps):",

    /* Config Sections */
    "cfg.title": "기기 세부 환경 설정 (Device Config)",
    "cfg.save": "설정 저장하기",
    "cfg.reload": "되돌리기",
    "cfg.export": "설정 파일로 백업 (JSON)",
    "cfg.import": "백업 파일 불러오기",
    "cfg.sec_motion_core": "1. 기본 마우스 감도 (Core Motion)",
    "cfg.sec_motion_modes": "2. 모드별 세부 동작 특성 (3 Modes Motion)",
    "cfg.sec_click_freeze": "3. 클릭 시 커서 흔들림 방지 (Click-Freeze)",
    "cfg.sec_ema": "4. 손떨림 방지 및 가속 스무딩 (Adaptive EMA Filter)",
    "cfg.sec_snap": "5. 직선 그리기 보정 (Snap-to-Axis)",
    "cfg.sec_power": "6. 배터리 절전 및 전원 관리 (Power Management)",
    "cfg.sec_buttons": "7. 버튼 클릭/더블클릭 반응 속도 (Button Timings)",

    /* Common */
    "btn.apply": "적용",
    "btn.save": "저장",
    "btn.cancel": "취소",
    "btn.close": "닫기",
    "btn.test": "테스트"
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
    "tab.slots": "Slots",
    "tab.macros": "Macros",
    "tab.config": "Config",
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

    /* Macros */
    "macros.title": "Macro Sequence Manager",
    "macros.add": "+ New Macro",
    "macros.save": "Save Macros",
    "macros.reload": "Reload",
    "macros.name_label": "Macro Name:",
    "macros.step_label": "Steps:",

    /* Config Sections */
    "cfg.title": "Device Configuration",
    "cfg.save": "Save Configuration",
    "cfg.reload": "Reload",
    "cfg.export": "Backup to File (JSON)",
    "cfg.import": "Restore from Backup",
    "cfg.sec_motion_core": "1. Core Motion Sensitivity",
    "cfg.sec_motion_modes": "2. Mode-Specific Motion Parameters",
    "cfg.sec_click_freeze": "3. Anti-Shake Click-Freeze Filter",
    "cfg.sec_ema": "4. Tremor Filter & Smoothing (Adaptive EMA)",
    "cfg.sec_snap": "5. Straight Line Assist (Snap-to-Axis)",
    "cfg.sec_power": "6. Power & Sleep Management",
    "cfg.sec_buttons": "7. Button Response Timings",

    /* Common */
    "btn.apply": "Apply",
    "btn.save": "Save",
    "btn.cancel": "Cancel",
    "btn.close": "Close",
    "btn.test": "Test"
  }
};

/**
 * i18n 번역 조회 헬퍼
 * @param {string} key 번역 키
 * @param {string} fallback 기본 반환값
 * @returns {string}
 */
function t(key, fallback = "") {
  const langTable = I18N_DICT[g_currLang] || I18N_DICT.ko;
  return langTable[key] || (I18N_DICT.ko && I18N_DICT.ko[key]) || fallback || key;
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
  } catch (e) {}

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
  } catch (e) {}

  if (!saved) {
    const nav = (navigator.language || navigator.userLanguage || "").toLowerCase();
    saved = nav.startsWith("ko") ? "ko" : "en";
  }

  setLanguage(saved);
}
