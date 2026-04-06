/* =======================================================
   File: /www/app_0310.js
   Backend-aligned full (W10_WebConfig_0303.h)
   - /api/status (no-store)
   - /api/keycodes (mods/kb/consumer + precision_modes)
   - /api/config /save /apply /export /import /rollback
   - /api/control (cmd set_ppt / set_dpi / set_precision / set_safe_mode / set_ota_guard ...)
   - /api/ppt (v2) + /api/ppt/test
   - /api/ota, /api/ota/status
======================================================= */


function qs(id){ return document.getElementById(id); }
function qsa(sel){ return document.querySelectorAll(sel); }

async function apiGet(url){
  const r = await fetch(url, {cache:"no-store"});
  const t = await r.text();
  let j = null;
  try{ j = JSON.parse(t); }catch(e){}
  return {ok:r.ok, status:r.status, text:t, json:j};
}

async function apiPostJson(url, obj){
  const r = await fetch(url, {
    method:"POST",
    headers: {"Content-Type":"application/json"},
    body: JSON.stringify(obj),
    cache:"no-store"
  });
  const t = await r.text();
  let j = null;
  try{ j = JSON.parse(t); }catch(e){}
  return {ok:r.ok, status:r.status, text:t, json:j};
}

async function apiPostText(url, text){
  const r = await fetch(url, {
    method:"POST",
    headers: {"Content-Type":"application/json"},
    body: text,
    cache:"no-store"
  });
  const t = await r.text();
  let j = null;
  try{ j = JSON.parse(t); }catch(e){}
  return {ok:r.ok, status:r.status, text:t, json:j};
}

function pretty(o){
  try{ return JSON.stringify(o, null, 2); }catch(e){ return String(o); }
}

function setPill(el, text, good){
  el.textContent = text;
  el.style.borderColor = good ? "rgba(76,125,255,.55)" : "rgba(39,48,72,.9)";
  el.style.background = good ? "rgba(76,125,255,.12)" : "rgba(255,255,255,.03)";
}

function setMsg(text, good){
  const el = qs("cfgMsg");
  el.textContent = text;
  el.style.borderColor = good ? "rgba(76,125,255,.55)" : "rgba(255,77,77,.55)";
  el.style.background = good ? "rgba(76,125,255,.10)" : "rgba(255,77,77,.10)";
}

function fillSelect(el, items, valueKey, labelKey){
  el.innerHTML = "";
  for(const it of items){
    const opt = document.createElement("option");
    opt.value = String(it[valueKey]);
    opt.textContent = it[labelKey];
    el.appendChild(opt);
  }
}

function fillPageSelect(el){
  el.innerHTML = "";
  for(const it of [{v:"kb",t:"kb"},{v:"consumer",t:"consumer"}]){
    const opt = document.createElement("option");
    opt.value = it.v;
    opt.textContent = it.t;
    el.appendChild(opt);
  }
}

function bindPptRow(prefix){
  return { page: qs(prefix+"Page"), mod: qs(prefix+"Mod"), code: qs(prefix+"Code") };
}

let g_keycodes = null;
let g_config = null;
let g_lastWifiFingerprint = "";
let g_lastStatus = null;
let g_schema = null;

function unwrapApi(resp){
  const j = resp.json;
  if(j && typeof j === 'object' && typeof j.ok === 'boolean' && ('code' in j) && ('data' in j)){
    return { ok: !!j.ok, code: String(j.code||""), msg: String(j.msg||""), data: j.data };
  }
  return { ok: resp.ok, code: resp.ok ? "ok" : `http_${resp.status}`, msg: "", data: j };
}

async function loadSchema(){
  const r = await apiGet("/json/public/schema_0310.json");
  if(r.ok && r.json){
    g_schema = r.json;
    applySchemaToUi();
  }
}

function _getSchemaNode(path){
  let n = g_schema;
  for(const k of path){
    if(!n || typeof n !== 'object') return null;
    n = n[k];
  }
  return (n && typeof n === 'object') ? n : null;
}

function _applySchemaToInput(id, path){
  const el = qs(id);
  if(!el) return;
  const s = _getSchemaNode(path);
  if(!s) return;
  if(s.min !== undefined) el.min = String(s.min);
  if(s.max !== undefined) el.max = String(s.max);
  if(s.step !== undefined) el.step = String(s.step);
  if(s.max_len !== undefined) el.maxLength = Number(s.max_len);
  if(s.default !== undefined && (el.placeholder === "" || el.placeholder === undefined)) el.placeholder = String(s.default);
}

