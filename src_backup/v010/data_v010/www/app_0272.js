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
  const t = await r.text();
  try{ return JSON.parse(t); }catch(e){ return {ok:false, err:"bad_json", raw:t}; }
}
function pretty(obj){ return JSON.stringify(obj, null, 2); }

let pollTimer=null;
let KEYCODES = { mods:[], kb:[], consumer:[] };
let PPTMAP = null;
let CFGOBJ = null;

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

    banner(h.state||0);

    $("kGyro").textContent = fmt(gyro.rms||0,2);
    $("kCursor").textContent = fmt(e.cursor_rms||0,2);
    $("kSpike").textContent = String(an.spike_count_10s ?? "-");
    $("kFail").textContent = String(an.consecutive_fail ?? "-");
    $("kI2c").textContent = `${i2c.recover_count ?? "-"} / ${i2c.recover_last_ok ? "OK":"FAIL"}`;
    $("kBle").textContent = (e.ble_connected ? "ON":"OFF");
    $("kFsm").textContent = `${e.fsm_state ?? "-"} / ${e.fsm_sub ?? "-"}`;
    $("kHeap").textContent = String(s.heap_free ?? "-");
    $("kIp").textContent = String(net.ip ?? "-");

    const ul = $("alertList");
    ul.innerHTML = "";

    const score = (h.score ?? 1000);
    if(score < 620) addAlert(ul, `Health DEGRADED (score=${score})`);
    else if(score < 820) addAlert(ul, `Health WARN (score=${score})`);

    if((gyro.rms||0) > 8) addAlert(ul, `High gyro noise RMS=${fmt(gyro.rms,2)} deg/s`);
    if((e.cursor_rms||0) > 6) addAlert(ul, `High cursor noise RMS=${fmt(e.cursor_rms,2)} px`);
    if((an.spike_count_10s||0) >= 3) addAlert(ul, `Spike detected: ${an.spike_count_10s} in 10s`);
    if((an.consecutive_fail||0) >= 5) addAlert(ul, `Consecutive fail: ${an.consecutive_fail}`);
    if((i2c.recover_count||0) >= 2) addAlert(ul, `I2C recover happened: ${i2c.recover_count}`);
    if(i2c.recover_last_ok === false) addAlert(ul, `I2C recover last FAILED`);

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

// ---------- PPT editor ----------
const PPT_ACTIONS = ["start","exit","next","prev","black","laser"];

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

function kbOptionList(query){
  const list = filterKbList(query).slice(0, 400);
  return list.map(k=>({ value:k.code, label:`${k.name} (${toHex2(k.code)})` }));
}

function toHex2(n){ return "0x" + (n>>>0).toString(16).toUpperCase().padStart(2,"0"); }
function toHex8(n){ return "0x" + (n>>>0).toString(16).toUpperCase().padStart(8,"0"); }

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

  const page = pageSel.value;
  let mod = parseInt(modSel.value||"0",10);
  let code = 0;

  if(page==="consumer"){
    const preset = conSel.value;
    if(preset==="raw"){
      const txt = (rawIn.value||"0").trim().toLowerCase();
      code = txt.startsWith("0x") ? (parseInt(txt,16)>>>0) : (parseInt(txt,10)>>>0);
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
    const pageSel = makeSelect([{value:"kb",label:"Keyboard(0x07)"},{value:"consumer",label:"Consumer(Media)"}], pageVal);
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
    };
    pageSel.onchange = refreshVis;
    conSel.onchange = ()=>{ rawIn.style.opacity = (conSel.value==="raw") ? "1" : "0.55"; };
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
  $("pptMsg").textContent = `${save?"save":"apply"} ${res.ok?"OK":"FAIL"} (applied=${res.applied})`;
  await loadPptMap();
}

// ---------- Config Editor ----------
function bindRange(id, outId, fmtFn=(v)=>String(v)){
  const el=$(id), out=$(outId);
  const sync=()=>{ out.textContent = fmtFn(el.value); };
  el.oninput=sync; sync();
}

function validateWiFiFields(obj){
  const apPass = obj?.wifi?.ap?.pass || "";
  const staPass= obj?.wifi?.sta?.pass || "";
  const mdns = obj?.wifi?.mdns?.host || "";
  const bad = [];
  if(apPass && apPass.length < 8) bad.push("AP PASS must be >= 8 chars");
  if(staPass && staPass.length < 8) bad.push("STA PASS must be >= 8 chars");
  if(mdns.includes(" ") || mdns.includes("/")) bad.push("mDNS host contains invalid char");
  return bad;
}

