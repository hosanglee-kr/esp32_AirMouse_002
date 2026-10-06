/* =======================================================
   File: /www/lib/am_offline_0414.js
   Elite AirMouse WebConfig v0414 — Offline Simulator
   - 로드 순서: 3
   - [Phase 2.0.2 N-2] boot 객체 SPEC 정합
   - [Phase 2.3 M-1/L-1] saveOfflineStore 가드
   - [Phase 2.4 M-11] 참조 반환 방지 (deep clone)
   - [Phase 2.5 H-3] 재귀 병합
   ======================================================= */

const G_OFFLINE_START_TIME = Date.now();

/* =======================================================
   오프라인 트리거 라이브러리 (27개)
   ======================================================= */
const G_OFFLINE_TRIGGERS = [
  { idx: 0, name: "Top L Click", group: "button", locked: true },
  { idx: 1, name: "Top L Double", group: "button", locked: false },
  { idx: 2, name: "Top L Long", group: "button", locked: false },
  { idx: 3, name: "Top M Click", group: "button", locked: false },
  { idx: 4, name: "Top M Hold", group: "button", locked: true },
  { idx: 5, name: "Top R Click", group: "button", locked: false },
  { idx: 6, name: "Top R Double", group: "button", locked: false },
  { idx: 7, name: "Top R Long", group: "button", locked: false },
  { idx: 8, name: "Side F Click", group: "button", locked: false },
  { idx: 9, name: "Side F Long", group: "button", locked: false },
  { idx: 10, name: "Side C Click", group: "button", locked: false },
  { idx: 11, name: "Side C Double", group: "button", locked: true },
  { idx: 12, name: "Side C 2s Hold", group: "button", locked: true },
  { idx: 13, name: "Side R Click", group: "button", locked: false },
  { idx: 14, name: "Side R Long", group: "button", locked: false },
  { idx: 15, name: "Flick Left", group: "gesture", locked: false },
  { idx: 16, name: "Flick Right", group: "gesture", locked: false },
  { idx: 17, name: "Flick Up", group: "gesture", locked: false },
  { idx: 18, name: "Flick Down", group: "gesture", locked: false },
  { idx: 19, name: "Linear Left", group: "gesture", locked: false },
  { idx: 20, name: "Linear Right", group: "gesture", locked: false },
  { idx: 21, name: "Linear Up", group: "gesture", locked: false },
  { idx: 22, name: "Linear Down", group: "gesture", locked: false },
  { idx: 23, name: "Tilt Left", group: "tilt", locked: false },
  { idx: 24, name: "Tilt Right", group: "tilt", locked: false },
  { idx: 25, name: "Tilt Up", group: "tilt", locked: false },
  { idx: 26, name: "Tilt Down", group: "tilt", locked: false }
];

/* =======================================================
   오프라인 키코드
   ======================================================= */
