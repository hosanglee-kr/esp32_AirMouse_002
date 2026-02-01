// File: data/www/app_019.js
const $ = (id) => document.getElementById(id);

async function jget(url) { const r = await fetch(url); return await r.json(); }
async function jpost(url, body) {
    const r = await fetch(url, { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) });
    return await r.json();
}

let g_keycodes = { mods: [], keys: [] };

function renderKeySelect() {
    const q = ($("keySearch").value || "").toLowerCase().trim();
    const grp = $("keyGroup").value || "";
    
    const ks = g_keycodes.keys.filter(k => {
        const okQ = q ? (String(k.name).toLowerCase().includes(q)) : true;
        const okG = grp ? (k.group === grp) : true;
        return okQ && okG;
    });
    
    const sel = $("keySelect");
    sel.innerHTML = "";
    ks.slice(0, 800).forEach(k => {
        const opt = document.createElement("option");
        opt.value = JSON.stringify({ page: k.page, usage: k.usage });
        opt.textContent = `${k.name}  (p:${k.page} u:${k.usage})`;
        sel.appendChild(opt);
    });
}

function renderMods() {
    const sel = $("modSelect");
    sel.innerHTML = "";
    g_keycodes.mods.forEach(m => {
        const opt = document.createElement("option");
        opt.value = m.mask;
        opt.textContent = `${m.name} (0x${Number(m.mask).toString(16).padStart(2,"0")})`;
        sel.appendChild(opt);
    });
}

async function loadKeycodes() {
    g_keycodes = await jget("/api/keycodes");
    renderMods();
    renderKeySelect();
}

async function pollStatus() {
    const s = await jget("/api/status");
    $("statusJson").textContent = JSON.stringify(s, null, 2);
    
    const mode = s?.net?.mode || "?";
    const ip = s?.net?.ip || "?";
    const ble = s?.e10?.ble_connected ? "BLE:ON" : "BLE:OFF";
    const dpi = s?.e10?.dpi_level ?? "?";
    const ppt = s?.e10?.ppt_mode ? "PPT:ON" : "PPT:OFF";
    $("statusLine").textContent = `${mode} ${ip} | ${ble} | ${ppt} | DPI:${dpi}`;
}

async function loadConfig() {
    const c = await jget("/api/config");
    $("cfgJson").textContent = JSON.stringify(c, null, 2);
}

async function saveConfig() {
    let obj = {};
    try { obj = JSON.parse($("cfgJson").textContent || "{}"); }
    catch (e) { $("msgBox").textContent = "cfgJson is not valid JSON"; return; }
    
    const out = await jpost("/api/config", obj);
    $("msgBox").textContent = out.message || JSON.stringify(out);
}

async function resetConfig() {
    const out = await jpost("/api/reset", {});
    $("msgBox").textContent = out.message || JSON.stringify(out);
}

async function reboot() {
    await jpost("/api/reboot", {});
}

async function control(body) {
    const out = await jpost("/api/control", body);
    $("msgBox").textContent = out.ok ? "OK" : "FAIL";
}

function bind() {
    $("keySearch").addEventListener("input", renderKeySelect);
    $("keyGroup").addEventListener("change", renderKeySelect);
    
    $("btnLoad").onclick = loadConfig;
    $("btnSave").onclick = saveConfig;
    $("btnReset").onclick = resetConfig;
    $("btnReboot").onclick = reboot;
    
    $("btnPptOn").onclick = () => control({ ppt_mode: true });
    $("btnPptOff").onclick = () => control({ ppt_mode: false });
    $("btnDpi1").onclick = () => control({ dpi_level: 1 });
    $("btnDpi2").onclick = () => control({ dpi_level: 2 });
    $("btnDpi3").onclick = () => control({ dpi_level: 3 });
}

(async function main() {
    bind();
    await loadKeycodes();
    await loadConfig();
    await pollStatus();
    setInterval(pollStatus, 1000);
})();
