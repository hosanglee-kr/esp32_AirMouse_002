/* =======================================================
   File: /www/lib/am_status_0410.js
   Elite AirMouse WebConfig v0410 — Status / Diag / OTA
   - 로드 순서: 6
   - 의존: am_base_0410.js
   ======================================================= */

/* =======================================================
   Status 새로고침
   ======================================================= */
async function refreshStatus() {
   updateAppModeUi();
   
   const r = await apiGet("/api/status?compact=1");
   const u = unwrapApi(r);
   if (!u.ok || !u.data) return;
   
   g_lastStatus = u.data;
   const g = u.data.groups || {};
   const sys = g.sys || {};
   const mem = g.mem || {};
   const net = g.net || {};
   const e10 = g.e10 || {};
   const boot = g.boot || {};
   const cfg = g.config || {};
   
   if (qs("stUptime")) qs("stUptime").textContent = `${sys.uptime_ms ?? "-"} ms`;
   if (qs("stHeap")) qs("stHeap").textContent = `${mem.heap_free ?? "-"} B`;
   if (qs("stWiFi")) qs("stWiFi").textContent = `${net.mode ?? "-"} / ${net.ssid ?? "-"}`;
   if (qs("stProfile")) {
      qs("stProfile").textContent =
         `#${cfg.profile_idx ?? "-"} ${cfg.profile_name ?? ""} / 총 ${cfg.profile_count ?? "-"}개`;
   }
   
   setPill(qs("pillBle"), `BLE: ${e10.ble_connected ? "ON" : "OFF"}`, !!e10.ble_connected);
   setPill(qs("pillSafe"), `SAFE: ${boot.safe_mode ? "ON" : "OFF"}`, !!boot.safe_mode);
   
   const el = qs("statusJson");
   if (el) el.textContent = pretty(u);
}

/* =======================================================
   Online / Offline 모드 전환
   ======================================================= */
function toggleAppMode() {
   const targetMode = (g_appMode === APP_MODE_ONLINE) ? APP_MODE_OFFLINE : APP_MODE_ONLINE;
   if (!confirm(`현재 모드: ${g_appMode}\n${targetMode} 모드로 전환하시겠습니까?`)) return;
   
   setAppMode(targetMode);
   setMsg(`모드 전환 완료: ${g_appMode}`, true);
   
   profileReloadAll().then(() => {
      renderSlotEditor();
      macroRenderList();
      macroRenderEditor();
      cfgLoad();
      refreshStatus();
   });
}

/* =======================================================
   Tab / Diag 상태 헬퍼
   ======================================================= */
function isTabOn(name) {
   const el = qs("tab-" + name);
   return !!(el && el.classList.contains("on"));
}

function isDiagTyping() {
   const f = qs("diagFilter");
   if (!f) return false;
   return (document.activeElement === f) || (nowMs() < g_diagTypingUntilMs);
}

/* =======================================================
   Diag 렌더
   ======================================================= */
function renderDiag(u) {
   const data = u.data || {};
   const d = data.diag || {};
   const events = Array.isArray(data.events) ? data.events : [];
   
   const cEl = qs("diagCounters");
   if (cEl) {
      cEl.innerHTML = "";
      const items = [
         ["body_too_large", d.body_too_large],
         ["no_body_slot", d.no_body_slot],
         ["bad_json", d.json_bad],
         ["safe_blocked", d.safe_blocked],
         ["ota_blocked", d.ota_blocked]
      ];
      for (const [k, v] of items) {
         const div = document.createElement("div");
         div.className = "pill";
         div.textContent = `${k}: ${v ?? 0}`;
         cEl.appendChild(div);
      }
   }
   
   const listEl = qs("diagEvents");
   if (listEl) {
      listEl.innerHTML = "";
      const f = (qs("diagFilter")?.value || "").trim();
      const shown = events.slice().reverse()
         .filter(ev => !f || String(ev.code || "").includes(f))
         .slice(0, 80);
      
      for (const ev of shown) {
         const div = document.createElement("div");
         div.className = "logline";
         div.textContent = `[${ev.ms ?? 0}ms] ${ev.code ?? "-"}`;
         listEl.appendChild(div);
      }
      
      if (!shown.length) {
         const div = document.createElement("div");
         div.className = "hint2";
         div.textContent = "No events.";
         listEl.appendChild(div);
      }
   }
   
   const rawEl = qs("diagJson");
   if (rawEl) rawEl.textContent = pretty(u);
}

async function refreshDiag() {
   const r = await apiGet("/api/diag");
   renderDiag(unwrapApi(r));
}

/* =======================================================
   OTA 업로드 / 상태
   ======================================================= */
async function otaUpload() {
   const f = qs("otaFile")?.files?.[0];
   if (!f) { alert("파일을 선택하세요."); return; }
   
   qs("otaHint").textContent = `uploading: ${f.name}...`;
   const r = await fetch("/api/ota", { method: "POST", body: f });
   qs("otaHint").textContent = await r.text();
   await otaStatus();
}

async function otaStatus() {
   const r = await apiGet("/api/ota/status");
   qs("otaJson").textContent = pretty(unwrapApi(r).data || r.text);
}
