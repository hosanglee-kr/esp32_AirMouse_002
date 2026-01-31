/*-- ======================================================
File: data/www/app_011.js
====================================================== -->
*/

const $ = (id) => document.getElementById(id);

function setStatus(msg) { $("status").textContent = msg; }

async function postJson(url, obj) {
  const r = await fetch(url, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: obj ? JSON.stringify(obj) : "",
  });
  const t = await r.text();
  return { ok: r.ok, status: r.status, text: t };
}

async function postNoBody(url) {
  const r = await fetch(url, { method: "POST" });
  const t = await r.text();
  return { ok: r.ok, status: r.status, text: t };
}

function cfgToUI(cfg) {
  $("dpi_level").value = String(cfg.dpi_level ?? 2);
  $("hard_click_lock").checked = !!cfg.hard_click_lock;

  $("accel_threshold").value = cfg.accel_threshold ?? 8.0;
  $("scroll_cursor_damp").value = cfg.scroll_cursor_damp ?? 0.25;

  const pk = cfg.ppt_keys || {};
  $("ppt_start_mod").value = pk.start?.mod ?? 225;
  $("ppt_start_key").value = pk.start?.key ?? 62;

  $("ppt_exit_mod").value = pk.exit?.mod ?? 0;
  $("ppt_exit_key").value = pk.exit?.key ?? 41;

  $("ppt_next_mod").value = pk.next?.mod ?? 0;
  $("ppt_next_key").value = pk.next?.key ?? 78;

  $("ppt_prev_mod").value = pk.prev?.mod ?? 0;
  $("ppt_prev_key").value = pk.prev?.key ?? 75;

  $("ppt_black_mod").value = pk.black?.mod ?? 0;
  $("ppt_black_key").value = pk.black?.key ?? 5;

  $("ppt_laser_mod").value = pk.laser?.mod ?? 224;
  $("ppt_laser_key").value = pk.laser?.key ?? 15;
}

function uiToCfg() {
  return {
    dpi_level: parseInt($("dpi_level").value, 10),
    hard_click_lock: $("hard_click_lock").checked,

    accel_threshold: parseFloat($("accel_threshold").value),
    scroll_cursor_damp: parseFloat($("scroll_cursor_damp").value),

    // 기존 필드들도 유지해야 하면 여기 확장(현재 예시는 PPT 중심)
    // (이미 쓰고 있는 scale_base/accel_gain/wheel/gesture 입력 UI가 있다면 함께 합치세요)

    ppt_keys: {
      start: { mod: parseInt($("ppt_start_mod").value, 10), key: parseInt($("ppt_start_key").value, 10) },
      exit:  { mod: parseInt($("ppt_exit_mod").value, 10),  key: parseInt($("ppt_exit_key").value, 10)  },
      next:  { mod: parseInt($("ppt_next_mod").value, 10),  key: parseInt($("ppt_next_key").value, 10)  },
      prev:  { mod: parseInt($("ppt_prev_mod").value, 10),  key: parseInt($("ppt_prev_key").value, 10)  },
      black: { mod: parseInt($("ppt_black_mod").value, 10), key: parseInt($("ppt_black_key").value, 10) },
      laser: { mod: parseInt($("ppt_laser_mod").value, 10), key: parseInt($("ppt_laser_key").value, 10) },
    }
  };
}

async function loadConfig() {
  setStatus("Loading...");
  const r = await fetch("/api/config", { method: "GET" });
  const t = await r.text();
  if (!r.ok) { setStatus("Load failed: " + r.status + "\n" + t); return; }
  const cfg = JSON.parse(t);
  cfgToUI(cfg);
  setStatus("Loaded.\n" + JSON.stringify(cfg, null, 2));
}

async function saveConfig() {
  const cfg = uiToCfg();
  setStatus("Saving...\n" + JSON.stringify(cfg, null, 2));

  const r = await postJson("/api/config", cfg);
  if (!r.ok) { setStatus("Save failed: " + r.status + "\n" + r.text); return; }

  let msg = r.text;
  try {
    const j = JSON.parse(r.text);
    msg = `Saved OK. applied=${j.applied}\n` + JSON.stringify(j, null, 2);
  } catch (e) {}

  setStatus(msg);
  await loadConfig();
}

async function resetDefaults() {
  setStatus("Resetting to defaults...");
  const r = await postNoBody("/api/reset");
  if (!r.ok) { setStatus("Reset failed: " + r.status + "\n" + r.text); return; }
  setStatus("Reset OK.\n" + r.text);
  await loadConfig();
}

async function rebootDevice() {
  setStatus("Rebooting...");
  const r = await postNoBody("/api/reboot");
  setStatus("Reboot requested.\n" + r.text + "\n(몇 초 후 페이지 새로고침)");
}

// 프리셋
function bindPresets() {
  $("ppt_start_preset").addEventListener("click", () => { $("ppt_start_mod").value = 225; $("ppt_start_key").value = 62; });
  $("ppt_exit_preset").addEventListener("click",  () => { $("ppt_exit_mod").value  = 0;   $("ppt_exit_key").value  = 41; });
  $("ppt_next_preset").addEventListener("click",  () => { $("ppt_next_mod").value  = 0;   $("ppt_next_key").value  = 78; });
  $("ppt_prev_preset").addEventListener("click",  () => { $("ppt_prev_mod").value  = 0;   $("ppt_prev_key").value  = 75; });
  $("ppt_black_preset").addEventListener("click", () => { $("ppt_black_mod").value = 0;   $("ppt_black_key").value = 5;  });
  $("ppt_laser_preset").addEventListener("click", () => { $("ppt_laser_mod").value = 224; $("ppt_laser_key").value = 15; });
}

window.addEventListener("load", () => {
  $("btn_load").addEventListener("click", loadConfig);
  $("btn_save").addEventListener("click", saveConfig);
  $("btn_reset").addEventListener("click", resetDefaults);
  $("btn_reboot").addEventListener("click", rebootDevice);

  bindPresets();
  loadConfig().catch((e) => setStatus("Load error: " + e));
});
