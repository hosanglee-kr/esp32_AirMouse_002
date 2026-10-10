/* =======================================================
   File: /www/lib/am_offline_0415.js
   Elite AirMouse WebConfig v0415 — Offline Simulator
   - 로드 순서: 3
   - [v0415 Critical Fix] Consumer mask Descriptor 정합 (24-bit)
     · 이전 0412: 16-bit 자체 압축 (Descriptor 불일치 14/16)
     · v0415: 24-bit HID Report Descriptor 정합 (100%)
   - [v0415] 스키마 411 / api_ver 411
   - [v0415] ppt_mode → active_mode 일원화
   - [v0415] Mode 3 슬롯 p32 재매핑 (WWW_*, KB_POWER)
   - [v0415] OFFLINE_STORAGE_KEY 스키마 자동 초기화
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
    { value: 5, name: "HOST_CYCLE" }
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
      0x66: "Power"  // [v0415] HID Keyboard Page Power
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
  api_ver: 411,  // [v0415]
  note: "v0415: Descriptor SSOT consumer mask (24-bit). " +
    "action_kinds + specials + triggers for UI. " +
    "mods mask == HID modifier byte. " +
    "kb=usage-id(0x07) + Power(0x66). consumer=24-bit mask. " +
    "MACRO kind(10) references macro index via p32."
};

/* =======================================================
   오프라인 기본 프로파일 (v0415 Schema 411)
   -------------------------------------------------------
   - Consumer 슬롯 값: 24-bit Descriptor 정합
   - Mode 3: TV Power → KB_POWER(0x66), CH_UP/DOWN → NONE
   - active_mode로 PPT 판정 (ppt_mode 필드 삭제)
   ======================================================= */
const G_OFFLINE_DEFAULT_PROFILE_0 = {
  ver: 411,  // [v0415]
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
    //   idx 8 : VOL_UP      (0x2000)
    //   idx 9 : NEXT_TRACK  (0x0020, 동일)
    //   idx 10: PLAY_PAUSE  (0x0800)
    //   idx 13: VOL_DOWN    (0x4000)
    //   idx 14: PREV_TRACK  (0x0040, 동일)
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
      //   FF/REWIND/MUTE/VOL_*/PLAY_PAUSE → 신 24-bit 값
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

/* =======================================================
   오프라인 로컬 저장소
   - [v0415] STORAGE_KEY 갱신: airmouse_v0412 → airmouse_v0415
     · 스키마 변경(v410→v411)으로 이전 스토어 자동 초기화 유도
   ======================================================= */
const OFFLINE_STORAGE_KEY = "airmouse_v0415_offline_store";
const OFFLINE_STORE_SCHEMA = 6;   // v0415 유지 (내부 스키마 별도)

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
    // [v0415] ppt_mode → active_mode
    const gE10 = {
      ble_connected: false,
      active_mode: 1,
      dpi_level: 2,
      precision_mode: 0,
      gate: { ota_guard: false }
    };
    const gBoot = { safe_mode: false, fail_count: 0, pending: false, last_reset_reason: 0 };
    const gConfig = {
      ver: 411,
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
  else if (path === "/api/reboot/check") {
    resObj.data = { required: false, mask: 0, reasons: "", allowed: false, deny_code: "no_reboot_needed" };
  }
  else {
    resObj.msg = "Offline stub response";
  }

  const jsonStr = JSON.stringify(resObj);
  return { ok: true, status: 200, text: jsonStr, json: resObj };
}