function applySchemaToUi(){
  if(!g_schema) return;
  _applySchemaToInput("staSsid", ["wifi","sta_ssid"]);
  _applySchemaToInput("staPass", ["wifi","sta_pass"]);
  _applySchemaToInput("apSsid",  ["wifi","ap_ssid"]);
  _applySchemaToInput("apPass",  ["wifi","ap_pass"]);
  _applySchemaToInput("mdnsHost",["wifi","mdns_host"]);

  _applySchemaToInput("accelThreshold", ["e10","accel_threshold"]);
  _applySchemaToInput("scrollDamp",     ["e10","scroll_cursor_damp"]);

  _applySchemaToInput("sb0", ["e10","scale_base_0"]);
  _applySchemaToInput("sb1", ["e10","scale_base_1"]);
  _applySchemaToInput("sb2", ["e10","scale_base_2"]);
  _applySchemaToInput("ag0", ["e10","accel_gain_0"]);
  _applySchemaToInput("ag1", ["e10","accel_gain_1"]);
  _applySchemaToInput("ag2", ["e10","accel_gain_2"]);
  _applySchemaToInput("wheelTh",      ["e10","wheel_threshold_deg"]);
  _applySchemaToInput("wheelStepMax", ["e10","wheel_step_max"]);
  _applySchemaToInput("flickDeg",     ["e10","gesture_flick_deg"]);
  _applySchemaToInput("cooldownMs",   ["e10","gesture_cooldown_ms"]);
}

/** 10진/16진(0x..) 파싱 */
function parseIntFlex(v, def=0){
  if(v === null || v === undefined) return def;
  const s = String(v).trim();
  if(!s) return def;
  // 0x.. 허용
  if(/^0x[0-9a-f]+$/i.test(s)){
    const n = parseInt(s, 16);
    return Number.isFinite(n) ? n : def;
  }
  // 그냥 10진
  const n = Number(s);
  return Number.isFinite(n) ? Math.trunc(n) : def;
}

function parseNum(v, def=0){
  const n = Number(v);
  return Number.isFinite(n) ? n : def;
}
function parseBool(v){
  if(v === true || v === false) return v;
  const s = String(v).toLowerCase();
  return (s === "true" || s === "1" || s === "on");
}

function wifiFingerprint(cfg){
  const w = cfg?.wifi || {};
  const s = w?.sta || {};
  const a = w?.ap || {};
  const m = w?.mdns || {};
  return [ w.mode, s.ssid, s.pass, a.ssid, a.pass, m.host ].map(v=>String(v ?? "")).join("|");
}

function ensureDefaults(cfg){
  cfg = cfg || {};
  cfg.wifi = cfg.wifi || {};
  cfg.wifi.sta = cfg.wifi.sta || {};
  cfg.wifi.ap = cfg.wifi.ap || {};
  cfg.wifi.mdns = cfg.wifi.mdns || {};

  cfg.e10 = cfg.e10 || {};
  cfg.e10.wheel = cfg.e10.wheel || {};
  cfg.e10.gesture = cfg.e10.gesture || {};
  cfg.e10.precision = cfg.e10.precision || {};
  if(cfg.e10.precision_mode === undefined) cfg.e10.precision_mode = 0;
  return cfg;
}

/* ---------------- Tabs ---------------- */
function bindTabs(){
  qsa(".tab").forEach(b=>{
    b.addEventListener("click", ()=>{
      qsa(".tab").forEach(x=>x.classList.remove("on"));
      qsa(".tabpane").forEach(x=>x.classList.remove("on"));
      b.classList.add("on");
      const id = "tab-" + b.getAttribute("data-tab");
      qs(id).classList.add("on");
    });
  });
}

/* ---------------- Status ---------------- */
function formatPrecSummary(e10){
  if(!e10) return "-";
  const mode = (e10.precision_mode ?? "-");
  return `mode:${mode}`;
}

