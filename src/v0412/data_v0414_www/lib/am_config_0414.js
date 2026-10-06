/* =======================================================
   File: /www/lib/am_config_0414.js
   Elite AirMouse WebConfig v0414 — E10 Config / Control / SafeBoot
   - 로드 순서: 6
   - [Phase 2.0.1 N-1] deep_idle_timeout_ms 클램프
   - [Phase 3.2 H-8] 프리셋 19필드 전체화
   - [Phase 3.6 L-5] bindConfigDirtyTracker + clearDirty
   ======================================================= */

/* =======================================================
   Config → UI 반영
   ======================================================= */
function configToUi(cfg) {
  if (!cfg) return;
  const e = cfg.e10 || {};

  if (qs("e10Dpi")) qs("e10Dpi").value = String(e.dpi_level ?? 2);
  if (qs("e10HardClick")) qs("e10HardClick").value = String((e.hard_click_lock ?? true) ? "true" : "false");
  if (qs("ledBrightness")) qs("ledBrightness").value = String(e.led_brightness ?? 128);
  if (qs("accelThreshold")) qs("accelThreshold").value = String(e.accel_threshold ?? 8.0);
  if (qs("scrollDamp")) qs("scrollDamp").value = String(e.scroll_cursor_damp ?? 0.25);

  const sb = e.scale_base || [0.55, 0.75, 1.0];
  if (qs("sb0")) qs("sb0").value = String(sb[0]);
  if (qs("sb1")) qs("sb1").value = String(sb[1]);
  if (qs("sb2")) qs("sb2").value = String(sb[2]);

  const ag = e.accel_gain || [0.35, 0.55, 0.85];
  if (qs("ag0")) qs("ag0").value = String(ag[0]);
  if (qs("ag1")) qs("ag1").value = String(ag[1]);
  if (qs("ag2")) qs("ag2").value = String(ag[2]);

  const w = e.wheel || {};
  if (qs("wheelTh")) qs("wheelTh").value = String(w.threshold_deg ?? 90);
  if (qs("wheelStepMax")) qs("wheelStepMax").value = String(w.step_max ?? 6);

  const g = e.gesture || {};
  if (qs("flickDeg")) qs("flickDeg").value = String(g.flick_deg ?? 200);
  if (qs("cooldownMs")) qs("cooldownMs").value = String(g.cooldown_ms ?? 600);

  // ====================================================
  // [Phase 1~3] Motion Advanced
  // ====================================================
  const ma = e.motion_adv || {};

  const cf = ma.click_freeze || {};
  if (qs("cfEnable")) qs("cfEnable").value = String((cf.enable ?? true) ? "true" : "false");
  if (qs("cfGyroTh")) qs("cfGyroTh").value = String(cf.gyro_th ?? 15.0);
  if (qs("cfMaxMs")) qs("cfMaxMs").value = String(cf.max_ms ?? 150);
  if (qs("cfHoldMs")) qs("cfHoldMs").value = String(cf.hold_ms ?? 20);
  if (qs("cfFadeoutMs")) qs("cfFadeoutMs").value = String(cf.fadeout_ms ?? 30);
  if (qs("cfMoveTh")) qs("cfMoveTh").value = String(cf.move_th ?? 2.0);
  if (qs("cfFreezeMoveTh")) qs("cfFreezeMoveTh").value = String(cf.freeze_move_th ?? 30.0);

  const ema = ma.ema || {};
  if (qs("emaAlphaMin")) qs("emaAlphaMin").value = String(ema.alpha_min ?? 0.05);
  if (qs("emaAlphaMax")) qs("emaAlphaMax").value = String(ema.alpha_max ?? 0.80);
  if (qs("emaDeadzoneTh")) qs("emaDeadzoneTh").value = String(ema.deadzone_th ?? 3.0);
  if (qs("emaFastTh")) qs("emaFastTh").value = String(ema.fast_th ?? 15.0);
  if (qs("emaReversalTh")) qs("emaReversalTh").value = String(ema.reversal_th ?? 8.0);
  if (qs("emaReversalReset")) qs("emaReversalReset").value = String((ema.reversal_reset ?? true) ? "true" : "false");

  const snap = ma.snap || {};
  if (qs("snapEnable")) qs("snapEnable").value = String((snap.enable ?? true) ? "true" : "false");
  if (qs("snapMode1")) qs("snapMode1").checked = !!(snap.mode_mask & 0x01);
  if (qs("snapMode2")) qs("snapMode2").checked = !!(snap.mode_mask & 0x02);
  if (qs("snapMode3")) qs("snapMode3").checked = !!(snap.mode_mask & 0x04);
  if (qs("snapAxisMode")) qs("snapAxisMode").value = String(snap.axis_mode ?? 0);
  if (qs("snapRatio")) qs("snapRatio").value = String(snap.ratio_enter ?? 4.0);
  if (qs("snapStrength")) qs("snapStrength").value = String(snap.strength ?? 0.85);
  if (qs("snapConfirmFrames")) qs("snapConfirmFrames").value = String(snap.confirm_frames ?? 3);

  // ====================================================
  // [Phase 11.6] Power
  // ====================================================
  const pw = e.power || {};
  const pIdleMs = Array.isArray(pw.idle_timeout_ms) ?
    pw.idle_timeout_ms :
    [60000, 120000, 300000];

  if (qs("pwrIdleSleepM1")) qs("pwrIdleSleepM1").value = String(Math.round((pIdleMs[0] ?? 60000) / 1000));
  if (qs("pwrIdleSleepM2")) qs("pwrIdleSleepM2").value = String(Math.round((pIdleMs[1] ?? 120000) / 1000));
  if (qs("pwrIdleSleepM3")) qs("pwrIdleSleepM3").value = String(Math.round((pIdleMs[2] ?? 300000) / 1000));
  if (qs("pwrIdleSleepBle")) qs("pwrIdleSleepBle").value = String(Math.round((pw.idle_timeout_ble_ms ?? 300000) / 1000));
  if (qs("pwrPairingIdleSec")) qs("pwrPairingIdleSec").value = String(Math.round((pw.pairing_idle_timeout_ms ?? 30000) / 1000));
  if (qs("pwrDeepSleepSec")) qs("pwrDeepSleepSec").value = String(Math.round((pw.deep_idle_timeout_ms ?? 600000) / 1000));
  if (qs("pwrWakeMinActiveMs")) qs("pwrWakeMinActiveMs").value = String(pw.wake_min_active_ms ?? 500);
  if (qs("pwrWomThreshold")) qs("pwrWomThreshold").value = String(pw.wom_threshold ?? 25);
  if (qs("pwrWomDuration")) qs("pwrWomDuration").value = String(pw.wom_duration ?? 4);
  if (qs("pwrFastRecalibMs")) qs("pwrFastRecalibMs").value = String(pw.fast_recalib_ms ?? 300);
  if (qs("pwrLedFadeoutMs")) qs("pwrLedFadeoutMs").value = String(pw.led_fadeout_ms ?? 500);
  if (qs("pwrLedFadeinMs")) qs("pwrLedFadeinMs").value = String(pw.led_fadein_ms ?? 300);

  // ====================================================
  // [Phase 11.7] Button Timing
  // ====================================================
  const bt = e.button || {};
  if (qs("btnDebouncePressMs")) qs("btnDebouncePressMs").value = String(bt.debounce_press_ms ?? 32);
  if (qs("btnDebounceReleaseMs")) qs("btnDebounceReleaseMs").value = String(bt.debounce_release_ms ?? 16);
  if (qs("btnLongDelayMs")) qs("btnLongDelayMs").value = String(bt.long_delay_ms ?? 800);
  if (qs("btnDoubleDelayMs")) qs("btnDoubleDelayMs").value = String(bt.double_delay_ms ?? 320);
  if (qs("btnMinClickMs")) qs("btnMinClickMs").value = String(bt.min_click_ms ?? 16);
  if (qs("btnHold2sMs")) qs("btnHold2sMs").value = String(bt.hold_2s_ms ?? 2000);
  if (qs("btnHold3sMs")) qs("btnHold3sMs").value = String(bt.hold_3s_ms ?? 3000);
  if (qs("btnDebounceMinTicks")) qs("btnDebounceMinTicks").value = String(bt.debounce_min_ticks ?? 3);

  if (qs("cfgJsonArea")) qs("cfgJsonArea").value = pretty(cfg);
}

