/* =======================================================
   File: /www/lib/am_offline_data_0415.js
   Elite AirMouse WebConfig v0415 — Offline Mock Data
   -------------------------------------------------------
   소스명 : am_offline_data_0415.js
   모듈약어 : OFFLINE_DATA
   모듈명 : Offline Simulator - Mock Data
   -------------------------------------------------------
   기능 요약
    - 오프라인 시뮬레이터 목업 데이터 (데이터 전담)
      · 27개 트리거 라이브러리 (G_OFFLINE_TRIGGERS)
      · 키코드 프리셋 (G_OFFLINE_KEYCODES, 24-bit Descriptor 정합)
      · 기본 프로파일 (G_OFFLINE_DEFAULT_PROFILE_0, 스키마 412)
      · 오프라인 시작 시각 (G_OFFLINE_START_TIME)

   [v0415 Critical Fix]
    - Consumer mask Descriptor 정합 (24-bit)
      · 이전 0412: 16-bit 자체 압축 (Descriptor 불일치 14/16)
      · v0415: 24-bit HID Report Descriptor 정합 (100%)
    - 스키마 412 / api_ver 411
    - ppt_mode → active_mode 일원화
    - Mode 3 슬롯 p32 재매핑 (WWW_*, KB_POWER)

   [v0415 rev18.1 - Phase 5-B 분할]
    - 본 모듈은 am_offline_0415.js에서 데이터 파트 분리
      · am_offline_data_0415.js   : 본 파일 (목업 데이터)
      · am_offline_store_0415.js  : localStorage 스토어 관리
      · am_offline_0415.js        : handleOfflineApi + 오케스트레이션
    - 로드 순서 SSOT:
      am_base → am_i18n → am_offline_data → am_offline_store → am_offline
    - 데이터 전용 (함수 정의 없음) → 부작용 0

   [코드 네이밍 규칙]
    - 전역 상수           : G_모듈약어_ 접두사 (G_OFFLINE_*)
    - 전역 변수           : g_모듈약어_ 접두사
    - 함수                : camelCase
    - 로컬 변수           : v_ 접두사
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
   오프라인 키코드 (v0415: Consumer mask Descriptor 24-bit 정합)
   -------------------------------------------------------
    - 비트 순서는 HID Report Descriptor (Report ID 0x43) SSOT
    - C20_Action_0415.h의 EN_C20_Consumer_t와 값 체계 동일
    - W10_Def_0415.h의 G_W10_CONSUMER[]와도 값 체계 동일
      (라벨만 W10은 CamelCase, C20/offline은 UPPER_SNAKE_CASE)
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
  // [v0415] 24-bit Descriptor 정합 (Bit 0 ~ Bit 23, 총 24개 + None)
  consumer: [
    { name: "PLAY",       mask: 0x00000001 },  // Bit 0  : 0xB0
    { name: "PAUSE",      mask: 0x00000002 },  // Bit 1  : 0xB1
    { name: "RECORD",     mask: 0x00000004 },  // Bit 2  : 0xB2
    { name: "FF",         mask: 0x00000008 },  // Bit 3  : 0xB3
    { name: "REWIND",     mask: 0x00000010 },  // Bit 4  : 0xB4
    { name: "NEXT_TRACK", mask: 0x00000020 },  // Bit 5  : 0xB5
    { name: "PREV_TRACK", mask: 0x00000040 },  // Bit 6  : 0xB6
    { name: "STOP",       mask: 0x00000080 },  // Bit 7  : 0xB7
    { name: "EJECT",      mask: 0x00000100 },  // Bit 8  : 0xB8
    { name: "RANDOM",     mask: 0x00000200 },  // Bit 9  : 0xB9
    { name: "REPEAT",     mask: 0x00000400 },  // Bit 10 : 0xBC
    { name: "PLAY_PAUSE", mask: 0x00000800 },  // Bit 11 : 0xCD
    { name: "MUTE",       mask: 0x00001000 },  // Bit 12 : 0xE2
    { name: "VOL_UP",     mask: 0x00002000 },  // Bit 13 : 0xE9
    { name: "VOL_DOWN",   mask: 0x00004000 },  // Bit 14 : 0xEA
    { name: "WWW_HOME",   mask: 0x00008000 },  // Bit 15 : 0x0223 (AC Home)
    { name: "MY_COMP",    mask: 0x00010000 },  // Bit 16 : 0x0194
    { name: "CALC",       mask: 0x00020000 },  // Bit 17 : 0x0192
    { name: "WWW_FAV",    mask: 0x00040000 },  // Bit 18 : 0x022A
    { name: "WWW_SEARCH", mask: 0x00080000 },  // Bit 19 : 0x0221 (AC Search)
    { name: "WWW_STOP",   mask: 0x00100000 },  // Bit 20 : 0x0226
    { name: "WWW_BACK",   mask: 0x00200000 },  // Bit 21 : 0x0224 (AC Back)
    { name: "MEDIA_SEL",  mask: 0x00400000 },  // Bit 22 : 0x0183
    { name: "MAIL",       mask: 0x00800000 }   // Bit 23 : 0x018A
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
    { value: 5, name: "HOST_CYCLE" },
    { value: 6, name: "HOST_SELECT_0" },
    { value: 7, name: "HOST_SELECT_1" },
    { value: 8, name: "HOST_SELECT_2" },
    { value: 9, name: "HOST_TOGGLE" },
    { value: 10, name: "HOST_CANCEL" }
  ],
  // [v0415] Keyboard Page Power (0x66) — TV 전원 대체
  kb: (function () {
    const arr = [];
    const names = {
      0: "None", 4: "A", 5: "B", 6: "C", 7: "D", 8: "E", 9: "F", 10: "G", 11: "H", 12: "I", 13: "J", 14: "K", 15: "L",
      16: "M", 17: "N", 18: "O", 19: "P", 20: "Q", 21: "R", 22: "S", 23: "T", 24: "U", 25: "V", 26: "W", 27: "X", 28: "Y", 29: "Z",
      30: "1", 31: "2", 32: "3", 33: "4", 34: "5", 35: "6", 36: "7", 37: "8", 38: "9", 39: "0",
      40: "Enter", 41: "Esc", 42: "Backspace", 43: "Tab", 44: "Space",
      58: "F1", 59: "F2", 60: "F3", 61: "F4", 62: "F5", 63: "F6", 64: "F7", 65: "F8", 66: "F9", 67: "F10", 68: "F11", 69: "F12",
      75: "PageUp", 78: "PageDown", 79: "Right", 80: "Left", 81: "Down", 82: "Up",
      0x66: "Power",  // [v0415] HID Keyboard Page Power

      // [v0415 rev13 / B-3] 기호 키 (0x2D~0x38)
      0x2D: "Minus", 0x2E: "Equal", 0x2F: "LBracket", 0x30: "RBracket",
      0x31: "Backslash", 0x32: "NonUSHash", 0x33: "Semicolon", 0x34: "Quote",
      0x35: "Grave", 0x36: "Comma", 0x37: "Period", 0x38: "Slash",

      // 편집/탐색 (0x39, 0x46~0x4D)
      0x39: "CapsLock",
      0x46: "PrtSc", 0x47: "ScrollLock", 0x48: "Pause",
      0x49: "Insert", 0x4A: "Home", 0x4C: "Delete", 0x4D: "End",

      // 숫자 키패드 (0x53~0x63)
      0x53: "NumLock",
      0x54: "KP_Slash", 0x55: "KP_Star", 0x56: "KP_Minus", 0x57: "KP_Plus",
      0x58: "KP_Enter",
      0x59: "KP_1", 0x5A: "KP_2", 0x5B: "KP_3", 0x5C: "KP_4",
      0x5D: "KP_5", 0x5E: "KP_6", 0x5F: "KP_7", 0x60: "KP_8",
      0x61: "KP_9", 0x62: "KP_0", 0x63: "KP_Dot",

      // 수정자 (0xE0~0xE7)
      0xE0: "LCtrl", 0xE1: "LShift", 0xE2: "LAlt", 0xE3: "LGui",
      0xE4: "RCtrl", 0xE5: "RShift", 0xE6: "RAlt", 0xE7: "RGui"
    };
    // [v0415 rev13 / B-3] 온라인 W10::_kbName과 완전 파리티 (106 codes)
    for (let c = 0; c <= 0xE7; c++) {
      if (!names[c]) continue;
      arr.push({ code: c, name: names[c] });
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
  api_ver: 411,  // [v0415]

  note: "v0415 rev15: Descriptor SSOT consumer mask (24-bit). " +
    "gesture.* removed (schema 412). flick.* is SSOT for M30. " +
    "action_kinds + specials + triggers for UI. " +
    "mods mask == HID modifier byte. " +
    "kb=usage-id(0x07) + Power(0x66). consumer=24-bit mask. " +
    "MACRO kind(10) references macro index via p32."
};

/* =======================================================
   오프라인 기본 프로파일 (v0415 Schema 412)
   -------------------------------------------------------
    - Consumer 슬롯 값: 24-bit Descriptor 정합
    - Mode 3: TV Power → KB_POWER(0x66), CH_UP/DOWN → NONE
    - active_mode로 PPT 판정 (ppt_mode 필드 삭제)
   ======================================================= */
