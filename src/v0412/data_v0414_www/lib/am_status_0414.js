/* =======================================================
   File: /www/lib/am_status_0414.js
   Elite AirMouse WebConfig v0414 — Status / Diag / OTA
   - 로드 순서: 7
   - [Phase 2.1 C-3/H-5] keyTest 통합 (am_base 버전 흡수)
   - [Phase 3.1 H-4] 배너 dismiss 세션 플래그
   - [Phase 3.4 M-10] 캘리브 피드백 통일
   - [Phase 3.7 L-7] 배터리 N/A 처리
   - [Phase 4.2 M-9] reboot/check 폴링 중복 제거
   ======================================================= */

/* =======================================================
   [Phase 3.1 H-4] 배너 dismiss 세션 플래그
   ======================================================= */
let g_bannerDismissed = false;
let g_bannerDismissedMask = 0;

function dismissRebootBanner(maskSnapshot) {
  g_bannerDismissed = true;
  g_bannerDismissedMask = (maskSnapshot >>> 0);
}

/* =======================================================
   Status 새로고침
   ======================================================= */
async function refreshStatus() {
  updateAppModeUi();

  const r = await apiGet("/api/status?compact=1");
  const u = unwrapApi(r);
  if (!u.ok || !u.data) return;

  g_lastStatus = u.data;
  const g = u.data.groups || {};
  const sys = g.sys || {};
  const mem = g.mem || {};
  const net = g.net || {};
  const e10 = g.e10 || {};
  const boot = g.boot || {};
  const cfg = g.config || {};

  if (qs("stUptime")) qs("stUptime").textContent = `${sys.uptime_ms ?? "-"} ms`;
  if (qs("stHeap")) qs("stHeap").textContent = `${mem.heap_free ?? "-"} B`;
  if (qs("stWiFi")) qs("stWiFi").textContent = `${net.mode ?? "-"} / ${net.ssid ?? "-"}`;
  if (qs("stProfile")) {
    qs("stProfile").textContent =
      `#${cfg.profile_idx ?? "-"} ${cfg.profile_name ?? ""} / 총 ${cfg.profile_count ?? "-"}개`;
  }

  setPill(qs("pillBle"), `BLE: ${e10.ble_connected ? "ON" : "OFF"}`, !!e10.ble_connected);
  setPill(qs("pillSafe"), `SAFE: ${boot.safe_mode ? "ON" : "OFF"}`, !!boot.safe_mode);

  // 상단 통합 알약
  const pProf = qs("pStatProf"), pBat = qs("pStatBat"), pBle = qs("pStatBle");
  if (pProf) pProf.textContent = `#${cfg.profile_idx ?? 0}`;

  // [Phase 3.7 L-7] 배터리 ADC 미구현 → "N/A" + 툴팁
  if (pBat) {
    const v_pct = (sys.bat_pct != null) ? sys.bat_pct : null;
    if (v_pct != null) {
      pBat.textContent = `${v_pct}%`;
      pBat.style.color = "";
      pBat.removeAttribute("title");
    } else {
      pBat.textContent = "N/A";
      pBat.style.color = "var(--txt-muted)";
      pBat.title = (typeof t === "function") ? t("dash.batt_na_tip") : "Battery ADC not implemented";
    }
  }

  if (pBle) {
    pBle.textContent = e10.ble_connected ? "BLE ON" : "BLE OFF";
    pBle.style.color = e10.ble_connected ? "var(--success)" : "var(--txt-muted)";
  }

  // [N-5] 재부팅 배너
  _updateRebootBanner(u.data.policy || {});

  // [N-7] Quick Tuning 활성 상태
  _updateQuickTuningActive(e10.dpi_level, e10.precision_mode);

  // [N-2] OTA Guard 상태 반영
  const v_guard = !!(e10.gate && e10.gate.ota_guard);
  const v_guardEl = qs("otaGuardManual");
  if (v_guardEl) v_guardEl.checked = v_guard;

  // [M-9] _updateRebootCheckAsync() 호출 제거
  //   - /api/status 응답의 policy.reboot_required / reboot_reason_mask로 충분
  //   - 별도 /api/reboot/check 조회는 중복 트래픽
}

/* =======================================================
   [N-5, H-4] 재부팅 배너
   ======================================================= */
