/* =======================================================
   File: /www/lib/am_base_0412.js
   Elite AirMouse WebConfig v0412 — Base Utilities
   - 모드 상수 / DOM·파싱 유틸 / API 래퍼 / 전역 상태
   - 로드 순서: 1 (최우선)
   - [v0412] C-06 (_kbUsageLabel "None"), N-13 (로딩 헬퍼)
   ======================================================= */

/* ---------------- 모드 상수 ---------------- */
const APP_MODE_ONLINE  = "ONLINE";
const APP_MODE_OFFLINE = "OFFLINE";

/* ---------------- DOM 유틸 ---------------- */
function qs(id){ return document.getElementById(id); }
function qsa(sel){ return document.querySelectorAll(sel); }

/* ---------------- 전역 상태 ---------------- */
let g_appMode = (window.location.protocol === "file:") ? APP_MODE_OFFLINE : APP_MODE_ONLINE;
let g_keycodes = null;
let g_triggers = [];
let g_profile  = null;
let g_view     = "global";
let g_macroSel = -1;
let g_lastStatus = null;
let g_diagTypingUntilMs = 0;

function nowMs(){ return Date.now(); }

/* ---------------- 공통 유틸 ---------------- */
function pretty(o){
  try { return JSON.stringify(o, null, 2); } catch(e){ return String(o); }
}

function parseIntFlex(v, def = 0){
  if (v === null || v === undefined) return def;
  const s = String(v).trim();
  if (!s) return def;
  if (/^0x[0-9a-f]+$/i.test(s)) return parseInt(s, 16);
  const n = Number(s);
  return Number.isFinite(n) ? Math.trunc(n) : def;
}

function parseNum(v, def = 0){
  const n = Number(v);
  return Number.isFinite(n) ? n : def;
}

function parseBool(v){
  if (v === true || v === false) return v;
  const s = String(v).toLowerCase();
  return s === "true" || s === "1" || s === "on";
}

function setMsg(text, good){
  const el = qs("cfgMsg"); if (!el) return;
  el.textContent = text;
  el.style.borderColor = good ? "rgba(76,125,255,.55)" : "rgba(255,77,77,.55)";
  el.style.background  = good ? "rgba(76,125,255,.10)" : "rgba(255,77,77,.10)";
}

function setPill(el, text, good){
  if (!el) return;
  el.textContent = text;
  el.style.borderColor = good ? "rgba(76,125,255,.55)" : "rgba(39,48,72,.9)";
  el.style.background  = good ? "rgba(76,125,255,.12)" : "rgba(255,255,255,.03)";
}

/* ---------------- API Envelope ---------------- */
function unwrapApi(resp){
  const j = resp.json;
  if (j && typeof j === "object" && typeof j.ok === "boolean" && ("code" in j) && ("data" in j)) {
    return { ok: !!j.ok, code: String(j.code || ""), msg: String(j.msg || ""), data: j.data };
  }
  return { ok: resp.ok, code: resp.ok ? "ok" : `http_${resp.status}`, msg: "", data: j };
}

/* ---------------- 모드 전환 ---------------- */
function setAppMode(mode){
  g_appMode = mode;
  updateAppModeUi();
}

function updateAppModeUi(){
  const pill = qs("pillNet");
  if (!pill) return;
  if (g_appMode === APP_MODE_OFFLINE) {
    setPill(pill, "NET: OFFLINE", false);
    pill.style.borderColor = "rgba(255, 170, 0, 0.6)";
    pill.style.background  = "rgba(255, 170, 0, 0.12)";
  } else {
    setPill(pill, "NET: ONLINE", true);
  }
}

/* ---------------- 통신 래퍼 ---------------- */
async function apiGet(url){
  if (g_appMode === APP_MODE_OFFLINE) {
    return handleOfflineApi(url, "GET", null);
  }
  try {
    const r = await fetch(url, { cache: "no-store" });
    const t = await r.text();
    let j = null; try { j = JSON.parse(t); } catch(e){}
    return { ok: r.ok, status: r.status, text: t, json: j };
  } catch(e) {
    console.warn(`[Network] ${url} fetch failed, falling back to OFFLINE mode:`, e);
    setAppMode(APP_MODE_OFFLINE);
    setMsg("네트워크 연결 실패: Offline 모드로 전환되었습니다.", false);
    return handleOfflineApi(url, "GET", null);
  }
}

async function apiPostJson(url, obj){
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
    let j = null; try { j = JSON.parse(t); } catch(e){}
    return { ok: r.ok, status: r.status, text: t, json: j };
  } catch(e) {
    console.warn(`[Network] ${url} post failed, falling back to OFFLINE mode:`, e);
    setAppMode(APP_MODE_OFFLINE);
    setMsg("네트워크 연결 실패: Offline 모드로 전환되었습니다.", false);
    return handleOfflineApi(url, "POST", obj);
  }
}

/* =======================================================
   HID Usage 드롭다운 헬퍼
   - g_keycodes 없으면 G_OFFLINE_KEYCODES 폴백
   ======================================================= */
function _kbUsageGroup(code, name) {
  if (code === 0) return "없음";
  if (code >= 0x04 && code <= 0x1D) return "알파벳 (A~Z)";
  if (code >= 0x1E && code <= 0x27) return "숫자 (1~0)";
  if (code >= 0x28 && code <= 0x2C) return "기본 키";
  if (code >= 0x3A && code <= 0x45) return "펑션 키 (F1~F12)";
  if (code === 0x4B) return "네비게이션";
  if (code === 0x4E) return "네비게이션";
  if (code >= 0x4F && code <= 0x52) return "네비게이션";
  return "기타 (심볼/특수)";
}

// [C-06] "None" 특수 처리
function _kbUsageLabel(code, name) {
  const hex = "0x" + code.toString(16).toUpperCase().padStart(2, "0");
  if (name === "None") return "None (없음)";
  if (name && !/^0x/i.test(name)) return name + " (" + hex + ")";
  return hex;
}

function _kbUsageName(code) {
  const kb = (g_keycodes && g_keycodes.kb) || [];
  for (const it of kb) {
    if (it.code === code) {
      if (it.name && !/^0x/i.test(it.name)) return it.name;
      return "0x" + code.toString(16).toUpperCase().padStart(2, "0");
    }
  }
  return "0x" + code.toString(16).toUpperCase().padStart(2, "0");
}

// usage dropdown 생성 + 값 세팅 (오프라인 폴백 포함)
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
    console.info("[populateKbUsageSelect] fallback to G_OFFLINE_KEYCODES.kb",
                 v_kb.length, "items");
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

  const ORDER = [
    "없음",
    "알파벳 (A~Z)",
    "숫자 (1~0)",
    "기본 키",
    "펑션 키 (F1~F12)",
    "네비게이션",
    "기타 (심볼/특수)"
  ];

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
   [N-13] 전역 로딩 오버레이 제어
   ======================================================= */
function showLoading(text){
  const ov = qs("loadingOverlay");
  const tx = qs("loadingText");
  if (tx && text) tx.textContent = text;
  if (ov) ov.style.display = "flex";
}
function hideLoading(){
  const ov = qs("loadingOverlay");
  if (ov) ov.style.display = "none";
}
