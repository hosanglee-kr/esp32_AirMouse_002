// =======================================================
// File: src/v010/data_v010/www/app_0274.js
// =======================================================
const $ = (id)=>document.getElementById(id);

async function jget(url){
  const r = await fetch(url, {cache:"no-store"});
  return await r.json();
}
async function jpost(url, body){
  const r = await fetch(url, {
    method:"POST",
    headers: {"Content-Type":"application/json"},
    body: JSON.stringify(body)
  });
  return await r.json();
}
function pretty(obj){ return JSON.stringify(obj, null, 2); }

let pollTimer=null;
let KEYCODES = { mods:[], kb:[], consumer:[] };
let PPTMAP = null;
let CFGRAW = null;

function setAutoPoll(on){
  if(pollTimer){ clearInterval(pollTimer); pollTimer=null; }
  if(on) pollTimer=setInterval(refreshStatus, 1000);
}

function banner(state){
  const el = $("healthBanner");
  el.classList.remove("ok","warn","bad");
  if(state===0){ el.classList.add("ok"); el.textContent="OK"; }
  else if(state===1){ el.classList.add("warn"); el.textContent="WARN"; }
  else { el.classList.add("bad"); el.textContent="DEGRADED"; }
}

function addAlert(list, msg){
  if(!msg) return;
  const li = document.createElement("li");
  li.textContent = msg;
  list.appendChild(li);
}

function fmt(n, d=2){
  if(n===null || n===undefined) return "-";
  if(typeof n==="number") return n.toFixed(d);
  return String(n);
}
function clamp(n, lo, hi){ n=Number(n); if(Number.isNaN(n)) return lo; return Math.max(lo, Math.min(hi,n)); }

function setPill(el, text){ el.textContent=text; }

// ---------- Status ----------
async function refreshStatus(){
  try{
    const s = await jget("/api/status");
    $("statusBox").textContent = pretty(s);

    const e = (s.e10||{});
    const h = (e.health||{});
    const gyro = (e.gyro||{});
    const an = (e.anomaly||{});
    const i2c = (e.i2c||{});
    const net = (s.net||{});
    const boot = (s.boot||{});

    banner(h.state||0);

    $("kGyro").textContent = fmt(gyro.rms||0,2);
    $("kCursor").textContent = fmt(e.cursor_rms||0,2);
    $("kSpike").textContent = String(an.spike_count_10s ?? "-");
    $("kFail").textContent = String(an.consecutive_fail ?? "-");
    $("kI2c").textContent = `${i2c.recover_count ?? "-"} / ${i2c.recover_last_ok ? "OK":"FAIL"}`;
    $("kBle").textContent = (e.ble_connected ? "ON":"OFF");

    setPill($("netPill"), `${net.mode||"-"} • ${net.ip||"-"} • ${net.ssid||"-"}`);
    setPill($("bootPill"), `safe=${boot.safe_mode?1:0} fail=${boot.fail_count ?? "-"} pending=${boot.pending?1:0}`);

    // sync runtime controls (best-effort)
    $("ctlPptMode").checked = !!e.ppt_mode;
    $("ctlPrecMode").checked = !!e.precision_mode;
    $("ctlDpi").value = String(e.dpi_level ?? "2");

    // alerts
    const ul = $("alertList");
    ul.innerHTML = "";

    if((h.score||1000) < 620) addAlert(ul, `Health DEGRADED (score=${h.score})`);
    else if((h.score||1000) < 820) addAlert(ul, `Health WARN (score=${h.score})`);

    if((gyro.rms||0) > 8) addAlert(ul, `High gyro noise RMS=${fmt(gyro.rms,2)} deg/s`);
    if((e.cursor_rms||0) > 6) addAlert(ul, `High cursor noise RMS=${fmt(e.cursor_rms,2)} px`);

    if((an.spike_count_10s||0) >= 3) addAlert(ul, `Spike detected: ${an.spike_count_10s} in 10s`);
    if((an.consecutive_fail||0) >= 5) addAlert(ul, `Consecutive fail: ${an.consecutive_fail}`);

    if((i2c.recover_count||0) >= 2) addAlert(ul, `I2C recover happened: ${i2c.recover_count}`);
    if(i2c.recover_last_ok === false) addAlert(ul, `I2C recover last FAILED`);

    if(boot.safe_mode) addAlert(ul, `SAFE BOOT ACTIVE: AP forced (-SAFE). Exit via SafeBoot 해제 + reboot.`);

    if(ul.children.length===0) addAlert(ul, "No alerts.");
  }catch(e){
    $("statusBox").textContent = "status error: " + e;
  }
}

