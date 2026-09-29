/* =======================================================
   File: /www/lib/am_macro_0410.js
   Elite AirMouse WebConfig v0410 — Macro Editor
   - 로드 순서: 4
   - 의존: am_base_0410.js, am_profile_0410.js
   ======================================================= */

/* =======================================================
   매크로 접근자
   ======================================================= */
function getMacrosArray(){
  if (!g_profile || !g_profile.config) return [];
  if (!g_profile.config.macros) g_profile.config.macros = [];
  return g_profile.config.macros;
}

/* =======================================================
   매크로 목록 렌더
   ======================================================= */
function macroRenderList(){
  const root = qs("macroList");
  if (!root) return;
  root.innerHTML = "";

  const macros = getMacrosArray();
  const cntEl = qs("macroCountLabel");
  if (cntEl) cntEl.textContent = `${macros.length} / 8`;

  const btnAdd = qs("btnMacroAdd");
  if (btnAdd) btnAdd.disabled = (macros.length >= 8);

  if (macros.length === 0){
    const div = document.createElement("div");
    div.className = "hint2";
    div.textContent = "등록된 매크로가 없습니다.";
    root.appendChild(div);
    return;
  }

  macros.forEach((m, i) => {
    const el = document.createElement("div");
    el.className = "macro-item";
    if (i === g_macroSel) el.classList.add("on");

    const idxEl = document.createElement("span");
    idxEl.className = "macro-item-idx";
    idxEl.textContent = String(i);
    el.appendChild(idxEl);

    const nameEl = document.createElement("span");
    nameEl.className = "macro-item-name";
    nameEl.textContent = m.name || `Macro ${i}`;
    el.appendChild(nameEl);

    const stepsEl = document.createElement("span");
    stepsEl.className = "macro-item-steps";
    stepsEl.textContent = `${(m.steps || []).length} step`;
    el.appendChild(stepsEl);

    el.onclick = () => {
      g_macroSel = i;
      macroRenderList();
      macroRenderEditor();
    };

    root.appendChild(el);
  });
}

/* =======================================================
   매크로 편집기 렌더
   ======================================================= */
function macroRenderEditor(){
  const root = qs("macroEditor");
  if (!root) return;
  root.innerHTML = "";

  const macros = getMacrosArray();

  if (g_macroSel < 0 || g_macroSel >= macros.length){
    root.innerHTML = '<div class="hint2">좌측 목록에서 매크로를 선택하세요.</div>';
    return;
  }

  const m = macros[g_macroSel];
  if (!m.steps) m.steps = [];

  /* ---- head: 이름 + 삭제/테스트 ---- */
  const head = document.createElement("div");
  head.className = "macro-editor-head";

  const nameInp = document.createElement("input");
  nameInp.type = "text";
  nameInp.maxLength = 15;
  nameInp.placeholder = "매크로 이름";
  nameInp.value = m.name || "";
  nameInp.oninput = () => { m.name = nameInp.value; };
  head.appendChild(nameInp);

  const btnTest = document.createElement("button");
  btnTest.className = "btn primary mini";
  btnTest.textContent = "Test";
  btnTest.onclick = async () => {
    if (!m.steps || m.steps.length === 0){ alert("빈 매크로입니다."); return; }
    if (!confirm(`매크로 "${m.name}" 실행?`)) return;

    const saved = await macroSave(true);
    if (!saved){ alert("저장 실패"); return; }

    const u = unwrapApi(await apiPostJson("/api/action/test_macro", { idx: g_macroSel }));
    if (!u.ok) alert("매크로 실행 실패: " + (u.msg || u.code));
  };
  head.appendChild(btnTest);

  const btnDel = document.createElement("button");
  btnDel.className = "btn danger mini";
  btnDel.textContent = "삭제";
  btnDel.onclick = async () => {
    if (!confirm(`매크로 "${m.name}" 삭제하시겠습니까?`)) return;
    macros.splice(g_macroSel, 1);
    g_macroSel = macros.length > 0 ? Math.min(g_macroSel, macros.length - 1) : -1;
    await macroSave(true);
    macroRenderList();
    macroRenderEditor();
    renderSlotEditor();
  };
  head.appendChild(btnDel);

  root.appendChild(head);

  /* ---- steps ---- */
  const stepsWrap = document.createElement("div");
  stepsWrap.className = "macro-steps";

  m.steps.forEach((s, i) => {
    const row = document.createElement("div");
    row.className = "macro-step";

    /* index */
    const idxEl = document.createElement("div");
    idxEl.className = "macro-step-idx";
    idxEl.textContent = String(i + 1);
    row.appendChild(idxEl);

    /* params */
    const params = document.createElement("div");
    params.className = "macro-step-params";
    buildMacroStepParams(params, s);
    row.appendChild(params);

    /* delay */
    const delayWrap = document.createElement("div");
    const delayInp = document.createElement("input");
    delayInp.className = "macro-step-delay";
    delayInp.type = "number";
    delayInp.min = "0";
    delayInp.max = "2000";
    delayInp.step = "10";
    delayInp.value = String(s.d ?? 0);
    delayInp.oninput = () => {
      let v = parseIntFlex(delayInp.value, 0);
      if (v < 0) v = 0;
      if (v > 2000) v = 2000;
      s.d = v;
    };
    delayWrap.appendChild(delayInp);
    const dl = document.createElement("span");
    dl.className = "macro-step-delay-label";
    dl.textContent = "delay ms (0~2000)";
    delayWrap.appendChild(dl);
    row.appendChild(delayWrap);

    /* actions (up / down / delete) */
    const acts = document.createElement("div");
    acts.className = "macro-step-actions";

    const up = document.createElement("button");
    up.className = "btn mini"; up.textContent = "▲";
    up.disabled = (i === 0);
    up.onclick = () => {
      [m.steps[i - 1], m.steps[i]] = [m.steps[i], m.steps[i - 1]];
      macroRenderEditor();
    };
    acts.appendChild(up);

    const down = document.createElement("button");
    down.className = "btn mini"; down.textContent = "▼";
    down.disabled = (i === m.steps.length - 1);
    down.onclick = () => {
      [m.steps[i + 1], m.steps[i]] = [m.steps[i], m.steps[i + 1]];
      macroRenderEditor();
    };
    acts.appendChild(down);

    const del = document.createElement("button");
    del.className = "btn danger mini"; del.textContent = "✕";
    del.onclick = () => {
      m.steps.splice(i, 1);
      macroRenderEditor();
    };
    acts.appendChild(del);

    row.appendChild(acts);
    stepsWrap.appendChild(row);
  });

  root.appendChild(stepsWrap);

  /* ---- add step ---- */
  const btnAddStep = document.createElement("button");
  btnAddStep.className = "btn";
  btnAddStep.style.marginTop = "10px";
  btnAddStep.textContent = "+ Step 추가";
  btnAddStep.disabled = (m.steps.length >= 8);
  btnAddStep.onclick = () => {
    if (m.steps.length >= 8) return;
    m.steps.push({ k: 4, h: 0, d: 0, p16: 4, p32: 0 });
    macroRenderEditor();
  };
  root.appendChild(btnAddStep);

  const stepInfo = document.createElement("div");
  stepInfo.className = "hint2";
  stepInfo.style.marginTop = "6px";
  stepInfo.textContent = `${m.steps.length} / 8 step`;
  root.appendChild(stepInfo);
}

