/* =======================================================
   File: /www/lib/am_profile_0410.js
   Elite AirMouse WebConfig v0410 — Profile / Slots / ActionEditor
   - 로드 순서: 3
   - 의존: am_base_0410.js
   ======================================================= */

/* =======================================================
   프로파일 관리
   ======================================================= */
async function profileReloadAll(){
  const r = await apiGet("/api/profiles");
  const u = unwrapApi(r);
  if (!u.ok || !u.data){ setMsg("profiles load failed", false); return null; }

  const list   = u.data.profiles || [];
  const active = u.data.active ?? 0;
  const count  = u.data.count  ?? list.length;

  const sel = qs("profSelect");
  if (sel){
    sel.innerHTML = "";
    for (const p of list){
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
  if (u2.ok && u2.data){
    g_profile = {
      idx: u2.data.idx,
      count: u2.data.count,
      config: u2.data.config || {}
    };
  }

  return { active, count, list };
}

async function profileSwitch(idx){
  const u = unwrapApi(await apiPostJson("/api/profiles/switch", { idx }));
  if (!u.ok){ alert("switch failed: " + (u.msg || u.code)); return; }
  await profileReloadAll();
  renderSlotEditor();
  macroRenderList();
  macroRenderEditor();
  await refreshStatus();
}

async function profileCreate(){
  const name = prompt("새 프로파일 이름 (max 15자):", "New");
  if (!name) return;
  const u = unwrapApi(await apiPostJson("/api/profiles/create",
    { name: name.trim().substring(0, 15) }));
  if (!u.ok){ alert("create failed: " + (u.msg || u.code)); return; }
  await profileReloadAll();
  renderSlotEditor();
  macroRenderList();
  macroRenderEditor();
}

async function profileDelete(){
  if (!g_profile) return;
  if (!confirm(`프로파일 #${g_profile.idx} 삭제하시겠습니까?`)) return;
  const u = unwrapApi(await apiPostJson("/api/profiles/delete", { idx: g_profile.idx }));
  if (!u.ok){ alert("delete failed: " + (u.msg || u.code)); return; }
  await profileReloadAll();
  renderSlotEditor();
  macroRenderList();
  macroRenderEditor();
}

async function profileRename(){
  if (!g_profile) return;
  const cur = (g_profile.config && g_profile.config.name) || "";
  const name = prompt("새 이름 (max 15자):", cur);
  if (!name) return;
  const u = unwrapApi(await apiPostJson("/api/profiles/rename",
    { idx: g_profile.idx, name: name.trim().substring(0, 15) }));
  if (!u.ok){ alert("rename failed: " + (u.msg || u.code)); return; }
  await profileReloadAll();
}

/* =======================================================
   트리거 라이브러리 & 키코드 로드
   ======================================================= */
async function loadTriggers() {
	/* 1) keycodes에 포함된 triggers 사용 (온라인/오프라인 공통) */
	if (g_keycodes && Array.isArray(g_keycodes.triggers) && g_keycodes.triggers.length) {
		g_triggers = g_keycodes.triggers;
		return;
	}
	
	/* 2) /api/triggers 별도 조회 */
	const r = await apiGet("/api/triggers");
	const u = unwrapApi(r);
	if (u.ok && u.data && Array.isArray(u.data.triggers) && u.data.triggers.length) {
		g_triggers = u.data.triggers;
		return;
	}
	
	/* 3) 최후 fallback: 오프라인 상수 직접 참조 */
	if (typeof G_OFFLINE_TRIGGERS !== "undefined" && G_OFFLINE_TRIGGERS.length) {
		console.warn("[loadTriggers] using offline fallback constant");
		g_triggers = G_OFFLINE_TRIGGERS;
		return;
	}
	
	g_triggers = [];
	console.error("[loadTriggers] no triggers available");
}


async function loadKeycodes(){
  const r = await apiGet("/api/keycodes");
  const u = unwrapApi(r);
  if (u.ok && u.data){
    g_keycodes = u.data;
  } else if (r.json && r.json.action_kinds){
    g_keycodes = r.json;
  }
}

/* =======================================================
   슬롯 편집기 — 헬퍼
   ======================================================= */
function viewToModeIdx(v){
  if (v === "mode1") return 0;
  if (v === "mode2") return 1;
  if (v === "mode3") return 2;
  return -1;
}

function getSlotsArray(){
  if (!g_profile || !g_profile.config) return null;
  const s = g_profile.config.slots;
  if (!s) return null;

  const mi = viewToModeIdx(g_view);
  if (mi < 0) return s.global || [];

  const m = (s.modes || [])[mi];
  return (m && m.slots) ? m.slots : [];
}

function getOverrideMask(){
  if (!g_profile || !g_profile.config) return 0;
  const mi = viewToModeIdx(g_view);
  if (mi < 0) return 0;
  const s = g_profile.config.slots;
  const m = (s.modes || [])[mi];
  return m ? (m.mask >>> 0) : 0;
}

function setOverrideBit(trigIdx, on){
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
  else    s.modes[mi].mask = (s.modes[mi].mask & ~(1 << trigIdx)) >>> 0;

  if (on){
    const gArr = s.global || [];
    if (gArr[trigIdx]){
      s.modes[mi].slots[trigIdx] = JSON.parse(JSON.stringify(gArr[trigIdx]));
    }
  }
}

/* =======================================================
   슬롯 편집기 렌더
   - Tilt 경고 배너는 그룹 for 루프 내부 (TDZ 회피)
   - Live Test는 최신 배열 재조회 (stale 회피)
   ======================================================= */
/* =======================================================
   슬롯 편집기 렌더 (v0411)
   - Tilt 그룹: Mode 3 전용이므로 Mode 1/2 뷰에서는 편집 비활성
   - Global 뷰: Tilt 편집 허용 + 상속 안내 배너
   - Mode 3 뷰: Tilt 편집 정상
   - 그 외: Tilt 그룹에 사선 패턴 + pointer-events 차단
   ======================================================= */
function renderSlotEditor(){
  const root = qs("slotEditor");
  if (!root) return;

  if (!g_profile || !g_profile.config){
    root.innerHTML = '<div class="hint2">프로파일 로드 대기 중…</div>';
    return;
  }

  const triggers = g_triggers || [];
  const slotsArr = getSlotsArray() || [];
  const mask     = getOverrideMask();
  const mi       = viewToModeIdx(g_view);
  const isGlobal = (mi < 0);

  root.innerHTML = "";

  const groups = [
    { key: "button",  label: "Button Triggers" },
    { key: "gesture", label: "Gesture (Flick / Linear)" },
    { key: "tilt",    label: "Tilt Hold" }
  ];

  for (const g of groups){
    const isTiltGroup  = (g.key === "tilt");
    /* Tilt + (Mode 1/2 뷰) → 편집 비활성 */
    const tiltDisabled = isTiltGroup && !isGlobal && (mi !== 2);

    /* ── 그룹 헤더 ── */
    const head = document.createElement("div");
    head.className = "trig-group-head";
    if (isTiltGroup) {
      head.textContent = "Tilt Hold (Mode 3 · TV 전용)";
      if (tiltDisabled) head.classList.add("dim");
    } else {
      head.textContent = g.label;
    }
    root.appendChild(head);

    /* ── Tilt 그룹 배너 ── */
    if (isTiltGroup) {
      const banner = document.createElement("div");
      banner.className = "tilt-banner";

      if (isGlobal) {
        banner.classList.add("info");
        banner.textContent =
          "ℹ 여기서 설정한 Global 값은 Mode 3 (TV) 에 자동 상속됩니다.";
      } else if (tiltDisabled) {
        banner.classList.add("warn");
        banner.textContent =
          "⚠ Tilt Hold 는 Mode 3 (TV) 에서만 실제 발동합니다. " +
          "이 뷰에서는 편집할 수 없습니다. Global 또는 Mode 3 에서 설정하세요.";
      }

      if (banner.textContent) root.appendChild(banner);
    }

    /* ── 트리거 행 ── */
    for (const t of triggers){
      if (t.group !== g.key) continue;

      const idx = t.idx;
      const row = document.createElement("div");
      row.className = "trig-row";
      row.dataset.trig = String(idx);

      if (t.locked)       row.classList.add("locked");
      if (tiltDisabled)   row.classList.add("disabled-by-mode");

      const isOver = isGlobal ? false : ((mask & (1 << idx)) !== 0);
      if (!isGlobal && isOver && !tiltDisabled) row.classList.add("override");

      /* (1) 트리거 이름 */
      const nameEl = document.createElement("div");
      nameEl.className = "trig-name";
      nameEl.innerHTML = `${t.locked ? "🔒 " : ""}${t.name}`;
      row.appendChild(nameEl);

      /* (2) Global/Mode 뱃지 */
      const badge = document.createElement("div");
      badge.className = "trig-badge";

      if (tiltDisabled) {
        /* Mode 1/2 + Tilt: 상속 상태만 표시, 클릭 불가 */
        badge.textContent = "M3";
        badge.classList.add("g");
        badge.title = "Mode 3 (TV) 전용 — Global 값 상속";
        row.appendChild(badge);

      } else if (isGlobal) {
        badge.textContent = "G";
        badge.classList.add("g");
        row.appendChild(badge);

      } else if (isOver) {
        badge.textContent = `M${mi + 1}`;
        badge.classList.add("m");
        badge.title = "클릭하여 Global 로 되돌림";
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
        badge.title = "클릭하여 이 Mode 에 override 생성";
        if (!t.locked) {
          badge.style.cursor = "pointer";
          badge.onclick = () => {
            setOverrideBit(idx, true);
            renderSlotEditor();
          };
        }
        row.appendChild(badge);
      }

      /* (3) 액션 편집기 */
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
        if (mi < 0){
          g_profile.config.slots.global[idx] = newSlot;
        } else {
          const m = g_profile.config.slots.modes[mi];
          m.slots[idx] = newSlot;
        }
      });
      row.appendChild(editor);

      /* (4) Live Test */
      const testCell = document.createElement("div");
      testCell.className = "trig-test";
      if (!t.locked && !tiltDisabled){
        const btn = document.createElement("button");
        btn.className = "btn mini";
        btn.textContent = "Test";
        btn.onclick = async () => {
          const arr = getSlotsArray();
          const s = (arr && arr[idx]) ? arr[idx] : slot;
          if (!s || s.k === 0){ alert("할당된 액션이 없습니다."); return; }
          if (!confirm(`Live Test 실행? (kind: ${s.k})`)) return;
          const u = unwrapApi(await apiPostJson("/api/action/test", {
            k: s.k, h: s.h, p16: s.p16, p32: s.p32
          }));
          if (!u.ok) alert("Test failed: " + (u.msg || u.code));
        };
        testCell.appendChild(btn);
      }
      row.appendChild(testCell);

      root.appendChild(row);
    }
  }
}

