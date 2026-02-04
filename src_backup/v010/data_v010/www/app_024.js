// =======================================================
// File: src/v024/data_v024/www/app_024.js
// =======================================================
const $ = (id)=>document.getElementById(id);

async function jget(url){
  const r = await fetch(url, {cache:"no-store"});
  let j=null;
  try{ j = await r.json(); }catch(e){ j = null; }
  if(!r.ok){
    const msg = (j && (j.err||j.error)) ? (j.err||j.error) : `HTTP_${r.status}`;
    throw new Error(`${msg}`);
  }
  return j;
}

async function jpost(url, body){
  const r = await fetch(url, {
    method:"POST",
    headers: {"Content-Type":"application/json"},
    body: JSON.stringify(body||{})
  });
  let j=null;
  try{ j = await r.json(); }catch(e){ j = null; }
  if(!r.ok){
    const msg = (j && (j.err||j.error)) ? (j.err||j.error) : `HTTP_${r.status}`;
    const e = new Error(`${msg}`);
    e.httpStatus = r.status;
    e.payload = j;
    throw e;
  }
  return j;
}

function pretty(obj){ return JSON.stringify(obj, null, 2); }

let pollTimer=null;
let KEYCODES = { mods:[], kb:[], consumer:[], kb_meta:{} };
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

function fmt(n, d=2){
  if(n===null || n===undefined) return "-";
  if(typeof n==="number") return n.toFixed(d);
  return String(n);
}

function clearAlerts(){
  const ul = $("alertList");
  ul.innerHTML = "";
}

function addAlertLi(level, text){
  const ul = $("alertList");
  const li = document.createElement("li");
  li.className = `al ${level||"info"}`;
  li.textContent = text;
  ul.appendChild(li);
}

function showMsg(id, text){
  const el = $(id);
  if(el) el.textContent = text;
}

function isOtaLockedError(e){
  return (e && (e.httpStatus===423 || String(e.message||"").includes("ota_locked")));
}