async function refreshStatus(){
  const r = await apiGet("/api/status?compact=1");
  const u = unwrapApi(r);
  if(!u.ok || !u.data){
    qs("statusJson").textContent = u.msg || r.text || "status failed";
    return;
  }

  const j = u.data;
  g_lastStatus = j;

  const g = j.groups || {};
  const sys  = g.sys  || j.sys  || {};
  const mem  = g.mem  || j.mem  || {};
  const net  = g.net  || j.net  || {};
  const e10  = g.e10  || j.e10  || {};
  const boot = g.boot || j.boot || {};
  const pol  = j.policy || g.policy || {};

  qs("stUptime").textContent = `${sys.uptime_ms ?? j.uptime_ms ?? "-"} ms`;
  qs("stHeap").textContent   = `${mem.heap_free ?? j.heap_free ?? "-"} bytes`;

  qs("stWiFi").textContent = `${net.mode ?? "-"} / ${net.ssid ?? "-"}`;
  qs("stMdns").textContent = `${net.mdns ?? "-"}`;

  qs("stPptMode").textContent = String(e10.ppt_mode ?? "-");
  qs("stPrec").textContent    = formatPrecSummary(e10);

  setPill(qs("pillNet"),  `NET: ${net.mode ?? "-"}`, true);
  setPill(qs("pillBle"),  `BLE: ${e10.ble_connected ? "ON" : "OFF"}`, !!e10.ble_connected);
  setPill(qs("pillSafe"), `SAFE: ${boot.safe_mode ? "ON" : "OFF"}`, !!boot.safe_mode);

  const needReboot = !!pol.reboot_required;
  const btnRb = qs("btnReboot");
  if(btnRb){
    btnRb.disabled = !needReboot;
    btnRb.title = needReboot ? `재부팅 필요: ${pol.reboot_reasons || ""}` : "재부팅 필요 없음";
  }

  qs("statusJson").textContent = pretty(u);
}

/* ---------------- Diagnostics ---------------- */
async function refreshDiag(){
  const r = await apiGet("/api/diag");
  const u = unwrapApi(r);
  const el = qs("diagJson");
  if(el) el.textContent = pretty(u);
}

function bindDiag(){
  const b1 = qs("btnDiagRefresh");
  const b2 = qs("btnDiagClear");
  if(b1) b1.addEventListener("click", refreshDiag);
  if(b2) b2.addEventListener("click", async ()=>{
    const r = await apiPostJson("/api/diag/clear", {});
    const u = unwrapApi(r);
    const el = qs("diagJson");
    if(el) el.textContent = pretty(u);
    await refreshStatus();
  });
}

/* ---------------- keycodes + datalist ---------------- */
function buildDatalist(dlEl, items, kind){
  // kind: "kb"(usage-id) or "consumer"(mask)
  dlEl.innerHTML = "";
  const max = 220; // 너무 많으면 브라우저가 버벅여서 상한
  for(let i=0;i<items.length && i<max;i++){
    const it = items[i];
    const opt = document.createElement("option");
    if(kind === "kb"){
      const code = Number(it.code) || 0;
      opt.value = `0x${code.toString(16).toUpperCase().padStart(2,"0")}`;
      opt.label = `${it.name} (${opt.value})`;
    }else{
      const mask = Number(it.mask) >>> 0;
      opt.value = `0x${mask.toString(16).toUpperCase()}`;
      opt.label = `${it.name} (${opt.value})`;
    }
    dlEl.appendChild(opt);
  }
}

function syncPptRowHints(row){
  // page가 consumer면 mod는 의미 없음 -> 0 + disable
  const isConsumer = (row.page.value === "consumer");
  if(isConsumer){
    row.mod.value = "0";
    row.mod.disabled = true;
    row.code.setAttribute("list", "dlConsumer");
  }else{
    row.mod.disabled = false;
    row.code.setAttribute("list", "dlKb");
  }
}

async function loadKeycodes(){
  const r = await apiGet("/api/keycodes");
  if(!r.ok || !r.json) throw new Error("keycodes load failed");
  g_keycodes = r.json;

  const mods = r.json.mods || [];
  const modItems = mods.map(m => ({mask:m.mask, name:`${m.name} (0x${Number(m.mask).toString(16)})`}));

  const modSelects = ["pptStartMod","pptExitMod","pptNextMod","pptPrevMod","pptBlackMod","pptLaserMod"];
  for(const id of modSelects) fillSelect(qs(id), modItems, "mask", "name");

  const pageSelects = ["pptStartPage","pptExitPage","pptNextPage","pptPrevPage","pptBlackPage","pptLaserPage"];
  for(const id of pageSelects) fillPageSelect(qs(id));

  // precision modes
  const pmRaw = r.json.precision_modes || [];
  const pmItems = pmRaw.map(x => ({
    value: (x.value ?? x.mode ?? 0),
    name:  String(x.name ?? `mode${x.value ?? x.mode ?? 0}`)
  }));
  if(pmItems.length === 0) pmItems.push({value:0, name:"0"});
  fillSelect(qs("precMode"), pmItems, "value", "name");
  fillSelect(qs("ctlPrecMode"), pmItems, "value", "name");

  // datalist (추천)
  buildDatalist(qs("dlKb"), r.json.kb || [], "kb");
  buildDatalist(qs("dlConsumer"), r.json.consumer || [], "consumer");

  // page change에 따른 mod/list 동기화
  const rows = [
    bindPptRow("pptStart"),
    bindPptRow("pptExit"),
    bindPptRow("pptNext"),
    bindPptRow("pptPrev"),
    bindPptRow("pptBlack"),
    bindPptRow("pptLaser"),
  ];
  for(const row of rows){
    syncPptRowHints(row);
    row.page.addEventListener("change", ()=> syncPptRowHints(row));
  }

  return r.json;
}

