function qs(id) { return document.getElementById(id); }

async function apiGet(url) {
    const r = await fetch(url, { cache: "no-store" });
    const t = await r.text();
    let j = null;
    try { j = JSON.parse(t); } catch (e) {}
    return { ok: r.ok, status: r.status, text: t, json: j };
}

async function apiPostJson(url, obj) {
    const r = await fetch(url, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(obj),
        cache: "no-store"
    });
    const t = await r.text();
    let j = null;
    try { j = JSON.parse(t); } catch (e) {}
    return { ok: r.ok, status: r.status, text: t, json: j };
}

function pretty(o) {
    try { return JSON.stringify(o, null, 2); } catch (e) { return String(o); }
}

function setPill(el, text, good) {
    el.textContent = text;
    el.style.borderColor = good ? "rgba(76,125,255,.55)" : "rgba(39,48,72,.9)";
    el.style.background = good ? "rgba(76,125,255,.12)" : "rgba(255,255,255,.03)";
}

function fillSelect(el, items, valueKey, labelKey) {
    el.innerHTML = "";
    for (const it of items) {
        const opt = document.createElement("option");
        opt.value = String(it[valueKey]);
        opt.textContent = it[labelKey];
        el.appendChild(opt);
    }
}

function fillPageSelect(el) {
    el.innerHTML = "";
    const a = [
        { v: "kb", t: "kb" },
        { v: "consumer", t: "consumer" },
    ];
    for (const it of a) {
        const opt = document.createElement("option");
        opt.value = it.v;
        opt.textContent = it.t;
        el.appendChild(opt);
    }
}

function bindPptRow(prefix) {
    return {
        page: qs(prefix + "Page"),
        mod: qs(prefix + "Mod"),
        code: qs(prefix + "Code"),
    };
}

async function loadKeycodes() {
    const r = await apiGet("/api/keycodes");
    if (!r.ok || !r.json) throw new Error("keycodes load failed");
    
    const mods = r.json.mods || [];
    // page select은 직접 채움
    const modItems = mods.map(m => ({ mask: m.mask, name: `${m.name} (0x${Number(m.mask).toString(16)})` }));
    
    const modSelects = ["pptStartMod", "pptExitMod", "pptNextMod", "pptPrevMod", "pptBlackMod", "pptLaserMod"];
    for (const id of modSelects) {
        fillSelect(qs(id), modItems, "mask", "name");
    }
    
    const pageSelects = ["pptStartPage", "pptExitPage", "pptNextPage", "pptPrevPage", "pptBlackPage", "pptLaserPage"];
    for (const id of pageSelects) {
        fillPageSelect(qs(id));
    }
    
    return r.json;
}

function getPptPayload() {
    const rows = {
        start: bindPptRow("pptStart"),
        exit: bindPptRow("pptExit"),
        next: bindPptRow("pptNext"),
        prev: bindPptRow("pptPrev"),
        black: bindPptRow("pptBlack"),
        laser: bindPptRow("pptLaser"),
    };
    
    const map = {};
    for (const k of Object.keys(rows)) {
        const r = rows[k];
        map[k] = {
            page: r.page.value,
            mod: Number(r.mod.value) || 0,
            code: Number(r.code.value) || 0
        };
    }
    
    return {
        save: qs("swPptSave").checked,
        map
    };
}

function setPptForm(map) {
    const rows = {
        start: bindPptRow("pptStart"),
        exit: bindPptRow("pptExit"),
        next: bindPptRow("pptNext"),
        prev: bindPptRow("pptPrev"),
        black: bindPptRow("pptBlack"),
        laser: bindPptRow("pptLaser"),
    };
    
    for (const k of Object.keys(rows)) {
        const r = rows[k];
        const m = map[k] || {};
        r.page.value = (m.page === "consumer") ? "consumer" : "kb";
        r.mod.value = String(m.mod ?? 0);
        r.code.value = String(m.code ?? 0);
    }
}

