#!/usr/bin/env python3
"""
check_schema_0412.py — 백엔드↔프론트 필드명 정합성 검증
- C10_Def_0412.h (ST_C10_PowerConfig_t, ST_C10_ButtonConfig_t) 필드 추출
- am_config_0414.js의 configToUi/uiToConfig에서 참조하는 power/button 필드 추출
- am_offline_0414.js 기본 프로파일의 power/button 키 추출
- 3자 비교 → 불일치 시 exit code 1

사용:
    python check_schema.py
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# -------------------------------------------------------
# 파일 경로
# -------------------------------------------------------
H_DEF         = ROOT / "C10_Def_0412.h"
JS_CONFIG     = ROOT / "data_v0414_www" / "lib" / "am_config_0414.js"
JS_OFFLINE    = ROOT / "data_v0414_www" / "lib" / "am_offline_0414.js"


def extract_c_struct_fields(lines, struct_name):
    """
    'struct ST_C10_PowerConfig_t { ... };' 블록에서 필드명 추출.
    배열 필드(idle_timeout_ms[3])도 이름만 취함.
    """
    start = -1
    for i, line in enumerate(lines):
        if re.search(rf'struct\s+{re.escape(struct_name)}\s*\{{', line):
            start = i
            break
    if start < 0:
        return []

    fields = []
    depth = 1
    for i in range(start + 1, len(lines)):
        line = lines[i]

        # 주석 제거
        code = _strip_comments(line)

        # 중괄호 카운트
        depth += code.count('{') - code.count('}')
        if depth <= 0:
            break

        # 필드 패턴: <type> <name>; 또는 <type> <name>[N];
        m = re.match(r'\s*(?:uint\d+_t|int\d+_t|bool|float|double|char)\s+(\w+)\s*(?:\[[^\]]*\])?\s*;', code)
        if m:
            name = m.group(1)
            if name == '_pad' or name.startswith('_'):
                continue
            fields.append(name)

    return fields


def extract_js_field_refs(text, section):
    """
    am_config_0414.js에서 특정 섹션(power/button) 필드 참조 추출.
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
    am_offline_0414.js의 G_OFFLINE_DEFAULT_PROFILE_0.e10.{power,button} 블록 필드명 추출.
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

    if ok:
        print("\n[OK] schema field names match (H ↔ JS ↔ Offline)")
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
        print(f"  [{label}] ✅ match")


if __name__ == "__main__":
    sys.exit(main())
    