function _updateRebootBanner(policy) {
  const banner = qs("bannerReboot");
  if (!banner) return;

  const v_req     = !!(policy && policy.reboot_required);
  const v_reasons = (policy && policy.reboot_reasons) || "";
  const v_mask    = ((policy && policy.reboot_reason_mask) || 0) >>> 0;

  // 재부팅 불필요 → 숨김 + dismiss 초기화
  if (!v_req) {
    banner.style.display = "none";
    g_bannerDismissed = false;
    g_bannerDismissedMask = 0;
    return;
  }

  // 사용자가 같은 마스크로 dismiss한 상태 → 숨김 유지
  if (g_bannerDismissed && v_mask === g_bannerDismissedMask) {
    banner.style.display = "none";
    return;
  }

  // 마스크가 바뀌면 dismiss 무효화
  if (g_bannerDismissed && v_mask !== g_bannerDismissedMask) {
    g_bannerDismissed = false;
    g_bannerDismissedMask = 0;
  }

  banner.style.display = "flex";
  const rEl = qs("bannerRebootReason");
  if (rEl) rEl.textContent = v_reasons || "(WiFi 변경 등)";

  // [M-9] banner.title에 mask 정보 (구 _updateRebootCheckAsync 역할)
  banner.title =
    `mask: 0x${v_mask.toString(16)}\n` +
    `reasons: ${v_reasons || "(none)"}`;
}

/* =======================================================
   [N-7] Quick Tuning 활성 상태
   ======================================================= */
function _updateQuickTuningActive(dpi, prec) {
  [1, 2, 3].forEach(v => {
    const el = qs("btnDpi" + v);
    if (el) el.classList.toggle("on", (dpi === v));
  });
  [0, 1, 2, 3, 4].forEach(v => {
    const el = qs("btnPrec" + v);
    if (el) el.classList.toggle("on", (prec === v));
  });
}

/* =======================================================
   [N-3] Diag 카운터 초기화
   ======================================================= */
async function diagClear() {
  if (!confirm(t("pop.diag_reset_confirm"))) return;

  const u = unwrapApi(await apiPostJson("/api/diag/clear", {}));
  if (!u.ok) {
    alert(`${t("pop.diag_reset_fail")} ${u.msg || u.code}`);
    return;
  }
  setMsg(t("diag.reset_ok"), true);
  await refreshDiag();
}

/* =======================================================
   [N-7] 실시간 DPI / Precision
   ======================================================= */
async function ctlSetDpi(level) {
  const u = unwrapApi(await apiPostJson("/api/control", {
    cmd: "set_dpi", level: level, snapshot: true
  }));
  if (!u.ok) alert(`DPI fail: ${u.msg || u.code}`);
  else setMsg(t("pop.dpi_applied", { v: level }), true);
  await refreshStatus();
}

async function ctlSetPrecision(mode) {
  const u = unwrapApi(await apiPostJson("/api/control", {
    cmd: "set_precision", mode: mode, snapshot: true
  }));
  if (!u.ok) alert(`Precision fail: ${u.msg || u.code}`);
  else setMsg(t("pop.prec_applied", { v: mode }), true);
  await refreshStatus();
}

/* =======================================================
   [N-8] Host Cycle
   ======================================================= */
async function ctlHostCycle() {
  if (!confirm(t("pop.host_cycle_confirm"))) return;

  const u = unwrapApi(await apiPostJson("/api/action/test", {
    k: 9, h: 0, p16: 5, p32: 0
  }));
  if (!u.ok) alert(`${t("pop.host_cycle_fail")} ${u.msg || u.code}`);
  else setMsg(t("pop.host_cycle_ok"), true);
}

/* =======================================================
   [N-18] I2C 복구
   ======================================================= */
async function ctlI2cRecoverWithFeedback() {
  if (!confirm(t("pop.i2c_recover_confirm"))) return;

  const v_beforeCnt = (g_lastStatus
    && g_lastStatus.groups
    && g_lastStatus.groups.e10
    && g_lastStatus.groups.e10.i2c
    && g_lastStatus.groups.e10.i2c.recover_count) || 0;

  const u = unwrapApi(await apiPostJson("/api/control", {
    cmd: "i2c_recover", snapshot: false
  }));
  if (!u.ok) {
    alert(`${t("pop.i2c_recover_fail")} ${u.msg || u.code}`);
    return;
  }

  setMsg(t("trouble.i2c_progress"), true);

  setTimeout(async () => {
    await refreshStatus();
    const v_afterCnt = (g_lastStatus
      && g_lastStatus.groups
      && g_lastStatus.groups.e10
      && g_lastStatus.groups.e10.i2c
      && g_lastStatus.groups.e10.i2c.recover_count) || 0;
    const v_lastOk = !!(g_lastStatus
      && g_lastStatus.groups
      && g_lastStatus.groups.e10
      && g_lastStatus.groups.e10.i2c
      && g_lastStatus.groups.e10.i2c.recover_last_ok);

    if (v_afterCnt > v_beforeCnt) {
      if (v_lastOk) {
        setMsg(t("trouble.i2c_success", { cnt: v_afterCnt }), true);
      } else {
        setMsg(t("trouble.i2c_fail", { cnt: v_afterCnt }), false);
        alert(t("pop.i2c_recover_hardware_warn"));
      }
    } else {
      setMsg(t("trouble.i2c_sent"), true);
    }
  }, 3000);
}

