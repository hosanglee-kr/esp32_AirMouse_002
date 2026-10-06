#!/usr/bin/env python3
"""
check_i18n_0412.py — i18n KO/EN 키 정합성 검증
- am_i18n_0414.js에서 I18N_DICT.ko / I18N_DICT.en 블록의 키를 추출하여 비교
- 키 개수 불일치 / 누락 / 잉여 발견 시 exit code 1

사용:
    python check_i18n_0412.py                              # 기본 경로 사용
    python check_i18n_0412.py /path/to/am_i18n_0414.js    # 명시 경로
"""

import re
import sys
from pathlib import Path

# -------------------------------------------------------
# 기본 경로 (프로젝트 루트 기준)
# -------------------------------------------------------
# tools_v0412/ → 상위가 v0412/ 이므로 parent.parent
DEFAULT_PATH = Path(__file__).resolve().parent.parent / "data_v0414_www" / "lib" / "am_i18n_0414.js"

# 키 패턴: "quoted.key.name": "값"
KEY_PATTERN = re.compile(r'"([a-zA-Z0-9_.\-]+)"\s*:\s*"')

# 섹션 시작 패턴
KO_START = re.compile(r'^\s*ko\s*:\s*\{')
EN_START = re.compile(r'^\s*en\s*:\s*\{')


def extract_keys_from_block(lines, start_idx):
    """
    start_idx 위치의 '{' 부터 매칭되는 '}' 까지 라인을 순회하며
    KEY_PATTERN에 매칭되는 키를 수집.
    """
    keys = []
    depth = 0
    started = False

    for i in range(start_idx, len(lines)):
        line = lines[i]

        # depth 추적 ('{' 증가, '}' 감소) — 문자열 내부의 중괄호는 무시
        # 단순화: KEY_PATTERN과 별개로 depth만 셈
        for ch in _strip_strings(line):
            if ch == '{':
                depth += 1
                started = True
            elif ch == '}':
                depth -= 1

        # 키 추출 (블록 내부일 때만)
        if started and depth > 0:
            for m in KEY_PATTERN.finditer(line):
                keys.append(m.group(1))

        # 블록 종료
        if started and depth == 0:
            break

    return keys


def _strip_strings(line):
    """문자열 리터럴 내부의 중괄호를 무력화 (depth 계산용)."""
    out = []
    in_str = False
    esc = False
    quote = None
    for ch in line:
        if esc:
            out.append(' ')
            esc = False
            continue
        if ch == '\\':
            esc = True
            out.append(' ')
            continue
        if in_str:
            if ch == quote:
                in_str = False
                out.append(' ')
            else:
                out.append(' ')
            continue
        if ch in ('"', "'"):
            in_str = True
            quote = ch
            out.append(' ')
            continue
        out.append(ch)
    return ''.join(out)


def find_block_start(lines, pattern):
    """패턴과 일치하는 첫 라인의 인덱스 반환."""
    for i, line in enumerate(lines):
        if pattern.search(line):
            return i
    return -1


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_PATH

    if not path.exists():
        print(f"[FAIL] file not found: {path}", file=sys.stderr)
        return 1

    print(f"[check_i18n] parsing: {path}")
    text = path.read_text(encoding="utf-8")
    lines = text.splitlines()

    ko_start = find_block_start(lines, KO_START)
    en_start = find_block_start(lines, EN_START)

    if ko_start < 0:
        print("[FAIL] 'ko: {' block not found", file=sys.stderr)
        return 1
    if en_start < 0:
        print("[FAIL] 'en: {' block not found", file=sys.stderr)
        return 1

    ko_keys = extract_keys_from_block(lines, ko_start)
    en_keys = extract_keys_from_block(lines, en_start)

    ko_set = set(ko_keys)
    en_set = set(en_keys)

    print(f"[check_i18n] KO keys: {len(ko_keys)}  (unique: {len(ko_set)})")
    print(f"[check_i18n] EN keys: {len(en_keys)}  (unique: {len(en_set)})")

    # 중복 키 검출
    ko_dup = _find_duplicates(ko_keys)
    en_dup = _find_duplicates(en_keys)

    ok = True

    if ko_dup:
        print(f"\n[WARN] KO duplicated keys ({len(ko_dup)}):")
        for k in sorted(ko_dup):
            print(f"   - {k}")
        ok = False

    if en_dup:
        print(f"\n[WARN] EN duplicated keys ({len(en_dup)}):")
        for k in sorted(en_dup):
            print(f"   - {k}")
        ok = False

    # 누락/잉여 검출
    missing_in_en = ko_set - en_set
    missing_in_ko = en_set - ko_set

    if missing_in_en:
        print(f"\n[FAIL] keys in KO but missing in EN ({len(missing_in_en)}):")
        for k in sorted(missing_in_en):
            print(f"   - {k}")
        ok = False

    if missing_in_ko:
        print(f"\n[FAIL] keys in EN but missing in KO ({len(missing_in_ko)}):")
        for k in sorted(missing_in_ko):
            print(f"   - {k}")
        ok = False

    if ok:
        print(f"\n[OK] KO/EN key sets match: {len(ko_set)} keys each")
        return 0
    else:
        print("\n[FAIL] i18n key mismatch detected")
        return 1


def _find_duplicates(keys):
    seen = set()
    dup = set()
    for k in keys:
        if k in seen:
            dup.add(k)
        seen.add(k)
    return dup


if __name__ == "__main__":
    sys.exit(main())
    