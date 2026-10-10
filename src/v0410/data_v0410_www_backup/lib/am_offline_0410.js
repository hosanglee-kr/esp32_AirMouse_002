/* =======================================================
   File: /www/lib/am_offline_0410.js
   Elite AirMouse WebConfig v0410 — Offline Simulator
   - 오프라인 기본 데이터 + localStorage 스토어 + 모킹 라우터
   - 로드 순서: 2
   - 의존: am_base_0410.js
   ======================================================= */

/* ---------------- 오프라인 시작 시각 ---------------- */
const G_OFFLINE_START_TIME = Date.now();

/* =======================================================
   오프라인 트리거 라이브러리 (27개)
   ======================================================= */
const G_OFFLINE_TRIGGERS = [
  { idx: 0,  name: "Top L Click",    group: "button",  locked: true },
  { idx: 1,  name: "Top L Double",   group: "button",  locked: false },
  { idx: 2,  name: "Top L Long",     group: "button",  locked: false },
  { idx: 3,  name: "Top M Click",    group: "button",  locked: false },
  { idx: 4,  name: "Top M Hold",     group: "button",  locked: true },
  { idx: 5,  name: "Top R Click",    group: "button",  locked: false },
  { idx: 6,  name: "Top R Double",   group: "button",  locked: false },
  { idx: 7,  name: "Top R Long",     group: "button",  locked: false },
  { idx: 8,  name: "Side F Click",   group: "button",  locked: false },
  { idx: 9,  name: "Side F Long",    group: "button",  locked: false },
  { idx: 10, name: "Side C Click",   group: "button",  locked: false },
  { idx: 11, name: "Side C Double",  group: "button",  locked: true },
  { idx: 12, name: "Side C 2s Hold", group: "button",  locked: true },
  { idx: 13, name: "Side R Click",   group: "button",  locked: false },
  { idx: 14, name: "Side R Long",    group: "button",  locked: false },
  { idx: 15, name: "Flick Left",     group: "gesture", locked: false },
  { idx: 16, name: "Flick Right",    group: "gesture", locked: false },
  { idx: 17, name: "Flick Up",       group: "gesture", locked: false },
  { idx: 18, name: "Flick Down",     group: "gesture", locked: false },
  { idx: 19, name: "Linear Left",    group: "gesture", locked: false },
  { idx: 20, name: "Linear Right",   group: "gesture", locked: false },
  { idx: 21, name: "Linear Up",      group: "gesture", locked: false },
  { idx: 22, name: "Linear Down",    group: "gesture", locked: false },
  { idx: 23, name: "Tilt Left",      group: "tilt",    locked: false },
  { idx: 24, name: "Tilt Right",     group: "tilt",    locked: false },
  { idx: 25, name: "Tilt Up",        group: "tilt",    locked: false },
  { idx: 26, name: "Tilt Down",      group: "tilt",    locked: false }
];

/* =======================================================
   오프라인 키코드 라이브러리
   - 백엔드 W10_WebApi_Status_0410.cpp::apiKeycodes() 와 1:1 정합
   ======================================================= */