/* =======================================================
   [Phase 2.1 C-3/H-5] Key Test — am_base 버전 흡수
   ======================================================= */
async function keyTest(page, mod, code, labelOverride) {
  // 1) 로컬 부가 로그 (Recent Command Log)
  const label = labelOverride || ((page === "kb")
    ? `KB u=${code} mod=${mod}`
    : `Consumer 0x${(code >>> 0).toString(16).toUpperCase()}`);
  if (typeof pushRecentLog === "function") {
    pushRecentLog(label, true);
  }

  // 2) diagEvents 라인 삽입 (개발자 관측용)
  const evEl = qs("diagEvents");
  if (evEl) {
    const line = document.createElement("div");
    line.className = "logline";
    line.textContent = `[${Math.floor(performance.now())}ms] KEYTEST ${label}`;
    evEl.insertBefore(line, evEl.firstChild);
    while (evEl.children.length > 60) evEl.removeChild(evEl.lastChild);
  }

  // 3) 코드 유효성 검사
  if (!Number.isFinite(code) || code < 0) {
    alert(t("trouble.invalid_code"));
    return;
  }

  // 4) 서버 요청 (오프라인 mock 포함)
  const u = unwrapApi(await apiPostJson("/api/ppt/test", {
    page: page, mod: mod, code: code
  }));
  if (!u.ok) {
    setMsg(`${t("pop.key_test_fail")} ${u.msg || u.code}`, false);
    alert(`${t("pop.key_test_fail")} ${u.msg || u.code}`);
    return;
  }
  setMsg(`Key Test OK · ${page} mod=${mod} code=${code}`, true);
}

/* =======================================================
   [Phase 3.4 M-10] 자이로 캘리브 피드백
   ======================================================= */
async function ctlGyroCalibWithFeedback() {
  if (!confirm(t("pop.gyro_calib_confirm"))) return;

  const v_beforeRms = _getE10GyroRms();

  const u = unwrapApi(await apiPostJson("/api/control", {
    cmd: "gyro_calib", snapshot: false
  }));
  if (!u.ok) {
    alert(`${t("pop.gyro_calib_fail")} ${u.msg || u.code}`);
    return;
  }

  // [M-10] 최근 로그 기록 (문제해결 카드 경로에서도 반영)
  if (typeof pushRecentLog === "function") pushRecentLog("GYRO_CALIB", true);

  showLoading(t("loading.processing"));

  setTimeout(async () => {
    await refreshStatus();
    hideLoading();

    const v_afterRms = _getE10GyroRms();
    const v_bias = _getE10Bias();
    const isEn = (typeof g_currLang !== "undefined" && g_currLang === "en");

    const v_prefix = isEn ? "Gyro calibration complete" : "자이로 영점 조절 완료";
    const v_msg =
      `${v_prefix} · RMS ${v_beforeRms.toFixed(2)} → ${v_afterRms.toFixed(2)} deg/s\n` +
      `Bias: (${v_bias.x.toFixed(2)}, ${v_bias.y.toFixed(2)}, ${v_bias.z.toFixed(2)})`;

    setMsg(v_msg, true);
    console.info("[N-19] calibration result", {
      before: v_beforeRms, after: v_afterRms, bias: v_bias
    });
  }, 2000);
}

function _getE10GyroRms() {
  return (g_lastStatus
    && g_lastStatus.groups
    && g_lastStatus.groups.e10
    && g_lastStatus.groups.e10.gyro
    && g_lastStatus.groups.e10.gyro.rms) || 0;
}

function _getE10Bias() {
  const g = (g_lastStatus
    && g_lastStatus.groups
    && g_lastStatus.groups.e10
    && g_lastStatus.groups.e10.gyro) || {};
  return { x: g.bias_x || 0, y: g.bias_y || 0, z: g.bias_z || 0 };
}

