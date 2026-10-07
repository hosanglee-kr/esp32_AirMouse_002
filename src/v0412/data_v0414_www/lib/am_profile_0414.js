/* =======================================================
   File: /www/lib/am_profile_0414.js
   Elite AirMouse WebConfig v0414 — Profile / Slots / ActionEditor
   - 로드 순서: 3
   - [Phase 2.2 H-1] getActiveProfile() 정식 정의
   - [Phase 3.6 L-5] saveProfile 성공 시 clearDirty, emit에 markSlotDirty
   ======================================================= */

/* =======================================================
   [Phase 2.2 H-1] 활성 프로파일 config 접근자
   ======================================================= */
function getActiveProfile() {
  return (g_profile && g_profile.config) ? g_profile.config : null;
}

/* =======================================================
   프로파일 관리
   ======================================================= */
async function profileReloadAll() {
  const r = await apiGet("/api/profiles");
  const u = unwrapApi(r);
  if (!u.ok || !u.data) { setMsg("profiles load failed", false); return null; }

  const list = u.data.profiles || [];
  const active = u.data.active ?? 0;
  const count = u.data.count ?? list.length;

  const sel = qs("profSelect");
  if (sel) {
    sel.innerHTML = "";
    for (const p of list) {
      const o = document.createElement("option");
      o.value = String(p.idx);
      o.textContent = `${p.idx}: ${p.name}`;
      sel.appendChild(o);
    }
    sel.value = String(active);
  }

  const btnCreate = qs("btnProfCreate");
  if (btnCreate) btnCreate.disabled = (count >= 5);
  const btnDelete = qs("btnProfDelete");
  if (btnDelete) btnDelete.disabled = (count <= 1);

  const r2 = await apiGet("/api/profiles/active");
  const u2 = unwrapApi(r2);
  if (u2.ok && u2.data) {
    g_profile = {
      idx: u2.data.idx,
      count: u2.data.count,
      config: u2.data.config || {}
    };
  }

  return { active, count, list };
}

/* [C-04] 프로파일 스위치 재진입 방지 */
let g_profileSwitchBusy = false;

async function profileSwitch(idx) {
  if (g_profileSwitchBusy) {
    console.info("[profileSwitch] busy, ignored idx=", idx);
    return;
  }
  g_profileSwitchBusy = true;

  const v_sel = qs("profSelect");
  if (v_sel) v_sel.disabled = true;

  showLoading(t("pop.prof_switching", { idx }));

  try {
    const u = unwrapApi(await apiPostJson("/api/profiles/switch", { idx }));
    if (!u.ok) { alert(`${t("pop.switch_fail")} ${u.msg || u.code}`); return; }

    await profileReloadAll();
    renderSlotEditor();
    macroRenderList();
    macroRenderEditor();
    await refreshStatus();
  } finally {
    g_profileSwitchBusy = false;
    if (v_sel) v_sel.disabled = false;
    hideLoading();
  }
}

async function profileCreate() {
  const name = prompt(t("pop.prof_name_prompt"), "New");
  if (!name) return;

  showLoading(t("loading.processing"));
  try {
    const u = unwrapApi(await apiPostJson("/api/profiles/create",
      { name: name.trim().substring(0, 15) }));
    if (!u.ok) { alert(`${t("pop.prof_create_fail")} ${u.msg || u.code}`); return; }
    await profileReloadAll();
    renderSlotEditor();
    macroRenderList();
    macroRenderEditor();
  } finally {
    hideLoading();
  }
}

async function profileDelete() {
  if (!g_profile) return;

  const v_count = (g_profile.count || 1);

  let msg;
  if (v_count > 1) {
    msg = t("pop.prof_delete_active_confirm", { idx: g_profile.idx });
  } else {
    msg = t("pop.prof_delete_confirm", { idx: g_profile.idx, name: g_profile.config?.name || "" });
  }
  if (!confirm(msg)) return;

  showLoading(t("loading.processing"));
  try {
    const u = unwrapApi(await apiPostJson("/api/profiles/delete", { idx: g_profile.idx }));
    if (!u.ok) { alert(`${t("pop.prof_delete_fail")} ${u.msg || u.code}`); return; }
    await profileReloadAll();
    renderSlotEditor();
    macroRenderList();
    macroRenderEditor();
  } finally {
    hideLoading();
  }
}