/* =======================================================
   매크로 step 파라미터 편집기
   - MACRO 중첩 금지 + SPECIAL 제외
   ======================================================= */

function buildMacroStepParams(container, s){
  /* ── 방어: g_keycodes가 비어 있으면 오프라인 상수로 fallback ── */
  const v_kc = (g_keycodes && g_keycodes.action_kinds && g_keycodes.action_kinds.length)
             ? g_keycodes
             : ((typeof G_OFFLINE_KEYCODES !== "undefined") ? G_OFFLINE_KEYCODES : null);

  const kinds    = (v_kc && v_kc.action_kinds) || [];
  const consumer = (v_kc && v_kc.consumer)     || [];
  const mods     = (v_kc && v_kc.mods)         || [];

  if (!kinds.length || !mods.length){
    console.error("[buildMacroStepParams] keycodes missing", {
      g_keycodes, G_OFFLINE_KEYCODES_type: typeof G_OFFLINE_KEYCODES
    });
  }
  
  const allowedKinds = kinds.filter(k => k.value !== 0 && k.value !== 9 && k.value !== 10);

  const selKind = document.createElement("select");
  selKind.className = "select mini";
  selKind.innerHTML = `<option value="0">NONE</option>`;
  for (const k of allowedKinds){
    const o = document.createElement("option");
    o.value = String(k.value);
    o.textContent = k.name;
    selKind.appendChild(o);
  }
  selKind.value = String(s.k ?? 0);
  container.appendChild(selKind);

  function renderParamsFor(){
    while (container.children.length > 1){
      container.removeChild(container.lastChild);
    }

    const k = s.k;

    if (k === 1 || k === 2){
      const sel = document.createElement("select");
      sel.className = "select mini";
      sel.innerHTML = `
        <option value="0">None</option>
        <option value="1">Left</option>
        <option value="2">Right</option>
        <option value="4">Middle</option>
        <option value="8">Back</option>
        <option value="16">Forward</option>`;
      sel.value = String(s.p16 ?? 0);
      sel.onchange = () => { s.p16 = parseIntFlex(sel.value, 0); };
      container.appendChild(sel);
    }
    else if (k === 3){
      const axisSel = document.createElement("select");
      axisSel.className = "select mini";
      axisSel.innerHTML = `<option value="0">Y</option><option value="1">X</option>`;
      const dirSel = document.createElement("select");
      dirSel.className = "select mini";
      dirSel.innerHTML = `<option value="0">Up/Left</option><option value="1">Down/Right</option>`;

      axisSel.value = String((s.p16 >>> 8) & 0xFF);
      dirSel.value  = String(s.p16 & 0xFF);
      const upd = () => { s.p16 = (((axisSel.value | 0) << 8) | (dirSel.value | 0)); };
      axisSel.onchange = upd;
      dirSel.onchange  = upd;
      container.appendChild(axisSel);
      container.appendChild(dirSel);
    }
    else if (k === 4 || k === 6){
      // KB_TAP / KB_REPEAT
      const selMod = document.createElement("select");
      selMod.className = "select mini";
      selMod.innerHTML = `<option value="0">None</option>`;
      for (const mm of mods){
        const o = document.createElement("option");
        o.value = String(mm.mask);
        o.textContent = mm.name;
        selMod.appendChild(o);
      }
      selMod.value = String((s.p32 ?? 0) & 0xFF);
      selMod.onchange = () => { s.p32 = (s.p32 & ~0xFF) | (parseIntFlex(selMod.value, 0) & 0xFF); };
      container.appendChild(selMod);

      const selU = document.createElement("select");
      selU.className = "select mini";
      selU.style.minWidth = "150px";
      populateKbUsageSelect(selU, s.p16 ?? 0);
      selU.onchange = () => { s.p16 = parseIntFlex(selU.value, 0) & 0xFF; };
      container.appendChild(selU);
    }
    else if (k === 5){
      // KB_COMBO
      const selMod = document.createElement("select");
      selMod.className = "select mini";
      selMod.innerHTML = `<option value="0">None</option>`;
      for (const mm of mods){
        const o = document.createElement("option");
        o.value = String(mm.mask);
        o.textContent = mm.name;
        selMod.appendChild(o);
      }
      selMod.value = String((s.p32 ?? 0) & 0xFF);
      selMod.onchange = () => {
        s.p32 = ((s.p32 & 0xFFFFFF00) | (parseIntFlex(selMod.value, 0) & 0xFF)) >>> 0;
      };
      container.appendChild(selMod);

      for (let i = 0; i < 3; i++) {
        const shift = 8 + i * 8;
        const curCode = (s.p32 >>> shift) & 0xFF;
        
        const selU = document.createElement("select");
        selU.className = "select mini";
        selU.style.minWidth = "150px";
        populateKbUsageSelect(selU, curCode);
        selU.onchange = () => {
          const v = parseIntFlex(selU.value, 0) & 0xFF;
          s.p32 = ((s.p32 & ~(0xFF << shift)) | (v << shift)) >>> 0;
        };
        container.appendChild(selU);
      }
    }
    else if (k === 7 || k === 8){
      // CONSUMER_TAP / CONSUMER_REPEAT
      const sel = document.createElement("select");
      sel.className = "select mini";
      sel.innerHTML = `<option value="0">None</option>`;
      for (const c of consumer){
        const o = document.createElement("option");
        o.value = String(c.mask >>> 0);
        o.textContent = c.name;
        sel.appendChild(o);
      }
      sel.value = String(s.p32 >>> 0);
      sel.onchange = () => { s.p32 = parseIntFlex(sel.value, 0) >>> 0; };
      container.appendChild(sel);
    }
  }

  selKind.onchange = () => {
    s.k = parseIntFlex(selKind.value, 0);
    s.h = 0;
    s.p16 = 0;
    s.p32 = 0;
    renderParamsFor();
  };

  renderParamsFor();
}

