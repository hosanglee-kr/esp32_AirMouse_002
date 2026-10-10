/* =======================================================
   File: /www/lib/am_base_0415.js
   Elite AirMouse WebConfig v0415 — Base Utilities
   - 로드 순서: 1
   - [v0415] Consumer mask Descriptor 정합 (AC_* → WWW_*, POWER/CH/TV_INPUT 삭제)
   - [v0415] Keyboard Page Power (0x66) 그룹 추가
   ======================================================= */

/* ---------------- 모드 상수 ---------------- */
const APP_MODE_ONLINE = "ONLINE";
const APP_MODE_OFFLINE = "OFFLINE";

/* ---------------- DOM 유틸 ---------------- */
function qs(id) { return document.getElementById(id); }
function qsa(sel) { return document.querySelectorAll(sel); }

/* ---------------- 전역 상태 ---------------- */
let g_appMode = (window.location.protocol === "file:") ? APP_MODE_OFFLINE : APP_MODE_ONLINE;
let g_keycodes = null;
let g_triggers = [];
let g_profile = null;
let g_view = "global";
let g_macroSel = -1;
let g_lastStatus = null;
let g_diagTypingUntilMs = 0;
let g_currentActiveMode = 1;
let g_isRemoteFolded = false;
const REMOTE_FOLD_KEY = "am_remote_folded_0415";

/* ---------------- [L-5] Unsaved changes 추적 ---------------- */
let g_cfgDirty  = false;
let g_slotDirty = false;

function markCfgDirty()      { g_cfgDirty = true; }
function markSlotDirty()     { g_slotDirty = true; }

function clearCfgDirty()     { g_cfgDirty  = false; }
function clearSlotDirty()    { g_slotDirty = false; }
function clearDirty()        { g_cfgDirty = false; g_slotDirty = false; }

function hasUnsavedChanges() { return g_cfgDirty || g_slotDirty; }

function nowMs() { return Date.now(); }

/* ---------------- 공통 유틸 ---------------- */
function pretty(o) {
  try { return JSON.stringify(o, null, 2); } catch (e) { return String(o); }
}

function parseIntFlex(v, def = 0) {
  if (v === null || v === undefined) return def;
  const s = String(v).trim();
  if (!s) return def;
  if (/^0x[0-9a-f]+$/i.test(s)) return parseInt(s, 16);
  const n = Number(s);
  return Number.isFinite(n) ? Math.trunc(n) : def;
}

function parseNum(v, def = 0) {
  const n = Number(v);
  return Number.isFinite(n) ? n : def;
}

function parseBool(v) {
  if (v === true || v === false) return v;
  const s = String(v).toLowerCase().trim();
  return s === "true" || s === "1" || s === "on";
}

function setMsg(text, good) {
  const el = qs("cfgMsg"); if (!el) return;
  el.textContent = text;
  el.style.borderColor = good ? "rgba(76,125,255,.55)" : "rgba(255,77,77,.55)";
  el.style.background = good ? "rgba(76,125,255,.10)" : "rgba(255,77,77,.10)";
}

function setPill(el, text, good) {
  if (!el) return;
  el.textContent = text;
  el.style.borderColor = good ? "rgba(76,125,255,.55)" : "rgba(39,48,72,.9)";
  el.style.background = good ? "rgba(76,125,255,.12)" : "rgba(255,255,255,.03)";
}

/* ---------------- API Envelope ---------------- */
function unwrapApi(resp) {
  if (!resp) {
    return { ok: false, code: "no_response", msg: "", data: null };
  }
  const j = resp.json;
  if (j && typeof j === "object" && typeof j.ok === "boolean" && ("code" in j) && ("data" in j)) {
    return { ok: !!j.ok, code: String(j.code || ""), msg: String(j.msg || ""), data: j.data };
  }
  return { ok: !!resp.ok, code: resp.ok ? "ok" : `http_${resp.status}`, msg: "", data: j };
}

/* ---------------- 모드 전환 ---------------- */
function setAppMode(mode) {
  g_appMode = mode;
  updateAppModeUi();
  if (typeof _updateNetLabel === "function") _updateNetLabel();
}

