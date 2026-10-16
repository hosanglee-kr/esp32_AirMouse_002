# =======================================================
# File: src/v0400/tools_v0400/pio_gzip_0400.py
# ---------------------------------------------------------------------
# AirMouse Elite S3 v0.400.x — LittleFS www staging + gzip 생성
#
# [방식 A] 소스/파생물 최소 분리
#   - data_v0400/json/**  : 소스(VCS)     — 본 스크립트가 손대지 않음
#   - data_v0400/www/**   : 파생물(.gitignore) — 매 buildfs마다 클린 후 재생성
#
# [설계 요약]
#   1) SRC(원본):  <PROJECT>/src/v0400/data_v0400_www/**
#   2) DST(빌드fs): <PROJECT>/src/v0400/data_v0400/www/** (= PROJECT_DATA_DIR/www)
#   3) 규칙
#      - html/css/js : DST에 .gz 생성(원본은 DST에 두지 않음 → LittleFS 절약)
#      - svg/png/webp/ico/txt/json : DST에 원본 복사( gzip 생성 X )
#      - 하위 폴더 구조 동일 유지, 동일 파일 존재 시 overwrite
#   4) buildfs 직전 DST/www 전체 삭제 → 구파일 잔존으로 인한 잘못된 서빙 방지
#
# [정책 메모]
#   - platformio.ini: data_dir = ./src/v0400/data_v0400
#   - extra_scripts : pre:src/v0400/tools_v0400/pio_gzip_0400.py
#   - json/config_0400.json, json/boot_state_0400.json, json/public/*.json 은
#     VCS 커밋 대상이며, 이 스크립트는 관여하지 않는다.
# =======================================================
Import("env")
import os
import gzip
import shutil
from SCons.Script import COMMAND_LINE_TARGETS

# -------------------------------------------------------
# [경로 정의]
# -------------------------------------------------------
# SRC: version-controlled 웹 소스
SRC_WWW_DIR = os.path.join(
    env["PROJECT_DIR"], "src", "v0400", "data_v0400_www"
)

# DST: buildfs 스테이징 (data_dir 기준 www/)
DST_WWW_DIR = os.path.join(env["PROJECT_DATA_DIR"], "www")

# -------------------------------------------------------
# [확장자 정책]
# -------------------------------------------------------
# gzip 생성 대상 (텍스트)
GZ_EXTS = {".html", ".css", ".js"}

# 복사만 대상 (바이너리/정적 + txt + json)
COPY_ONLY_EXTS = {".svg", ".png", ".webp", ".ico", ".txt", ".json"}

# 이름 기반 화이트리스트 (확장자 없는 케이스 대비)
COPY_ONLY_NAMES = {"robots.txt", "favicon.ico"}


# =======================================================
# [유틸]
# =======================================================
def ensure_dir(path):
    if path and not os.path.isdir(path):
        os.makedirs(path, exist_ok=True)


def relpath_safe(path, base):
    """경로를 base 기준 상대경로(슬래시 통일)로 반환."""
    return os.path.relpath(path, base).replace("\\", "/")


def gzip_file(src_path, dst_gz_path):
    """src_path를 gzip 압축하여 dst_gz_path에 저장. mtime=0으로 결정론적 빌드 지원."""
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
    """src_path를 dst_path로 복사(메타데이터 보존)."""
    ensure_dir(os.path.dirname(dst_path))
    shutil.copy2(src_path, dst_path)
    print(f"  [COPY] {os.path.basename(src_path)} -> "
          f"{relpath_safe(dst_path, env['PROJECT_DATA_DIR'])}")


# =======================================================
# [DST 클린]
# -------------------------------------------------------
# data_v0400/www/ 아래의 예전 파일(.gz 포함)이 다음 빌드에 그대로 포함되면
# "구버전 파일 서빙" 문제가 발생한다. buildfs 직전에 www 전체를 삭제하고,
# SRC 기준으로만 다시 구성한다.
# -------------------------------------------------------
# 주의: data_v0400/json/** 은 소스이므로 절대 손대지 않는다.
# =======================================================
def clean_dst_www():
    if os.path.isdir(DST_WWW_DIR):
        shutil.rmtree(DST_WWW_DIR, ignore_errors=True)
    ensure_dir(DST_WWW_DIR)


# =======================================================
# [SYNC] data_v0400_www → data_v0400/www
# =======================================================
def sync_www():
    if not os.path.isdir(SRC_WWW_DIR):
        print(f"[WWW] SRC missing: {SRC_WWW_DIR}")
        return

    ensure_dir(DST_WWW_DIR)

    for root, _dirs, files in os.walk(SRC_WWW_DIR):
        for fn in files:
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
# [PRE-ACTION] buildfs / uploadfs 직전 실행 보장
# =======================================================
_has_run_sync = False

def run_www_sync(source=None, target=None, env=None):
    global _has_run_sync
    if _has_run_sync:
        return
    _has_run_sync = True

    print("[WWW] Clean start")
    clean_dst_www()
    print("[WWW] Clean done")

    print("[WWW] Sync+Gzip start")
    sync_www()
    print("[WWW] Sync+Gzip done")


# 1) CLI 또는 IDE에서 buildfs / uploadfs 타깃이 요청된 경우, 빌드 시작 전 즉시 동기화 실행
if any(t in COMMAND_LINE_TARGETS for t in ["buildfs", "uploadfs"]):
    run_www_sync()

# 2) SCons의 LittleFS 바이너리 빌드 타깃 노드에 PreAction 등록 (mklittlefs 실행 직전 보장)
fs_bin_name = env.subst("${ESP32_FS_IMAGE_NAME}.bin") if "${ESP32_FS_IMAGE_NAME}" in env else "littlefs.bin"
fs_bin_path = os.path.join(env.subst("$BUILD_DIR"), fs_bin_name)
env.AddPreAction(fs_bin_path, run_www_sync)

# 3) 하위 호환성을 위해 buildfs 타깃에도 등록
env.AddPreAction("buildfs", run_www_sync)