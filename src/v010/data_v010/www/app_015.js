// data/www/app_015.js : /api/keycodes + 고급값 저장/로드 포함(묶음)
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

const PPT_ACTIONS = ["start", "exit", "next", "prev", "black", "laser"];

let g_mods = [];
let g_keys = [];

function buildSelectOptions(sel, items, textKey, valueKey) {
  sel.innerHTML = "";
  for (const it of items) {
    const o = document.createElement("option");
    o.textContent = it[textKey];
    o.value = String(it[valueKey]);
    sel.appendChild(o);
  }
}
function setSelectByValue(sel, v) {
  const s = String(v ?? 0);
  for (let i = 0; i < sel.options.length; i++) {
    if (sel.options[i].value === s) { sel.selectedIndex = i; return; }
  }
  sel.selectedIndex = 0;
}

function initKeyUIFromTables() {
  document.querySelectorAll("select.mod").forEach(sel => buildSelectOptions(sel, g_mods, "name", "mask"));
  document.querySelectorAll("select.key").forEach(sel => buildSelectOptions(sel, g_keys, "name", "code"));
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
  return { mod: Number(modSel?.value ?? 0), key: Number(keySel?.value ?? 0) };
}

function applyConfigToUI(cfg) {
  $("dpi_level").value = String(cfg.dpi_level ?? 2);
  $("hard_click_lock").checked = !!cfg.hard_click_lock;

  $("scale_base_1").value = String(cfg.scale_base?.[0] ?? 0.55);
  $("scale_base_2").value = String(cfg.scale_base?.[1] ?? 0.75);
  $("scale_base_3").value = String(cfg.scale_base?.[2] ?? 1.00);

  $("accel_gain_1").value = String(cfg.accel_gain?.[0] ?? 0.35);
  $("accel_gain_2").value = String(cfg.accel_gain?.[1] ?? 0.55);
  $("accel_gain_3").value = String(cfg.accel_gain?.[2] ?? 0.85);

  $("accel_threshold").value = String(cfg.accel_threshold ?? 8.0);

  $("wheel_threshold_deg").value = String(cfg.wheel?.threshold_deg ?? 90);
  $("wheel_step_max").value = String(cfg.wheel?.step_max ?? 6);

  $("gesture_flick_deg").value = String(cfg.gesture?.flick_deg ?? 200);
  $("gesture_cooldown_ms").value = String(cfg.gesture?.cooldown_ms ?? 600);

  $("scroll_cursor_damp").value = String(cfg.scroll_cursor_damp ?? 0.25);

  const pk = cfg.ppt_keys || {};
  for (const a of PPT_ACTIONS) {
    const obj = pk[a] || { mod: 0, key: 0 };
    setPptDropdown(a, obj.mod, obj.key);
  }

  $("raw_start_mod").value = String(pk?.start?.mod ?? 0);
  $("raw_start_key").value = String(pk?.start?.key ?? 0);
}

function gatherUIToConfig() {
  const cfg = {};
  cfg.dpi_level = Number($("dpi_level").value);
  cfg.hard_click_lock = $("hard_click_lock").checked;

  cfg.scale_base = [
    Number($("scale_base_1").value),
    Number($("scale_base_2").value),
    Number($("scale_base_3").value),
  ];

  cfg.accel_gain = [
    Number($("accel_gain_1").value),
    Number($("accel_gain_2").value),
    Number($("accel_gain_3").value),
  ];

  cfg.accel_threshold = Number($("accel_threshold").value);

  cfg.wheel = {
    threshold_deg: Number($("wheel_threshold_deg").value),
    step_max: Number($("wheel_step_max").value),
  };

  cfg.gesture = {
    flick_deg: Number($("gesture_flick_deg").value),
    cooldown_ms: Number($("gesture_cooldown_ms").value),
  };

  cfg.scroll_cursor_damp = Number($("scroll_cursor_damp").value);

  cfg.ppt_keys = {};
  for (const a of PPT_ACTIONS) cfg.ppt_keys[a] = getPptDropdown(a);

  const rsm = Number($("raw_start_mod").value || 0);
  const rsk = Number($("raw_start_key").value || 0);
  if (!(rsm === 0 && rsk === 0)) cfg.ppt_keys.start = { mod: rsm, key: rsk };

  return cfg;
}

async function loadKeycodes() {
  log("[GET] /api/keycodes");
  const kc = await apiGet("/api/keycodes");
  g_mods = kc.mods || [];
  g_keys = kc.keys || [];
  if (g_mods.length === 0) g_mods = [{ name: "None", mask: 0 }];
  if (g_keys.length === 0) g_keys = [{ name: "None", code: 0 }];
  initKeyUIFromTables();
  log(`keycodes loaded: mods=${g_mods.length}, keys=${g_keys.length}`);
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
  bind();
  await loadKeycodes();
  await reload();
});