const G_OFFLINE_KEYCODES = {
  action_kinds: [
    { value: 0,  name: "NONE",            hasP16: false, hasP32: false },
    { value: 1,  name: "MOUSE_CLICK",     hasP16: true,  hasP32: false, p16hint: "mask" },
    { value: 2,  name: "MOUSE_HOLD",      hasP16: true,  hasP32: false, p16hint: "mask" },
    { value: 3,  name: "MOUSE_WHEEL",     hasP16: true,  hasP32: false, p16hint: "axis|dir" },
    { value: 4,  name: "KB_TAP",          hasP16: true,  hasP32: true,  p16hint: "usage", p32hint: "mod" },
    { value: 5,  name: "KB_COMBO",        hasP16: false, hasP32: true,  p32hint: "mod|u1|u2|u3" },
    { value: 6,  name: "KB_REPEAT",       hasP16: true,  hasP32: true,  p16hint: "usage", p32hint: "mod" },
    { value: 7,  name: "CONSUMER_TAP",    hasP16: false, hasP32: true,  p32hint: "mask32" },
    { value: 8,  name: "CONSUMER_REPEAT", hasP16: false, hasP32: true,  p32hint: "mask32" },
    { value: 9,  name: "SPECIAL",         hasP16: true,  hasP32: false, p16hint: "special" },
    { value: 10, name: "MACRO",           hasP16: false, hasP32: true,  p32hint: "macroIdx" }
  ],
  mods: [
    { name: "None",    mask: 0 },
    { name: "L_CTRL",  mask: 1 },
    { name: "L_SHIFT", mask: 2 },
    { name: "L_ALT",   mask: 4 },
    { name: "L_GUI",   mask: 8 },
    { name: "R_CTRL",  mask: 16 },
    { name: "R_SHIFT", mask: 32 },
    { name: "R_ALT",   mask: 64 },
    { name: "R_GUI",   mask: 128 }
  ],
  consumer: [
    { name: "VOL_UP",     mask: 0x00000001 },
    { name: "VOL_DOWN",   mask: 0x00000002 },
    { name: "MUTE",       mask: 0x00000004 },
    { name: "PLAY_PAUSE", mask: 0x00000008 },
    { name: "STOP",       mask: 0x00000010 },
    { name: "NEXT_TRACK", mask: 0x00000020 },
    { name: "PREV_TRACK", mask: 0x00000040 },
    { name: "FF",         mask: 0x00000080 },
    { name: "REWIND",     mask: 0x00000100 },
    { name: "AC_BACK",    mask: 0x00000200 },
    { name: "AC_HOME",    mask: 0x00000400 },
    { name: "AC_SEARCH",  mask: 0x00000800 },
    { name: "POWER",      mask: 0x00001000 },
    { name: "TV_INPUT",   mask: 0x00002000 },
    { name: "CH_UP",      mask: 0x00004000 },
    { name: "CH_DOWN",    mask: 0x00008000 }
  ],
  precision_modes: [
    { name: "OFF",  value: 0 },
    { name: "LOW",  value: 1 },
    { name: "MED",  value: 2 },
    { name: "HIGH", value: 3 },
    { name: "PPT",  value: 4 }
  ],
  specials: [
    { value: 0, name: "NONE" },
    { value: 1, name: "GYRO_RECALIB" },
    { value: 2, name: "SLEEP_NOW" },
    { value: 3, name: "MODE_CYCLE" },
    { value: 4, name: "PAIRING" },
    { value: 5, name: "HOST_CYCLE" }
  ],
  kb: (function(){
    const arr = [];
    const names = {
      0:"None",4:"A",5:"B",6:"C",7:"D",8:"E",9:"F",10:"G",11:"H",12:"I",13:"J",14:"K",15:"L",
      16:"M",17:"N",18:"O",19:"P",20:"Q",21:"R",22:"S",23:"T",24:"U",25:"V",26:"W",27:"X",28:"Y",29:"Z",
      30:"1",31:"2",32:"3",33:"4",34:"5",35:"6",36:"7",37:"8",38:"9",39:"0",
      40:"Enter",41:"Esc",42:"Backspace",43:"Tab",44:"Space",
      58:"F1",59:"F2",60:"F3",61:"F4",62:"F5",63:"F6",64:"F7",65:"F8",66:"F9",67:"F10",68:"F11",69:"F12",
      75:"PageUp",78:"PageDown",79:"Right",80:"Left",81:"Down",82:"Up"
    };
    for (let c = 0; c <= 0xE7; c++) {
      arr.push({ code: c, name: names[c] || ("0x" + c.toString(16).toUpperCase().padStart(2, "0")) });
    }
    return arr;
  })(),
	/* ★ 신규: 온라인 /api/keycodes 응답과 1:1 정합 */
	triggers: G_OFFLINE_TRIGGERS,
	trigger_count: G_OFFLINE_TRIGGERS.length
};