/* =======================================================
   [Phase 4.2 M-9] reboot/check (수동 조회 API만 유지)
   - refreshStatus에서 주기 호출 제거됨
   - 필요 시 개발자 콘솔에서 rebootCheck() 수동 호출 가능
   ======================================================= */
async function rebootCheck() {
  const u = unwrapApi(await apiGet("/api/reboot/check"));
  return (u.ok && u.data) ? u.data : null;
}

/* =======================================================
   Tab / Diag 헬퍼
   ======================================================= */
function isTabOn(name) {
  const el = qs("tab-" + name);
  return !!(el && el.classList.contains("on"));
}

function isDiagTyping() {
  const f = qs("diagFilter");
  if (!f) return false;
  return (document.activeElement === f) || (nowMs() < g_diagTypingUntilMs);
}

/* =======================================================
   Diag 렌더
   ======================================================= */
function renderDiag(u) {
  const data = u.data || {};
  const d = data.diag || {};
  const events = Array.isArray(data.events) ? data.events : [];

  const cEl = qs("diagCounters");
  if (cEl) {
    cEl.innerHTML = "";
    const items = [
      ["body_too_large", d.body_too_large],
      ["no_body_slot", d.no_body_slot],
      ["bad_json", d.json_bad],
      ["safe_blocked", d.safe_blocked],
      ["ota_blocked", d.ota_blocked]
    ];
    for (const [k, v] of items) {
      const div = document.createElement("div");
      div.className = "pill";
      div.textContent = `${k}: ${v ?? 0}`;
      cEl.appendChild(div);
    }
  }

  const listEl = qs("diagEvents");
  if (listEl) {
    listEl.innerHTML = "";
    const f = (qs("diagFilter")?.value || "").trim();
    const shown = events.slice().reverse()
      .filter(ev => !f || String(ev.code || "").includes(f))
      .slice(0, 80);

    for (const ev of shown) {
      const div = document.createElement("div");
      div.className = "logline";
      div.textContent = `[${ev.ms ?? 0}ms] ${ev.code ?? "-"}`;
      listEl.appendChild(div);
    }

    if (!shown.length) {
      const div = document.createElement("div");
      div.className = "hint2";
      div.textContent = "No events.";
      listEl.appendChild(div);
    }
  }

  const rawEl = qs("diagJson");
  if (rawEl) rawEl.textContent = pretty(u);
}

async function refreshDiag() {
  const r = await apiGet("/api/diag");
  renderDiag(unwrapApi(r));
}

/* =======================================================
   OTA 업로드 / 상태
   ======================================================= */
async function otaUpload() {
  const f = qs("otaFile")?.files?.[0];
  if (!f) { alert(t("pop.file_select_req")); return; }

  qs("otaHint").textContent = `uploading: ${f.name}...`;
  const r = await fetch("/api/ota", { method: "POST", body: f });
  qs("otaHint").textContent = await r.text();
  await otaStatus();
}

async function otaStatus() {
  const r = await apiGet("/api/ota/status");
  qs("otaJson").textContent = pretty(unwrapApi(r).data || r.text);
}

/* =======================================================
   최근 명령 로그
   ======================================================= */
const RECENT_LOGS_MAX = 8;
const g_recentLogs = [];

function pushRecentLog(cmd, ok) {
  const tStr = new Date().toLocaleTimeString("ko-KR", { hour12: false });
  g_recentLogs.unshift({ t: tStr, cmd, ok });
  if (g_recentLogs.length > RECENT_LOGS_MAX) g_recentLogs.pop();
  renderRecentLogs();
}

function renderRecentLogs() {
  const el = qs("recentLogList"); if (!el) return;
  if (!g_recentLogs.length) {
    el.innerHTML = `<span class="hint2">${t("log.none")}</span>`;
    return;
  }
  el.innerHTML = g_recentLogs.map(l =>
    `<div class="recent-log-item ${l.ok ? "good" : "bad"}"><span class="rlog-t">${l.t}</span><span class="rlog-cmd">${l.cmd}</span></div>`
  ).join("");
}

/* =======================================================
   문제 해결 도구
   - [Phase 3.4 M-10] calib → ctlGyroCalibWithFeedback()
   - [Phase 1.5 H-6] safemode/safeinfo/safeexit 서버 상태 기반
   - [Phase 1.4 C-5] factory → factoryReset()
   ======================================================= */