/* =======================================================
   Action 편집기 (kind + 파라미터)
   ======================================================= */
function renderActionEditor(slot, editable, onChange) {
	const wrap = document.createElement("div");
	wrap.className = "action-editor";
	
	const v_kc = (g_keycodes && g_keycodes.action_kinds && g_keycodes.action_kinds.length) ?
		g_keycodes :
		((typeof G_OFFLINE_KEYCODES !== "undefined") ? G_OFFLINE_KEYCODES : null);
	
	const kinds = (v_kc && v_kc.action_kinds) || [];
	const specials = (v_kc && v_kc.specials) || [];
	const consumer = (v_kc && v_kc.consumer) || [];
	const mods = (v_kc && v_kc.mods) || [];

  const v_needsFallback =
      !kinds.length || !mods.length || !consumer.length || !specials.length;

  if (v_needsFallback &&
      typeof G_OFFLINE_KEYCODES !== "undefined" &&
      G_OFFLINE_KEYCODES) {
    if (!kinds.length)    kinds    = G_OFFLINE_KEYCODES.action_kinds || [];
    if (!mods.length)     mods     = G_OFFLINE_KEYCODES.mods         || [];
    if (!consumer.length) consumer = G_OFFLINE_KEYCODES.consumer     || [];
    if (!specials.length) specials = G_OFFLINE_KEYCODES.specials     || [];
    console.warn("[renderActionEditor] fallback to G_OFFLINE_KEYCODES (g_keycodes empty)");
  }

  const cur = {
    k:   slot.k   ?? 0,
    h:   slot.h   ?? 0,
    p16: slot.p16 ?? 0,
    p32: slot.p32 ?? 0
  };

  function emit(){
    onChange({ k: cur.k, h: cur.h, p16: cur.p16, p32: cur.p32 });
  }

  function renderParams(){
    wrap.innerHTML = "";

    /* kind select */
    const selKind = document.createElement("select");
    selKind.className = "select mini";
    selKind.disabled  = !editable;
    for (const kk of kinds){
      const o = document.createElement("option");
      o.value = String(kk.value);
      o.textContent = kk.name;
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

    if (k === 0){
      /* NONE */
    }
    else if (k === 1 || k === 2){
      /* MOUSE_CLICK / MOUSE_HOLD */
      const sel = document.createElement("select");
      sel.className = "select mini"; sel.disabled = !editable;
      sel.innerHTML = `
        <option value="0">None</option>
        <option value="1">Left</option>
        <option value="2">Right</option>
        <option value="4">Middle</option>
        <option value="8">Back</option>
        <option value="16">Forward</option>`;
      sel.value = String(cur.p16);
      sel.onchange = () => { cur.p16 = parseIntFlex(sel.value, 0); emit(); };
      wrap.appendChild(sel);
    }
    else if (k === 3){
      /* MOUSE_WHEEL */
      const axisSel = document.createElement("select");
      axisSel.className = "select mini"; axisSel.disabled = !editable;
      axisSel.innerHTML = `
        <option value="0">Y (Vertical)</option>
        <option value="1">X (Pan)</option>`;
      const dirSel = document.createElement("select");
      dirSel.className = "select mini"; dirSel.disabled = !editable;
      dirSel.innerHTML = `
        <option value="0">Up / Left</option>
        <option value="1">Down / Right</option>`;

      const axis = (cur.p16 >>> 8) & 0xFF;
      const dir  = cur.p16 & 0xFF;
      axisSel.value = String(axis);
      dirSel.value  = String(dir);

      const upd = () => {
        cur.p16 = ((parseIntFlex(axisSel.value, 0) << 8) | (parseIntFlex(dirSel.value, 0) & 0xFF));
        emit();
      };
      axisSel.onchange = upd;
      dirSel.onchange  = upd;

      wrap.appendChild(axisSel);
      wrap.appendChild(dirSel);
    }
    else if (k === 4 || k === 6){
      // KB_TAP / KB_REPEAT (mod + usage)
      const selMod = document.createElement("select");
      selMod.className = "select mini";
      selMod.disabled = !editable;
      selMod.innerHTML = `<option value="0">None</option>`;
      for (const m of mods){
        const o = document.createElement("option");
        o.value = String(m.mask);
        o.textContent = m.name;
        selMod.appendChild(o);
      }
      selMod.value = String(cur.p32 & 0xFF);
      selMod.onchange = () => { cur.p32 = parseIntFlex(selMod.value,0); emit(); };
      wrap.appendChild(selMod);
    
      // ✅ usage: 드롭다운 (키 이름으로 선택)
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
    else if (k === 5){
      // KB_COMBO
      const selMod = document.createElement("select");
      selMod.className = "select mini"; selMod.disabled = !editable;
      selMod.innerHTML = `<option value="0">None</option>`;
      for (const m of mods){
        const o = document.createElement("option");
        o.value = String(m.mask);
        o.textContent = m.name;
        selMod.appendChild(o);
      }
      selMod.value = String(cur.p32 & 0xFF);
      selMod.onchange = () => {
        cur.p32 = ((cur.p32 & 0xFFFFFF00) | parseIntFlex(selMod.value, 0)) >>> 0;
        emit();
      };
      wrap.appendChild(selMod);
      // u1, u2, u3: 각각 드롭다운
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
    else if (k === 7 || k === 8){
      // CONSUMER_TAP / CONSUMER_REPEAT
      const sel = document.createElement("select");
      sel.className = "select mini"; sel.disabled = !editable;
      sel.innerHTML = `<option value="0">None</option>`;
      for (const c of consumer){
        const o = document.createElement("option");
        o.value = String(c.mask >>> 0);
        o.textContent = c.name;
        sel.appendChild(o);
      }
      sel.value = String(cur.p32 >>> 0);
      sel.onchange = () => { cur.p32 = parseIntFlex(sel.value, 0) >>> 0; emit(); };
      wrap.appendChild(sel);
    }
    else if (k === 9){
      // SPECIAL 
      const sel = document.createElement("select");
      sel.className = "select mini"; sel.disabled = !editable;
      for (const s of specials){
        const o = document.createElement("option");
        o.value = String(s.value);
        o.textContent = s.name;
        sel.appendChild(o);
      }
      sel.value = String(cur.p16);
      sel.onchange = () => { cur.p16 = parseIntFlex(sel.value, 0); emit(); };
      wrap.appendChild(sel);
    }
    else if (k === 10){
      /* MACRO */
      const macros = (g_profile && g_profile.config && g_profile.config.macros) || [];

      const sel = document.createElement("select");
      sel.className = "select mini";
      sel.disabled  = !editable;
      sel.innerHTML = "";

      if (macros.length === 0){
        const o = document.createElement("option");
        o.value = "0";
        o.textContent = "(매크로 없음)";
        sel.appendChild(o);
        sel.disabled = true;
      } else {
        for (let i = 0; i < macros.length; i++){
          const o = document.createElement("option");
          o.value = String(i);
          o.textContent = `${i}: ${macros[i].name || ("Macro " + i)}`;
          sel.appendChild(o);
        }
        const curIdx = (cur.p32 >= 0 && cur.p32 < macros.length) ? cur.p32 : 0;
        if (curIdx !== cur.p32){
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

      if (macros.length > 0){
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
   ======================================================= */
async function saveProfile(){
  if (!g_profile) return;
  const u = unwrapApi(await apiPostJson("/api/profiles/active", g_profile.config));
  if (!u.ok){ alert("save failed: " + (u.msg || u.code)); return; }
  await profileReloadAll();
  renderSlotEditor();
  macroRenderList();
  macroRenderEditor();
  await refreshStatus();
  setMsg("프로파일 저장 완료", true);
}