/* ---------------- PPT ---------------- */
function getPptPayload(){
  const rows = {
    start: bindPptRow("pptStart"),
    exit:  bindPptRow("pptExit"),
    next:  bindPptRow("pptNext"),
    prev:  bindPptRow("pptPrev"),
    black: bindPptRow("pptBlack"),
    laser: bindPptRow("pptLaser"),
  };

  const map = {};
  for(const k of Object.keys(rows)){
    const r = rows[k];
    const page = (r.page.value === "consumer") ? "consumer" : "kb";
    const mod  = (page === "consumer") ? 0 : (parseIntFlex(r.mod.value, 0) & 0xFF);
    const code = parseIntFlex(r.code.value, 0);
    map[k] = { page, mod, code };
  }
  return { save: qs("swPptSave").checked, map };
}

function setPptForm(map){
  const rows = {
    start: bindPptRow("pptStart"),
    exit:  bindPptRow("pptExit"),
    next:  bindPptRow("pptNext"),
    prev:  bindPptRow("pptPrev"),
    black: bindPptRow("pptBlack"),
    laser: bindPptRow("pptLaser"),
  };

  for(const k of Object.keys(rows)){
    const r = rows[k];
    const m = map[k] || {};
    r.page.value = (m.page === "consumer") ? "consumer" : "kb";
    r.mod.value  = String(m.mod ?? 0);
    r.code.value = (m.code !== undefined && m.code !== null) ? String(m.code) : "0";
    syncPptRowHints(r);
  }
  qs("pptJson").textContent = pretty({map});
}

async function pptReload(){
  const r = await apiGet("/api/ppt");
  if(!r.ok || !r.json){ alert("ppt load failed"); return; }
  setPptForm(r.json.map || {});
}

async function pptSave(){
  const payload = getPptPayload();
  const r = await apiPostJson("/api/ppt", payload);
  if(!r.ok){ alert("ppt save failed: " + (r.json?.err || r.text)); return; }
  await pptReload();
}

async function pptTest(action){
  const rows = {
    start: bindPptRow("pptStart"),
    exit:  bindPptRow("pptExit"),
    next:  bindPptRow("pptNext"),
    prev:  bindPptRow("pptPrev"),
    black: bindPptRow("pptBlack"),
    laser: bindPptRow("pptLaser"),
  };
  const r = rows[action];
  const page = (r.page.value === "consumer") ? "consumer" : "kb";
  const payload = {
    page,
    mod: (page === "consumer") ? 0 : (parseIntFlex(r.mod.value, 0) & 0xFF),
    code: parseIntFlex(r.code.value, 0),
  };
  const res = await apiPostJson("/api/ppt/test", payload);
  if(!res.ok) alert("test failed: " + (res.json?.err || res.text));
}

/* ---------------- Control ---------------- */
async function ctlSetPpt(enable){
  const payload = { cmd:"set_ppt", enable: !!enable, snapshot: qs("ctlSnapshot").checked };
  const r = await apiPostJson("/api/control", payload);
  if(!r.ok) alert("set_ppt failed: " + (r.json?.err || r.text));
  await refreshStatus();
}

async function ctlSetPrecisionMode(mode){
  const v = parseIntFlex(mode, 0);
  const payload = { cmd:"set_precision", mode: v, snapshot: qs("ctlSnapshot").checked };
  const r = await apiPostJson("/api/control", payload);
  if(!r.ok) alert("set_precision failed: " + (r.json?.err || r.text));
  await refreshStatus();
}

async function ctlPrecOff(){
  await ctlSetPrecisionMode(0);
}

/* ---------------- SafeBoot/Reset/Reboot ---------------- */
async function safeInfo(){
  const r = await apiGet("/api/safeboot");
  alert(pretty(r.json || r.text));
}
async function safeExit(){
  const r = await apiPostJson("/api/safeboot", {exit:true});
  alert(pretty(r.json || r.text));
}
async function factoryReset(){
  if(!confirm("Factory Reset 진행? (재부팅됨)")) return;
  const r = await fetch("/api/factory_reset", {method:"POST"});
  const t = await r.text();
  alert(t);
}
async function reboot(){
  if(!confirm("재부팅 할까요?")) return;
  // Prefer policy snapshot from status; fallback to reboot/check
  let pol = (g_lastStatus?.policy) || (g_lastStatus?.groups?.policy) || {};
  if(pol.reboot_required === undefined){
    const chk = unwrapApi(await apiGet("/api/reboot/check"));
    if(chk.ok) pol = { reboot_required: chk.data?.required, reboot_reason_mask: chk.data?.mask, reboot_reasons: chk.data?.reasons };
  }

  const mask = Number(pol.reboot_reason_mask || 0) >>> 0;
  const body = {};
  if(mask) body.reason_mask = mask;

  const r = unwrapApi(await apiPostJson("/api/reboot", body));
  alert(pretty(r));
}