function validateE10Fields(obj){
  const e=obj?.e10||{};
  const bad=[];
  const dpi=e.dpi_level;
  if(dpi<1||dpi>3) bad.push("DPI must be 1..3");
  const wheel=e.wheel?.step_max;
  if(wheel<1||wheel>12) bad.push("Wheel step max must be 1..12");
  const cd=e.gesture?.cooldown_ms;
  if(cd<50||cd>3000) bad.push("Gesture cooldown must be 50..3000");
  const sm=e.precision?.smooth;
  if(sm<0||sm>0.99) bad.push("Precision smooth must be 0..0.99");
  return bad;
}

function uiToCfgPatch(){
  const wifiMode=parseInt($("wifiMode").value,10);
  const staSsid=$("staSsid").value||"";
  const staPass=$("staPass").value||"";
  const apSsid=$("apSsid").value||"";
  const apPass=$("apPass").value||"";
  const mdns=$("mdnsHost").value||"";

  const e10={
    dpi_level: parseInt($("dpi").value,10),
    accel_threshold: parseFloat($("accTh").value),
    wheel: { threshold_deg: parseFloat($("wheelTh").value), step_max: parseInt($("wheelStep").value,10) },
    gesture: { flick_deg: parseFloat($("flick").value), cooldown_ms: parseInt($("cool").value,10) },
    scroll_cursor_damp: parseFloat($("damp").value),
    precision: {
      enable: $("precEnable").checked,
      deadzone: parseFloat($("precDead").value),
      gain: parseFloat($("precGain").value),
      smooth: parseFloat($("precSmooth").value),
      entry_ms: parseInt($("precEntryMs").value,10),
      exit_ms: parseInt($("precExitMs").value,10),
      entry_still_deg: parseFloat($("precStill").value),
      exit_move_deg: parseFloat($("precMove").value),
      profile: 1
    }
  };

  return {
    wifi:{
      mode: wifiMode,
      sta:{ ssid: staSsid, pass: staPass },
      ap:{ ssid: apSsid, pass: apPass },
      mdns:{ host: mdns }
    },
    e10
  };
}

function fillCfgUI(cfg){
  // wifi
  $("wifiMode").value = String(cfg?.wifi?.mode ?? 0);
  $("staSsid").value = cfg?.wifi?.sta?.ssid ?? "";
  $("staPass").value = cfg?.wifi?.sta?.pass ?? "";
  $("apSsid").value  = cfg?.wifi?.ap?.ssid ?? "";
  $("apPass").value  = cfg?.wifi?.ap?.pass ?? "";
  $("mdnsHost").value= cfg?.wifi?.mdns?.host ?? "";

  // e10
  $("dpi").value = String(cfg?.e10?.dpi_level ?? 2);
  $("accTh").value = String(cfg?.e10?.accel_threshold ?? 8.0);
  $("wheelTh").value = String(cfg?.e10?.wheel?.threshold_deg ?? 90.0);
  $("wheelStep").value = String(cfg?.e10?.wheel?.step_max ?? 6);
  $("flick").value = String(cfg?.e10?.gesture?.flick_deg ?? 200.0);
  $("cool").value = String(cfg?.e10?.gesture?.cooldown_ms ?? 600);
  $("damp").value = String(cfg?.e10?.scroll_cursor_damp ?? 0.25);

  $("precEnable").checked = !!(cfg?.e10?.precision?.enable ?? false);
  $("precDead").value = String(cfg?.e10?.precision?.deadzone ?? 1.2);
  $("precGain").value = String(cfg?.e10?.precision?.gain ?? 0.65);
  $("precSmooth").value = String(cfg?.e10?.precision?.smooth ?? 0.85);
  $("precEntryMs").value = String(cfg?.e10?.precision?.entry_ms ?? 180);
  $("precExitMs").value = String(cfg?.e10?.precision?.exit_ms ?? 160);
  $("precStill").value = String(cfg?.e10?.precision?.entry_still_deg ?? 2.2);
  $("precMove").value = String(cfg?.e10?.precision?.exit_move_deg ?? 7.5);

  // bind pill text
  bindRange("dpi","dpiV",(v)=>v);
  bindRange("accTh","accThV",(v)=>Number(v).toFixed(1));
  bindRange("wheelTh","wheelThV",(v)=>Number(v).toFixed(0));
  bindRange("wheelStep","wheelStepV",(v)=>v);
  bindRange("flick","flickV",(v)=>Number(v).toFixed(0));
  bindRange("cool","coolV",(v)=>v);
  bindRange("damp","dampV",(v)=>Number(v).toFixed(2));

  bindRange("precDead","precDeadV",(v)=>Number(v).toFixed(1));
  bindRange("precGain","precGainV",(v)=>Number(v).toFixed(2));
  bindRange("precSmooth","precSmoothV",(v)=>Number(v).toFixed(2));
  bindRange("precEntryMs","precEntryMsV",(v)=>v);
  bindRange("precExitMs","precExitMsV",(v)=>v);
  bindRange("precStill","precStillV",(v)=>Number(v).toFixed(1));
  bindRange("precMove","precMoveV",(v)=>Number(v).toFixed(1));
}