/* =======================================================
   매크로 추가 / 저장 / 재로드
   ======================================================= */
async function macroAdd(){
  const macros = getMacrosArray();
  if (macros.length >= 8) return;

  const name = prompt("새 매크로 이름 (max 15자):", "Macro");
  if (!name) return;

  macros.push({
    name: name.trim().substring(0, 15),
    steps: [{ k: 4, h: 0, d: 0, p16: 4, p32: 0 }]
  });

  g_macroSel = macros.length - 1;
  await macroSave(true);
  macroRenderList();
  macroRenderEditor();
  renderSlotEditor();
}

async function macroSave(silent = false){
  if (!g_profile){ await cfgLoad(); }

  const patch = { macros: getMacrosArray() };
  const u = unwrapApi(await apiPostJson("/api/profiles/active", patch));
  if (!u.ok){
    if (!silent) alert("매크로 저장 실패: " + (u.msg || u.code));
    return false;
  }
  if (!silent){
    await profileReloadAll();
    macroRenderList();
    macroRenderEditor();
    renderSlotEditor();
    setMsg("매크로 저장 완료", true);
  }
  return true;
}

async function macroReload(){
  await profileReloadAll();
  g_macroSel = -1;
  macroRenderList();
  macroRenderEditor();
  renderSlotEditor();
}