/* =======================================================
   UI → Config 반영
   ======================================================= */
function uiToConfig() {
  if (!g_profile || !g_profile.config) g_profile = g_profile || { config: {} };
  const cfg = g_profile.config;
  cfg.e10 = cfg.e10 || {};

  const e = cfg.e10;
  e.dpi_level = parseNum(qs("e10Dpi")?.value, 2);
  e.hard_click_lock = parseBool(qs("e10HardClick")?.value);
  e.led_brightness = parseNum(qs("ledBrightness")?.value, 128);
  e.accel_threshold = parseNum(qs("accelThreshold")?.value, 8.0);
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
  e.wheel.step_max = parseNum(qs("wheelStepMax")?.value, 6);

  e.gesture = e.gesture || {};
  e.gesture.flick_deg = parseNum(qs("flickDeg")?.value, 200);
  e.gesture.cooldown_ms = parseNum(qs("cooldownMs")?.value, 600);

  // ====================================================
  // [Phase 1~3] Motion Advanced
  // ====================================================
  // [H-1] 서버 validateE10()과 정합하도록 클라이언트 클램프
  e.motion_adv = e.motion_adv || {};
  
  const _c = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
  
  // [Phase 1] Click-Freeze
  e.motion_adv.click_freeze = e.motion_adv.click_freeze || {};
  const cf = e.motion_adv.click_freeze;
  cf.enable = parseBool(qs("cfEnable")?.value);
  cf.gyro_th = _c(parseNum(qs("cfGyroTh")?.value, 15.0), 5.0, 30.0);
  cf.max_ms = _c(parseNum(qs("cfMaxMs")?.value, 150), 50, 300);
  cf.hold_ms = _c(parseNum(qs("cfHoldMs")?.value, 20), 0, 50);
  cf.fadeout_ms = _c(parseNum(qs("cfFadeoutMs")?.value, 30), 0, 80);
  cf.move_th = _c(parseNum(qs("cfMoveTh")?.value, 2.0), 1.0, 10.0);
  cf.freeze_move_th = _c(parseNum(qs("cfFreezeMoveTh")?.value, 30.0), 10.0, 60.0);
  
  // [Phase 2] Adaptive EMA
  e.motion_adv.ema = e.motion_adv.ema || {};
  const ema = e.motion_adv.ema;
  ema.alpha_min = _c(parseNum(qs("emaAlphaMin")?.value, 0.05), 0.01, 0.20);
  ema.alpha_max = _c(parseNum(qs("emaAlphaMax")?.value, 0.80), 0.50, 0.95);
  if (ema.alpha_min >= ema.alpha_max) ema.alpha_max = ema.alpha_min + 0.01;
  ema.deadzone_th = _c(parseNum(qs("emaDeadzoneTh")?.value, 3.0), 1.0, 10.0);
  ema.fast_th = _c(parseNum(qs("emaFastTh")?.value, 15.0), 10.0, 30.0);
  if (ema.deadzone_th >= ema.fast_th) ema.fast_th = ema.deadzone_th + 1.0;
  ema.reversal_th = _c(parseNum(qs("emaReversalTh")?.value, 8.0), 5.0, 20.0);
  ema.reversal_reset = parseBool(qs("emaReversalReset")?.value);
  
  // [Phase 3] Snap-to-Axis
  e.motion_adv.snap = e.motion_adv.snap || {};
  const snap = e.motion_adv.snap;
  snap.enable = parseBool(qs("snapEnable")?.value);
  snap.mode_mask = ((qs("snapMode1")?.checked ? 0x01 : 0) |
    (qs("snapMode2")?.checked ? 0x02 : 0) |
    (qs("snapMode3")?.checked ? 0x04 : 0)) & 0x07;
  snap.axis_mode = _c(parseNum(qs("snapAxisMode")?.value, 0), 0, 2);
  snap.ratio_enter = _c(parseNum(qs("snapRatio")?.value, 4.0), 2.0, 10.0);
  snap.strength = _c(parseNum(qs("snapStrength")?.value, 0.85), 0.5, 1.0);
  snap.confirm_frames = _c(parseNum(qs("snapConfirmFrames")?.value, 3), 1, 10);
  

  // ====================================================
  // [Phase 11.6] Power
  // ====================================================
  e.power = e.power || {};
  const pSec2Ms = (v, defSec) => Math.max(0, Math.round(parseNum(v, defSec) * 1000));

  e.power.idle_timeout_ms = [
    pSec2Ms(qs("pwrIdleSleepM1")?.value, 60),
    pSec2Ms(qs("pwrIdleSleepM2")?.value, 120),
    pSec2Ms(qs("pwrIdleSleepM3")?.value, 300)
  ];
  e.power.idle_timeout_ble_ms = pSec2Ms(qs("pwrIdleSleepBle")?.value, 300);
  e.power.pairing_idle_timeout_ms = pSec2Ms(qs("pwrPairingIdleSec")?.value, 30);

  // [N-1] 서버 validateE10: 0 또는 300000~7200000ms(300~7200초)만 허용
  //   - 범위 밖 값이면 가장 가까운 유효값으로 스냅 (조용한 저장 실패 방지)
  {
    const v_deepSec = parseNum(qs("pwrDeepSleepSec")?.value, 600);
    if (v_deepSec !== 0 && v_deepSec < 300) {
      e.power.deep_idle_timeout_ms = 300 * 1000;      // 자동 상향 (300초)
    } else if (v_deepSec > 7200) {
      e.power.deep_idle_timeout_ms = 7200 * 1000;     // 자동 하향 (7200초)
    } else {
      e.power.deep_idle_timeout_ms = Math.round(v_deepSec * 1000);
    }
  }

  e.power.wake_min_active_ms = parseNum(qs("pwrWakeMinActiveMs")?.value, 500);
  e.power.wom_threshold = parseNum(qs("pwrWomThreshold")?.value, 25);
  e.power.wom_duration = parseNum(qs("pwrWomDuration")?.value, 4);
  e.power.fast_recalib_ms = parseNum(qs("pwrFastRecalibMs")?.value, 300);
  e.power.led_fadeout_ms = parseNum(qs("pwrLedFadeoutMs")?.value, 500);
  e.power.led_fadein_ms = parseNum(qs("pwrLedFadeinMs")?.value, 300);

  // 클라이언트 측 범위 클램프
  const _pwClamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
  e.power.wake_min_active_ms = _pwClamp(e.power.wake_min_active_ms, 100, 2000);
  e.power.wom_threshold = _pwClamp(e.power.wom_threshold, 5, 100);
  e.power.wom_duration = _pwClamp(e.power.wom_duration, 1, 50);
  e.power.fast_recalib_ms = _pwClamp(e.power.fast_recalib_ms, 100, 1000);
  e.power.led_fadeout_ms = _pwClamp(e.power.led_fadeout_ms, 0, 1000);
  e.power.led_fadein_ms = _pwClamp(e.power.led_fadein_ms, 0, 1000);

  // ====================================================
  // [Phase 11.7] Button Timing
  // ====================================================
  e.button = e.button || {};
  e.button.debounce_press_ms = parseNum(qs("btnDebouncePressMs")?.value, 32);
  e.button.debounce_release_ms = parseNum(qs("btnDebounceReleaseMs")?.value, 16);
  e.button.long_delay_ms = parseNum(qs("btnLongDelayMs")?.value, 800);
  e.button.double_delay_ms = parseNum(qs("btnDoubleDelayMs")?.value, 320);
  e.button.min_click_ms = parseNum(qs("btnMinClickMs")?.value, 16);
  e.button.hold_2s_ms = parseNum(qs("btnHold2sMs")?.value, 2000);
  e.button.hold_3s_ms = parseNum(qs("btnHold3sMs")?.value, 3000);
  e.button.debounce_min_ticks = parseNum(qs("btnDebounceMinTicks")?.value, 3);

  // 서버 validateE10 강제 조건 클라이언트 선반영
  if (e.button.debounce_press_ms < e.button.debounce_release_ms) {
    e.button.debounce_press_ms = e.button.debounce_release_ms;
  }
  if (e.button.hold_2s_ms >= e.button.hold_3s_ms) {
    e.button.hold_3s_ms = e.button.hold_2s_ms + 1000;
  }
  const _align8 = (v) => Math.round(v / 8) * 8;
  e.button.debounce_press_ms = Math.max(8, _align8(e.button.debounce_press_ms));
  e.button.debounce_release_ms = Math.max(8, _align8(e.button.debounce_release_ms));
  e.button.min_click_ms = _align8(e.button.min_click_ms);

  return cfg;
}