const G_OFFLINE_KEYCODES = {
  action_kinds: [
    { value: 0, name: "NONE", hasP16: false, hasP32: false },
    { value: 1, name: "MOUSE_CLICK", hasP16: true, hasP32: false, p16hint: "mask" },
    { value: 2, name: "MOUSE_HOLD", hasP16: true, hasP32: false, p16hint: "mask" },
    { value: 3, name: "MOUSE_WHEEL", hasP16: true, hasP32: false, p16hint: "axis|dir" },
    { value: 4, name: "KB_TAP", hasP16: true, hasP32: true, p16hint: "usage", p32hint: "mod" },
    { value: 5, name: "KB_COMBO", hasP16: false, hasP32: true, p32hint: "mod|u1|u2|u3" },
    { value: 6, name: "KB_REPEAT", hasP16: true, hasP32: true, p16hint: "usage", p32hint: "mod" },
    { value: 7, name: "CONSUMER_TAP", hasP16: false, hasP32: true, p32hint: "mask32" },
    { value: 8, name: "CONSUMER_REPEAT", hasP16: false, hasP32: true, p32hint: "mask32" },
    { value: 9, name: "SPECIAL", hasP16: true, hasP32: false, p16hint: "special" },
    { value: 10, name: "MACRO", hasP16: false, hasP32: true, p32hint: "macroIdx" }
  ],
  mods: [
    { name: "None", mask: 0 },
    { name: "L_CTRL", mask: 1 },
    { name: "L_SHIFT", mask: 2 },
    { name: "L_ALT", mask: 4 },
    { name: "L_GUI", mask: 8 },
    { name: "R_CTRL", mask: 16 },
    { name: "R_SHIFT", mask: 32 },
    { name: "R_ALT", mask: 64 },
    { name: "R_GUI", mask: 128 }
  ],
  consumer: [
    { name: "VOL_UP", mask: 0x00000001 },
    { name: "VOL_DOWN", mask: 0x00000002 },
    { name: "MUTE", mask: 0x00000004 },
    { name: "PLAY_PAUSE", mask: 0x00000008 },
    { name: "STOP", mask: 0x00000010 },
    { name: "NEXT_TRACK", mask: 0x00000020 },
    { name: "PREV_TRACK", mask: 0x00000040 },
    { name: "FF", mask: 0x00000080 },
    { name: "REWIND", mask: 0x00000100 },
    { name: "AC_BACK", mask: 0x00000200 },
    { name: "AC_HOME", mask: 0x00000400 },
    { name: "AC_SEARCH", mask: 0x00000800 },
    { name: "POWER", mask: 0x00001000 },
    { name: "TV_INPUT", mask: 0x00002000 },
    { name: "CH_UP", mask: 0x00004000 },
    { name: "CH_DOWN", mask: 0x00008000 }
  ],
  precision_modes: [
    { name: "OFF", value: 0 },
    { name: "LOW", value: 1 },
    { name: "MED", value: 2 },
    { name: "HIGH", value: 3 },
    { name: "PPT", value: 4 }
  ],
  specials: [
    { value: 0, name: "NONE" },
    { value: 1, name: "GYRO_RECALIB" },
    { value: 2, name: "SLEEP_NOW" },
    { value: 3, name: "MODE_CYCLE" },
    { value: 4, name: "PAIRING" },
    { value: 5, name: "HOST_CYCLE" }
  ],
  kb: (function () {
    const arr = [];
    const names = {
      0: "None", 4: "A", 5: "B", 6: "C", 7: "D", 8: "E", 9: "F", 10: "G", 11: "H", 12: "I", 13: "J", 14: "K", 15: "L",
      16: "M", 17: "N", 18: "O", 19: "P", 20: "Q", 21: "R", 22: "S", 23: "T", 24: "U", 25: "V", 26: "W", 27: "X", 28: "Y", 29: "Z",
      30: "1", 31: "2", 32: "3", 33: "4", 34: "5", 35: "6", 36: "7", 37: "8", 38: "9", 39: "0",
      40: "Enter", 41: "Esc", 42: "Backspace", 43: "Tab", 44: "Space",
      58: "F1", 59: "F2", 60: "F3", 61: "F4", 62: "F5", 63: "F6", 64: "F7", 65: "F8", 66: "F9", 67: "F10", 68: "F11", 69: "F12",
      75: "PageUp", 78: "PageDown", 79: "Right", 80: "Left", 81: "Down", 82: "Up"
    };
    for (let c = 0; c <= 0xE7; c++) {
      arr.push({ code: c, name: names[c] || ("0x" + c.toString(16).toUpperCase().padStart(2, "0")) });
    }
    return arr;
  })(),

  directions: [
    { name: "LEFT" }, { name: "RIGHT" }, { name: "UP" }, { name: "DOWN" }
  ],
  groups: [
    { name: "slots", count: 15 },
    { name: "flick", count: 4 },
    { name: "linear", count: 4 },
    { name: "tilt", count: 4 }
  ],
  slots_meta: [
    { id: "S1", label: "Top L Click", btn: "TOP_L", evt: "CLICK" },
    { id: "S2", label: "Top L Double", btn: "TOP_L", evt: "DOUBLE" },
    { id: "S3", label: "Top L Long", btn: "TOP_L", evt: "LONG" },
    { id: "S4", label: "Top M Click", btn: "TOP_M", evt: "CLICK" },
    { id: "S5", label: "Top M Hold", btn: "TOP_M", evt: "HOLD" },
    { id: "S6", label: "Top R Click", btn: "TOP_R", evt: "CLICK" },
    { id: "S7", label: "Top R Double", btn: "TOP_R", evt: "DOUBLE" },
    { id: "S8", label: "Top R Long", btn: "TOP_R", evt: "LONG" },
    { id: "S9", label: "Side F Click", btn: "SIDE_F", evt: "CLICK" },
    { id: "S10", label: "Side F Long", btn: "SIDE_F", evt: "LONG" },
    { id: "S11", label: "Side C Click", btn: "SIDE_C", evt: "CLICK" },
    { id: "S12", label: "Side C Double", btn: "SIDE_C", evt: "DOUBLE" },
    { id: "S13", label: "Side C 2s Hold", btn: "SIDE_C", evt: "HOLD_2S" },
    { id: "S14", label: "Side R Click", btn: "SIDE_R", evt: "CLICK" },
    { id: "S15", label: "Side R Long", btn: "SIDE_R", evt: "LONG" }
  ],

  triggers: G_OFFLINE_TRIGGERS,
  trigger_count: G_OFFLINE_TRIGGERS.length,
  api_ver: 410,
  note: "v0412: profile-based slots (Global + Mode Override). " +
    "action_kinds + specials + triggers for UI. " +
    "mods mask == HID modifier byte. " +
    "kb=usage-id(0x07), consumer=32-bit mask. " +
    "MACRO kind(10) references macro index via p32."
};

