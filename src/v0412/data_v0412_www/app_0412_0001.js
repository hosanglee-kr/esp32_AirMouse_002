/* =======================================================
   File: /www/app_0412_0001.js
   Elite AirMouse WebConfig v0412 — UI Binding + Main Entry
   - 로드 순서: 7 (최종)
   - 의존: lib/* 전체
   - [v0412] N-1~N-21 신규 버튼 바인딩 통합
   ======================================================= */

function bindTabs() {
	qsa(".tab").forEach(b => b.addEventListener("click", () => {
		const prevDiag = isTabOn("diag");
		qsa(".tab").forEach(x => x.classList.remove("on"));
		qsa(".tabpane").forEach(x => x.classList.remove("on"));
		b.classList.add("on");
		const pane = qs("tab-" + b.getAttribute("data-tab"));
		if (pane) pane.classList.add("on");
		if (!prevDiag && isTabOn("diag")) refreshDiag().catch(console.error);
	}));
}

function bindViewToggle() {
	qsa(".view-tab").forEach(b => b.addEventListener("click", () => {
		qsa(".view-tab").forEach(x => x.classList.remove("on"));
		b.classList.add("on");
		g_view = b.dataset.view || "global";
		renderSlotEditor();
	}));
}

function bindUi() {
	bindTabs();
	bindViewToggle();

	/* 상단 바 */
	qs("selLang")?.addEventListener("change", (e) => {
		setLanguage(e.target.value);
	});
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
	qs("btnForceRelease")?.addEventListener("click", ctlForceRelease);

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

	/* ====================================================
	   Phase 1+2 (N-1 ~ N-8, N-18) 바인딩
	   ==================================================== */

	/* [N-1] SafeBoot 해제 */
	qs("btnSafeExit")?.addEventListener("click", safeBootExit);

	/* [N-3] Diag 카운터 초기화 */
	qs("btnDiagClear")?.addEventListener("click", diagClear);

	/* [N-4] SafeMode 수동 토글 */
	qs("btnSafeModeEnter")?.addEventListener("click", () => safeModeSet(true));
	qs("btnSafeModeExit")?.addEventListener("click", () => safeModeSet(false));

	/* [N-7] Quick Tuning (DPI / Precision) */
	[1, 2, 3].forEach(v => {
		qs("btnDpi" + v)?.addEventListener("click", () => ctlSetDpi(v));
	});
	[0, 1, 2, 3, 4].forEach(v => {
		qs("btnPrec" + v)?.addEventListener("click", () => ctlSetPrecision(v));
	});

	/* [N-8] Host Cycle */
	qs("btnHostCycle")?.addEventListener("click", ctlHostCycle);

	/* [N-18] I2C 복구 (결과 표시) — 기존 버튼을 개선 버전으로 교체 */
	const v_btnI2c = qs("btnI2cRecover");
	if (v_btnI2c) {
		const v_new = v_btnI2c.cloneNode(true);
		v_btnI2c.parentNode.replaceChild(v_new, v_btnI2c);
		v_new.addEventListener("click", ctlI2cRecoverWithFeedback);
	}

	/* [N-2] OTA Guard 수동 토글 */
	qs("btnOtaGuardApply")?.addEventListener("click", () => {
		const v_on = !!qs("otaGuardManual")?.checked;
		otaGuardSet(v_on);
	});

	/* [N-5] 재부팅 배너 버튼 */
	qs("btnBannerReboot")?.addEventListener("click", rebootDevice);
	qs("btnBannerClose")?.addEventListener("click", () => {
		const banner = qs("bannerReboot");
		if (banner) banner.style.display = "none";
	});

	/* ====================================================
	   Phase 3+4 (N-6 ~ N-21) 바인딩
	   ==================================================== */

	/* [N-9] 단발 키 테스트 */
	qs("btnKeyTest")?.addEventListener("click", () => {
		const v_page = qs("ktPage")?.value || "kb";
		const v_mod = parseIntFlex(qs("ktMod")?.value || "0", 0);
		const v_code = parseIntFlex(qs("ktCode")?.value || "0", 0);
		keyTest(v_page, v_mod, v_code);
	});

	/* [N-19] 자이로 캘리브 (기존 버튼을 개선 버전으로 교체) */
	const v_btnGyro = qs("btnGyroCalib");
	if (v_btnGyro) {
		const v_new = v_btnGyro.cloneNode(true);
		v_btnGyro.parentNode.replaceChild(v_new, v_btnGyro);
		v_new.addEventListener("click", ctlGyroCalibWithFeedback);
	}

	/* [N-11] Config Rollback (비활성 안내) */
	qs("btnCfgRollback")?.addEventListener("click", () => {
		alert("Rollback은 v0412에서 폐기되었습니다.\n프로파일 스위치 또는 Factory Reset을 사용하세요.");
	});

	// ====================================================
	// [Phase 1~3] Motion 프리셋 버튼
	// ====================================================
	qs("btnPresetPC")?.addEventListener("click", () => applyMotionPreset("PC"));
	qs("btnPresetPPT")?.addEventListener("click", () => applyMotionPreset("PPT"));
	qs("btnPresetTV")?.addEventListener("click", () => applyMotionPreset("TV"));
	qs("btnPresetGaming")?.addEventListener("click", () => applyMotionPreset("Gaming"));
	qs("btnPresetPrecision")?.addEventListener("click", () => applyMotionPreset("Precision"));

}

/* =======================================================
   Main
   ======================================================= */
async function main() {
	if (typeof initI18n === "function") initI18n();
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

	/* [C-07] setInterval 오버랩 방지 */
	let v_pollBusy = false;
	setInterval(async () => {
		if (v_pollBusy) return;
		v_pollBusy = true;
		try {
			await refreshStatus();
			const auto = qs("diagAuto")?.checked;
			if (auto && isTabOn("diag") && !isDiagTyping()) {
				await refreshDiag();
			}
		} catch (e) { }
		finally { v_pollBusy = false; }
	}, 2500);
}

main().catch(e => {
	const el = qs("statusJson");
	if (el) el.textContent = String(e?.stack || e);
});