/* =======================================================
   오프라인 기본 프로파일
   - 백엔드 C10_Config_0410.cpp::makeDefaultsSlots() 값과 1:1 정합
   ======================================================= */
const G_OFFLINE_DEFAULT_PROFILE_0 = {
    ver: 410,
    name: "Default",
    wifi: {
      mode: 0, // 0=Auto, 1=AP, 2=STA
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
    active_peer_index: 0
  },
  slots: {
    global: [
      { k: 2, h: 1, p16: 1,  p32: 0 },       // 0  TOP_L_CLICK     MouseHold L
      { k: 1, h: 0, p16: 1,  p32: 0 },       // 1  TOP_L_DOUBLE    MouseClick L
      { k: 4, h: 0, p16: 43, p32: 4 },       // 2  TOP_L_LONG      LALT + Tab
      { k: 1, h: 0, p16: 4,  p32: 0 },       // 3  TOP_M_CLICK     MouseClick M
      { k: 0, h: 0, p16: 0,  p32: 0 },       // 4  TOP_M_HOLD      None
      { k: 1, h: 0, p16: 2,  p32: 0 },       // 5  TOP_R_CLICK     MouseClick R
      { k: 4, h: 0, p16: 41, p32: 0 },       // 6  TOP_R_DOUBLE    Esc
      { k: 4, h: 0, p16: 22, p32: 10 },      // 7  TOP_R_LONG      LGUI|LSHIFT + S
      { k: 7, h: 0, p16: 0,  p32: 1 },       // 8  SIDE_F_CLICK    VOL_UP
      { k: 7, h: 0, p16: 0,  p32: 32 },      // 9  SIDE_F_LONG     NEXT_TRACK
      { k: 7, h: 0, p16: 0,  p32: 8 },       // 10 SIDE_C_CLICK    PLAY_PAUSE
      { k: 0, h: 0, p16: 0,  p32: 0 },       // 11 SIDE_C_DOUBLE   None
      { k: 0, h: 0, p16: 0,  p32: 0 },       // 12 SIDE_C_HOLD_2S  None
      { k: 7, h: 0, p16: 0,  p32: 2 },       // 13 SIDE_R_CLICK    VOL_DOWN
      { k: 7, h: 0, p16: 0,  p32: 64 },      // 14 SIDE_R_LONG     PREV_TRACK
      { k: 5, h: 0, p16: 0,  p32: 20489 },   // 15 FLICK_LEFT      LCTRL|LGUI + Left
      { k: 5, h: 0, p16: 0,  p32: 20233 },   // 16 FLICK_RIGHT     LCTRL|LGUI + Right
      { k: 4, h: 0, p16: 7,  p32: 8 },       // 17 FLICK_UP        LGUI + D
      { k: 4, h: 0, p16: 43, p32: 4 },       // 18 FLICK_DOWN      LALT + Tab
      { k: 0, h: 0, p16: 0,  p32: 0 },       // 19 LINEAR_LEFT     None
      { k: 0, h: 0, p16: 0,  p32: 0 },       // 20 LINEAR_RIGHT    None
      { k: 0, h: 0, p16: 0,  p32: 0 },       // 21 LINEAR_UP       None
      { k: 0, h: 0, p16: 0,  p32: 0 },       // 22 LINEAR_DOWN     None
      { k: 0, h: 0, p16: 0,  p32: 0 },       // 23 TILT_LEFT       None
      { k: 0, h: 0, p16: 0,  p32: 0 },       // 24 TILT_RIGHT      None
      { k: 0, h: 0, p16: 0,  p32: 0 },       // 25 TILT_UP         None
      { k: 0, h: 0, p16: 0,  p32: 0 }        // 26 TILT_DOWN       None
    ],
    modes: [
      /* ---------- Mode 1 (PC): 완전 상속 ---------- */
      { mask: 0, slots: [] },

      /* ---------- Mode 2 (PPT): 완전 오버라이드 ---------- */
      {
        mask: 134217727,
        slots: [
          { k: 4, h: 0, p16: 78, p32: 0 },   // 0  PAGEDOWN
          { k: 4, h: 0, p16: 75, p32: 0 },   // 1  PAGEUP
          { k: 4, h: 0, p16: 62, p32: 2 },   // 2  LSHIFT + F5
          { k: 4, h: 0, p16: 15, p32: 1 },   // 3  LCTRL + L
          { k: 0, h: 0, p16: 0,  p32: 0 },   // 4  None
          { k: 4, h: 0, p16: 41, p32: 0 },   // 5  Esc
          { k: 4, h: 0, p16: 5,  p32: 0 },   // 6  B
          { k: 4, h: 0, p16: 26, p32: 0 },   // 7  W
          { k: 4, h: 0, p16: 78, p32: 0 },   // 8  PAGEDOWN
          { k: 4, h: 0, p16: 62, p32: 2 },   // 9  LSHIFT + F5
          { k: 4, h: 0, p16: 19, p32: 1 },   // 10 LCTRL + P
          { k: 0, h: 0, p16: 0,  p32: 0 },   // 11 None
          { k: 0, h: 0, p16: 0,  p32: 0 },   // 12 None
          { k: 4, h: 0, p16: 75, p32: 0 },   // 13 PAGEUP
          { k: 4, h: 0, p16: 41, p32: 0 },   // 14 Esc
          { k: 4, h: 0, p16: 78, p32: 0 },   // 15 PAGEDOWN
          { k: 4, h: 0, p16: 75, p32: 0 },   // 16 PAGEUP
          { k: 4, h: 0, p16: 5,  p32: 0 },   // 17 B
          { k: 0, h: 0, p16: 0,  p32: 0 },   // 18 None
          { k: 0, h: 0, p16: 0,  p32: 0 },   // 19 None
          { k: 0, h: 0, p16: 0,  p32: 0 },   // 20 None
          { k: 0, h: 0, p16: 0,  p32: 0 },   // 21 None
          { k: 0, h: 0, p16: 0,  p32: 0 },   // 22 None
          { k: 0, h: 0, p16: 0,  p32: 0 },   // 23 None
          { k: 0, h: 0, p16: 0,  p32: 0 },   // 24 None
          { k: 0, h: 0, p16: 0,  p32: 0 },   // 25 None
          { k: 0, h: 0, p16: 0,  p32: 0 }    // 26 None
        ]
      },

      /* ---------- Mode 3 (TV): 완전 오버라이드 ---------- */
      {
        mask: 134217727,
        slots: [
          { k: 7, h: 0, p16: 0, p32: 512 },    // 0  AC_BACK
          { k: 7, h: 0, p16: 0, p32: 1024 },   // 1  AC_HOME
          { k: 7, h: 0, p16: 0, p32: 4096 },   // 2  POWER
          { k: 4, h: 0, p16: 40, p32: 0 },     // 3  Enter
          { k: 0, h: 0, p16: 0,  p32: 0 },     // 4  None
          { k: 7, h: 0, p16: 0, p32: 1024 },   // 5  AC_HOME
          { k: 7, h: 0, p16: 0, p32: 8192 },   // 6  TV_INPUT
          { k: 7, h: 0, p16: 0, p32: 4096 },   // 7  POWER
          { k: 7, h: 0, p16: 0, p32: 1 },      // 8  VOL_UP
          { k: 7, h: 0, p16: 0, p32: 16384 },  // 9  CH_UP
          { k: 7, h: 0, p16: 0, p32: 8 },      // 10 PLAY_PAUSE
          { k: 0, h: 0, p16: 0,  p32: 0 },     // 11 None
          { k: 0, h: 0, p16: 0,  p32: 0 },     // 12 None
          { k: 7, h: 0, p16: 0, p32: 2 },      // 13 VOL_DOWN
          { k: 7, h: 0, p16: 0, p32: 32768 },  // 14 CH_DOWN
          { k: 7, h: 0, p16: 0, p32: 256 },    // 15 REWIND
          { k: 7, h: 0, p16: 0, p32: 128 },    // 16 FF
          { k: 7, h: 0, p16: 0, p32: 4 },      // 17 MUTE
          { k: 0, h: 0, p16: 0,  p32: 0 },     // 18 None
          { k: 0, h: 0, p16: 0,  p32: 0 },     // 19 None
          { k: 0, h: 0, p16: 0,  p32: 0 },     // 20 None
          { k: 0, h: 0, p16: 0,  p32: 0 },     // 21 None
          { k: 0, h: 0, p16: 0,  p32: 0 },     // 22 None
          { k: 4, h: 0, p16: 80, p32: 0 },     // 23 Arrow Left
          { k: 4, h: 0, p16: 79, p32: 0 },     // 24 Arrow Right
          { k: 4, h: 0, p16: 82, p32: 0 },     // 25 Arrow Up
          { k: 4, h: 0, p16: 81, p32: 0 }      // 26 Arrow Down
        ]
      }
    ]
  },
  macros: []
};

