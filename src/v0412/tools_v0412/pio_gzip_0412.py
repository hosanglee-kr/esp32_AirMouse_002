# =======================================================
# File: src/v0412/tools_v0412/pio_gzip_0412.py
# ---------------------------------------------------------------------
# AirMouse Elite S3 v0.400.x — LittleFS www staging + gzip 생성
#                          + [Phase 5.3] 빌드 전 정합성 검증
#
# [방식 A] 소스/파생물 최소 분리
#   - data_v0412/json/**  : 소스(VCS)     — 본 스크립트가 손대지 않음
#   - data_v0412/www/**   : 파생물(.gitignore) — 매 buildfs마다 클린 후 재생성
#
# [설계 요약]
#   1) SRC(원본):  <PROJECT>/src/v0412/data_v0414_www/**
#   2) DST(빌드fs): <PROJECT>/src/v0412/data_v0412/www/** (= PROJECT_DATA_DIR/www)
#   3) 규칙
#      - html/css/js : DST에 .gz 생성(원본은 DST에 두지 않음 → LittleFS 절약)
#      - svg/png/webp/ico/txt/json : DST에 원본 복사( gzip 생성 X )
#      - 하위 폴더 구조 동일 유지, 동일 파일 존재 시 overwrite
#   4) buildfs 직전 DST/www 전체 삭제 → 구파일 잔존으로 인한 잘못된 서빙 방지
#
# [Phase 5.3 검증]
#   - 다음 타깃에서 검증 스크립트를 순차 실행:
#       · buildprog  (일반 펌웨어 빌드) — 검증만 (CI 실패 조기 감지)
#       · buildfs / uploadfs           — 검증 + clean + sync + gzip
#       1) check_i18n_0412.py    : KO/EN 키 1:1 정합성
#       2) check_schema_0412.py  : 백엔드 H ↔ 프론트 JS ↔ 오프라인 기본값 필드명 3자 비교
#   - 하나라도 실패(exit != 0) 시 SystemExit(1) → 빌드 중단
#   - 긴급 우회: 환경변수 G_SKIP_VERIFY=1
#
# [SCons 환경 주의]
#   - PlatformIO의 extra_scripts는 exec() 컨텍스트로 로드되므로 __file__ 미정의.
#   - 경로는 env["PROJECT_DIR"] 기준으로 구성한다.
#
# [정책 메모]
#   - platformio.ini: data_dir = ./src/v0412/data_v0412
#   - extra_scripts : pre:src/v0412/tools_v0412/pio_gzip_0412.py
#   - json/config_0412.json, json/boot_state_0412.json, json/public/*.json 은
#     VCS 커밋 대상이며, 이 스크립트는 관여하지 않는다.
# =======================================================
Import("env")
import os
import sys
import gzip
import shutil
import subprocess
from SCons.Script import COMMAND_LINE_TARGETS

# -------------------------------------------------------
# [경로 정의]
# -------------------------------------------------------
# SCons의 exec() 컨텍스트에서는 __file__이 정의되지 않으므로
# PROJECT_DIR 기반으로 경로를 조립한다.
_PROJECT_DIR = env["PROJECT_DIR"]

# SRC: version-controlled 웹 소스 (v0414)
SRC_WWW_DIR = os.path.join(_PROJECT_DIR, "src", "v0412", "data_v0414_www")

# DST: buildfs 스테이징 (data_dir 기준 www/)
DST_WWW_DIR = os.path.join(env["PROJECT_DATA_DIR"], "www")

# 검증 스크립트 위치 (본 파일과 동일 디렉토리)
THIS_DIR = os.path.join(_PROJECT_DIR, "src", "v0412", "tools_v0412")

# -------------------------------------------------------
# [확장자 및 제외 정책]
# -------------------------------------------------------
GZ_EXTS = {".html", ".css", ".js"}
COPY_ONLY_EXTS = {".svg", ".png", ".webp", ".ico", ".txt", ".json"}
COPY_ONLY_NAMES = {"robots.txt", "favicon.ico"}

EXCLUDE_NAMES = {
    "app_0412_0001.js", "app_0413_0001_1.js",
    "index_0412.html", "index_0413_1.html", "index_0413_2.html",
    "index_0413_standalone.html", "index_0413_standalone2.html", "index_0413_standalone3.html",
    "style_0412.css", "style_0413_1.css", "style_0413_2.css"
}