/* ---------------- Config Editor (0301 유지) ---------------- */
/* NOTE: 여기 부분은 0301과 동일 로직 유지(검증/Apply/Save/Import/Export/Rollback) */

function validateClient(cfg){
  const errs = [];

  const w = cfg.wifi || {};
  const mode = parseNum(w.mode, 0);
  if(!(mode === 0 || mode === 1 || mode === 2)) errs.push("wifi.mode must be 0|1|2");

  const apPass = String(w?.ap?.pass ?? "");
  if(apPass.length > 0 && apPass.length < 8) errs.push("wifi.ap.pass must be empty or >=8 chars");

  const mdns = String(w?.mdns?.host ?? "");
  if(mdns.length > 32) errs.push("wifi.mdns.host len <= 32");

  const e = cfg.e10 || {};
  const dpi = parseNum(e.dpi_level, 0);
  if(dpi < 0 || dpi > 2) errs.push("e10.dpi_level 0..2");

  const at = parseNum(e.accel_threshold, 0);
  if(at < 0 || at > 50) errs.push("e10.accel_threshold 0..50");

  const damp = parseNum(e.scroll_cursor_damp, 0);
  if(damp < 0 || damp > 1) errs.push("e10.scroll_cursor_damp 0..1");

  const wh = e.wheel || {};
  const wth = parseNum(wh.threshold_deg, 0);
  if(wth < 1 || wth > 360) errs.push("e10.wheel.threshold_deg 1..360");
  const wsm = parseNum(wh.step_max, 0);
  if(wsm < 1 || wsm > 50) errs.push("e10.wheel.step_max 1..50");

  const ge = e.gesture || {};
  const fd = parseNum(ge.flick_deg, 0);
  if(fd < 10 || fd > 2000) errs.push("e10.gesture.flick_deg 10..2000");
  const cd = parseNum(ge.cooldown_ms, 0);
  if(cd < 0 || cd > 20000) errs.push("e10.gesture.cooldown_ms 0..20000");

  const p = e.precision || {};
  const dz = parseNum(p.deadzone, 0);
  if(dz < 0 || dz > 50) errs.push("e10.precision.deadzone 0..50");
  const gain = parseNum(p.gain, 0);
  if(gain < 0 || gain > 5) errs.push("e10.precision.gain 0..5");
  const acc = parseNum(p.accel, 0);
  if(acc < 0 || acc > 5) errs.push("e10.precision.accel 0..5");
  const ms = parseNum(p.max_step, 0);
  if(ms < 1 || ms > 200) errs.push("e10.precision.max_step 1..200");
  const sm = parseNum(p.smooth, 0);
  if(sm < 0 || sm > 1) errs.push("e10.precision.smooth 0..1");

  const em = parseNum(p.entry_ms, 0);
  if(em < 50 || em > 5000) errs.push("e10.precision.entry_ms 50..5000");
  const xm = parseNum(p.exit_ms, 0);
  if(xm < 50 || xm > 5000) errs.push("e10.precision.exit_ms 50..5000");
  const es = parseNum(p.entry_still_deg, 0);
  if(es < 0.1 || es > 20) errs.push("e10.precision.entry_still_deg 0.1..20");
  const xm2 = parseNum(p.exit_move_deg, 0);
  if(xm2 < 0.1 || xm2 > 50) errs.push("e10.precision.exit_move_deg 0.1..50");

  const pr = parseNum(p.profile, 0);
  if(pr < 0 || pr > 5) errs.push("e10.precision.profile 0..5");

  return errs;
}