async function handleTrouble(kind) {
  try {
    switch (kind) {
      case "calib":
        // [M-10] N-19 피드백 통합 (RMS 변화량 + bias 좌표)
        await ctlGyroCalibWithFeedback();
        break;

      case "release":
        await apiPostJson("/api/control", { cmd: "force_release", snapshot: false });
        setMsg(t("pop.force_release_ok"), true);
        pushRecentLog("FORCE_RELEASE", true);
        break;

      case "host":
        await apiPostJson("/api/action/test", { k: 9, h: 0, p16: 5, p32: 0 });
        setMsg(t("pop.host_cycle_ok"), true);
        pushRecentLog("HOST_CYCLE", true);
        break;

      case "pair":
        await apiPostJson("/api/action/test", { k: 9, h: 0, p16: 4, p32: 0 });
        setMsg(t("pop.pair_ok"), true);
        pushRecentLog("PAIRING", true);
        break;

      case "i2c":
        await apiPostJson("/api/control", { cmd: "i2c_recover", snapshot: false });
        setMsg(t("pop.i2c_ok"), true);
        pushRecentLog("I2C_RECOVER", true);
        break;

      case "sleep":
        await apiPostJson("/api/action/test", { k: 9, h: 0, p16: 2, p32: 0 });
        setMsg(t("pop.sleep_ok"), true);
        pushRecentLog("SLEEP_NOW", true);
        break;

      case "factory":
        // [C-5, L-8] config 탭과 동일한 factoryReset()으로 위임
        await factoryReset();
        break;

      case "safemode": {
        // [H-6, D-2=(B)] 서버 상태를 진실 공급원으로 사용
        const v_current = !!(g_lastStatus
          && g_lastStatus.groups
          && g_lastStatus.groups.boot
          && g_lastStatus.groups.boot.safe_mode);
        const v_target = !v_current;

        const msg = v_target ? t("pop.safemode_enter_confirm") : t("pop.safemode_exit_confirm");
        if (!confirm(msg)) return;

        const u = unwrapApi(await apiPostJson("/api/control", {
          cmd: "set_safe_mode", enable: v_target, snapshot: true
        }));
        if (!u.ok) {
          alert(`${t("pop.safemode_change_fail")} ${u.msg || u.code}`);
          return;
        }
        setMsg(v_target ? t("pop.safemode_on") : t("pop.safemode_off"), true);
        pushRecentLog(`SAFE_MODE_${v_target ? "ON" : "OFF"}`, true);
        await refreshStatus();
        break;
      }

      case "safeinfo": {
        const v_safe = !!(g_lastStatus
          && g_lastStatus.groups
          && g_lastStatus.groups.boot
          && g_lastStatus.groups.boot.safe_mode);
        const v_fail = (g_lastStatus?.groups?.boot?.fail_count) ?? 0;
        const v_pending = !!(g_lastStatus?.groups?.boot?.pending);

        alert(pretty({
          safe_mode: v_safe,
          fail_count: v_fail,
          pending: v_pending,
          net_mode: g_appMode,
          note: "SafeMode is server-authoritative (boot_state)."
        }));
        break;
      }

      case "safeexit": {
        // [H-6, D-2=(B)] /api/safeboot exit 사용
        if (!confirm(t("pop.safeboot_exit_confirm"))) return;

        const u = unwrapApi(await apiPostJson("/api/safeboot", { exit: true }));
        if (!u.ok) {
          alert(`${t("pop.safeboot_exit_fail")} ${u.msg || u.code}`);
          return;
        }
        pushRecentLog("SAFEBOOT_EXIT", true);
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
        break;
      }
    }
  } catch (e) {
    setMsg(`${t("pop.trouble_fail")} ${e.message || e}`, false);
  }
}

function bindTroubleshoot() {
  qsa("[data-trouble]").forEach(btn => {
    btn.addEventListener("click", () => handleTrouble(btn.getAttribute("data-trouble")));
  });
}

/* =======================================================
   Online / Offline 모드 전환
   ======================================================= */
function toggleAppMode() {
  const targetMode = (g_appMode === APP_MODE_ONLINE) ? APP_MODE_OFFLINE : APP_MODE_ONLINE;
  if (!confirm(t("pop.net_mode_switch_confirm", { current: g_appMode, target: targetMode }))) return;

  setAppMode(targetMode);
  setMsg(t("pop.mode_switch_ok", { tgt: g_appMode }), true);

  profileReloadAll().then(() => {
    renderSlotEditor();
    macroRenderList();
    macroRenderEditor();
    cfgLoad();
    refreshStatus();
  });
}