// ---------- Keycodes ----------
async function loadKeycodes(){
  KEYCODES = await jget("/api/keycodes");
  renderPptTable();
}
function toHex2(n){ return "0x" + (n>>>0).toString(16).toUpperCase().padStart(2,"0"); }
function toHex8(n){ return "0x" + (n>>>0).toString(16).toUpperCase().padStart(8,"0"); }

function filterKbList(query){
  query = (query||"").trim().toLowerCase();
  if(!query) return KEYCODES.kb || [];

  const hexMatch = query.match(/^(0x)?([0-9a-f]{1,2})$/);
  let hex = null;
  if(hexMatch) hex = parseInt(hexMatch[2], 16);

  return (KEYCODES.kb||[]).filter(k=>{
    const name = (k.name||"").toLowerCase();
    if(hex!==null) return (k.code===hex) || name.includes(query);
    return name.includes(query);
  });
}
function kbOptionList(query){
  const list = filterKbList(query).slice(0, 400);
  return list.map(k=>({ value:k.code, label:`${k.name} (${toHex2(k.code)})` }));
}
function makeSelect(options, value){
  const sel = document.createElement("select");
  options.forEach(opt=>{
    const o=document.createElement("option");
    o.value = opt.value;
    o.textContent = opt.label;
    if(String(opt.value)===String(value)) o.selected=true;
    sel.appendChild(o);
  });
  return sel;
}

// ---------- PPT editor ----------
const PPT_ACTIONS = ["start","exit","next","prev","black","laser"];

async function loadPptMap(){
  const r = await jget("/api/ppt");
  PPTMAP = r.map || {};
  renderPptTable();
  $("pptMsg").textContent = "loaded";
}

function currentRowData(action, row){
  const pageSel = row.querySelector(`[data-role="page"]`);
  const modSel  = row.querySelector(`[data-role="mod"]`);
  const kbSel   = row.querySelector(`[data-role="kbkey"]`);
  const conSel  = row.querySelector(`[data-role="conpreset"]`);
  const rawIn   = row.querySelector(`[data-role="rawmask"]`);

  const page = pageSel.value; // "kb" | "consumer"
  let mod = parseInt(modSel.value||"0",10);
  let code = 0;

  if(page==="consumer"){
    const preset = conSel.value;
    if(preset==="raw"){
      const txt = (rawIn.value||"0").trim().toLowerCase();
      if(txt.startsWith("0x")) code = parseInt(txt,16)>>>0;
      else code = parseInt(txt,10)>>>0;
    }else{
      code = parseInt(preset,10)>>>0;
    }
    mod = 0;
  }else{
    code = parseInt(kbSel.value||"0",10)>>>0;
  }
  return { action, page, mod, code };
}