// ---------- STATUS ----------
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

    $("kGyro").textContent   = fmt(gyro.rms||0,2);
    $("kCursor").textContent = fmt(e.cursor_rms||0,2);
    $("kSpike").textContent  = String(an.spike_count_10s ?? "-");
    $("kFail").textContent   = String(an.consecutive_fail ?? "-");
    $("kI2c").textContent    = `${i2c.recover_count ?? "-"} / ${i2c.recover_last_ok ? "OK":"FAIL"}`;
    $("kBle").textContent    = (e.ble_connected ? "ON":"OFF");

    // network mini
    const net = (s.net||{});
    $("kNet").textContent = `${net.mode||"-"} / ${(net.ip||"-")}`;
    $("kMdns").textContent = (net.mdns||"-");

    // OTA mini
    const ota = (s.ota||{});
    $("kOta").textContent = ota.in_progress ? "IN PROGRESS" : (ota.ok ? "OK" : "-");

    // Alerts: prefer server alerts[]
    clearAlerts();
    if(Array.isArray(s.alerts) && s.alerts.length){
      s.alerts.forEach(a=>{
        const level = (a.level||"info").toLowerCase(); // ok|warn|crit
        const code  = a.code ? `[${a.code}] ` : "";
        const hint  = a.hint ? ` / ${a.hint}` : "";
        addAlertLi(level, `${code}${a.msg||"-"}${hint}`);
      });
    }else{
      // fallback (v023 style) if alerts not provided
      const score = (h.score||1000);
      if(score < 620) addAlertLi("crit", `Health DEGRADED (score=${score})`);
      else if(score < 820) addAlertLi("warn", `Health WARN (score=${score})`);
      if((gyro.rms||0) > 8) addAlertLi("warn", `High gyro noise RMS=${fmt(gyro.rms,2)} deg/s`);
      if((e.cursor_rms||0) > 6) addAlertLi("warn", `High cursor noise RMS=${fmt(e.cursor_rms,2)} px`);
      if((an.spike_count_10s||0) >= 3) addAlertLi("warn", `Spike detected: ${an.spike_count_10s} in 10s`);
      if((an.consecutive_fail||0) >= 5) addAlertLi("crit", `Consecutive fail: ${an.consecutive_fail}`);
      if((i2c.recover_count||0) >= 2) addAlertLi("warn", `I2C recover happened: ${i2c.recover_count}`);
      if(i2c.recover_last_ok === false) addAlertLi("crit", `I2C recover last FAILED`);
      if($("alertList").children.length===0) addAlertLi("ok","No alerts.");
    }
  }catch(e){
    $("statusBox").textContent = "status error: " + e;
    clearAlerts();
    addAlertLi("crit", "Status fetch failed: " + e);
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

function toHex2(n){ return "0x" + (n>>>0).toString(16).toUpperCase().padStart(2,"0"); }
function toHex8(n){ return "0x" + (n>>>0).toString(16).toUpperCase().padStart(8,"0"); }

function kbOptionList(query){
  const list = filterKbList(query).slice(0, 500);
  return list.map(k=>{
    const tag = k.is_modifier_usage ? " [MOD]" : "";
    return { value:k.code, label:`${k.name} (${toHex2(k.code)})${tag}` };
  });
}

async function loadPptMap(){
  const r = await jget("/api/ppt");
  PPTMAP = r.map || {};
  renderPptTable();
  showMsg("pptMsg","loaded");
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
      if(txt.startsWith("0x")) code = (parseInt(txt,16)>>>0);
      else code = (parseInt(txt,10)>>>0);
    }else{
      code = (parseInt(preset,10)>>>0);
    }
    mod = 0;
  }else{
    code = (parseInt(kbSel.value||"0",10)>>>0);
    // modifier usage guard: 0xE0~0xE7는 금지(서버 policy)
    if(code>=0xE0 && code<=0xE7){
      // 자동으로 None으로 내리거나, UI 경고만 띄움(여기선 경고)
      // 실제 전송은 막지 않되, 저장/적용 때 경고 처리
    }
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
      o.value = String((c.mask>>>0));
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

    // per-row warning
    const warn = document.createElement("div");
    warn.className="pptWarn";
    warn.textContent = "";

    // buttons
    const btnTest=document.createElement("button");
    btnTest.textContent="Test";
    btnTest.onclick = async ()=>{
      try{
        const d = currentRowData(action,row);
        const res = await jpost("/api/ppt/test", { page:d.page, mod:d.mod, code:d.code });
        showMsg("pptMsg", `test:${action} ${res.ok?"OK":"FAIL"}`);
      }catch(e){
        showMsg("pptMsg", `test:${action} FAIL (${e.message})`);
      }
    };

    const btnApply=document.createElement("button");
    btnApply.textContent="Apply";
    btnApply.onclick = async ()=>{
      try{
        const map = collectMapFromUI();
        const warnText = validateMap(map);
        if(warnText) showMsg("pptMsg", warnText);

        const res = await jpost("/api/ppt", { save:false, map });
        showMsg("pptMsg", `apply ${res.ok?"OK":"FAIL"} (applied=${res.applied})`);
        await refreshStatus();
      }catch(e){
        showMsg("pptMsg", isOtaLockedError(e) ? "apply blocked: OTA in progress" : `apply FAIL (${e.message})`);
      }
    };

    // row build
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

    row.appendChild(warn);

    const refreshVis = ()=>{
      const p = pageSel.value;
      const kbOn = (p==="kb");
      modSel.disabled = !kbOn;
      kbSel.style.display = kbOn ? "" : "none";
      conSel.style.display = kbOn ? "none" : "";
      rawIn.style.display = kbOn ? "none" : "";

      const d = currentRowData(action,row);
      warn.textContent = (d.page==="kb" && d.code>=0xE0 && d.code<=0xE7)
        ? "주의: 0xE0~0xE7은 modifier usage입니다. mod-mask로 설정하세요."
        : "";
    };
    pageSel.onchange = refreshVis;
    kbSel.onchange = refreshVis;
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

function validateMap(map){
  // only warn; server will accept but E10 policy prefers mod-mask
  for(const k in map){
    const v = map[k];
    if(v.page==="kb" && v.code>=0xE0 && v.code<=0xE7){
      return "경고: kb code에 modifier usage(0xE0~0xE7)가 포함됨. mod-mask로 바꾸는 것을 권장.";
    }
  }
  return "";
}

async function savePptMap(save){
  try{
    const map = collectMapFromUI();
    const warnText = validateMap(map);
    if(warnText) showMsg("pptMsg", warnText);

    const res = await jpost("/api/ppt", { save: !!save, map });
    showMsg("pptMsg", `${save?"save":"apply"} ${res.ok?"OK":"FAIL"} (applied=${res.applied})`);
    await loadPptMap();
  }catch(e){
    showMsg("pptMsg", isOtaLockedError(e) ? `${save?"save":"apply"} blocked: OTA in progress` : `${save?"save":"apply"} FAIL (${e.message})`);
  }
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
  try{
    const f = $("fileImport").files[0];
    if(!f){ showMsg("sysMsg","select json"); return; }
    const text = await f.text();
    const r = await fetch("/api/config/import", { method:"POST", headers:{"Content-Type":"application/json"}, body:text });
    const j = await r.json().catch(()=>({ok:false, err:`HTTP_${r.status}`}));
    if(!r.ok){
      showMsg("sysMsg", pretty(j));
      if(r.status===423) showMsg("sysMsg", "blocked: OTA in progress");
      return;
    }
    showMsg("sysMsg", pretty(j));
    await refreshStatus();
  }catch(e){
    showMsg("sysMsg", "import error: " + e.message);
  }
}

async function rollbackConfig(){
  try{
    if(!confirm("Rollback to .bak ?")) return;
    const r = await fetch("/api/config/rollback", {method:"POST"});
    const j = await r.json().catch(()=>({ok:false, err:`HTTP_${r.status}`}));
    if(!r.ok){
      showMsg("sysMsg", pretty(j));
      if(r.status===423) showMsg("sysMsg", "blocked: OTA in progress");
      return;
    }
    showMsg("sysMsg", pretty(j));
  }catch(e){
    showMsg("sysMsg", "rollback error: " + e.message);
  }
}

async function reboot(){
  if(!confirm("Reboot device?")) return;
  await fetch("/api/reboot", {method:"POST"});
  showMsg("sysMsg","rebooting...");
}

async function otaStatus(){
  try{
    const s = await jget("/api/ota/status");
    showMsg("sysMsg", pretty(s));
  }catch(e){
    showMsg("sysMsg", "ota status error: " + e.message);
  }
}

async function otaCancel(){
  if(!confirm("Cancel OTA?")) return;
  try{
    const r = await fetch("/api/ota/cancel", {method:"POST"});
    const j = await r.json().catch(()=>({ok:false, err:`HTTP_${r.status}`}));
    showMsg("sysMsg", pretty(j));
    await refreshStatus();
  }catch(e){
    showMsg("sysMsg", "ota cancel error: " + e.message);
  }
}

async function otaReboot(){
  if(!confirm("Reboot (after OTA OK)?")) return;
  await fetch("/api/ota/reboot", {method:"POST"});
  showMsg("sysMsg","rebooting...");
}

async function otaUpload(){
  const f = $("fileOta").files[0];
  if(!f){ showMsg("sysMsg","select .bin"); return; }
  $("otaBar").style.width="0%";
  showMsg("sysMsg","uploading...");

  const xhr = new XMLHttpRequest();
  xhr.open("POST","/api/ota",true);

  xhr.upload.onprogress = (e)=>{
    if(e.lengthComputable){
      const p = (e.loaded/e.total)*100;
      $("otaBar").style.width = p.toFixed(1)+"%";
      showMsg("sysMsg", `ota ${p.toFixed(1)}% (${e.loaded}/${e.total})`);
    }
  };
  xhr.onload = async ()=>{
    try{
      const j = JSON.parse(xhr.responseText||"{}");
      showMsg("sysMsg", pretty(j));
      await refreshStatus();
      // OTA 성공 시 자동 재부팅은 서버 정책에 따라 다름(024는 /api/ota는 status만 반환)
      if(j.ok) showMsg("sysMsg", "OTA uploaded. Press 'OTA Reboot' to restart.");
    }catch(e){
      showMsg("sysMsg","ota parse error");
    }
  };
  xhr.onerror = ()=>{ showMsg("sysMsg","ota failed"); };

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

  $("btnOtaStatus").onclick = otaStatus;
  $("btnOtaCancel").onclick = otaCancel;
  $("btnOtaReboot").onclick = otaReboot;
  $("btnOta").onclick = otaUpload;
}

(async function(){
  bind();
  await refreshStatus();
  setAutoPoll(true);
  await loadKeycodes();
  await loadPptMap();
})();

