/* =======================================================
   File: /www/app_0313.js
   Backend-aligned full (W10_WebConfig_0315 / W10_Web_0315.h align)
   - /api/status
   - /api/diag /clear
   - /api/keycodes
   - /api/config /save /apply /export /import /rollback
   - /api/control
   - /api/ppt /test
   - /api/ota /status
======================================================= */

function qs(id){ return document.getElementById(id); }
function qsa(sel){ return document.querySelectorAll(sel); }

let g_diagTypingUntilMs = 0;

function nowMs(){ return Date.now(); }

function isDiagTyping(){
  const f = qs("diagFilter");
  if(!f) return false;
  const active = (document.activeElement === f);
  const typingWindow = (nowMs() < g_diagTypingUntilMs);
  return active || typingWindow;
}

function isTabOn(name){
  const el = qs("tab-" + name);
  return !!(el && el.classList.contains("on"));
}

async function apiGet(url){
  const r = await fetch(url, { cache:"no-store" });
  const t = await r.text();
  let j = null;
  try{ j = JSON.parse(t); }catch(e){}
  return { ok:r.ok, status:r.status, text:t, json:j };
}

async function apiPostJson(url, obj){
  const r = await fetch(url, {
    method:"POST",
    headers:{ "Content-Type":"application/json" },
    body: JSON.stringify(obj),
    cache:"no-store"
  });
  const t = await r.text();
  let j = null;
  try{ j = JSON.parse(t); }catch(e){}
  return { ok:r.ok, status:r.status, text:t, json:j };
}

function pretty(o){
  try{ return JSON.stringify(o, null, 2); }catch(e){ return String(o); }
}

function setPill(el, text, good){
  if(!el) return;
  el.textContent = text;
  el.style.borderColor = good ? "rgba(76,125,255,.55)" : "rgba(39,48,72,.9)";
  el.style.background = good ? "rgba(76,125,255,.12)" : "rgba(255,255,255,.03)";
}

function setMsg(text, good){
  const el = qs("cfgMsg");
  if(!el) return;
  el.textContent = text;
  el.style.borderColor = good ? "rgba(76,125,255,.55)" : "rgba(255,77,77,.55)";
  el.style.background = good ? "rgba(76,125,255,.10)" : "rgba(255,77,77,.10)";
}

function fillSelect(el, items, valueKey, labelKey){
  if(!el) return;
  el.innerHTML = "";
  for(const it of items){
    const opt = document.createElement("option");
    opt.value = String(it[valueKey]);
    opt.textContent = it[labelKey];
    el.appendChild(opt);
  }
}

function fillPageSelect(el){
  if(!el) return;
  el.innerHTML = "";
  for(const it of [{v:"kb", t:"kb"}, {v:"consumer", t:"consumer"}]){
    const opt = document.createElement("option");
    opt.value = it.v;
    opt.textContent = it.t;
    el.appendChild(opt);
  }
}

function bindPptRow(prefix){
  return {
    page: qs(prefix + "Page"),
    mod:  qs(prefix + "Mod"),
    code: qs(prefix + "Code")
  };
}

let g_keycodes = null;
let g_config = null;
let g_lastWifiFingerprint = "";
let g_lastStatus = null;
let g_schema = null;
let g_cfgBaseline = null;

function unwrapApi(resp){
  const j = resp.json;
  if(j && typeof j === "object" && typeof j.ok === "boolean" && ("code" in j) && ("data" in j)){
    return {
      ok: !!j.ok,
      code: String(j.code || ""),
      msg: String(j.msg || ""),
      data: j.data
    };
  }
  return {
    ok: resp.ok,
    code: resp.ok ? "ok" : `http_${resp.status}`,
    msg: "",
    data: j
  };
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
    if(!n || typeof n !== "object") return null;
    n = n[k];
  }
  return (n && typeof n === "object") ? n : null;
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
  if(s.default !== undefined && (el.placeholder === "" || el.placeholder === undefined)){
    el.placeholder = String(s.default);
  }
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

