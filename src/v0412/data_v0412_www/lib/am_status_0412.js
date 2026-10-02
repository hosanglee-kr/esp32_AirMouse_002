/* =======================================================
   File: /www/lib/am_status_0412.js
   Elite AirMouse WebConfig v0412 — Status / Diag / OTA
   - 로드 순서: 6
   - 의존: am_base_0412.js
   - [v0412] N-3 (diagClear), N-5 (배너), N-6 (rebootCheck),
             N-7 (ctlSetDpi/Precision), N-8 (ctlHostCycle),
             N-9 (keyTest), N-18 (I2C 피드백), N-19 (캘리브 피드백)
   ======================================================= */

/* =======================================================
   Status 새로고침
   ======================================================= */
async function refreshStatus(){
  updateAppModeUi();

  const r = await apiGet("/api/status?compact=1");
  const u = unwrapApi(r);
  if (!u.ok || !u.data) return;

  g_lastStatus = u.data;
  const g    = u.data.groups || {};
  const sys  = g.sys  || {};
  const mem  = g.mem  || {};
  const net  = g.net  || {};
  const e10  = g.e10  || {};
  const boot = g.boot || {};
  const cfg  = g.config || {};

  if (qs("stUptime")) qs("stUptime").textContent = `${sys.uptime_ms ?? "-"} ms`;
  if (qs("stHeap"))   qs("stHeap").textContent   = `${mem.heap_free ?? "-"} B`;
  if (qs("stWiFi"))   qs("stWiFi").textContent   = `${net.mode ?? "-"} / ${net.ssid ?? "-"}`;
  if (qs("stProfile")){
    qs("stProfile").textContent =
      `#${cfg.profile_idx ?? "-"} ${cfg.profile_name ?? ""} / 총 ${cfg.profile_count ?? "-"}개`;
  }

  setPill(qs("pillBle"),  `BLE: ${e10.ble_connected ? "ON" : "OFF"}`, !!e10.ble_connected);
  setPill(qs("pillSafe"), `SAFE: ${boot.safe_mode ? "ON" : "OFF"}`,   !!boot.safe_mode);

  // [N-5] 재부팅 배너
  _updateRebootBanner(u.data.policy || {});

  // [N-7] Quick Tuning 활성 상태
  _updateQuickTuningActive(e10.dpi_level, e10.precision_mode);

  // [N-2] OTA Guard 상태 반영
  const v_guard = !!(e10.gate && e10.gate.ota_guard);
  const v_guardEl = qs("otaGuardManual");
  if (v_guardEl) v_guardEl.checked = v_guard;

  // [N-6, N-16] reboot/check 백그라운드
  _updateRebootCheckAsync();

  const el = qs("statusJson");
  if (el) el.textContent = pretty(u);
}

/* =======================================================
   [N-5] 재부팅 필요 배너
   ======================================================= */
function _updateRebootBanner(policy){
  const banner = qs("bannerReboot");
  if (!banner) return;

  const v_req     = !!(policy && policy.reboot_required);
  const v_reasons = (policy && policy.reboot_reasons) || "";

  banner.style.display = v_req ? "flex" : "none";
  const rEl = qs("bannerRebootReason");
  if (rEl) rEl.textContent = v_reasons || "(WiFi 변경 등)";
}

/* =======================================================
   [N-7] Quick Tuning 활성 상태
   ======================================================= */