function renderPptTable(){
  const box = $("pptTable");
  if(!box) return;
  box.innerHTML = "";

  const q = $("keySearch") ? $("keySearch").value : "";

  PPT_ACTIONS.forEach(action=>{
    const row = document.createElement("div");
    row.className = "pptRow";

    const lbl = document.createElement("div");
    lbl.className="lbl";
    lbl.textContent = action.toUpperCase();

    const pageVal = (PPTMAP && PPTMAP[action] && PPTMAP[action].page) ? PPTMAP[action].page : "kb";
    const pageSel = makeSelect([
      {value:"kb", label:"Keyboard(0x07)"},
      {value:"consumer", label:"Consumer(Media)"}
    ], pageVal);
    pageSel.dataset.role="page";

    const modVal = (PPTMAP && PPTMAP[action]) ? (PPTMAP[action].mod || 0) : 0;
    const modSel = makeSelect((KEYCODES.mods||[]).map(m=>({value:m.mask, label:`${m.name} (${toHex2(m.mask)})`})), modVal);
    modSel.dataset.role="mod";

    const kbVal = (PPTMAP && PPTMAP[action]) ? (PPTMAP[action].code || 0) : 0;
    const kbSel = makeSelect(kbOptionList(q), kbVal);
    kbSel.dataset.role="kbkey";

    const conVal = (PPTMAP && PPTMAP[action]) ? (PPTMAP[action].code || 0) : 0;
    const conSel = document.createElement("select");
    conSel.dataset.role="conpreset";
    (KEYCODES.consumer||[]).forEach(c=>{
      const o=document.createElement("option");
      o.value = String(c.mask>>>0);
      o.textContent = `${c.name} (${toHex8(c.mask)})`;
      if((c.mask>>>0) === (conVal>>>0)) o.selected=true;
      conSel.appendChild(o);
    });
    const rawOpt=document.createElement("option");
    rawOpt.value="raw";
    rawOpt.textContent="Raw mask...";
    conSel.appendChild(rawOpt);

    const rawIn=document.createElement("input");
    rawIn.type="text";
    rawIn.placeholder="0x00000000";
    rawIn.value = toHex8(conVal>>>0);
    rawIn.dataset.role="rawmask";

    const btnTest=document.createElement("button");
    btnTest.textContent="Test";
    btnTest.onclick = async ()=>{
      const d = currentRowData(action,row);
      const res = await jpost("/api/ppt/test", { page:d.page, mod:d.mod, code:d.code });
      $("pptMsg").textContent = `test:${action} ${res.ok?"OK":"FAIL"}`;
    };

    const btnApply=document.createElement("button");
    btnApply.textContent="Apply";
    btnApply.onclick = async ()=>{
      const map = collectMapFromUI();
      const res = await jpost("/api/ppt", { save:false, map });
      $("pptMsg").textContent = `apply ${res.ok?"OK":"FAIL"}`;
      await refreshStatus();
    };

    row.appendChild(lbl);
    row.appendChild(pageSel);
    row.appendChild(modSel);

    const keyCell=document.createElement("div");
    keyCell.style.display="flex";
    keyCell.style.gap="8px";
    keyCell.style.alignItems="center";
    keyCell.appendChild(kbSel);
    keyCell.appendChild(conSel);
    keyCell.appendChild(rawIn);
    row.appendChild(keyCell);

    const btnCell1=document.createElement("div");
    btnCell1.appendChild(btnTest);
    const btnCell2=document.createElement("div");
    btnCell2.appendChild(btnApply);
    row.appendChild(btnCell1);
    row.appendChild(btnCell2);

    const refreshVis = ()=>{
      const p = pageSel.value;
      const kbOn = (p==="kb");
      modSel.disabled = !kbOn;
      kbSel.style.display = kbOn ? "" : "none";
      conSel.style.display = kbOn ? "none" : "";
      rawIn.style.display = kbOn ? "none" : "";
      rawIn.style.opacity = (conSel.value==="raw") ? "1" : "0.55";
    };
    pageSel.onchange = refreshVis;
    conSel.onchange = refreshVis;
    refreshVis();

    box.appendChild(row);
  });
}

function collectMapFromUI(){
  const map = {};
  const rows = Array.from(document.querySelectorAll(".pptRow"));
  rows.forEach(r=>{
    const action = r.querySelector(".lbl").textContent.toLowerCase();
    const d = currentRowData(action, r);
    map[action] = { page:d.page, mod:d.mod, code:d.code };
  });
  return map;
}

async function savePptMap(save){
  const map = collectMapFromUI();
  const res = await jpost("/api/ppt", { save: !!save, map });
  $("pptMsg").textContent = `${save?"save":"apply"} ${res.ok?"OK":"FAIL"} (saved=${res.saved}, applied=${res.applied})`;
  await loadPptMap();
  await refreshStatus();
}

// ---------- Tabs ----------
function bindTabs(){
  const tabs = Array.from(document.querySelectorAll(".tab"));
  tabs.forEach(t=>{
    t.onclick = ()=>{
      tabs.forEach(x=>x.classList.remove("on"));
      t.classList.add("on");
      const id=t.dataset.tab;
      Array.from(document.querySelectorAll(".tabBody")).forEach(b=>b.classList.remove("on"));
      $(id).classList.add("on");
    };
  });
}

// ---------- Config UI ----------
function setSlider(id, v){
  const el=$(id);
  if(!el) return;
  el.value = String(v);
  const lab=$(id+"_v");
  if(lab) lab.textContent = String(Number(el.value).toFixed(2));
}
function hookSlider(id, digits=2){
  const el=$(id); const lab=$(id+"_v");
  if(!el || !lab) return;
  const f=()=>lab.textContent = Number(el.value).toFixed(digits);
  el.addEventListener("input", f);
  f();
}