function updateAppModeUi() {
  const pill = qs("pillNet");
  if (!pill) return;
  if (g_appMode === APP_MODE_OFFLINE) {
    pill.textContent = "NET: OFFLINE";
    pill.style.borderColor = "rgba(255,170,0,.6)";
    pill.style.background = "rgba(255,170,0,.12)";
    pill.style.color = "var(--warn)";
  } else {
    pill.textContent = "NET: ONLINE";
    pill.style.borderColor = "rgba(76,125,255,.5)";
    pill.style.background = "rgba(76,125,255,.12)";
    pill.style.color = "var(--primary)";
  }
}

/* ---------------- 통신 래퍼 ---------------- */
async function apiGet(url) {
  if (g_appMode === APP_MODE_OFFLINE) {
    return handleOfflineApi(url, "GET", null);
  }
  try {
    const r = await fetch(url, { cache: "no-store" });
    const t = await r.text();
    let j = null; try { j = JSON.parse(t); } catch (e) { }
    return { ok: r.ok, status: r.status, text: t, json: j };
  } catch (e) {
    console.warn(`[Network] ${url} fetch failed, falling back to OFFLINE:`, e);
    setAppMode(APP_MODE_OFFLINE);
    const failMsg = (typeof g_currLang !== "undefined" && g_currLang === "en")
      ? "Network connection failed: Switched to Offline Simulator."
      : "네트워크 연결 실패: 오프라인 시뮬레이터로 전환됨.";
    setMsg(failMsg, false);
    return handleOfflineApi(url, "GET", null);
  }
}

async function apiPostJson(url, obj) {
  if (g_appMode === APP_MODE_OFFLINE) {
    return handleOfflineApi(url, "POST", obj);
  }
  try {
    const r = await fetch(url, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(obj),
      cache: "no-store"
    });
    const t = await r.text();
    let j = null; try { j = JSON.parse(t); } catch (e) { }
    return { ok: r.ok, status: r.status, text: t, json: j };
  } catch (e) {
    console.warn(`[Network] ${url} post failed, falling back to OFFLINE:`, e);
    setAppMode(APP_MODE_OFFLINE);
    const failMsg = (typeof g_currLang !== "undefined" && g_currLang === "en")
      ? "Network connection failed: Switched to Offline Simulator."
      : "네트워크 연결 실패: 오프라인 시뮬레이터로 전환됨.";
    setMsg(failMsg, false);
    return handleOfflineApi(url, "POST", obj);
  }
}

/* =======================================================
   HID Usage 드롭다운 헬퍼
   ======================================================= */
function _kbUsageGroup(code, name) {
  const isEn = (typeof g_currLang !== "undefined" && g_currLang === "en");
  if (code === 0) return isEn ? "Unassigned" : "선택 안 함";
  if (code >= 0x04 && code <= 0x1D) return isEn ? "Letters (A~Z)" : "영문 알파벳 (A~Z)";
  if (code >= 0x1E && code <= 0x27) return isEn ? "Numbers (1~0)" : "숫자 키 (1~0)";
  if (code >= 0x28 && code <= 0x2C) return isEn ? "Basic Keys (Enter/Space)" : "기본 편집 키 (Enter/Space 등)";
  if (code >= 0x3A && code <= 0x45) return isEn ? "Function Keys (F1~F12)" : "기능 키 (F1~F12)";
  if (code === 0x4B || code === 0x4E || (code >= 0x4F && code <= 0x52)) {
    return isEn ? "Navigation & Arrow Keys" : "탐색 및 방향 키";
  }
  // [v0415] Keyboard Page Power (HID 0x66) 그룹
  if (code === 0x66) return isEn ? "Power / System" : "전원 및 시스템";
  return isEn ? "Special & Symbol Keys" : "기타 특수 및 기호 키";
}

function _kbUsageLabel(code, name) {
  const isEn = (typeof g_currLang !== "undefined" && g_currLang === "en");
  const hex = "0x" + code.toString(16).toUpperCase().padStart(2, "0");
  if (name === "None") return isEn ? "None (Unassigned)" : "선택 안 함 (None)";
  if (name === "Power") return isEn ? "Power (Keyboard 0x66)" : "전원 (Keyboard 0x66)";
  if (name && !/^0x/i.test(name)) return `${name} (${hex})`;
  return hex;
}