/* =======================================================
   Config 로드 / 저장
   - [Phase 3.6 L-5] 저장 성공 시 clearDirty
   ======================================================= */

async function cfgLoad() {
  if (!g_profile || !g_profile.config) {
    const r = await apiGet("/api/profiles/active");
    const u = unwrapApi(r);
    if (!u.ok || !u.data) { setMsg(t("cfg.load_fail"), false); return; }
    g_profile = {
      idx: u.data.idx,
      count: u.data.count,
      config: u.data.config || {}
    };
  }
  configToUi(g_profile.config);
  setMsg(t("cfg.load_ok"), true);
}


async function cfgSave() {
  if (!g_profile) { await cfgLoad(); }
  const cfg = uiToConfig();

  const patch = { e10: cfg.e10 };
  const u = unwrapApi(await apiPostJson("/api/profiles/active", patch));
  if (!u.ok) {
    setMsg(`${t("cfg.save_fail")}: ${u.msg || u.code}`, false);
    return;
  }

  if (g_profile.config) g_profile.config.e10 = cfg.e10;
  configToUi(g_profile.config);

  setMsg(t("cfg.save_ok_resp", { reloaded: (u.data && u.data.reloaded) ? "yes" : "no" }), true);
  // [L-5] 저장 성공 시 dirty 클리어
  // ── 이후 ──
  if (typeof clearCfgDirty === "function") clearCfgDirty();
  await refreshStatus();
}

