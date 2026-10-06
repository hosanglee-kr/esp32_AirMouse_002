/* =======================================================
   File: /www/lib/am_remote_0414.js
   Elite AirMouse WebConfig v0414 — Virtual Controller & Joystick
   - 가상 리모컨 렌더러 / 하이브리드 조이스틱 인터랙션
   - 모드별 도구 (Mode Tools) / 기기 모드 실시간 전환
   - 로드 순서: 7 (am_status_0414.js 다음, app_0414_0001.js 이전)
   - 의존: am_base_0414.js, am_i18n_0414.js, am_offline_0414.js
   ======================================================= */

/* =======================================================
   1. Mode Tools (모드별 빠른 실행 도구 정의 및 렌더러)
   ======================================================= */
const MODE_TOOLS = {
  1: {
    badge: "PC", ico: "🖱️", groups: [
      {
        labelKey: "tools.pc.window", items: [
          { labelKey: "tools.pc.win_max", page: "kb", mod: 8, code: 82, ico: "⬆️" },
          { labelKey: "tools.pc.win_left", page: "kb", mod: 8, code: 80, ico: "⬅️" },
          { labelKey: "tools.pc.win_right", page: "kb", mod: 8, code: 79, ico: "➡️" },
          { labelKey: "tools.pc.win_desk", page: "kb", mod: 8, code: 7, ico: "🖥️" },
          { labelKey: "tools.pc.taskview", page: "kb", mod: 8, code: 43, ico: "🗂️" }
        ]
      },
      {
        labelKey: "tools.pc.edit", items: [
          { labelKey: "tools.pc.copy", page: "kb", mod: 1, code: 6, ico: "📋" },
          { labelKey: "tools.pc.paste", page: "kb", mod: 1, code: 25, ico: "📥" },
          { labelKey: "tools.pc.undo", page: "kb", mod: 1, code: 29, ico: "↩️" },
          { labelKey: "tools.pc.select", page: "kb", mod: 1, code: 4, ico: "🔲" },
          { labelKey: "tools.pc.find", page: "kb", mod: 1, code: 9, ico: "🔍" }
        ]
      },
      {
        labelKey: "tools.pc.misc", items: [
          { labelKey: "tools.pc.newtab", page: "kb", mod: 1, code: 23, ico: "➕" },
          { labelKey: "tools.pc.close", page: "kb", mod: 1, code: 26, ico: "✖️" },
          { labelKey: "tools.pc.lock", page: "kb", mod: 8, code: 15, ico: "🔒" }
        ]
      }
    ]
  },
  2: {
    badge: "PPT", ico: "🎤", groups: [
      {
        labelKey: "tools.ppt.show", items: [
          { labelKey: "tools.ppt.start", page: "kb", mod: 0, code: 62, ico: "▶️" },
          { labelKey: "tools.ppt.start_cur", page: "kb", mod: 2, code: 62, ico: "⏯️" },
          { labelKey: "tools.ppt.end", page: "kb", mod: 0, code: 41, ico: "⏹️" }
        ]
      },
      {
        labelKey: "tools.ppt.screen", items: [
          { labelKey: "tools.ppt.black", page: "kb", mod: 0, code: 5, ico: "⬛" },
          { labelKey: "tools.ppt.white", page: "kb", mod: 0, code: 26, ico: "⬜" },
          { labelKey: "tools.ppt.pen", page: "kb", mod: 1, code: 19, ico: "🖊️" },
          { labelKey: "tools.ppt.laser", page: "kb", mod: 1, code: 15, ico: "🔴" }
        ]
      },
      {
        labelKey: "tools.ppt.nav", items: [
          { labelKey: "tools.ppt.home", page: "kb", mod: 0, code: 74, ico: "⏮️" },
          { labelKey: "tools.ppt.endpg", page: "kb", mod: 0, code: 77, ico: "⏭️" }
        ]
      }
    ]
  },
  3: {
    badge: "TV", ico: "📺", groups: [
      {
        labelKey: "tools.tv.channel", items: [
          { labelKey: "tools.tv.ch_up", page: "consumer", mod: 0, code: 0x4000, ico: "📶" },
          { labelKey: "tools.tv.ch_down", page: "consumer", mod: 0, code: 0x8000, ico: "📶" }
        ]
      },
      {
        labelKey: "tools.tv.media", items: [
          { labelKey: "tools.tv.play", page: "consumer", mod: 0, code: 0x0008, ico: "▶️" },
          { labelKey: "tools.tv.stop", page: "consumer", mod: 0, code: 0x0010, ico: "⏹️" },
          { labelKey: "tools.tv.next", page: "consumer", mod: 0, code: 0x0020, ico: "⏭️" },
          { labelKey: "tools.tv.prev", page: "consumer", mod: 0, code: 0x0040, ico: "⏮️" }
        ]
      },
      {
        labelKey: "tools.tv.nav", items: [
          { labelKey: "tools.tv.home", page: "consumer", mod: 0, code: 0x0400, ico: "🏠" },
          { labelKey: "tools.tv.back", page: "consumer", mod: 0, code: 0x0200, ico: "↩️" },
          { labelKey: "tools.tv.input", page: "consumer", mod: 0, code: 0x2000, ico: "🔀" },
          { labelKey: "tools.tv.search", page: "consumer", mod: 0, code: 0x0800, ico: "🔍" }
        ]
      },
      {
        labelKey: "tools.tv.vol", items: [
          { labelKey: "tools.tv.vol_up", page: "consumer", mod: 0, code: 0x0001, ico: "🔊" },
          { labelKey: "tools.tv.vol_down", page: "consumer", mod: 0, code: 0x0002, ico: "🔉" },
          { labelKey: "tools.tv.mute", page: "consumer", mod: 0, code: 0x0004, ico: "🔇" }
        ]
      }
    ]
  }
};

