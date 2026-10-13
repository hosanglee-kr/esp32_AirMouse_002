/* =======================================================
   File: /www/lib/am_status_0415.js
   Elite AirMouse WebConfig v0415 — Status / Diag / OTA
   - 로드 순서: 7
   - [v0415] 파일명/헤더만 _0415로 갱신 (본문 v0414 그대로)
   - [Phase 2.1 C-3/H-5] keyTest 통합
   - [Phase 3.1 H-4] 배너 dismiss 세션 플래그
   - [Phase 3.4 M-10] 캘리브 피드백 통일
   - [Phase 3.7 L-7] 배터리 N/A 처리
   - [Phase 4.2 M-9] reboot/check 폴링 중복 제거
   - [사전 fix] refreshStatus에 서버 active_mode 폴링 반영
   - [v0415 Wave D-2] ctlSetDpi / ctlSetPrecision / rebootCheck 삭제
   - [v0415 Wave F] BLE pill에 peer 이름 표시
   - [v0415 Wave G] 배터리 pill 숨김 (ADC 미구현)
   - [v0415 Wave H] 진단 health 카드 (P2-1) + OTA 버전 정보 (P2-5)
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

  // ====================================================
  // [v0415 Wave G] 배터리 pill 숨김 (ADC 하드웨어 미구현, P1-4)
  //   - 사용자 지시: N/A 표시 대신 pill 자체 숨김
  //   - 구분자(·)도 함께 숨겨 시각적 어색함 제거
  //   - 실 배터리 지원 시점(v0416+)에 복원 예정
  // ====================================================
  if (pBat) {
    const v_pct = (sys.bat_pct != null) ? sys.bat_pct : null;
    if (v_pct != null) {
      // 배터리 값이 있으면 표시 (v0416 대비 미리 지원)
      pBat.textContent = `${v_pct}%`;
      pBat.style.display = "";
      pBat.style.color = "";
      const v_prevSep = pBat.previousElementSibling;
      if (v_prevSep && v_prevSep.classList.contains("pm-sep")) {
        v_prevSep.style.display = "";
      }
    } else {
      // 미지원 → pill + 앞 구분자 숨김
      pBat.style.display = "none";
      const v_prevSep = pBat.previousElementSibling;
      if (v_prevSep && v_prevSep.classList.contains("pm-sep")) {
        v_prevSep.style.display = "none";
      }
    }
  }

  // ====================================================
  // [v0415 Wave F] BLE pill에 peer 이름 표시 (P0-2)
  //   - 연결 시: "🔗 {peer_name}" (없으면 "BLE ON")
  //   - 미연결 시: "BLE OFF" + 클릭 액션(페어링 시작)
  // ====================================================
  if (pBle) {
    const v_peer = (e10.ble_peer_name || "").trim();
    if (e10.ble_connected) {
      if (v_peer.length > 0) {
        // 16자 제한 (긴 이름 ellipsis)
        const v_short = v_peer.length > 16 ? v_peer.slice(0, 15) + "…" : v_peer;
        pBle.textContent = `🔗 ${v_short}`;
        pBle.title = `연결됨: ${v_peer}`;
      } else {
        pBle.textContent = "BLE ON";
        pBle.title = "BLE 연결됨";
      }
      pBle.style.color = "var(--success)";
      pBle.style.cursor = "default";
      pBle.onclick = null;
    } else {
      pBle.textContent = "BLE OFF";
      pBle.style.color = "var(--txt-muted)";
      pBle.title = "클릭하여 페어링 모드 진입";
      pBle.style.cursor = "pointer";
      pBle.onclick = () => {
        if (typeof handleTrouble === "function") handleTrouble("pair");
      };
    }
  }

  // [N-5] 재부팅 배너
  _updateRebootBanner(u.data.policy || {});

  // [N-7] Quick Tuning 활성 상태
  _updateQuickTuningActive(e10.dpi_level, e10.precision_mode);

  // ====================================================
  // [사전 fix] 서버 active_mode를 프론트 SSOT로 반영
  // ----------------------------------------------------
  // 문제: 하드코딩 더블클릭(Side C × 2), MODE_CYCLE, set_mode 등
  //       어떤 경로로 변경되어도 프론트 UI가 갱신 안 됨.
  //       g_currentActiveMode는 부팅 시 초기값 유지 → stale.
  //
  // 해결: 2.5초 폴링(refreshStatus) 시 서버 값과 비교 →
  //       다르면 g_currentActiveMode 갱신 + UI 재렌더.
  //
  // 영향: 홈화면 카드 layout(mode-pc/ppt/tv), 배지, 탭, 도구 모두 동기화.
  // ====================================================
  {
    const v_serverMode = e10.active_mode;
    if (Number.isFinite(v_serverMode) && v_serverMode >= 1 && v_serverMode <= 3) {
      if (g_currentActiveMode !== v_serverMode) {
        g_currentActiveMode = v_serverMode;
        if (typeof _renderController === "function") _renderController(v_serverMode);
        if (typeof _updateModeTabs === "function")   _updateModeTabs();
        if (typeof renderModeTools === "function")   renderModeTools();
      }
    }
  }

  // ====================================================
  // [v0415 Wave H] 진단 health 카드 갱신 (P2-1)
  //   + OTA 탭 버전 정보 카드 갱신 (P2-5)
  // ====================================================
  _renderHealthCard(e10, sys, cfg);

  // [N-2] OTA Guard 상태 반영
  const v_guard = !!(e10.gate && e10.gate.ota_guard);
  const v_guardEl = qs("otaGuardManual");
  if (v_guardEl) v_guardEl.checked = v_guard;
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

  // [H-4] 클래스 기반 통일 (인라인 style 제거)
  const _showBanner = (show) => {
    banner.style.display = ""; // 인라인 초기화
    banner.classList.toggle("on", !!show);
  };
  
  if (!v_req) {
    _showBanner(false);
    g_bannerDismissed = false;
    g_bannerDismissedMask = 0;
    return;
  }
  
  if (g_bannerDismissed && v_mask === g_bannerDismissedMask) {
    _showBanner(false);
    return;
  }
  
  if (g_bannerDismissed && v_mask !== g_bannerDismissedMask) {
    g_bannerDismissed = false;
    g_bannerDismissedMask = 0;
  }
  
  _showBanner(true);
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
  // [v0415 rev15 / Wave C] Config 폼의 precMode select 동기화
  const pm = qs("precMode");
  if (pm && Number.isFinite(prec) && (prec >= 0 && prec <= 4)) {
    pm.value = String(prec);
  }
}

/* =======================================================
   [v0415 Wave H] 진단 요약 카드 렌더 (P2-1)
   -------------------------------------------------------
   - /api/status.groups.e10.health.{state,score} UI화
   - state: 0=OK, 1=WARN, 2=DEGRADED
   - score: 0~1000
   - 메타: 센서/BLE/가동시간/온도
   ======================================================= */