/* =======================================================
   오프라인 기본 프로파일 (SPEC rev6 정합)
   ======================================================= */
const G_OFFLINE_DEFAULT_PROFILE_0 = {
  ver: 410,
  name: "Default",
  wifi: {
    mode: 0,
    sta: { ssid: "", pass: "" },
    ap: { ssid: "EliteAirMouse", pass: "12345678" },
    mdns: { host: "elite-airmouse" }
  },
  e10: {
    dpi_level: 2,
    hard_click_lock: true,
    scale_base: [0.55, 0.75, 1.0],
    accel_gain: [0.35, 0.55, 0.85],
    accel_threshold: 8.0,
    scroll_cursor_damp: 0.25,
    wheel: { threshold_deg: 90.0, step_max: 6 },
    gesture: { flick_deg: 200.0, cooldown_ms: 600 },
    precision: {
      mode: 0, deadzone: 1.2, gain: 0.65, accel: 0.25,
      max_step: 18, smooth: 0.85, entry_ms: 450, exit_ms: 300,
      entry_still_deg: 1.2, exit_move_deg: 3.5, profile: 0
    },
    led_brightness: 128,
    battery_adc_enabled: false,
    gyro_bias: { still_th: 2.0, still_win_ms: 250, alpha: 0.001 },
    linear: { th: 0.3, impulse_th: 0.5, window_ms: 300 },
    flick: { p2p_th: 400.0, window_ms: 200, cooldown_ms: 600 },
    tilt_hold: { angle_deg: 15.0, hold_ms: 300, repeat_hz: 3 },
    sleep_idle_timeout_ms: 60000,
    active_mode: 1,
    active_peer_index: 0,

    // [Phase 1~3] Motion Advanced
    motion_adv: {
      click_freeze: {
        enable: true,
        gyro_th: 15.0,
        max_ms: 150,
        hold_ms: 20,
        fadeout_ms: 30,
        move_th: 2.0,
        freeze_move_th: 30.0
      },
      ema: {
        alpha_min: 0.05,
        alpha_max: 0.80,
        deadzone_th: 3.0,
        fast_th: 15.0,
        reversal_th: 8.0,
        reversal_reset: true
      },
      snap: {
        enable: true,
        mode_mask: 0x02,
        axis_mode: 0,
        confirm_frames: 3,
        ratio_enter: 4.0,
        strength: 0.85
      }
    },

    // [Phase 11.6] Power (SPEC rev6)
    power: {
      idle_timeout_ms: [60000, 120000, 300000],
      idle_timeout_ble_ms: 300000,
      pairing_idle_timeout_ms: 30000,
      deep_idle_timeout_ms: 600000,
      wake_min_active_ms: 500,
      wom_threshold: 25,
      wom_duration: 4,
      fast_recalib_ms: 300,
      led_fadeout_ms: 500,
      led_fadein_ms: 300
    },

    // [Phase 11.7] Button (SPEC rev6)
    button: {
      debounce_press_ms: 32,
      debounce_release_ms: 16,
      long_delay_ms: 800,
      double_delay_ms: 320,
      hold_2s_ms: 2000,
      hold_3s_ms: 3000,
      min_click_ms: 16,
      debounce_min_ticks: 3
    }
  },

  slots: {
    global: [
      { k: 2, h: 1, p16: 1, p32: 0 },
      { k: 1, h: 0, p16: 1, p32: 0 },
      { k: 4, h: 0, p16: 43, p32: 4 },
      { k: 1, h: 0, p16: 4, p32: 0 },
      { k: 0, h: 0, p16: 0, p32: 0 },
      { k: 1, h: 0, p16: 2, p32: 0 },
      { k: 4, h: 0, p16: 41, p32: 0 },
      { k: 4, h: 0, p16: 22, p32: 10 },
      { k: 7, h: 0, p16: 0, p32: 1 },
      { k: 7, h: 0, p16: 0, p32: 32 },
      { k: 7, h: 0, p16: 0, p32: 8 },
      { k: 0, h: 0, p16: 0, p32: 0 },
      { k: 0, h: 0, p16: 0, p32: 0 },
      { k: 7, h: 0, p16: 0, p32: 2 },
      { k: 7, h: 0, p16: 0, p32: 64 },
      { k: 5, h: 0, p16: 0, p32: 20489 },
      { k: 5, h: 0, p16: 0, p32: 20233 },
      { k: 4, h: 0, p16: 7, p32: 8 },
      { k: 4, h: 0, p16: 43, p32: 4 },
      { k: 0, h: 0, p16: 0, p32: 0 },
      { k: 0, h: 0, p16: 0, p32: 0 },
      { k: 0, h: 0, p16: 0, p32: 0 },
      { k: 0, h: 0, p16: 0, p32: 0 },
      { k: 0, h: 0, p16: 0, p32: 0 },
      { k: 0, h: 0, p16: 0, p32: 0 },
      { k: 0, h: 0, p16: 0, p32: 0 },
      { k: 0, h: 0, p16: 0, p32: 0 }
    ],
    modes: [
      { mask: 0, slots: [] },
      {
        mask: 134217727,
        slots: [
          { k: 4, h: 0, p16: 78, p32: 0 },
          { k: 4, h: 0, p16: 75, p32: 0 },
          { k: 4, h: 0, p16: 62, p32: 2 },
          { k: 4, h: 0, p16: 15, p32: 1 },
          { k: 0, h: 0, p16: 0, p32: 0 },
          { k: 4, h: 0, p16: 41, p32: 0 },
          { k: 4, h: 0, p16: 5, p32: 0 },
          { k: 4, h: 0, p16: 26, p32: 0 },
          { k: 4, h: 0, p16: 78, p32: 0 },
          { k: 4, h: 0, p16: 62, p32: 2 },
          { k: 4, h: 0, p16: 19, p32: 1 },
          { k: 0, h: 0, p16: 0, p32: 0 },
          { k: 0, h: 0, p16: 0, p32: 0 },
          { k: 4, h: 0, p16: 75, p32: 0 },
          { k: 4, h: 0, p16: 41, p32: 0 },
          { k: 4, h: 0, p16: 78, p32: 0 },
          { k: 4, h: 0, p16: 75, p32: 0 },
          { k: 4, h: 0, p16: 5, p32: 0 },
          { k: 0, h: 0, p16: 0, p32: 0 },
          { k: 0, h: 0, p16: 0, p32: 0 },
          { k: 0, h: 0, p16: 0, p32: 0 },
          { k: 0, h: 0, p16: 0, p32: 0 },
          { k: 0, h: 0, p16: 0, p32: 0 },
          { k: 0, h: 0, p16: 0, p32: 0 },
          { k: 0, h: 0, p16: 0, p32: 0 },
          { k: 0, h: 0, p16: 0, p32: 0 },
          { k: 0, h: 0, p16: 0, p32: 0 }
        ]
      },
      {
        mask: 134217727,
        slots: [
          { k: 7, h: 0, p16: 0, p32: 512 },
          { k: 7, h: 0, p16: 0, p32: 1024 },
          { k: 7, h: 0, p16: 0, p32: 4096 },
          { k: 4, h: 0, p16: 40, p32: 0 },
          { k: 0, h: 0, p16: 0, p32: 0 },
          { k: 7, h: 0, p16: 0, p32: 1024 },
          { k: 7, h: 0, p16: 0, p32: 8192 },
          { k: 7, h: 0, p16: 0, p32: 4096 },
          { k: 7, h: 0, p16: 0, p32: 1 },
          { k: 7, h: 0, p16: 0, p32: 16384 },
          { k: 7, h: 0, p16: 0, p32: 8 },
          { k: 0, h: 0, p16: 0, p32: 0 },
          { k: 0, h: 0, p16: 0, p32: 0 },
          { k: 7, h: 0, p16: 0, p32: 2 },
          { k: 7, h: 0, p16: 0, p32: 32768 },
          { k: 7, h: 0, p16: 0, p32: 256 },
          { k: 7, h: 0, p16: 0, p32: 128 },
          { k: 7, h: 0, p16: 0, p32: 4 },
          { k: 0, h: 0, p16: 0, p32: 0 },
          { k: 0, h: 0, p16: 0, p32: 0 },
          { k: 0, h: 0, p16: 0, p32: 0 },
          { k: 0, h: 0, p16: 0, p32: 0 },
          { k: 0, h: 0, p16: 0, p32: 0 },
          { k: 4, h: 0, p16: 80, p32: 0 },
          { k: 4, h: 0, p16: 79, p32: 0 },
          { k: 4, h: 0, p16: 82, p32: 0 },
          { k: 4, h: 0, p16: 81, p32: 0 }
        ]
      }
    ]
  },
  macros: []
};