function renderModeTools() {
  const bodyEl = qs("modeToolsBody"); if (!bodyEl) return;
  const badgeEl = qs("modeToolsBadge");
  const icoEl = qs("modeToolsIco");
  const cfg = MODE_TOOLS[g_currentActiveMode] || MODE_TOOLS[1];
  if (badgeEl) badgeEl.textContent = cfg.badge;
  if (icoEl) icoEl.textContent = cfg.ico;

  bodyEl.innerHTML = "";
  for (const group of cfg.groups) {
    const gDiv = document.createElement("div");
    gDiv.className = "tool-group";
    const gT = document.createElement("div");
    gT.className = "tool-group-title";
    gT.textContent = t(group.labelKey);
    gDiv.appendChild(gT);

    const bWrap = document.createElement("div");
    bWrap.className = "tool-btns";
    for (const item of group.items) {
      const btn = document.createElement("button");
      btn.className = "tool-btn";
      btn.innerHTML = `<span class="tool-ico">${item.ico || "•"}</span><span>${t(item.labelKey)}</span>`;
      btn.addEventListener("click", () => keyTest(item.page, item.mod || 0, item.code));
      bWrap.appendChild(btn);
    }
    gDiv.appendChild(bWrap);
    bodyEl.appendChild(gDiv);
  }
}

/* =======================================================
   2. Virtual Remote Configuration by Mode
   ======================================================= */
