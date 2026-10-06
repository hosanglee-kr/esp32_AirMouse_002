/* =======================================================
   File: /www/app_0414_0001.js
   Elite AirMouse WebConfig v0414 — UI Binding + Main Entry
   - 로드 순서: 8 (최종)
   - 의존: lib/* 전체 (am_base, am_i18n, am_offline, am_profile,
                         am_macro, am_config, am_status, am_remote)
   ======================================================= */

const MODE_TOOLS_FOLD_KEY = "am_tools_folded_0413";
const TROUBLE_FOLD_KEY = "am_trouble_folded_0413";

/* =======================================================
   1. Drawer (슬라이딩 오프캔버스 네비게이션)
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
      if (tabId === "diag" && typeof refreshDiag === "function") {
        refreshDiag().catch(console.error);
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
   2. Banner (재부팅 배너 제어)
   ======================================================= */
function bindBanner() {
  qs("btnBannerClose")?.addEventListener("click", () => {
    qs("bannerReboot")?.classList.remove("on");
    const banner = qs("bannerReboot");
    if (banner) banner.style.display = "none";
  });
  qs("btnBannerReboot")?.addEventListener("click", () => {
    if (confirm(t("pop.reboot_confirm"))) rebootDevice();
  });
}

/* =======================================================
   3. Quick Tuning (포인터 속도 및 정밀도 빠른 튜닝)
   ======================================================= */
function bindQuickTuning() {
  [1, 2, 3].forEach(v => {
    qs("btnDpi" + v)?.addEventListener("click", async () => {
      const prof = (typeof getActiveProfile === "function") ? getActiveProfile() : (g_profile?.config);
      if (prof?.e10) { prof.e10.dpi_level = v; if (typeof saveOfflineStore === "function") saveOfflineStore(); }
      await apiPostJson("/api/control", { cmd: "set_dpi", level: v, snapshot: true });
      if (typeof pushRecentLog === "function") pushRecentLog(`DPI → ${v}`, true);
      setMsg(t("pop.dpi_applied", { v }), true);
      refreshStatus();
    });
  });

  [0, 1, 2, 3, 4].forEach(v => {
    qs("btnPrec" + v)?.addEventListener("click", async () => {
      const prof = (typeof getActiveProfile === "function") ? getActiveProfile() : (g_profile?.config);
      if (prof?.e10) { prof.e10.precision_mode = v; if (typeof saveOfflineStore === "function") saveOfflineStore(); }
      await apiPostJson("/api/control", { cmd: "set_precision", mode: v, snapshot: true });
      if (typeof pushRecentLog === "function") pushRecentLog(`PREC → ${v}`, true);
      setMsg(t("pop.prec_applied", { v }), true);
      refreshStatus();
    });
  });
}

/* =======================================================
   4. Slots, Macros, Config, Presets, Diag, OTA 바인딩
   ======================================================= */
function bindSlotsAndConfig() {
  /* Slots View Switcher (Global / Mode 1 / Mode 2 / Mode 3) */
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

  /* Macros */
  qs("btnMacroAdd")?.addEventListener("click", macroAdd);
  qs("btnMacroSave")?.addEventListener("click", () => macroSave(false));
  qs("btnMacroReload")?.addEventListener("click", macroReload);

  /* Config */
  qs("btnCfgLoad")?.addEventListener("click", cfgLoad);
  qs("btnCfgSave")?.addEventListener("click", cfgSave);
  qs("btnCfgExport")?.addEventListener("click", cfgExport);
  qs("cfgImportFile")?.addEventListener("change", (e) => {
    const f = e.target.files?.[0];
    e.target.value = "";
    cfgImport(f);
  });
  
  /* Presets */
  qsa("[data-preset]").forEach(b => {
    b.addEventListener("click", () => applyMotionPreset(b.dataset.preset));
  });

  /* Factory Reset */
  qs("btnFactory")?.addEventListener("click", factoryReset);

  /* Diag & Key Test */
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

  /* OTA */
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

  /* Profile Bar / Drawer Profile Select */
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

  /* Virtual Remote & Mode Tabs & Troubleshoot */
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
    if (typeof loadOfflineStore === "function") loadOfflineStore();
    if (typeof initI18n === "function") initI18n();
    
    /* 2. 키코드 로드 (온라인: /api/keycodes, 오프라인: fallback) */
    //   [C-1] loadKeycodes가 g_keycodes를 갱신 → 이후 loadTriggers가 여기서 파생
    if (typeof loadKeycodes === "function") {
      await loadKeycodes();
    }
    if (typeof G_OFFLINE_KEYCODES !== "undefined" && !g_keycodes) {
      g_keycodes = G_OFFLINE_KEYCODES;
    }
    
    /* 3. 트리거 라이브러리 로드 ([C-1] 누락 수정) */
    if (typeof loadTriggers === "function") {
      await loadTriggers();
    } else if (typeof G_OFFLINE_TRIGGERS !== "undefined") {
      g_triggers = G_OFFLINE_TRIGGERS;
    }
    
    /* 4. 프로필 로드 */
    await profileReloadAll();
    

  /* 5. UI 및 인터랙션 바인딩 */
  bindUi();
  initHybridJoystick();

  /* 6. 에디터 및 설정 렌더 */
  renderSlotEditor();
  macroRenderList();
  macroRenderEditor();
  await cfgLoad();
  if (typeof renderRecentLogs === "function") renderRecentLogs();

  /* 7. 활성 모드에 맞게 가상 리모컨 초기 렌더 */
  const prof = (typeof getActiveProfile === "function") ? getActiveProfile() : (g_profile?.config);
  const initMode = prof?.e10?.active_mode || 1;
  applyRemoteMode(initMode);

  /* 8. 상태 폴링 및 넷 모드 표시 */
  _updateNetLabel();
  await refreshStatus();

  /* 9. 백그라운드 주기적 폴링 (2.5초) */
  setInterval(async () => {
    try {
      await refreshStatus();
      if (isTabOn("diag") && !isDiagTyping()) {
        await refreshDiag();
      }
    } catch (e) {
      /* silent */
    }
  }, 2500);
}

/* DOM 로드 완료 시 구동 */
document.addEventListener("DOMContentLoaded", () => {
  main().catch(e => console.error("[main] error:", e));
});
