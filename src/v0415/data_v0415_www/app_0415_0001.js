/* =======================================================
   File: /www/app_0415_0001.js
   Elite AirMouse WebConfig v0415 — UI Binding + Main Entry
   - 로드 순서: 9 (최종)
   - [v0415] 파일명/헤더만 _0415로 갱신 (본문 v0414 그대로)
   - [Phase 3.1] 배너 dismiss 플래그 연동
   - [Phase 3.6] bindConfigDirtyTracker 호출
   ======================================================= */
   

const MODE_TOOLS_FOLD_KEY = "am_tools_folded_0415";
const TROUBLE_FOLD_KEY = "am_trouble_folded_0415";


/* =======================================================
   1. Drawer
   ======================================================= */
function openDrawer() {
  qs("drawer")?.classList.add("on");
  qs("drawerBackdrop")?.classList.add("on");
}

function closeDrawer() {
  qs("drawer")?.classList.remove("on");
  qs("drawerBackdrop")?.classList.remove("on");
}

function _updateNetLabel() {
  const lbl = qs("drawerNetLabel");
  if (lbl) lbl.textContent = `NET: ${g_appMode}`;
}

function bindDrawer() {
  qs("btnMenu")?.addEventListener("click", openDrawer);
  qs("btnDrawerClose")?.addEventListener("click", closeDrawer);
  qs("drawerBackdrop")?.addEventListener("click", closeDrawer);
  
  qsa(".drawer-item[data-tab]").forEach(btn => {
    btn.addEventListener("click", () => {
      const tabId = btn.getAttribute("data-tab");
      qsa(".drawer-item[data-tab]").forEach(x => x.classList.remove("on"));
      btn.classList.add("on");
      qsa(".tabpane").forEach(x => x.classList.remove("on"));
      qs("tab-" + tabId)?.classList.add("on");
      window.scrollTo({ top: 0, behavior: "smooth" });
      closeDrawer();
      if (tabId === "diag") {
        if (typeof refreshDiag === "function") refreshDiag().catch(console.error);
        if (typeof renderRecentLogs === "function") renderRecentLogs(); // [H-9]
      }
    });
  });
  
  qs("btnDrawerRefresh")?.addEventListener("click", () => {
    refreshStatus();
    closeDrawer();
  });
  
  qs("btnDrawerReboot")?.addEventListener("click", () => {
    if (confirm(t("pop.reboot_confirm"))) rebootDevice();
  });
  
  qs("btnDrawerNetToggle")?.addEventListener("click", () => {
    const target = (g_appMode === APP_MODE_ONLINE) ? APP_MODE_OFFLINE : APP_MODE_ONLINE;
    if (!confirm(t("pop.net_mode_switch_confirm", { current: g_appMode, target }))) return;
    g_appMode = target;
    _updateNetLabel();
    updateAppModeUi();
    setMsg(t("pop.mode_switch_ok", { tgt: g_appMode }), true);
    profileReloadAll().then(() => {
      renderSlotEditor();
      macroRenderList();
      macroRenderEditor();
      cfgLoad();
      refreshStatus();
    });
    closeDrawer();
  });
  
  qs("drawer")?.querySelectorAll("[data-lang]").forEach(b => {
    b.addEventListener("click", () => {
      setLanguage(b.dataset.lang);
      closeDrawer();
    });
  });
}

/* =======================================================
   2. Banner [Phase 3.1 H-4]
   ======================================================= */
function bindBanner() {
  qs("btnBannerClose")?.addEventListener("click", () => {
    const banner = qs("bannerReboot");
    if (banner) {
      banner.style.display = ""; // [H-4] 인라인 초기화
      banner.classList.remove("on");
    }
    const v_mask = (g_lastStatus?.policy?.reboot_reason_mask) || 0;
    if (typeof dismissRebootBanner === "function") dismissRebootBanner(v_mask);
  });
  
  qs("btnBannerReboot")?.addEventListener("click", () => {
    if (confirm(t("pop.reboot_confirm"))) rebootDevice();
  });
}

