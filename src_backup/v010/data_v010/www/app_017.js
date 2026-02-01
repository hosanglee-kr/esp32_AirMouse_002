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
let g_keyFilter = "";

function buildSelectOptions(sel, items, textKey, valueKey, filterFn) {
  sel.innerHTML = "";
  for (const it of items) {
    if (filterFn && !filterFn(it)) continue;
    const o = document.createElement("option");
    o.textContent = it[textKey];
    o.value = String(it[valueKey]);
    sel.appendChild(o);
  }
}
function setSelectByValue(sel, v) {
  const s = String(v ?? 0);
  for (let i=0; i<sel.options.length; i++) {
    if (sel.options[i].value === s) { sel.selectedIndex = i; return; }
  }
  sel.selectedIndex = 0;
}

function filterKeyItem(it) {
  if (!g_keyFilter) return true;
  const t = String(it.name || "").toLowerCase();
  return t.includes(g_keyFilter);
}

function refreshKeyDropdownsPreserveSelection() {
  document.querySelectorAll("select.key").forEach(sel => {
    const prev = sel.value;
    buildSelectOptions(sel, g_keys, "name", "code", filterKeyItem);
    if (prev) setSelectByValue(sel, prev);
  });

  // preview
  const preview = g_keys.filter(filterKeyItem).slice(0, 14).map(k => `${k.name}:${k.code}`).join(" | ");
  $("key_preview").textContent = preview || "-";
}