const REMOTE_CONFIG_BY_MODE = {
  1: {
    themeClass: "mode-pc", badgeKey: "remote.badge_pc", guideKey: "remote.guide_pc",
    top1: { label: "Win", subKey: "remote.sub_start", action: () => keyTest("kb", 8, 0) },
    top2: { label: "🔇", subKey: "remote.sub_mute", action: () => keyTest("consumer", 0, 0x0004) },
    ringUp: { label: "▲", action: () => keyTest("kb", 0, 82) },
    ringDown: { label: "▼", action: () => keyTest("kb", 0, 81) },
    ringLeft: { label: "◀", action: () => keyTest("kb", 0, 80) },
    ringRight: { label: "▶", action: () => keyTest("kb", 0, 79) },
    knobText: "Enter", knobTap: () => keyTest("kb", 0, 40),
    sub1: { label: "ESC", subKey: "remote.sub_cancel", action: () => keyTest("kb", 0, 41) },
    sub2: { label: "캡처", subKey: "remote.sub_screenshot", action: () => keyTest("kb", 10, 22) },
    pad1U: { label: "Vol +", action: () => keyTest("consumer", 0, 0x0001) },
    pad1D: { label: "Vol -", action: () => keyTest("consumer", 0, 0x0002) },
    pad2U: { label: "▶⏸", action: () => keyTest("consumer", 0, 0x0008) },
    pad2D: { label: "정지", action: () => keyTest("consumer", 0, 0x0010) },
    pad3U: { label: "PgUp", action: () => keyTest("kb", 0, 75) },
    pad3D: { label: "PgDn", action: () => keyTest("kb", 0, 78) },
    pad3LblKey: "remote.pad_doc"
  },
  2: {
    themeClass: "mode-ppt", badgeKey: "remote.badge_ppt", guideKey: "remote.guide_ppt",
    top1: { labelKey: "remote.ppt_f5", action: () => keyTest("kb", 0, 58) },
    top2: { labelKey: "remote.ppt_shf5", action: () => keyTest("kb", 2, 62) },
    ringUp: { labelKey: "remote.ppt_end", action: () => keyTest("kb", 0, 41) },
    ringDown: { labelKey: "remote.ppt_laser", action: () => keyTest("kb", 1, 15) },
    ringLeft: { labelKey: "remote.ppt_prev", action: () => keyTest("kb", 0, 75) },
    ringRight: { labelKey: "remote.ppt_next", action: () => keyTest("kb", 0, 78) },
    knobTextKey: "remote.ppt_sel", knobTap: () => keyTest("kb", 0, 40),
    sub1: { labelKey: "remote.ppt_black", action: () => keyTest("kb", 0, 5) },
    sub2: { labelKey: "remote.ppt_white", action: () => keyTest("kb", 0, 26) },
    pad1U: { labelKey: "remote.ppt_prev", action: () => keyTest("kb", 0, 75) },
    pad1D: { labelKey: "remote.ppt_next", action: () => keyTest("kb", 0, 78) },
    pad2U: { labelKey: "remote.ppt_pen", action: () => keyTest("kb", 1, 19) },
    pad2D: { labelKey: "remote.ppt_arrow", action: () => keyTest("kb", 1, 4) },
    pad3U: { label: "Vol +", action: () => keyTest("consumer", 0, 0x0001) },
    pad3D: { label: "Vol -", action: () => keyTest("consumer", 0, 0x0002) },
    pad3LblKey: "remote.pad_ppt_vol"
  },
  3: {
    themeClass: "mode-tv", badgeKey: "remote.badge_tv", guideKey: "remote.guide_tv",
    top1: { labelKey: "remote.tv_power", action: () => keyTest("consumer", 0, 0x1000) },
    top2: { label: "🔇", action: () => keyTest("consumer", 0, 0x0004) },
    ringUp: { label: "▲", action: () => keyTest("kb", 0, 82) },
    ringDown: { label: "▼", action: () => keyTest("kb", 0, 81) },
    ringLeft: { label: "◀", action: () => keyTest("kb", 0, 80) },
    ringRight: { label: "▶", action: () => keyTest("kb", 0, 79) },
    knobTextKey: "remote.tv_ok", knobTap: () => keyTest("kb", 0, 40),
    sub1: { labelKey: "remote.tv_back", action: () => keyTest("consumer", 0, 0x0200) },
    sub2: { labelKey: "remote.tv_home", action: () => keyTest("consumer", 0, 0x0400) },
    pad1U: { label: "VOL +", action: () => keyTest("consumer", 0, 0x0001) },
    pad1D: { label: "VOL -", action: () => keyTest("consumer", 0, 0x0002) },
    pad2U: { label: "▶⏸", action: () => keyTest("consumer", 0, 0x0008) },
    pad2D: { labelKey: "remote.tv_input", action: () => keyTest("consumer", 0, 0x2000) },
    pad3U: { labelKey: "remote.tv_ch_up", action: () => keyTest("consumer", 0, 0x4000) },
    pad3D: { labelKey: "remote.tv_ch_down", action: () => keyTest("consumer", 0, 0x8000) },
    pad3LblKey: "remote.pad_ch"
  }
};

