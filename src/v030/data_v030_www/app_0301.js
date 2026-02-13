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

// precision enum (백엔드가 내려주면 우선 사용, 없으면 fallback)
let g_precisionModes = [
  {value:0, label:"0 (OFF)"},
  {value:1, label:"1 (ON)"},
];

function wifiFingerprint(cfg){
  const w = cfg?.wifi || {};
  const s = w?.sta || {};
  const a = w?.ap || {};
  const m = w?.mdns || {};
  return [ w.mode, s.ssid, s.pass, a.ssid, a.pass, m.host ]
    .map(v=>String(v ?? "")).join("|");
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

function validateClient(cfg){
  const errs = [];

  // WiFi
  const w = cfg.wifi || {};
  const mode = parseNum(w.mode, 0);
  if(!(mode === 0 || mode === 1 || mode === 2)) errs.push("wifi.mode must be 0|1|2");

  const apPass = String(w?.ap?.pass ?? "");
  if(apPass.length > 0 && apPass.length < 8) errs.push("wifi.ap.pass must be empty or >=8 chars");

  const mdns = String(w?.mdns?.host ?? "");
  if(mdns.length > 32) errs.push("wifi.mdns.host len <= 32");

  // E10 numeric ranges (서버 validate 범위에 맞춤)
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
  const cfg = JSON.parse(qs("cfgJsonArea").value || "{}");

  // WiFi
  cfg.wifi = cfg.wifi || {};
  cfg.wifi.mode = parseNum(qs("wifiMode").value, 0);
  cfg.wifi.sta = cfg.wifi.sta || {};
  cfg.wifi.sta.ssid = String(qs("staSsid").value || "");
  cfg.wifi.sta.pass = String(qs("staPass").value || "");
  cfg.wifi.ap = cfg.wifi.ap || {};
  cfg.wifi.ap.ssid = String(qs("apSsid").value || "");
  cfg.wifi.ap.pass = String(qs("apPass").value || "");
  cfg.wifi.mdns = cfg.wifi.mdns || {};
  cfg.wifi.mdns.host = String(qs("mdnsHost").value || "");

  // E10
  cfg.e10 = cfg.e10 || {};
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

  cfg.e10.wheel = cfg.e10.wheel || {};
  cfg.e10.wheel.threshold_deg = parseNum(qs("wheelTh").value, 90.0);
  cfg.e10.wheel.step_max = parseNum(qs("wheelStepMax").value, 6);

  cfg.e10.gesture = cfg.e10.gesture || {};
  cfg.e10.gesture.flick_deg = parseNum(qs("flickDeg").value, 200.0);
  cfg.e10.gesture.cooldown_ms = parseNum(qs("cooldownMs").value, 600);

  cfg.e10.scroll_cursor_damp = parseNum(qs("scrollDamp").value, 0.25);

  cfg.e10.precision = cfg.e10.precision || {};
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

  qs("cfgJsonArea").value = pretty(cfg);
  return cfg;
}

function configToUi(cfg){
  // 안전 기본 구조
  cfg.wifi = cfg.wifi || {};
  cfg.wifi.sta = cfg.wifi.sta || {};
  cfg.wifi.ap = cfg.wifi.ap || {};
  cfg.wifi.mdns = cfg.wifi.mdns || {};
  cfg.e10 = cfg.e10 || {};
  cfg.e10.wheel = cfg.e10.wheel || {};
  cfg.e10.gesture = cfg.e10.gesture || {};
  cfg.e10.precision = cfg.e10.precision || {};

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

  qs("cfgJsonArea").value = pretty(cfg);

  // WiFi 변경 감지
  const fp = wifiFingerprint(cfg);
  qs("swNeedReboot").checked = (g_lastWifiFingerprint && fp !== g_lastWifiFingerprint);
}

function currentJsonFromArea(){
  const t = qs("cfgJsonArea").value || "";
  return JSON.parse(t);
}

// ---------------- Status ----------------
function _labelForPrecMode(mode){
  const m = Number(mode);
  const hit = g_precisionModes.find(x => Number(x.value) === m);
  return hit ? hit.label : String(mode ?? "-");
}

async function refreshStatus(){
  const r = await apiGet("/api/status");
  if(!r.ok || !r.json){
    qs("statusJson").textContent = r.text || "status failed";
    return;
  }

  const j = r.json;
  qs("stUptime").textContent = `${j.uptime_ms ?? "-"} ms`;
  qs("stHeap").textContent = `${j.heap_free ?? "-"} bytes`;

  const net = j.net || {};
  qs("stWiFi").textContent = `${net.mode ?? "-"} / ${net.ssid ?? "-"}`;
  qs("stMdns").textContent = `${net.mdns ?? "-"}`;

  const e10 = j.e10 || {};
  qs("stPptMode").textContent = String(e10.ppt_mode ?? "-");
  // 백엔드 기준: precision_enable 없음, precision_mode만 표기
  qs("stPrecMode").textContent = _labelForPrecMode(e10.precision_mode);

  const boot = j.boot || {};
  setPill(qs("pillNet"), `NET: ${net.mode ?? "-"}`, true);
  setPill(qs("pillBle"), `BLE: ${e10.ble_connected ? "ON" : "OFF"}`, !!e10.ble_connected);
  setPill(qs("pillSafe"), `SAFE: ${boot.safe_mode ? "ON" : "OFF"}`, !!boot.safe_mode);

  // Quick Control 셀렉트에 현재값 반영(옵션이 있으면)
  const cur = String(e10.precision_mode ?? "0");
  if (qs("ctlPrecMode").value !== cur) qs("ctlPrecMode").value = cur;

  qs("statusJson").textContent = pretty(j);
}

// ---------------- Tabs ----------------
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

// ---------------- Keycodes (+ precision modes) ----------------
function _fallbackPrecisionModes(){
  return [
    {value:0, label:"0 (OFF)"},
    {value:1, label:"1 (ON)"},
  ];
}

function _extractPrecisionModesFromKeycodes(kc){
  // 기대 형태(향후 백엔드 확장 가정):
  // { precision_modes: [ {value:0,name:"OFF"}, ... ] }
  const pm = kc?.precision_modes;
  if(Array.isArray(pm) && pm.length){
    const out = [];
    for(const it of pm){
      const v = (it?.value ?? it?.mode ?? it?.id);
      const n = (it?.name ?? it?.label ?? it?.text);
      if(v === undefined || v === null) continue;
      out.push({value:Number(v), label: n ? `${v} (${n})` : String(v)});
    }
    if(out.length) return out;
  }
  return _fallbackPrecisionModes();
}

function _fillPrecisionControlSelect(){
  const el = qs("ctlPrecMode");
  fillSelect(el, g_precisionModes, "value", "label");
}

async function loadKeycodes(){
  const r = await apiGet("/api/keycodes");
  if(!r.ok || !r.json) throw new Error("keycodes load failed");
  g_keycodes = r.json;

  // mods
  const mods = r.json.mods || [];
  const modItems = mods.map(m => ({mask:m.mask, name:`${m.name} (0x${Number(m.mask).toString(16)})`}));

  const modSelects = ["pptStartMod","pptExitMod","pptNextMod","pptPrevMod","pptBlackMod","pptLaserMod"];
  for(const id of modSelects) fillSelect(qs(id), modItems, "mask", "name");

  // pages
  const pageSelects = ["pptStartPage","pptExitPage","pptNextPage","pptPrevPage","pptBlackPage","pptLaserPage"];
  for(const id of pageSelects) fillPageSelect(qs(id));

  // precision enum (있으면 사용, 없으면 fallback)
  g_precisionModes = _extractPrecisionModesFromKeycodes(r.json);
  _fillPrecisionControlSelect();

  return r.json;
}

// ---------------- Quick Control (precision mode) ----------------
async function ctlPrecApply(){
  const mode = Number(qs("ctlPrecMode").value) || 0;
  const payload = { cmd:"set_precision", mode: mode, snapshot:true };
  const r = await apiPostJson("/api/control", payload);
  if(!r.ok){
    alert("Precision apply failed: " + (r.json?.err || r.text));
    return;
  }
  await refreshStatus();
}

// ---------------- PPT ----------------
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
    map[k] = { page: r.page.value, mod: Number(r.mod.value) || 0, code: Number(r.code.value) || 0 };
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
    r.code.value = String(m.code ?? 0);
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
  const payload = { page: r.page.value, mod: Number(r.mod.value)||0, code: Number(r.code.value)||0 };
  const res = await apiPostJson("/api/ppt/test", payload);
  if(!res.ok) alert("test failed");
}