function cfgFromUI(){
  // WiFi
  const wifi = {
    mode: Number($("wifi_mode").value),
    sta: { ssid: $("wifi_sta_ssid").value || "", pass: $("wifi_sta_pass").value || "" },
    ap:  { ssid: $("wifi_ap_ssid").value || "",  pass: $("wifi_ap_pass").value || ""  },
    mdns:{ host: $("wifi_mdns_host").value || "" }
  };

  // E10
  const e10 = {
    dpi_level: Number($("e10_dpi_level").value),
    hard_click_lock: $("e10_hard_click_lock").checked,
    scale_base: [ Number($("e10_scale0").value), Number($("e10_scale1").value), Number($("e10_scale2").value) ],
    accel_gain: [ Number($("e10_gain0").value), Number($("e10_gain1").value), Number($("e10_gain2").value) ],
    accel_threshold: Number($("e10_accel_threshold").value),
    wheel: { threshold_deg: Number($("e10_wheel_th").value), step_max: Number($("e10_wheel_step").value) },
    gesture: { flick_deg: Number($("e10_flick").value), cooldown_ms: Number($("e10_cd").value) },
    scroll_cursor_damp: Number($("e10_damp").value),
    precision: {
      enable: $("e10_prec_en").checked,
      deadzone: Number($("e10_prec_dz").value),
      gain: Number($("e10_prec_gain").value),
      accel: Number($("e10_prec_acc").value),
      max_step: Number($("e10_prec_ms").value),
      smooth: Number($("e10_prec_sm").value)
    }
  };

  return { wifi, e10 };
}

function validateCfgLocal(cfg){
  const errs = [];

  // WiFi lengths (C10 struct sizes 기준)
  const ssidOk = (s)=> (s.length<=32);
  const passOk = (s)=> (s.length<=64);

  if(!ssidOk(cfg.wifi.sta.ssid)) errs.push("STA SSID too long (<=32)");
  if(!passOk(cfg.wifi.sta.pass)) errs.push("STA PASS too long (<=64)");
  if(!ssidOk(cfg.wifi.ap.ssid)) errs.push("AP SSID too long (<=32)");
  if(!passOk(cfg.wifi.ap.pass)) errs.push("AP PASS too long (<=64)");
  if((cfg.wifi.mdns.host||"").length>32) errs.push("mDNS host too long (<=32)");

  // AP pass: empty or >=8 (WPA2)
  if(cfg.wifi.ap.pass && cfg.wifi.ap.pass.length>0 && cfg.wifi.ap.pass.length<8) errs.push("AP PASS must be empty(open) or >=8");

  // numbers sanity
  const e=cfg.e10;
  if(e.dpi_level<0 || e.dpi_level>2) errs.push("dpi_level range 0..2");
  if(e.wheel.step_max<1 || e.wheel.step_max>50) errs.push("wheel.step_max range 1..50");
  if(e.gesture.cooldown_ms<0 || e.gesture.cooldown_ms>10000) errs.push("gesture.cooldown_ms range 0..10000");
  if(e.precision.max_step<1 || e.precision.max_step>200) errs.push("precision.max_step range 1..200");

  return errs;
}

