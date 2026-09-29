/* =======================================================
   File: /www/lib/am_base_0410.js
   Elite AirMouse WebConfig v0410 — Base Utilities
   - 모드 상수 / DOM·파싱 유틸 / API 래퍼 / 전역 상태
   - 로드 순서: 1 (최우선)
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
let g_profile  = null;      // { idx, count, config:{...} }
let g_view     = "global";  // "global" | "mode1" | "mode2" | "mode3"
let g_macroSel = -1;        // 현재 편집 중인 매크로 인덱스
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
  return {
    ok: resp.ok,
    code: resp.ok ? "ok" : `http_${resp.status}`,
    msg: "",
    data: j
  };
}

/* ---------------- 모드 전환 UI ---------------- */
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