function _renderHealthCard(e10, sys, cfg) {
  const card  = qs("healthCard");
  const icon  = qs("healthIcon");
  const score = qs("healthScore");
  const state = qs("healthState");
  const fill  = qs("healthBarFill");
  if (!card || !score) return;

  // 서버 응답에서 읽기
  const h = e10.health || {};
  const v_stateVal = Number.isFinite(h.state) ? h.state : 0;   // 0=OK, 1=WARN, 2=DEGRADED
  const v_scoreVal = Number.isFinite(h.score) ? h.score : 1000;

  // 상태 → 클래스/아이콘/라벨
  let v_cls = "ok";
  let v_icon = "✅";
  let v_label = t("diag.health_ok");
  if (v_stateVal === 1) {
    v_cls = "warn"; v_icon = "⚠️"; v_label = t("diag.health_warn");
  } else if (v_stateVal >= 2) {
    v_cls = "bad";  v_icon = "❌"; v_label = t("diag.health_degraded");
  }

  // 카드 클래스 정리 (health-ok / health-warn / health-bad)
  card.classList.remove("health-ok", "health-warn", "health-bad");
  card.classList.add("health-" + v_cls);

  if (icon)  icon.textContent = v_icon;
  if (score) score.textContent = Math.round(v_scoreVal);
  if (state) {
    state.textContent = v_label;
    state.classList.remove("ok", "warn", "bad");
    state.classList.add(v_cls);
  }
  if (fill) {
    fill.style.width = Math.max(0, Math.min(100, v_scoreVal / 10)) + "%";
    fill.classList.remove("ok", "warn", "bad");
    fill.classList.add(v_cls);
  }

  // 메타 필드
  const elSensor = qs("healthSensor");
  const elBle    = qs("healthBle");
  const elUptime = qs("healthUptime");
  const elTemp   = qs("healthTemp");

  if (elSensor) {
    const i2c = e10.i2c || {};
    elSensor.textContent = (i2c.recover_last_ok === false) ? "⚠️" : "✓";
  }
  if (elBle) {
    elBle.textContent = e10.ble_connected ? "✓" : "✗";
  }
  if (elUptime) {
    const v_ms = sys.uptime_ms || 0;
    const v_hr = Math.floor(v_ms / 3600000);
    const v_mn = Math.floor((v_ms % 3600000) / 60000);
    elUptime.textContent = v_hr > 0 ? `${v_hr}h ${v_mn}m` : `${v_mn}m`;
  }
  if (elTemp) {
    const v_temp = e10.temp_c;
    elTemp.textContent = Number.isFinite(v_temp) ? `${v_temp.toFixed(1)}°C` : "-";
  }

  // OTA 탭 버전 정보도 함께 갱신 (P2-5)
  _renderOtaVersion(e10, cfg);
}

