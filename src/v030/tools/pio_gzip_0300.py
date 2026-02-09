# =======================================================
# File: tools/pio_gzip_0300.py
# - src/v010/data_v010_www → data/www 동기화 + gzip 생성
# - (반영) DST(www) 클린 후 재생성
# - (반영) .txt 확장자 복사 허용
# - gzip.open mtime 미지원 환경 대응(인자 미사용)
# =======================================================
Import("env")
import os
import gzip
import shutil

# -------------------------------------------------------
# [설계 요약]
# 1) SRC(원본):   <PROJECT_DIR>/src/v010/data_v010_www/...
# 2) DST(빌드fs): <PROJECT_DIR>/data/www/...
# 3) 규칙
#   - html/css/js: DST에 .gz 생성(원본은 DST에 없어도 됨)
#   - svg/png/webp/ico/txt: DST에 원본 복사( gzip 생성 X )
#   - 하위 폴더 구조 동일 유지
#   - 동일 파일 존재 시 overwrite
#   - (0279) 빌드 전 DST/www 전체 삭제 → 남은 구파일로 인한 잘못된 서빙 방지
# -------------------------------------------------------

SRC_WWW_DIR = os.path.join(env["PROJECT_DIR"], env["PROJECT_SRC_DIR"], "v010", "data_v010_www")
DST_WWW_DIR = os.path.join(env["PROJECT_DATA_DIR"], "www")

# gzip 대상(텍스트)
GZ_EXTS = {".html", ".css", ".js"}

# 복사만 대상(바이너리/정적 + txt 허용)
COPY_ONLY_EXTS = {".svg", ".png", ".webp", ".ico", ".txt"}
COPY_ONLY_NAMES = {"robots.txt", "favicon.ico"}  # 이름 기반 허용(확장자 없는 케이스 대비)

def ensure_dir(path):
    if not os.path.isdir(path):
        os.makedirs(path, exist_ok=True)

def relpath_safe(path, base):
    rp = os.path.relpath(path, base)
    rp = rp.replace("\\", "/")
    return rp

def gzip_file(src_path, dst_gz_path):
    ensure_dir(os.path.dirname(dst_gz_path))

    with open(src_path, "rb") as f_in:
        # mtime 인자 미사용(호환성)
        with gzip.open(dst_gz_path, "wb", compresslevel=9) as f_out:
            shutil.copyfileobj(f_in, f_out)

    orig_size = os.path.getsize(src_path)
    gz_size = os.path.getsize(dst_gz_path)
    ratio = (1 - (gz_size / orig_size)) * 100 if orig_size > 0 else 0
    print(f"  [GZIP] {os.path.basename(src_path)}: {orig_size} -> {gz_size} bytes ({ratio:.1f}% saved)")

def copy_file(src_path, dst_path):
    ensure_dir(os.path.dirname(dst_path))
    shutil.copy2(src_path, dst_path)
    print(f"  [COPY] {os.path.basename(src_path)} -> {relpath_safe(dst_path, env['PROJECT_DATA_DIR'])}")

def clean_dst_www():
    # ---------------------------------------------------
    # [DST 클린]
    # - data/www 아래에 남아있는 예전 파일(.gz 포함)이
    #   다음 빌드에서 그대로 포함되면 "구버전 파일 서빙" 문제가 발생함.
    # - 따라서 buildfs 직전에 www 전체를 삭제하고,
    #   SRC 기준으로만 다시 구성한다.
    # ---------------------------------------------------
    if os.path.isdir(DST_WWW_DIR):
        shutil.rmtree(DST_WWW_DIR, ignore_errors=True)
    ensure_dir(DST_WWW_DIR)

def sync_www():
    # SRC 없으면 조용히 종료
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

            # 1) gzip 대상 (html/css/js)
            if ext in GZ_EXTS:
                dst_gz = dst_path + ".gz"
                gzip_file(src_path, dst_gz)
                # 요구: data/www/ 폴더에 gz파일 원본은 없어도됨
                # -> 원본(dst_path)은 생성하지 않는다
                continue

            # 2) 복사만 대상(이미지/ico/txt/robots 등)
            if ext in COPY_ONLY_EXTS or fn.lower() in COPY_ONLY_NAMES:
                copy_file(src_path, dst_path)
                continue

            # 3) 기타 파일은 기본적으로 무시(원치 않는 파일 유입 방지)
            # 필요 시 여기에 허용 목록 추가
            # print(f"  [SKIP] {rel}")
            pass

def before_buildfs(source, target, env):
    print("[WWW] Clean start")
    clean_dst_www()
    print("[WWW] Clean done")

    print("[WWW] Sync+Gzip start")
    sync_www()
    print("[WWW] Sync+Gzip done")

# buildfs(=SPIFFS/LittleFS 이미지 생성) 직전에 실행
env.AddPreAction("buildfs", before_buildfs)