/* ---------------- 사용자 친화 라벨 매핑 ---------------- */
function getActionKindFriendlyName(name, val) {
  const isEn = (typeof g_currLang !== "undefined" && g_currLang === "en");
  const MAP_KO = {
    "NONE": "할당 안 함 (선택 안 함)",
    "MOUSE_CLICK": "마우스 클릭",
    "MOUSE_HOLD": "마우스 누른 상태 유지 (드래그용)",
    "MOUSE_WHEEL": "휠 스크롤",
    "KB_TAP": "키보드 단일 키",
    "KB_COMBO": "단축키 조합 (최대 3개 동시)",
    "KB_REPEAT": "키 반복 입력 (누르는 동안 연속 입력)",
    "CONSUMER_TAP": "미디어 / 리모컨 제어",
    "CONSUMER_REPEAT": "미디어 연속 제어 (볼륨 연속 조절 등)",
    "SPECIAL": "기기 특수 제어 (영점 조절 / 절전 등)",
    "MACRO": "연속 동작 매크로 실행"
  };
  const MAP_EN = {
    "NONE": "Unassigned (None)",
    "MOUSE_CLICK": "Mouse Click",
    "MOUSE_HOLD": "Mouse Hold (Drag)",
    "MOUSE_WHEEL": "Wheel Scroll",
    "KB_TAP": "Single Key Tap",
    "KB_COMBO": "Hotkey Combination (Up to 3)",
    "KB_REPEAT": "Key Repeat (Continuous)",
    "CONSUMER_TAP": "Media / Remote Control",
    "CONSUMER_REPEAT": "Media Continuous Control",
    "SPECIAL": "Special Device Action",
    "MACRO": "Execute Macro Sequence"
  };
  const desc = isEn ? MAP_EN[name] : MAP_KO[name];
  return desc ? `${name} — ${desc}` : (name || `Kind ${val}`);
}

function getMouseButtonFriendlyName(val) {
  const isEn = (typeof g_currLang !== "undefined" && g_currLang === "en");
  const MAP_KO = {
    0: "선택 안 함 (None)",
    1: "왼쪽 클릭 (기본 클릭)",
    2: "오른쪽 클릭 (컨텍스트 메뉴)",
    4: "가운데 클릭 (휠 클릭)",
    8: "뒤로 가기 (4번 버튼)",
    16: "앞으로 가기 (5번 버튼)"
  };
  const MAP_EN = {
    0: "None (Unassigned)",
    1: "Left Click (Primary)",
    2: "Right Click (Secondary)",
    4: "Middle Click (Wheel)",
    8: "Back (Button 4)",
    16: "Forward (Button 5)"
  };
  return (isEn ? MAP_EN[val] : MAP_KO[val]) || `Button ${val}`;
}

function getScrollFriendlyName(isAxis, val) {
  const isEn = (typeof g_currLang !== "undefined" && g_currLang === "en");
  if (isAxis) {
    if (val === 0) return isEn ? "Y-Axis (Vertical Scroll)" : "Y축 (세로 스크롤)";
    return isEn ? "X-Axis (Horizontal Pan)" : "X축 (가로 스크롤 / 팬)";
  } else {
    if (val === 0) return isEn ? "Up / Left" : "위로 / 왼쪽";
    return isEn ? "Down / Right" : "아래로 / 오른쪽";
  }
}

