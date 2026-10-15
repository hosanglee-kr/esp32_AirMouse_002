/* =======================================================
   File: /www/lib/am_config_0415.js
   Elite AirMouse WebConfig v0415 — E10 Config / Control / SafeBoot
   -------------------------------------------------------
   소스명 : am_config_0415.js
   모듈약어 : CFG
   모듈명 : Config Orchestrator (Partition Entry + Load/Save + Preset + Control)
   -------------------------------------------------------
   기능 요약
    - configToUi(cfg) : config → UI 오케스트레이션
      · configToUi_system(cfg, e) + configToUi_motion(cfg, e) 호출
    - uiToConfig()    : UI → config 오케스트레이션
      · uiToConfig_system(cfg) + uiToConfig_motion(cfg) 호출
    - cfgLoad / cfgSave (Wi-Fi patch 확장 포함)
    - bindConfigDirtyTracker (dirty 추적)
    - cfgExport / cfgImport
    - ctlForceRelease / safeBootExit / factoryReset / rebootDevice
    - applyMotionPreset(presetKey)

   [v0415 Critical]
    - ctlSetPpt() 삭제 (set_ppt cmd 삭제, Round G/M)

   [Phase 2.0.1 N-1]
    - deep_idle_timeout_ms 클램프 (System 파티션에 위임)

   [Phase 3.2 H-8]
    - 프리셋 19필드 전체화 (MOTION_PRESETS는 motion 파티션)

   [Phase 3.6 L-5]
    - bindConfigDirtyTracker + clearDirty

   [v0415 rev15 / Wave A]
    - cfgSave() patch 확장: { e10, wifi } → 백엔드 _bumpRebootMaskFromBoot 트리거

   [v0415 Wave D-2]
    - safeModeSet / otaGuardSet 삭제

   [v0415 Wave G]
    - rebootDevice 폴링 개선 (진행 상황 표시)

   [v0415 rev18.1 - Phase 5-B 분할]
    - 본 .js는 3-모듈로 분할됨
      · am_config_motion_0415.js  : Motion Advanced / Precision / Gyro / Linear / Flick / Tilt (B-5)
      · am_config_system_0415.js  : Pointer / Power / Button Timing / Wi-Fi (B-6)
      · am_config_0415.js         : 본 파일 (오케스트레이션 + 나머지)
    - 로드 순서 SSOT:
      am_config_motion → am_config_system → am_config
    - configToUi / uiToConfig 엔트리는 각 파티션에 위임

   [코드 네이밍 규칙]
    - 전역 상수           : G_모듈약어_ 접두사
    - 전역 변수           : g_모듈약어_ 접두사
    - 함수                : camelCase
    - 로컬 변수           : v_ 접두사
   ======================================================= */


/* =======================================================
   Config → UI (오케스트레이션)
   -------------------------------------------------------
    - system 파티션 + motion 파티션 순차 호출
    - 최종 cfgJsonArea 반영
   ======================================================= */
function configToUi(cfg) {
  if (!cfg) return;
  const e = cfg.e10 || {};

  configToUi_system(cfg, e);
  configToUi_motion(cfg, e);

  if (qs("cfgJsonArea")) qs("cfgJsonArea").value = pretty(cfg);
}

/* =======================================================
   UI → Config (오케스트레이션)
   -------------------------------------------------------
    - system 파티션 + motion 파티션 순차 호출
    - cfg / cfg.e10 초기화 보장
   ======================================================= */
function uiToConfig() {
  if (!g_profile || !g_profile.config) g_profile = g_profile || { config: {} };
  const cfg = g_profile.config;
  cfg.e10 = cfg.e10 || {};

  uiToConfig_system(cfg);
  uiToConfig_motion(cfg);

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

  // [v0415 rev15 / Wave A] WiFi도 함께 전송 (백엔드 _bumpRebootMaskFromBoot 트리거)
  const patch = { e10: cfg.e10, wifi: cfg.wifi };
  const u = unwrapApi(await apiPostJson("/api/profiles/active", patch));

  if (!u.ok) {
    setMsg(`${t("cfg.save_fail")}: ${u.msg || u.code}`, false);
    return;
  }

  if (g_profile.config) {
    g_profile.config.e10  = cfg.e10;
    g_profile.config.wifi = cfg.wifi;
  }
  configToUi(g_profile.config);

  setMsg(t("cfg.save_ok_resp", { reloaded: (u.data && u.data.reloaded) ? "yes" : "no" }), true);
  // [L-5] 저장 성공 시 dirty 클리어
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

async function ctlForceRelease() {
  const u = unwrapApi(await apiPostJson("/api/control", {
    cmd: "force_release", snapshot: false
  }));
  if (!u.ok) alert(`${t("pop.test_fail")} ${u.msg || u.code}`);
  else {
    alert(t("pop.force_release_ok"));
    if (typeof pushRecentLog === "function") pushRecentLog("FORCE_RELEASE", true);
  }
  await refreshStatus();
}

/* =======================================================
   SafeBoot / Factory Reset / Reboot
   ======================================================= */
async function safeBootExit() {
  if (!confirm(t("pop.safeboot_exit_confirm"))) return;

  setMsg(t("pop.safeboot_exit_req"), true);

  const u = unwrapApi(await apiPostJson("/api/safeboot", { exit: true }));
  if (!u.ok) {
    alert(`${t("pop.safeboot_exit_fail")} ${u.msg || u.code}`);
    setMsg(t("pop.safeboot_exit_fail"), false);
    return;
  }

  if (typeof pushRecentLog === "function") pushRecentLog("SAFEBOOT_EXIT", true);
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

/* [v0415 Wave D-2] safeModeSet / otaGuardSet 삭제
   - safeModeSet: handleTrouble("safemode")가 직접 처리
   - otaGuardSet: btnOtaGuardApply가 app_0415_0001.js에서 직접 처리
*/

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

/* =======================================================
   [Phase 3.6 L-5] 미저장 경고 + [v0415 Wave G] 폴링 진행 표시
   ======================================================= */
async function rebootDevice() {
  if (typeof hasUnsavedChanges === "function" && hasUnsavedChanges()) {
    if (!confirm(t("pop.unsaved_changes_confirm"))) return;
  }
  if (!confirm(t("pop.reboot_confirm"))) return;

  const u = unwrapApi(await apiPostJson("/api/reboot", { delay_ms: 500 }));
  if (!u.ok) {
    alert(`${t("pop.reboot_fail")} ${u.msg || u.code}`);
    return;
  }

  // ====================================================
  // [v0415 Wave G] 재부팅 진행 폴링 (약 8초)
  // ====================================================
  showLoading(t("pop.reboot_in_progress"));

  let v_tries = 0;
  const v_timer = setInterval(async () => {
    v_tries++;
    try {
      const r = await fetch("/api/status?compact=1", { cache: "no-store" });
      if (r.ok) {
        clearInterval(v_timer);
        hideLoading();
        location.reload();
        return;
      }
    } catch (e) { /* 미부활 시 계속 시도 */ }

    if (v_tries >= 20) {   // 20초 타임아웃
      clearInterval(v_timer);
      hideLoading();
      alert(t("pop.reboot_ap_fallback"));
    }
  }, 1000);
}

/* =======================================================
   [Phase 3.2 H-8] Motion 프리셋 적용 (19필드 전체)
   -------------------------------------------------------
    - MOTION_PRESETS 상수는 am_config_motion_0415.js에 정의
    - 본 함수는 프리셋 값 → DOM 반영 → 저장 오케스트레이션
   ======================================================= */
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

  // [Phase 3] Snap-to-Axis 6 필드 (총 8개 DOM)
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