/* =======================================================
   3. Quick Tuning
   ======================================================= */
function bindQuickTuning() {
  [1, 2, 3].forEach(v => {
    qs("btnDpi" + v)?.addEventListener("click", async () => {
      const prof = (typeof getActiveProfile === "function") ? getActiveProfile() : (g_profile?.config);
      if (prof?.e10) {
        prof.e10.dpi_level = v;
        // [F-5] saveOfflineStore 내부에 이미 모드 가드가 있어 호출부 조건 제거
        if (typeof saveOfflineStore === "function") saveOfflineStore();
      }
      await apiPostJson("/api/control", { cmd: "set_dpi", level: v, snapshot: true });
      if (typeof pushRecentLog === "function") pushRecentLog(`DPI → ${v}`, true);
      setMsg(t("pop.dpi_applied", { v }), true);
      refreshStatus();
    });
  });
  
  [0, 1, 2, 3, 4].forEach(v => {
    qs("btnPrec" + v)?.addEventListener("click", async () => {
      const prof = (typeof getActiveProfile === "function") ? getActiveProfile() : (g_profile?.config);
      if (prof?.e10) {
        prof.e10.precision_mode = v;
        // [F-5] saveOfflineStore 내부에 이미 모드 가드가 있어 호출부 조건 제거
        if (typeof saveOfflineStore === "function") saveOfflineStore();
      }
      await apiPostJson("/api/control", { cmd: "set_precision", mode: v, snapshot: true });
      if (typeof pushRecentLog === "function") pushRecentLog(`PREC → ${v}`, true);
      setMsg(t("pop.prec_applied", { v }), true);
      refreshStatus();
    });
  });
}

/* =======================================================
   4. Slots, Macros, Config, Presets, Diag, OTA
   ======================================================= */
function bindSlotsAndConfig() {
  qsa(".view-tab").forEach(b => b.addEventListener("click", () => {
    qsa(".view-tab").forEach(x => x.classList.remove("on"));
    b.classList.add("on");
    g_view = b.dataset.view || "global";
    renderSlotEditor();
  }));
  
  qs("btnSlotSave")?.addEventListener("click", saveProfile);
  qs("btnSlotReload")?.addEventListener("click", async () => {
    await profileReloadAll();
    renderSlotEditor();
    macroRenderList();
    macroRenderEditor();
  });
  
  qs("btnMacroAdd")?.addEventListener("click", macroAdd);
  qs("btnMacroSave")?.addEventListener("click", () => macroSave(false));
  qs("btnMacroReload")?.addEventListener("click", macroReload);
  
  qs("btnCfgLoad")?.addEventListener("click", cfgLoad);
  qs("btnCfgSave")?.addEventListener("click", cfgSave);
  qs("btnCfgExport")?.addEventListener("click", cfgExport);
  qs("cfgImportFile")?.addEventListener("change", (e) => {
    const f = e.target.files?.[0];
    e.target.value = "";
    cfgImport(f);
  });
  
  qsa("[data-preset]").forEach(b => {
    b.addEventListener("click", () => applyMotionPreset(b.dataset.preset));
  });
  
  qs("btnFactory")?.addEventListener("click", factoryReset);
  
  qs("btnDiagRefresh")?.addEventListener("click", refreshDiag);
  qs("diagFilter")?.addEventListener("input", () => {
    g_diagTypingUntilMs = nowMs() + 1200;
    refreshDiag().catch(console.error);
  });
  qs("btnDiagClear")?.addEventListener("click", diagClear);
  qs("btnKeyTest")?.addEventListener("click", () => {
    const page = qs("ktPage")?.value || "kb";
    const mod = parseIntFlex(qs("ktMod")?.value, 0);
    const code = parseIntFlex(qs("ktCode")?.value, 40);
    keyTest(page, mod, code);
  });
  
  qs("btnOta")?.addEventListener("click", otaUpload);
  qs("btnOtaStatus")?.addEventListener("click", otaStatus);
  qs("btnOtaGuardApply")?.addEventListener("click", async () => {
    const en = !!qs("otaGuardManual")?.checked;
    await apiPostJson("/api/control", { cmd: "set_ota_guard", enable: en, snapshot: true });
    setMsg(en ? t("pop.ota_guard_on") : t("pop.ota_guard_off"), true);
    refreshStatus();
  });
}