async function profileRename() {
  if (!g_profile) return;
  const cur = (g_profile.config && g_profile.config.name) || "";
  const name = prompt(t("pop.prof_rename_prompt"), cur);
  if (!name) return;
  const u = unwrapApi(await apiPostJson("/api/profiles/rename",
    { idx: g_profile.idx, name: name.trim().substring(0, 15) }));
  if (!u.ok) { alert(`${t("pop.prof_rename_fail")} ${u.msg || u.code}`); return; }
  await profileReloadAll();
}

/* =======================================================
   트리거 라이브러리 & 키코드 로드
   ======================================================= */
async function loadTriggers() {
  if (g_keycodes && Array.isArray(g_keycodes.triggers) && g_keycodes.triggers.length) {
    g_triggers = g_keycodes.triggers;
    return;
  }

  if (g_appMode === APP_MODE_OFFLINE && typeof G_OFFLINE_TRIGGERS !== "undefined") {
    g_triggers = G_OFFLINE_TRIGGERS;
    return;
  }

  const r = await apiGet("/api/triggers");
  const u = unwrapApi(r);
  if (u.ok && u.data && Array.isArray(u.data.triggers) && u.data.triggers.length) {
    g_triggers = u.data.triggers;
    return;
  }

  if (typeof G_OFFLINE_TRIGGERS !== "undefined") {
    console.warn("[loadTriggers] fallback constant");
    g_triggers = G_OFFLINE_TRIGGERS;
    return;
  }

  g_triggers = [];
  console.error("[loadTriggers] no triggers available");
}

async function loadKeycodes() {
  const r = await apiGet("/api/keycodes");
  const u = unwrapApi(r);
  if (u.ok && u.data) {
    g_keycodes = u.data;
  } else if (r.json && r.json.action_kinds) {
    g_keycodes = r.json;
  }
}

/* =======================================================
   슬롯 편집기 — 헬퍼
   ======================================================= */
function viewToModeIdx(v) {
  if (v === "mode1") return 0;
  if (v === "mode2") return 1;
  if (v === "mode3") return 2;
  return -1;
}

function getSlotsArray() {
  if (!g_profile || !g_profile.config) return null;
  const s = g_profile.config.slots;
  if (!s) return null;

  const mi = viewToModeIdx(g_view);

  function _ensure27(arr) {
    while (arr.length < 27) arr.push({ k: 0, h: 0, p16: 0, p32: 0 });
    if (arr.length > 27) arr.length = 27;
    return arr;
  }

  if (mi < 0) {
    if (!Array.isArray(s.global)) s.global = [];
    return _ensure27(s.global);
  }

  if (!Array.isArray(s.modes)) s.modes = [];
  if (!s.modes[mi]) s.modes[mi] = { mask: 0, slots: [] };
  if (!Array.isArray(s.modes[mi].slots)) s.modes[mi].slots = [];

  return _ensure27(s.modes[mi].slots);
}

function getOverrideMask() {
  if (!g_profile || !g_profile.config) return 0;
  const mi = viewToModeIdx(g_view);
  if (mi < 0) return 0;
  const s = g_profile.config.slots;
  const m = (s.modes || [])[mi];
  return m ? (m.mask >>> 0) : 0;
}

function setOverrideBit(trigIdx, on) {
  if (!g_profile || !g_profile.config) return;
  const mi = viewToModeIdx(g_view);
  if (mi < 0) return;
  const s = g_profile.config.slots;
  if (!s.modes) s.modes = [];
  if (!s.modes[mi]) s.modes[mi] = { mask: 0, slots: [] };
  if (!s.modes[mi].slots) s.modes[mi].slots = [];
  while (s.modes[mi].slots.length < 27) {
    s.modes[mi].slots.push({ k: 0, h: 0, p16: 0, p32: 0 });
  }

  if (on) s.modes[mi].mask = (s.modes[mi].mask | (1 << trigIdx)) >>> 0;
  else s.modes[mi].mask = (s.modes[mi].mask & ~(1 << trigIdx)) >>> 0;

  if (on) {
    const gArr = s.global || [];
    if (gArr[trigIdx]) {
      s.modes[mi].slots[trigIdx] = JSON.parse(JSON.stringify(gArr[trigIdx]));
    }
  }
}

