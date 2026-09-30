/* =======================================================
   File: /www/lib/am_config_0410.js
   Elite AirMouse WebConfig v0410 — E10 Config / Control / SafeBoot
   - 로드 순서: 5
   - 의존: am_base_0410.js, am_profile_0410.js
   ======================================================= */

/* =======================================================
   Config → UI 반영
   ======================================================= */
function configToUi(cfg){
  if (!cfg) return;
  const e = cfg.e10 || {};

  if (qs("e10Dpi"))         qs("e10Dpi").value         = String(e.dpi_level ?? 2);
  if (qs("e10HardClick"))   qs("e10HardClick").value   = String((e.hard_click_lock ?? true) ? "true" : "false");
  if (qs("ledBrightness"))  qs("ledBrightness").value  = String(e.led_brightness ?? 128);
  if (qs("accelThreshold")) qs("accelThreshold").value = String(e.accel_threshold ?? 8.0);
  if (qs("scrollDamp"))     qs("scrollDamp").value     = String(e.scroll_cursor_damp ?? 0.25);

  const sb = e.scale_base || [0.55, 0.75, 1.0];
  if (qs("sb0")) qs("sb0").value = String(sb[0]);
  if (qs("sb1")) qs("sb1").value = String(sb[1]);
  if (qs("sb2")) qs("sb2").value = String(sb[2]);

  const ag = e.accel_gain || [0.35, 0.55, 0.85];
  if (qs("ag0")) qs("ag0").value = String(ag[0]);
  if (qs("ag1")) qs("ag1").value = String(ag[1]);
  if (qs("ag2")) qs("ag2").value = String(ag[2]);

  const w = e.wheel || {};
  if (qs("wheelTh"))      qs("wheelTh").value      = String(w.threshold_deg ?? 90);
  if (qs("wheelStepMax")) qs("wheelStepMax").value = String(w.step_max ?? 6);

  const g = e.gesture || {};
  if (qs("flickDeg"))   qs("flickDeg").value   = String(g.flick_deg ?? 200);
  if (qs("cooldownMs")) qs("cooldownMs").value = String(g.cooldown_ms ?? 600);

  if (qs("cfgJsonArea")) qs("cfgJsonArea").value = pretty(cfg);
}

/* =======================================================
   UI → Config 반영 (E10만)
   ======================================================= */
function uiToConfig(){
  if (!g_profile || !g_profile.config) g_profile = g_profile || { config: {} };
  const cfg = g_profile.config;
  cfg.e10 = cfg.e10 || {};

  const e = cfg.e10;
  e.dpi_level          = parseNum(qs("e10Dpi")?.value, 2);
  e.hard_click_lock    = parseBool(qs("e10HardClick")?.value);
  e.led_brightness     = parseNum(qs("ledBrightness")?.value, 128);
  e.accel_threshold    = parseNum(qs("accelThreshold")?.value, 8.0);
  e.scroll_cursor_damp = parseNum(qs("scrollDamp")?.value, 0.25);

  e.scale_base = [
    parseNum(qs("sb0")?.value, 0.55),
    parseNum(qs("sb1")?.value, 0.75),
    parseNum(qs("sb2")?.value, 1.00)
  ];
  e.accel_gain = [
    parseNum(qs("ag0")?.value, 0.35),
    parseNum(qs("ag1")?.value, 0.55),
    parseNum(qs("ag2")?.value, 0.85)
  ];

  e.wheel = e.wheel || {};
  e.wheel.threshold_deg = parseNum(qs("wheelTh")?.value, 90);
  e.wheel.step_max      = parseNum(qs("wheelStepMax")?.value, 6);

  e.gesture = e.gesture || {};
  e.gesture.flick_deg   = parseNum(qs("flickDeg")?.value, 200);
  e.gesture.cooldown_ms = parseNum(qs("cooldownMs")?.value, 600);

  return cfg;
}

/* =======================================================
   Config 로드 / 저장 / Export / Import
   ======================================================= */
async function cfgLoad(){
  if (!g_profile || !g_profile.config){
    const r = await apiGet("/api/profiles/active");
    const u = unwrapApi(r);
    if (!u.ok || !u.data){ setMsg("Load failed", false); return; }
    g_profile = {
      idx: u.data.idx,
      count: u.data.count,
      config: u.data.config || {}
    };
  }
  configToUi(g_profile.config);
  setMsg("Config loaded", true);
}

