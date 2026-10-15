/* =======================================================
   File: /www/lib/am_config_motion_0415.js
   Elite AirMouse WebConfig v0415 — Config / Motion Partition
   -------------------------------------------------------
   소스명 : am_config_motion_0415.js
   모듈약어 : CFG_MOTION
   모듈명 : Config UI - Motion Advanced / Precision / Gyro / Linear / Flick / Tilt
   -------------------------------------------------------
   기능 요약
    - configToUi_motion(cfg, e)  : config → UI (Motion 파트)
    - uiToConfig_motion(cfg)     : UI → config (Motion 파트)
    - MOTION_PRESETS             : 프리셋 5종 (PC/PPT/TV/Gaming/Precision)

   [대상 DOM 필드]
    - Click-Freeze  : cfEnable, cfGyroTh, cfMaxMs, cfHoldMs,
                      cfFadeoutMs, cfMoveTh, cfFreezeMoveTh
    - EMA           : emaAlphaMin, emaAlphaMax, emaDeadzoneTh,
                      emaFastTh, emaReversalTh, emaReversalReset
    - Snap          : snapEnable, snapMode1/2/3, snapAxisMode,
                      snapRatio, snapStrength, snapConfirmFrames
    - Precision     : precMode, precDeadzone, precGain, precAccel,
                      precMaxStep, precSmooth, precEntryMs, precExitMs,
                      precEntryStillDeg, precExitMoveDeg, precProfile
    - Gyro Bias     : gyroStillTh, gyroStillWinMs, gyroAlpha
    - Linear        : linTh, linImpulseTh, linWindowMs
    - Flick         : flickDeg(p2p_th), cooldownMs, flickWindowMs
    - Tilt Hold     : tiltAngleDeg, tiltHoldMs, tiltRepeatHz

   [v0415 rev15 / Wave C+D]
    - Precision / Gyro Bias / Linear / Flick(window) / Tilt Hold 매핑
    - BUG-2/3 수정 사항 승계

   [v0415 rev18.1 - Phase 5-B 분할]
    - 본 .js는 am_config_0415.js에서 Motion 파트 분리 (3-모듈 분할)
      · am_config_motion_0415.js  : 본 파일 (Motion)
      · am_config_system_0415.js  : System (기본/Power/Button/Wi-Fi)
      · am_config_0415.js         : 오케스트레이션 (configToUi/uiToConfig + 나머지)
    - 로드 순서 SSOT:
      am_config_motion → am_config_system → am_config
    - configToUi/uiToConfig 엔트리는 오케스트레이터가 각 파티션 호출

   [코드 네이밍 규칙]
    - 전역 상수           : G_모듈약어_ 접두사
    - 전역 변수           : g_모듈약어_ 접두사
    - 함수                : camelCase (또는 _partition 접미사)
    - 로컬 변수           : v_ 접두사
   ======================================================= */

/* =======================================================
   Config → UI (Motion 파트)
   -------------------------------------------------------
    - 입력 e는 이미 cfg.e10 (오케스트레이터가 전달)
    - 클램프 없음 (configToUi는 값 그대로 표시)
   ======================================================= */
