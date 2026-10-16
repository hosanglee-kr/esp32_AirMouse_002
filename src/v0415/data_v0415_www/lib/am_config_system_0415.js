/* =======================================================
   File: /www/lib/am_config_system_0415.js
   Elite AirMouse WebConfig v0415 — Config / System Partition
   -------------------------------------------------------
   소스명 : am_config_system_0415.js
   모듈약어 : CFG_SYSTEM
   모듈명 : Config UI - Pointer / Power / Button Timing / Wi-Fi
   -------------------------------------------------------
   기능 요약
    - configToUi_system(cfg, e) : config → UI (System 파트)
    - uiToConfig_system(cfg)    : UI → config (System 파트)

   [대상 DOM 필드]
    - 기본 포인터   : e10Dpi, e10HardClick, ledBrightness,
                      accelThreshold, scrollDamp
    - 스케일/가속   : sb0/1/2, ag0/1/2
    - 휠            : wheelTh, wheelStepMax
    - Power         : pwrIdleSleepM1/M2/M3, pwrIdleSleepBle,
                      pwrPairingIdleSec, pwrDeepSleepSec,
                      pwrWakeMinActiveMs, pwrWomThreshold,
                      pwrWomDuration, pwrFastRecalibMs,
                      pwrLedFadeoutMs, pwrLedFadeinMs
    - Button Timing : btnDebouncePressMs, btnDebounceReleaseMs,
                      btnLongDelayMs, btnDoubleDelayMs,
                      btnMinClickMs, btnHold2sMs, btnHold3sMs,
                      btnDebounceMinTicks
    - Wi-Fi         : wifiMode, wifiApSsid, wifiApPass,
                      wifiStaSsid, wifiStaPass, wifiMdns

   [v0415 rev15 / Wave A]
    - Wi-Fi UI 매핑 (mode/ap/sta/mdns)
    - cfgSave() patch 확장 ({e10, wifi})은 오케스트레이터 담당

   [v0415 rev18.1 - Phase 5-B 분할]
    - 본 .js는 am_config_0415.js에서 System 파트 분리 (3-모듈 분할)
      · am_config_motion_0415.js  : Motion Advanced / Precision / Gyro / ...
      · am_config_system_0415.js  : 본 파일 (Pointer / Power / Button / Wi-Fi)
      · am_config_0415.js         : 오케스트레이션 (configToUi/uiToConfig + 나머지)
    - 로드 순서 SSOT:
      am_config_motion → am_config_system → am_config

   [코드 네이밍 규칙]
    - 전역 상수           : G_모듈약어_ 접두사
    - 전역 변수           : g_모듈약어_ 접두사
    - 함수                : camelCase (또는 _partition 접미사)
    - 로컬 변수           : v_ 접두사
   ======================================================= */

/* =======================================================
   Config → UI (System 파트)
   -------------------------------------------------------
    - 입력 e는 이미 cfg.e10 (오케스트레이터가 전달)
    - 클램프 없음 (configToUi는 값 그대로 표시)
   ======================================================= */