/* =======================================================
   [Phase 3.6 L-5] Config 폼 dirty 추적 (이벤트 위임)
   ======================================================= */
function bindConfigDirtyTracker() {
  const tabConfig = qs("tab-config");
  if (!tabConfig) return;
  if (tabConfig.dataset.dirtyBound === "1") return;
  tabConfig.dataset.dirtyBound = "1";

  tabConfig.addEventListener("input",  () => { if (typeof markCfgDirty === "function") markCfgDirty(); });
  tabConfig.addEventListener("change", () => { if (typeof markCfgDirty === "function") markCfgDirty(); });
}

/* =======================================================
   [N-10] Config Export
   ======================================================= */
function cfgExport() {
  const cfg = (g_profile && g_profile.config) ? g_profile.config : {};
  const pName = (cfg.name || ("profile_" + (g_profile ? g_profile.idx : 0)))
    .replace(/[^a-zA-Z0-9_\-]/g, "_");

  if (g_appMode === APP_MODE_ONLINE) {
    const a = document.createElement("a");
    a.href = `/api/config/export?filename=${encodeURIComponent(pName + ".json")}`;
    a.download = "";
    document.body.appendChild(a);
    a.click();
    a.remove();
    return;
  }

  const blob = new Blob([pretty(cfg)], { type: "application/json" });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = `${pName}.json`;
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}