function getModifierFriendlyName(name) {
  const isEn = (typeof g_currLang !== "undefined" && g_currLang === "en");
  const MAP_KO = {
    "None": "조합 키 없음 (None)",
    "L_CTRL": "Ctrl (왼쪽)",
    "L_SHIFT": "Shift (왼쪽)",
    "L_ALT": "Alt (왼쪽)",
    "L_GUI": "시작 / Win (왼쪽)",
    "R_CTRL": "Ctrl (오른쪽)",
    "R_SHIFT": "Shift (오른쪽)",
    "R_ALT": "Alt (오른쪽)",
    "R_GUI": "시작 / Win (오른쪽)"
  };
  const MAP_EN = {
    "None": "No Modifier (None)",
    "L_CTRL": "Left Ctrl",
    "L_SHIFT": "Left Shift",
    "L_ALT": "Left Alt",
    "L_GUI": "Left Win / Start",
    "R_CTRL": "Right Ctrl",
    "R_SHIFT": "Right Shift",
    "R_ALT": "Right Alt",
    "R_GUI": "Right Win / Start"
  };
  return (isEn ? MAP_EN[name] : MAP_KO[name]) || name;
}

/* -------------------------------------------------------
   [v0415] Consumer 친화 라벨 매핑 (Descriptor SSOT 정합)
   - AC_BACK/HOME/SEARCH → WWW_BACK/HOME/SEARCH (라벨 유지)
   - POWER/TV_INPUT/CH_UP/CH_DOWN 삭제 (Descriptor 미지원)
   - 신규: PLAY/PAUSE/RECORD/EJECT/RANDOM/REPEAT/MY_COMP/CALC/FAV/STOP/MEDIA_SEL/MAIL
   ------------------------------------------------------- */
function getConsumerFriendlyName(name) {
  const isEn = (typeof g_currLang !== "undefined" && g_currLang === "en");
  const MAP_KO = {
    "PLAY": "재생",
    "PAUSE": "일시 중지",
    "RECORD": "녹음",
    "FF": "빨리 감기",
    "REWIND": "되감기",
    "NEXT_TRACK": "다음 트랙 (다음 곡)",
    "PREV_TRACK": "이전 트랙 (이전 곡)",
    "STOP": "정지",
    "EJECT": "꺼내기",
    "RANDOM": "무작위 재생",
    "REPEAT": "반복 재생",
    "PLAY_PAUSE": "재생 / 일시 중지",
    "MUTE": "음소거",
    "VOL_UP": "볼륨 올리기",
    "VOL_DOWN": "볼륨 내리기",
    "WWW_HOME": "홈 화면",
    "MY_COMP": "내 컴퓨터",
    "CALC": "계산기",
    "WWW_FAV": "즐겨찾기",
    "WWW_SEARCH": "검색 창 열기",
    "WWW_STOP": "페이지 로딩 중지",
    "WWW_BACK": "뒤로 가기",
    "MEDIA_SEL": "미디어 선택",
    "MAIL": "메일"
  };
  const MAP_EN = {
    "PLAY": "Play",
    "PAUSE": "Pause",
    "RECORD": "Record",
    "FF": "Fast Forward",
    "REWIND": "Rewind",
    "NEXT_TRACK": "Next Track",
    "PREV_TRACK": "Previous Track",
    "STOP": "Stop",
    "EJECT": "Eject",
    "RANDOM": "Random Play",
    "REPEAT": "Repeat",
    "PLAY_PAUSE": "Play / Pause",
    "MUTE": "Mute",
    "VOL_UP": "Volume Up",
    "VOL_DOWN": "Volume Down",
    "WWW_HOME": "Home Screen",
    "MY_COMP": "My Computer",
    "CALC": "Calculator",
    "WWW_FAV": "WWW Favorites",
    "WWW_SEARCH": "Open Search",
    "WWW_STOP": "WWW Stop",
    "WWW_BACK": "Back",
    "MEDIA_SEL": "Media Select",
    "MAIL": "Mail"
  };
  const desc = isEn ? MAP_EN[name] : MAP_KO[name];
  return desc ? `${name} — ${desc}` : name;
}