function applyCfgToUI(cfg){
  // WiFi
  $("wifi_mode").value = String(cfg?.wifi?.mode ?? 0);
  $("wifi_sta_ssid").value = cfg?.wifi?.sta?.ssid ?? "";
  $("wifi_sta_pass").value = cfg?.wifi?.sta?.pass ?? "";
  $("wifi_ap_ssid").value  = cfg?.wifi?.ap?.ssid ?? "EliteAirMouse";
  $("wifi_ap_pass").value  = cfg?.wifi?.ap?.pass ?? "12345678";
  $("wifi_mdns_host").value = cfg?.wifi?.mdns?.host ?? "elite-airmouse";

  // E10
  $("e10_dpi_level").value = String(cfg?.e10?.dpi_level ?? 2);
  $("e10_hard_click_lock").checked = !!(cfg?.e10?.hard_click_lock ?? true);

  const sb = cfg?.e10?.scale_base ?? [0.55,0.75,1.0];
  const ag = cfg?.e10?.accel_gain ?? [0.35,0.55,0.85];

  setSlider("e10_scale0", sb[0]); setSlider("e10_scale1", sb[1]); setSlider("e10_scale2", sb[2]);
  setSlider("e10_gain0", ag[0]);  setSlider("e10_gain1", ag[1]);  setSlider("e10_gain2", ag[2]);

  setSlider("e10_accel_threshold", cfg?.e10?.accel_threshold ?? 8.0);

  setSlider("e10_wheel_th", cfg?.e10?.wheel?.threshold_deg ?? 90.0);
  $("e10_wheel_step").value = String(cfg?.e10?.wheel?.step_max ?? 6);

  setSlider("e10_flick", cfg?.e10?.gesture?.flick_deg ?? 200.0);
  $("e10_cd").value = String(cfg?.e10?.gesture?.cooldown_ms ?? 600);

  setSlider("e10_damp", cfg?.e10?.scroll_cursor_damp ?? 0.25);

  $("e10_prec_en").checked = !!(cfg?.e10?.precision?.enable ?? false);
  setSlider("e10_prec_dz", cfg?.e10?.precision?.deadzone ?? 1.2);
  setSlider("e10_prec_gain", cfg?.e10?.precision?.gain ?? 0.65);
  setSlider("e10_prec_acc", cfg?.e10?.precision?.accel ?? 0.25);
  setSlider("e10_prec_sm", cfg?.e10?.precision?.smooth ?? 0.85);
  $("e10_prec_ms").value = String(cfg?.e10?.precision?.max_step ?? 18);
}

async function loadConfig(){
  const raw = await fetch("/api/config", {cache:"no-store"}).then(r=>r.json());
  CFGRAW = raw;
  $("cfgRaw").textContent = pretty(raw);

  // raw에서 wifi/e10 추출
  const cfg = { wifi: raw.wifi || {}, e10: raw.e10 || {} };
  applyCfgToUI(cfg);
  $("cfgMsg").textContent = "loaded";
}

async function cfgApplyOrSave(save){
  const cfg = cfgFromUI();
  const errs = validateCfgLocal(cfg);
  if(errs.length){
    $("cfgMsg").textContent = "LOCAL INVALID: " + errs[0];
    return;
  }
  const url = save ? "/api/config/save" : "/api/config/apply";
  const res = await jpost(url, cfg); // {wifi,e10}만 보내도 C10 patch가 처리
  $("cfgMsg").textContent = `${save?"save":"apply"} ${res.ok?"OK":"FAIL"} (${save?("saved="+res.saved):("applied="+res.applied)})`;
  await refreshStatus();
  if(save) await loadConfig();
}

// ---------- Raw tab tools ----------
async function exportConfig(){
  const r = await fetch("/api/config/export", {cache:"no-store"});
  const blob = await r.blob();
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = "config.json";
  a.click();
  URL.revokeObjectURL(url);
}

async function importConfig(){
  const f = $("fileImport").files[0];
  if(!f){ $("cfgMsg").textContent="select json"; return; }
  const text = await f.text();
  const r = await fetch("/api/config/import", { method:"POST", headers:{"Content-Type":"application/json"}, body:text });
  const j = await r.json();
  $("cfgMsg").textContent = `import ${j.ok?"OK":"FAIL"} (saved=${j.saved}, applied=${j.applied})`;
  await loadConfig();
  await refreshStatus();
}

async function rollbackConfig(){
  if(!confirm("Rollback to .bak ?")) return;
  const r = await fetch("/api/config/rollback", {method:"POST"});
  const j = await r.json();
  $("cfgMsg").textContent = `rollback ${j.ok?"OK":"FAIL"} (applied=${j.applied})`;
  await loadConfig();
  await refreshStatus();
}

// ---------- Control / SafeBoot / Factory ----------
async function applyRuntimeControl(){
  const body = {
    ppt_mode: $("ctlPptMode").checked,
    precision_mode: $("ctlPrecMode").checked,
    dpi_level: Number($("ctlDpi").value)
  };
  const r = await jpost("/api/control", body);
  $("ctlMsg").textContent = r.ok ? "OK" : "FAIL";
  await refreshStatus();
}