function uiToConfig(){
  const cfg = ensureDefaults(JSON.parse(qs("cfgJsonArea").value || "{}"));

  cfg.wifi.mode = parseNum(qs("wifiMode").value, 0);
  cfg.wifi.sta.ssid = String(qs("staSsid").value || "");
  cfg.wifi.sta.pass = String(qs("staPass").value || "");
  cfg.wifi.ap.ssid = String(qs("apSsid").value || "");
  cfg.wifi.ap.pass = String(qs("apPass").value || "");
  cfg.wifi.mdns.host = String(qs("mdnsHost").value || "");

  cfg.e10.dpi_level = parseNum(qs("e10Dpi").value, 2);
  cfg.e10.hard_click_lock = parseBool(qs("e10HardClick").value);

  cfg.e10.scale_base = [
    parseNum(qs("sb0").value, 0.55),
    parseNum(qs("sb1").value, 0.75),
    parseNum(qs("sb2").value, 1.00),
  ];
  cfg.e10.accel_gain = [
    parseNum(qs("ag0").value, 0.35),
    parseNum(qs("ag1").value, 0.55),
    parseNum(qs("ag2").value, 0.85),
  ];
  cfg.e10.accel_threshold = parseNum(qs("accelThreshold").value, 8.0);

  cfg.e10.wheel.threshold_deg = parseNum(qs("wheelTh").value, 90.0);
  cfg.e10.wheel.step_max = parseNum(qs("wheelStepMax").value, 6);

  cfg.e10.gesture.flick_deg = parseNum(qs("flickDeg").value, 200.0);
  cfg.e10.gesture.cooldown_ms = parseNum(qs("cooldownMs").value, 600);

  cfg.e10.scroll_cursor_damp = parseNum(qs("scrollDamp").value, 0.25);

  cfg.e10.precision.enable = parseBool(qs("precEnable").value);
  cfg.e10.precision.deadzone = parseNum(qs("precDeadzone").value, 1.2);
  cfg.e10.precision.gain = parseNum(qs("precGain").value, 0.65);
  cfg.e10.precision.accel = parseNum(qs("precAccel").value, 0.25);
  cfg.e10.precision.max_step = parseNum(qs("precMaxStep").value, 18);
  cfg.e10.precision.smooth = parseNum(qs("precSmooth").value, 0.85);
  cfg.e10.precision.entry_ms = parseNum(qs("precEntryMs").value, 450);
  cfg.e10.precision.exit_ms = parseNum(qs("precExitMs").value, 300);
  cfg.e10.precision.entry_still_deg = parseNum(qs("precEntryStill").value, 1.2);
  cfg.e10.precision.exit_move_deg = parseNum(qs("precExitMove").value, 3.5);
  cfg.e10.precision.profile = parseNum(qs("precProfile").value, 0);

  cfg.e10.precision_mode = parseIntFlex(qs("precMode").value, 0);

  qs("cfgJsonArea").value = pretty(cfg);
  return cfg;
}

function configToUi(cfg){
  cfg = ensureDefaults(cfg);

  qs("wifiMode").value = String(cfg.wifi.mode ?? 0);
  qs("staSsid").value = String(cfg.wifi.sta.ssid ?? "");
  qs("staPass").value = String(cfg.wifi.sta.pass ?? "");
  qs("apSsid").value = String(cfg.wifi.ap.ssid ?? "EliteAirMouse");
  qs("apPass").value = String(cfg.wifi.ap.pass ?? "12345678");
  qs("mdnsHost").value = String(cfg.wifi.mdns.host ?? "elite-airmouse");

  qs("e10Dpi").value = String(cfg.e10.dpi_level ?? 2);
  qs("e10HardClick").value = String((cfg.e10.hard_click_lock ?? true) ? "true" : "false");

  const sb = cfg.e10.scale_base || [0.55,0.75,1.0];
  qs("sb0").value = String(sb[0] ?? 0.55);
  qs("sb1").value = String(sb[1] ?? 0.75);
  qs("sb2").value = String(sb[2] ?? 1.0);

  const ag = cfg.e10.accel_gain || [0.35,0.55,0.85];
  qs("ag0").value = String(ag[0] ?? 0.35);
  qs("ag1").value = String(ag[1] ?? 0.55);
  qs("ag2").value = String(ag[2] ?? 0.85);

  qs("accelThreshold").value = String(cfg.e10.accel_threshold ?? 8.0);
  qs("scrollDamp").value = String(cfg.e10.scroll_cursor_damp ?? 0.25);

  qs("wheelTh").value = String(cfg.e10.wheel.threshold_deg ?? 90.0);
  qs("wheelStepMax").value = String(cfg.e10.wheel.step_max ?? 6);

  qs("flickDeg").value = String(cfg.e10.gesture.flick_deg ?? 200.0);
  qs("cooldownMs").value = String(cfg.e10.gesture.cooldown_ms ?? 600);

  const p = cfg.e10.precision;
  qs("precEnable").value = String((p.enable ?? false) ? "true" : "false");
  qs("precDeadzone").value = String(p.deadzone ?? 1.2);
  qs("precGain").value = String(p.gain ?? 0.65);
  qs("precAccel").value = String(p.accel ?? 0.25);
  qs("precMaxStep").value = String(p.max_step ?? 18);
  qs("precSmooth").value = String(p.smooth ?? 0.85);
  qs("precEntryMs").value = String(p.entry_ms ?? 450);
  qs("precExitMs").value = String(p.exit_ms ?? 300);
  qs("precEntryStill").value = String(p.entry_still_deg ?? 1.2);
  qs("precExitMove").value = String(p.exit_move_deg ?? 3.5);
  qs("precProfile").value = String(p.profile ?? 0);

  qs("precMode").value = String(cfg.e10.precision_mode ?? 0);
  qs("ctlPrecMode").value = qs("precMode").value;

  qs("cfgJsonArea").value = pretty(cfg);

  const fp = wifiFingerprint(cfg);
  qs("swNeedReboot").checked = (g_lastWifiFingerprint && fp !== g_lastWifiFingerprint);
}