function configToUi_motion(cfg, e) {
  if (!e) return;

  // ====================================================
  // [v0415 rev15 / 스키마 412] flick.* 단일 소스
  //   - UI 라벨("휘두르기 감도", "재발동 대기")은 유지
  //   - 실제 필드: flick.p2p_th / flick.cooldown_ms (M30 SSOT)
  // ====================================================
  const fl = e.flick || {};
  if (qs("flickDeg"))      qs("flickDeg").value      = String(fl.p2p_th      ?? 400);
  if (qs("cooldownMs"))    qs("cooldownMs").value    = String(fl.cooldown_ms ?? 600);
  if (qs("flickWindowMs")) qs("flickWindowMs").value = String(fl.window_ms   ?? 200);

  // ====================================================
  // Motion Advanced
  // ====================================================
  const ma = e.motion_adv || {};

  // [Phase 1] Click-Freeze
  const cf = ma.click_freeze || {};
  if (qs("cfEnable"))       qs("cfEnable").value       = String((cf.enable ?? true) ? "true" : "false");
  if (qs("cfGyroTh"))       qs("cfGyroTh").value       = String(cf.gyro_th        ?? 15.0);
  if (qs("cfMaxMs"))        qs("cfMaxMs").value        = String(cf.max_ms         ?? 150);
  if (qs("cfHoldMs"))       qs("cfHoldMs").value       = String(cf.hold_ms        ?? 20);
  if (qs("cfFadeoutMs"))    qs("cfFadeoutMs").value    = String(cf.fadeout_ms     ?? 30);
  if (qs("cfMoveTh"))       qs("cfMoveTh").value       = String(cf.move_th        ?? 2.0);
  if (qs("cfFreezeMoveTh")) qs("cfFreezeMoveTh").value = String(cf.freeze_move_th ?? 30.0);

  // [Phase 2] Adaptive EMA
  const ema = ma.ema || {};
  if (qs("emaAlphaMin"))      qs("emaAlphaMin").value      = String(ema.alpha_min      ?? 0.05);
  if (qs("emaAlphaMax"))      qs("emaAlphaMax").value      = String(ema.alpha_max      ?? 0.80);
  if (qs("emaDeadzoneTh"))    qs("emaDeadzoneTh").value    = String(ema.deadzone_th    ?? 3.0);
  if (qs("emaFastTh"))        qs("emaFastTh").value        = String(ema.fast_th        ?? 15.0);
  if (qs("emaReversalTh"))    qs("emaReversalTh").value    = String(ema.reversal_th    ?? 8.0);
  if (qs("emaReversalReset")) qs("emaReversalReset").value = String((ema.reversal_reset ?? true) ? "true" : "false");

  // [Phase 3] Snap-to-Axis
  const snap = ma.snap || {};
  if (qs("snapEnable"))        qs("snapEnable").value        = String((snap.enable ?? true) ? "true" : "false");
  if (qs("snapMode1"))         qs("snapMode1").checked       = !!(snap.mode_mask & 0x01);
  if (qs("snapMode2"))         qs("snapMode2").checked       = !!(snap.mode_mask & 0x02);
  if (qs("snapMode3"))         qs("snapMode3").checked       = !!(snap.mode_mask & 0x04);
  if (qs("snapAxisMode"))      qs("snapAxisMode").value      = String(snap.axis_mode      ?? 0);
  if (qs("snapRatio"))         qs("snapRatio").value         = String(snap.ratio_enter    ?? 4.0);
  if (qs("snapStrength"))      qs("snapStrength").value      = String(snap.strength       ?? 0.85);
  if (qs("snapConfirmFrames")) qs("snapConfirmFrames").value = String(snap.confirm_frames ?? 3);

  // ====================================================
  // [v0415 rev15 / Wave C] Precision 상세 파라미터
  //   - JSON: e10.precision.{mode, deadzone, gain, ...} (nested)
  // ====================================================
  const p = e.precision || {};
  if (qs("precMode"))          qs("precMode").value          = String(p.mode            ?? 0);
  if (qs("precDeadzone"))      qs("precDeadzone").value      = String(p.deadzone        ?? 1.2);
  if (qs("precGain"))          qs("precGain").value          = String(p.gain            ?? 0.65);
  if (qs("precAccel"))         qs("precAccel").value         = String(p.accel           ?? 0.25);
  if (qs("precMaxStep"))       qs("precMaxStep").value       = String(p.max_step        ?? 18);
  if (qs("precSmooth"))        qs("precSmooth").value        = String(p.smooth          ?? 0.85);
  if (qs("precEntryMs"))       qs("precEntryMs").value       = String(p.entry_ms        ?? 450);
  if (qs("precExitMs"))        qs("precExitMs").value        = String(p.exit_ms         ?? 300);
  if (qs("precEntryStillDeg")) qs("precEntryStillDeg").value = String(p.entry_still_deg ?? 1.2);
  if (qs("precExitMoveDeg"))   qs("precExitMoveDeg").value   = String(p.exit_move_deg   ?? 3.5);
  if (qs("precProfile"))       qs("precProfile").value       = String(p.profile         ?? 0);

  // ====================================================
  // [v0415 rev15 / Wave C] Gyro Bias Tracker 파라미터
  // ====================================================
  const gb = e.gyro_bias || {};
  if (qs("gyroStillTh"))    qs("gyroStillTh").value    = String(gb.still_th     ?? 2.0);
  if (qs("gyroStillWinMs")) qs("gyroStillWinMs").value = String(gb.still_win_ms ?? 250);
  if (qs("gyroAlpha"))      qs("gyroAlpha").value      = String(gb.alpha        ?? 0.001);

  // ====================================================
  // [v0415 rev15 / Wave D] Linear / Tilt Hold
  //   - flick.window_ms는 상단에서 이미 처리
  // ====================================================
  const ln = e.linear || {};
  if (qs("linTh"))        qs("linTh").value        = String(ln.th         ?? 0.3);
  if (qs("linImpulseTh")) qs("linImpulseTh").value = String(ln.impulse_th ?? 0.5);
  if (qs("linWindowMs"))  qs("linWindowMs").value  = String(ln.window_ms  ?? 300);

  const th = e.tilt_hold || {};
  if (qs("tiltAngleDeg")) qs("tiltAngleDeg").value = String(th.angle_deg ?? 15.0);
  if (qs("tiltHoldMs"))   qs("tiltHoldMs").value   = String(th.hold_ms   ?? 300);
  if (qs("tiltRepeatHz")) qs("tiltRepeatHz").value = String(th.repeat_hz ?? 3);
}

