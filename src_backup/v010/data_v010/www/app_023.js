// =======================================================
// File: src/v023/data_v023/www/app_023.js
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

    banner(h.state||0);

    $("kGyro").textContent = fmt(gyro.rms||0,2);
    $("kCursor").textContent = fmt(e.cursor_rms||0,2);
    $("kSpike").textContent = String(an.spike_count_10s ?? "-");
    $("kFail").textContent = String(an.consecutive_fail ?? "-");
    $("kI2c").textContent = `${i2c.recover_count ?? "-"} / ${i2c.recover_last_ok ? "OK":"FAIL"}`;
    $("kBle").textContent = (e.ble_connected ? "ON":"OFF");

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

  // allow hex search: "0x3e" or "3e"
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
  const list = filterKbList(query).slice(0, 400); // UI 부담 줄이기
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

    // page
    const pageVal = (PPTMAP && PPTMAP[action] && PPTMAP[action].page) ? PPTMAP[action].page : "kb";
    const pageSel = makeSelect([
      {value:"kb", label:"Keyboard(0x07)"},
      {value:"consumer", label:"Consumer(Media)"}
    ], pageVal);
    pageSel.dataset.role="page";

    // mod
    const modVal = (PPTMAP && PPTMAP[action]) ? (PPTMAP[action].mod || 0) : 0;
    const modSel = makeSelect((KEYCODES.mods||[]).map(m=>({value:m.mask, label:`${m.name} (${toHex2(m.mask)})`})), modVal);
    modSel.dataset.role="mod";

    // kb key select
    const kbVal = (PPTMAP && PPTMAP[action]) ? (PPTMAP[action].code || 0) : 0;
    const kbSel = makeSelect(kbOptionList(q), kbVal);
    kbSel.dataset.role="kbkey";

    // consumer preset + raw
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

    // buttons
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
      // apply only: send full map without saving
      const map = collectMapFromUI();
      const res = await jpost("/api/ppt", { save:false, map });
      $("pptMsg").textContent = `apply ${res.ok?"OK":"FAIL"}`;
      await refreshStatus();
    };

    // row build
    row.appendChild(lbl);
    row.appendChild(pageSel);
    row.appendChild(modSel);

    // key area: kbSel or consumer controls
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

    // show/hide depending on page
    const refreshVis = ()=>{
      const p = pageSel.value;
      const kbOn = (p==="kb");
      modSel.disabled = !kbOn;
      kbSel.style.display = kbOn ? "" : "none";
      conSel.style.display = kbOn ? "none" : "";
      rawIn.style.display = kbOn ? "none" : "";
    };
    pageSel.onchange = refreshVis;
    conSel.onchange = ()=>{
      rawIn.style.opacity = (conSel.value==="raw") ? "1" : "0.55";
    };
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

// ---------- Config / OTA quick ----------
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
}
async function rollbackConfig(){
  if(!confirm("Rollback to .bak ?")) return;
  const r = await fetch("/api/config/rollback", {method:"POST"});
  const j = await r.json();
  $("sysMsg").textContent = pretty(j);
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
})();

