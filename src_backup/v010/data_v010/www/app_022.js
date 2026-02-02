// =======================================================
// File: src/v022/data_v022/www/app_022.js
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

let pollTimer = null;

async function refreshStatus(){
  try{
    const s = await jget("/api/status");
    $("statusBox").textContent = pretty(s);
  }catch(e){
    $("statusBox").textContent = "status error: " + e;
  }
}

function setAutoPoll(on){
  if(pollTimer){ clearInterval(pollTimer); pollTimer=null; }
  if(on){
    pollTimer = setInterval(refreshStatus, 1000);
  }
}

async function loadConfig(){
  const c = await jget("/api/config");
  $("cfgBox").value = pretty(c);
}

async function saveConfig(){
  try{
    const raw = $("cfgBox").value.trim();
    const obj = JSON.parse(raw);
    const res = await fetch("/api/config", {
      method:"POST",
      headers:{"Content-Type":"application/json"},
      body: JSON.stringify(obj)
    });
    const j = await res.json();
    $("cfgMsg").textContent = pretty(j);
  }catch(e){
    $("cfgMsg").textContent = "save error: " + e;
  }
}

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
  // server accepts raw JSON string body
  const r = await fetch("/api/config/import", {
    method:"POST",
    headers:{"Content-Type":"application/json"},
    body: text
  });
  const j = await r.json();
  $("cfgMsg").textContent = pretty(j);
  await loadConfig();
}

async function rollbackConfig(){
  const ok = confirm("Rollback to .bak ? (restore last good config)");
  if(!ok) return;
  const r = await fetch("/api/config/rollback", {method:"POST"});
  const j = await r.json();
  $("cfgMsg").textContent = pretty(j);
  await loadConfig();
}

async function reboot(){
  const ok = confirm("Reboot device?");
  if(!ok) return;
  await fetch("/api/reboot", {method:"POST"});
  $("cfgMsg").textContent = "rebooting...";
}

async function controlPpt(on){
  const j = await jpost("/api/control", {ppt_mode: !!on});
  $("cfgMsg").textContent = pretty(j);
  await refreshStatus();
}
async function controlDpi(dpi){
  const j = await jpost("/api/control", {dpi_level: dpi});
  $("cfgMsg").textContent = pretty(j);
  await refreshStatus();
}
async function controlPrecision(on){
  const j = await jpost("/api/control", {precision_mode: !!on});
  $("cfgMsg").textContent = pretty(j);
  await refreshStatus();
}

async function loadKeycodes(){
  const j = await jget("/api/keycodes");
  const keys = j.keys || [];
  const mods = j.mods || [];
  window.__keycodes = {keys, mods};
  renderKeycodes();
}

function renderKeycodes(){
  const q = ($("keySearch").value || "").toLowerCase().trim();
  const {keys=[], mods=[]} = window.__keycodes || {};
  const out = {
    mods,
    keys: keys.filter(k => !q || (k.name||"").toLowerCase().includes(q)).slice(0, 200)
  };
  $("keysBox").textContent = pretty(out) + (keys.length>200 ? "\n\n(showing first 200)" : "");
}

async function otaUpload(){
  const f = $("fileOta").files[0];
  if(!f){ $("otaMsg").textContent="select .bin"; return; }

  $("otaMsg").textContent = "uploading...";
  $("otaBar").style.width = "0%";

  const xhr = new XMLHttpRequest();
  xhr.open("POST", "/api/ota", true);

  xhr.upload.onprogress = (e)=>{
    if(e.lengthComputable){
      const p = (e.loaded / e.total) * 100;
      $("otaBar").style.width = p.toFixed(1) + "%";
      $("otaMsg").textContent = `upload ${p.toFixed(1)}% (${e.loaded}/${e.total})`;
    }
  };

  xhr.onload = ()=>{
    try{
      const j = JSON.parse(xhr.responseText || "{}");
      $("otaMsg").textContent = pretty(j) + "\n(If ok==true, device will reboot automatically.)";
    }catch(e){
      $("otaMsg").textContent = "ota response parse error";
    }
  };

  xhr.onerror = ()=>{ $("otaMsg").textContent = "ota upload failed"; };

  const form = new FormData();
  form.append("update", f, f.name);
  xhr.send(form);
}

function bind(){
  $("btnRefresh").onclick = refreshStatus;
  $("autoPoll").onchange = (e)=> setAutoPoll(e.target.checked);

  $("btnGetCfg").onclick = loadConfig;
  $("btnSaveCfg").onclick = saveConfig;

  $("btnExport").onclick = exportConfig;
  $("btnImport").onclick = importConfig;
  $("btnRollback").onclick = rollbackConfig;
  $("btnReboot").onclick = reboot;

  $("btnPptOn").onclick = ()=>controlPpt(true);
  $("btnPptOff").onclick = ()=>controlPpt(false);

  document.querySelectorAll(".btnDpi").forEach(b=>{
    b.onclick = ()=>controlDpi(parseInt(b.dataset.dpi,10));
  });

  $("btnPrecOn").onclick = ()=>controlPrecision(true);
  $("btnPrecOff").onclick = ()=>controlPrecision(false);

  $("btnKeys").onclick = loadKeycodes;
  $("keySearch").oninput = renderKeycodes;

  $("btnOta").onclick = otaUpload;
}

(async function(){
  bind();
  await refreshStatus();
  setAutoPoll(true);
  await loadConfig();
})();