function initKeyUIFromTables() {
  document.querySelectorAll("select.mod").forEach(sel => buildSelectOptions(sel, g_mods, "name", "mask"));
  refreshKeyDropdownsPreserveSelection();
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

// ---------------------
// UI <-> config
// ---------------------
function applyConfigToUI(cfg) {
  // wifi
  const w = cfg.wifi || {};
  $("wifi_mode").value = String(w.mode ?? 0);
  $("sta_ssid").value = String(w.sta?.ssid ?? "");
  $("sta_pass").value = String(w.sta?.pass ?? "");
  $("ap_ssid").value  = String(w.ap?.ssid ?? "EliteAirMouse");
  $("ap_pass").value  = String(w.ap?.pass ?? "12345678");
  $("mdns_host").value = String(w.mdns?.host ?? "elite-airmouse");

  // e10
  const e = cfg.e10 || {};
  $("dpi_level").value = String(e.dpi_level ?? 2);
  $("hard_click_lock").checked = !!e.hard_click_lock;

  // ppt keys
  const pk = e.ppt_keys || {};
  for (const a of PPT_ACTIONS) {
    const obj = pk[a] || { mod: 0, key: 0 };
    setPptDropdown(a, obj.mod, obj.key);
  }
}

function gatherUIToConfig() {
  const cfg = {};
  cfg.wifi = {
    mode: Number($("wifi_mode").value),
    sta: { ssid: $("sta_ssid").value || "", pass: $("sta_pass").value || "" },
    ap:  { ssid: $("ap_ssid").value || "",  pass: $("ap_pass").value || "" },
    mdns:{ host: $("mdns_host").value || "elite-airmouse" }
  };

  cfg.e10 = {};
  cfg.e10.dpi_level = Number($("dpi_level").value);
  cfg.e10.hard_click_lock = $("hard_click_lock").checked;

  cfg.e10.ppt_keys = {};
  for (const a of PPT_ACTIONS) cfg.e10.ppt_keys[a] = getPptDropdown(a);

  return cfg;
}

// ---------------------
// Load tables/config
// ---------------------
async function loadKeycodes() {
  log("[GET] /api/keycodes");
  const kc = await apiGet("/api/keycodes");
  g_mods = kc.mods || [];
  g_keys = kc.keys || [];
  initKeyUIFromTables();
  log(`keycodes loaded: mods=${g_mods.length}, keys=${g_keys.length}`);
}

async function reloadConfig() {
  log("[GET] /api/config");
  const cfg = await apiGet("/api/config");
  applyConfigToUI(cfg);
  log("loaded.");
}

async function saveConfig() {
  const cfg = gatherUIToConfig();
  log("[POST] /api/config");
  const res = await apiPost("/api/config", cfg);
  log(JSON.stringify(res));
}

async function resetCfg() {
  log("[POST] /api/reset");
  const res = await apiPost("/api/reset");
  log(JSON.stringify(res));
  await reloadConfig();
}

async function reboot() {
  log("[POST] /api/reboot");
  const res = await apiPost("/api/reboot");
  log(JSON.stringify(res));
}

// ---------------------
// Status polling
// ---------------------
function fmtUptime(ms) {
  const s = Math.floor(ms/1000);
  const h = Math.floor(s/3600);
  const m = Math.floor((s%3600)/60);
  const ss = s%60;
  return `${h}h ${m}m ${ss}s`;
}

async function pollStatus() {
  try {
    const st = await apiGet("/api/status");

    const net = st.net || {};
    $("st_wifi").textContent = `${net.mode || "-"} / ${net.ssid || "-"}`;
    $("st_ip").textContent = net.ip || "-";
    $("st_mdns").textContent = net.mdns || "-";

    $("st_uptime").textContent = fmtUptime(st.uptime_ms || 0);
    $("st_heap").textContent = String(st.heap_free ?? "-");

    const e = st.e10 || {};
    $("st_ble").textContent = e.ble_connected ? "Connected" : "Disconnected";
    $("st_dpi").textContent = String(e.dpi_level ?? "-");
    $("st_ppt").textContent = e.ppt_mode ? "ON" : "OFF";

    const g = e.gyro || {};
    $("st_bias").textContent = `x=${(g.bias_x??0).toFixed?.(3) ?? g.bias_x} y=${(g.bias_y??0).toFixed?.(3) ?? g.bias_y} z=${(g.bias_z??0).toFixed?.(3) ?? g.bias_z}`;
    $("st_temp").textContent = `${(e.temp_c ?? 0).toFixed?.(1) ?? e.temp_c} C`;

    const t = e.timing || {};
    $("st_sampling").textContent = `target=${t.sampling_ms_target ?? "-"}ms avg=${(t.sampling_ms_avg ?? 0).toFixed?.(2) ?? t.sampling_ms_avg}ms`;

    const err = e.err || {};
    $("st_err").textContent = `mpu=${err.mpu_read ?? 0} mutex=${err.mutex_miss ?? 0} overrun=${err.task_overrun ?? 0}`;
  } catch (e) {
    // ignore
  }
}

let g_pollTimer = null;

// ---------------------
// Remote control (PPT/DPI)
// ---------------------
async function control(payload) {
  const res = await apiPost("/api/control", payload);
  log(`[control] ${JSON.stringify(payload)} => ${JSON.stringify(res)}`);
  await pollStatus();
}

function bind() {
  $("btn_reload").addEventListener("click", reloadConfig);
  $("btn_save").addEventListener("click", saveConfig);
  $("btn_reset").addEventListener("click", resetCfg);
  $("btn_reboot").addEventListener("click", reboot);

  $("key_filter").addEventListener("input", (e) => {
    g_keyFilter = String(e.target.value || "").toLowerCase().trim();
    refreshKeyDropdownsPreserveSelection();
  });

  $("btn_ppt_toggle").addEventListener("click", async () => {
    // toggle: 현재 상태를 모르므로 일단 status 한번 읽고 반전
    const st = await apiGet("/api/status");
    const cur = !!(st.e10 && st.e10.ppt_mode);
    await control({ ppt_mode: !cur });
  });

  $("btn_dpi_1").addEventListener("click", () => control({ dpi_level: 1 }));
  $("btn_dpi_2").addEventListener("click", () => control({ dpi_level: 2 }));
  $("btn_dpi_3").addEventListener("click", () => control({ dpi_level: 3 }));
}

window.addEventListener("load", async () => {
  bind();
  await loadKeycodes();
  await reloadConfig();

  await pollStatus();
  if (g_pollTimer) clearInterval(g_pollTimer);
  g_pollTimer = setInterval(pollStatus, 1000); // 1s polling
});