/* =======================================================
   UI → Config (Motion 파트)
   -------------------------------------------------------
    - cfg.e10은 이미 존재 (오케스트레이터 보장)
    - 서버 validateE10 범위와 1:1 클램프
   ======================================================= */
function uiToConfig_motion(cfg) {
  if (!cfg || !cfg.e10) return;
  const e = cfg.e10;

  // ====================================================
  // [v0415 rev15 / 스키마 412] flick.* 단일 소스
  // ====================================================
  e.flick = e.flick || {};
  e.flick.p2p_th      = parseNum(qs("flickDeg")?.value, 400);
  e.flick.cooldown_ms = parseNum(qs("cooldownMs")?.value, 600);
  e.flick.window_ms   = parseNum(qs("flickWindowMs")?.value, 200);

  // 클라이언트 클램프 (백엔드 validateE10 정합)
  const _flickClamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
  e.flick.p2p_th      = _flickClamp(e.flick.p2p_th,      50,  2000);
  e.flick.cooldown_ms = _flickClamp(e.flick.cooldown_ms, 100, 5000);
  e.flick.window_ms   = _flickClamp(e.flick.window_ms,   50,  500);

  // ====================================================
  // Motion Advanced
  // ====================================================
  const _c = (v, lo, hi) => Math.max(lo, Math.min(hi, v));

  e.motion_adv = e.motion_adv || {};

  // [Phase 1] Click-Freeze
  e.motion_adv.click_freeze = e.motion_adv.click_freeze || {};
  const cf = e.motion_adv.click_freeze;
  cf.enable         = parseBool(qs("cfEnable")?.value);
  cf.gyro_th        = _c(parseNum(qs("cfGyroTh")?.value, 15.0),       5.0,  30.0);
  cf.max_ms         = _c(parseNum(qs("cfMaxMs")?.value, 150),         50,   300);
  cf.hold_ms        = _c(parseNum(qs("cfHoldMs")?.value, 20),         0,    50);
  cf.fadeout_ms     = _c(parseNum(qs("cfFadeoutMs")?.value, 30),      0,    80);
  cf.move_th        = _c(parseNum(qs("cfMoveTh")?.value, 2.0),        1.0,  10.0);
  cf.freeze_move_th = _c(parseNum(qs("cfFreezeMoveTh")?.value, 30.0), 10.0, 60.0);

  // [Phase 2] Adaptive EMA
  e.motion_adv.ema = e.motion_adv.ema || {};
  const ema = e.motion_adv.ema;
  ema.alpha_min   = _c(parseNum(qs("emaAlphaMin")?.value, 0.05), 0.01, 0.20);
  ema.alpha_max   = _c(parseNum(qs("emaAlphaMax")?.value, 0.80), 0.50, 0.95);
  if (ema.alpha_min >= ema.alpha_max) ema.alpha_max = ema.alpha_min + 0.01;
  ema.deadzone_th = _c(parseNum(qs("emaDeadzoneTh")?.value, 3.0), 1.0, 10.0);
  ema.fast_th     = _c(parseNum(qs("emaFastTh")?.value, 15.0),   10.0, 30.0);
  if (ema.deadzone_th >= ema.fast_th) ema.fast_th = ema.deadzone_th + 1.0;
  ema.reversal_th    = _c(parseNum(qs("emaReversalTh")?.value, 8.0), 5.0, 20.0);
  ema.reversal_reset = parseBool(qs("emaReversalReset")?.value);

  // [Phase 3] Snap-to-Axis
  e.motion_adv.snap = e.motion_adv.snap || {};
  const snap = e.motion_adv.snap;
  snap.enable  = parseBool(qs("snapEnable")?.value);
  snap.mode_mask = ((qs("snapMode1")?.checked ? 0x01 : 0) |
                    (qs("snapMode2")?.checked ? 0x02 : 0) |
                    (qs("snapMode3")?.checked ? 0x04 : 0)) & 0x07;
  snap.axis_mode      = _c(parseNum(qs("snapAxisMode")?.value, 0),       0,    2);
  snap.ratio_enter    = _c(parseNum(qs("snapRatio")?.value, 4.0),        2.0,  10.0);
  snap.strength       = _c(parseNum(qs("snapStrength")?.value, 0.85),    0.5,  1.0);
  snap.confirm_frames = _c(parseNum(qs("snapConfirmFrames")?.value, 3),  1,    10);

  // ====================================================
  // [v0415 rev15 / Wave C] Precision 상세 파라미터
  //   - 강제 조건: exit_move_deg > entry_still_deg
  // ====================================================
  const _pc = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
  e.precision = e.precision || {};
  e.precision.mode            = _pc(parseNum(qs("precMode")?.value, 0),          0,    4);
  e.precision.deadzone        = _pc(parseNum(qs("precDeadzone")?.value, 1.2),    0,    40);
  e.precision.gain            = _pc(parseNum(qs("precGain")?.value, 0.65),       0.01, 10);
  e.precision.accel           = _pc(parseNum(qs("precAccel")?.value, 0.25),      0,    5);
  e.precision.max_step        = _pc(parseNum(qs("precMaxStep")?.value, 18),      1,    200);
  e.precision.smooth          = _pc(parseNum(qs("precSmooth")?.value, 0.85),     0,    0.97);
  e.precision.entry_ms        = _pc(parseNum(qs("precEntryMs")?.value, 450),     50,   5000);
  e.precision.exit_ms         = _pc(parseNum(qs("precExitMs")?.value, 300),      50,   5000);
  e.precision.entry_still_deg = _pc(parseNum(qs("precEntryStillDeg")?.value, 1.2), 0.1, 20);
  e.precision.exit_move_deg   = _pc(parseNum(qs("precExitMoveDeg")?.value, 3.5),   0.1, 50);
  if (e.precision.exit_move_deg <= e.precision.entry_still_deg) {
    e.precision.exit_move_deg = e.precision.entry_still_deg + 0.5;
  }
  e.precision.profile = _pc(parseNum(qs("precProfile")?.value, 0), 0, 5);

  // ====================================================
  // [v0415 rev15 / Wave C] Gyro Bias Tracker
  // ====================================================
  e.gyro_bias = e.gyro_bias || {};
  e.gyro_bias.still_th     = _pc(parseNum(qs("gyroStillTh")?.value, 2.0),    0.1,    20);
  e.gyro_bias.still_win_ms = _pc(parseNum(qs("gyroStillWinMs")?.value, 250), 50,     5000);
  e.gyro_bias.alpha        = _pc(parseNum(qs("gyroAlpha")?.value, 0.001),    0.0001, 0.1);

  // ====================================================
  // [v0415 rev15 / Wave D] Linear / Tilt Hold
  // ====================================================
  e.linear = e.linear || {};
  e.linear.th         = _pc(parseNum(qs("linTh")?.value, 0.3),        0.05, 5);
  e.linear.impulse_th = _pc(parseNum(qs("linImpulseTh")?.value, 0.5), 0.1,  5);
  e.linear.window_ms  = _pc(parseNum(qs("linWindowMs")?.value, 300),  50,   1000);

  e.tilt_hold = e.tilt_hold || {};
  e.tilt_hold.angle_deg = _pc(parseNum(qs("tiltAngleDeg")?.value, 15.0), 5,   45);
  e.tilt_hold.hold_ms   = _pc(parseNum(qs("tiltHoldMs")?.value, 300),    100, 2000);
  e.tilt_hold.repeat_hz = _pc(parseNum(qs("tiltRepeatHz")?.value, 3),    1,   20);
}