const G_OFFLINE_DEFAULT_PROFILE_0 = {
  ver: 412, // [v0415 rev15 / 스키마 412]
  name: "Default",
  wifi: {
    mode: 0,
    sta: { ssid: "", pass: "" },
    ap: { ssid: "EliteAirMouse", pass: "12345678" },
    mdns: { host: "elite-airmouse" }
  },
  e10: {
    dpi_level: 2,
    hard_click_lock: true,  // v0415: M10 삭제, 스키마 잔존 (no-op)
    scale_base: [0.55, 0.75, 1.0],
    accel_gain: [0.35, 0.55, 0.85],
    accel_threshold: 8.0,
    scroll_cursor_damp: 0.25,

    wheel: { threshold_deg: 90.0, step_max: 6 },
    // [v0415 rev15 / 스키마 412] gesture.* 삭제 — flick.* 단일 소스 (SSOT)

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
    active_mode: 1,           // [v0415] ppt_mode 삭제 → active_mode 일원화
    active_peer_index: 0,

    // Motion Advanced
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
    // Global 슬롯 (Mode 1 base)
    // [v0415] Consumer p32 값 → 24-bit 정합
    global: [
      { k: 2, h: 1, p16: 1, p32: 0 },        // S1  Top L Click    (locked: MouseHold L)
      { k: 1, h: 0, p16: 1, p32: 0 },        // S2  Top L Double
      { k: 4, h: 0, p16: 43, p32: 4 },       // S3  Top L Long     (Alt+Tab)
      { k: 1, h: 0, p16: 4, p32: 0 },        // S4  Top M Click    (Mouse Middle)
      { k: 0, h: 0, p16: 0, p32: 0 },        // S5  Top M Hold     (locked: MoveGate)
      { k: 1, h: 0, p16: 2, p32: 0 },        // S6  Top R Click
      { k: 4, h: 0, p16: 41, p32: 0 },       // S7  Top R Double   (ESC)
      { k: 4, h: 0, p16: 22, p32: 10 },      // S8  Top R Long     (Win+Shift+S)
      { k: 7, h: 0, p16: 0, p32: 0x00002000 }, // S9  Side F Click   VOL_UP
      { k: 7, h: 0, p16: 0, p32: 0x00000020 }, // S10 Side F Long    NEXT_TRACK
      { k: 7, h: 0, p16: 0, p32: 0x00000800 }, // S11 Side C Click   PLAY_PAUSE
      { k: 0, h: 0, p16: 0, p32: 0 },        // S12 Side C Double  (locked)
      { k: 0, h: 0, p16: 0, p32: 0 },        // S13 Side C 2s Hold (locked)
      { k: 7, h: 0, p16: 0, p32: 0x00004000 }, // S14 Side R Click   VOL_DOWN
      { k: 7, h: 0, p16: 0, p32: 0x00000040 }, // S15 Side R Long    PREV_TRACK
      { k: 5, h: 0, p16: 0, p32: 20489 },    // F1  Flick Left     (Ctrl+Win+Left)
      { k: 5, h: 0, p16: 0, p32: 20233 },    // F2  Flick Right    (Ctrl+Win+Right)
      { k: 4, h: 0, p16: 7, p32: 8 },        // F3  Flick Up       (Win+D)
      { k: 4, h: 0, p16: 43, p32: 4 },       // F4  Flick Down     (Alt+Tab)
      { k: 0, h: 0, p16: 0, p32: 0 },        // L1  Linear Left
      { k: 0, h: 0, p16: 0, p32: 0 },        // L2  Linear Right
      { k: 0, h: 0, p16: 0, p32: 0 },        // L3  Linear Up
      { k: 0, h: 0, p16: 0, p32: 0 },        // L4  Linear Down
      { k: 0, h: 0, p16: 0, p32: 0 },        // T1  Tilt Left
      { k: 0, h: 0, p16: 0, p32: 0 },        // T2  Tilt Right
      { k: 0, h: 0, p16: 0, p32: 0 },        // T3  Tilt Up
      { k: 0, h: 0, p16: 0, p32: 0 }         // T4  Tilt Down
    ],
    modes: [
      // Mode 1: 완전 상속 (mask=0)
      { mask: 0, slots: [] },
      // Mode 2: 완전 오버라이드 (mask = 0x07FFFFFF)
      {
        mask: 0x07FFFFFF,
        slots: [
          { k: 4, h: 0, p16: 78, p32: 0 },     // S1  PageDown
          { k: 4, h: 0, p16: 75, p32: 0 },     // S2  PageUp
          { k: 4, h: 0, p16: 62, p32: 2 },     // S3  Shift+F5
          { k: 4, h: 0, p16: 15, p32: 1 },     // S4  Ctrl+L (laser)
          { k: 0, h: 0, p16: 0, p32: 0 },      // S5
          { k: 4, h: 0, p16: 41, p32: 0 },     // S6  ESC
          { k: 4, h: 0, p16: 5, p32: 0 },      // S7  B
          { k: 4, h: 0, p16: 26, p32: 0 },     // S8  W
          { k: 4, h: 0, p16: 78, p32: 0 },     // S9  PageDown
          { k: 4, h: 0, p16: 62, p32: 2 },     // S10 Shift+F5
          { k: 4, h: 0, p16: 19, p32: 1 },     // S11 Ctrl+P
          { k: 0, h: 0, p16: 0, p32: 0 },      // S12
          { k: 0, h: 0, p16: 0, p32: 0 },      // S13
          { k: 4, h: 0, p16: 75, p32: 0 },     // S14 PageUp
          { k: 4, h: 0, p16: 41, p32: 0 },     // S15 ESC
          { k: 4, h: 0, p16: 78, p32: 0 },     // F1  PageDown
          { k: 4, h: 0, p16: 75, p32: 0 },     // F2  PageUp
          { k: 4, h: 0, p16: 5, p32: 0 },      // F3  B
          { k: 0, h: 0, p16: 0, p32: 0 },      // F4
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
      // Mode 3: 완전 오버라이드 (mask = 0x07FFFFFF)
      // [v0415] 재매핑:
      //   AC_* → WWW_*
      //   POWER → KB_TAP(0, 0x66)
      //   CH_UP/DOWN → NONE
      //   TV_INPUT → WWW_SEARCH
      {
        mask: 0x07FFFFFF,
        slots: [
          { k: 7, h: 0, p16: 0, p32: 0x00200000 },  // S1  Top L Click   WWW_BACK
          { k: 7, h: 0, p16: 0, p32: 0x00008000 },  // S2  Top L Double  WWW_HOME
          { k: 4, h: 0, p16: 0x66, p32: 0 },        // S3  Top L Long    KB_TAP(POWER 0x66)
          { k: 4, h: 0, p16: 40, p32: 0 },          // S4  Top M Click   Enter
          { k: 0, h: 0, p16: 0, p32: 0 },           // S5  Top M Hold
          { k: 7, h: 0, p16: 0, p32: 0x00008000 },  // S6  Top R Click   WWW_HOME
          { k: 7, h: 0, p16: 0, p32: 0x00080000 },  // S7  Top R Double  WWW_SEARCH
          { k: 4, h: 0, p16: 0x66, p32: 0 },        // S8  Top R Long    KB_TAP(POWER 0x66)
          { k: 7, h: 0, p16: 0, p32: 0x00002000 },  // S9  Side F Click  VOL_UP
          { k: 0, h: 0, p16: 0, p32: 0 },           // S10 Side F Long   (CH_UP 삭제 → NONE)
          { k: 7, h: 0, p16: 0, p32: 0x00000800 },  // S11 Side C Click  PLAY_PAUSE
          { k: 0, h: 0, p16: 0, p32: 0 },           // S12
          { k: 0, h: 0, p16: 0, p32: 0 },           // S13
          { k: 7, h: 0, p16: 0, p32: 0x00004000 },  // S14 Side R Click  VOL_DOWN
          { k: 0, h: 0, p16: 0, p32: 0 },           // S15 Side R Long   (CH_DOWN 삭제 → NONE)
          { k: 7, h: 0, p16: 0, p32: 0x00000010 },  // F1  Flick Left    REWIND
          { k: 7, h: 0, p16: 0, p32: 0x00000008 },  // F2  Flick Right   FF
          { k: 7, h: 0, p16: 0, p32: 0x00001000 },  // F3  Flick Up      MUTE
          { k: 0, h: 0, p16: 0, p32: 0 },           // F4  Flick Down
          { k: 0, h: 0, p16: 0, p32: 0 },
          { k: 0, h: 0, p16: 0, p32: 0 },
          { k: 0, h: 0, p16: 0, p32: 0 },
          { k: 0, h: 0, p16: 0, p32: 0 },
          { k: 4, h: 0, p16: 80, p32: 0 },          // T1  Tilt Left     Left
          { k: 4, h: 0, p16: 79, p32: 0 },          // T2  Tilt Right    Right
          { k: 4, h: 0, p16: 82, p32: 0 },          // T3  Tilt Up       Up
          { k: 4, h: 0, p16: 81, p32: 0 }           // T4  Tilt Down     Down
        ]
      }
    ]
  },
  macros: []
};
