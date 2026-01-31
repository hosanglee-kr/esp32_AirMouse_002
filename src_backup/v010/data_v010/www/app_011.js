// ------------------------------------------------------
// W10 app_011.js
//  - Key name dropdown -> {mod,key} numeric 저장
//  - Modifier는 HID 표준 비트 사용(일반적으로 호환됨)
// ------------------------------------------------------

const $ = (id) => document.getElementById(id);
const log = (m) => { const el = $("log"); el.textContent = (el.textContent + m + "\n"); el.scrollTop = el.scrollHeight; };

async function apiGet(path) {
  const r = await fetch(path, { cache: "no-store" });
  return await r.json();
}

async function apiPost(path, obj) {
  const r = await fetch(path, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: obj ? JSON.stringify(obj) : ""
  });
  return await r.json();
}

// ------------------------------------------------------
// Modifier bits (HID standard)
// (Mystfit CompositeHID가 일반 HID modifier bit로 구현된 전제)
// ------------------------------------------------------
const MODS = [
  { name: "None", value: 0 },
  { name: "LeftCtrl", value: 0x01 },
  { name: "LeftShift", value: 0x02 },
  { name: "LeftAlt", value: 0x04 },
  { name: "LeftGUI", value: 0x08 },
  { name: "RightCtrl", value: 0x10 },
  { name: "RightShift", value: 0x20 },
  { name: "RightAlt", value: 0x40 },
  { name: "RightGUI", value: 0x80 },
];

// ------------------------------------------------------
// Key codes (자주 쓰는 것 위주)
// - 필요하면 목록만 계속 늘리면 됨
// - 값은 "KeyboardHIDCodes.h"에 맞춰 정리하는 방식 추천
//   (현재는 일반 HID usage ID 관례 기반)
// ------------------------------------------------------
const KEYS = [
  { name: "A", code: 0x04 },
  { name: "B", code: 0x05 },
  { name: "C", code: 0x06 },
  { name: "L", code: 0x0F },
  { name: "P", code: 0x13 },
  { name: "I", code: 0x0C },

  { name: "Enter", code: 0x28 },
  { name: "Esc", code: 0x29 },
  { name: "Space", code: 0x2C },

  { name: "PageUp", code: 0x4B },
  { name: "PageDown", code: 0x4E },

  { name: "F5", code: 0x3E },
  { name: "F1", code: 0x3A },
  { name: "F2", code: 0x3B },
  { name: "F3", code: 0x3C },
  { name: "F4", code: 0x3D },

  { name: "LeftArrow", code: 0x50 },
  { name: "RightArrow", code: 0x4F },
  { name: "UpArrow", code: 0x52 },
  { name: "DownArrow", code: 0x51 },
];

const PPT_ACTIONS = ["start", "exit", "next", "prev", "black", "laser"];

function buildSelectOptions(sel, items, getText, getValue) {
  sel.innerHTML = "";
  for (const it of items) {
    const o = document.createElement("option");
    o.textContent = getText(it);
    o.value = String(getValue(it));
    sel.appendChild(o);
  }
}

function initKeyUI() {
  const modSels = document.querySelectorAll("select.mod");
  const keySels = document.querySelectorAll("select.key");

  modSels.forEach(sel => buildSelectOptions(sel, MODS, x => x.name, x => x.value));
  keySels.forEach(sel => buildSelectOptions(sel, KEYS, x => x.name, x => x.code));
}

function setSelectByValue(sel, v) {
  const s = String(v ?? 0);
  for (let i = 0; i < sel.options.length; i++) {
    if (sel.options[i].value === s) { sel.selectedIndex = i; return; }
  }
  // 못 찾으면 0으로
  sel.selectedIndex = 0;
}

function setPptDropdown(action, mod, key) {
  const modSel = document.querySelector(`select.mod[data-key="${action}"]`);
  const keySel = document.querySelector(`select.key[data-key="${action}"]`);
  if (!modSel || !keySel) return;
  setSelectByValue(modSel, mod);
  setSelectByValue(keySel, key);
}

function getPptDropdown(action) {
  const modSel = document.querySelector(`select.mod[data-key="${action}"]`);
  const keySel = document.querySelector(`select.key[data-key="${action}"]`);
  return {
    mod: Number(modSel?.value ?? 0),
    key: Number(keySel?.value ?? 0),
  };
}

function applyConfigToUI(cfg) {
  $("dpi_level").value = String(cfg.dpi_level ?? 2);
  $("hard_click_lock").checked = !!cfg.hard_click_lock;

  const pk = cfg.ppt_keys || {};
  for (const a of PPT_ACTIONS) {
    const obj = pk[a] || { mod: 0, key: 0 };
    setPptDropdown(a, obj.mod, obj.key);
  }

  // advanced raw inputs (예시 1개만)
  $("raw_start_mod").value = String(pk?.start?.mod ?? 0);
  $("raw_start_key").value = String(pk?.start?.key ?? 0);
}

function gatherUIToConfig() {
  const cfg = {};
  cfg.dpi_level = Number($("dpi_level").value);
  cfg.hard_click_lock = $("hard_click_lock").checked;

  cfg.ppt_keys = {};
  for (const a of PPT_ACTIONS) {
    cfg.ppt_keys[a] = getPptDropdown(a);
  }

  // advanced override example(값이 있으면 덮어쓰기)
  const rsm = Number($("raw_start_mod").value || 0);
  const rsk = Number($("raw_start_key").value || 0);
  // 사용자가 명시적으로 숫자를 바꿨을 때만 덮고 싶으면 별도 플래그를 두면 됨.
  // 여기서는 "0,0 이외면" 덮는 형태
  if (!(rsm === 0 && rsk === 0)) {
    cfg.ppt_keys.start = { mod: rsm, key: rsk };
  }

  return cfg;
}

async function reload() {
  log("[GET] /api/config");
  const cfg = await apiGet("/api/config");
  applyConfigToUI(cfg);
  log("loaded.");
}

async function save() {
  const cfg = gatherUIToConfig();
  log("[POST] /api/config");
  const res = await apiPost("/api/config", cfg);
  log(JSON.stringify(res));
}

async function resetCfg() {
  log("[POST] /api/reset");
  const res = await apiPost("/api/reset");
  log(JSON.stringify(res));
  await reload();
}

async function reboot() {
  log("[POST] /api/reboot");
  const res = await apiPost("/api/reboot");
  log(JSON.stringify(res));
}

function bind() {
  $("btn_reload").addEventListener("click", reload);
  $("btn_save").addEventListener("click", save);
  $("btn_reset").addEventListener("click", resetCfg);
  $("btn_reboot").addEventListener("click", reboot);
}

window.addEventListener("load", async () => {
  initKeyUI();
  bind();
  await reload();
});