async function cfgSave(){
  if (!g_profile){ await cfgLoad(); }
  const cfg = uiToConfig();

  const patch = { e10: cfg.e10 };
  const u = unwrapApi(await apiPostJson("/api/profiles/active", patch));
  if (!u.ok){
    setMsg("Save failed: " + (u.msg || u.code), false);
    return;
  }

  /* 슬롯/매크로 편집 상태 유지 (e10만 갱신) */
  if (g_profile.config) g_profile.config.e10 = cfg.e10;
  configToUi(g_profile.config);

  setMsg("Save OK (reloaded=" + (u.data && u.data.reloaded ? "yes" : "no") + ")", true);
  await refreshStatus();
}

function cfgExport(){
  const cfg = (g_profile && g_profile.config) ? g_profile.config : {};
  const blob = new Blob([pretty(cfg)], { type: "application/json" });
  const url  = URL.createObjectURL(blob);

  const a = document.createElement("a");
  a.href = url;
  const pName = (cfg.name || ("profile_" + (g_profile ? g_profile.idx : 0)))
                .replace(/[^a-zA-Z0-9_\-]/g, "_");
  a.download = `${pName}.json`;
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}

async function cfgImport(file){
  if (!file) return;

  const text = await file.text();
  let obj = null;
  try {
    obj = JSON.parse(text);
  } catch(e){
    setMsg("Import JSON error: " + e.message, false);
    return;
  }

  const u = unwrapApi(await apiPostJson("/api/config/import", obj));
  if (!u.ok){
    setMsg("Import failed: " + (u.msg || u.code), false);
    return;
  }

  setMsg("Import OK (new idx=" + ((u.data && u.data.idx) ?? "?") + ")", true);
  await profileReloadAll();
  renderSlotEditor();
  macroRenderList();
  macroRenderEditor();
  await cfgLoad();
}

/* =======================================================
   Quick Control
   ======================================================= */
async function ctlSetPpt(enable){
  const u = unwrapApi(await apiPostJson("/api/control", {
    cmd: "set_ppt", enable: !!enable, snapshot: false
  }));
  if (!u.ok) alert("set_ppt failed: " + (u.msg || u.code));
  await refreshStatus();
}

async function ctlGyroCalib(){
  if (!confirm("기기를 평평한 곳에 정지 유지.\n자이로 캘리브레이션 진행?")) return;
  const u = unwrapApi(await apiPostJson("/api/control", {
    cmd: "gyro_calib", snapshot: false
  }));
  if (!u.ok) alert("gyro_calib failed: " + (u.msg || u.code));
  else       alert("요청 완료");
  await refreshStatus();
}

async function ctlForceRelease(){
  const u = unwrapApi(await apiPostJson("/api/control", {
    cmd: "force_release", snapshot: false
  }));
  if (!u.ok) alert("force_release failed: " + (u.msg || u.code));
  else       alert("모든 버튼/키 해제 완료");
  await refreshStatus();
}

async function ctlI2cRecover(){
  if (!confirm("MPU6050 I2C 복구 진행?")) return;
  const u = unwrapApi(await apiPostJson("/api/control", {
    cmd: "i2c_recover", snapshot: false
  }));
  if (!u.ok) alert("i2c_recover failed: " + (u.msg || u.code));
  else       alert("요청 완료");
  await refreshStatus();
}

/* =======================================================
   SafeBoot / Factory Reset / Reboot
   ======================================================= */
async function safeInfo(){
  const r = await apiGet("/api/safeboot");
  alert(pretty(r.json || r.text));
}

async function factoryReset(){
  if (!confirm("Factory Reset 진행? (모든 프로파일 삭제 및 재부팅)")) return;
  const r = await fetch("/api/factory_reset", { method: "POST" });
  const t = await r.text();
  alert(t);
}

async function rebootDevice(){
  if (!confirm("디바이스를 재부팅하시겠습니까?")) return;
  const u = unwrapApi(await apiPostJson("/api/reboot", { delay_ms: 500 }));
  alert(u.ok ? "재부팅 요청 완료 (약 3초 후 재접속)"
             : "재부팅 요청 실패: " + (u.msg || u.code));
}