/* =======================================================
   슬롯 편집기 렌더
   ======================================================= */
function renderSlotEditor() {
  const root = qs("slotEditor");
  if (!root) return;

  if (!g_profile || !g_profile.config) {
    const isEn = (typeof g_currLang !== "undefined" && g_currLang === "en");
    root.innerHTML = `<div class="hint2">${isEn ? "Waiting for profile to load…" : "프로필 로드 대기 중…"}</div>`;
    return;
  }

  const triggers = (Array.isArray(g_triggers) && g_triggers.length) ?
    g_triggers :
    (g_keycodes && Array.isArray(g_keycodes.triggers) && g_keycodes.triggers.length) ?
    g_keycodes.triggers :
    (typeof G_OFFLINE_TRIGGERS !== "undefined" ? G_OFFLINE_TRIGGERS : []);

  const slotsArr = getSlotsArray() || [];
  const mask = getOverrideMask();
  const mi = viewToModeIdx(g_view);
  const isGlobal = (mi < 0);

  root.innerHTML = "";

  const groups = [
    { key: "button", label: t("slots.grp_btn") },
    { key: "gesture", label: t("slots.grp_gesture") },
    { key: "tilt", label: t("slots.grp_tilt") }
  ];

  const v_metaArr = (g_keycodes && Array.isArray(g_keycodes.slots_meta))
    ? g_keycodes.slots_meta : [];

  for (const g of groups) {
    const isTiltGroup = (g.key === "tilt");
    const tiltDisabled = isTiltGroup && !isGlobal && (mi !== 2);

    const head = document.createElement("div");
    head.className = "trig-group-head";
    if (isTiltGroup) {
      head.textContent = t("slots.grp_tilt");
      if (tiltDisabled) head.classList.add("dim");
    } else {
      head.textContent = g.label;
    }
    root.appendChild(head);

    if (isTiltGroup) {
      const banner = document.createElement("div");
      banner.className = "tilt-banner";

      if (isGlobal) {
        banner.classList.add("info");
        banner.textContent = t("slots.tilt_global_info");
      } else if (tiltDisabled) {
        banner.classList.add("warn");
        banner.textContent = t("slots.tilt_disabled_warn");
      }

      if (banner.textContent) root.appendChild(banner);
    }

    for (const t of triggers) {
      if (t.group !== g.key) continue;

      const idx = t.idx;
      const row = document.createElement("div");
      row.className = "trig-row";
      row.dataset.trig = String(idx);

      if (t.locked) row.classList.add("locked");
      if (tiltDisabled) row.classList.add("disabled-by-mode");

      const isOver = isGlobal ? false : ((mask & (1 << idx)) !== 0);
      if (!isGlobal && isOver && !tiltDisabled) row.classList.add("override");

      const nameEl = document.createElement("div");
      nameEl.className = "trig-name";

      let v_metaSub = "";
      const v_meta = v_metaArr.find(m => m.id === `S${idx + 1}`);
      if (v_meta && v_meta.btn && v_meta.evt) {
        v_metaSub = `<span class="meta-sub">(${v_meta.btn} / ${v_meta.evt})</span>`;
      }

      nameEl.innerHTML = `${t.locked ? "🔒 " : ""}${t.name}${v_metaSub}`;
      row.appendChild(nameEl);

      const badge = document.createElement("div");
      badge.className = "trig-badge";

      if (tiltDisabled) {
        badge.textContent = "M3";
        badge.classList.add("g");
        badge.title = t("slots.badge_m3_title");
        row.appendChild(badge);
      } else if (isGlobal) {
        badge.textContent = "G";
        badge.classList.add("g");
        row.appendChild(badge);
      } else if (isOver) {
        badge.textContent = `M${mi + 1}`;
        badge.classList.add("m");
        badge.title = t("slots.badge_m_title");
        if (!t.locked) {
          badge.style.cursor = "pointer";
          badge.onclick = () => {
            setOverrideBit(idx, false);
            renderSlotEditor();
          };
        }
        row.appendChild(badge);
      } else {
        badge.textContent = "G";
        badge.classList.add("g");
        badge.title = t("slots.badge_g_title");
        if (!t.locked) {
          badge.style.cursor = "pointer";
          badge.onclick = () => {
            setOverrideBit(idx, true);
            renderSlotEditor();
          };
        }
        row.appendChild(badge);
      }

      const slot = isGlobal
        ? (slotsArr[idx] || { k: 0, h: 0, p16: 0, p32: 0 })
        : (isOver
          ? (slotsArr[idx] || { k: 0, h: 0, p16: 0, p32: 0 })
          : ((g_profile.config.slots.global && g_profile.config.slots.global[idx])
            || { k: 0, h: 0, p16: 0, p32: 0 }));

      const isEditable = !t.locked && !tiltDisabled && (isGlobal || isOver);

      const editor = renderActionEditor(slot, isEditable, (newSlot) => {
        const arr = getSlotsArray();
        if (!arr) return;
        const v_mi = viewToModeIdx(g_view);
        if (v_mi < 0) {
          g_profile.config.slots.global[idx] = newSlot;
        } else {
          const m = g_profile.config.slots.modes[v_mi];
          m.slots[idx] = newSlot;
        }
      });
      row.appendChild(editor);

      const testCell = document.createElement("div");
      testCell.className = "trig-test";
      if (!t.locked && !tiltDisabled) {
        const btn = document.createElement("button");
        btn.className = "btn mini";
        btn.textContent = "Test";
        btn.onclick = async () => {
          const arr = getSlotsArray();
          const s = (arr && arr[idx]) ? arr[idx] : slot;
          if (!s || s.k === 0) { alert(t("pop.no_action_assigned")); return; }
          if (!confirm(t("pop.live_test_confirm", { kind: s.k }))) return;
          const u = unwrapApi(await apiPostJson("/api/action/test", {
            k: s.k, h: s.h, p16: s.p16, p32: s.p32
          }));
          if (!u.ok) alert(`${t("pop.test_fail")} ${u.msg || u.code}`);
        };
        testCell.appendChild(btn);
      }
      row.appendChild(testCell);

      root.appendChild(row);
    }
  }
}