/* =======================================================
   5. UI 통합 바인딩
   ======================================================= */
function bindUi() {
  bindDrawer();
  bindBanner();
  bindQuickTuning();
  bindSlotsAndConfig();
  // [Phase 3.6 L-5] Config 폼 dirty 추적 (1회 바인딩)
  if (typeof bindConfigDirtyTracker === "function") bindConfigDirtyTracker();
  
  qs("profSelect")?.addEventListener("change", () => {
    const idx = parseIntFlex(qs("profSelect").value, 0);
    profileSwitch(idx).then(() => {
      cfgLoad();
      const prof = (typeof getActiveProfile === "function") ? getActiveProfile() : (g_profile?.config);
      if (prof?.e10?.active_mode) applyRemoteMode(prof.e10.active_mode);
    });
  });
  
  qs("btnProfReload")?.addEventListener("click", async () => {
    await profileReloadAll();
    renderSlotEditor();
    macroRenderList();
    macroRenderEditor();
    await cfgLoad();
  });
  qs("btnProfCreate")?.addEventListener("click", profileCreate);
  qs("btnProfDelete")?.addEventListener("click", profileDelete);
  qs("btnProfRename")?.addEventListener("click", profileRename);
  
  bindModeTabs();
  bindRemoteFoldToggle();
  bindTroubleshoot();
  bindFoldPersist("modeToolsCard", MODE_TOOLS_FOLD_KEY, false);
  bindFoldPersist("troubleCard", TROUBLE_FOLD_KEY, false);
}

/* =======================================================
   6. Main Entry Point
   ======================================================= */
async function main() {
  /* 1. 오프라인 스토어 및 다국어 초기화 */
  // [F-7] 온라인 모드에서는 스토어 로드 생략 (handleOfflineApi 내부에서 lazy 로드)
  if (g_appMode === APP_MODE_OFFLINE && typeof loadOfflineStore === "function") {
    loadOfflineStore();
  }
  if (typeof initI18n === "function") initI18n();
  
  /* 2. 키코드 로드 [C-1] */
  if (typeof loadKeycodes === "function") await loadKeycodes();
  if (typeof G_OFFLINE_KEYCODES !== "undefined" && !g_keycodes) {
    g_keycodes = G_OFFLINE_KEYCODES;
  }
  
  /* 3. 트리거 로드 [C-1] */
  if (typeof loadTriggers === "function") {
    await loadTriggers();
  } else if (typeof G_OFFLINE_TRIGGERS !== "undefined") {
    g_triggers = G_OFFLINE_TRIGGERS;
  }
  
  /* 4. 프로필 */
  await profileReloadAll();
  
  /* 5. UI 바인딩 */
  bindUi();
  initHybridJoystick();
  
  /* 6. 렌더 */
  renderSlotEditor();
  macroRenderList();
  macroRenderEditor();
  await cfgLoad();
  if (typeof renderRecentLogs === "function") renderRecentLogs();
  
  /* 7. 리모컨 초기 모드 */
  const prof = (typeof getActiveProfile === "function") ? getActiveProfile() : (g_profile?.config);
  const initMode = prof?.e10?.active_mode || 1;
  applyRemoteMode(initMode);
  
  /* 8. 상태 폴링 */
  _updateNetLabel();
  await refreshStatus();
  
  /* 9. 2.5초 주기 폴링 */
  setInterval(async () => {
    try {
      await refreshStatus();
      if (isTabOn("diag") && !isDiagTyping()) {
        await refreshDiag();
      }
    } catch (e) { /* silent */ }
  }, 2500);
}

document.addEventListener("DOMContentLoaded", () => {
  main().catch(e => console.error("[main] error:", e));
});