function currentJsonFromArea(){
  const t = qs("cfgJsonArea").value || "";
  return JSON.parse(t);
}

async function cfgLoad(){
  const r = await apiGet("/api/config");
  if(!r.ok || !r.json){
    setMsg("Config load failed: " + (r.json?.err || r.text), false);
    return;
  }
  g_config = ensureDefaults(r.json);
  g_lastWifiFingerprint = wifiFingerprint(g_config);
  qs("swNeedReboot").checked = false;

  configToUi(g_config);
  setMsg("Config loaded", true);
}

function cfgValidate(){
  uiToConfig();
  let parsed = null;
  try{ parsed = currentJsonFromArea(); }
  catch(e){
    setMsg("JSON parse error: " + e.message, false);
    return false;
  }
  const errs = validateClient(parsed);
  if(errs.length){
    setMsg("검증 실패:\n- " + errs.join("\n- "), false);
    return false;
  }
  setMsg("검증 OK", true);
  return true;
}

async function cfgApply(){
  uiToConfig();
  if(!cfgValidate()) return;

  const text = qs("cfgJsonArea").value;
  const r = await apiPostText("/api/config/apply", text);
  if(!r.ok){
    setMsg("Apply failed: " + (r.json?.err || r.text), false);
    return;
  }

  // precision_mode는 runtime(C10 owned) 반영
  const cfg = currentJsonFromArea();
  await ctlSetPrecisionMode(cfg?.e10?.precision_mode ?? 0);

  setMsg("Apply OK (not saved). Precision applied via /api/control.", true);
  await refreshStatus();
}

async function cfgSave(){
  uiToConfig();
  if(!cfgValidate()) return;

  const text = qs("cfgJsonArea").value;
  const r = await apiPostText("/api/config/save", text);
  if(!r.ok){
    setMsg("Save failed: " + (r.json?.err || r.text), false);
    return;
  }

  let savedCfg = null;
  try{ savedCfg = JSON.parse(text); }catch(e){}
  if(savedCfg){
    const fp = wifiFingerprint(savedCfg);
    const changed = (g_lastWifiFingerprint && fp !== g_lastWifiFingerprint);
    qs("swNeedReboot").checked = changed;
    g_lastWifiFingerprint = fp;
    if(changed) setMsg("Save OK. WiFi changed -> reboot required.", true);
    else setMsg("Save OK.", true);
  }else{
    setMsg("Save OK.", true);
  }

  await ctlSetPrecisionMode(currentJsonFromArea()?.e10?.precision_mode ?? 0);
  await refreshStatus();
}

async function cfgExport(){
  const a = document.createElement("a");
  a.href = "/api/config/export";
  a.download = "config.json";
  document.body.appendChild(a);
  a.click();
  a.remove();
}

async function cfgImport(file){
  if(!file){ return; }
  const text = await file.text();
  const r = await apiPostText("/api/config/import", text);
  if(!r.ok){
    setMsg("Import failed: " + (r.json?.err || r.text), false);
    return;
  }
  setMsg("Import OK. Reloading...", true);
  await cfgLoad();
  await refreshStatus();
}

async function cfgRollback(){
  if(!confirm("Rollback (.bak) 수행?")) return;
  const r = await fetch("/api/config/rollback", {method:"POST", headers:{"Content-Type":"application/json"}, body:"{}"});
  const t = await r.text();
  let j=null; try{ j=JSON.parse(t);}catch(e){}
  if(!r.ok){
    setMsg("Rollback failed: " + (j?.err || t), false);
    return;
  }
  setMsg("Rollback OK. Reloading...", true);
  await cfgLoad();
  await refreshStatus();
}

