/* =======================================================
   File: /www/app_0410.003.js
   Elite AirMouse WebConfig — v0410 (Profile + Slot + Macro Editor)
   ======================================================= */

/* ---------------- 모드 전역 상수 ---------------- */
const APP_MODE_ONLINE  = "ONLINE";
const APP_MODE_OFFLINE = "OFFLINE";

/* ---------------- 유틸 ---------------- */
function qs(id){ return document.getElementById(id); }
function qsa(sel){ return document.querySelectorAll(sel); }

/* ---------------- 오프라인 기본 데이터 (data_v0410/json 기반) ---------------- */
const G_OFFLINE_START_TIME = Date.now();

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
  })()
};

/* =======================================================
   G_OFFLINE_DEFAULT_PROFILE_0
   - 백엔드 C10_Config_0410.cpp::makeDefaultsSlots() 값과 1:1 정합
   - 슬롯 인덱스 ↔ 트리거 매핑 (EN_C10_Trigger_t):
       0  TOP_L_CLICK       1  TOP_L_DOUBLE      2  TOP_L_LONG
       3  TOP_M_CLICK       4  TOP_M_HOLD        5  TOP_R_CLICK
       6  TOP_R_DOUBLE      7  TOP_R_LONG        8  SIDE_F_CLICK
       9  SIDE_F_LONG      10  SIDE_C_CLICK     11  SIDE_C_DOUBLE
      12  SIDE_C_HOLD_2S   13  SIDE_R_CLICK     14  SIDE_R_LONG
      15  FLICK_LEFT       16  FLICK_RIGHT      17  FLICK_UP
      18  FLICK_DOWN       19  LINEAR_LEFT      20  LINEAR_RIGHT
      21  LINEAR_UP        22  LINEAR_DOWN      23  TILT_LEFT
      24  TILT_RIGHT       25  TILT_UP          26  TILT_DOWN
   - kind: NONE=0, MOUSE_CLICK=1, MOUSE_HOLD=2, MOUSE_WHEEL=3,
           KB_TAP=4, KB_COMBO=5, KB_REPEAT=6,
           CONSUMER_TAP=7, CONSUMER_REPEAT=8, SPECIAL=9, MACRO=10
   - holdMode: NONE=0, PRESS=1, REPEAT=2
   ======================================================= */
const G_OFFLINE_DEFAULT_PROFILE_0 = {
  name: "Default",
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
    /* ---------------------------------------------------
       global[]  — Mode 1 (PC Air Mouse) 기준 기본값
       --------------------------------------------------- */
    global: [
      /*  0 TOP_L_CLICK      */ { k: 2, h: 1, p16: 1,  p32: 0 },       // MouseHold L
      /*  1 TOP_L_DOUBLE     */ { k: 1, h: 0, p16: 1,  p32: 0 },       // MouseClick L
      /*  2 TOP_L_LONG       */ { k: 4, h: 0, p16: 43, p32: 4 },       // LALT + Tab
      /*  3 TOP_M_CLICK      */ { k: 1, h: 0, p16: 4,  p32: 0 },       // MouseClick M
      /*  4 TOP_M_HOLD       */ { k: 0, h: 0, p16: 0,  p32: 0 },       // None (Move Gate)
      /*  5 TOP_R_CLICK      */ { k: 1, h: 0, p16: 2,  p32: 0 },       // MouseClick R
      /*  6 TOP_R_DOUBLE     */ { k: 4, h: 0, p16: 41, p32: 0 },       // Esc
      /*  7 TOP_R_LONG       */ { k: 4, h: 0, p16: 22, p32: 10 },      // LGUI|LSHIFT + S
      /*  8 SIDE_F_CLICK     */ { k: 7, h: 0, p16: 0,  p32: 1 },       // Consumer VOL_UP
      /*  9 SIDE_F_LONG      */ { k: 7, h: 0, p16: 0,  p32: 32 },      // Consumer NEXT_TRACK
      /* 10 SIDE_C_CLICK     */ { k: 7, h: 0, p16: 0,  p32: 8 },       // Consumer PLAY_PAUSE
      /* 11 SIDE_C_DOUBLE    */ { k: 0, h: 0, p16: 0,  p32: 0 },       // None (Mode Cycle)
      /* 12 SIDE_C_HOLD_2S   */ { k: 0, h: 0, p16: 0,  p32: 0 },       // None (Pairing)
      /* 13 SIDE_R_CLICK     */ { k: 7, h: 0, p16: 0,  p32: 2 },       // Consumer VOL_DOWN
      /* 14 SIDE_R_LONG      */ { k: 7, h: 0, p16: 0,  p32: 64 },      // Consumer PREV_TRACK
      /* 15 FLICK_LEFT       */ { k: 5, h: 0, p16: 0,  p32: 20489 },   // KB_COMBO LCTRL|LGUI + Left
      /* 16 FLICK_RIGHT      */ { k: 5, h: 0, p16: 0,  p32: 20233 },   // KB_COMBO LCTRL|LGUI + Right
      /* 17 FLICK_UP         */ { k: 4, h: 0, p16: 7,  p32: 8 },       // LGUI + D
      /* 18 FLICK_DOWN       */ { k: 4, h: 0, p16: 43, p32: 4 },       // LALT + Tab
      /* 19 LINEAR_LEFT      */ { k: 0, h: 0, p16: 0,  p32: 0 },       // None
      /* 20 LINEAR_RIGHT     */ { k: 0, h: 0, p16: 0,  p32: 0 },       // None
      /* 21 LINEAR_UP        */ { k: 0, h: 0, p16: 0,  p32: 0 },       // None
      /* 22 LINEAR_DOWN      */ { k: 0, h: 0, p16: 0,  p32: 0 },       // None
      /* 23 TILT_LEFT        */ { k: 0, h: 0, p16: 0,  p32: 0 },       // None
      /* 24 TILT_RIGHT       */ { k: 0, h: 0, p16: 0,  p32: 0 },       // None
      /* 25 TILT_UP          */ { k: 0, h: 0, p16: 0,  p32: 0 },       // None
      /* 26 TILT_DOWN        */ { k: 0, h: 0, p16: 0,  p32: 0 }        // None
    ],

    /* ---------------------------------------------------
       modes[] — 3개 Mode 의 Override 슬롯 + mask
       mask: 비트 i=1 → modes[m].slots[i] 사용, 0 → global[i] 상속
       --------------------------------------------------- */
    modes: [
      /* ---------- Mode 1 (PC): 완전 상속 (mask=0) ---------- */
      { mask: 0, slots: [] },

      /* ---------- Mode 2 (PPT): 완전 오버라이드 ---------- */
      {
        mask: 134217727,   // 0x07FFFFFF (27개 트리거 전부)
        slots: [
          /*  0 TOP_L_CLICK    */ { k: 4, h: 0, p16: 78, p32: 0 },   // PAGEDOWN
          /*  1 TOP_L_DOUBLE   */ { k: 4, h: 0, p16: 75, p32: 0 },   // PAGEUP
          /*  2 TOP_L_LONG     */ { k: 4, h: 0, p16: 62, p32: 2 },   // LSHIFT + F5
          /*  3 TOP_M_CLICK    */ { k: 4, h: 0, p16: 15, p32: 1 },   // LCTRL + L
          /*  4 TOP_M_HOLD     */ { k: 0, h: 0, p16: 0,  p32: 0 },   // None
          /*  5 TOP_R_CLICK    */ { k: 4, h: 0, p16: 41, p32: 0 },   // Esc
          /*  6 TOP_R_DOUBLE   */ { k: 4, h: 0, p16: 5,  p32: 0 },   // B
          /*  7 TOP_R_LONG     */ { k: 4, h: 0, p16: 26, p32: 0 },   // W
          /*  8 SIDE_F_CLICK   */ { k: 4, h: 0, p16: 78, p32: 0 },   // PAGEDOWN
          /*  9 SIDE_F_LONG    */ { k: 4, h: 0, p16: 62, p32: 2 },   // LSHIFT + F5
          /* 10 SIDE_C_CLICK   */ { k: 4, h: 0, p16: 19, p32: 1 },   // LCTRL + P
          /* 11 SIDE_C_DOUBLE  */ { k: 0, h: 0, p16: 0,  p32: 0 },   // None
          /* 12 SIDE_C_HOLD_2S */ { k: 0, h: 0, p16: 0,  p32: 0 },   // None
          /* 13 SIDE_R_CLICK   */ { k: 4, h: 0, p16: 75, p32: 0 },   // PAGEUP
          /* 14 SIDE_R_LONG    */ { k: 4, h: 0, p16: 41, p32: 0 },   // Esc
          /* 15 FLICK_LEFT     */ { k: 4, h: 0, p16: 78, p32: 0 },   // PAGEDOWN
          /* 16 FLICK_RIGHT    */ { k: 4, h: 0, p16: 75, p32: 0 },   // PAGEUP
          /* 17 FLICK_UP       */ { k: 4, h: 0, p16: 5,  p32: 0 },   // B
          /* 18 FLICK_DOWN     */ { k: 0, h: 0, p16: 0,  p32: 0 },   // None
          /* 19 LINEAR_LEFT    */ { k: 0, h: 0, p16: 0,  p32: 0 },   // None
          /* 20 LINEAR_RIGHT   */ { k: 0, h: 0, p16: 0,  p32: 0 },   // None
          /* 21 LINEAR_UP      */ { k: 0, h: 0, p16: 0,  p32: 0 },   // None
          /* 22 LINEAR_DOWN    */ { k: 0, h: 0, p16: 0,  p32: 0 },   // None
          /* 23 TILT_LEFT      */ { k: 0, h: 0, p16: 0,  p32: 0 },   // None
          /* 24 TILT_RIGHT     */ { k: 0, h: 0, p16: 0,  p32: 0 },   // None
          /* 25 TILT_UP        */ { k: 0, h: 0, p16: 0,  p32: 0 },   // None
          /* 26 TILT_DOWN      */ { k: 0, h: 0, p16: 0,  p32: 0 }    // None
        ]
      },

      /* ---------- Mode 3 (TV): 완전 오버라이드 ---------- */
      {
        mask: 134217727,   // 0x07FFFFFF
        slots: [
          /*  0 TOP_L_CLICK    */ { k: 7, h: 0, p16: 0, p32: 512 },    // Consumer AC_BACK
          /*  1 TOP_L_DOUBLE   */ { k: 7, h: 0, p16: 0, p32: 1024 },   // Consumer AC_HOME
          /*  2 TOP_L_LONG     */ { k: 7, h: 0, p16: 0, p32: 4096 },   // Consumer POWER
          /*  3 TOP_M_CLICK    */ { k: 4, h: 0, p16: 40, p32: 0 },     // Enter
          /*  4 TOP_M_HOLD     */ { k: 0, h: 0, p16: 0,  p32: 0 },     // None
          /*  5 TOP_R_CLICK    */ { k: 7, h: 0, p16: 0, p32: 1024 },   // Consumer AC_HOME
          /*  6 TOP_R_DOUBLE   */ { k: 7, h: 0, p16: 0, p32: 8192 },   // Consumer TV_INPUT
          /*  7 TOP_R_LONG     */ { k: 7, h: 0, p16: 0, p32: 4096 },   // Consumer POWER
          /*  8 SIDE_F_CLICK   */ { k: 7, h: 0, p16: 0, p32: 1 },      // Consumer VOL_UP
          /*  9 SIDE_F_LONG    */ { k: 7, h: 0, p16: 0, p32: 16384 },  // Consumer CH_UP
          /* 10 SIDE_C_CLICK   */ { k: 7, h: 0, p16: 0, p32: 8 },      // Consumer PLAY_PAUSE
          /* 11 SIDE_C_DOUBLE  */ { k: 0, h: 0, p16: 0,  p32: 0 },     // None
          /* 12 SIDE_C_HOLD_2S */ { k: 0, h: 0, p16: 0,  p32: 0 },     // None
          /* 13 SIDE_R_CLICK   */ { k: 7, h: 0, p16: 0, p32: 2 },      // Consumer VOL_DOWN
          /* 14 SIDE_R_LONG    */ { k: 7, h: 0, p16: 0, p32: 32768 },  // Consumer CH_DOWN
          /* 15 FLICK_LEFT     */ { k: 7, h: 0, p16: 0, p32: 256 },    // Consumer REWIND
          /* 16 FLICK_RIGHT    */ { k: 7, h: 0, p16: 0, p32: 128 },    // Consumer FF
          /* 17 FLICK_UP       */ { k: 7, h: 0, p16: 0, p32: 4 },      // Consumer MUTE
          /* 18 FLICK_DOWN     */ { k: 0, h: 0, p16: 0,  p32: 0 },     // None
          /* 19 LINEAR_LEFT    */ { k: 0, h: 0, p16: 0,  p32: 0 },     // None
          /* 20 LINEAR_RIGHT   */ { k: 0, h: 0, p16: 0,  p32: 0 },     // None
          /* 21 LINEAR_UP      */ { k: 0, h: 0, p16: 0,  p32: 0 },     // None
          /* 22 LINEAR_DOWN    */ { k: 0, h: 0, p16: 0,  p32: 0 },     // None
          /* 23 TILT_LEFT      */ { k: 4, h: 0, p16: 80, p32: 0 },     // Arrow Left
          /* 24 TILT_RIGHT     */ { k: 4, h: 0, p16: 79, p32: 0 },     // Arrow Right
          /* 25 TILT_UP        */ { k: 4, h: 0, p16: 82, p32: 0 },     // Arrow Up
          /* 26 TILT_DOWN      */ { k: 4, h: 0, p16: 81, p32: 0 }      // Arrow Down
        ]
      }
    ]
  },
  macros: []
};

