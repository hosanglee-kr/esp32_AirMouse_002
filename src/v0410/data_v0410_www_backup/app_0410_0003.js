/* =======================================================
   File: /www/app_0410_0003.js
   Elite AirMouse WebConfig v0410 — UI Binding + Main Entry
   - 로드 순서: 7 (최종)
   - 의존: lib/* 전체 (am_base, am_offline, am_profile,
                       am_macro, am_config, am_status)
   ======================================================= */

/* =======================================================
   탭 바인딩
   ======================================================= */
function bindTabs() {
	qsa(".tab").forEach(b => b.addEventListener("click", () => {
		const prevDiag = isTabOn("diag");
		
		qsa(".tab").forEach(x => x.classList.remove("on"));
		qsa(".tabpane").forEach(x => x.classList.remove("on"));
		
		b.classList.add("on");
		const pane = qs("tab-" + b.getAttribute("data-tab"));
		if (pane) pane.classList.add("on");
		
		if (!prevDiag && isTabOn("diag")) {
			refreshDiag().catch(console.error);
		}
	}));
}

/* =======================================================
   Global / Mode 뷰 토글 바인딩
   ======================================================= */
function bindViewToggle() {
	qsa(".view-tab").forEach(b => b.addEventListener("click", () => {
		qsa(".view-tab").forEach(x => x.classList.remove("on"));
		b.classList.add("on");
		g_view = b.dataset.view || "global";
		renderSlotEditor();
	}));
}

/* =======================================================
   전체 UI 바인딩
   ======================================================= */
function bindUi() {
	bindTabs();
	bindViewToggle();
	
	/* 상단 바 */
	qs("pillNet")?.addEventListener("click", toggleAppMode);
	qs("btnRefresh")?.addEventListener("click", refreshStatus);
	qs("btnReboot")?.addEventListener("click", rebootDevice);
	
	/* ---- Profile ---- */
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
	qs("profSelect")?.addEventListener("change", () => {
		const idx = parseIntFlex(qs("profSelect").value, 0);
		profileSwitch(idx).then(() => cfgLoad());
	});
	
	/* ---- Slots ---- */
	qs("btnSlotSave")?.addEventListener("click", saveProfile);
	qs("btnSlotReload")?.addEventListener("click", async () => {
		await profileReloadAll();
		renderSlotEditor();
		macroRenderList();
		macroRenderEditor();
	});
	
	/* ---- Macros ---- */
	qs("btnMacroAdd")?.addEventListener("click", macroAdd);
	qs("btnMacroSave")?.addEventListener("click", () => macroSave(false));
	qs("btnMacroReload")?.addEventListener("click", macroReload);
	
	/* ---- Config ---- */
	qs("btnCfgLoad")?.addEventListener("click", cfgLoad);
	qs("btnCfgSave")?.addEventListener("click", cfgSave);
	qs("btnCfgExport")?.addEventListener("click", cfgExport);
	qs("cfgImportFile")?.addEventListener("change", (e) => {
		const f = e.target.files?.[0];
		e.target.value = "";
		cfgImport(f);
	});
	
	/* ---- Quick Control ---- */
	qs("btnCtlPptOn")?.addEventListener("click", () => ctlSetPpt(true));
	qs("btnCtlPptOff")?.addEventListener("click", () => ctlSetPpt(false));
	qs("btnGyroCalib")?.addEventListener("click", ctlGyroCalib);
	qs("btnForceRelease")?.addEventListener("click", ctlForceRelease);
	qs("btnI2cRecover")?.addEventListener("click", ctlI2cRecover);
	
	/* ---- SafeBoot / Factory ---- */
	qs("btnSafeInfo")?.addEventListener("click", safeInfo);
	qs("btnFactory")?.addEventListener("click", factoryReset);
	
	/* ---- OTA ---- */
	qs("btnOta")?.addEventListener("click", otaUpload);
	qs("btnOtaStatus")?.addEventListener("click", otaStatus);
	
	/* ---- Diag ---- */
	qs("btnDiagRefresh")?.addEventListener("click", refreshDiag);
	qs("diagFilter")?.addEventListener("input", () => {
		g_diagTypingUntilMs = nowMs() + 1200;
		refreshDiag().catch(console.error);
	});
}

/* =======================================================
   Main
   ======================================================= */
async function main() {
	updateAppModeUi();
	bindUi();
	
	await loadKeycodes();
	await loadTriggers();
	await profileReloadAll();
	renderSlotEditor();
	macroRenderList();
	macroRenderEditor();
	await cfgLoad();
	await refreshStatus();
	
	/* 주기적 상태 새로고침 (2.5s) */
	setInterval(() => {
		refreshStatus().catch(() => {});
		const auto = qs("diagAuto")?.checked;
		if (auto && isTabOn("diag") && !isDiagTyping()) {
			refreshDiag().catch(() => {});
		}
	}, 2500);
}

main().catch(e => {
	const el = qs("statusJson");
	if (el) el.textContent = String(e?.stack || e);
});