/* =======================================================
   오프라인 로컬 저장소
   ======================================================= */
const OFFLINE_STORAGE_KEY = "airmouse_v0412_offline_store";
const OFFLINE_STORE_SCHEMA = 6;   // [Phase 2.0] power/button SPEC rev6 동기화
let g_offlineStore = null;

function loadOfflineStore() {
  try {
    const raw = localStorage.getItem(OFFLINE_STORAGE_KEY);
    if (raw) {
      const parsed = JSON.parse(raw);
      if (parsed
        && typeof parsed === "object"
        && parsed._v === OFFLINE_STORE_SCHEMA
        && parsed.profiles) {
        g_offlineStore = parsed;
        return;
      }
      console.info("[offline] storage schema mismatch → reset",
        "found:", parsed && parsed._v,
        "expected:", OFFLINE_STORE_SCHEMA);
    }
  } catch (e) { }

  g_offlineStore = {
    _v: OFFLINE_STORE_SCHEMA,
    active: 0,
    profiles: [{ idx: 0, name: "Default" }],
    profileData: {
      0: JSON.parse(JSON.stringify(G_OFFLINE_DEFAULT_PROFILE_0))
    }
  };
}

function saveOfflineStore() {
  // [Phase 2.3 M-1, L-1] 온라인 모드에서는 오프라인 스토어를 조작하지 않음
  if (g_appMode !== APP_MODE_OFFLINE) return;

  try {
    if (g_offlineStore) {
      localStorage.setItem(OFFLINE_STORAGE_KEY, JSON.stringify(g_offlineStore));
    }
  } catch (e) { }
}