# =======================================================
# [Phase 5.3] 정합성 검증
# =======================================================
def _run_verify_script(script_name, description, timeout_sec=30):
    """단일 검증 스크립트 실행. 성공(exit 0) 시 True, 실패 시 False."""
    script_path = os.path.join(THIS_DIR, script_name)

    if not os.path.isfile(script_path):
        print(f"[verify] SKIP: {script_name} not found (dev-only check)")
        return True

    print(f"[verify] running {script_name}  ({description})", flush=True)
    try:
        result = subprocess.run(
            [sys.executable, script_path],
            capture_output=True,
            text=True,
            timeout=timeout_sec,
            cwd=_PROJECT_DIR,
        )
    except subprocess.TimeoutExpired:
        print(f"[verify] FAIL: {script_name} timed out ({timeout_sec}s)", flush=True)
        return False
    except Exception as exc:
        print(f"[verify] FAIL: {script_name} exec error: {exc}", flush=True)
        return False

    # stdout/stderr 출력 (들여쓰기)
    if result.stdout:
        for line in result.stdout.rstrip().splitlines():
            print(f"  {line}")
    if result.stderr:
        for line in result.stderr.rstrip().splitlines():
            print(f"  {line}", file=sys.stderr)

    if result.returncode != 0:
        print(f"[verify] ❌ {script_name} FAILED (exit={result.returncode})", flush=True)
        return False

    print(f"[verify] ✅ {script_name} passed", flush=True)
    return True


def run_verification():
    """전체 검증 시퀀스. 실패 시 SystemExit. G_SKIP_VERIFY=1로 우회 가능."""
    if os.environ.get("G_SKIP_VERIFY") == "1":
        print("[verify] SKIPPED by G_SKIP_VERIFY=1", flush=True)
        return

    print("[verify] ---- Start verification ----", flush=True)

    checks = [
        ("check_i18n_0412.py",   "KO/EN key parity"),
        ("check_schema_0412.py", "backend↔frontend field names"),
    ]

    all_ok = True
    for name, desc in checks:
        if not _run_verify_script(name, desc):
            all_ok = False
            # 첫 실패에서 즉시 중단하지 않고 모두 보고
            # (개발자가 한 번에 여러 이슈를 볼 수 있도록)

    print("[verify] ---- End verification ----", flush=True)

    if not all_ok:
        print("", flush=True)
        print("[verify] ❌ Verification FAILED. Build aborted.", flush=True)
        print("[verify]    Fix the reported issues, or set G_SKIP_VERIFY=1 to bypass (hotfix only).", flush=True)
        raise SystemExit(1)


# =======================================================
# [유틸]
# =======================================================
def ensure_dir(path):
    if path and not os.path.isdir(path):
        os.makedirs(path, exist_ok=True)


def relpath_safe(path, base):
    return os.path.relpath(path, base).replace("\\", "/")


def gzip_file(src_path, dst_gz_path):
    ensure_dir(os.path.dirname(dst_gz_path))
    with open(src_path, "rb") as f_in:
        with gzip.open(dst_gz_path, "wb", compresslevel=9) as f_out:
            shutil.copyfileobj(f_in, f_out)

    orig_size = os.path.getsize(src_path)
    gz_size   = os.path.getsize(dst_gz_path)
    ratio     = (1 - (gz_size / orig_size)) * 100 if orig_size > 0 else 0
    print(f"  [GZIP] {os.path.basename(src_path)}: "
          f"{orig_size} -> {gz_size} bytes ({ratio:.1f}% saved)")


def copy_file(src_path, dst_path):
    ensure_dir(os.path.dirname(dst_path))
    shutil.copy2(src_path, dst_path)
    print(f"  [COPY] {os.path.basename(src_path)} -> "
          f"{relpath_safe(dst_path, env['PROJECT_DATA_DIR'])}")


# =======================================================
# [DST 클린]
# =======================================================
def clean_dst_www():
    if os.path.isdir(DST_WWW_DIR):
        shutil.rmtree(DST_WWW_DIR, ignore_errors=True)
    ensure_dir(DST_WWW_DIR)