/* ---------------- 오프라인 로컬 저장소 관리 ---------------- */
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

  // 기본값으로 초기화
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

/* ---------------- 오프라인 모킹 라우터 ---------------- */
function handleOfflineApi(url, method, body){
  if (!g_offlineStore) loadOfflineStore();

  const u = new URL(url, "http://localhost");
  const path = u.pathname;

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
      const cfg = g_offlineStore.profileData[activeIdx] || JSON.parse(JSON.stringify(G_OFFLINE_DEFAULT_PROFILE_0));
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
      // deep merge or overwrite
      if (body) {
        const target = g_offlineStore.profileData[activeIdx] || JSON.parse(JSON.stringify(G_OFFLINE_DEFAULT_PROFILE_0));
        const merged = Object.assign({}, target);
        
        if (body.name !== undefined) merged.name = body.name;
        if (body.e10 !== undefined) merged.e10 = Object.assign({}, target.e10 || {}, body.e10);
        if (body.slots !== undefined) merged.slots = JSON.parse(JSON.stringify(body.slots));
        if (body.macros !== undefined) merged.macros = JSON.parse(JSON.stringify(body.macros));
        if (body.wifi !== undefined) merged.wifi = Object.assign({}, target.wifi || {}, body.wifi);
        
        g_offlineStore.profileData[activeIdx] = merged;
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
        return { ok: false, status: 400, text: JSON.stringify({ ok: false, code: "limit_reached", msg: "최대 5개까지 생성 가능합니다." }), json: { ok: false, code: "limit_reached", msg: "최대 5개" } };
      }
      // 빈 index 찾기
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
        return { ok: false, status: 400, text: JSON.stringify({ ok: false, code: "cannot_delete", msg: "최소 1개의 프로파일이 필요합니다." }), json: { ok: false, code: "cannot_delete", msg: "최소 1개" } };
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
		// 이후 공통 경로에서 JSON.stringify(resObj) 반환
	}

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
  else if (path === "/api/diag") {
    resObj.data = {
      diag: { body_too_large: 0, no_body_slot: 0, json_bad: 0, safe_blocked: 0, ota_blocked: 0 },
      events: [
        { ms: Date.now() - G_OFFLINE_START_TIME, code: "OFFLINE_MODE_ACTIVE" }
      ]
    };
  }
  else if (path === "/api/control") {
    resObj.msg = "오프라인 모드: 시뮬레이션 명령 실행됨";
    resObj.data = { cmd: body?.cmd, simulated: true };
  }
  else if (path === "/api/action/test") {
    resObj.msg = "오프라인 모드: 가상 액션 테스트 시뮬레이션";
    resObj.data = body;
  }
  else if (path === "/api/config/import") {
    if (method === "POST" && body) {
      if (g_offlineStore.profiles.length >= 5) {
        return { ok: false, status: 400, text: JSON.stringify({ ok: false, code: "limit_reached", msg: "최대 5개까지 가능합니다." }), json: { ok: false, code: "limit_reached" } };
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
    return { ok: true, status: 200, text: "오프라인 모드: SafeBoot 정보 시뮬레이션", json: { safe_mode: false, offline: true } };
  }
  else {
    resObj.msg = "Offline stub response";
  }

  const jsonStr = JSON.stringify(resObj);
  return { ok: true, status: 200, text: jsonStr, json: resObj };
}

/* ---------------- 모드 상태 변수 & 통신 함수 ---------------- */
let g_appMode = (window.location.protocol === "file:") ? APP_MODE_OFFLINE : APP_MODE_ONLINE;

function setAppMode(mode){
  g_appMode = mode;
  updateAppModeUi();
}

function updateAppModeUi(){
  const pill = qs("pillNet");
  if (!pill) return;
  if (g_appMode === APP_MODE_OFFLINE) {
    setPill(pill, "NET: OFFLINE", false);
    pill.style.borderColor = "rgba(255, 170, 0, 0.6)";
    pill.style.background  = "rgba(255, 170, 0, 0.12)";
  } else {
    setPill(pill, "NET: ONLINE", true);
  }
}

async function apiGet(url){
  if (g_appMode === APP_MODE_OFFLINE) {
    return handleOfflineApi(url, "GET", null);
  }
  try {
    const r = await fetch(url, { cache:"no-store" });
    const t = await r.text();
    let j = null; try { j = JSON.parse(t); } catch(e){}
    return { ok:r.ok, status:r.status, text:t, json:j };
  } catch(e) {
    console.warn(`[Network] ${url} fetch failed, falling back to OFFLINE mode:`, e);
    setAppMode(APP_MODE_OFFLINE);
    setMsg("네트워크 연결 실패: Offline 모드로 전환되었습니다.", false);
    return handleOfflineApi(url, "GET", null);
  }
}

async function apiPostJson(url, obj){
  if (g_appMode === APP_MODE_OFFLINE) {
    return handleOfflineApi(url, "POST", obj);
  }
  try {
    const r = await fetch(url, {
      method:"POST",
      headers:{ "Content-Type":"application/json" },
      body: JSON.stringify(obj),
      cache:"no-store"
    });
    const t = await r.text();
    let j = null; try { j = JSON.parse(t); } catch(e){}
    return { ok:r.ok, status:r.status, text:t, json:j };
  } catch(e) {
    console.warn(`[Network] ${url} post failed, falling back to OFFLINE mode:`, e);
    setAppMode(APP_MODE_OFFLINE);
    setMsg("네트워크 연결 실패: Offline 모드로 전환되었습니다.", false);
    return handleOfflineApi(url, "POST", obj);
  }
}

function pretty(o){ try { return JSON.stringify(o, null, 2); } catch(e){ return String(o); } }

function unwrapApi(resp){
  const j = resp.json;
  if (j && typeof j === "object" && typeof j.ok === "boolean" && ("code" in j) && ("data" in j)) {
    return { ok:!!j.ok, code:String(j.code||""), msg:String(j.msg||""), data:j.data };
  }
  return { ok:resp.ok, code:resp.ok?"ok":`http_${resp.status}`, msg:"", data:j };
}

function parseIntFlex(v, def=0){
  if (v === null || v === undefined) return def;
  const s = String(v).trim();
  if (!s) return def;
  if (/^0x[0-9a-f]+$/i.test(s)) return parseInt(s, 16);
  const n = Number(s);
  return Number.isFinite(n) ? Math.trunc(n) : def;
}

function parseNum(v, def=0){ const n = Number(v); return Number.isFinite(n) ? n : def; }
function parseBool(v){ if (v===true||v===false) return v; const s=String(v).toLowerCase(); return s==="true"||s==="1"||s==="on"; }

function setMsg(text, good){
  const el = qs("cfgMsg"); if (!el) return;
  el.textContent = text;
  el.style.borderColor = good ? "rgba(76,125,255,.55)" : "rgba(255,77,77,.55)";
  el.style.background  = good ? "rgba(76,125,255,.10)" : "rgba(255,77,77,.10)";
}

function setPill(el, text, good){
  if (!el) return;
  el.textContent = text;
  el.style.borderColor = good ? "rgba(76,125,255,.55)" : "rgba(39,48,72,.9)";
  el.style.background  = good ? "rgba(76,125,255,.12)" : "rgba(255,255,255,.03)";
}

/* ---------------- 전역 상태 ---------------- */
let g_keycodes = null;
let g_triggers = [];
let g_profile  = null;      // { idx, count, config:{...} }
let g_view     = "global";  // "global" | "mode1" | "mode2" | "mode3"
let g_macroSel = -1;        // 현재 편집 중인 매크로 인덱스
let g_lastStatus = null;
let g_diagTypingUntilMs = 0;

function nowMs(){ return Date.now(); }

/* =======================================================
   프로파일 관리
   ======================================================= */
async function profileReloadAll(){
  const r = await apiGet("/api/profiles");
  const u = unwrapApi(r);
  if (!u.ok || !u.data){ setMsg("profiles load failed", false); return null; }

  const list = u.data.profiles || [];
  const active = u.data.active ?? 0;
  const count  = u.data.count  ?? list.length;

  // select
  const sel = qs("profSelect");
  if (sel){
    sel.innerHTML = "";
    for (const p of list){
      const o = document.createElement("option");
      o.value = String(p.idx);
      o.textContent = `${p.idx}: ${p.name}`;
      sel.appendChild(o);
    }
    sel.value = String(active);
  }

  // 버튼 상태 (create 한도 5개)
  const btnCreate = qs("btnProfCreate");
  if (btnCreate) btnCreate.disabled = (count >= 5);
  const btnDelete = qs("btnProfDelete");
  if (btnDelete) btnDelete.disabled = (count <= 1);

  // 상세 로드
  const r2 = await apiGet("/api/profiles/active");
  const u2 = unwrapApi(r2);
  if (u2.ok && u2.data){
    g_profile = {
      idx: u2.data.idx,
      count: u2.data.count,
      config: u2.data.config || {}
    };
  }

  return { active, count, list };
}

async function profileSwitch(idx){
  const u = unwrapApi(await apiPostJson("/api/profiles/switch", { idx }));
  if (!u.ok){ alert("switch failed: " + (u.msg || u.code)); return; }
  await profileReloadAll();
  renderSlotEditor();
  macroRenderList();
  macroRenderEditor();
  await refreshStatus();
}

async function profileCreate(){
  const name = prompt("새 프로파일 이름 (max 15자):", "New");
  if (!name) return;
  const u = unwrapApi(await apiPostJson("/api/profiles/create", { name: name.trim().substring(0, 15) }));
  if (!u.ok){ alert("create failed: " + (u.msg || u.code)); return; }
  await profileReloadAll();
  renderSlotEditor();
  macroRenderList();
  macroRenderEditor();
}

async function profileDelete(){
  if (!g_profile) return;
  if (!confirm(`프로파일 #${g_profile.idx} 삭제하시겠습니까?`)) return;
  const u = unwrapApi(await apiPostJson("/api/profiles/delete", { idx: g_profile.idx }));
  if (!u.ok){ alert("delete failed: " + (u.msg || u.code)); return; }
  await profileReloadAll();
  renderSlotEditor();
  macroRenderList();
  macroRenderEditor();
}

async function profileRename(){
  if (!g_profile) return;
  const cur = (g_profile.config && g_profile.config.name) || "";
  const name = prompt("새 이름 (max 15자):", cur);
  if (!name) return;
  const u = unwrapApi(await apiPostJson("/api/profiles/rename", { idx: g_profile.idx, name: name.trim().substring(0, 15) }));
  if (!u.ok){ alert("rename failed: " + (u.msg || u.code)); return; }
  await profileReloadAll();
}

/* =======================================================
   트리거 라이브러리 & 키코드
   ======================================================= */
async function loadTriggers() {
  // loadKeycodes 이후 호출되면 이미 로드됨
  if (g_keycodes && Array.isArray(g_keycodes.triggers) && g_keycodes.triggers.length) {
    g_triggers = g_keycodes.triggers;
    return;
  }
  const r = await apiGet("/api/triggers");
  const u = unwrapApi(r);
  if (u.ok && u.data) g_triggers = u.data.triggers || [];
}

async function loadKeycodes() {
  const r = await apiGet("/api/keycodes");
  const u = unwrapApi(r);
  if (u.ok && u.data) {
    g_keycodes = u.data;
  } else if (r.json && r.json.action_kinds) {
    g_keycodes = r.json; // 오프라인 fallback
  }
}

/* =======================================================
   슬롯 편집기
   ======================================================= */

// view 문자열 → mode 숫자 (0~2), global은 -1
function viewToModeIdx(v){
  if (v === "mode1") return 0;
  if (v === "mode2") return 1;
  if (v === "mode3") return 2;
  return -1;
}

// 슬롯 배열 참조 얻기 (global or mode별)
function getSlotsArray(){
  if (!g_profile || !g_profile.config) return null;
  const s = g_profile.config.slots;
  if (!s) return null;

  const mi = viewToModeIdx(g_view);
  if (mi < 0) return s.global || [];

  const m = (s.modes || [])[mi];
  return (m && m.slots) ? m.slots : [];
}

function getOverrideMask(){
  if (!g_profile || !g_profile.config) return 0;
  const mi = viewToModeIdx(g_view);
  if (mi < 0) return 0;
  const s = g_profile.config.slots;
  const m = (s.modes || [])[mi];
  return m ? (m.mask >>> 0) : 0;
}

function setOverrideBit(trigIdx, on){
  if (!g_profile || !g_profile.config) return;
  const mi = viewToModeIdx(g_view);
  if (mi < 0) return;
  const s = g_profile.config.slots;
  if (!s.modes) s.modes = [];
  if (!s.modes[mi]) s.modes[mi] = { mask:0, slots:[] };
  if (!s.modes[mi].slots) s.modes[mi].slots = [];
  // 슬롯 배열 27개 dense 보장
  while (s.modes[mi].slots.length < 27) s.modes[mi].slots.push({ k:0, h:0, p16:0, p32:0 });

  if (on) s.modes[mi].mask = (s.modes[mi].mask | (1 << trigIdx)) >>> 0;
  else    s.modes[mi].mask = (s.modes[mi].mask & ~(1 << trigIdx)) >>> 0;

  // override 켤 때: global 값 복사
  if (on){
    const gArr = s.global || [];
    if (gArr[trigIdx]){
      s.modes[mi].slots[trigIdx] = JSON.parse(JSON.stringify(gArr[trigIdx]));
    }
  }
}

/* =======================================================
   슬롯 편집기 — renderSlotEditor (v0410.003 fix)
   ======================================================= */
function renderSlotEditor(){
  const root = qs("slotEditor");
  if (!root) return;

  if (!g_profile || !g_profile.config){
    root.innerHTML = '<div class="hint2">프로파일 로드 대기 중…</div>';
    return;
  }

  const triggers = g_triggers || [];
  const slotsArr = getSlotsArray() || [];
  const mask     = getOverrideMask();
  const isGlobal = (viewToModeIdx(g_view) < 0);

  root.innerHTML = "";

  // 그룹 정의
  const groups = [
    { key: "button",  label: "Button Triggers" },
    { key: "gesture", label: "Gesture (Flick / Linear)" },
    { key: "tilt",    label: "Tilt Hold (Mode 3 에서만 발동)" }
  ];

  for (const g of groups){
    // ── Tilt 그룹 경고 배너 (Mode 3가 아닐 때만) ──
    //   v0410.002의 버그: 이 블록이 for 루프 밖에 있어 g가 TDZ 상태였음.
    //   수정: 각 그룹 head 생성 직전, for 루프 내부로 이동.
    if (g.key === "tilt" && viewToModeIdx(g_view) !== 2){
      const warn = document.createElement("div");
      warn.className = "hint2";
      warn.style.cssText = "margin:6px 0;color:var(--warn-dark);";
      warn.textContent = "⚠ 이 그룹은 Mode 3 (TV) 에서만 실제 발동합니다.";
      root.appendChild(warn);
    }

    // ── 그룹 헤더 ──
    const head = document.createElement("div");
    head.className = "trig-group-head";
    head.textContent = g.label;
    root.appendChild(head);

    // ── 트리거 행 ──
    for (const t of triggers){
      if (t.group !== g.key) continue;

      const idx = t.idx;
      const row = document.createElement("div");
      row.className = "trig-row";
      row.dataset.trig = String(idx);

      // 잠금 표시
      if (t.locked) row.classList.add("locked");

      // override 여부
      const isOver = isGlobal ? false : ((mask & (1 << idx)) !== 0);
      if (!isGlobal && isOver) row.classList.add("override");

      // (1) 트리거 이름
      const nameEl = document.createElement("div");
      nameEl.className = "trig-name";
      nameEl.innerHTML = `${t.locked ? "🔒 " : ""}${t.name}`;
      row.appendChild(nameEl);

      // (2) Global/Mode 뱃지
      const badge = document.createElement("div");
      badge.className = "trig-badge";
      if (isGlobal){
        badge.textContent = "G";
        badge.classList.add("g");
      } else if (isOver){
        badge.textContent = `M${viewToModeIdx(g_view)+1}`;
        badge.classList.add("m");
        badge.title = "클릭하여 Global로 되돌림";
      } else {
        badge.textContent = "G";
        badge.classList.add("g");
        badge.title = "클릭하여 이 Mode에 override 생성";
      }
      if (!t.locked && !isGlobal){
        badge.style.cursor = "pointer";
        badge.onclick = () => {
          if (isOver){
            setOverrideBit(idx, false);
          } else {
            setOverrideBit(idx, true);
          }
          renderSlotEditor();
        };
      }
      row.appendChild(badge);

      // (3) 액션 편집기
      const slot = isGlobal
        ? (slotsArr[idx] || {k:0,h:0,p16:0,p32:0})
        : (isOver
            ? (slotsArr[idx] || {k:0,h:0,p16:0,p32:0})
            : ((g_profile.config.slots.global && g_profile.config.slots.global[idx])
                 || {k:0,h:0,p16:0,p32:0}));

      const isEditable = !t.locked && (isGlobal || isOver);
      const editor = renderActionEditor(slot, isEditable, (newSlot) => {
        const arr = getSlotsArray();
        if (!arr) return;
        const mi = viewToModeIdx(g_view);
        if (mi < 0){
          g_profile.config.slots.global[idx] = newSlot;
        } else {
          const m = g_profile.config.slots.modes[mi];
          m.slots[idx] = newSlot;
        }
      });
      row.appendChild(editor);

      // (4) Live Test
      const testCell = document.createElement("div");
      testCell.className = "trig-test";
      if (!t.locked){
        const btn = document.createElement("button");
        btn.className = "btn mini";
        btn.textContent = "Test";
        
        
        btn.onclick = async () => {
					// 최신 슬롯 재조회
					const arr = getSlotsArray();
					const s = (arr && arr[idx]) ? arr[idx] : slot;
					if (!s || s.k === 0) { alert("할당된 액션이 없습니다."); return; }
          if (!confirm(`Live Test 실행? (kind: ${s.k})`)) return;
          const u = unwrapApi(await apiPostJson("/api/action/test", {
            k: s.k, h: s.h, p16: s.p16, p32: s.p32
          }));
          if (!u.ok) alert("Test failed: " + (u.msg || u.code));
        };
        testCell.appendChild(btn);
      }
      row.appendChild(testCell);

      root.appendChild(row);
    }
  }
}

/* =======================================================
   Action 편집기 (kind + 파라미터)
   ======================================================= */
function renderActionEditor(slot, editable, onChange){
  const wrap = document.createElement("div");
  wrap.className = "action-editor";

  const kinds = (g_keycodes && g_keycodes.action_kinds) || [];
  const specials = (g_keycodes && g_keycodes.specials) || [];
  const consumer = (g_keycodes && g_keycodes.consumer) || [];
  const mods = (g_keycodes && g_keycodes.mods) || [];

  const cur = { k: slot.k ?? 0, h: slot.h ?? 0, p16: slot.p16 ?? 0, p32: slot.p32 ?? 0 };

  function emit(){ onChange({ k:cur.k, h:cur.h, p16:cur.p16, p32:cur.p32 }); }

  function renderParams(){
    wrap.innerHTML = "";

    // kind select
    const selKind = document.createElement("select");
    selKind.className = "select mini";
    selKind.disabled = !editable;
    selKind.innerHTML = "";
    for (const kk of kinds){
      const o = document.createElement("option");
      o.value = String(kk.value);
      o.textContent = kk.name;
      selKind.appendChild(o);
    }
    selKind.value = String(cur.k);
    selKind.onchange = () => {
      cur.k = parseIntFlex(selKind.value, 0);
      cur.h = 0; cur.p16 = 0; cur.p32 = 0;
      renderParams();
      emit();
    };
    wrap.appendChild(selKind);

    const k = cur.k;

    if (k === 0){
      // NONE
    } else if (k === 1 || k === 2){
      // MOUSE_CLICK / HOLD (mask)
      const sel = document.createElement("select");
      sel.className = "select mini";
      sel.disabled = !editable;
      sel.innerHTML = `
        <option value="0">None</option>
        <option value="1">Left</option>
        <option value="2">Right</option>
        <option value="4">Middle</option>
        <option value="8">Back</option>
        <option value="16">Forward</option>`;
      sel.value = String(cur.p16);
      sel.onchange = () => { cur.p16 = parseIntFlex(sel.value, 0); emit(); };
      wrap.appendChild(sel);

    } else if (k === 3){
      // MOUSE_WHEEL
      const axisSel = document.createElement("select");
      axisSel.className = "select mini"; axisSel.disabled = !editable;
      axisSel.innerHTML = `
        <option value="0">Y (Vertical)</option>
        <option value="1">X (Pan)</option>`;
      const dirSel = document.createElement("select");
      dirSel.className = "select mini"; dirSel.disabled = !editable;
      dirSel.innerHTML = `
        <option value="0">Up / Left</option>
        <option value="1">Down / Right</option>`;

      const axis = (cur.p16 >>> 8) & 0xFF;
      const dir  = cur.p16 & 0xFF;
      axisSel.value = String(axis);
      dirSel.value  = String(dir);

      const upd = () => { cur.p16 = ((parseIntFlex(axisSel.value,0)<<8) | (parseIntFlex(dirSel.value,0)&0xFF)); emit(); };
      axisSel.onchange = upd; dirSel.onchange = upd;

      wrap.appendChild(axisSel);
      wrap.appendChild(dirSel);

    } else if (k === 4 || k === 6){
      // KB_TAP / KB_REPEAT (mod + usage)
      const selMod = document.createElement("select");
      selMod.className = "select mini"; selMod.disabled = !editable;
      selMod.innerHTML = `<option value="0">None</option>`;
      for (const m of mods){
        const o = document.createElement("option");
        o.value = String(m.mask);
        o.textContent = m.name;
        selMod.appendChild(o);
      }
      selMod.value = String(cur.p32 & 0xFF);
      selMod.onchange = () => { cur.p32 = parseIntFlex(selMod.value,0); emit(); };
      wrap.appendChild(selMod);

      const inpU = document.createElement("input");
      inpU.className = "inp mini"; inpU.type = "number";
      inpU.disabled = !editable;
      inpU.placeholder = "usage";
      inpU.value = String(cur.p16);
      inpU.oninput = () => { cur.p16 = parseIntFlex(inpU.value,0) & 0xFF; emit(); };
      wrap.appendChild(inpU);

    } else if (k === 5){
      // KB_COMBO (mod | u1<<8 | u2<<16 | u3<<24)
      const selMod = document.createElement("select");
      selMod.className = "select mini"; selMod.disabled = !editable;
      selMod.innerHTML = `<option value="0">None</option>`;
      for (const m of mods){
        const o = document.createElement("option");
        o.value = String(m.mask);
        o.textContent = m.name;
        selMod.appendChild(o);
      }
      selMod.value = String(cur.p32 & 0xFF);
      selMod.onchange = () => {
        cur.p32 = ((cur.p32 & 0xFFFFFF00) | parseIntFlex(selMod.value,0)) >>> 0;
        emit();
      };
      wrap.appendChild(selMod);

      for (let i = 0; i < 3; i++){
        const inp = document.createElement("input");
        inp.className = "inp mini"; inp.type = "number";
        inp.disabled = !editable;
        inp.placeholder = "u" + (i+1);
        const shift = 8 + i*8;
        inp.value = String((cur.p32 >>> shift) & 0xFF);
        inp.oninput = () => {
          const v = parseIntFlex(inp.value,0) & 0xFF;
          cur.p32 = ((cur.p32 & ~(0xFF << shift)) | (v << shift)) >>> 0;
          emit();
        };
        wrap.appendChild(inp);
      }

    } else if (k === 7 || k === 8){
      // CONSUMER_TAP / REPEAT
      const sel = document.createElement("select");
      sel.className = "select mini"; sel.disabled = !editable;
      sel.innerHTML = `<option value="0">None</option>`;
      for (const c of consumer){
        const o = document.createElement("option");
        o.value = String(c.mask >>> 0);
        o.textContent = c.name;
        sel.appendChild(o);
      }
      sel.value = String(cur.p32 >>> 0);
      sel.onchange = () => { cur.p32 = parseIntFlex(sel.value,0) >>> 0; emit(); };
      wrap.appendChild(sel);

    } else if (k === 9){
      // SPECIAL
      const sel = document.createElement("select");
      sel.className = "select mini"; sel.disabled = !editable;
      sel.innerHTML = "";
      for (const s of specials){
        const o = document.createElement("option");
        o.value = String(s.value);
        o.textContent = s.name;
        sel.appendChild(o);
      }
      sel.value = String(cur.p16);
      sel.onchange = () => { cur.p16 = parseIntFlex(sel.value,0); emit(); };
      wrap.appendChild(sel);

    } else if (k === 10){
      // MACRO: g_profile.config.macros에서 드롭다운
      const macros = (g_profile && g_profile.config && g_profile.config.macros) || [];

      const sel = document.createElement("select");
      sel.className = "select mini";
      sel.disabled = !editable;
      sel.innerHTML = "";

      if (macros.length === 0){
        const o = document.createElement("option");
        o.value = "0";
        o.textContent = "(매크로 없음)";
        sel.appendChild(o);
        sel.disabled = true;
      } else {
        for (let i = 0; i < macros.length; i++){
          const o = document.createElement("option");
          o.value = String(i);
          o.textContent = `${i}: ${macros[i].name || ("Macro " + i)}`;
          sel.appendChild(o);
        }
        const curIdx = (cur.p32 >= 0 && cur.p32 < macros.length) ? cur.p32 : 0;
        if (curIdx !== cur.p32){
          cur.p32 = curIdx;
          emit();
        }
        sel.value = String(curIdx);
      }

      sel.onchange = () => {
        cur.p32 = parseIntFlex(sel.value, 0);
        emit();
      };
      wrap.appendChild(sel);

      if (macros.length > 0){
        const idx = (cur.p32 >= 0 && cur.p32 < macros.length) ? cur.p32 : 0;
        const hintEl = document.createElement("span");
        hintEl.className = "hint2";
        hintEl.style.fontSize = ".74em";
        hintEl.textContent = `${(macros[idx].steps || []).length} step`;
        wrap.appendChild(hintEl);
      }
    }
  }

  renderParams();
  return wrap;
}

/* =======================================================
   저장
   ======================================================= */
async function saveProfile(){
  if (!g_profile) return;
  const u = unwrapApi(await apiPostJson("/api/profiles/active", g_profile.config));
  if (!u.ok){ alert("save failed: " + (u.msg || u.code)); return; }
  await profileReloadAll();
  renderSlotEditor();
  macroRenderList();
  macroRenderEditor();
  await refreshStatus();
  setMsg("프로파일 저장 완료", true);
}

/* =======================================================
   Macros
   ======================================================= */
function getMacrosArray(){
  if (!g_profile || !g_profile.config) return [];
  if (!g_profile.config.macros) g_profile.config.macros = [];
  return g_profile.config.macros;
}

function macroRenderList(){
  const root = qs("macroList");
  if (!root) return;
  root.innerHTML = "";

  const macros = getMacrosArray();
  const cntEl = qs("macroCountLabel");
  if (cntEl) cntEl.textContent = `${macros.length} / 8`;

  const btnAdd = qs("btnMacroAdd");
  if (btnAdd) btnAdd.disabled = (macros.length >= 8);

  if (macros.length === 0){
    const div = document.createElement("div");
    div.className = "hint2";
    div.textContent = "등록된 매크로가 없습니다.";
    root.appendChild(div);
    return;
  }

  macros.forEach((m, i) => {
    const el = document.createElement("div");
    el.className = "macro-item";
    if (i === g_macroSel) el.classList.add("on");

    const idxEl = document.createElement("span");
    idxEl.className = "macro-item-idx";
    idxEl.textContent = String(i);
    el.appendChild(idxEl);

    const nameEl = document.createElement("span");
    nameEl.className = "macro-item-name";
    nameEl.textContent = m.name || `Macro ${i}`;
    el.appendChild(nameEl);

    const stepsEl = document.createElement("span");
    stepsEl.className = "macro-item-steps";
    stepsEl.textContent = `${(m.steps || []).length} step`;
    el.appendChild(stepsEl);

    el.onclick = () => {
      g_macroSel = i;
      macroRenderList();
      macroRenderEditor();
    };

    root.appendChild(el);
  });
}

function macroRenderEditor(){
  const root = qs("macroEditor");
  if (!root) return;
  root.innerHTML = "";

  const macros = getMacrosArray();

  if (g_macroSel < 0 || g_macroSel >= macros.length){
    root.innerHTML = '<div class="hint2">좌측 목록에서 매크로를 선택하세요.</div>';
    return;
  }

  const m = macros[g_macroSel];
  if (!m.steps) m.steps = [];

  // ---- head: 이름 + 삭제/테스트 ----
  const head = document.createElement("div");
  head.className = "macro-editor-head";

  const nameInp = document.createElement("input");
  nameInp.type = "text";
  nameInp.maxLength = 15;
  nameInp.placeholder = "매크로 이름";
  nameInp.value = m.name || "";
  nameInp.oninput = () => { m.name = nameInp.value; };
  head.appendChild(nameInp);

  const btnTest = document.createElement("button");
  btnTest.className = "btn primary mini";
  btnTest.textContent = "Test";
  btnTest.onclick = async () => {
    if (!m.steps || m.steps.length === 0){ alert("빈 매크로입니다."); return; }
    if (!confirm(`매크로 "${m.name}" 실행?`)) return;

    // 1) 서버에 현재 상태 저장
    const saved = await macroSave(true);
    if (!saved){ alert("저장 실패"); return; }

    // 2) Live Test API 호출
    const u = unwrapApi(await apiPostJson("/api/action/test_macro", { idx: g_macroSel }));
    if (!u.ok) alert("매크로 실행 실패: " + (u.msg || u.code));
  };
  head.appendChild(btnTest);

  const btnDel = document.createElement("button");
  btnDel.className = "btn danger mini";
  btnDel.textContent = "삭제";
  btnDel.onclick = async () => {
    if (!confirm(`매크로 "${m.name}" 삭제하시겠습니까?`)) return;
    macros.splice(g_macroSel, 1);
    g_macroSel = macros.length > 0 ? Math.min(g_macroSel, macros.length - 1) : -1;
    await macroSave(true);
    macroRenderList();
    macroRenderEditor();
    renderSlotEditor();
  };
  head.appendChild(btnDel);

  root.appendChild(head);

  // ---- steps ----
  const stepsWrap = document.createElement("div");
  stepsWrap.className = "macro-steps";

  m.steps.forEach((s, i) => {
    const row = document.createElement("div");
    row.className = "macro-step";

    // index
    const idxEl = document.createElement("div");
    idxEl.className = "macro-step-idx";
    idxEl.textContent = String(i + 1);
    row.appendChild(idxEl);

    // params
    const params = document.createElement("div");
    params.className = "macro-step-params";
    buildMacroStepParams(params, s);
    row.appendChild(params);

    // delay
    const delayWrap = document.createElement("div");
    const delayInp = document.createElement("input");
    delayInp.className = "macro-step-delay";
    delayInp.type = "number";
    delayInp.min = "0";
    delayInp.max = "2000";
    delayInp.step = "10";
    delayInp.value = String(s.d ?? 0);
    delayInp.oninput = () => {
      let v = parseIntFlex(delayInp.value, 0);
      if (v < 0) v = 0;
      if (v > 2000) v = 2000;
      s.d = v;
    };
    delayWrap.appendChild(delayInp);
    const dl = document.createElement("span");
    dl.className = "macro-step-delay-label";
    dl.textContent = "delay ms (0~2000)";
    delayWrap.appendChild(dl);
    row.appendChild(delayWrap);

    // actions (up / down / delete)
    const acts = document.createElement("div");
    acts.className = "macro-step-actions";

    const up = document.createElement("button");
    up.className = "btn mini"; up.textContent = "▲";
    up.disabled = (i === 0);
    up.onclick = () => {
      [m.steps[i-1], m.steps[i]] = [m.steps[i], m.steps[i-1]];
      macroRenderEditor();
    };
    acts.appendChild(up);

    const down = document.createElement("button");
    down.className = "btn mini"; down.textContent = "▼";
    down.disabled = (i === m.steps.length - 1);
    down.onclick = () => {
      [m.steps[i+1], m.steps[i]] = [m.steps[i], m.steps[i+1]];
      macroRenderEditor();
    };
    acts.appendChild(down);

    const del = document.createElement("button");
    del.className = "btn danger mini"; del.textContent = "✕";
    del.onclick = () => {
      m.steps.splice(i, 1);
      macroRenderEditor();
    };
    acts.appendChild(del);

    row.appendChild(acts);
    stepsWrap.appendChild(row);
  });

  root.appendChild(stepsWrap);

  // ---- add step ----
  const btnAddStep = document.createElement("button");
  btnAddStep.className = "btn";
  btnAddStep.style.marginTop = "10px";
  btnAddStep.textContent = "+ Step 추가";
  btnAddStep.disabled = (m.steps.length >= 8);
  btnAddStep.onclick = () => {
    if (m.steps.length >= 8) return;
    m.steps.push({ k: 4, h: 0, d: 0, p16: 4, p32: 0 });  // default: KB_TAP A
    macroRenderEditor();
  };
  root.appendChild(btnAddStep);

  const stepInfo = document.createElement("div");
  stepInfo.className = "hint2";
  stepInfo.style.marginTop = "6px";
  stepInfo.textContent = `${m.steps.length} / 8 step`;
  root.appendChild(stepInfo);
}

function buildMacroStepParams(container, s){
  const kinds = (g_keycodes && g_keycodes.action_kinds) || [];
  const consumer = (g_keycodes && g_keycodes.consumer) || [];
  const mods = (g_keycodes && g_keycodes.mods) || [];

  // MACRO 중첩 금지 + SPECIAL 제외
  const allowedKinds = kinds.filter(k => k.value !== 0 && k.value !== 9 && k.value !== 10);

  const selKind = document.createElement("select");
  selKind.className = "select mini";
  selKind.innerHTML = `<option value="0">NONE</option>`;
  for (const k of allowedKinds){
    const o = document.createElement("option");
    o.value = String(k.value);
    o.textContent = k.name;
    selKind.appendChild(o);
  }
  selKind.value = String(s.k ?? 0);
  container.appendChild(selKind);

  function renderParamsFor(){
    while (container.children.length > 1){
      container.removeChild(container.lastChild);
    }

    const k = s.k;

    if (k === 1 || k === 2){ // MOUSE_CLICK / HOLD
      const sel = document.createElement("select");
      sel.className = "select mini";
      sel.innerHTML = `
        <option value="0">None</option>
        <option value="1">Left</option>
        <option value="2">Right</option>
        <option value="4">Middle</option>
        <option value="8">Back</option>
        <option value="16">Forward</option>`;
      sel.value = String(s.p16 ?? 0);
      sel.onchange = () => { s.p16 = parseIntFlex(sel.value, 0); };
      container.appendChild(sel);

    } else if (k === 3){ // MOUSE_WHEEL
      const axisSel = document.createElement("select");
      axisSel.className = "select mini";
      axisSel.innerHTML = `<option value="0">Y</option><option value="1">X</option>`;
      const dirSel = document.createElement("select");
      dirSel.className = "select mini";
      dirSel.innerHTML = `<option value="0">Up/Left</option><option value="1">Down/Right</option>`;

      axisSel.value = String((s.p16 >>> 8) & 0xFF);
      dirSel.value  = String(s.p16 & 0xFF);
      const upd = () => { s.p16 = (((axisSel.value|0)<<8) | (dirSel.value|0)); };
      axisSel.onchange = upd; dirSel.onchange = upd;
      container.appendChild(axisSel);
      container.appendChild(dirSel);

    } else if (k === 4 || k === 6){ // KB_TAP / KB_REPEAT
      const selMod = document.createElement("select");
      selMod.className = "select mini";
      selMod.innerHTML = `<option value="0">None</option>`;
      for (const mm of mods){
        const o = document.createElement("option");
        o.value = String(mm.mask);
        o.textContent = mm.name;
        selMod.appendChild(o);
      }
      selMod.value = String((s.p32 ?? 0) & 0xFF);
      selMod.onchange = () => { s.p32 = (s.p32 & ~0xFF) | (parseIntFlex(selMod.value,0) & 0xFF); };
      container.appendChild(selMod);

      const inp = document.createElement("input");
      inp.className = "inp mini";
      inp.type = "number";
      inp.placeholder = "usage";
      inp.value = String(s.p16 ?? 0);
      inp.oninput = () => { s.p16 = parseIntFlex(inp.value, 0) & 0xFF; };
      container.appendChild(inp);

    } else if (k === 5){ // KB_COMBO
      const selMod = document.createElement("select");
      selMod.className = "select mini";
      selMod.innerHTML = `<option value="0">None</option>`;
      for (const mm of mods){
        const o = document.createElement("option");
        o.value = String(mm.mask);
        o.textContent = mm.name;
        selMod.appendChild(o);
      }
      selMod.value = String((s.p32 ?? 0) & 0xFF);
      selMod.onchange = () => {
        s.p32 = ((s.p32 & 0xFFFFFF00) | (parseIntFlex(selMod.value,0) & 0xFF)) >>> 0;
      };
      container.appendChild(selMod);

      for (let i = 0; i < 3; i++){
        const inp = document.createElement("input");
        inp.className = "inp mini"; inp.type = "number";
        inp.placeholder = "u" + (i+1);
        const shift = 8 + i*8;
        inp.value = String((s.p32 >>> shift) & 0xFF);
        inp.oninput = () => {
          const v = parseIntFlex(inp.value, 0) & 0xFF;
          s.p32 = ((s.p32 & ~(0xFF << shift)) | (v << shift)) >>> 0;
        };
        container.appendChild(inp);
      }

    } else if (k === 7 || k === 8){ // CONSUMER
      const sel = document.createElement("select");
      sel.className = "select mini";
      sel.innerHTML = `<option value="0">None</option>`;
      for (const c of consumer){
        const o = document.createElement("option");
        o.value = String(c.mask >>> 0);
        o.textContent = c.name;
        sel.appendChild(o);
      }
      sel.value = String(s.p32 >>> 0);
      sel.onchange = () => { s.p32 = parseIntFlex(sel.value, 0) >>> 0; };
      container.appendChild(sel);
    }
  }

  selKind.onchange = () => {
    s.k = parseIntFlex(selKind.value, 0);
    s.h = 0;
    s.p16 = 0;
    s.p32 = 0;
    renderParamsFor();
  };

  renderParamsFor();
}

async function macroAdd(){
  const macros = getMacrosArray();
  if (macros.length >= 8) return;

  const name = prompt("새 매크로 이름 (max 15자):", "Macro");
  if (!name) return;

  macros.push({
    name: name.trim().substring(0, 15),
    steps: [{ k: 4, h: 0, d: 0, p16: 4, p32: 0 }]  // default: KB_TAP A
  });

  g_macroSel = macros.length - 1;
  await macroSave(true);
  macroRenderList();
  macroRenderEditor();
  renderSlotEditor();
}

async function macroSave(silent = false){
  if (!g_profile){ await cfgLoad(); }

  const patch = { macros: getMacrosArray() };
  const u = unwrapApi(await apiPostJson("/api/profiles/active", patch));
  if (!u.ok){
    if (!silent) alert("매크로 저장 실패: " + (u.msg || u.code));
    return false;
  }
  if (!silent){
    await profileReloadAll();
    macroRenderList();
    macroRenderEditor();
    renderSlotEditor();
    setMsg("매크로 저장 완료", true);
  }
  return true;
}

async function macroReload(){
  await profileReloadAll();
  g_macroSel = -1;
  macroRenderList();
  macroRenderEditor();
  renderSlotEditor();
}

/* =======================================================
   Config 탭 (활성 프로파일 E10 파라미터)
   ======================================================= */
function configToUi(cfg){
  if (!cfg) return;
  const e = cfg.e10 || {};
  if (qs("e10Dpi"))        qs("e10Dpi").value = String(e.dpi_level ?? 2);
  if (qs("e10HardClick"))  qs("e10HardClick").value = String((e.hard_click_lock ?? true) ? "true" : "false");
  if (qs("ledBrightness")) qs("ledBrightness").value = String(e.led_brightness ?? 128);
  if (qs("accelThreshold"))qs("accelThreshold").value = String(e.accel_threshold ?? 8.0);
  if (qs("scrollDamp"))    qs("scrollDamp").value = String(e.scroll_cursor_damp ?? 0.25);

  const sb = e.scale_base || [0.55,0.75,1.0];
  qs("sb0") && (qs("sb0").value = String(sb[0]));
  qs("sb1") && (qs("sb1").value = String(sb[1]));
  qs("sb2") && (qs("sb2").value = String(sb[2]));

  const ag = e.accel_gain || [0.35,0.55,0.85];
  qs("ag0") && (qs("ag0").value = String(ag[0]));
  qs("ag1") && (qs("ag1").value = String(ag[1]));
  qs("ag2") && (qs("ag2").value = String(ag[2]));

  const w = e.wheel || {};
  qs("wheelTh")      && (qs("wheelTh").value      = String(w.threshold_deg ?? 90));
  qs("wheelStepMax") && (qs("wheelStepMax").value = String(w.step_max ?? 6));

  const g = e.gesture || {};
  qs("flickDeg")   && (qs("flickDeg").value   = String(g.flick_deg ?? 200));
  qs("cooldownMs") && (qs("cooldownMs").value = String(g.cooldown_ms ?? 600));

  if (qs("cfgJsonArea")) qs("cfgJsonArea").value = pretty(cfg);
}

function uiToConfig(){
  if (!g_profile || !g_profile.config) g_profile = g_profile || { config: {} };
  const cfg = g_profile.config;
  cfg.e10 = cfg.e10 || {};

  const e = cfg.e10;
  e.dpi_level         = parseNum(qs("e10Dpi")?.value, 2);
  e.hard_click_lock   = parseBool(qs("e10HardClick")?.value);
  e.led_brightness    = parseNum(qs("ledBrightness")?.value, 128);
  e.accel_threshold   = parseNum(qs("accelThreshold")?.value, 8.0);
  e.scroll_cursor_damp= parseNum(qs("scrollDamp")?.value, 0.25);

  e.scale_base = [
    parseNum(qs("sb0")?.value, 0.55),
    parseNum(qs("sb1")?.value, 0.75),
    parseNum(qs("sb2")?.value, 1.00),
  ];
  e.accel_gain = [
    parseNum(qs("ag0")?.value, 0.35),
    parseNum(qs("ag1")?.value, 0.55),
    parseNum(qs("ag2")?.value, 0.85),
  ];

  e.wheel = e.wheel || {};
  e.wheel.threshold_deg = parseNum(qs("wheelTh")?.value, 90);
  e.wheel.step_max      = parseNum(qs("wheelStepMax")?.value, 6);

  e.gesture = e.gesture || {};
  e.gesture.flick_deg   = parseNum(qs("flickDeg")?.value, 200);
  e.gesture.cooldown_ms = parseNum(qs("cooldownMs")?.value, 600);

  return cfg;
}

async function cfgLoad(){
  if (!g_profile || !g_profile.config){
    const r = await apiGet("/api/profiles/active");
    const u = unwrapApi(r);
    if (!u.ok || !u.data){ setMsg("Load failed", false); return; }
    g_profile = { idx: u.data.idx, count: u.data.count, config: u.data.config || {} };
  }
  configToUi(g_profile.config);
  setMsg("Config loaded", true);
}

async function cfgSave() {
  if (!g_profile) { await cfgLoad(); }
  const cfg = uiToConfig();
  const patch = { e10: cfg.e10 };
  const u = unwrapApi(await apiPostJson("/api/profiles/active", patch));
  if (!u.ok) { setMsg("Save failed: " + (u.msg || u.code), false); return; }
  
  // 슬롯/매크로는 그대로 두고 e10만 갱신
  if (g_profile.config) g_profile.config.e10 = cfg.e10;
  configToUi(g_profile.config);
  setMsg("Save OK (reloaded=" + (u.data?.reloaded ? "yes" : "no") + ")", true);
  await refreshStatus();
}

function cfgExport(){
  const cfg = (g_profile && g_profile.config) ? g_profile.config : {};
  const blob = new Blob([pretty(cfg)], { type: "application/json" });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  const pName = (cfg.name || ("profile_" + (g_profile ? g_profile.idx : 0))).replace(/[^a-zA-Z0-9_\-]/g, "_");
  a.download = `${pName}.json`;
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}

async function cfgImport(file){
  if (!file) return;
  const text = await file.text();
  let obj = null;
  try { obj = JSON.parse(text); } catch(e){
    setMsg("Import JSON error: " + e.message, false);
    return;
  }
  const u = unwrapApi(await apiPostJson("/api/config/import", obj));
  if (!u.ok){
    setMsg("Import failed: " + (u.msg || u.code), false);
    return;
  }
  setMsg("Import OK (new idx=" + (u.data?.idx ?? "?") + ")", true);
  await profileReloadAll();
  renderSlotEditor();
  macroRenderList();
  macroRenderEditor();
  await cfgLoad();
}

/* =======================================================
   Control (Quick)
   ======================================================= */
async function ctlSetPpt(enable){
  const u = unwrapApi(await apiPostJson("/api/control", {
    cmd: "set_ppt", enable: !!enable, snapshot: false
  }));
  if (!u.ok) alert("set_ppt failed: " + (u.msg || u.code));
  await refreshStatus();
}

async function ctlGyroCalib(){
  if (!confirm("기기를 평평한 곳에 정지 유지.\n자이로 캘리브레이션 진행?")) return;
  const u = unwrapApi(await apiPostJson("/api/control", {
    cmd: "gyro_calib", snapshot: false
  }));
  if (!u.ok) alert("gyro_calib failed: " + (u.msg || u.code));
  else alert("요청 완료");
  await refreshStatus();
}

async function ctlForceRelease(){
  const u = unwrapApi(await apiPostJson("/api/control", {
    cmd: "force_release", snapshot: false
  }));
  if (!u.ok) alert("force_release failed: " + (u.msg || u.code));
  else alert("모든 버튼/키 해제 완료");
  await refreshStatus();
}

async function ctlI2cRecover(){
  if (!confirm("MPU6050 I2C 복구 진행?")) return;
  const u = unwrapApi(await apiPostJson("/api/control", {
    cmd: "i2c_recover", snapshot: false
  }));
  if (!u.ok) alert("i2c_recover failed: " + (u.msg || u.code));
  else alert("요청 완료");
  await refreshStatus();
}

/* =======================================================
   SafeBoot / Factory / Reboot
   ======================================================= */
async function safeInfo(){
  const r = await apiGet("/api/safeboot");
  alert(pretty(r.json || r.text));
}

async function factoryReset(){
  if (!confirm("Factory Reset 진행? (모든 프로파일 삭제 및 재부팅)")) return;
  const r = await fetch("/api/factory_reset", { method:"POST" });
  const t = await r.text();
  alert(t);
}

async function rebootDevice(){
  if (!confirm("디바이스를 재부팅하시겠습니까?")) return;
  const u = unwrapApi(await apiPostJson("/api/reboot", { delay_ms: 500 }));
  alert(u.ok ? "재부팅 요청 완료 (약 3초 후 재접속)" : "재부팅 요청 실패: " + (u.msg || u.code));
}

/* =======================================================
   Status / Diag / OTA
   ======================================================= */
async function refreshStatus(){
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
  if (qs("stHeap"))   qs("stHeap").textContent   = `${mem.heap_free ?? "-"} B`;
  if (qs("stWiFi"))   qs("stWiFi").textContent   = `${net.mode ?? "-"} / ${net.ssid ?? "-"}`;
  if (qs("stProfile")){
    qs("stProfile").textContent = `#${cfg.profile_idx ?? "-"} ${cfg.profile_name ?? ""} / 총 ${cfg.profile_count ?? "-"}개`;
  }

  setPill(qs("pillBle"), `BLE: ${e10.ble_connected ? "ON" : "OFF"}`, !!e10.ble_connected);
  setPill(qs("pillSafe"), `SAFE: ${boot.safe_mode ? "ON" : "OFF"}`, !!boot.safe_mode);

  const el = qs("statusJson");
  if (el) el.textContent = pretty(u);
}

function toggleAppMode(){
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

function isTabOn(name){
  const el = qs("tab-" + name);
  return !!(el && el.classList.contains("on"));
}

function isDiagTyping(){
  const f = qs("diagFilter");
  if (!f) return false;
  return (document.activeElement === f) || (nowMs() < g_diagTypingUntilMs);
}

function renderDiag(u){
  const data = u.data || {};
  const d = data.diag || {};
  const events = Array.isArray(data.events) ? data.events : [];
  const cEl = qs("diagCounters");
  if (cEl){
    cEl.innerHTML = "";
    const items = [
      ["body_too_large", d.body_too_large],
      ["no_body_slot",   d.no_body_slot],
      ["bad_json",       d.json_bad],
      ["safe_blocked",   d.safe_blocked],
      ["ota_blocked",    d.ota_blocked]
    ];
    for (const [k,v] of items){
      const div = document.createElement("div");
      div.className = "pill";
      div.textContent = `${k}: ${v ?? 0}`;
      cEl.appendChild(div);
    }
  }
  const listEl = qs("diagEvents");
  if (listEl){
    listEl.innerHTML = "";
    const f = (qs("diagFilter")?.value || "").trim();
    const shown = events.slice().reverse()
      .filter(ev => !f || String(ev.code || "").includes(f))
      .slice(0, 80);
    for (const ev of shown){
      const div = document.createElement("div");
      div.className = "logline";
      div.textContent = `[${ev.ms ?? 0}ms] ${ev.code ?? "-"}`;
      listEl.appendChild(div);
    }
    if (!shown.length){
      const div = document.createElement("div");
      div.className = "hint2"; div.textContent = "No events.";
      listEl.appendChild(div);
    }
  }
  const rawEl = qs("diagJson");
  if (rawEl) rawEl.textContent = pretty(u);
}

async function refreshDiag(){
  const r = await apiGet("/api/diag");
  renderDiag(unwrapApi(r));
}

async function otaUpload(){
  const f = qs("otaFile")?.files?.[0];
  if (!f){ alert("파일을 선택하세요."); return; }
  qs("otaHint").textContent = `uploading: ${f.name}...`;
  const r = await fetch("/api/ota", { method:"POST", body:f });
  qs("otaHint").textContent = await r.text();
  await otaStatus();
}

async function otaStatus(){
  const r = await apiGet("/api/ota/status");
  qs("otaJson").textContent = pretty(unwrapApi(r).data || r.text);
}

/* =======================================================
   탭 / 바인딩
   ======================================================= */
function bindTabs(){
  qsa(".tab").forEach(b => b.addEventListener("click", () => {
    const prevDiag = isTabOn("diag");
    qsa(".tab").forEach(x => x.classList.remove("on"));
    qsa(".tabpane").forEach(x => x.classList.remove("on"));
    b.classList.add("on");
    const pane = qs("tab-" + b.getAttribute("data-tab"));
    if (pane) pane.classList.add("on");
    if (!prevDiag && isTabOn("diag")) refreshDiag().catch(console.error);
  }));
}

function bindViewToggle(){
  qsa(".view-tab").forEach(b => b.addEventListener("click", () => {
    qsa(".view-tab").forEach(x => x.classList.remove("on"));
    b.classList.add("on");
    g_view = b.dataset.view || "global";
    renderSlotEditor();
  }));
}

function bindUi(){
  bindTabs();
  bindViewToggle();

  qs("pillNet")?.addEventListener("click", toggleAppMode);
  qs("btnRefresh")?.addEventListener("click", refreshStatus);
  qs("btnReboot")?.addEventListener("click", rebootDevice);

  // ---- Profile ----
  qs("btnProfReload")?.addEventListener("click", async () => {
    await profileReloadAll();
    renderSlotEditor();
    macroRenderList();
    macroRenderEditor();
    await cfgLoad();
  });
  qs("btnProfCreate")?.addEventListener("click", profileCreate);
  qs("btnProfDelete")?.addEventListener("click", profileDelete);
  qs("btnProfRename")?.addEventListener("click", profileRename);
  qs("profSelect")?.addEventListener("change", () => {
    const idx = parseIntFlex(qs("profSelect").value, 0);
    profileSwitch(idx).then(() => cfgLoad());
  });

  // ---- Slots ----
  qs("btnSlotSave")?.addEventListener("click", saveProfile);
  qs("btnSlotReload")?.addEventListener("click", async () => {
    await profileReloadAll();
    renderSlotEditor();
    macroRenderList();
    macroRenderEditor();
  });

  // ---- Macros ----
  qs("btnMacroAdd")?.addEventListener("click", macroAdd);
  qs("btnMacroSave")?.addEventListener("click", () => macroSave(false));
  qs("btnMacroReload")?.addEventListener("click", macroReload);

  // ---- Config ----
  qs("btnCfgLoad")?.addEventListener("click", cfgLoad);
  qs("btnCfgSave")?.addEventListener("click", cfgSave);
  qs("btnCfgExport")?.addEventListener("click", cfgExport);
  qs("cfgImportFile")?.addEventListener("change", (e) => {
    const f = e.target.files?.[0];
    e.target.value = "";
    cfgImport(f);
  });

  // ---- Quick Control ----
  qs("btnCtlPptOn")?.addEventListener("click", () => ctlSetPpt(true));
  qs("btnCtlPptOff")?.addEventListener("click", () => ctlSetPpt(false));
  qs("btnGyroCalib")?.addEventListener("click", ctlGyroCalib);
  qs("btnForceRelease")?.addEventListener("click", ctlForceRelease);
  qs("btnI2cRecover")?.addEventListener("click", ctlI2cRecover);

  // ---- SafeBoot / Factory ----
  qs("btnSafeInfo")?.addEventListener("click", safeInfo);
  qs("btnFactory")?.addEventListener("click", factoryReset);

  // ---- OTA ----
  qs("btnOta")?.addEventListener("click", otaUpload);
  qs("btnOtaStatus")?.addEventListener("click", otaStatus);

  // ---- Diag ----
  qs("btnDiagRefresh")?.addEventListener("click", refreshDiag);
  qs("diagFilter")?.addEventListener("input", () => {
    g_diagTypingUntilMs = nowMs() + 1200;
    refreshDiag().catch(console.error);
  });
}

/* =======================================================
   Main
   ======================================================= */
async function main(){
  updateAppModeUi();
  bindUi();

  await loadKeycodes();
  await loadTriggers();
  await profileReloadAll();
  renderSlotEditor();
  macroRenderList();
  macroRenderEditor();
  await cfgLoad();
  await refreshStatus();

  setInterval(() => {
    refreshStatus().catch(()=>{});
    const auto = qs("diagAuto")?.checked;
    if (auto && isTabOn("diag") && !isDiagTyping()) refreshDiag().catch(()=>{});
  }, 2500);
}

main().catch(e => {
  const el = qs("statusJson");
  if (el) el.textContent = String(e?.stack || e);
});