function parseIntFlex(v, def=0){
  if(v === null || v === undefined) return def;
  const s = String(v).trim();
  if(!s) return def;

  if(/^0x[0-9a-f]+$/i.test(s)){
    const n = parseInt(s, 16);
    return Number.isFinite(n) ? n : def;
  }

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
  return [w.mode, s.ssid, s.pass, a.ssid, a.pass, m.host].map(v => String(v ?? "")).join("|");
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

function deepClone(o){
  try{ return JSON.parse(JSON.stringify(o)); }catch(e){ return null; }
}

function diffObjects(oldObj, newObj){
  if(oldObj === undefined) oldObj = null;
  if(newObj === undefined) newObj = null;

  if(typeof newObj !== "object" || newObj === null){
    return (oldObj === newObj) ? undefined : newObj;
  }

  if(Array.isArray(newObj)){
    const a = JSON.stringify(oldObj);
    const b = JSON.stringify(newObj);
    return (a === b) ? undefined : newObj;
  }

  const patch = {};
  let changed = false;
  const keys = new Set([
    ...(oldObj && typeof oldObj === "object" ? Object.keys(oldObj) : []),
    ...Object.keys(newObj)
  ]);

  for(const k of keys){
    const sub = diffObjects(oldObj ? oldObj[k] : undefined, newObj[k]);
    if(sub !== undefined){
      patch[k] = sub;
      changed = true;
    }
  }
  return changed ? patch : undefined;
}

function getByPath(obj, path){
  let n = obj;
  for(const k of path){
    if(!n || typeof n !== "object") return undefined;
    n = n[k];
  }
  return n;
}

function validateBySchema(cfg){
  const errs = [];
  if(!g_schema) return errs;

  function checkNode(node, val, label){
    if(!node || typeof node !== "object") return;

    if(node.type === "str"){
      const s = String(val ?? "");
      if(node.max_len !== undefined && s.length > Number(node.max_len)) errs.push(`${label}: max_len ${node.max_len}`);
      if(node.min_len !== undefined && s.length < Number(node.min_len) && s.length > 0) errs.push(`${label}: min_len ${node.min_len}`);
      return;
    }

    if(node.type === "bool") return;

    const n = Number(val);
    if(!Number.isFinite(n)){
      errs.push(`${label}: not a number`);
      return;
    }
    if(node.min !== undefined && n < Number(node.min)) errs.push(`${label}: min ${node.min}`);
    if(node.max !== undefined && n > Number(node.max)) errs.push(`${label}: max ${node.max}`);
  }

  const checks = [
    {label:"wifi.sta.ssid", path:["wifi","sta","ssid"], schema:["wifi","sta","ssid"]},
    {label:"wifi.sta.pass", path:["wifi","sta","pass"], schema:["wifi","sta","pass"]},
    {label:"wifi.ap.ssid",  path:["wifi","ap","ssid"],  schema:["wifi","ap","ssid"]},
    {label:"wifi.ap.pass",  path:["wifi","ap","pass"],  schema:["wifi","ap","pass"]},
    {label:"wifi.mdns.host",path:["wifi","mdns","host"],schema:["wifi","mdns","host"]},

    {label:"e10.accel_threshold", path:["e10","accel_threshold"], schema:["e10","accel_threshold"]},
    {label:"e10.scroll_cursor_damp", path:["e10","scroll_cursor_damp"], schema:["e10","scroll_cursor_damp"]},
    {label:"e10.precision_mode", path:["e10","precision_mode"], schema:["e10","precision_mode"]},

    {label:"e10.wheel.threshold_deg", path:["e10","wheel","threshold_deg"], schema:["e10","wheel_threshold_deg"]},
    {label:"e10.wheel.step_max", path:["e10","wheel","step_max"], schema:["e10","wheel_step_max"]},
    {label:"e10.gesture.flick_deg", path:["e10","gesture","flick_deg"], schema:["e10","gesture_flick_deg"]},
    {label:"e10.gesture.cooldown_ms", path:["e10","gesture","cooldown_ms"], schema:["e10","gesture_cooldown_ms"]},
  ];

  for(const c of checks){
    const v = getByPath(cfg, c.path);
    const node = _getSchemaNode(c.schema);
    checkNode(node, v, c.label);
  }

  for(let i=0;i<3;i++){
    const sb = getByPath(cfg, ["e10","scale_base",i]);
    const ag = getByPath(cfg, ["e10","accel_gain",i]);
    checkNode(_getSchemaNode(["e10", `scale_base_${i}`]), sb, `e10.scale_base[${i}]`);
    checkNode(_getSchemaNode(["e10", `accel_gain_${i}`]), ag, `e10.accel_gain[${i}]`);
  }

  return errs;
}

/* ---------------- Tabs ---------------- */
function bindTabs(){
  qsa(".tab").forEach(b => {
    b.addEventListener("click", () => {
      const prevDiagOn = isTabOn("diag");

      qsa(".tab").forEach(x => x.classList.remove("on"));
      qsa(".tabpane").forEach(x => x.classList.remove("on"));

      b.classList.add("on");
      const id = "tab-" + b.getAttribute("data-tab");
      const pane = qs(id);
      if(pane) pane.classList.add("on");

      const nowDiagOn = isTabOn("diag");
      if(!prevDiagOn && nowDiagOn){
        // Diagnostics 탭 진입 시 1회 갱신(auto ON/OFF 공통)
        refreshDiag().catch(console.error);
      }
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
    const el = qs("statusJson");
    if(el) el.textContent = u.msg || r.text || "status failed";
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

  if(qs("stUptime")) qs("stUptime").textContent = `${sys.uptime_ms ?? j.uptime_ms ?? "-"} ms`;
  if(qs("stHeap"))   qs("stHeap").textContent   = `${mem.heap_free ?? j.heap_free ?? "-"} bytes`;

  if(qs("stWiFi")) qs("stWiFi").textContent = `${net.mode ?? "-"} / ${net.ssid ?? "-"}`;
  if(qs("stMdns")) qs("stMdns").textContent = `${net.mdns ?? "-"}`;

  if(qs("stPptMode")) qs("stPptMode").textContent = String(e10.ppt_mode ?? "-");
  if(qs("stPrec"))    qs("stPrec").textContent    = formatPrecSummary(e10);

  setPill(qs("pillNet"),  `NET: ${net.mode ?? "-"}`, true);
  setPill(qs("pillBle"),  `BLE: ${e10.ble_connected ? "ON" : "OFF"}`, !!e10.ble_connected);
  setPill(qs("pillSafe"), `SAFE: ${boot.safe_mode ? "ON" : "OFF"}`, !!boot.safe_mode);

  const needReboot = !!pol.reboot_required;
  const btnRb = qs("btnReboot");
  if(btnRb){
    btnRb.disabled = !needReboot;
    btnRb.title = needReboot ? `재부팅 필요: ${pol.reboot_reasons || ""}` : "재부팅 필요 없음";
  }

  const el = qs("statusJson");
  if(el) el.textContent = pretty(u);
}

/* ---------------- Diagnostics ---------------- */
function renderDiag(u){
  const data = u.data || {};
  const d = data.diag || {};
  const events = Array.isArray(data.events) ? data.events : [];

  const cEl = qs("diagCounters");
  if(cEl){
    const items = [
      ["body_too_large", d.body_too_large_count],
      ["no_body_slot",   d.no_body_slot_count],
      ["bad_json",       d.bad_json_count],
      ["safe_blocked",   d.safe_blocked_count],
      ["ota_blocked",    d.ota_blocked_count],
    ];

    cEl.innerHTML = "";
    for(const [k, v] of items){
      const div = document.createElement("div");
      div.className = "pill";
      div.textContent = `${k}: ${v ?? 0}`;
      cEl.appendChild(div);
    }
  }

  const listEl = qs("diagEvents");
  if(listEl){
    const f = (qs("diagFilter")?.value || "").trim();
    listEl.innerHTML = "";

    const shown = events
      .slice()
      .reverse()
      .filter(ev => !f || String(ev.code || "").includes(f))
      .slice(0, 80);

    for(const ev of shown){
      const div = document.createElement("div");
      div.className = "logline";
      div.textContent = `[${ev.ms ?? 0}ms] ${ev.code ?? "-"}`;
      listEl.appendChild(div);
    }

    if(!shown.length){
      const div = document.createElement("div");
      div.className = "hint2";
      div.textContent = "No events.";
      listEl.appendChild(div);
    }
  }

  const rawEl = qs("diagJson");
  if(rawEl) rawEl.textContent = pretty(u);
}

async function refreshDiag(){
  const r = await apiGet("/api/diag");
  const u = unwrapApi(r);
  renderDiag(u);
}

function bindDiag(){
  const b1 = qs("btnDiagRefresh");
  const b2 = qs("btnDiagClear");
  const f  = qs("diagFilter");

  if(b1){
    b1.addEventListener("click", refreshDiag);
  }

  if(b2){
    b2.addEventListener("click", async () => {
      if(!confirm("진단 카운터/이벤트를 초기화할까요?")) return;
      const r = await apiPostJson("/api/diag/clear", {});
      renderDiag(unwrapApi(r));
      await refreshStatus();
    });
  }

  if(f){
    f.addEventListener("input", () => {
      g_diagTypingUntilMs = nowMs() + 1200;
      refreshDiag().catch(console.error); // auto OFF여도 즉시 반영
    });

    f.addEventListener("focus", () => {
      g_diagTypingUntilMs = nowMs() + 1200;
    });

    f.addEventListener("blur", () => {
      g_diagTypingUntilMs = 0;
    });
  }
}

/* ---------------- keycodes + datalist ---------------- */
function buildDatalist(dlEl, items, kind){
  if(!dlEl) return;
  dlEl.innerHTML = "";

  const max = 220;
  for(let i=0;i<items.length && i<max;i++){
    const it = items[i];
    const opt = document.createElement("option");

    if(kind === "kb"){
      const code = Number(it.code) || 0;
      opt.value = `0x${code.toString(16).toUpperCase().padStart(2, "0")}`;
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
  if(!row || !row.page || !row.mod || !row.code) return;

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
  const modItems = mods.map(m => ({
    mask:m.mask,
    name:`${m.name} (0x${Number(m.mask).toString(16)})`
  }));

  const modSelects = ["pptStartMod","pptExitMod","pptNextMod","pptPrevMod","pptBlackMod","pptLaserMod"];
  for(const id of modSelects){
    fillSelect(qs(id), modItems, "mask", "name");
  }

  const pageSelects = ["pptStartPage","pptExitPage","pptNextPage","pptPrevPage","pptBlackPage","pptLaserPage"];
  for(const id of pageSelects){
    fillPageSelect(qs(id));
  }

  const pmRaw = r.json.precision_modes || [];
  const pmItems = pmRaw.map(x => ({
    value: (x.value ?? x.mode ?? 0),
    name:  String(x.name ?? `mode${x.value ?? x.mode ?? 0}`)
  }));
  if(pmItems.length === 0) pmItems.push({value:0, name:"0"});

  fillSelect(qs("precMode"), pmItems, "value", "name");
  fillSelect(qs("ctlPrecMode"), pmItems, "value", "name");

  buildDatalist(qs("dlKb"), r.json.kb || [], "kb");
  buildDatalist(qs("dlConsumer"), r.json.consumer || [], "consumer");

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
    if(row.page){
      row.page.addEventListener("change", () => syncPptRowHints(row));
    }
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

  return {
    save: qs("swPptSave")?.checked ?? true,
    map
  };
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
    if(r.page) r.page.value = (m.page === "consumer") ? "consumer" : "kb";
    if(r.mod)  r.mod.value  = String(m.mod ?? 0);
    if(r.code) r.code.value = (m.code !== undefined && m.code !== null) ? String(m.code) : "0";
    syncPptRowHints(r);
  }

  const el = qs("pptJson");
  if(el) el.textContent = pretty({ map });
}

async function pptReload(){
  const r = await apiGet("/api/ppt");
  if(!r.ok || !r.json){
    alert("ppt load failed");
    return;
  }
  setPptForm(r.json.map || {});
}

async function pptSave(){
  const payload = getPptPayload();
  const r = await apiPostJson("/api/ppt", payload);
  if(!r.ok){
    alert("ppt save failed: " + (r.json?.err || r.text));
    return;
  }
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
  if(!res.ok){
    alert("test failed: " + (res.json?.err || res.text));
  }
}

/* ---------------- Control ---------------- */
async function ctlSetPpt(enable){
  const payload = {
    cmd:"set_ppt",
    enable: !!enable,
    snapshot: qs("ctlSnapshot")?.checked ?? true
  };
  const r = await apiPostJson("/api/control", payload);
  if(!r.ok) alert("set_ppt failed: " + (r.json?.err || r.text));
  await refreshStatus();
}

async function ctlSetPrecisionMode(mode){
  const v = parseIntFlex(mode, 0);
  const payload = {
    cmd:"set_precision",
    mode: v,
    snapshot: qs("ctlSnapshot")?.checked ?? true
  };
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
  const r = await apiPostJson("/api/safeboot", { exit:true });
  alert(pretty(r.json || r.text));
}

async function factoryReset(){
  if(!confirm("Factory Reset 진행? (재부팅됨)")) return;
  const r = await fetch("/api/factory_reset", { method:"POST" });
  const t = await r.text();
  alert(t);
}

async function reboot(){
  if(!confirm("재부팅 할까요?")) return;

  let pol = (g_lastStatus?.policy) || (g_lastStatus?.groups?.policy) || {};
  if(pol.reboot_required === undefined){
    const chk = unwrapApi(await apiGet("/api/reboot/check"));
    if(chk.ok){
      pol = {
        reboot_required: chk.data?.required,
        reboot_reason_mask: chk.data?.mask,
        reboot_reasons: chk.data?.reasons
      };
    }
  }

  const mask = Number(pol.reboot_reason_mask || 0) >>> 0;
  const body = {};
  if(mask) body.reason_mask = mask;

  const r = unwrapApi(await apiPostJson("/api/reboot", body));
  alert(pretty(r));
}

/* ---------------- Config Editor ---------------- */
function validateClient(cfg){
  return validateBySchema(cfg);
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

  const p = cfg.e10.precision || {};
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
  g_cfgBaseline = deepClone(g_config);
  g_lastWifiFingerprint = wifiFingerprint(g_config);

  qs("swNeedReboot").checked = false;
  configToUi(g_config);
  setMsg("Config loaded", true);
}

function cfgValidate(){
  uiToConfig();

  let parsed = null;
  try{
    parsed = currentJsonFromArea();
  }catch(e){
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

  const curr = currentJsonFromArea();
  const base = g_cfgBaseline || {};

  const patch = diffObjects(base, curr) || {};
  if(!patch || (typeof patch === "object" && !Array.isArray(patch) && Object.keys(patch).length === 0)){
    setMsg("No changes to apply.", true);
    return;
  }

  const r = await apiPostJson("/api/config/apply", patch);
  if(!r.ok){
    setMsg("Apply failed: " + (r.json?.err || r.text), false);
    return;
  }

  const cfg = currentJsonFromArea();
  await ctlSetPrecisionMode(cfg?.e10?.precision_mode ?? 0);

  setMsg("Apply OK (not saved). Precision applied via /api/control.", true);
  await refreshStatus();
}

async function cfgSave(){
  uiToConfig();
  if(!cfgValidate()) return;

  const curr = currentJsonFromArea();
  const base = g_cfgBaseline || {};

  const patch = diffObjects(base, curr) || {};
  if(!patch || (typeof patch === "object" && !Array.isArray(patch) && Object.keys(patch).length === 0)){
    setMsg("No changes to save.", true);
    return;
  }

  const prevFp = g_lastWifiFingerprint || "";
  const r = await apiPostJson("/api/config/save", patch);

  if(!r.ok){
    setMsg("Save failed: " + (r.json?.err || r.text), false);
    return;
  }

  let savedCfg = null;

  await cfgLoad();

  try{ savedCfg = currentJsonFromArea(); }catch(e){}

  if(savedCfg){
    const fp = wifiFingerprint(savedCfg);
    const changed = (prevFp && fp !== prevFp);
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
  if(!file) return;

  const text = await file.text();

  let obj = null;
  try{
    obj = JSON.parse(text);
  }catch(e){
    setMsg("Import JSON parse error: " + e.message, false);
    return;
  }

  const r = await apiPostJson("/api/config/import", obj);
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

  const r = await fetch("/api/config/rollback", {
    method:"POST",
    headers:{ "Content-Type":"application/json" },
    body:"{}"
  });

  const t = await r.text();
  let j = null;
  try{ j = JSON.parse(t); }catch(e){}

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
  if(!f){
    alert("파일 선택");
    return;
  }

  qs("otaHint").textContent = `uploading: ${f.name} (${f.size} bytes)`;
  const r = await fetch("/api/ota", { method:"POST", body:f });
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

  qs("btnRefresh")?.addEventListener("click", refreshStatus);
  qs("btnReboot")?.addEventListener("click", reboot);

  qs("btnSafeInfo")?.addEventListener("click", safeInfo);
  qs("btnSafeExit")?.addEventListener("click", safeExit);
  qs("btnFactory")?.addEventListener("click", factoryReset);

  qs("btnCtlPptOn")?.addEventListener("click", () => ctlSetPpt(true));
  qs("btnCtlPptOff")?.addEventListener("click", () => ctlSetPpt(false));
  qs("btnCtlPrecOff")?.addEventListener("click", ctlPrecOff);
  qs("btnCtlPrecApply")?.addEventListener("click", () => ctlSetPrecisionMode(qs("ctlPrecMode").value));

  qs("btnPptReload")?.addEventListener("click", pptReload);
  qs("btnPptSave")?.addEventListener("click", pptSave);
  qsa("button[data-test]").forEach(b => {
    b.addEventListener("click", () => pptTest(b.getAttribute("data-test")));
  });

  qs("btnOta")?.addEventListener("click", otaUpload);
  qs("btnOtaStatus")?.addEventListener("click", otaStatus);

  qs("btnCfgLoad")?.addEventListener("click", cfgLoad);
  qs("btnCfgValidate")?.addEventListener("click", cfgValidate);
  qs("btnCfgApply")?.addEventListener("click", cfgApply);
  qs("btnCfgSave")?.addEventListener("click", cfgSave);
  qs("btnCfgExport")?.addEventListener("click", cfgExport);
  qs("btnCfgRollback")?.addEventListener("click", cfgRollback);

  qs("cfgImportFile")?.addEventListener("change", (e) => {
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
    const el = qs(id);
    if(!el) continue;

    el.addEventListener("input", () => {
      const cfg = uiToConfig();
      const fp = wifiFingerprint(cfg);
      qs("swNeedReboot").checked = (g_lastWifiFingerprint && fp !== g_lastWifiFingerprint);
      if(id === "precMode") qs("ctlPrecMode").value = qs("precMode").value;
    });

    el.addEventListener("change", () => {
      const cfg = uiToConfig();
      const fp = wifiFingerprint(cfg);
      qs("swNeedReboot").checked = (g_lastWifiFingerprint && fp !== g_lastWifiFingerprint);
      if(id === "precMode") qs("ctlPrecMode").value = qs("precMode").value;
    });
  }

  qs("cfgJsonArea")?.addEventListener("input", () => {
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

/* ---------------- Main ---------------- */
async function main(){
  bindUi();
  await loadSchema();
  await loadKeycodes();
  await pptReload();
  await cfgLoad();
  await refreshStatus();

  // 초기 diag 갱신은 조건부:
  // - diagnostics 탭이 현재 on 이거나
  // - auto 가 on 인 경우
  const auto0 = qs("diagAuto")?.checked;
  if(isTabOn("diag") || auto0){
    await refreshDiag();
  }

  setInterval(async () => {
    await refreshStatus();

    const auto = qs("diagAuto")?.checked;
    if(auto && isTabOn("diag") && !isDiagTyping()){
      await refreshDiag();
    }
  }, 2500);
}

main().catch(e => {
  const el = qs("statusJson");
  if(el) el.textContent = String(e?.stack || e);
});