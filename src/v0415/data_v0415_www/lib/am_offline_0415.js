/* =======================================================
   File: /www/lib/am_offline_0415.js
   Elite AirMouse WebConfig v0415 — Offline API Router
   -------------------------------------------------------
   소스명 : am_offline_0415.js
   모듈약어 : OFFLINE
   모듈명 : Offline Simulator - API Router
   -------------------------------------------------------
   기능 요약
    - 오프라인 시뮬레이터 API 라우터 (handleOfflineApi 전담)
      · /api/profiles           (GET)
      · /api/profiles/active    (GET, POST)
      · /api/profiles/switch    (POST)
      · /api/profiles/create    (POST)
      · /api/profiles/delete    (POST)
      · /api/profiles/rename    (POST)
      · /api/triggers           (GET)
      · /api/keycodes           (GET)
      · /api/status             (GET)
      · /api/diag               (GET)
      · /api/diag/clear         (POST)
      · /api/control            (POST, set_mode 지원)
      · /api/action/test        (POST)
      · /api/action/test_macro  (POST)
      · /api/ppt/test           (POST)
      · /api/hosts              (GET)
      · /api/hosts/select       (POST)
      · /api/hosts/cancel       (POST)
      · /api/hosts/label        (POST)
      · /api/config/import      (POST)
      · /api/safeboot           (GET, POST)
      · /api/factory_reset      (POST)
      · /api/reboot/check       (GET)

   [v0415 Critical Fix]
    - Consumer mask Descriptor 정합 (24-bit)
    - 스키마 412 / api_ver 411
    - ppt_mode → active_mode 일원화
    - Mode 3 슬롯 p32 재매핑 (WWW_*, KB_POWER)
    - Wave F: set_mode 지원 (active_mode 실제 갱신)

   [v0415 rev18.1 - Phase 5-B 분할]
    - 본 .js는 3-모듈로 분할됨
      · am_offline_data_0415.js   : 목업 데이터 (B-2)
      · am_offline_store_0415.js  : localStorage 스토어 (B-3)
      · am_offline_0415.js        : 본 파일 (API 라우터)
    - 로드 순서 SSOT:
      am_base → am_i18n → am_offline_data → am_offline_store → am_offline
    - handleOfflineApi는 data + store 모듈에 의존

   [코드 네이밍 규칙]
    - 전역 상수           : G_모듈약어_ 접두사
    - 전역 변수           : g_모듈약어_ 접두사
    - 함수                : camelCase
    - 로컬 변수           : v_ 접두사
   ======================================================= */

/* =======================================================
   오프라인 모킹 라우터
   -------------------------------------------------------
    - data 모듈 (G_OFFLINE_KEYCODES / G_OFFLINE_TRIGGERS /
      G_OFFLINE_DEFAULT_PROFILE_0 / G_OFFLINE_START_TIME) 의존
    - store 모듈 (g_offlineStore / loadOfflineStore /
      saveOfflineStore / OFFLINE_STORAGE_KEY) 의존
   ======================================================= */