function _renderController(mode) {
  const cfg = REMOTE_CONFIG_BY_MODE[mode] || REMOTE_CONFIG_BY_MODE[1];
  const card = qs("remoteCard");
  if (card) {
    card.classList.remove("mode-pc", "mode-ppt", "mode-tv", "remote-flash");
    void card.offsetWidth;
    card.classList.add(cfg.themeClass, "remote-flash");
    setTimeout(() => card.classList.remove("remote-flash"), 700);
  }
  const badge = qs("remoteActiveModeBadge");
  if (badge) badge.textContent = t(cfg.badgeKey);
  const hintEl = qs("dpadGuideHint");
  if (hintEl) hintEl.textContent = t(cfg.guideKey);

  const setBtn = (id, def, subId) => {
    const el = qs(id); if (!el || !def) return;
    const v_lbl = def.labelKey ? t(def.labelKey) : def.label;
    if (v_lbl !== undefined) el.textContent = v_lbl;
    const cloned = el.cloneNode(true);
    el.parentNode.replaceChild(cloned, el);
    if (typeof def.action === "function") cloned.addEventListener("click", def.action);
    if (subId) {
      const sEl = qs(subId);
      if (sEl) sEl.textContent = def.subKey ? t(def.subKey) : "";
    }
  };
  setBtn("rbtnTop1", cfg.top1, "lblTop1");
  setBtn("rbtnTop2", cfg.top2, "lblTop2");
  setBtn("rbtnRingUp", cfg.ringUp);
  setBtn("rbtnRingDown", cfg.ringDown);
  setBtn("rbtnRingLeft", cfg.ringLeft);
  setBtn("rbtnRingRight", cfg.ringRight);
  setBtn("rbtnSub1", cfg.sub1, "lblSub1");
  setBtn("rbtnSub2", cfg.sub2, "lblSub2");
  setBtn("rbtnPad1Up", cfg.pad1U);
  setBtn("rbtnPad1Down", cfg.pad1D);
  setBtn("rbtnPad2Up", cfg.pad2U);
  setBtn("rbtnPad2Down", cfg.pad2D);
  setBtn("rbtnPad3Up", cfg.pad3U);
  setBtn("rbtnPad3Down", cfg.pad3D);
  const pad3Lbl = qs("lblPad3");
  if (pad3Lbl && cfg.pad3LblKey) pad3Lbl.textContent = t(cfg.pad3LblKey);
  const knobText = qs("joyKnobText");
  if (knobText) knobText.textContent = cfg.knobTextKey ? t(cfg.knobTextKey) : (cfg.knobText || "");
}

function _updateModeTabs() {
  qsa(".preview-tab").forEach(b => {
    b.classList.toggle("on", parseIntFlex(b.dataset.mode, 0) === g_currentActiveMode);
  });
}

/* =======================================================
   3. Mode Switching (실제 기기 모드 전환)
   ======================================================= */
async function ctlSetMode(target) {
  target = Math.max(1, Math.min(3, target | 0));
  const cur = g_currentActiveMode;
  if (target === cur) return true;
  if (!confirm(t("pop.mode_switch_confirm", { cur, tgt: target }))) return false;

  const cfg = (typeof getActiveProfile === "function") ? getActiveProfile() : (g_profile ? g_profile.config : null);
  if (cfg && cfg.e10) {
    cfg.e10.active_mode = target;
    if (typeof saveOfflineStore === "function") saveOfflineStore();
  }

  showLoading(`모드 ${target} 전환 중…`);
  try {
    if (g_appMode === APP_MODE_ONLINE) {
      const steps = ((target - cur) + 3) % 3;
      for (let i = 0; i < steps; i++) {
        // EN_C20_ACT_SPECIAL (k=9, p16=3 MODE NEXT)
        await apiPostJson("/api/action/test", { k: 9, h: 0, p16: 3, p32: 0 });
        await new Promise(r => setTimeout(r, 320));
      }
    } else {
      await new Promise(r => setTimeout(r, 400));
    }
    applyRemoteMode(target);
    setMsg(t("pop.mode_switch_ok", { tgt: target }), true);
    if (typeof pushRecentLog === "function") pushRecentLog(`MODE → ${target}`, true);
    return true;
  } finally {
    hideLoading();
  }
}

function applyRemoteMode(mode) {
  g_currentActiveMode = mode;
  const cfg = (typeof getActiveProfile === "function") ? getActiveProfile() : (g_profile ? g_profile.config : null);
  if (cfg && cfg.e10) {
    cfg.e10.active_mode = mode;
    if (typeof saveOfflineStore === "function") saveOfflineStore();
  }
  _renderController(mode);
  _updateModeTabs();
  renderModeTools();
}

/* =======================================================
   4. Fold Persistence (아코디언 및 리모컨 접기 상태 유지)
   ======================================================= */
function bindFoldPersist(detailsId, storageKey, defaultOpen) {
  const el = qs(detailsId); if (!el) return;
  let v_open = !!defaultOpen;
  try {
    const saved = localStorage.getItem(storageKey);
    if (saved === "1") v_open = true;
    else if (saved === "0") v_open = false;
  } catch (e) { }
  el.open = v_open;
  el.addEventListener("toggle", () => {
    try { localStorage.setItem(storageKey, el.open ? "1" : "0"); } catch (e) { }
  });
}

function _restoreRemoteFold() {
  try { g_isRemoteFolded = (localStorage.getItem(REMOTE_FOLD_KEY) === "1"); }
  catch (e) { g_isRemoteFolded = false; }
  const body = qs("remoteCollapseBody"), btn = qs("btnToggleRemoteFold");
  if (body) body.classList.toggle("folded", g_isRemoteFolded);
  if (btn) btn.textContent = g_isRemoteFolded ? "펼치기 ▼" : "접기 ▲";
}

