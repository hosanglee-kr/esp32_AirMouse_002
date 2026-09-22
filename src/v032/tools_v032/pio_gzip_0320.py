# =======================================================
# File: src/v032/tools_v032/pio_gzip_0320.py
# - AirMouse Elite S3 v0.32.x — LittleFS staging + gzip 생성
# - (반영) DST(data_v032/) 클린 후 재생성
# - (반영) .txt / robots.txt 복사 허용
# - (반영) gzip.open mtime 인자 미사용(호환성)
# - (반영) 소스(VCS) / 산출물(.gitignore) 완전 분리
# =======================================================
Import("env")
import os
import gzip
import shutil

# -------------------------------------------------------
# [설계 요약]
#  SRC (VCS, 수정 대상)                      DST (buildfs, 파생물)
#  src/v032/data_v032_www/            →  data_v032/www/           (gz + copy)
#  src/v032/data_v032_json_public/    →  data_v032/json/public/   (copy only)
#  src/v032/data_v032_json_boot/      →  data_v032/json/          (copy only)
#
#  - data_dir = ./src/v032/data_v032  (platformio.ini)
#  - data_v032/ 전체는 .gitignore 대상
# -------------------------------------------------------

# --- SRC (version-controlled) ---
SRC_WWW_DIR    = os.path.join(env["PROJECT_DIR"], env["PROJECT_SRC_DIR"], "v032", "data_v032_www")
SRC_JSONPUB_DIR= os.path.join(env["PROJECT_DIR"], env["PROJECT_SRC_DIR"], "v032", "data_v032_json_public")
SRC_JSONBOOT_DIR= os.path.join(env["PROJECT_DIR"], env["PROJECT_SRC_DIR"], "v032", "data_v032_json_boot")

# --- DST (buildfs staging, gitignored) ---
DST_ROOT_DIR   = env["PROJECT_DATA_DIR"]              # .../src/v032/data_v032
DST_WWW_DIR    = os.path.join(DST_ROOT_DIR, "www")
DST_JSONPUB_DIR= os.path.join(DST_ROOT_DIR, "json", "public")
DST_JSONBOOT_DIR= os.path.join(DST_ROOT_DIR, "json")

# --- 정책 ---
GZ_EXTS        = {".html", ".css", ".js"}
COPY_ONLY_EXTS = {".svg", ".png", ".webp", ".ico", ".txt", ".json"}
COPY_ONLY_NAMES= {"robots.txt", "favicon.ico"}


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

    orig = os.path.getsize(src_path)
    gz   = os.path.getsize(dst_gz_path)
    ratio = (1 - (gz / orig)) * 100 if orig > 0 else 0
    print(f"  [GZIP] {os.path.basename(src_path)}: {orig} -> {gz} bytes ({ratio:.1f}% saved)")


def copy_file(src_path, dst_path):
    ensure_dir(os.path.dirname(dst_path))
    shutil.copy2(src_path, dst_path)
    print(f"  [COPY] {os.path.basename(src_path)} -> {relpath_safe(dst_path, DST_ROOT_DIR)}")


def clean_dst_all():
    # data_v032/ 전체를 삭제 후 재생성 — 구파일 잔존으로 인한 잘못된 서빙 방지
    if os.path.isdir(DST_ROOT_DIR):
        shutil.rmtree(DST_ROOT_DIR, ignore_errors=True)
    ensure_dir(DST_ROOT_DIR)


def sync_www():
    if not os.path.isdir(SRC_WWW_DIR):
        print(f"[WWW] SRC missing: {SRC_WWW_DIR}")
        return
    ensure_dir(DST_WWW_DIR)

    for root, _, files in os.walk(SRC_WWW_DIR):
        for fn in files:
            src_path = os.path.join(root, fn)
            ext = os.path.splitext(fn)[1].lower()
            rel = relpath_safe(src_path, SRC_WWW_DIR)
            dst_path = os.path.join(DST_WWW_DIR, rel.replace("/", os.sep))

            if ext in GZ_EXTS:
                gzip_file(src_path, dst_path + ".gz")
                # 원본은 DST에 두지 않음(LittleFS 절약)
                continue

            if ext in COPY_ONLY_EXTS or fn.lower() in COPY_ONLY_NAMES:
                copy_file(src_path, dst_path)
                continue

            # print(f"  [SKIP] {rel}")


def sync_json_public():
    if not os.path.isdir(SRC_JSONPUB_DIR):
        print(f"[JSONPUB] SRC missing: {SRC_JSONPUB_DIR}")
        return
    ensure_dir(DST_JSONPUB_DIR)

    for root, _, files in os.walk(SRC_JSONPUB_DIR):
        for fn in files:
            if os.path.splitext(fn)[1].lower() != ".json":
                continue
            src_path = os.path.join(root, fn)
            rel = relpath_safe(src_path, SRC_JSONPUB_DIR)
            dst_path = os.path.join(DST_JSONPUB_DIR, rel.replace("/", os.sep))
            copy_file(src_path, dst_path)


def sync_json_boot():
    # config / boot_state 초기값 → DST/json/ 루트
    if not os.path.isdir(SRC_JSONBOOT_DIR):
        print(f"[JSONBOOT] SRC missing (optional): {SRC_JSONBOOT_DIR}")
        return
    ensure_dir(DST_JSONBOOT_DIR)

    for root, _, files in os.walk(SRC_JSONBOOT_DIR):
        for fn in files:
            if os.path.splitext(fn)[1].lower() != ".json":
                continue
            src_path = os.path.join(root, fn)
            rel = relpath_safe(src_path, SRC_JSONBOOT_DIR)
            dst_path = os.path.join(DST_JSONBOOT_DIR, rel.replace("/", os.sep))
            copy_file(src_path, dst_path)


def before_buildfs(source, target, env):
    print("[STAGE] Clean data_v032/ start")
    clean_dst_all()
    print("[STAGE] Clean done")

    print("[STAGE] WWW sync+gzip start")
    sync_www()
    print("[STAGE] WWW done")

    print("[STAGE] JSON public sync start")
    sync_json_public()
    print("[STAGE] JSON public done")

    print("[STAGE] JSON boot (config/boot_state) sync start")
    sync_json_boot()
    print("[STAGE] JSON boot done")


env.AddPreAction("buildfs", before_buildfs)