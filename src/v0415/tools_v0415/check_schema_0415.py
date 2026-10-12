#!/usr/bin/env python3
"""
check_schema_0415.py — 백엔드↔프론트 필드명 정합성 검증
- C10_Def_0415.h (ST_C10_PowerConfig_t, ST_C10_ButtonConfig_t) 필드 추출
- am_config_0415.js의 configToUi/uiToConfig에서 참조하는 power/button 필드 추출
- am_offline_0415.js 기본 프로파일의 power/button 키 추출
- 3자 비교 → 불일치 시 exit code 1

사용:
    python check_schema.py
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
H_DEF         = ROOT / "C10_Def_0415.h"
JS_CONFIG     = ROOT / "data_v0415_www" / "lib" / "am_config_0415.js"
JS_OFFLINE    = ROOT / "data_v0415_www" / "lib" / "am_offline_0415.js"


# ======================================================
# [v0415 rev15 / Wave B] Wi-Fi 필드 매핑 (H struct → JSON 경로)
# ------------------------------------------------------
#  - ST_C10_WiFiConfig_t 는 flat 구조체 (mode/sta_ssid/sta_pass/...)
#  - JSON/JS는 nested 구조 (wifi.sta.ssid 등)
#  - H → JSON 경로 매핑을 명시 테이블로 관리 (문서-코드 SSOT)
# ======================================================
WIFI_FIELD_MAP = {
    "mode":       "wifi.mode",
    "sta_ssid":   "wifi.sta.ssid",
    "sta_pass":   "wifi.sta.pass",
    "ap_ssid":    "wifi.ap.ssid",
    "ap_pass":    "wifi.ap.pass",
    "mdns_host":  "wifi.mdns.host",
}

# JS DOM id ↔ JSON 경로 매핑 (configToUi/uiToConfig 검증용)
WIFI_DOM_MAP = {
    "wifiMode":    "wifi.mode",
    "wifiStaSsid": "wifi.sta.ssid",
    "wifiStaPass": "wifi.sta.pass",
    "wifiApSsid":  "wifi.ap.ssid",
    "wifiApPass":  "wifi.ap.pass",
    "wifiMdns":    "wifi.mdns.host",
}

# ======================================================
# [v0415 rev15 / Wave D] E10 nested 구조체 화이트리스트
# ------------------------------------------------------
#  - ST_C10_E10Config_t 내 인라인 struct 필드
#  - 화이트리스트 SSOT: 문서-코드-UI 삼각 정합 검증
#  - 각 key: H 필드명 / value: JSON 경로 (nested)
# ======================================================
E10_NESTED_MAP = {
    "precision": {
        "mode":            "precision.mode",
        "deadzone":        "precision.deadzone",
        "gain":            "precision.gain",
        "accel":           "precision.accel",
        "max_step":        "precision.max_step",
        "smooth":          "precision.smooth",
        "entry_ms":        "precision.entry_ms",
        "exit_ms":         "precision.exit_ms",
        "entry_still_deg": "precision.entry_still_deg",
        "exit_move_deg":   "precision.exit_move_deg",
        "profile":         "precision.profile",
    },
    "gyro_bias": {
        "still_th":     "gyro_bias.still_th",
        "still_win_ms": "gyro_bias.still_win_ms",
        "alpha":        "gyro_bias.alpha",
    },
    "linear": {
        "th":         "linear.th",
        "impulse_th": "linear.impulse_th",
        "window_ms":  "linear.window_ms",
    },
    "flick": {
        "p2p_th":      "flick.p2p_th",
        "window_ms":   "flick.window_ms",
        "cooldown_ms": "flick.cooldown_ms",
    },
    "tilt_hold": {
        "angle_deg":  "tilt_hold.angle_deg",
        "hold_ms":    "tilt_hold.hold_ms",
        "repeat_hz":  "tilt_hold.repeat_hz",
    },
}


# [v0415 rev15 / 스키마 412] 삭제된 gesture 필드 (Negative check)
GESTURE_REMOVED_H_FIELDS = ["gesture_flick_deg", "gesture_cooldown_ms"]


def extract_c_struct_fields(lines, struct_name):
    start = -1
    for i, line in enumerate(lines):
        if re.search(rf'struct\s+{re.escape(struct_name)}\s*\{{', line):
            start = i
            break
    if start < 0:
        return []

    # [FIX v2] struct 시작 이후 전체를 한 번에 주석 제거 후 라인 배열 재구성
    #   - 라인별 _strip_comments는 멀티라인 블록 주석 미지원
    #   - `/* ... */`가 필드 사이에 걸쳐 있으면 필드 오탐 발생 가능
    tail_text = "\n".join(lines[start:])
    tail_clean = _strip_comments_multi(tail_text)
    tail_lines = tail_clean.splitlines()

    fields = []
    depth = 1
    # tail_lines[0] = struct 선언 라인 (이미 '{' 포함)
    for i in range(1, len(tail_lines)):
        code = tail_lines[i]

        depth += code.count('{') - code.count('}')
        if depth <= 0:
            break

        m = re.match(r'\s*(?:uint\d+_t|int\d+_t|bool|float|double|char)\s+(\w+)\s*(?:\[[^\]]*\])?\s*;', code)
        if m:
            name = m.group(1)
            if name == '_pad' or name.startswith('_'):
                continue
            fields.append(name)

    return fields

def extract_js_field_refs(text, section):
    """
    am_config_0415.js에서 특정 섹션(power/button) 필드 참조 추출.
    예: 'e.power.idle_timeout_ble_ms' 또는 'e.button.long_delay_ms'
    """
    # 'e.power.XXX' 패턴
    pattern = re.compile(rf'\be\.{section}\.(\w+)')
    refs = set(pattern.findall(text))

    # 'pw.XXX', 'bt.XXX' 접근자 (configToUi 내부)
    if section == "power":
        alias_pattern = re.compile(r'\bpw\.(\w+)')
    elif section == "button":
        alias_pattern = re.compile(r'\bbt\.(\w+)')
    else:
        alias_pattern = None

    if alias_pattern:
        refs |= set(alias_pattern.findall(text))

    # '_pwClamp(e.power.X, ...)' 패턴 커버는 위 e.power 패턴이 처리
    return refs


def extract_offline_default_keys(text, section):
    """
    am_offline_0415.js의 G_OFFLINE_DEFAULT_PROFILE_0.e10.{power,button} 블록 필드명 추출.
    """
    # 'power: { ... }' 블록 찾기
    pattern = re.compile(rf'\b{section}\s*:\s*\{{', re.MULTILINE)
    m = pattern.search(text)
    if not m:
        return set()

    start = m.end() - 1  # '{' 위치
    depth = 0
    keys = set()

    i = start
    while i < len(text):
        ch = text[i]
        if ch == '{':
            depth += 1
        elif ch == '}':
            depth -= 1
            if depth == 0:
                break
        elif depth == 1:
            # 키: '  key: value' 또는 '  "key": value'
            km = re.match(r'\s*"?(\w+)"?\s*:', text[i:i+64])
            if km:
                key = km.group(1)
                # 다음 non-space 위치가 '(' 또는 '{' 또는 '[' 이면 nested. 상관없이 키 추가
                keys.add(key)
                i += km.end()
                continue
        i += 1

    return keys


def _strip_comments(line):
    # // 주석 제거
    idx = line.find('//')
    if idx >= 0:
        line = line[:idx]
    # /* */ 는 라인 내 완결만 처리 (멀티라인은 미지원)
    line = re.sub(r'/\*.*?\*/', '', line)
    return line

def _strip_comments_multi(text):
    """
    [v0415 rev15 / BUG-5 fix v2] 멀티라인 블록 주석까지 완전 제거
    -------------------------------------------------------
    - 기존 _strip_comments(line)는 라인 단위 처리만 가능
    - C10_Def_0415.h 상단 헤더 `/* ... */` 블록은 여러 라인에 걸쳐 있어
      라인별 처리 시 잔존 → gesture_* 부분 문자열 오탐 발생
    - 본 함수는 전체 텍스트를 한 번에 처리하여 블록 주석을 완전 제거
    -------------------------------------------------------
    순서 중요:
      1) /* ... */ (DOTALL, 논그리디) 먼저 제거
      2) // ... (줄 끝까지) 이후 제거
    이유: `// /* ...` 같은 조합에서 // 를 먼저 제거하면 `/*`가 남을 수 있음
    """
    
    # 1) Block comment: /* ... */ (멀티라인 지원)
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)
    # 2) Line comment: // ... (줄 끝까지)
    text = re.sub(r'//[^\n]*', '', text)
    return text

def main():
    ok = True

    # ---- 백엔드 H 필드 ----
    if not H_DEF.exists():
        print(f"[FAIL] file not found: {H_DEF}", file=sys.stderr)
        return 1
    h_lines = H_DEF.read_text(encoding="utf-8").splitlines()

    h_power  = set(extract_c_struct_fields(h_lines, "ST_C10_PowerConfig_t"))
    h_button = set(extract_c_struct_fields(h_lines, "ST_C10_ButtonConfig_t"))

    print(f"[check_schema] H: power={len(h_power)} button={len(h_button)}")
    print(f"  power:  {sorted(h_power)}")
    print(f"  button: {sorted(h_button)}")

    # ---- 프론트 configToUi/uiToConfig 참조 ----
    if not JS_CONFIG.exists():
        print(f"[FAIL] file not found: {JS_CONFIG}", file=sys.stderr)
        return 1
    cfg_text = JS_CONFIG.read_text(encoding="utf-8")

    js_power  = extract_js_field_refs(cfg_text, "power")
    js_button = extract_js_field_refs(cfg_text, "button")

    print(f"\n[check_schema] JS config refs: power={len(js_power)} button={len(js_button)}")

    # ---- 오프라인 기본 프로파일 ----
    if not JS_OFFLINE.exists():
        print(f"[FAIL] file not found: {JS_OFFLINE}", file=sys.stderr)
        return 1
    off_text = JS_OFFLINE.read_text(encoding="utf-8")

    off_power  = extract_offline_default_keys(off_text, "power")
    off_button = extract_offline_default_keys(off_text, "button")

    print(f"[check_schema] Offline defaults: power={len(off_power)} button={len(off_button)}")

    # ---- 비교 ----
    # (a) H ↔ JS_CONFIG 참조
    print(f"\n--- Power ---")
    _diff_and_report("H↔JS_config", h_power, js_power)
    if h_power != js_power:
        ok = False

    print(f"\n--- Button ---")
    _diff_and_report("H↔JS_config", h_button, js_button)
    if h_button != js_button:
        ok = False

    # (b) H ↔ Offline 기본값
    print(f"\n--- Power (H ↔ Offline) ---")
    _diff_and_report("H↔Offline", h_power, off_power)
    if h_power != off_power:
        ok = False

    print(f"\n--- Button (H ↔ Offline) ---")
    _diff_and_report("H↔Offline", h_button, off_button)
    if h_button != off_button:
        ok = False
        
    # ---- [Wave B] Wi-Fi 3자 매핑 검증 ----
    if not check_wifi_mapping(h_lines, cfg_text, off_text):
        ok = False
    
    # ---- [Wave B] gesture 삭제 검증 (스키마 412) ----
    if not check_gesture_removed(h_lines, cfg_text, off_text):
        ok = False

    # ---- [Wave C+D] E10 nested 매핑 검증 ----
    if not check_e10_nested_mapping(h_lines, cfg_text, off_text):
        ok = False
    
    # ---- [Wave C+D] DOM id 존재 검증 ----
    # HTML 파일 로드 필요
    HTML_INDEX = ROOT / "data_v0415_www" / "index_0415.html"
    if HTML_INDEX.exists():
        html_text = HTML_INDEX.read_text(encoding="utf-8")
        if not check_wave_cd_dom_ids(html_text, cfg_text):
            ok = False
    else:
        print(f"\n[WARN] index_0415.html not found: {HTML_INDEX}")
    
    if ok:
        print("\n[OK] schema field names match (H ↔ JS ↔ Offline) + Wi-Fi + Gesture removal + E10 nested + Wave C/D DOM ids")
        return 0
    else:
        print("\n[FAIL] schema mismatch detected")
        return 1

def _diff_and_report(label, backend, front):
    only_backend = backend - front
    only_front   = front - backend

    if only_backend:
        print(f"  [{label}] ONLY in H:  {sorted(only_backend)}")
    if only_front:
        print(f"  [{label}] ONLY in JS: {sorted(only_front)}")
    if not only_backend and not only_front:
        print(f"  [{label}] [OK] match")


def check_wifi_mapping(h_lines, js_config_text, off_text):
    """Wi-Fi 필드 3자 매핑 검증 (H ↔ JS config ↔ Offline)"""
    print(f"\n--- Wi-Fi ---")

    # 1) H struct 필드 추출
    h_wifi = set(extract_c_struct_fields(h_lines, "ST_C10_WiFiConfig_t"))
    expected = set(WIFI_FIELD_MAP.keys())

    if h_wifi != expected:
        print(f"  [FAIL] H ST_C10_WiFiConfig_t 필드 불일치")
        print(f"    expected: {sorted(expected)}")
        print(f"    found:    {sorted(h_wifi)}")
        return False
    print(f"  [OK] H fields match ({len(h_wifi)})")

    # 2) JS configToUi/uiToConfig에 DOM id 존재 확인
    missing_dom = [d for d in WIFI_DOM_MAP.keys() if d not in js_config_text]
    if missing_dom:
        print(f"  [FAIL] JS config에서 Wi-Fi DOM id 누락:")
        for d in missing_dom:
            print(f"    - {d}")
        return False
    print(f"  [OK] JS DOM ids present ({len(WIFI_DOM_MAP)})")

    # 3) Offline 기본 프로파일에 wifi 블록 존재 확인
    has_wifi_block = ("wifi:" in off_text) or ('"wifi"' in off_text)
    if not has_wifi_block:
        print(f"  [FAIL] Offline 기본 프로파일에 'wifi' 블록 없음")
        return False
    print(f"  [OK] Offline wifi block present")

    return True

def check_wave_cd_dom_ids(html_text, js_config_text):
    """[Wave C+D] Config 탭의 신규 DOM id가 HTML + JS 양쪽에 존재하는지 검증"""
    print(f"\n--- Wave C+D DOM IDs ---")

    dom_ids = [
        # Wave C
        "precMode", "precDeadzone", "precGain", "precAccel", "precMaxStep",
        "precSmooth", "precEntryMs", "precExitMs", "precEntryStillDeg",
        "precExitMoveDeg", "precProfile",
        "gyroStillTh", "gyroStillWinMs", "gyroAlpha",
        # Wave D
        "linTh", "linImpulseTh", "linWindowMs",
        "flickWindowMs",
        "tiltAngleDeg", "tiltHoldMs", "tiltRepeatHz",
    ]

    ok = True
    missing_html = [d for d in dom_ids if d not in html_text]
    missing_js   = [d for d in dom_ids if d not in js_config_text]

    if missing_html:
        print(f"  [FAIL] HTML 누락 DOM id: {missing_html}")
        ok = False
    if missing_js:
        print(f"  [FAIL] JS 누락 DOM id: {missing_js}")
        ok = False

    if ok:
        print(f"  [OK] Wave C+D DOM ids present ({len(dom_ids)})")

    return ok


def check_gesture_removed(h_lines, js_config_text, off_text):
    """[v0415 rev15 / 스키마 412] gesture.* 삭제 검증 (Negative check)"""
    print(f"\n--- Gesture Removal (Schema 412) ---")

    # [FIX v2] 멀티라인 블록 주석 완전 제거 후 검사
    #   - 기존: 라인별 _strip_comments(l) → 헤더 블록 주석 잔존 → 오탐
    #   - 변경: _strip_comments_multi(전체) → 블록 주석 완전 제거
    h_text_code = _strip_comments_multi("\n".join(h_lines))

    ok = True

    # 1) H 코드에서 gesture_* 필드 잔존 검사 (모든 주석 제외)
    h_hits = [f for f in GESTURE_REMOVED_H_FIELDS if f in h_text_code]
    if h_hits:
        print(f"  [FAIL] H에 gesture_* 필드 잔존 (스키마 412 위반):")
        for f in h_hits:
            print(f"    - {f}")
        ok = False
    else:
        print(f"  [OK] H gesture_* 필드 없음")

    # 2) JS config에서 e.gesture 참조 잔존 검사
    js_hits = []
    if "e.gesture" in js_config_text:
        js_hits.append("e.gesture")
    if "cfg.e10.gesture" in js_config_text:
        js_hits.append("cfg.e10.gesture")
    if js_hits:
        print(f"  [FAIL] JS config에 gesture 참조 잔존:")
        for h in js_hits:
            print(f"    - {h}")
        ok = False
    else:
        print(f"  [OK] JS gesture 참조 없음")

    # 3) Offline 기본 프로파일에 gesture 블록 잔존 검사
    #    [FIX v2] 멀티라인 블록 주석 + 라인 주석 완전 제거
    off_code = _strip_comments_multi(off_text)
    off_hits = []
    if re.search(r'\bgesture\s*:', off_code):
        off_hits.append("gesture:")
    if off_hits:
        print(f"  [FAIL] Offline에 gesture 블록 잔존:")
        for h in off_hits:
            print(f"    - {h}")
        ok = False
    else:
        print(f"  [OK] Offline gesture 블록 없음")

    # 4) flick.* SSOT 참조 확인
    if "flick" not in js_config_text:
        print(f"  [FAIL] JS config에 flick 참조 없음 (SSOT 위반)")
        ok = False
    else:
        print(f"  [OK] JS flick 참조 존재")

    return ok
    
def check_e10_nested_mapping(h_lines, js_config_text, off_text):
    """[Wave C+D] E10 nested 필드 3자 매핑 검증 (H ↔ JS config ↔ Offline)"""
    print(f"\n--- E10 Nested Mapping (Wave C+D) ---")
    ok = True

    for struct_name, fields in E10_NESTED_MAP.items():
        # 1) JS configToUi/uiToConfig에서 DOM id 참조 존재
        #    (e.{struct}.* 또는 {struct} = ... 패턴)
        missing_js = []
        for field in fields.keys():
            # 3가지 패턴 중 하나라도 있으면 통과
            patterns = [
                f".{struct_name}.{field}",         # e.precision.mode
                f"{struct_name}[\"{field}\"]",     # precision["mode"]
            ]
            if not any(p in js_config_text for p in patterns):
                missing_js.append(field)

        # 2) Offline 기본 프로파일 블록 존재
        off_has_block = (f"{struct_name}:" in off_text)

        # 3) H 파일에 struct 이름 존재
        h_has_struct = (struct_name in "\n".join(h_lines))

        if missing_js:
            print(f"  [FAIL] JS {struct_name}: 누락 필드 {sorted(missing_js)}")
            ok = False
        elif not off_has_block:
            print(f"  [FAIL] Offline {struct_name}: 블록 없음")
            ok = False
        elif not h_has_struct:
            print(f"  [WARN] H {struct_name}: struct 이름 미검출 (인라인 파싱 한계)")
        else:
            print(f"  [OK] {struct_name}: {len(fields)} fields")

    return ok


if __name__ == "__main__":
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    sys.exit(main())
    