function _updateQuickTuningActive(dpi, prec){
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
async function diagClear(){
  if (!confirm("진단 카운터 및 이벤트 로그를 초기화하시겠습니까?")) return;

  const u = unwrapApi(await apiPostJson("/api/diag/clear", {}));
  if (!u.ok){
    alert("초기화 실패: " + (u.msg || u.code));
    return;
  }
  setMsg("진단 카운터 초기화 완료", true);
  await refreshDiag();
}

/* =======================================================
   [N-7] 실시간 DPI / Precision (apply-only)
   ======================================================= */
async function ctlSetDpi(level){
  const u = unwrapApi(await apiPostJson("/api/control", {
    cmd: "set_dpi", level: level, snapshot: true
  }));
  if (!u.ok) alert("DPI 변경 실패: " + (u.msg || u.code));
  else setMsg(`DPI ${level} 적용`, true);
  await refreshStatus();
}

async function ctlSetPrecision(mode){
  const u = unwrapApi(await apiPostJson("/api/control", {
    cmd: "set_precision", mode: mode, snapshot: true
  }));
  if (!u.ok) alert("Precision 변경 실패: " + (u.msg || u.code));
  else setMsg(`Precision ${mode} 적용`, true);
  await refreshStatus();
}

/* =======================================================
   [N-8] Host Cycle
   ======================================================= */
async function ctlHostCycle(){
  if (!confirm("다른 저장된 호스트로 순환하시겠습니까?\n(연결이 끊겼다가 재연결됩니다)")) return;

  const u = unwrapApi(await apiPostJson("/api/action/test", {
    k: 9, h: 0, p16: 5, p32: 0
  }));
  if (!u.ok) alert("Host Cycle 실패: " + (u.msg || u.code));
  else setMsg("호스트 순환 요청됨", true);
}

/* =======================================================
   [N-18] I2C 복구 결과 표시
   ======================================================= */
async function ctlI2cRecoverWithFeedback(){
  if (!confirm("MPU6050 I2C 복구를 진행하시겠습니까?")) return;

  const v_beforeCnt = (g_lastStatus
      && g_lastStatus.groups
      && g_lastStatus.groups.e10
      && g_lastStatus.groups.e10.i2c
      && g_lastStatus.groups.e10.i2c.recover_count) || 0;

  const u = unwrapApi(await apiPostJson("/api/control", {
    cmd: "i2c_recover", snapshot: false
  }));
  if (!u.ok){
    alert("I2C 복구 요청 실패: " + (u.msg || u.code));
    return;
  }

  setMsg("I2C 복구 진행 중…", true);

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

    if (v_afterCnt > v_beforeCnt){
      if (v_lastOk){
        setMsg(`I2C 복구 완료 (성공) · 총 ${v_afterCnt}회`, true);
      } else {
        setMsg(`I2C 복구 실패 · 총 ${v_afterCnt}회`, false);
        alert("I2C 복구가 실패했습니다.\nMPU6050 배선/전원을 확인하세요.");
      }
    } else {
      setMsg("I2C 복구 요청 전송됨 (결과 미확인)", true);
    }
  }, 3000);
}

/* =======================================================
   [N-9] 단발 키 테스트
   ======================================================= */
async function keyTest(page, mod, code){
  if (!Number.isFinite(code) || code < 0){
    alert("code가 유효하지 않습니다.");
    return;
  }

  const u = unwrapApi(await apiPostJson("/api/ppt/test", {
    page: page, mod: mod, code: code
  }));
  if (!u.ok){
    setMsg("Key Test 실패: " + (u.msg || u.code), false);
    alert("Key Test 실패: " + (u.msg || u.code));
    return;
  }
  setMsg(`Key Test OK · ${page} mod=${mod} code=${code}`, true);
}

/* =======================================================
   [N-19] 자이로 캘리브 피드백
   ======================================================= */
async function ctlGyroCalibWithFeedback(){
  if (!confirm("기기를 평평한 곳에 놓고 정지 유지하세요.\n자이로 캘리브레이션을 진행하시겠습니까?")) return;

  const v_beforeRms = _getE10GyroRms();

  const u = unwrapApi(await apiPostJson("/api/control", {
    cmd: "gyro_calib", snapshot: false
  }));
  if (!u.ok){
    alert("자이로 캘리브 요청 실패: " + (u.msg || u.code));
    return;
  }

  showLoading("자이로 캘리브 진행 중… (약 1초)");

  setTimeout(async () => {
    await refreshStatus();
    hideLoading();

    const v_afterRms = _getE10GyroRms();
    const v_bias = _getE10Bias();

    const v_msg =
      `캘리브 완료 · RMS ${v_beforeRms.toFixed(2)} → ${v_afterRms.toFixed(2)} deg/s\n` +
      `Bias: (${v_bias.x.toFixed(2)}, ${v_bias.y.toFixed(2)}, ${v_bias.z.toFixed(2)})`;

    setMsg(v_msg, true);
    console.info("[N-19] calibration result", {
      before: v_beforeRms, after: v_afterRms, bias: v_bias
    });
  }, 2000);
}

function _getE10GyroRms(){
  return (g_lastStatus
      && g_lastStatus.groups
      && g_lastStatus.groups.e10
      && g_lastStatus.groups.e10.gyro
      && g_lastStatus.groups.e10.gyro.rms) || 0;
}