# =======================================================
# [SYNC] data_v0414_www → data_v0412/www
# =======================================================
def sync_www():
    if not os.path.isdir(SRC_WWW_DIR):
        print(f"[WWW] SRC missing: {SRC_WWW_DIR}")
        return

    ensure_dir(DST_WWW_DIR)

    for root, _dirs, files in os.walk(SRC_WWW_DIR):
        for fn in files:
            if fn in EXCLUDE_NAMES:
                continue

            src_path = os.path.join(root, fn)
            ext      = os.path.splitext(fn)[1].lower()

            rel      = relpath_safe(src_path, SRC_WWW_DIR)
            dst_path = os.path.join(DST_WWW_DIR, rel.replace("/", os.sep))

            # 1) gzip 대상 (html/css/js) — 원본은 DST에 두지 않음
            if ext in GZ_EXTS:
                gzip_file(src_path, dst_path + ".gz")
                continue

            # 2) 복사만 대상 (이미지/txt/json 등)
            if ext in COPY_ONLY_EXTS or fn.lower() in COPY_ONLY_NAMES:
                copy_file(src_path, dst_path)
                continue

            # 3) 기타 파일은 무시 (원치 않는 파일 유입 방지)
            # print(f"  [SKIP] {rel}")


# =======================================================
# [PRE-ACTION] buildprog / buildfs / uploadfs 훅
# -------------------------------------------------------
# 시퀀스 (프로세스 내 1회 보장):
#   1) 검증 (check_i18n / check_schema) — 실패 시 즉시 중단
#   2) DST 클린 (buildfs 계열만)
#   3) SRC → DST 동기화 + gzip (buildfs 계열만)
# =======================================================
_has_run_verify = False   # [개선 2] 검증 1회 보장 (buildprog ↔ buildfs 공유)
_has_run_sync   = False   # [개선 2] clean+sync 1회 보장 (buildfs 계열)


def _ensure_verify_done():
    """검증이 프로세스 내에서 1회만 실행되도록 보장."""
    global _has_run_verify
    if _has_run_verify:
        return
    _has_run_verify = True
    run_verification()


def run_verify_only(source=None, target=None, env=None):
    """[개선 2] buildprog pre-action: 검증만 실행 (gzip 동기화 없음).

    목적: 일반 빌드(`pio run -e esp32-s3-zero`)에서도 i18n/schema 검증을
          실행하여 CI에서 실패를 조기에 감지 (firmware 빌드 시간 낭비 방지).
    """
    _ensure_verify_done()


def run_www_sync(source=None, target=None, env=None):
    """buildfs/uploadfs pre-action: 검증 + clean + sync + gzip.

    검증은 `_ensure_verify_done()`으로 1회만 실행됨.
    buildfs 계열에서는 clean + sync + gzip 을 수행.
    """
    global _has_run_sync
    if _has_run_sync:
        return
    _has_run_sync = True

    # ---- 검증 (buildprog에서 이미 했으면 skip) ----
    _ensure_verify_done()

    # ---- DST 클린 ----
    print("[WWW] Clean start")
    clean_dst_www()
    print("[WWW] Clean done")

    # ---- Sync + Gzip ----
    print("[WWW] Sync+Gzip start")
    sync_www()
    print("[WWW] Sync+Gzip done")


# =======================================================
# [HOOK 등록]
# =======================================================

# 1) CLI/IDE에서 buildfs/uploadfs 타깃 요청 시 즉시 실행 (early sync)
if any(t in COMMAND_LINE_TARGETS for t in ["buildfs", "uploadfs"]):
    run_www_sync()

# 2) LittleFS 바이너리 빌드 노드 pre-action (mklittlefs 직전 보장)
#    [개선 1] fs_bin_name 조건 안전화:
#      - 기존: `"${ESP32_FS_IMAGE_NAME}" in env` (항상 False → fallback 고정)
#      - 변경: env.subst() 결과가 리터럴 "${...}"이면 미정의로 간주 → fallback
_fs_name_raw = env.subst("${ESP32_FS_IMAGE_NAME}").strip()
if _fs_name_raw and not _fs_name_raw.startswith("${"):
    fs_bin_name = _fs_name_raw + ".bin"
else:
    fs_bin_name = "littlefs.bin"

fs_bin_path = os.path.join(env.subst("$BUILD_DIR"), fs_bin_name)
env.AddPreAction(fs_bin_path, run_www_sync)

# 3) buildfs 타깃 pre-action (하위 호환성)
env.AddPreAction("buildfs", run_www_sync)

# 4) [개선 2] buildprog 타깃 pre-action → 일반 빌드에서도 검증 실행
#    - `pio run -e esp32-s3-zero` 시점에 i18n/schema 검증 → CI 조기 실패
#    - `_ensure_verify_done()`이 buildfs와 공유되어 중복 실행 방지
env.AddPreAction("buildprog", run_verify_only)