/* =======================================================
   Motion 프리셋 5종 (19필드 전체)
   -------------------------------------------------------
    - applyMotionPreset()이 참조 (오케스트레이터)
    - 각 프리셋은 Motion Advanced의 19개 필드 전량 지정
   ======================================================= */
const MOTION_PRESETS = {
  /* 일반 PC */
  PC: {
    click_freeze: {
      enable: true, gyro_th: 15.0, max_ms: 150, hold_ms: 20,
      fadeout_ms: 30, move_th: 2.0, freeze_move_th: 30.0
    },
    ema: {
      alpha_min: 0.05, alpha_max: 0.80, deadzone_th: 3.0,
      fast_th: 15.0, reversal_th: 8.0, reversal_reset: true
    },
    snap: {
      enable: false, mode_mask: 0x00, axis_mode: 0,
      confirm_frames: 3, ratio_enter: 4.0, strength: 0.85
    }
  },

  /* 프레젠테이션 */
  PPT: {
    click_freeze: {
      enable: true, gyro_th: 15.0, max_ms: 100, hold_ms: 20,
      fadeout_ms: 30, move_th: 2.0, freeze_move_th: 30.0
    },
    ema: {
      alpha_min: 0.05, alpha_max: 0.80, deadzone_th: 3.0,
      fast_th: 15.0, reversal_th: 8.0, reversal_reset: true
    },
    snap: {
      enable: true, mode_mask: 0x02, axis_mode: 0,
      confirm_frames: 3, ratio_enter: 4.0, strength: 0.85
    }
  },

  /* 스마트 TV */
  TV: {
    click_freeze: {
      enable: false, gyro_th: 15.0, max_ms: 150, hold_ms: 20,
      fadeout_ms: 30, move_th: 2.0, freeze_move_th: 30.0
    },
    ema: {
      alpha_min: 0.10, alpha_max: 0.85, deadzone_th: 3.5,
      fast_th: 15.0, reversal_th: 8.0, reversal_reset: true
    },
    snap: {
      enable: false, mode_mask: 0x00, axis_mode: 0,
      confirm_frames: 3, ratio_enter: 4.0, strength: 0.85
    }
  },

  /* 게이밍 */
  Gaming: {
    click_freeze: {
      enable: false, gyro_th: 15.0, max_ms: 150, hold_ms: 20,
      fadeout_ms: 30, move_th: 2.0, freeze_move_th: 30.0
    },
    ema: {
      alpha_min: 0.15, alpha_max: 0.90, deadzone_th: 2.0,
      fast_th: 12.0, reversal_th: 8.0, reversal_reset: true
    },
    snap: {
      enable: false, mode_mask: 0x00, axis_mode: 0,
      confirm_frames: 3, ratio_enter: 4.0, strength: 0.85
    }
  },

  /* 정밀 작업 */
  Precision: {
    click_freeze: {
      enable: true, gyro_th: 15.0, max_ms: 200, hold_ms: 25,
      fadeout_ms: 40, move_th: 2.0, freeze_move_th: 30.0
    },
    ema: {
      alpha_min: 0.03, alpha_max: 0.70, deadzone_th: 4.0,
      fast_th: 15.0, reversal_th: 8.0, reversal_reset: true
    },
    snap: {
      enable: false, mode_mask: 0x00, axis_mode: 0,
      confirm_frames: 3, ratio_enter: 4.0, strength: 0.85
    }
  }
};