function configToUi_system(cfg, e) {
  if (!e) return;
  
  // ====================================================
  // 기본 포인터 파라미터
  // ====================================================
  if (qs("e10Dpi")) qs("e10Dpi").value = String(e.dpi_level ?? 2);
  if (qs("e10HardClick")) qs("e10HardClick").value = String((e.hard_click_lock ?? true) ? "true" : "false");
  if (qs("ledBrightness")) qs("ledBrightness").value = String(e.led_brightness ?? 128);
  if (qs("accelThreshold")) qs("accelThreshold").value = String(e.accel_threshold ?? 8.0);
  if (qs("scrollDamp")) qs("scrollDamp").value = String(e.scroll_cursor_damp ?? 0.25);
  
  // ====================================================
  // scale_base / accel_gain
  // ====================================================
  const sb = e.scale_base || [0.55, 0.75, 1.0];
  if (qs("sb0")) qs("sb0").value = String(sb[0]);
  if (qs("sb1")) qs("sb1").value = String(sb[1]);
  if (qs("sb2")) qs("sb2").value = String(sb[2]);
  
  const ag = e.accel_gain || [0.35, 0.55, 0.85];
  if (qs("ag0")) qs("ag0").value = String(ag[0]);
  if (qs("ag1")) qs("ag1").value = String(ag[1]);
  if (qs("ag2")) qs("ag2").value = String(ag[2]);
  
  // ====================================================
  // 휠 스크롤
  // ====================================================
  const w = e.wheel || {};
  if (qs("wheelTh")) qs("wheelTh").value = String(w.threshold_deg ?? 90);
  if (qs("wheelStepMax")) qs("wheelStepMax").value = String(w.step_max ?? 6);
  
  // ====================================================
  // [Phase 11.6] Power
  // ====================================================
  const pw = e.power || {};
  const pIdleMs = Array.isArray(pw.idle_timeout_ms) ?
    pw.idle_timeout_ms :
    [60000, 120000, 300000];
  
  if (qs("pwrIdleSleepM1")) qs("pwrIdleSleepM1").value = String(Math.round((pIdleMs[0] ?? 60000) / 1000));
  if (qs("pwrIdleSleepM2")) qs("pwrIdleSleepM2").value = String(Math.round((pIdleMs[1] ?? 120000) / 1000));
  if (qs("pwrIdleSleepM3")) qs("pwrIdleSleepM3").value = String(Math.round((pIdleMs[2] ?? 300000) / 1000));
  if (qs("pwrIdleSleepBle")) qs("pwrIdleSleepBle").value = String(Math.round((pw.idle_timeout_ble_ms ?? 300000) / 1000));
  if (qs("pwrPairingIdleSec")) qs("pwrPairingIdleSec").value = String(Math.round((pw.pairing_idle_timeout_ms ?? 30000) / 1000));
  if (qs("pwrDeepSleepSec")) qs("pwrDeepSleepSec").value = String(Math.round((pw.deep_idle_timeout_ms ?? 600000) / 1000));
  if (qs("pwrWakeMinActiveMs")) qs("pwrWakeMinActiveMs").value = String(pw.wake_min_active_ms ?? 500);
  if (qs("pwrWomThreshold")) qs("pwrWomThreshold").value = String(pw.wom_threshold ?? 25);
  if (qs("pwrWomDuration")) qs("pwrWomDuration").value = String(pw.wom_duration ?? 4);
  if (qs("pwrFastRecalibMs")) qs("pwrFastRecalibMs").value = String(pw.fast_recalib_ms ?? 300);
  if (qs("pwrLedFadeoutMs")) qs("pwrLedFadeoutMs").value = String(pw.led_fadeout_ms ?? 500);
  if (qs("pwrLedFadeinMs")) qs("pwrLedFadeinMs").value = String(pw.led_fadein_ms ?? 300);
  
  // ====================================================
  // [Phase 11.7] Button Timing
  // ====================================================
  const bt = e.button || {};
  if (qs("btnDebouncePressMs")) qs("btnDebouncePressMs").value = String(bt.debounce_press_ms ?? 32);
  if (qs("btnDebounceReleaseMs")) qs("btnDebounceReleaseMs").value = String(bt.debounce_release_ms ?? 16);
  if (qs("btnLongDelayMs")) qs("btnLongDelayMs").value = String(bt.long_delay_ms ?? 800);
  if (qs("btnDoubleDelayMs")) qs("btnDoubleDelayMs").value = String(bt.double_delay_ms ?? 320);
  if (qs("btnMinClickMs")) qs("btnMinClickMs").value = String(bt.min_click_ms ?? 16);
  if (qs("btnHold2sMs")) qs("btnHold2sMs").value = String(bt.hold_2s_ms ?? 2000);
  if (qs("btnHold3sMs")) qs("btnHold3sMs").value = String(bt.hold_3s_ms ?? 3000);
  if (qs("btnDebounceMinTicks")) qs("btnDebounceMinTicks").value = String(bt.debounce_min_ticks ?? 3);
  
  // ====================================================
  // [v0415 rev15 / Wave A] Wi-Fi 설정 UI 매핑
  // ====================================================
  const wifi = cfg.wifi || {};
  if (qs("wifiMode")) qs("wifiMode").value = String(wifi.mode ?? 0);
  if (qs("wifiApSsid")) qs("wifiApSsid").value = (wifi.ap && wifi.ap.ssid) || "EliteAirMouse";
  if (qs("wifiApPass")) qs("wifiApPass").value = (wifi.ap && wifi.ap.pass) || "";
  if (qs("wifiStaSsid")) qs("wifiStaSsid").value = (wifi.sta && wifi.sta.ssid) || "";
  if (qs("wifiStaPass")) qs("wifiStaPass").value = (wifi.sta && wifi.sta.pass) || "";
  if (qs("wifiMdns")) qs("wifiMdns").value = (wifi.mdns && wifi.mdns.host) || "elite-airmouse";
}