function getSpecialFriendlyName(name) {
  const isEn = (typeof g_currLang !== "undefined" && g_currLang === "en");
  const MAP_KO = {
    "NONE": "동작 없음",
    "GYRO_RECALIB": "자이로 영점 조절 (수평·오프셋 보정)",
    "SLEEP_NOW": "지금 절전 모드 시작",
    "MODE_CYCLE": "작동 모드 순환 전환 (PC → 프레젠테이션 → 스마트 TV)",
    "PAIRING": "블루투스 페어링(연결 대기) 시작",
    "HOST_CYCLE": "호스트 기기 전환 (블루투스 호스트 순환)"
  };
  const MAP_EN = {
    "NONE": "None",
    "GYRO_RECALIB": "Gyro Zero Calibration (Level & Offset)",
    "SLEEP_NOW": "Enter Sleep Mode Now",
    "MODE_CYCLE": "Cycle Operating Mode (PC → Presentation → Smart TV)",
    "PAIRING": "Start Bluetooth Pairing",
    "HOST_CYCLE": "Host Device Cycle (Bluetooth Easy-Switch)"
  };
  const desc = isEn ? MAP_EN[name] : MAP_KO[name];
  return desc ? `${name} — ${desc}` : name;
}

function populateKbUsageSelect(p_selectEl, p_currentCode) {
  if (!p_selectEl) return;
  p_selectEl.innerHTML = "";

  let v_kb = null;
  if (g_keycodes && Array.isArray(g_keycodes.kb) && g_keycodes.kb.length) {
    v_kb = g_keycodes.kb;
  } else if (typeof G_OFFLINE_KEYCODES !== "undefined" &&
    G_OFFLINE_KEYCODES &&
    Array.isArray(G_OFFLINE_KEYCODES.kb) &&
    G_OFFLINE_KEYCODES.kb.length) {
    v_kb = G_OFFLINE_KEYCODES.kb;
  }

  if (!v_kb || !v_kb.length) {
    const o = document.createElement("option");
    o.value = String(p_currentCode);
    o.textContent = _kbUsageLabel(p_currentCode, null);
    p_selectEl.appendChild(o);
    console.warn("[populateKbUsageSelect] no kb data");
    return;
  }

  const groups = new Map();
  for (const item of v_kb) {
    const grp = _kbUsageGroup(item.code, item.name);
    if (!groups.has(grp)) groups.set(grp, []);
    groups.get(grp).push(item);
  }

  const isEn = (typeof g_currLang !== "undefined" && g_currLang === "en");
  const ORDER_KO = [
    "선택 안 함",
    "영문 알파벳 (A~Z)",
    "숫자 키 (1~0)",
    "기본 편집 키 (Enter/Space 등)",
    "기능 키 (F1~F12)",
    "탐색 및 방향 키",
    "전원 및 시스템",
    "기타 특수 및 기호 키"
  ];
  const ORDER_EN = [
    "Unassigned",
    "Letters (A~Z)",
    "Numbers (1~0)",
    "Basic Keys (Enter/Space)",
    "Function Keys (F1~F12)",
    "Navigation & Arrow Keys",
    "Power / System",
    "Special & Symbol Keys"
  ];
  const ORDER = isEn ? ORDER_EN : ORDER_KO;

  for (const grpName of ORDER) {
    const items = groups.get(grpName);
    if (!items || !items.length) continue;

    const og = document.createElement("optgroup");
    og.label = grpName;
    for (const item of items) {
      const o = document.createElement("option");
      o.value = String(item.code);
      o.textContent = _kbUsageLabel(item.code, item.name);
      og.appendChild(o);
    }
    p_selectEl.appendChild(og);
  }

  const cur = String(p_currentCode);
  let found = false;
  for (const opt of p_selectEl.options) {
    if (opt.value === cur) { found = true; break; }
  }
  if (!found) {
    const o = document.createElement("option");
    o.value = cur;
    o.textContent = "(custom) " + _kbUsageLabel(p_currentCode, null);
    p_selectEl.appendChild(o);
  }

  p_selectEl.value = cur;
}

/* =======================================================
   전역 로딩 오버레이 제어
   ======================================================= */
function showLoading(text) {
  const ov = qs("loadingOverlay");
  const tx = qs("loadingText");
  if (tx && text) tx.textContent = text;
  if (ov) {
    ov.classList.add("on");
    ov.style.display = "flex";
  }
}

function hideLoading() {
  const ov = qs("loadingOverlay");
  if (ov) {
    ov.classList.remove("on");
    ov.style.display = "none";
  }
}
