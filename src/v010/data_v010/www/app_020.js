const $ = (id)=>document.getElementById(id);

async function jget(url){
  const r = await fetch(url,{cache:"no-store"});
  if(!r.ok) throw new Error(`${url} ${r.status}`);
  return await r.json();
}
async function jpost(url,obj){
  const r = await fetch(url,{
    method:"POST",
    headers:{"Content-Type":"application/json"},
    body:JSON.stringify(obj)
  });
  if(!r.ok) throw new Error(`${url} ${r.status}`);
  return await r.json();
}

let g_cfg=null;
let g_keys=null;

function fmtMs(ms){
  if(ms<1000) return `${ms} ms`;
  const s=Math.floor(ms/1000);
  const m=Math.floor(s/60);
  const h=Math.floor(m/60);
  if(h>0) return `${h}h ${m%60}m`;
  if(m>0) return `${m}m ${s%60}s`;
  return `${s}s`;
}

function setTuningUI(accelTh,damp,snap){
  $("rngAccelTh").value = accelTh;
  $("rngDamp").value = damp;
  $("rngSnap").value = snap;
  $("vAccelTh").textContent = accelTh.toFixed(1);
  $("vDamp").textContent = damp.toFixed(2);
  $("vSnap").textContent = snap.toFixed(2);
}

function hookRanges(){
  const upd=()=>{
    $("vAccelTh").textContent = parseFloat($("rngAccelTh").value).toFixed(1);
    $("vDamp").textContent = parseFloat($("rngDamp").value).toFixed(2);
    $("vSnap").textContent = parseFloat($("rngSnap").value).toFixed(2);
  };
  ["rngAccelTh","rngDamp","rngSnap"].forEach(id=>{
    $(id).addEventListener("input",upd);
  });
  upd();
}

function renderMods(mods){
  const sel=$("selMod");
  sel.innerHTML="";
  mods.forEach(m=>{
    const o=document.createElement("option");
    o.value=m.mask;
    o.textContent=`${m.name} (0x${m.mask.toString(16).padStart(2,"0")})`;
    sel.appendChild(o);
  });
}

function renderKeyList(keys, q){
  const box=$("listKeys");
  box.innerHTML="";
  const qq=(q||"").trim().toLowerCase();

  keys.filter(k=>{
    if(!qq) return true;
    return k.name.toLowerCase().includes(qq);
  }).slice(0,500).forEach(k=>{
    const it=document.createElement("div");
    it.className="item";
    const a=document.createElement("div");
    a.textContent=k.name;
    const b=document.createElement("div");
    b.className="badge";
    b.textContent=`0x${k.code.toString(16).padStart(2,"0")} (${k.code})`;
    it.appendChild(a); it.appendChild(b);
    box.appendChild(it);
  });
}

async function loadAll(){
  g_cfg = await jget("/api/config");
  g_keys = await jget("/api/keycodes");

  renderMods(g_keys.mods);
  renderKeyList(g_keys.keys,"");

  const e = g_cfg.e10;
  setTuningUI(e.accel_threshold, e.scroll_cursor_damp, (e.drift?.zero_snap_th ?? 0.6));
  $("chkHard").checked = !!e.hard_click_lock;

  hookRanges();

  $("qKey").addEventListener("input", ()=>{
    renderKeyList(g_keys.keys, $("qKey").value);
  });
}

async function pollStatus(){
  try{
    const s = await jget("/api/status");

    $("stIp").textContent = s.net?.ip ?? "-";
    $("stMdns").textContent = s.net?.mdns ?? "-";
    $("stUp").textContent = fmtMs(s.uptime_ms ?? 0);
    $("stHeap").textContent = `${s.heap_free ?? "-"} bytes`;
    $("stRssi").textContent = (s.e10?.rssi ?? 0) ? `${s.e10.rssi} dBm` : "-";
    $("stTemp").textContent = (s.e10?.temp_c ?? 0).toFixed(1) + " °C";
    $("stDt").textContent = (s.e10?.timing?.sampling_ms_avg ?? 0).toFixed(2) + " ms";
    $("stRe").textContent = `${s.e10?.reconnect_count ?? 0}`;
    $("stDe").textContent = (s.e10?.degraded ? "YES" : "no");

    $("pillConn").textContent = `BLE: ${s.e10?.ble_connected ? "OK" : "NO"}`;
    $("pillNet").textContent = `NET: ${s.net?.mode ?? "-"}`;

    // reflect live state
    // NOTE: /api/control은 토글이 아니라 set을 쓰므로, 여기서는 상태 기반으로 버튼 라벨만 바꿈
    $("btnPpt").textContent = `PPT: ${s.e10?.ppt_mode ? "ON" : "OFF"} (toggle)`;
  }catch(e){
    $("pillConn").textContent="BLE: -";
    $("pillNet").textContent="NET: -";
  }
}

async function bindActions(){
  $("btnReboot").onclick = async ()=>{
    await jpost("/api/reboot",{});
  };
  $("btnReset").onclick = async ()=>{
    if(!confirm("Factory Reset?")) return;
    const r = await jpost("/api/reset",{});
    $("noteSave").textContent = JSON.stringify(r);
  };

  $("btnPpt").onclick = async ()=>{
    // toggle: 먼저 status 읽고 반전 set
    const s = await jget("/api/status");
    const on = !!s.e10?.ppt_mode;
    await jpost("/api/control",{ppt_mode:!on});
  };

  $("btnDpi1").onclick = async ()=>{ await jpost("/api/control",{dpi_level:1}); };
  $("btnDpi2").onclick = async ()=>{ await jpost("/api/control",{dpi_level:2}); };
  $("btnDpi3").onclick = async ()=>{ await jpost("/api/control",{dpi_level:3}); };

  $("chkHard").onchange = async ()=>{
    await jpost("/api/control",{hard_click_lock: $("chkHard").checked});
  };

  $("btnApplyTuning").onclick = async ()=>{
    const accel = parseFloat($("rngAccelTh").value);
    const damp  = parseFloat($("rngDamp").value);
    const snap  = parseFloat($("rngSnap").value);
    const r = await jpost("/api/control",{tuning:{accel_threshold:accel,scroll_cursor_damp:damp,zero_snap_th:snap}});
    $("noteSave").textContent = `Applied: ${JSON.stringify(r)}`;
  };

  $("btnSaveAll").onclick = async ()=>{
    // config GET -> patch e10 fields -> POST save
    const cfg = await jget("/api/config");
    cfg.e10.hard_click_lock = $("chkHard").checked;
    cfg.e10.accel_threshold = parseFloat($("rngAccelTh").value);
    cfg.e10.scroll_cursor_damp = parseFloat($("rngDamp").value);
    cfg.e10.drift = cfg.e10.drift || {};
    cfg.e10.drift.zero_snap_th = parseFloat($("rngSnap").value);

    const r = await jpost("/api/config", cfg);
    $("noteSave").textContent = `Saved: ${JSON.stringify(r)}`;
  };
}

(async ()=>{
  await loadAll();
  await bindActions();
  await pollStatus();
  setInterval(pollStatus, 1000);
})();