/* =======================================================
   UI → Config (System 파트)
   -------------------------------------------------------
    - cfg.e10은 이미 존재 (오케스트레이터 보장)
    - 서버 validateE10/validateWiFi 범위와 1:1 클램프
   ======================================================= */
function uiToConfig_system(cfg) {
  if (!cfg || !cfg.e10) return;
  const e = cfg.e10;
  
  // ====================================================
  // 기본 포인터 파라미터
  // ====================================================
  e.dpi_level = parseNum(qs("e10Dpi")?.value, 2);
  e.hard_click_lock = parseBool(qs("e10HardClick")?.value);
  e.led_brightness = parseNum(qs("ledBrightness")?.value, 128);
  e.accel_threshold = parseNum(qs("accelThreshold")?.value, 8.0);
  e.scroll_cursor_damp = parseNum(qs("scrollDamp")?.value, 0.25);
  
  // ====================================================
  // scale_base / accel_gain
  // ====================================================
  e.scale_base = [
    parseNum(qs("sb0")?.value, 0.55),
    parseNum(qs("sb1")?.value, 0.75),
    parseNum(qs("sb2")?.value, 1.00)
  ];
  e.accel_gain = [
    parseNum(qs("ag0")?.value, 0.35),
    parseNum(qs("ag1")?.value, 0.55),
    parseNum(qs("ag2")?.value, 0.85)
  ];
  
  // ====================================================
  // 휠 스크롤
  // ====================================================
  e.wheel = e.wheel || {};
  e.wheel.threshold_deg = parseNum(qs("wheelTh")?.value, 90);
  e.wheel.step_max = parseNum(qs("wheelStepMax")?.value, 6);
  
  // ====================================================
  // [Phase 11.6] Power
  // ====================================================
  e.power = e.power || {};
  const pSec2Ms = (v, defSec) => Math.max(0, Math.round(parseNum(v, defSec) * 1000));
  
  e.power.idle_timeout_ms = [
    pSec2Ms(qs("pwrIdleSleepM1")?.value, 60),
    pSec2Ms(qs("pwrIdleSleepM2")?.value, 120),
    pSec2Ms(qs("pwrIdleSleepM3")?.value, 300)
  ];
  e.power.idle_timeout_ble_ms = pSec2Ms(qs("pwrIdleSleepBle")?.value, 300);
  e.power.pairing_idle_timeout_ms = pSec2Ms(qs("pwrPairingIdleSec")?.value, 30);
  
  // [N-1] deep_idle_timeout_ms 클램프 (0 또는 300000~7200000ms)
  {
    const v_deepSec = parseNum(qs("pwrDeepSleepSec")?.value, 600);
    if (v_deepSec !== 0 && v_deepSec < 300) {
      e.power.deep_idle_timeout_ms = 300 * 1000; // 자동 상향 (300초)
    } else if (v_deepSec > 7200) {
      e.power.deep_idle_timeout_ms = 7200 * 1000; // 자동 하향 (7200초)
    } else {
      e.power.deep_idle_timeout_ms = Math.round(v_deepSec * 1000);
    }
  }
  
  e.power.wake_min_active_ms = parseNum(qs("pwrWakeMinActiveMs")?.value, 500);
  e.power.wom_threshold = parseNum(qs("pwrWomThreshold")?.value, 25);
  e.power.wom_duration = parseNum(qs("pwrWomDuration")?.value, 4);
  e.power.fast_recalib_ms = parseNum(qs("pwrFastRecalibMs")?.value, 300);
  e.power.led_fadeout_ms = parseNum(qs("pwrLedFadeoutMs")?.value, 500);
  e.power.led_fadein_ms = parseNum(qs("pwrLedFadeinMs")?.value, 300);
  
  // 클라이언트 측 범위 클램프
  const _pwClamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
  e.power.wake_min_active_ms = _pwClamp(e.power.wake_min_active_ms, 100, 2000);
  e.power.wom_threshold = _pwClamp(e.power.wom_threshold, 5, 100);
  e.power.wom_duration = _pwClamp(e.power.wom_duration, 1, 50);
  e.power.fast_recalib_ms = _pwClamp(e.power.fast_recalib_ms, 100, 1000);
  e.power.led_fadeout_ms = _pwClamp(e.power.led_fadeout_ms, 0, 1000);
  e.power.led_fadein_ms = _pwClamp(e.power.led_fadein_ms, 0, 1000);
  
  // ====================================================
  // [Phase 11.7] Button Timing
  // ====================================================
  e.button = e.button || {};
  e.button.debounce_press_ms = parseNum(qs("btnDebouncePressMs")?.value, 32);
  e.button.debounce_release_ms = parseNum(qs("btnDebounceReleaseMs")?.value, 16);
  e.button.long_delay_ms = parseNum(qs("btnLongDelayMs")?.value, 800);
  e.button.double_delay_ms = parseNum(qs("btnDoubleDelayMs")?.value, 320);
  e.button.min_click_ms = parseNum(qs("btnMinClickMs")?.value, 16);
  e.button.hold_2s_ms = parseNum(qs("btnHold2sMs")?.value, 2000);
  e.button.hold_3s_ms = parseNum(qs("btnHold3sMs")?.value, 3000);
  e.button.debounce_min_ticks = parseNum(qs("btnDebounceMinTicks")?.value, 3);
  
  // 서버 validateE10 강제 조건 클라이언트 선반영
  if (e.button.debounce_press_ms < e.button.debounce_release_ms) {
    e.button.debounce_press_ms = e.button.debounce_release_ms;
  }
  if (e.button.hold_2s_ms >= e.button.hold_3s_ms) {
    e.button.hold_3s_ms = e.button.hold_2s_ms + 1000;
  }
  const _align8 = (v) => Math.round(v / 8) * 8;
  e.button.debounce_press_ms = Math.max(8, _align8(e.button.debounce_press_ms));
  e.button.debounce_release_ms = Math.max(8, _align8(e.button.debounce_release_ms));
  e.button.min_click_ms = _align8(e.button.min_click_ms);
  
  // ====================================================
  // [v0415 rev15 / Wave A] WiFi 설정 UI → config 매핑
  // ====================================================
  cfg.wifi = cfg.wifi || {};
  cfg.wifi.mode = parseNum(qs("wifiMode")?.value, 0);
  
  cfg.wifi.ap = cfg.wifi.ap || {};
  const v_apSsid = (qs("wifiApSsid")?.value || "").trim();
  cfg.wifi.ap.ssid = v_apSsid || "EliteAirMouse"; // 빈 값 방어
  cfg.wifi.ap.pass = qs("wifiApPass")?.value || "";
  
  cfg.wifi.sta = cfg.wifi.sta || {};
  cfg.wifi.sta.ssid = (qs("wifiStaSsid")?.value || "").trim();
  cfg.wifi.sta.pass = qs("wifiStaPass")?.value || "";
  
  cfg.wifi.mdns = cfg.wifi.mdns || {};
  const v_mdns = (qs("wifiMdns")?.value || "").trim();
  cfg.wifi.mdns.host = v_mdns || "elite-airmouse"; // 빈 값 방어
}