async function cfgLoad(){
  const cfg = await jget("/api/config");
  CFGOBJ = cfg;
  $("cfgBox").textContent = pretty(cfg);
  fillCfgUI(cfg);
  $("cfgMsg").textContent="loaded";
}

async function cfgApplyOnly(){
  const patch = uiToCfgPatch();
  const bad = [...validateWiFiFields(patch), ...validateE10Fields(patch)];
  if(bad.length){
    $("cfgMsg").textContent = "INVALID: " + bad.join(" | ");
    return;
  }
  const res = await jpost("/api/config/apply", patch);
  $("cfgMsg").textContent = pretty(res);
  await refreshStatus();
}

async function cfgSave(){
  const patch = uiToCfgPatch();
  const bad = [...validateWiFiFields(patch), ...validateE10Fields(patch)];
  if(bad.length){
    $("cfgMsg").textContent = "INVALID: " + bad.join(" | ");
    return;
  }
  // 저장은 full JSON이 필요하므로: 현재 config 로드 후 merge
  const base = await jget("/api/config");
  const merged = JSON.parse(JSON.stringify(base));
  merged.wifi = patch.wifi;
  merged.e10 = { ...(merged.e10||{}), ...(patch.e10||{}) };
  merged.e10.wheel = patch.e10.wheel;
  merged.e10.gesture = patch.e10.gesture;
  merged.e10.precision = patch.e10.precision;

  const res = await jpost("/api/config/save", merged);
  $("cfgMsg").textContent = pretty(res);
  await cfgLoad();
  await refreshStatus();
}

// ---------- Ops ----------
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
  if(!f){ $("sysMsg").textContent="select json"; return; }
  const text = await f.text();
  const r = await fetch("/api/config/import", { method:"POST", headers:{"Content-Type":"application/json"}, body:text });
  const j = await r.json();
  $("sysMsg").textContent = pretty(j);
  await refreshStatus();
  await cfgLoad();
}
async function rollbackConfig(){
  if(!confirm("Rollback to .bak ?")) return;
  const r = await fetch("/api/config/rollback", {method:"POST"});
  const j = await r.json();
  $("sysMsg").textContent = pretty(j);
  await cfgLoad();
}
async function reboot(){
  if(!confirm("Reboot device?")) return;
  await fetch("/api/reboot", {method:"POST"});
  $("sysMsg").textContent = "rebooting...";
}

async function otaUpload(){
  const f = $("fileOta").files[0];
  if(!f){ $("sysMsg").textContent="select .bin"; return; }
  $("otaBar").style.width="0%";
  $("sysMsg").textContent="uploading...";

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
    try{ $("sysMsg").textContent = pretty(JSON.parse(xhr.responseText||"{}")); }
    catch(e){ $("sysMsg").textContent="ota parse error"; }
  };
  xhr.onerror = ()=>{ $("sysMsg").textContent="ota failed"; };

  const form = new FormData();
  form.append("update", f, f.name);
  xhr.send(form);
}

// ---------- bind ----------
function bind(){
  $("btnRefresh").onclick = refreshStatus;
  $("autoPoll").onchange = (e)=>setAutoPoll(e.target.checked);

  $("btnReloadKeycodes").onclick = loadKeycodes;
  $("keySearch").oninput = renderPptTable;

  $("btnLoadKeys").onclick = loadPptMap;
  $("btnApplyKeys").onclick = ()=>savePptMap(false);
  $("btnSaveKeys").onclick  = ()=>savePptMap(true);

  $("btnCfgLoad").onclick = cfgLoad;
  $("btnCfgApply").onclick = cfgApplyOnly;
  $("btnCfgSave").onclick = cfgSave;

  $("btnExport").onclick = exportConfig;
  $("btnImport").onclick = importConfig;
  $("btnRollback").onclick = rollbackConfig;
  $("btnReboot").onclick = reboot;
  $("btnOta").onclick = otaUpload;
}

(async function(){
  bind();
  await refreshStatus();
  setAutoPoll(true);
  await loadKeycodes();
  await loadPptMap();
  await cfgLoad();
})();