function handleOfflineApi(url, method, body) {
  if (!g_offlineStore) loadOfflineStore();

  const u = new URL(url, "http://localhost");
  const path = u.pathname;
  const compact = (u.searchParams.get("compact") === "1");
  const flat    = (u.searchParams.get("flat") !== "0");

  let resObj = { ok: true, code: "ok", msg: "", data: null };

  if (path === "/api/profiles") {
    if (method === "GET") {
      resObj.data = {
        active: g_offlineStore.active,
        count: g_offlineStore.profiles.length,
        profiles: g_offlineStore.profiles
      };
    }
  }
  else if (path === "/api/profiles/active") {
    if (method === "GET") {
      const activeIdx = g_offlineStore.active;
      if (!g_offlineStore.profileData[activeIdx]) {
        g_offlineStore.profileData[activeIdx] =
          JSON.parse(JSON.stringify(G_OFFLINE_DEFAULT_PROFILE_0));
      }
      resObj.data = {
        idx: activeIdx,
        count: g_offlineStore.profiles.length,
        config: g_offlineStore.profileData[activeIdx]
      };
    } else if (method === "POST") {
      const activeIdx = g_offlineStore.active;
      if (!g_offlineStore.profileData[activeIdx]) {
        g_offlineStore.profileData[activeIdx] =
          JSON.parse(JSON.stringify(G_OFFLINE_DEFAULT_PROFILE_0));
      }
      const target = g_offlineStore.profileData[activeIdx];

      function _deepMerge(dst, src) {
        if (src === null || src === undefined) return dst;
        if (Array.isArray(src) || typeof src !== "object") return src;
        if (typeof dst !== "object" || dst === null || Array.isArray(dst)) {
          dst = {};
        }
        for (const k of Object.keys(src)) {
          const v = src[k];
          if (v === undefined) continue;
          if (v === null) { dst[k] = null; continue; }
          if (Array.isArray(v)) { dst[k] = JSON.parse(JSON.stringify(v)); continue; }
          if (typeof v === "object") {
            dst[k] = _deepMerge(dst[k], v);
            continue;
          }
          dst[k] = v;
        }
        return dst;
      }

      if (body) {
        if (body.name   !== undefined) target.name   = body.name;
        if (body.e10    !== undefined) target.e10    = _deepMerge(target.e10  || {}, body.e10);
        if (body.wifi   !== undefined) target.wifi   = _deepMerge(target.wifi || {}, body.wifi);
        if (body.slots  !== undefined) target.slots  = JSON.parse(JSON.stringify(body.slots));
        if (body.macros !== undefined) target.macros = JSON.parse(JSON.stringify(body.macros));
      }
      saveOfflineStore();
      resObj.data = { idx: activeIdx, saved: true, reloaded: true };
    }
  }
  else if (path === "/api/profiles/switch") {
    if (method === "POST" && body && typeof body.idx === "number") {
      g_offlineStore.active = body.idx;
      saveOfflineStore();
      resObj.data = { active: g_offlineStore.active };
    }
  }
  else if (path === "/api/profiles/create") {
    if (method === "POST" && body) {
      if (g_offlineStore.profiles.length >= 5) {
        return {
          ok: false, status: 400,
          text: JSON.stringify({ ok: false, code: "limit_reached", msg: "최대 5개까지 생성 가능합니다." }),
          json: { ok: false, code: "limit_reached", msg: "최대 5개" }
        };
      }
      const used = new Set(g_offlineStore.profiles.map(p => p.idx));
      let newIdx = 0;
      while (used.has(newIdx) && newIdx < 5) newIdx++;
      const newName = (body.name || ("Profile " + newIdx)).trim().substring(0, 15);
      const newCfg = JSON.parse(JSON.stringify(G_OFFLINE_DEFAULT_PROFILE_0));
      newCfg.name = newName;
      g_offlineStore.profiles.push({ idx: newIdx, name: newName });
      g_offlineStore.profileData[newIdx] = newCfg;
      g_offlineStore.active = newIdx;
      saveOfflineStore();
      resObj.data = { idx: newIdx, count: g_offlineStore.profiles.length };
    }
  }
  else if (path === "/api/profiles/delete") {
    if (method === "POST" && body && typeof body.idx === "number") {
      if (g_offlineStore.profiles.length <= 1) {
        return {
          ok: false, status: 400,
          text: JSON.stringify({ ok: false, code: "cannot_delete", msg: "최소 1개의 프로파일이 필요합니다." }),
          json: { ok: false, code: "cannot_delete", msg: "최소 1개" }
        };
      }
      g_offlineStore.profiles = g_offlineStore.profiles.filter(p => p.idx !== body.idx);
      delete g_offlineStore.profileData[body.idx];
      if (g_offlineStore.active === body.idx) {
        g_offlineStore.active = g_offlineStore.profiles[0].idx;
      }
      saveOfflineStore();
      resObj.data = { deleted: body.idx, count: g_offlineStore.profiles.length };
    }
  }
  else if (path === "/api/profiles/rename") {
    if (method === "POST" && body && typeof body.idx === "number") {
      const p = g_offlineStore.profiles.find(item => item.idx === body.idx);
      if (p) {
        p.name = (body.name || p.name).trim().substring(0, 15);
        if (g_offlineStore.profileData[body.idx]) {
          g_offlineStore.profileData[body.idx].name = p.name;
        }
        saveOfflineStore();
      }
      resObj.data = { idx: body.idx, name: p ? p.name : "" };
    }
  }
  else if (path === "/api/triggers") {
    resObj.data = { count: G_OFFLINE_TRIGGERS.length, triggers: G_OFFLINE_TRIGGERS };
  }
  else if (path === "/api/keycodes") {
    resObj.data = G_OFFLINE_KEYCODES;
  }

  else if (path === "/api/status") {
    const act = g_offlineStore.active;
    const actP = g_offlineStore.profiles.find(p => p.idx === act) || { idx: 0, name: "Default" };

    const gSys = { uptime_ms: Date.now() - G_OFFLINE_START_TIME, api_ver: 411 };
    const gMem = { heap_free: 245760, heap_min_free: 220000, heap_max_alloc: 245760 };
    const gNet = { mode: "OFFLINE", ssid: "Local / Offline", ip: "127.0.0.1", rssi: 0, mdns: "elite-airmouse.local" };
    const gDiag = { body_too_large: 0, no_body_slot: 0, json_bad: 0, safe_blocked: 0, ota_blocked: 0 };

    // [v0415 Wave F] 오프라인 active_mode를 실제 저장값에서 반영
    const v_activeE10 = g_offlineStore.profileData[act]?.e10 || {};
    const gE10 = {
      ble_connected: false,
      active_mode: v_activeE10.active_mode ?? 1,
      ble_peer_name: "",
      dpi_level: v_activeE10.dpi_level ?? 2,
      precision_mode: v_activeE10.precision?.mode ?? 0,
      gate: { ota_guard: false }
    };

    const gBoot = { safe_mode: false, fail_count: 0, pending: false, last_reset_reason: 0 };
    const gConfig = {
      ver: 412,
      etag_ok: false, etag: 0, size: 0,
      profile_idx: act,
      profile_count: g_offlineStore.profiles.length,
      profile_name: actP.name,
      last_apply_ok: true, last_apply_ms: 0, last_apply_age_ms: 0,
      last_apply_code: "", last_apply_src: ""
    };
    const gFeatures = { etag_config: true, reboot_api: true, safe_mode_policy: true, ota_guard: true, e10_observability: true };
    const gOta = { in_progress: false, total: 0, written: 0, ok: false, err: "none" };
    const gPolicy = {
      reboot_required: false, reboot_reason_mask: 0,
      reboot_reasons: "", safe_mode_api_limited: false,
      ota_upload_blocked: false
    };

    const topData = {
      uptime_ms: gSys.uptime_ms,
      heap_free: gMem.heap_free,
      heap_min_free: gMem.heap_min_free,
      heap_max_alloc: gMem.heap_max_alloc,
      api_ver: 411,
      sys: { ...gSys },
      mem: { ...gMem },
      net: { ...gNet },
      diag: { ...gDiag },
      e10: { ...gE10 },
      boot: { ...gBoot },
      config: { ...gConfig },
      features: { ...gFeatures },
      ota: { ...gOta },
      policy: { ...gPolicy },
      groups: {
        sys: gSys, mem: gMem, net: gNet, diag: gDiag, e10: gE10,
        boot: gBoot, config: gConfig, features: gFeatures, ota: gOta
      }
    };

    if (compact || !flat) {
      delete topData.uptime_ms;
      delete topData.heap_free;
      delete topData.heap_min_free;
      delete topData.heap_max_alloc;
      delete topData.api_ver;
    }
    if (compact) {
      delete topData.sys;
      delete topData.mem;
      delete topData.net;
      delete topData.diag;
      delete topData.ota;
      delete topData.boot;
      delete topData.config;
      delete topData.e10;
      delete topData.features;
    }
    resObj.data = topData;
  }

  else if (path === "/api/diag") {
    resObj.data = {
      diag: { body_too_large: 0, no_body_slot: 0, json_bad: 0, safe_blocked: 0, ota_blocked: 0 },
      events: [{ ms: Date.now() - G_OFFLINE_START_TIME, code: "OFFLINE_MODE_ACTIVE" }]
    };
  }
  else if (path === "/api/diag/clear") {
    resObj.data = { cleared: true };
  }
  else if (path === "/api/control") {
    // ====================================================
    // [v0415 Wave F] 오프라인 시뮬레이터 set_mode 지원
    //   - 활성 프로파일의 e10.active_mode를 실제로 갱신
    //   - 온라인 C10 저장과 동일한 효과 재현
    // ====================================================
    if (body && body.cmd === "set_mode") {
      const v_mode = Math.max(1, Math.min(3, body.mode | 0));
      const activeIdx = g_offlineStore.active;
      const target = g_offlineStore.profileData[activeIdx];
      if (target) {
        target.e10 = target.e10 || {};
        target.e10.active_mode = v_mode;
        saveOfflineStore();
      }
      resObj.msg = "오프라인: 모드 전환 시뮬레이션";
      resObj.data = { cmd: "set_mode", mode: v_mode, simulated: true };
    } else {
      resObj.msg = "오프라인 모드: 시뮬레이션 명령 실행됨";
      resObj.data = { cmd: body && body.cmd, simulated: true };
    }
  }

  else if (path === "/api/action/test") {
    resObj.msg = "오프라인 모드: 가상 액션 테스트 시뮬레이션";
    resObj.data = body;
  }
  else if (path === "/api/action/test_macro") {
    resObj.msg = "오프라인 모드: 가상 매크로 테스트 시뮬레이션";
    resObj.data = body;
  }
  else if (path === "/api/ppt/test") {
    resObj.msg = "오프라인 모드: 단발 키 테스트 시뮬레이션";
    resObj.data = body;
  }
  else if (path === "/api/hosts") {
    g_offlineStore.hosts = g_offlineStore.hosts || [
      { idx: 0, addr: "AA:BB:CC:DD:EE:01", label: "My PC", color: "#3B82F6", is_active: true, is_target: false },
      { idx: 1, addr: "AA:BB:CC:DD:EE:02", label: "Tablet", color: "#10B981", is_active: false, is_target: false }
    ];
    resObj.data = {
      bond_count: g_offlineStore.hosts.length,
      active_idx: g_offlineStore.activeHostIdx ?? 0,
      target_idx: g_offlineStore.targetHostIdx ?? 255,
      is_selecting: !!g_offlineStore.isSelectingHost,
      active_peer_name: g_offlineStore.hosts[g_offlineStore.activeHostIdx ?? 0]?.addr || "",
      hosts: g_offlineStore.hosts
    };
  }
  else if (path === "/api/hosts/select") {
    const idx = body ? body.idx : 0;
    g_offlineStore.activeHostIdx = idx;
    g_offlineStore.hosts?.forEach(h => { h.is_active = (h.idx === idx); });
    resObj.msg = "오프라인: 호스트 전환 시뮬레이션";
    resObj.data = { idx: idx, simulated: true };
  }
  else if (path === "/api/hosts/cancel") {
    g_offlineStore.isSelectingHost = false;
    resObj.msg = "오프라인: 호스트 전환 취소 시뮬레이션";
    resObj.data = { cancelled: true };
  }
  else if (path === "/api/hosts/label") {
    if (body && body.addr) {
      const h = g_offlineStore.hosts?.find(x => x.addr === body.addr);
      if (h) {
        if (body.label) h.label = body.label;
        if (body.color) h.color = body.color;
      }
    }
    resObj.msg = "오프라인: 호스트 라벨 저장 시뮬레이션";
    resObj.data = body;
  }
  else if (path === "/api/config/import") {
    if (method === "POST" && body) {
      if (g_offlineStore.profiles.length >= 5) {
        return {
          ok: false, status: 400,
          text: JSON.stringify({ ok: false, code: "limit_reached", msg: "최대 5개까지 가능합니다." }),
          json: { ok: false, code: "limit_reached" }
        };
      }
      const used = new Set(g_offlineStore.profiles.map(p => p.idx));
      let newIdx = 0;
      while (used.has(newIdx) && newIdx < 5) newIdx++;
      const impName = (body.name || ("Import_" + newIdx)).substring(0, 15);
      body.name = impName;
      g_offlineStore.profiles.push({ idx: newIdx, name: impName });
      g_offlineStore.profileData[newIdx] = JSON.parse(JSON.stringify(body));
      g_offlineStore.active = newIdx;
      saveOfflineStore();
      resObj.data = { idx: newIdx, count: g_offlineStore.profiles.length };
    }
  }
  else if (path === "/api/safeboot") {
    if (method === "POST" && body && body.exit === true) {
      resObj.data = { exit: true, offline: true, note: "Offline: no actual reboot." };
    } else {
      resObj.data = { safe_mode: false, fail_count: 0, pending: false, offline: true };
    }
  }
  else if (path === "/api/factory_reset") {
    if (method === "POST") {
      try { localStorage.removeItem(OFFLINE_STORAGE_KEY); } catch (e) { }
      g_offlineStore = null;
      loadOfflineStore();
      resObj.data = { reset: true, offline: true };
    }
  }
  else if (path === "/api/ota" || path === "/api/ota/status") {
    resObj.data = { status: "idle", progress: 0, ota_guard: true, offline: true };
    resObj.msg = "OTA simulated mode";
  }
  else {
    resObj.msg = "Offline stub response";
  }

  const jsonStr = JSON.stringify(resObj);
  return { ok: true, status: 200, text: jsonStr, json: resObj };
}
