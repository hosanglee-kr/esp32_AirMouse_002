# =======================================================
# File: tools/pio_gzip_0271.py
# =======================================================
# - src/v010/data_v010_www(원본) → PROJECT_DATA_DIR/www(빌드용 data) 로 복사
# - data/www/ 에는 원본 gz가 없어도 됨 (여기서 생성/overwrite)
# - 하위 폴더/파일 구조 동일 유지
# - HTML/CSS/JS : 원본 복사 + gzip(.gz) 생성(덮어쓰기)
# - SVG/PNG/WEBP/ICO 등 : 원본 복사만, gzip 제외
# - PlatformIO gzip.open mtime 미지원 환경 대응: mtime arg 미사용
#
# 주의:
# - env["PROJECT_DATA_DIR"] 아래에 json/ 와 www/ 가 존재하는 구조를 가정
# - 본 스크립트는 buildfs 직전에 동작하여 data 폴더를 최신화
# =======================================================
Import("env")
import os
import gzip
import shutil

# -------------------------------------------------------
# 입력(원본) 폴더:
#  - 요구: env["PROJECT_SRC_DIR"]/v010/data_v010_www 아래에 원본 존재
# -------------------------------------------------------
SRC_WWW_DIR = os.path.join(env["PROJECT_DIR"], env["PROJECT_SRC_DIR"], "v010", "data_v010_www")

# -------------------------------------------------------
# 출력(빌드 data) 폴더:
#  - PlatformIO data_dir = env["PROJECT_DATA_DIR"]
#  - 그 아래 www/로 복사
# -------------------------------------------------------
DATA_DIR = os.path.join(env["PROJECT_DATA_DIR"])
DST_WWW_DIR = os.path.join(DATA_DIR, "www")


def is_text_gzip_target(filename: str) -> bool:
    """
    gzip 대상:
      - html/css/js 만 gzip 생성
    """
    fn = filename.lower()
    return fn.endswith(".html") or fn.endswith(".css") or fn.endswith(".js")


def is_copy_only_asset(filename: str) -> bool:
    """
    복사만(비gzip) 대상 예:
      - svg/png/webp/ico/jpg/jpeg/gif 등
      - json도 요청사항에서 gzip 제외(공개 json은 no-store로 서빙)
    """
    # 여기서는 "gzip 대상이 아니면 모두 copy-only" 정책
    return not is_text_gzip_target(filename)


def ensure_dir(path: str):
    if not os.path.isdir(path):
        os.makedirs(path, exist_ok=True)


def gzip_file(src_path: str, dst_gz_path: str):
    """
    src_path를 gzip 압축하여 dst_gz_path로 저장(overwrite)
    - gzip.open(..., mtime=...) 사용하지 않음(호환)
    """
    ensure_dir(os.path.dirname(dst_gz_path))
    with open(src_path, "rb") as f_in:
        with gzip.open(dst_gz_path, "wb", compresslevel=9) as f_out:
            shutil.copyfileobj(f_in, f_out)

    orig_size = os.path.getsize(src_path)
    gz_size = os.path.getsize(dst_gz_path)
    ratio = (1 - (gz_size / orig_size)) * 100 if orig_size > 0 else 0
    print(f"  [GZIP] {os.path.basename(src_path)}: {orig_size} -> {gz_size} bytes ({ratio:.1f}% saved)")


def copy_file(src_path: str, dst_path: str):
    """
    파일 복사(overwrite)
    """
    ensure_dir(os.path.dirname(dst_path))
    shutil.copy2(src_path, dst_path)


def sync_www_tree():
    """
    SRC_WWW_DIR 트리를 순회하면서:
      - 상대 경로 유지하여 DST_WWW_DIR로 복사
      - html/css/js는 gzip까지 생성
      - 나머지는 복사만
    """
    if not os.path.isdir(SRC_WWW_DIR):
        print(f"[W10 WWW] source dir not found: {SRC_WWW_DIR}")
        return

    ensure_dir(DST_WWW_DIR)

    # 전체 스캔
    for root, _, files in os.walk(SRC_WWW_DIR):
        for fn in files:
            # 원본에 .gz가 있을 수 있으나, 요구: data/www에는 없어도 됨
            # - 원본 .gz는 복사하지 않음(혼선 방지)
            if fn.lower().endswith(".gz"):
                continue

            src_path = os.path.join(root, fn)

            # SRC_WWW_DIR 기준 상대 경로 계산
            rel_path = os.path.relpath(src_path, SRC_WWW_DIR)

            # 출력 경로: PROJECT_DATA_DIR/www/<rel_path>
            dst_path = os.path.join(DST_WWW_DIR, rel_path)

            # 1) 원본 복사(항상 overwrite)
            copy_file(src_path, dst_path)

            # 2) gzip 생성(텍스트만)
            if is_text_gzip_target(fn):
                dst_gz_path = dst_path + ".gz"
                gzip_file(src_path, dst_gz_path)
            else:
                # svg/png/webp/ico 등은 gzip 제외(복사만)
                pass


def before_buildfs(source, target, env):
    """
    buildfs 직전에 실행:
      - data/www를 src 원본과 동기화
      - 동일 파일 존재 시 overwrite
    """
    # data 폴더 존재 보장
    ensure_dir(DATA_DIR)

    # www 폴더 존재 보장
    ensure_dir(DST_WWW_DIR)

    print("[W10 WWW] syncing src/v010/data_v010_www -> data/www ...")
    sync_www_tree()
    print("[W10 WWW] sync done.")


# PlatformIO buildfs 전에 훅
env.AddPreAction("buildfs", before_buildfs)