/* =======================================================
   [v0415 Wave H] OTA 버전 카드 렌더 (P2-5)
   -------------------------------------------------------
   - fw_ver: /api/status top-level (G_APP_FW_VER)
   - api_ver: /api/status top-level (G_W10_API_VER)
   - cfg.ver: config 스키마 (412)
   ======================================================= */
function _renderOtaVersion(e10, cfg) {
  const elFw  = qs("otaFwVer");
  const elApi = qs("otaApiVer");
  const elCfg = qs("otaCfgVer");

  const top = g_lastStatus || {};
  if (elFw)  elFw.textContent  = top.fw_ver || "v0415";
  if (elApi) elApi.textContent = String(top.api_ver || 411);
  if (elCfg) elCfg.textContent = String(cfg?.ver || 412);
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

/* [v0415 Wave D-2] ctlSetDpi / ctlSetPrecision 삭제
   - bindQuickTuning(app_0415_0001.js)이 직접 /api/control 호출
*/

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
  // [G-1] 검증 먼저 (부정확한 로그 기록 방지)
  if (!Number.isFinite(code) || code < 0) {
    alert(t("trouble.invalid_code"));
    return;
  }

  // [FIX-10] Consumer code 0은 서버 _tapConsumerMask에서 no-op → 클라이언트 조기 반환
  //   - "Key Test OK" 오인 로그 방지
  //   - (Issue 5) CH_UP/DOWN 버튼은 disabled 처리되어 이 경로로 오지 않음
  if (page === "consumer" && (code >>> 0) === 0) {
    return;
  }

  const label = labelOverride || ((page === "kb")
    ? `KB u=${code} mod=${mod}`
    : `Consumer 0x${(code >>> 0).toString(16).toUpperCase()}`);
  if (typeof pushRecentLog === "function") pushRecentLog(label, true);

  const evEl = qs("diagEvents");
  if (evEl) {
    const line = document.createElement("div");
    line.className = "logline";
    line.textContent = `[${Math.floor(performance.now())}ms] KEYTEST ${label}`;
    evEl.insertBefore(line, evEl.firstChild);
    while (evEl.children.length > 60) evEl.removeChild(evEl.lastChild);
  }

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

/* [v0415 Wave D-2] rebootCheck 삭제
   - /api/reboot/check GET 라우팅 삭제됨
   - 재부팅 필요 여부는 /api/status.groups.policy.reboot_required 사용
   - 프론트는 refreshStatus()의 _updateRebootBanner()가 자동 처리
*/

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
  // [G-6] 로케일 무관 24시간 포맷
  const tStr = new Date().toLocaleTimeString(undefined, { hour12: false });
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
        // [G-10] ctlForceRelease로 통일
        await ctlForceRelease();
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
        // [G-5] N-18 피드백 통합 (recover_count 변화 + last_ok 검증)
        await ctlI2cRecoverWithFeedback();
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
        // [G-3] stale 상태 사용 방지: 토글 직전 새로고침
        await refreshStatus();
        
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

      case "safeexit":
        // [G-12] am_config의 safeBootExit()으로 통일
        await safeBootExit();
        break;
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