async function refreshStatus() {
    const r = await apiGet("/api/status");
    if (!r.ok || !r.json) {
        qs("statusJson").textContent = r.text || "status failed";
        return;
    }
    
    const j = r.json;
    
    qs("stUptime").textContent = `${j.uptime_ms ?? "-"} ms`;
    qs("stHeap").textContent = `${j.heap_free ?? "-"} bytes`;
    
    const net = j.net || {};
    qs("stWiFi").textContent = `${net.mode ?? "-"} / ${net.ssid ?? "-"}`;
    qs("stMdns").textContent = `${net.mdns ?? "-"}`;
    
    const e10 = j.e10 || {};
    qs("stPptMode").textContent = String(e10.ppt_mode ?? "-");
    qs("stPrec").textContent = `enable:${e10.precision_enable ?? "-"} / mode:${e10.precision_mode ?? "-"}`;
    
    setPill(qs("pillNet"), `NET: ${net.mode ?? "-"}`, true);
    setPill(qs("pillBle"), `BLE: ${e10.ble_connected ? "ON" : "OFF"}`, !!e10.ble_connected);
    
    qs("swPpt").checked = !!e10.ppt_mode;
    qs("swPrec").checked = !!e10.precision_mode;
    if (e10.dpi_level !== undefined) qs("selDpi").value = String(e10.dpi_level);
    
    qs("statusJson").textContent = pretty(j);
}

async function applyControl() {
    const payload = {
        ppt_mode: qs("swPpt").checked,
        precision_mode: qs("swPrec").checked,
        dpi_level: Number(qs("selDpi").value) || 0
    };
    const r = await apiPostJson("/api/control", payload);
    if (!r.ok) {
        alert("control failed: " + (r.json?.err || r.text));
    }
    await refreshStatus();
}

async function pptReload() {
    const r = await apiGet("/api/ppt");
    if (!r.ok || !r.json) {
        alert("ppt load failed");
        return;
    }
    setPptForm(r.json.map || {});
}

async function pptSave() {
    const payload = getPptPayload();
    const r = await apiPostJson("/api/ppt", payload);
    if (!r.ok) {
        alert("ppt save failed: " + (r.json?.err || r.text));
        return;
    }
    await pptReload();
}

async function pptTest(action) {
    const rows = {
        start: bindPptRow("pptStart"),
        exit: bindPptRow("pptExit"),
        next: bindPptRow("pptNext"),
        prev: bindPptRow("pptPrev"),
        black: bindPptRow("pptBlack"),
        laser: bindPptRow("pptLaser"),
    };
    const r = rows[action];
    const payload = {
        page: r.page.value,
        mod: Number(r.mod.value) || 0,
        code: Number(r.code.value) || 0
    };
    const res = await apiPostJson("/api/ppt/test", payload);
    if (!res.ok) alert("test failed");
}

async function safeInfo() {
    const r = await apiGet("/api/safeboot");
    alert(pretty(r.json || r.text));
}

async function safeExit() {
    const r = await apiPostJson("/api/safeboot", { exit: true });
    alert(pretty(r.json || r.text));
}

async function factoryReset() {
    if (!confirm("Factory Reset 진행? (재부팅됨)")) return;
    const r = await fetch("/api/factory_reset", { method: "POST" });
    const t = await r.text();
    alert(t);
}

async function reboot() {
    if (!confirm("재부팅 할까요?")) return;
    await fetch("/api/reboot", { method: "POST" });
}

async function otaUpload() {
    const f = qs("otaFile").files?.[0];
    if (!f) { alert("파일 선택"); return; }
    
    qs("otaHint").textContent = `uploading: ${f.name} (${f.size} bytes)`;
    
    const r = await fetch("/api/ota", { method: "POST", body: f });
    const t = await r.text();
    qs("otaHint").textContent = t;
}

function bindUi() {
    qs("btnRefresh").addEventListener("click", refreshStatus);
    qs("btnReboot").addEventListener("click", reboot);
    
    qs("swPpt").addEventListener("change", applyControl);
    qs("swPrec").addEventListener("change", applyControl);
    qs("selDpi").addEventListener("change", applyControl);
    
    qs("btnPptReload").addEventListener("click", pptReload);
    qs("btnPptSave").addEventListener("click", pptSave);
    
    document.querySelectorAll("button[data-test]").forEach(b => {
        b.addEventListener("click", () => pptTest(b.getAttribute("data-test")));
    });
    
    qs("btnSafeInfo").addEventListener("click", safeInfo);
    qs("btnSafeExit").addEventListener("click", safeExit);
    qs("btnFactory").addEventListener("click", factoryReset);
    
    qs("btnOta").addEventListener("click", otaUpload);
}

async function main() {
    bindUi();
    await loadKeycodes();
    await pptReload();
    await refreshStatus();
    setInterval(refreshStatus, 2500);
}
main().catch(e => {
    qs("statusJson").textContent = String(e?.stack || e);
});