function _getE10Bias(){
  const g = (g_lastStatus
      && g_lastStatus.groups
      && g_lastStatus.groups.e10
      && g_lastStatus.groups.e10.gyro) || {};
  return { x: g.bias_x || 0, y: g.bias_y || 0, z: g.bias_z || 0 };
}

/* =======================================================
   [N-6, N-16] reboot/check 백그라운드 조회
   ======================================================= */
let _rebootCheckBusy = false;

async function rebootCheck(){
  const u = unwrapApi(await apiGet("/api/reboot/check"));
  return (u.ok && u.data) ? u.data : null;
}

async function _updateRebootCheckAsync(){
  if (_rebootCheckBusy) return;
  _rebootCheckBusy = true;
  try {
    const d = await rebootCheck();
    if (d && d.required){
      const banner = qs("bannerReboot");
      if (banner){
        banner.style.display = "flex";
        banner.title =
          `mask: 0x${(d.mask >>> 0).toString(16)}\n` +
          `reasons: ${d.reasons}\n` +
          `allowed: ${d.allowed}` + (d.deny_code ? `\ndeny: ${d.deny_code}` : "");
      }
    }
  } catch(e){}
  finally { _rebootCheckBusy = false; }
}

/* =======================================================
   Online / Offline 모드 전환
   ======================================================= */
function toggleAppMode(){
  const targetMode = (g_appMode === APP_MODE_ONLINE) ? APP_MODE_OFFLINE : APP_MODE_ONLINE;
  if (!confirm(`현재 모드: ${g_appMode}\n${targetMode} 모드로 전환하시겠습니까?`)) return;

  setAppMode(targetMode);
  setMsg(`모드 전환 완료: ${g_appMode}`, true);

  profileReloadAll().then(() => {
    renderSlotEditor();
    macroRenderList();
    macroRenderEditor();
    cfgLoad();
    refreshStatus();
  });
}

/* =======================================================
   Tab / Diag 헬퍼
   ======================================================= */
function isTabOn(name){
  const el = qs("tab-" + name);
  return !!(el && el.classList.contains("on"));
}

function isDiagTyping(){
  const f = qs("diagFilter");
  if (!f) return false;
  return (document.activeElement === f) || (nowMs() < g_diagTypingUntilMs);
}

/* =======================================================
   Diag 렌더
   ======================================================= */
function renderDiag(u){
  const data   = u.data || {};
  const d      = data.diag || {};
  const events = Array.isArray(data.events) ? data.events : [];

  const cEl = qs("diagCounters");
  if (cEl){
    cEl.innerHTML = "";
    const items = [
      ["body_too_large", d.body_too_large],
      ["no_body_slot",   d.no_body_slot],
      ["bad_json",       d.json_bad],
      ["safe_blocked",   d.safe_blocked],
      ["ota_blocked",    d.ota_blocked]
    ];
    for (const [k, v] of items){
      const div = document.createElement("div");
      div.className = "pill";
      div.textContent = `${k}: ${v ?? 0}`;
      cEl.appendChild(div);
    }
  }

  const listEl = qs("diagEvents");
  if (listEl){
    listEl.innerHTML = "";
    const f = (qs("diagFilter")?.value || "").trim();
    const shown = events.slice().reverse()
      .filter(ev => !f || String(ev.code || "").includes(f))
      .slice(0, 80);

    for (const ev of shown){
      const div = document.createElement("div");
      div.className = "logline";
      div.textContent = `[${ev.ms ?? 0}ms] ${ev.code ?? "-"}`;
      listEl.appendChild(div);
    }

    if (!shown.length){
      const div = document.createElement("div");
      div.className = "hint2";
      div.textContent = "No events.";
      listEl.appendChild(div);
    }
  }

  const rawEl = qs("diagJson");
  if (rawEl) rawEl.textContent = pretty(u);
}

async function refreshDiag(){
  const r = await apiGet("/api/diag");
  renderDiag(unwrapApi(r));
}

/* =======================================================
   OTA 업로드 / 상태
   ======================================================= */
async function otaUpload(){
  const f = qs("otaFile")?.files?.[0];
  if (!f){ alert("파일을 선택하세요."); return; }

  qs("otaHint").textContent = `uploading: ${f.name}...`;
  const r = await fetch("/api/ota", { method: "POST", body: f });
  qs("otaHint").textContent = await r.text();
  await otaStatus();
}

async function otaStatus(){
  const r = await apiGet("/api/ota/status");
  qs("otaJson").textContent = pretty(unwrapApi(r).data || r.text);
}