async function cfgImport(file) {
  if (!file) return;

  const text = await file.text();
  let obj = null;
  try {
    obj = JSON.parse(text);
  } catch (e) {
    setMsg(t("cfg.import_err", { msg: e.message }), false);
    return;
  }

  showLoading(t("loading.processing"));
  try {
    const u = unwrapApi(await apiPostJson("/api/config/import", obj));
    if (!u.ok) {
      setMsg(`${t("cfg.import_fail")}: ${u.msg || u.code}`, false);
      return;
    }

    setMsg(t("cfg.import_ok", { idx: (u.data && u.data.idx) ?? "?" }), true);

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
async function ctlSetPpt(enable) {
  const u = unwrapApi(await apiPostJson("/api/control", {
    cmd: "set_ppt", enable: !!enable, snapshot: false
  }));
  if (!u.ok) alert("set_ppt failed: " + (u.msg || u.code));
  await refreshStatus();
}

async function ctlForceRelease() {
  const u = unwrapApi(await apiPostJson("/api/control", {
    cmd: "force_release", snapshot: false
  }));
  if (!u.ok) alert(`${t("pop.test_fail")} ${u.msg || u.code}`);
  else alert(t("pop.force_release_ok"));
  await refreshStatus();
}

/* =======================================================
   SafeBoot / Factory Reset / Reboot
   ======================================================= */
async function safeInfo() {
  const r = await apiGet("/api/safeboot");
  alert(pretty(r.json || r.text));
}

async function safeBootExit() {
  if (!confirm(t("pop.safeboot_exit_confirm"))) return;

  setMsg(t("pop.safeboot_exit_req"), true);

  const u = unwrapApi(await apiPostJson("/api/safeboot", { exit: true }));
  if (!u.ok) {
    alert(`${t("pop.safeboot_exit_fail")} ${u.msg || u.code}`);
    setMsg(t("pop.safeboot_exit_fail"), false);
    return;
  }

  setMsg(t("pop.safeboot_exit_done"), true);
  showLoading(t("pop.reboot_waiting"));

  let v_tries = 0;
  const v_timer = setInterval(async () => {
    v_tries++;
    try {
      const r = await fetch("/api/status?compact=1", { cache: "no-store" });
      if (r.ok) {
        clearInterval(v_timer);
        hideLoading();
        location.reload();
      }
    } catch (e) { }
    if (v_tries >= 15) {
      clearInterval(v_timer);
      hideLoading();
      alert(t("pop.reboot_delay_warn"));
    }
  }, 1000);
}

async function safeModeSet(enable) {
  const msg = enable
    ? t("pop.safemode_enter_confirm")
    : t("pop.safemode_exit_confirm");
  if (!confirm(msg)) return;

  const u = unwrapApi(await apiPostJson("/api/control", {
    cmd: "set_safe_mode", enable: !!enable, snapshot: true
  }));
  if (!u.ok) {
    alert(`${t("pop.safemode_change_fail")} ${u.msg || u.code}`);
    return;
  }
  setMsg(enable ? t("pop.safemode_on") : t("pop.safemode_off"), true);
  await refreshStatus();
}

async function otaGuardSet(enable) {
  const u = unwrapApi(await apiPostJson("/api/control", {
    cmd: "set_ota_guard", enable: !!enable, snapshot: true
  }));
  if (!u.ok) {
    alert("OTA Guard failed: " + (u.msg || u.code));
    return false;
  }
  setMsg("OTA Guard " + (enable ? "ON" : "OFF"), true);
  await refreshStatus();
  return true;
}

/* [N-20] Factory Reset — [Phase 3.6 L-5] 미저장 경고 추가 */
async function factoryReset() {
  // [L-5] 저장 안 된 변경사항 경고
  if (typeof hasUnsavedChanges === "function" && hasUnsavedChanges()) {
    if (!confirm(t("pop.unsaved_changes_confirm"))) return;
  }
  if (!confirm(t("pop.factory_reset_confirm"))) return;

  const hint = qs("factoryHint");
  if (hint) hint.textContent = t("pop.request_sending");

  try {
    const raw = await fetch("/api/factory_reset", { method: "POST" });
    const text = await raw.text();
    let json = null;
    try { json = JSON.parse(text); } catch (e) { }

    const u = unwrapApi({ ok: raw.ok, status: raw.status, text, json });
    if (!u.ok) {
      if (hint) hint.textContent = `${t("pop.factory_reset_fail")} ${u.code || ""}`;
      hideLoading();
      alert(`${t("pop.factory_reset_fail")} ${u.msg || u.code || raw.status}`);
      return;
    }

    if (hint) hint.textContent = t("pop.reboot_waiting");
    showLoading(t("pop.reboot_waiting"));

    let v_tries = 0;
    const v_timer = setInterval(async () => {
      v_tries++;
      try {
        const r2 = await fetch("/api/status?compact=1", { cache: "no-store" });
        if (r2.ok) {
          clearInterval(v_timer);
          hideLoading();
          location.reload();
        }
      } catch (e) { }
      if (v_tries >= 20) {
        clearInterval(v_timer);
        hideLoading();
        if (hint) hint.textContent = t("pop.reboot_delayed");
        alert(t("pop.reboot_delay_warn"));
      }
    }, 1000);

  } catch (e) {
    hideLoading();
    alert(`${t("pop.factory_reset_fail")} ${e.message}`);
  }
}

/* [Phase 3.6 L-5] 미저장 경고 추가 */
async function rebootDevice() {
  if (typeof hasUnsavedChanges === "function" && hasUnsavedChanges()) {
    if (!confirm(t("pop.unsaved_changes_confirm"))) return;
  }
  if (!confirm(t("pop.reboot_confirm"))) return;
  const u = unwrapApi(await apiPostJson("/api/reboot", { delay_ms: 500 }));
  alert(u.ok ? t("pop.reboot_requested")
    : `${t("pop.reboot_fail")} ${u.msg || u.code}`);
}

/* =======================================================
   [Phase 3.2 H-8] Motion 프리셋 (19필드 전체)
   ======================================================= */
const MOTION_PRESETS = {
  /* 일반 PC */
  PC: {
    click_freeze: {
      enable: true, gyro_th: 15.0, max_ms: 150, hold_ms: 20,
      fadeout_ms: 30, move_th: 2.0, freeze_move_th: 30.0
    },
    ema: {
      alpha_min: 0.05, alpha_max: 0.80, deadzone_th: 3.0,
      fast_th: 15.0, reversal_th: 8.0, reversal_reset: true
    },
    snap: {
      enable: false, mode_mask: 0x00, axis_mode: 0,
      confirm_frames: 3, ratio_enter: 4.0, strength: 0.85
    }
  },

  /* 프레젠테이션 */
  PPT: {
    click_freeze: {
      enable: true, gyro_th: 15.0, max_ms: 100, hold_ms: 20,
      fadeout_ms: 30, move_th: 2.0, freeze_move_th: 30.0
    },
    ema: {
      alpha_min: 0.05, alpha_max: 0.80, deadzone_th: 3.0,
      fast_th: 15.0, reversal_th: 8.0, reversal_reset: true
    },
    snap: {
      enable: true, mode_mask: 0x02, axis_mode: 0,
      confirm_frames: 3, ratio_enter: 4.0, strength: 0.85
    }
  },

  /* 스마트 TV */
  TV: {
    click_freeze: {
      enable: false, gyro_th: 15.0, max_ms: 150, hold_ms: 20,
      fadeout_ms: 30, move_th: 2.0, freeze_move_th: 30.0
    },
    ema: {
      alpha_min: 0.10, alpha_max: 0.85, deadzone_th: 3.5,
      fast_th: 15.0, reversal_th: 8.0, reversal_reset: true
    },
    snap: {
      enable: false, mode_mask: 0x00, axis_mode: 0,
      confirm_frames: 3, ratio_enter: 4.0, strength: 0.85
    }
  },

  /* 게이밍 */
  Gaming: {
    click_freeze: {
      enable: false, gyro_th: 15.0, max_ms: 150, hold_ms: 20,
      fadeout_ms: 30, move_th: 2.0, freeze_move_th: 30.0
    },
    ema: {
      alpha_min: 0.15, alpha_max: 0.90, deadzone_th: 2.0,
      fast_th: 12.0, reversal_th: 8.0, reversal_reset: true
    },
    snap: {
      enable: false, mode_mask: 0x00, axis_mode: 0,
      confirm_frames: 3, ratio_enter: 4.0, strength: 0.85
    }
  },

  /* 정밀 작업 */
  Precision: {
    click_freeze: {
      enable: true, gyro_th: 15.0, max_ms: 200, hold_ms: 25,
      fadeout_ms: 40, move_th: 2.0, freeze_move_th: 30.0
    },
    ema: {
      alpha_min: 0.03, alpha_max: 0.70, deadzone_th: 4.0,
      fast_th: 15.0, reversal_th: 8.0, reversal_reset: true
    },
    snap: {
      enable: false, mode_mask: 0x00, axis_mode: 0,
      confirm_frames: 3, ratio_enter: 4.0, strength: 0.85
    }
  }
};

async function applyMotionPreset(presetKey) {
  const p = MOTION_PRESETS[presetKey];
  if (!p) return;

  if (!confirm(t("pop.preset_apply_confirm", { preset: presetKey }))) return;

  // [Phase 1] Click-Freeze 7 필드
  if (qs("cfEnable"))       qs("cfEnable").value       = String(p.click_freeze.enable ? "true" : "false");
  if (qs("cfGyroTh"))       qs("cfGyroTh").value       = String(p.click_freeze.gyro_th);
  if (qs("cfMaxMs"))        qs("cfMaxMs").value        = String(p.click_freeze.max_ms);
  if (qs("cfHoldMs"))       qs("cfHoldMs").value       = String(p.click_freeze.hold_ms);
  if (qs("cfFadeoutMs"))    qs("cfFadeoutMs").value    = String(p.click_freeze.fadeout_ms);
  if (qs("cfMoveTh"))       qs("cfMoveTh").value       = String(p.click_freeze.move_th);
  if (qs("cfFreezeMoveTh")) qs("cfFreezeMoveTh").value = String(p.click_freeze.freeze_move_th);

  // [Phase 2] Adaptive EMA 6 필드
  if (qs("emaAlphaMin"))      qs("emaAlphaMin").value      = String(p.ema.alpha_min);
  if (qs("emaAlphaMax"))      qs("emaAlphaMax").value      = String(p.ema.alpha_max);
  if (qs("emaDeadzoneTh"))    qs("emaDeadzoneTh").value    = String(p.ema.deadzone_th);
  if (qs("emaFastTh"))        qs("emaFastTh").value        = String(p.ema.fast_th);
  if (qs("emaReversalTh"))    qs("emaReversalTh").value    = String(p.ema.reversal_th);
  if (qs("emaReversalReset")) qs("emaReversalReset").value = String(p.ema.reversal_reset ? "true" : "false");

  // [Phase 3] Snap-to-Axis 6 필드
  if (qs("snapEnable"))        qs("snapEnable").value        = String(p.snap.enable ? "true" : "false");
  if (qs("snapMode1"))         qs("snapMode1").checked       = !!(p.snap.mode_mask & 0x01);
  if (qs("snapMode2"))         qs("snapMode2").checked       = !!(p.snap.mode_mask & 0x02);
  if (qs("snapMode3"))         qs("snapMode3").checked       = !!(p.snap.mode_mask & 0x04);
  if (qs("snapAxisMode"))      qs("snapAxisMode").value      = String(p.snap.axis_mode);
  if (qs("snapConfirmFrames")) qs("snapConfirmFrames").value = String(p.snap.confirm_frames);
  if (qs("snapRatio"))         qs("snapRatio").value         = String(p.snap.ratio_enter);
  if (qs("snapStrength"))      qs("snapStrength").value      = String(p.snap.strength);

  uiToConfig();
  await cfgSave();

  const hint = qs("presetHint");
  if (hint) hint.textContent = t("pop.preset_applied", { preset: presetKey });
}