function bindRemoteFoldToggle() {
  const btn = qs("btnToggleRemoteFold"), body = qs("remoteCollapseBody");
  if (!btn || !body) return;
  _restoreRemoteFold();
  btn.addEventListener("click", () => {
    g_isRemoteFolded = !g_isRemoteFolded;
    body.classList.toggle("folded", g_isRemoteFolded);
    btn.textContent = g_isRemoteFolded ? "펼치기 ▼" : "접기 ▲";
    try { localStorage.setItem(REMOTE_FOLD_KEY, g_isRemoteFolded ? "1" : "0"); } catch (e) { }
  });
}

/* =======================================================
   5. Hybrid Joystick (D-Pad 링 + 아날로그 조이스틱 노브)
   ======================================================= */
function initHybridJoystick() {
  const ring = qs("dpadRing"), knob = qs("joyStickKnob");
  if (!ring || !knob) return;
  const MAX_RADIUS = 38, DEADZONE = 12;
  let isDragging = false, startX = 0, startY = 0, repeatTimer = null, activeDir = null;

  function getDir(dx, dy) {
    const a = Math.atan2(dy, dx) * 180 / Math.PI;
    if (a >= -45 && a <= 45) return "RIGHT";
    if (a > 45 && a < 135) return "DOWN";
    if (a >= 135 || a <= -135) return "LEFT";
    if (a < -45 && a > -135) return "UP";
    return null;
  }

  function triggerTilt(dir) {
    if (!dir) return;
    if (g_currentActiveMode === 2) {
      keyTest("kb", 0, (dir === "RIGHT" || dir === "DOWN") ? 78 : 75);
    } else {
      const map = { UP: 82, DOWN: 81, LEFT: 80, RIGHT: 79 };
      keyTest("kb", 0, map[dir] || 0);
    }
  }

  function handleStart(e) {
    if (e.target.classList.contains("ring-btn")) return;
    isDragging = true;
    const rect = ring.getBoundingClientRect();
    startX = rect.left + rect.width / 2;
    startY = rect.top + rect.height / 2;
    handleMove(e);
  }

  function handleMove(e) {
    if (!isDragging) return;
    const cx = e.touches ? e.touches[0].clientX : e.clientX;
    const cy = e.touches ? e.touches[0].clientY : e.clientY;
    let dx = cx - startX, dy = cy - startY;
    const dist = Math.hypot(dx, dy);
    if (dist > MAX_RADIUS) { dx = (dx / dist) * MAX_RADIUS; dy = (dy / dist) * MAX_RADIUS; }
    knob.style.transform = `translate(${dx}px, ${dy}px)`;
    if (dist > DEADZONE) {
      const dir = getDir(dx, dy);
      if (dir !== activeDir) {
        activeDir = dir;
        triggerTilt(dir);
        clearInterval(repeatTimer);
        repeatTimer = setInterval(() => { if (activeDir) triggerTilt(activeDir); }, 300);
      }
    } else {
      activeDir = null;
      clearInterval(repeatTimer);
    }
  }

  function handleEnd() {
    if (!isDragging) return;
    isDragging = false;
    clearInterval(repeatTimer);
    if (!activeDir) {
      const cfg = REMOTE_CONFIG_BY_MODE[g_currentActiveMode];
      if (cfg && cfg.knobTap) cfg.knobTap();
    }
    activeDir = null;
    knob.style.transition = "transform .15s cubic-bezier(0.175,0.885,0.32,1.275)";
    knob.style.transform = "translate(0px,0px)";
    setTimeout(() => { knob.style.transition = ""; }, 150);
  }

  knob.addEventListener("mousedown", handleStart);
  window.addEventListener("mousemove", handleMove);
  window.addEventListener("mouseup", handleEnd);
  knob.addEventListener("touchstart", handleStart, { passive: false });
  window.addEventListener("touchmove", handleMove, { passive: false });
  window.addEventListener("touchend", handleEnd);
  window.addEventListener("touchcancel", handleEnd);
}

/* =======================================================
   6. Mode Tabs 바인딩
   ======================================================= */
function bindModeTabs() {
  qsa(".preview-tab").forEach(b => {
    b.addEventListener("click", () => {
      const m = parseIntFlex(b.dataset.mode, 0);
      ctlSetMode(m);
    });
  });
}
