// ======================================================
// File: data/www/app.js
// ======================================================

const $ = (id) => document.getElementById(id);

function setStatus(msg) {
    $("status").textContent = msg;
}

function cfgToUI(cfg) {
    $("dpi_level").value = String(cfg.dpi_level ?? 2);
    $("hard_click_lock").checked = !!cfg.hard_click_lock;
    
    $("accel_threshold").value = cfg.accel_threshold ?? 8.0;
    $("scroll_cursor_damp").value = cfg.scroll_cursor_damp ?? 0.25;
    
    $("scale_base_1").value = cfg.scale_base?.[0] ?? 0.55;
    $("scale_base_2").value = cfg.scale_base?.[1] ?? 0.75;
    $("scale_base_3").value = cfg.scale_base?.[2] ?? 1.00;
    
    $("accel_gain_1").value = cfg.accel_gain?.[0] ?? 0.35;
    $("accel_gain_2").value = cfg.accel_gain?.[1] ?? 0.55;
    $("accel_gain_3").value = cfg.accel_gain?.[2] ?? 0.85;
    
    $("wheel_threshold_deg").value = cfg.wheel?.threshold_deg ?? 90.0;
    $("wheel_step_max").value = cfg.wheel?.step_max ?? 6;
    
    $("gesture_flick_deg").value = cfg.gesture?.flick_deg ?? 200.0;
    $("gesture_cooldown_ms").value = cfg.gesture?.cooldown_ms ?? 600;
}

function uiToCfg() {
    return {
        dpi_level: parseInt($("dpi_level").value, 10),
        hard_click_lock: $("hard_click_lock").checked,
        
        accel_threshold: parseFloat($("accel_threshold").value),
        scroll_cursor_damp: parseFloat($("scroll_cursor_damp").value),
        
        scale_base: [
            parseFloat($("scale_base_1").value),
            parseFloat($("scale_base_2").value),
            parseFloat($("scale_base_3").value),
        ],
        accel_gain: [
            parseFloat($("accel_gain_1").value),
            parseFloat($("accel_gain_2").value),
            parseFloat($("accel_gain_3").value),
        ],
        
        wheel: {
            threshold_deg: parseFloat($("wheel_threshold_deg").value),
            step_max: parseInt($("wheel_step_max").value, 10),
        },
        
        gesture: {
            flick_deg: parseFloat($("gesture_flick_deg").value),
            cooldown_ms: parseInt($("gesture_cooldown_ms").value, 10),
        },
    };
}

async function loadConfig() {
    setStatus("Loading...");
    const r = await fetch("/api/config", { method: "GET" });
    const t = await r.text();
    if (!r.ok) {
        setStatus("Load failed: " + r.status + "\n" + t);
        return;
    }
    const cfg = JSON.parse(t);
    cfgToUI(cfg);
    setStatus("Loaded.\n" + JSON.stringify(cfg, null, 2));
}

async function saveConfig() {
    const cfg = uiToCfg();
    setStatus("Saving...\n" + JSON.stringify(cfg, null, 2));
    
    const r = await fetch("/api/config", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(cfg),
    });
    
    const t = await r.text();
    if (!r.ok) {
        setStatus("Save failed: " + r.status + "\n" + t);
        return;
    }
    
    setStatus("Saved OK.\n" + t);
    // 저장 후 재조회(서버 적용 확인)
    await loadConfig();
}

window.addEventListener("load", () => {
    $("btn_load").addEventListener("click", loadConfig);
    $("btn_save").addEventListener("click", saveConfig);
    $("btn_reset").addEventListener("click", resetDefaults);
    $("btn_reboot").addEventListener("click", rebootDevice);

    loadConfig().catch((e) => setStatus("Load error: " + e));
});