/* =======================================================
   오프라인 모킹 라우터
   ======================================================= */

function handleOfflineApi(url, method, body) {
  if (!g_offlineStore) loadOfflineStore();

  const u = new URL(url, "http://localhost");
  const path = u.pathname;
  // [H-7] 쿼리 파라미터 파싱 (온라인 서버와 정합)
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
      // [Phase 2.4 M-11] 상수 참조 반환 금지 → 반드시 deep clone
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
      // [Phase 2.5 H-3] 재귀적 병합
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
    // [H-7] compact/flat 반영: top-level 필드 선택적 제거
    const act = g_offlineStore.active;
    const actP = g_offlineStore.profiles.find(p => p.idx === act) || { idx: 0, name: "Default" };
    const topData = {
      uptime_ms: Date.now() - G_OFFLINE_START_TIME,
      heap_free: 245760,
      heap_min_free: 220000,
      heap_max_alloc: 245760,
      api_ver: 410,
      groups: {
        sys: { uptime_ms: Date.now() - G_OFFLINE_START_TIME, api_ver: 410 },
        mem: { heap_free: 245760, heap_min_free: 220000, heap_max_alloc: 245760 },
        net: { mode: "OFFLINE", ssid: "Local / Offline", ip: "127.0.0.1", rssi: 0, mdns: "elite-airmouse.local" },
        diag: { body_too_large: 0, no_body_slot: 0, json_bad: 0, safe_blocked: 0, ota_blocked: 0 },
        e10: { ble_connected: false, ppt_mode: false, active_mode: 1, dpi_level: 2, precision_mode: 0,
               gate: { ota_guard: false } },
        boot: { safe_mode: false, fail_count: 0, pending: false, last_reset_reason: 0 },
        config: { ver: 410, etag_ok: false, etag: 0, size: 0,
                  profile_idx: act, profile_count: g_offlineStore.profiles.length, profile_name: actP.name,
                  last_apply_ok: true, last_apply_ms: 0, last_apply_age_ms: 0,
                  last_apply_code: "", last_apply_src: "" },
        features: { etag_config: true, reboot_api: true, safe_mode_policy: true, ota_guard: true, e10_observability: true },
        ota: { in_progress: false, total: 0, written: 0, ok: false, err: "none" }
      },
      policy: {
        reboot_required: false, reboot_reason_mask: 0,
        reboot_reasons: "", safe_mode_api_limited: false,
        ota_upload_blocked: false
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
      delete topData.e10;
      delete topData.boot;
      delete topData.config;
      delete topData.features;
      // groups만 남김
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
    resObj.msg = "오프라인 모드: 시뮬레이션 명령 실행됨";
    resObj.data = { cmd: body && body.cmd, simulated: true };
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
      // 오프라인: 재부팅 시뮬레이션 없이 캐시 초기화
      resObj.data = { exit: true, offline: true, note: "Offline: no actual reboot." };
    } else {
      resObj.data = { safe_mode: false, fail_count: 0, pending: false, offline: true };
    }
  }
  else if (path === "/api/factory_reset") {
    if (method === "POST") {
      // [H-7] 언어/UI 상태는 유지, 오프라인 스토어만 리셋
      try { localStorage.removeItem(OFFLINE_STORAGE_KEY); } catch (e) { }
      g_offlineStore = null;
      loadOfflineStore();
      resObj.data = { reset: true, offline: true };
    }
  }
  else if (path === "/api/reboot/check") {
    resObj.data = { required: false, mask: 0, reasons: "", allowed: false, deny_code: "no_reboot_needed" };
  }
  else {
    resObj.msg = "Offline stub response";
  }

  const jsonStr = JSON.stringify(resObj);
  return { ok: true, status: 200, text: jsonStr, json: resObj };
}