// ---------------- SafeBoot/Reset/Reboot ----------------
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
  await fetch("/api/reboot", {method:"POST"});
}

// ---------------- Config Editor ----------------
async function cfgLoad(){
  const r = await apiGet("/api/config");
  if(!r.ok || !r.json){
    setMsg("Config load failed: " + (r.json?.err || r.text), false);
    return;
  }
  g_config = r.json;

  // baseline
  g_lastWifiFingerprint = wifiFingerprint(g_config);
  qs("swNeedReboot").checked = false;

  configToUi(g_config);
  setMsg("Config loaded", true);
}

function cfgValidate(){
  // UI → JSON 반영
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
  setMsg("Apply OK (not saved). WiFi fields validated only.", true);
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

  // WiFi 변경 여부 감지 → 재부팅 표시
  let savedCfg = null;
  try{ savedCfg = JSON.parse(text); }catch(e){}
  if(savedCfg){
    const fp = wifiFingerprint(savedCfg);
    const changed = (g_lastWifiFingerprint && fp !== g_lastWifiFingerprint);
    qs("swNeedReboot").checked = changed;
    if(changed) setMsg("Save OK. WiFi changed -> reboot required.", true);
    else setMsg("Save OK.", true);
    g_lastWifiFingerprint = fp;
  }else{
    setMsg("Save OK.", true);
  }

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

// ---------------- OTA ----------------
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

// ---------------- Bind UI ----------------
function bindUi(){
  bindTabs();

  qs("btnRefresh").addEventListener("click", refreshStatus);
  qs("btnReboot").addEventListener("click", reboot);

  qs("btnCtlPrecApply").addEventListener("click", ctlPrecApply);

  qs("btnSafeInfo").addEventListener("click", safeInfo);
  qs("btnSafeExit").addEventListener("click", safeExit);
  qs("btnFactory").addEventListener("click", factoryReset);

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

  // UI 변경 시 JSON 반영 + WiFi 변경 감지
  const watchIds = [
    "wifiMode","staSsid","staPass","apSsid","apPass","mdnsHost",
    "e10Dpi","e10HardClick","accelThreshold","scrollDamp",
    "sb0","sb1","sb2","ag0","ag1","ag2","wheelTh","wheelStepMax","flickDeg","cooldownMs",
    "precEnable","precDeadzone","precGain","precAccel","precMaxStep","precSmooth",
    "precEntryMs","precExitMs","precEntryStill","precExitMove","precProfile"
  ];
  for(const id of watchIds){
    qs(id).addEventListener("input", ()=>{
      const cfg = uiToConfig();
      const fp = wifiFingerprint(cfg);
      qs("swNeedReboot").checked = (g_lastWifiFingerprint && fp !== g_lastWifiFingerprint);
    });
  }

  // JSON textarea 직접 수정 시
  qs("cfgJsonArea").addEventListener("input", ()=>{
    try{
      const cfg = currentJsonFromArea();
      const fp = wifiFingerprint(cfg);
      qs("swNeedReboot").checked = (g_lastWifiFingerprint && fp !== g_lastWifiFingerprint);
      setMsg("JSON edited (not applied)", true);
    }catch(e){
      setMsg("JSON parse error: " + e.message, false);
    }
  });
}

async function main(){
  bindUi();
  await loadKeycodes();
  await pptReload();
  await cfgLoad();
  await refreshStatus();
  setInterval(refreshStatus, 2500);
}

main().catch(e=>{
  qs("statusJson").textContent = String(e?.stack || e);
});
