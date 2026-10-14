#!/usr/bin/env python3
"""
check_schema_0415.py — 백엔드↔프론트 필드명 정합성 검증
- C10_Def_0415.h (ST_C10_PowerConfig_t, ST_C10_ButtonConfig_t) 필드 추출
- am_config_0415.js의 configToUi/uiToConfig에서 참조하는 power/button 필드 추출
- am_offline_0415.js 기본 프로파일의 power/button 키 추출
- 3자 비교 → 불일치 시 exit code 1

[v0415 Phase 3-D 확장]
- check_e10_json_parity(): _buildE10Json ↔ _patchE10Json 필드 대칭성
- check_w10_kb_names_sorted(): G_W10_KB_NAMES[] 이진 탐색 전제 검증
- check_w10_err_map_sorted(): G_W10_ERR_MAP[] 이진 탐색 전제 검증

사용:
    python check_schema_0415.py
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
H_DEF         = ROOT / "C10_Def_0415.h"
JS_CONFIG     = ROOT / "data_v0415_www" / "lib" / "am_config_0415.js"
JS_OFFLINE    = ROOT / "data_v0415_www" / "lib" / "am_offline_0415.js"


# ======================================================
# [v0415 rev15 / Wave B] Wi-Fi 필드 매핑
# ======================================================
WIFI_FIELD_MAP = {
    "mode":       "wifi.mode",
    "sta_ssid":   "wifi.sta.ssid",
    "sta_pass":   "wifi.sta.pass",
    "ap_ssid":    "wifi.ap.ssid",
    "ap_pass":    "wifi.ap.pass",
    "mdns_host":  "wifi.mdns.host",
}

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

    tail_text = "\n".join(lines[start:])
    tail_clean = _strip_comments_multi(tail_text)
    tail_lines = tail_clean.splitlines()

    fields = []
    depth = 1
    for i in range(1, len(tail_lines)):
        code = tail_lines[i]

        depth += code.count('{') - code.count('}')
        if depth <= 0:
            break

        m = re.match(
            r'\s*(?:uint\d+_t|int\d+_t|bool|float|double|char)\s+(\w+)\s*(?:\[[^\]]*\])?\s*;',
            code)
        if m:
            name = m.group(1)
            if name == '_pad' or name.startswith('_'):
                continue
            fields.append(name)

    return fields


def extract_js_field_refs(text, section):
    pattern = re.compile(rf'\be\.{section}\.(\w+)')
    refs = set(pattern.findall(text))

    if section == "power":
        alias_pattern = re.compile(r'\bpw\.(\w+)')
    elif section == "button":
        alias_pattern = re.compile(r'\bbt\.(\w+)')
    else:
        alias_pattern = None

    if alias_pattern:
        refs |= set(alias_pattern.findall(text))

    return refs


def extract_offline_default_keys(text, section):
    pattern = re.compile(rf'\b{section}\s*:\s*\{{', re.MULTILINE)
    m = pattern.search(text)
    if not m:
        return set()

    start = m.end() - 1
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
            km = re.match(r'\s*"?(\w+)"?\s*:', text[i:i+64])
            if km:
                key = km.group(1)
                keys.add(key)
                i += km.end()
                continue
        i += 1

    return keys


def _strip_comments(line):
    idx = line.find('//')
    if idx >= 0:
        line = line[:idx]
    line = re.sub(r'/\*.*?\*/', '', line)
    return line


def _strip_comments_multi(text):
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)
    text = re.sub(r'//[^\n]*', '', text)
    return text


# ======================================================
# [Wave E] JS 번들 완전성 검증
# ======================================================
def check_bundle_completeness(root):
    print(f"\n--- Bundle Completeness (Wave E) ---")

    html_path = root / "data_v0415_www" / "index_0415.html"
    if not html_path.exists():
        print(f"  [WARN] index_0415.html not found: {html_path}")
        return True
    html = html_path.read_text(encoding="utf-8")
    script_re = re.compile(
        r'<script\s+src="\./((?:lib/am_[^"]+\.js)|(?:app_[^"]+\.js))"\s*>\s*</script>'
    )
    html_order = script_re.findall(html)

    pio_path = root / "tools_v0415" / "pio_gzip_0415.py"
    if not pio_path.exists():
        print(f"  [WARN] pio_gzip_0415.py not found: {pio_path}")
        return True
    pio_src = pio_path.read_text(encoding="utf-8")
    m = re.search(r'BUNDLE_JS_ORDER\s*=\s*\[(.*?)\]', pio_src, re.DOTALL)
    if not m:
        print(f"  [FAIL] BUNDLE_JS_ORDER 파싱 실패")
        return False
    bundle_order = re.findall(r'"([^"]+)"', m.group(1))

    lib_dir = root / "data_v0415_www" / "lib"
    lib_files = sorted(p.name for p in lib_dir.glob("am_*.js")) if lib_dir.exists() else []

    ok = True

    if html_order != bundle_order:
        print(f"  [FAIL] HTML script 순서 ≠ BUNDLE_JS_ORDER")
        only_html   = [x for x in html_order   if x not in bundle_order]
        only_bundle = [x for x in bundle_order if x not in html_order]
        if only_html:
            print(f"    HTML에만: {only_html}")
        if only_bundle:
            print(f"    BUNDLE에만: {only_bundle}")
        if html_order == sorted(html_order) and bundle_order == sorted(bundle_order):
            print(f"    (집합은 동일, 순서만 다름)")
        ok = False
    else:
        print(f"  [OK] HTML ↔ BUNDLE_JS_ORDER 순서 일치 ({len(bundle_order)}개)")

    for rel in bundle_order:
        if not rel.startswith("lib/"):
            continue
        fpath = root / "data_v0415_www" / rel
        if not fpath.exists():
            print(f"  [FAIL] 번들 대상 파일 없음: {rel}")
            ok = False

    bundled_libs = {r for r in bundle_order if r.startswith("lib/")}
    actual_libs = {f"lib/{n}" for n in lib_files}
    missing = actual_libs - bundled_libs
    if missing:
        print(f"  [FAIL] 번들에서 누락된 lib 파일:")
        for m_ in sorted(missing):
            print(f"    - {m_}")
        ok = False

    return ok


# ======================================================
# [v0415 rev17] B20_HOST 상수 3자 정합 검증
# ======================================================
def check_b20_host_constants(root):
    print(f"\n--- B20_HOST Constants (rev17) ---")
    ok = True

    c10_path = root / "C10_Def_0415.h"
    if c10_path.exists():
        c10_src = c10_path.read_text(encoding="utf-8")
        m = re.search(r'HOSTS_MAX\s*=\s*(\d+)', c10_src)
        c10_hosts_max = int(m.group(1)) if m else None
    else:
        c10_hosts_max = None

    b20_path = root / "B20_Ble_0415.h"
    if not b20_path.exists():
        print(f"  [WARN] B20_Ble_0415.h not found")
        return True
    b20_src = b20_path.read_text(encoding="utf-8")

    m_bond = re.search(r'BOND_MAX\s*=\s*(\d+)', b20_src)
    m_to   = re.search(r'CONN_TIMEOUT_MS\s*=\s*(\d+)', b20_src)
    m_sent = re.search(r'TARGET_IDX_NONE\s*=\s*(0x[0-9A-Fa-f]+|\d+)', b20_src)

    b20_bond = int(m_bond.group(1)) if m_bond else None
    b20_to   = int(m_to.group(1))   if m_to   else None
    b20_sent = int(m_sent.group(1), 0) if m_sent else None

    print(f"  C10_DEF::HOSTS_MAX      = {c10_hosts_max}")
    print(f"  B20_HOST::BOND_MAX      = {b20_bond}")
    print(f"  B20_HOST::CONN_TIMEOUT_MS = {b20_to}")
    print(f"  B20_HOST::TARGET_IDX_NONE = {hex(b20_sent) if b20_sent is not None else None}")

    if c10_hosts_max is not None and b20_bond is not None and c10_hosts_max != b20_bond:
        print(f"  [FAIL] HOSTS_MAX({c10_hosts_max}) != BOND_MAX({b20_bond})")
        ok = False
    if b20_sent is not None and b20_sent != 0xFF:
        print(f"  [FAIL] TARGET_IDX_NONE != 0xFF")
        ok = False

    if ok:
        print(f"  [OK] B20_HOST constants consistent")
    return ok


# ======================================================
# [v0415 Phase 3-D] E10 JSON 대칭성 검증
# ------------------------------------------------------
#  _buildE10Json ↔ _patchE10Json 필드 대칭성
#  - 정규식으로 key 참조를 추출하여 좌우 비교
#  - 완전 파싱이 아닌 "휴리스틱" → false positive 가능
# ======================================================
def check_e10_json_parity(root):
    print(f"\n--- E10 JSON Parity (build ↔ patch) ---")
    p = root / "C10_Config_0415.cpp"
    if not p.exists():
        print(f"  [WARN] C10_Config_0415.cpp not found")
        return True

    src = p.read_text(encoding="utf-8")

    # build 함수 본문 추출
    m_build = re.search(
        r'void\s+CL_C10_Config::_buildE10Json\s*\([^)]*\)\s*\{(.*?)\n\}',
        src, re.DOTALL)
    # patch 함수 본문 추출
    m_patch = re.search(
        r'bool\s+CL_C10_Config::_patchE10Json\s*\([^)]*\)\s*\{(.*?)\n\}',
        src, re.DOTALL)

    if not m_build or not m_patch:
        print(f"  [WARN] build/patch 함수 파싱 실패")
        return True

    # 대입 패턴: p_parent["X"] = ... 또는 v_x["X"] = ...
    key_re = re.compile(r'\b\w+\["([^"]+)"\]\s*=')

    build_keys = set(key_re.findall(m_build.group(1)))
    patch_keys = set(key_re.findall(m_patch.group(1)))

    only_build = build_keys - patch_keys
    only_patch = patch_keys - build_keys

    ok = True
    if only_build:
        print(f"  [FAIL] build에만 존재 (patch 누락): {sorted(only_build)}")
        ok = False
    if only_patch:
        print(f"  [FAIL] patch에만 존재 (build 누락): {sorted(only_patch)}")
        ok = False
    if ok:
        print(f"  [OK] E10 JSON 대칭성 확인 ({len(build_keys)} keys)")
    return ok


# ======================================================
# [v0415 Phase 3-D] KB 이름 테이블 정렬 검증
# ======================================================
def check_w10_kb_names_sorted(root):
    print(f"\n--- W10 KB Names (sorted) ---")
    p = root / "W10_Def_0415.h"
    if not p.exists():
        print(f"  [WARN] W10_Def_0415.h not found")
        return True

    src = p.read_text(encoding="utf-8")
    m = re.search(r'G_W10_KB_NAMES\[\]\s*=\s*\{(.*?)\};', src, re.DOTALL)
    if not m:
        print(f"  [WARN] G_W10_KB_NAMES not found")
        return True

    codes = [int(c, 0) for c in re.findall(r'\{(0x[0-9A-Fa-f]+|\d+)\s*,', m.group(1))]
    if codes == sorted(codes):
        print(f"  [OK] {len(codes)} codes sorted")
        return True

    print(f"  [FAIL] codes not sorted")
    for i in range(1, len(codes)):
        if codes[i-1] > codes[i]:
            print(f"    {hex(codes[i-1])} > {hex(codes[i])}")
    return False


# ======================================================
# [v0415 Phase 3-D] 에러 코드 테이블 정렬 검증
# ======================================================
def check_w10_err_map_sorted(root):
    print(f"\n--- W10 Err Map (sorted) ---")
    p = root / "W10_Def_0415.h"
    if not p.exists():
        print(f"  [WARN] W10_Def_0415.h not found")
        return True

    src = p.read_text(encoding="utf-8")
    m = re.search(r'G_W10_ERR_MAP\[\]\s*=\s*\{(.*?)\};', src, re.DOTALL)
    if not m:
        print(f"  [WARN] G_W10_ERR_MAP not found")
        return True

    codes = re.findall(r'\{"([^"]+)"', m.group(1))
    if codes == sorted(codes):
        print(f"  [OK] {len(codes)} codes sorted")
        return True

    print(f"  [FAIL] codes not sorted")
    for i in range(1, len(codes)):
        if codes[i-1] > codes[i]:
            print(f"    {codes[i-1]} > {codes[i]}")
    return False


def main():
    ok = True

    if not H_DEF.exists():
        print(f"[FAIL] file not found: {H_DEF}", file=sys.stderr)
        return 1
    h_lines = H_DEF.read_text(encoding="utf-8").splitlines()

    h_power  = set(extract_c_struct_fields(h_lines, "ST_C10_PowerConfig_t"))
    h_button = set(extract_c_struct_fields(h_lines, "ST_C10_ButtonConfig_t"))

    print(f"[check_schema] H: power={len(h_power)} button={len(h_button)}")
    print(f"  power:  {sorted(h_power)}")
    print(f"  button: {sorted(h_button)}")

    if not JS_CONFIG.exists():
        print(f"[FAIL] file not found: {JS_CONFIG}", file=sys.stderr)
        return 1
    cfg_text = JS_CONFIG.read_text(encoding="utf-8")

    js_power  = extract_js_field_refs(cfg_text, "power")
    js_button = extract_js_field_refs(cfg_text, "button")

    print(f"\n[check_schema] JS config refs: power={len(js_power)} button={len(js_button)}")

    if not JS_OFFLINE.exists():
        print(f"[FAIL] file not found: {JS_OFFLINE}", file=sys.stderr)
        return 1
    off_text = JS_OFFLINE.read_text(encoding="utf-8")

    off_power  = extract_offline_default_keys(off_text, "power")
    off_button = extract_offline_default_keys(off_text, "button")

    print(f"[check_schema] Offline defaults: power={len(off_power)} button={len(off_button)}")

    print(f"\n--- Power ---")
    _diff_and_report("H↔JS_config", h_power, js_power)
    if h_power != js_power:
        ok = False

    print(f"\n--- Button ---")
    _diff_and_report("H↔JS_config", h_button, js_button)
    if h_button != js_button:
        ok = False

    print(f"\n--- Power (H ↔ Offline) ---")
    _diff_and_report("H↔Offline", h_power, off_power)
    if h_power != off_power:
        ok = False

    print(f"\n--- Button (H ↔ Offline) ---")
    _diff_and_report("H↔Offline", h_button, off_button)
    if h_button != off_button:
        ok = False

    if not check_wifi_mapping(h_lines, cfg_text, off_text):
        ok = False

    if not check_gesture_removed(h_lines, cfg_text, off_text):
        ok = False

    if not check_e10_nested_mapping(h_lines, cfg_text, off_text):
        ok = False

    HTML_INDEX = ROOT / "data_v0415_www" / "index_0415.html"
    if HTML_INDEX.exists():
        html_text = HTML_INDEX.read_text(encoding="utf-8")
        if not check_wave_cd_dom_ids(html_text, cfg_text):
            ok = False
    else:
        print(f"\n[WARN] index_0415.html not found: {HTML_INDEX}")

    if not check_bundle_completeness(ROOT):
        ok = False

    if not check_b20_host_constants(ROOT):
        ok = False

    # [Phase 3-D] 신규 검증
    if not check_e10_json_parity(ROOT):
        ok = False
    if not check_w10_kb_names_sorted(ROOT):
        ok = False
    if not check_w10_err_map_sorted(ROOT):
        ok = False

    if ok:
        print("\n[OK] schema field names match (H ↔ JS ↔ Offline) + Wi-Fi + Gesture removal + E10 nested + Wave C/D DOM ids + E10 JSON parity + W10 tables sorted")
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
    print(f"\n--- Wi-Fi ---")

    h_wifi = set(extract_c_struct_fields(h_lines, "ST_C10_WiFiConfig_t"))
    expected = set(WIFI_FIELD_MAP.keys())

    if h_wifi != expected:
        print(f"  [FAIL] H ST_C10_WiFiConfig_t 필드 불일치")
        print(f"    expected: {sorted(expected)}")
        print(f"    found:    {sorted(h_wifi)}")
        return False
    print(f"  [OK] H fields match ({len(h_wifi)})")

    missing_dom = [d for d in WIFI_DOM_MAP.keys() if d not in js_config_text]
    if missing_dom:
        print(f"  [FAIL] JS config에서 Wi-Fi DOM id 누락:")
        for d in missing_dom:
            print(f"    - {d}")
        return False
    print(f"  [OK] JS DOM ids present ({len(WIFI_DOM_MAP)})")

    has_wifi_block = ("wifi:" in off_text) or ('"wifi"' in off_text)
    if not has_wifi_block:
        print(f"  [FAIL] Offline 기본 프로파일에 'wifi' 블록 없음")
        return False
    print(f"  [OK] Offline wifi block present")

    return True


def check_wave_cd_dom_ids(html_text, js_config_text):
    print(f"\n--- Wave C+D DOM IDs ---")

    dom_ids = [
        "precMode", "precDeadzone", "precGain", "precAccel", "precMaxStep",
        "precSmooth", "precEntryMs", "precExitMs", "precEntryStillDeg",
        "precExitMoveDeg", "precProfile",
        "gyroStillTh", "gyroStillWinMs", "gyroAlpha",
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
    print(f"\n--- Gesture Removal (Schema 412) ---")

    h_text_code = _strip_comments_multi("\n".join(h_lines))

    ok = True

    h_hits = [f for f in GESTURE_REMOVED_H_FIELDS if f in h_text_code]
    if h_hits:
        print(f"  [FAIL] H에 gesture_* 필드 잔존 (스키마 412 위반):")
        for f in h_hits:
            print(f"    - {f}")
        ok = False
    else:
        print(f"  [OK] H gesture_* 필드 없음")

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

    if "flick" not in js_config_text:
        print(f"  [FAIL] JS config에 flick 참조 없음 (SSOT 위반)")
        ok = False
    else:
        print(f"  [OK] JS flick 참조 존재")

    return ok


def check_e10_nested_mapping(h_lines, js_config_text, off_text):
    print(f"\n--- E10 Nested Mapping (Wave C+D) ---")
    ok = True

    for struct_name, fields in E10_NESTED_MAP.items():
        missing_js = []
        for field in fields.keys():
            patterns = [
                f".{struct_name}.{field}",
                f"{struct_name}[\"{field}\"]",
            ]
            if not any(p in js_config_text for p in patterns):
                missing_js.append(field)

        off_has_block = (f"{struct_name}:" in off_text)
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
    