async function safeGet(){
  const r = await jget("/api/safeboot");
  $("safeMsg").textContent = r.ok ? `safe=${r.safe_mode?1:0} fail=${r.fail_count} pending=${r.pending?1:0}` : "FAIL";
}
async function safeExit(){
  const r = await jpost("/api/safeboot", {exit:true});
  $("safeMsg").textContent = r.ok ? "exit OK (reboot recommended)" : "exit FAIL";
}
async function factoryReset(){
  if(!confirm("Factory Reset? (will reboot)")) return;
  const r = await fetch("/api/factory_reset", {method:"POST"});
  const j = await r.json();
  $("safeMsg").textContent = j.ok ? "factory reset OK (rebooting...)" : "factory reset FAIL";
}
async function reboot(){
  if(!confirm("Reboot device?")) return;
  await fetch("/api/reboot", {method:"POST"});
  $("safeMsg").textContent = "rebooting...";
}

// ---------- OTA ----------
async function otaUpload(){
  const f = $("fileOta").files[0];
  if(!f){ $("sysMsg").textContent="select .bin"; return; }
  $("otaBar").style.width="0%";
  $("sysMsg").textContent="uploading...";
  $("otaMsg").textContent="-";

  const xhr = new XMLHttpRequest();
  xhr.open("POST","/api/ota",true);

  xhr.upload.onprogress = (e)=>{
    if(e.lengthComputable){
      const p = (e.loaded/e.total)*100;
      $("otaBar").style.width = p.toFixed(1)+"%";
      $("sysMsg").textContent = `ota ${p.toFixed(1)}% (${e.loaded}/${e.total})`;
    }
  };
  xhr.onload = ()=>{
    try{
      const j = JSON.parse(xhr.responseText||"{}");
      $("sysMsg").textContent = pretty(j);
      $("otaMsg").textContent = j.ok ? "OK (rebooting...)" : ("FAIL: "+(j.err||""));
    }catch(e){
      $("sysMsg").textContent="ota parse error";
    }
  };
  xhr.onerror = ()=>{ $("sysMsg").textContent="ota failed"; };

  const form = new FormData();
  form.append("update", f, f.name);
  xhr.send(form);
}
async function otaStatus(){
  const j = await jget("/api/ota/status");
  $("sysMsg").textContent = pretty(j);
  $("otaMsg").textContent = j.in_progress ? "uploading..." : (j.ok ? "ok" : (j.err||"fail"));
}

// ---------- bind ----------
function bind(){
  $("btnRefresh").onclick = refreshStatus;
  $("autoPoll").onchange = (e)=>setAutoPoll(e.target.checked);

  // runtime control
  $("btnCtlApply").onclick = applyRuntimeControl;

  // safe/factory/reboot
  $("btnSafeGet").onclick = safeGet;
  $("btnSafeExit").onclick = safeExit;
  $("btnFactory").onclick = factoryReset;
  $("btnReboot").onclick = reboot;

  // ppt
  $("btnReloadKeycodes").onclick = loadKeycodes;
  $("keySearch").oninput = renderPptTable;

  $("btnLoadKeys").onclick = loadPptMap;
  $("btnApplyKeys").onclick = ()=>savePptMap(false);
  $("btnSaveKeys").onclick  = ()=>savePptMap(true);

  // config
  $("btnCfgLoad").onclick = loadConfig;
  $("btnCfgApply").onclick = ()=>cfgApplyOrSave(false);
  $("btnCfgSave").onclick  = ()=>cfgApplyOrSave(true);

  // raw tab
  $("btnCfgExport").onclick = exportConfig;
  $("btnCfgImport").onclick = importConfig;
  $("btnCfgRollback").onclick = rollbackConfig;

  // ota
  $("btnOta").onclick = otaUpload;
  $("btnOtaStatus").onclick = otaStatus;

  bindTabs();

  // hook sliders
  ["e10_scale0","e10_scale1","e10_scale2","e10_gain0","e10_gain1","e10_gain2",
   "e10_accel_threshold","e10_wheel_th","e10_flick","e10_damp",
   "e10_prec_dz","e10_prec_gain","e10_prec_acc","e10_prec_sm"
  ].forEach(id=>hookSlider(id, 2));
}

// ---------- boot ----------
(async function(){
  bind();
  await refreshStatus();
  setAutoPoll(true);
  await loadKeycodes();
  await loadPptMap();
  await loadConfig();
  await safeGet();
})();