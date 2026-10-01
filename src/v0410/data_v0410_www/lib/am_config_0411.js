/* =======================================================
   File: /www/lib/am_config_0411.js
   Elite AirMouse WebConfig v0410 — E10 Config / Control / SafeBoot
   - 로드 순서: 5
   - 의존: am_base_0411.js, am_profile_0411.js
   - [v0411] N-1 (safeBootExit), N-2 (otaGuardSet), N-4 (safeModeSet),
             N-10 (cfgExport 서버화), N-20 (factoryReset 진행 표시),
             Phase 1~3 (Motion Advanced UI + 프리셋)
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

  // ====================================================
  // [Phase 1~3] Motion Advanced
  // ====================================================
  const ma = e.motion_adv || {};

  // [Phase 1] Click-Freeze
  const cf = ma.click_freeze || {};
  if (qs("cfEnable"))         qs("cfEnable").value         = String((cf.enable ?? true) ? "true" : "false");
  if (qs("cfGyroTh"))         qs("cfGyroTh").value         = String(cf.gyro_th ?? 15.0);
  if (qs("cfMaxMs"))          qs("cfMaxMs").value          = String(cf.max_ms ?? 150);
  if (qs("cfHoldMs"))         qs("cfHoldMs").value         = String(cf.hold_ms ?? 20);
  if (qs("cfFadeoutMs"))      qs("cfFadeoutMs").value      = String(cf.fadeout_ms ?? 30);
  if (qs("cfMoveTh"))         qs("cfMoveTh").value         = String(cf.move_th ?? 2.0);
  if (qs("cfFreezeMoveTh"))   qs("cfFreezeMoveTh").value   = String(cf.freeze_move_th ?? 30.0);

  // [Phase 2] Adaptive EMA
  const ema = ma.ema || {};
  if (qs("emaAlphaMin"))      qs("emaAlphaMin").value      = String(ema.alpha_min ?? 0.05);
  if (qs("emaAlphaMax"))      qs("emaAlphaMax").value      = String(ema.alpha_max ?? 0.80);
  if (qs("emaDeadzoneTh"))    qs("emaDeadzoneTh").value    = String(ema.deadzone_th ?? 3.0);
  if (qs("emaFastTh"))        qs("emaFastTh").value        = String(ema.fast_th ?? 15.0);
  if (qs("emaReversalTh"))    qs("emaReversalTh").value    = String(ema.reversal_th ?? 8.0);
  if (qs("emaReversalReset")) qs("emaReversalReset").value = String((ema.reversal_reset ?? true) ? "true" : "false");

  // [Phase 3] Snap-to-Axis
  const snap = ma.snap || {};
  if (qs("snapEnable"))        qs("snapEnable").value        = String((snap.enable ?? true) ? "true" : "false");
  if (qs("snapMode1"))         qs("snapMode1").checked       = !!(snap.mode_mask & 0x01);
  if (qs("snapMode2"))         qs("snapMode2").checked       = !!(snap.mode_mask & 0x02);
  if (qs("snapMode3"))         qs("snapMode3").checked       = !!(snap.mode_mask & 0x04);
  if (qs("snapAxisMode"))      qs("snapAxisMode").value      = String(snap.axis_mode ?? 0);
  if (qs("snapRatio"))         qs("snapRatio").value         = String(snap.ratio_enter ?? 4.0);
  if (qs("snapStrength"))      qs("snapStrength").value      = String(snap.strength ?? 0.85);
  if (qs("snapConfirmFrames")) qs("snapConfirmFrames").value = String(snap.confirm_frames ?? 3);

  // ====================================================
  // [Phase 11.6 & 11.7] Power & Button
  // ====================================================
  const pw = e.power || {};
  if (qs("pwrIdleSleepSec"))   qs("pwrIdleSleepSec").value   = String(pw.idle_sleep_sec ?? 60);
  if (qs("pwrDeepSleepSec"))   qs("pwrDeepSleepSec").value   = String(pw.deep_sleep_sec ?? 600);
  if (qs("pwrWomThreshold"))   qs("pwrWomThreshold").value   = String(pw.wom_threshold ?? 25);
  if (qs("pwrWomDuration"))    qs("pwrWomDuration").value    = String(pw.wom_duration ?? 2);
  if (qs("pwrLedFadeMs"))      qs("pwrLedFadeMs").value      = String(pw.led_fade_ms ?? 800);
  if (qs("pwrWakeDebounceMs")) qs("pwrWakeDebounceMs").value = String(pw.wake_debounce_ms ?? 150);

  const bt = e.button || {};
  if (qs("btnDebouncePressMs"))   qs("btnDebouncePressMs").value   = String(bt.debounce_press_ms ?? 20);
  if (qs("btnDebounceReleaseMs")) qs("btnDebounceReleaseMs").value = String(bt.debounce_release_ms ?? 30);
  if (qs("btnClickMs"))            qs("btnClickMs").value            = String(bt.click_ms ?? 250);
  if (qs("btnDblclickGapMs"))      qs("btnDblclickGapMs").value      = String(bt.dblclick_gap_ms ?? 300);
  if (qs("btnLongPressMs"))        qs("btnLongPressMs").value        = String(bt.long_press_ms ?? 600);
  if (qs("btnMinClickMs"))         qs("btnMinClickMs").value         = String(bt.min_click_ms ?? 30);

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

  // ====================================================
  // [Phase 1~3] Motion Advanced
  // ====================================================
  e.motion_adv = e.motion_adv || {};

  // [Phase 1] Click-Freeze
  e.motion_adv.click_freeze = e.motion_adv.click_freeze || {};
  const cf = e.motion_adv.click_freeze;
  cf.enable         = parseBool(qs("cfEnable")?.value);
  cf.gyro_th        = parseNum(qs("cfGyroTh")?.value, 15.0);
  cf.max_ms         = parseNum(qs("cfMaxMs")?.value, 150);
  cf.hold_ms        = parseNum(qs("cfHoldMs")?.value, 20);
  cf.fadeout_ms     = parseNum(qs("cfFadeoutMs")?.value, 30);
  cf.move_th        = parseNum(qs("cfMoveTh")?.value, 2.0);
  cf.freeze_move_th = parseNum(qs("cfFreezeMoveTh")?.value, 30.0);

  // [Phase 2] Adaptive EMA
  e.motion_adv.ema = e.motion_adv.ema || {};
  const ema = e.motion_adv.ema;
  ema.alpha_min      = parseNum(qs("emaAlphaMin")?.value, 0.05);
  ema.alpha_max      = parseNum(qs("emaAlphaMax")?.value, 0.80);
  ema.deadzone_th    = parseNum(qs("emaDeadzoneTh")?.value, 3.0);
  ema.fast_th        = parseNum(qs("emaFastTh")?.value, 15.0);
  ema.reversal_th    = parseNum(qs("emaReversalTh")?.value, 8.0);
  ema.reversal_reset = parseBool(qs("emaReversalReset")?.value);

  // [Phase 3] Snap-to-Axis
  e.motion_adv.snap = e.motion_adv.snap || {};
  const snap = e.motion_adv.snap;
  snap.enable         = parseBool(qs("snapEnable")?.value);
  snap.mode_mask      = (qs("snapMode1")?.checked ? 0x01 : 0) |
                        (qs("snapMode2")?.checked ? 0x02 : 0) |
                        (qs("snapMode3")?.checked ? 0x04 : 0);
  snap.axis_mode      = parseNum(qs("snapAxisMode")?.value, 0);
  snap.ratio_enter    = parseNum(qs("snapRatio")?.value, 4.0);
  snap.strength       = parseNum(qs("snapStrength")?.value, 0.85);
  snap.confirm_frames = parseNum(qs("snapConfirmFrames")?.value, 3);

  // ====================================================
  // [Phase 11.6 & 11.7] Power & Button
  // ====================================================
  e.power = e.power || {};
  e.power.idle_sleep_sec    = parseNum(qs("pwrIdleSleepSec")?.value, 60);
  e.power.deep_sleep_sec    = parseNum(qs("pwrDeepSleepSec")?.value, 600);
  e.power.wom_threshold     = parseNum(qs("pwrWomThreshold")?.value, 25);
  e.power.wom_duration      = parseNum(qs("pwrWomDuration")?.value, 2);
  e.power.led_fade_ms       = parseNum(qs("pwrLedFadeMs")?.value, 800);
  e.power.wake_debounce_ms  = parseNum(qs("pwrWakeDebounceMs")?.value, 150);

  e.button = e.button || {};
  e.button.debounce_press_ms   = parseNum(qs("btnDebouncePressMs")?.value, 20);
  e.button.debounce_release_ms = parseNum(qs("btnDebounceReleaseMs")?.value, 30);
  e.button.click_ms            = parseNum(qs("btnClickMs")?.value, 250);
  e.button.dblclick_gap_ms     = parseNum(qs("btnDblclickGapMs")?.value, 300);
  e.button.long_press_ms       = parseNum(qs("btnLongPressMs")?.value, 600);
  e.button.min_click_ms        = parseNum(qs("btnMinClickMs")?.value, 30);

  return cfg;
}

/* =======================================================
   Config 로드 / 저장
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

  if (g_profile.config) g_profile.config.e10 = cfg.e10;
  configToUi(g_profile.config);

  setMsg("Save OK (reloaded=" + (u.data && u.data.reloaded ? "yes" : "no") + ")", true);
  await refreshStatus();
}

/* =======================================================
   [N-10] Config Export (온라인: 서버 파일 사용)
   ======================================================= */