/* =======================================================
   오프라인 로컬 저장소 (localStorage)
   ======================================================= */
const OFFLINE_STORAGE_KEY = "airmouse_v0410_offline_store";
let g_offlineStore = null;

function loadOfflineStore(){
  try {
    const raw = localStorage.getItem(OFFLINE_STORAGE_KEY);
    if (raw) {
      const parsed = JSON.parse(raw);
      if (parsed && typeof parsed === "object" && parsed.profiles) {
        g_offlineStore = parsed;
        return;
      }
    }
  } catch(e){}

  g_offlineStore = {
    active: 0,
    profiles: [
      { idx: 0, name: "Default" }
    ],
    profileData: {
      0: JSON.parse(JSON.stringify(G_OFFLINE_DEFAULT_PROFILE_0))
    }
  };
}

function saveOfflineStore(){
  try {
    if (g_offlineStore) {
      localStorage.setItem(OFFLINE_STORAGE_KEY, JSON.stringify(g_offlineStore));
    }
  } catch(e){}
}

/* =======================================================
   오프라인 모킹 라우터
   - 모든 응답은 온라인과 동일한 envelope { ok, code, msg, data }
   ======================================================= */
function handleOfflineApi(url, method, body){
  if (!g_offlineStore) loadOfflineStore();

  const u = new URL(url, "http://localhost");
  const path = u.pathname;

  let resObj = { ok: true, code: "ok", msg: "", data: null };

  /* ---------- /api/profiles ---------- */
  if (path === "/api/profiles") {
    if (method === "GET") {
      resObj.data = {
        active: g_offlineStore.active,
        count: g_offlineStore.profiles.length,
        profiles: g_offlineStore.profiles
      };
    }
  }

  /* ---------- /api/profiles/active ---------- */
  else if (path === "/api/profiles/active") {
    if (method === "GET") {
      const activeIdx = g_offlineStore.active;
      const cfg = g_offlineStore.profileData[activeIdx]
                || JSON.parse(JSON.stringify(G_OFFLINE_DEFAULT_PROFILE_0));
      resObj.data = {
        idx: activeIdx,
        count: g_offlineStore.profiles.length,
        config: cfg
      };
    } else if (method === "POST") {
      const activeIdx = g_offlineStore.active;
      if (!g_offlineStore.profileData[activeIdx]) {
        g_offlineStore.profileData[activeIdx] = JSON.parse(JSON.stringify(G_OFFLINE_DEFAULT_PROFILE_0));
      }
      if (body) {
        const target = g_offlineStore.profileData[activeIdx]
                     || JSON.parse(JSON.stringify(G_OFFLINE_DEFAULT_PROFILE_0));
        const merged = Object.assign({}, target);

        if (body.name   !== undefined) merged.name   = body.name;
        if (body.e10    !== undefined) merged.e10    = Object.assign({}, target.e10  || {}, body.e10);
        if (body.slots  !== undefined) merged.slots  = JSON.parse(JSON.stringify(body.slots));
        if (body.macros !== undefined) merged.macros = JSON.parse(JSON.stringify(body.macros));
        if (body.wifi   !== undefined) merged.wifi   = Object.assign({}, target.wifi || {}, body.wifi);

        g_offlineStore.profileData[activeIdx] = merged;
      }
      saveOfflineStore();
      resObj.data = { idx: activeIdx, saved: true, reloaded: true };
    }
  }

  /* ---------- /api/profiles/switch ---------- */
  else if (path === "/api/profiles/switch") {
    if (method === "POST" && body && typeof body.idx === "number") {
      g_offlineStore.active = body.idx;
      saveOfflineStore();
      resObj.data = { active: g_offlineStore.active };
    }
  }

  /* ---------- /api/profiles/create ---------- */
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

  /* ---------- /api/profiles/delete ---------- */
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

  /* ---------- /api/profiles/rename ---------- */
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

  /* ---------- /api/triggers ---------- */
  else if (path === "/api/triggers") {
    resObj.data = { count: G_OFFLINE_TRIGGERS.length, triggers: G_OFFLINE_TRIGGERS };
  }

  /* ---------- /api/keycodes ---------- */
  else if (path === "/api/keycodes") {
    resObj.data = G_OFFLINE_KEYCODES;
  }

  /* ---------- /api/status ---------- */
  else if (path === "/api/status") {
    const act = g_offlineStore.active;
    const actP = g_offlineStore.profiles.find(p => p.idx === act) || { idx: 0, name: "Default" };
    resObj.data = {
      groups: {
        sys: { uptime_ms: Date.now() - G_OFFLINE_START_TIME, loop_hz: 100 },
        mem: { heap_free: 245760, heap_min: 220000 },
        net: { mode: "OFFLINE", ssid: "Local / Offline", ip: "127.0.0.1", rssi: 0 },
        e10: { ble_connected: false, ppt_active: false, active_mode: 1, battery_pct: 100 },
        boot: { boot_count: 1, safe_mode: false, reboot_reason: "Offline Simulator" },
        config: {
          profile_idx: act,
          profile_name: actP.name,
          profile_count: g_offlineStore.profiles.length
        }
      }
    };
  }

  /* ---------- /api/diag ---------- */
  else if (path === "/api/diag") {
    resObj.data = {
      diag: { body_too_large: 0, no_body_slot: 0, json_bad: 0, safe_blocked: 0, ota_blocked: 0 },
      events: [
        { ms: Date.now() - G_OFFLINE_START_TIME, code: "OFFLINE_MODE_ACTIVE" }
      ]
    };
  }

  /* ---------- /api/control ---------- */
  else if (path === "/api/control") {
    resObj.msg  = "오프라인 모드: 시뮬레이션 명령 실행됨";
    resObj.data = { cmd: body && body.cmd, simulated: true };
  }

  /* ---------- /api/action/test ---------- */
  else if (path === "/api/action/test") {
    resObj.msg  = "오프라인 모드: 가상 액션 테스트 시뮬레이션";
    resObj.data = body;
  }

  /* ---------- /api/action/test_macro ---------- */
  else if (path === "/api/action/test_macro") {
    resObj.msg  = "오프라인 모드: 가상 매크로 테스트 시뮬레이션";
    resObj.data = body;
  }

  /* ---------- /api/config/import ---------- */
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

  /* ---------- /api/safeboot ---------- */
  else if (path === "/api/safeboot") {
    return {
      ok: true, status: 200,
      text: "오프라인 모드: SafeBoot 정보 시뮬레이션",
      json: { safe_mode: false, offline: true }
    };
  }

  /* ---------- fallback ---------- */
  else {
    resObj.msg = "Offline stub response";
  }

  const jsonStr = JSON.stringify(resObj);
  return { ok: true, status: 200, text: jsonStr, json: resObj };
}