/* =======================================================
   Action 편집기
   - [Phase 3.6 L-5] emit 시 markSlotDirty
   ======================================================= */
function renderActionEditor(slot, editable, onChange) {
  const wrap = document.createElement("div");
  wrap.className = "action-editor";

  const v_kc = (g_keycodes && g_keycodes.action_kinds && g_keycodes.action_kinds.length)
    ? g_keycodes
    : ((typeof G_OFFLINE_KEYCODES !== "undefined") ? G_OFFLINE_KEYCODES : null);

  let kinds = (v_kc && v_kc.action_kinds) || [];
  let specials = (v_kc && v_kc.specials) || [];
  let consumer = (v_kc && v_kc.consumer) || [];
  let mods = (v_kc && v_kc.mods) || [];

  const v_needsFallback =
    !kinds.length || !mods.length || !consumer.length || !specials.length;

  if (v_needsFallback && typeof G_OFFLINE_KEYCODES !== "undefined" && G_OFFLINE_KEYCODES) {
    if (!kinds.length) kinds = G_OFFLINE_KEYCODES.action_kinds || [];
    if (!mods.length) mods = G_OFFLINE_KEYCODES.mods || [];
    if (!consumer.length) consumer = G_OFFLINE_KEYCODES.consumer || [];
    if (!specials.length) specials = G_OFFLINE_KEYCODES.specials || [];
  }

  const cur = {
    k: slot.k ?? 0,
    h: slot.h ?? 0,
    p16: slot.p16 ?? 0,
    p32: slot.p32 ?? 0
  };

  function emit() {
    onChange({ k: cur.k, h: cur.h, p16: cur.p16, p32: cur.p32 });
    // [L-5] 슬롯 편집 dirty 표시
    if (typeof markSlotDirty === "function") markSlotDirty();
  }

  function renderParams() {
    wrap.innerHTML = "";

    const selKind = document.createElement("select");
    selKind.className = "select mini";
    selKind.disabled = !editable;
    for (const kk of kinds) {
      const o = document.createElement("option");
      o.value = String(kk.value);
      o.textContent = getActionKindFriendlyName(kk.name, kk.value);
      selKind.appendChild(o);
    }
    selKind.value = String(cur.k);
    selKind.onchange = () => {
      cur.k = parseIntFlex(selKind.value, 0);
      cur.h = 0; cur.p16 = 0; cur.p32 = 0;
      renderParams();
      emit();
    };
    wrap.appendChild(selKind);

    const k = cur.k;

    if (k === 0) { /* NONE */ }
    else if (k === 1 || k === 2) {
      const sel = document.createElement("select");
      sel.className = "select mini"; sel.disabled = !editable;
      sel.innerHTML = [0, 1, 2, 4, 8, 16].map(v =>
        `<option value="${v}">${getMouseButtonFriendlyName(v)}</option>`
      ).join("");
      sel.value = String(cur.p16);
      sel.onchange = () => { cur.p16 = parseIntFlex(sel.value, 0); emit(); };
      wrap.appendChild(sel);
    }
    else if (k === 3) {
      const axisSel = document.createElement("select");
      axisSel.className = "select mini"; axisSel.disabled = !editable;
      axisSel.innerHTML = `
        <option value="0">${getScrollFriendlyName(true, 0)}</option>
        <option value="1">${getScrollFriendlyName(true, 1)}</option>`;
      const dirSel = document.createElement("select");
      dirSel.className = "select mini"; dirSel.disabled = !editable;
      dirSel.innerHTML = `
        <option value="0">${getScrollFriendlyName(false, 0)}</option>
        <option value="1">${getScrollFriendlyName(false, 1)}</option>`;

      axisSel.value = String((cur.p16 >>> 8) & 0xFF);
      dirSel.value = String(cur.p16 & 0xFF);

      const upd = () => {
        cur.p16 = ((parseIntFlex(axisSel.value, 0) << 8) | (parseIntFlex(dirSel.value, 0) & 0xFF));
        emit();
      };
      axisSel.onchange = upd;
      dirSel.onchange = upd;

      wrap.appendChild(axisSel);
      wrap.appendChild(dirSel);
    }
    else if (k === 4 || k === 6) {
      const selMod = document.createElement("select");
      selMod.className = "select mini";
      selMod.disabled = !editable;
      selMod.innerHTML = `<option value="0">${t("slots.opt_none_mod")}</option>`;
      for (const m of mods) {
        const o = document.createElement("option");
        o.value = String(m.mask);
        o.textContent = getModifierFriendlyName(m.name);
        selMod.appendChild(o);
      }
      selMod.value = String(cur.p32 & 0xFF);
      selMod.onchange = () => { cur.p32 = parseIntFlex(selMod.value, 0); emit(); };
      wrap.appendChild(selMod);

      const selU = document.createElement("select");
      selU.className = "select mini";
      selU.style.minWidth = "160px";
      selU.disabled = !editable;
      populateKbUsageSelect(selU, cur.p16);
      selU.onchange = () => {
        cur.p16 = parseIntFlex(selU.value, 0) & 0xFF;
        emit();
      };
      wrap.appendChild(selU);
    }
    else if (k === 5) {
      const selMod = document.createElement("select");
      selMod.className = "select mini"; selMod.disabled = !editable;
      selMod.innerHTML = `<option value="0">${t("slots.opt_none_mod")}</option>`;
      for (const m of mods) {
        const o = document.createElement("option");
        o.value = String(m.mask);
        o.textContent = getModifierFriendlyName(m.name);
        selMod.appendChild(o);
      }
      selMod.value = String(cur.p32 & 0xFF);
      selMod.onchange = () => {
        cur.p32 = ((cur.p32 & 0xFFFFFF00) | parseIntFlex(selMod.value, 0)) >>> 0;
        emit();
      };
      wrap.appendChild(selMod);

      for (let i = 0; i < 3; i++) {
        const shift = 8 + i * 8;
        const curCode = (cur.p32 >>> shift) & 0xFF;

        const selU = document.createElement("select");
        selU.className = "select mini";
        selU.style.minWidth = "150px";
        selU.disabled = !editable;
        populateKbUsageSelect(selU, curCode);
        selU.onchange = () => {
          const v = parseIntFlex(selU.value, 0) & 0xFF;
          cur.p32 = ((cur.p32 & ~(0xFF << shift)) | (v << shift)) >>> 0;
          emit();
        };
        wrap.appendChild(selU);
      }
    }
    else if (k === 7 || k === 8) {
      const sel = document.createElement("select");
      sel.className = "select mini"; sel.disabled = !editable;
      sel.innerHTML = `<option value="0">${t("slots.opt_none_action")}</option>`;
      for (const c of consumer) {
        const o = document.createElement("option");
        o.value = String(c.mask >>> 0);
        o.textContent = getConsumerFriendlyName(c.name);
        sel.appendChild(o);
      }
      sel.value = String(cur.p32 >>> 0);
      sel.onchange = () => { cur.p32 = parseIntFlex(sel.value, 0) >>> 0; emit(); };
      wrap.appendChild(sel);
    }
    else if (k === 9) {
      const sel = document.createElement("select");
      sel.className = "select mini"; sel.disabled = !editable;
      for (const s of specials) {
        const o = document.createElement("option");
        o.value = String(s.value);
        o.textContent = getSpecialFriendlyName(s.name);
        sel.appendChild(o);
      }
      sel.value = String(cur.p16);
      sel.onchange = () => { cur.p16 = parseIntFlex(sel.value, 0); emit(); };
      wrap.appendChild(sel);
    }
    else if (k === 10) {
      const macros = (g_profile && g_profile.config && g_profile.config.macros) || [];

      const sel = document.createElement("select");
      sel.className = "select mini";
      sel.disabled = !editable;
      sel.innerHTML = "";

      if (macros.length === 0) {
        const o = document.createElement("option");
        o.value = "0";
        o.textContent = t("slots.opt_no_macro");
        sel.appendChild(o);
        sel.disabled = true;
      } else {
        for (let i = 0; i < macros.length; i++) {
          const o = document.createElement("option");
          o.value = String(i);
          o.textContent = `${i}: ${macros[i].name || ("Macro " + i)}`;
          sel.appendChild(o);
        }
        const curIdx = (cur.p32 >= 0 && cur.p32 < macros.length) ? cur.p32 : 0;
        if (curIdx !== cur.p32) {
          cur.p32 = curIdx;
          emit();
        }
        sel.value = String(curIdx);
      }

      sel.onchange = () => {
        cur.p32 = parseIntFlex(sel.value, 0);
        emit();
      };
      wrap.appendChild(sel);

      if (macros.length > 0) {
        const idx = (cur.p32 >= 0 && cur.p32 < macros.length) ? cur.p32 : 0;
        const hintEl = document.createElement("span");
        hintEl.className = "hint2";
        hintEl.style.fontSize = ".74em";
        hintEl.textContent = `${(macros[idx].steps || []).length} step`;
        wrap.appendChild(hintEl);
      }
    }
  }

  renderParams();
  return wrap;
}

/* =======================================================
   프로파일 저장
   - [Phase 3.6 L-5] 저장 성공 시 clearDirty
   ======================================================= */

async function saveProfile() {
  if (!g_profile) return;
    
  showLoading(t("loading.processing"));
  try {
    // [G-8] slots만 patch 전송 (Config 탭 미저장값의 의도치 않은 저장 방지)
    const patch = { slots: g_profile.config.slots };
    const u = unwrapApi(await apiPostJson("/api/profiles/active", patch));
    if (!u.ok) { alert(`${t("pop.slot_save_fail")} ${u.msg || u.code}`); return; }

    await profileReloadAll();
    renderSlotEditor();
    macroRenderList();
    macroRenderEditor();
    await refreshStatus();
    if (typeof cfgLoad === "function") await cfgLoad(); // [H-12] Config 탭 UI 재렌더
    setMsg(t("slots.save") + " OK", true);
    if (typeof clearSlotDirty === "function") clearSlotDirty();
  } finally {
    hideLoading();
  }
}