function cfgExport(){
  const cfg = (g_profile && g_profile.config) ? g_profile.config : {};
  const pName = (cfg.name || ("profile_" + (g_profile ? g_profile.idx : 0)))
                .replace(/[^a-zA-Z0-9_\-]/g, "_");

  if (g_appMode === APP_MODE_ONLINE){
    const a = document.createElement("a");
    a.href = `/api/config/export?filename=${encodeURIComponent(pName + ".json")}`;
    a.download = "";
    document.body.appendChild(a);
    a.click();
    a.remove();
    return;
  }

  const blob = new Blob([pretty(cfg)], { type: "application/json" });
  const url  = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
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

  showLoading("Import 중…");
  try {
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
  } finally {
    hideLoading();
  }
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

async function ctlForceRelease(){
  const u = unwrapApi(await apiPostJson("/api/control", {
    cmd: "force_release", snapshot: false
  }));
  if (!u.ok) alert("force_release failed: " + (u.msg || u.code));
  else       alert("모든 버튼/키 해제 완료");
  await refreshStatus();
}

/* =======================================================
   SafeBoot / Factory Reset / Reboot
   ======================================================= */
async function safeInfo(){
  const r = await apiGet("/api/safeboot");
  alert(pretty(r.json || r.text));
}

/* [N-1] SafeBoot 해제 (재부팅 포함) */
async function safeBootExit(){
  if (!confirm("SafeMode를 해제하고 재부팅하시겠습니까?")) return;

  setMsg("SafeBoot 해제 요청 중…", true);

  const u = unwrapApi(await apiPostJson("/api/safeboot", { exit: true }));
  if (!u.ok){
    alert("SafeBoot 해제 실패: " + (u.msg || u.code));
    setMsg("SafeBoot 해제 실패", false);
    return;
  }

  setMsg("SafeBoot 해제 완료. 재부팅 대기 중…", true);
  showLoading("재부팅 대기 중…");

  let v_tries = 0;
  const v_timer = setInterval(async () => {
    v_tries++;
    try {
      const r = await fetch("/api/status?compact=1", { cache: "no-store" });
      if (r.ok){
        clearInterval(v_timer);
        hideLoading();
        location.reload();
      }
    } catch(e){}
    if (v_tries >= 15){
      clearInterval(v_timer);
      hideLoading();
      alert("재부팅이 지연되고 있습니다. 페이지를 새로고침 해주세요.");
    }
  }, 1000);
}

/* [N-4] SafeMode 수동 토글 */
async function safeModeSet(enable){
  const msg = enable
    ? "SafeMode로 진입합니다.\nHID 입력이 차단됩니다 (웹 설정은 계속 가능)."
    : "SafeMode를 해제합니다.\nHID 입력이 재개됩니다.";
  if (!confirm(msg)) return;

  const u = unwrapApi(await apiPostJson("/api/control", {
    cmd: "set_safe_mode", enable: !!enable, snapshot: true
  }));
  if (!u.ok){
    alert("SafeMode 변경 실패: " + (u.msg || u.code));
    return;
  }
  setMsg("SafeMode " + (enable ? "진입" : "해제") + " 완료", true);
  await refreshStatus();
}

/* [N-2] OTA Guard 수동 토글 */
async function otaGuardSet(enable){
  const u = unwrapApi(await apiPostJson("/api/control", {
    cmd: "set_ota_guard", enable: !!enable, snapshot: true
  }));
  if (!u.ok){
    alert("OTA Guard 변경 실패: " + (u.msg || u.code));
    return false;
  }
  setMsg("OTA Guard " + (enable ? "ON" : "OFF"), true);
  await refreshStatus();
  return true;
}

/* [N-20] Factory Reset (진행 표시 + 자동 재접속) */
async function factoryReset(){
  if (!confirm("Factory Reset 진행?\n\n모든 프로파일이 삭제되고 기본값으로 재부팅됩니다.")) return;

  const hint = qs("factoryHint");
  if (hint) hint.textContent = "요청 전송 중…";

  try {
    const r = await fetch("/api/factory_reset", { method: "POST" });
    const t = await r.text();

    if (hint) hint.textContent = "재부팅 대기 중…";
    showLoading("재부팅 대기 중…");

    let v_tries = 0;
    const v_timer = setInterval(async () => {
      v_tries++;
      try {
        const r2 = await fetch("/api/status?compact=1", { cache: "no-store" });
        if (r2.ok){
          clearInterval(v_timer);
          hideLoading();
          location.reload();
        }
      } catch(e){}
      if (v_tries >= 20){
        clearInterval(v_timer);
        hideLoading();
        if (hint) hint.textContent = "재부팅 지연 — 새로고침하세요";
        alert("재부팅이 지연되고 있습니다. 페이지를 새로고침 해주세요.");
      }
    }, 1000);

  } catch(e){
    hideLoading();
    alert("Factory Reset 실패: " + e.message);
  }
}

async function rebootDevice(){
  if (!confirm("디바이스를 재부팅하시겠습니까?")) return;
  const u = unwrapApi(await apiPostJson("/api/reboot", { delay_ms: 500 }));
  alert(u.ok ? "재부팅 요청 완료 (약 3초 후 재접속)"
             : "재부팅 요청 실패: " + (u.msg || u.code));
}

/* =======================================================
   [Phase 1~3] Motion Advanced 프리셋
   ======================================================= */
const MOTION_PRESETS = {
  PC: {
    click_freeze: { enable: true,  max_ms: 150, gyro_th: 15.0 },
    ema:          { alpha_min: 0.05, alpha_max: 0.80 },
    snap:         { enable: false, mode_mask: 0x00 }
  },
  PPT: {
    click_freeze: { enable: true,  max_ms: 100, gyro_th: 15.0 },
    ema:          { alpha_min: 0.05, alpha_max: 0.80 },
    snap:         { enable: true,  mode_mask: 0x02 }
  },
  TV: {
    click_freeze: { enable: false, max_ms: 150, gyro_th: 15.0 },
    ema:          { alpha_min: 0.10, alpha_max: 0.85 },
    snap:         { enable: false, mode_mask: 0x00 }
  },
  Gaming: {
    click_freeze: { enable: false, max_ms: 150, gyro_th: 15.0 },
    ema:          { alpha_min: 0.15, alpha_max: 0.90 },
    snap:         { enable: false, mode_mask: 0x00 }
  },
  Precision: {
    click_freeze: { enable: true,  max_ms: 200, gyro_th: 15.0 },
    ema:          { alpha_min: 0.03, alpha_max: 0.70 },
    snap:         { enable: false, mode_mask: 0x00 }
  }
};

async function applyMotionPreset(presetKey) {
  const p = MOTION_PRESETS[presetKey];
  if (!p) return;

  if (!confirm(`프리셋 "${presetKey}"을 현재 프로파일에 적용하시겠습니까?\n(저장 후 기기에 반영됩니다)`)) return;

  // [Phase 1] Click-Freeze UI 반영
  if (qs("cfEnable"))    qs("cfEnable").value    = String(p.click_freeze.enable ? "true" : "false");
  if (qs("cfMaxMs"))     qs("cfMaxMs").value     = String(p.click_freeze.max_ms);
  if (qs("cfGyroTh"))    qs("cfGyroTh").value    = String(p.click_freeze.gyro_th);

  // [Phase 2] EMA UI 반영
  if (qs("emaAlphaMin")) qs("emaAlphaMin").value = String(p.ema.alpha_min);
  if (qs("emaAlphaMax")) qs("emaAlphaMax").value = String(p.ema.alpha_max);

  // [Phase 3] Snap UI 반영
  if (qs("snapEnable"))  qs("snapEnable").value  = String(p.snap.enable ? "true" : "false");
  if (qs("snapMode1"))   qs("snapMode1").checked = !!(p.snap.mode_mask & 0x01);
  if (qs("snapMode2"))   qs("snapMode2").checked = !!(p.snap.mode_mask & 0x02);
  if (qs("snapMode3"))   qs("snapMode3").checked = !!(p.snap.mode_mask & 0x04);

  // Config 반영 + 저장
  uiToConfig();
  await cfgSave();

  const hint = qs("presetHint");
  if (hint) hint.textContent = `프리셋 "${presetKey}" 적용 및 저장 완료`;
}