/* ---------------- OTA ---------------- */
async function otaUpload(){
  const f = qs("otaFile").files?.[0];
  if(!f){ alert("파일 선택"); return; }

  qs("otaHint").textContent = `uploading: ${f.name} (${f.size} bytes)`;
  const r = await fetch("/api/ota", {method:"POST", body:f});
  const t = await r.text();
  qs("otaHint").textContent = t;
  await otaStatus();
}

async function otaStatus(){
  const r = await apiGet("/api/ota/status");
  qs("otaJson").textContent = pretty(r.json || r.text);
}

/* ---------------- Bind UI ---------------- */
function bindUi(){
  bindTabs();
  bindDiag();

  qs("btnRefresh").addEventListener("click", refreshStatus);
  qs("btnReboot").addEventListener("click", reboot);

  qs("btnSafeInfo").addEventListener("click", safeInfo);
  qs("btnSafeExit").addEventListener("click", safeExit);
  qs("btnFactory").addEventListener("click", factoryReset);

  qs("btnCtlPptOn").addEventListener("click", ()=>ctlSetPpt(true));
  qs("btnCtlPptOff").addEventListener("click", ()=>ctlSetPpt(false));
  qs("btnCtlPrecOff").addEventListener("click", ctlPrecOff);
  qs("btnCtlPrecApply").addEventListener("click", ()=>ctlSetPrecisionMode(qs("ctlPrecMode").value));

  qs("btnPptReload").addEventListener("click", pptReload);
  qs("btnPptSave").addEventListener("click", pptSave);
  qsa("button[data-test]").forEach(b=>{
    b.addEventListener("click", ()=> pptTest(b.getAttribute("data-test")));
  });

  qs("btnOta").addEventListener("click", otaUpload);
  qs("btnOtaStatus").addEventListener("click", otaStatus);

  qs("btnCfgLoad").addEventListener("click", cfgLoad);
  qs("btnCfgValidate").addEventListener("click", cfgValidate);
  qs("btnCfgApply").addEventListener("click", cfgApply);
  qs("btnCfgSave").addEventListener("click", cfgSave);
  qs("btnCfgExport").addEventListener("click", cfgExport);
  qs("btnCfgRollback").addEventListener("click", cfgRollback);

  qs("cfgImportFile").addEventListener("change", (e)=>{
    const f = e.target.files?.[0];
    e.target.value = "";
    cfgImport(f);
  });

  const watchIds = [
    "wifiMode","staSsid","staPass","apSsid","apPass","mdnsHost",
    "e10Dpi","e10HardClick","accelThreshold","scrollDamp",
    "sb0","sb1","sb2","ag0","ag1","ag2","wheelTh","wheelStepMax","flickDeg","cooldownMs",
    "precEnable","precMode","precDeadzone","precGain","precAccel","precMaxStep","precSmooth",
    "precEntryMs","precExitMs","precEntryStill","precExitMove","precProfile"
  ];
  for(const id of watchIds){
    qs(id).addEventListener("input", ()=>{
      const cfg = uiToConfig();
      const fp = wifiFingerprint(cfg);
      qs("swNeedReboot").checked = (g_lastWifiFingerprint && fp !== g_lastWifiFingerprint);
      if(id === "precMode") qs("ctlPrecMode").value = qs("precMode").value;
    });
    qs(id).addEventListener("change", ()=>{
      const cfg = uiToConfig();
      const fp = wifiFingerprint(cfg);
      qs("swNeedReboot").checked = (g_lastWifiFingerprint && fp !== g_lastWifiFingerprint);
      if(id === "precMode") qs("ctlPrecMode").value = qs("precMode").value;
    });
  }

  qs("cfgJsonArea").addEventListener("input", ()=>{
    try{
      const cfg = ensureDefaults(currentJsonFromArea());
      const fp = wifiFingerprint(cfg);
      qs("swNeedReboot").checked = (g_lastWifiFingerprint && fp !== g_lastWifiFingerprint);
      setMsg("JSON edited (not applied)", true);

      if(cfg?.e10?.precision_mode !== undefined){
        qs("precMode").value = String(cfg.e10.precision_mode);
        qs("ctlPrecMode").value = String(cfg.e10.precision_mode);
      }
    }catch(e){
      setMsg("JSON parse error: " + e.message, false);
    }
  });
}

async function main(){
  bindUi();
  await loadSchema();
  await loadKeycodes();
  await pptReload();
  await cfgLoad();
  await refreshStatus();
  await refreshDiag();
  setInterval(refreshStatus, 2500);
}

main().catch(e=>{
  qs("statusJson").textContent = String(e?.stack || e);
});
