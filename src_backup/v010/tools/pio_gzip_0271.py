# =======================================================
# File: tools/pio_sync_www_0274.py
# 목적:
#  - src/v010/data_v010_www 를 "배포 폴더(data/www)"로 동기화
#  - HTML/CSS/JS: data/www에 복사 -> gzip(.gz) 생성 -> 원본 삭제 (gz만 남김)
#  - SVG/PNG/WEBP: data/www에 원본만 복사 (gzip 제외, 원본 유지)
#
# 설계 의도:
#  - LittleFS 용량/전송 최적화:
#    * 텍스트 자산은 gzip만 남겨 용량 절감 + 네트워크 전송 최소화
#    * 이미지류는 gzip 이득이 거의 없거나(이미 압축됨) 브라우저 호환/성능상 의미가 적으므로 제외
#
# 주의:
#  - PlatformIO의 data_dir (= env["PROJECT_DATA_DIR"]) 아래에
#    json/ , www/ 폴더가 존재한다고 가정하지만, 없으면 자동 생성함
# =======================================================
Import("env")
import os
import gzip
import shutil

# -------------------------------
# 경로 설정
# -------------------------------
# 원본(개발자 자산 폴더)
SRC_WWW_DIR = os.path.join(env["PROJECT_DIR"], env["PROJECT_SRC_DIR"], "v010", "data_v010_www")

# 배포(PlatformIO data_dir)
DATA_DIR = env["PROJECT_DATA_DIR"]             # 예: <project>/data
DST_WWW_DIR = os.path.join(DATA_DIR, "www")    # 최종 배포 폴더

# -------------------------------
# 확장자 정책
# -------------------------------
# 텍스트: gzip 생성 후 원본 삭제 (gz만 유지)
GZIP_ONLY_EXTS = (".html", ".css", ".js")

# 이미지: 원본만 복사 (gzip 제외)
COPY_ONLY_EXTS = (".svg", ".png", ".webp")

# (선택) 기타 타입도 복사만 하고 싶으면 여기에 추가 가능
# COPY_ONLY_EXTS += (".ico", ".woff2", ".json")  # 같은 방식으로 확장 가능


def ensure_dir(path: str):
    """디렉토리가 없으면 생성."""
    if not os.path.isdir(path):
        os.makedirs(path, exist_ok=True)


def is_target_file(filename: str) -> bool:
    """처리 대상 파일인지 판단."""
    fn = filename.lower()
    if fn.endswith(".gz"):
        return False
    return fn.endswith(GZIP_ONLY_EXTS) or fn.endswith(COPY_ONLY_EXTS)


def gzip_file(src_path: str, gz_path: str):
    """
    gzip 생성.
    - mtime 옵션 미지원 환경을 고려하여 gzip.open(mtime=...) 사용 금지
    - 동일 파일이 있으면 overwrite
    """
    with open(src_path, "rb") as f_in:
        with gzip.open(gz_path, "wb", compresslevel=9) as f_out:
            shutil.copyfileobj(f_in, f_out)

    orig_size = os.path.getsize(src_path)
    gz_size = os.path.getsize(gz_path)
    ratio = (1 - (gz_size / orig_size)) * 100 if orig_size > 0 else 0
    print(f"  [GZIP] {os.path.basename(src_path)}: {orig_size} -> {gz_size} bytes ({ratio:.1f}% saved)")


def sync_one_file(src_path: str):
    """
    원본 파일 1개를 data/www로 동기화.
    - 상대경로(relpath) 그대로 유지하여 하위 폴더 구조 동일하게 유지
    - 확장자별 정책 적용
    """
    rel = os.path.relpath(src_path, SRC_WWW_DIR)   # 예: "index_0274.html" 또는 "assets/img.webp"
    dst_path = os.path.join(DST_WWW_DIR, rel)      # 예: "<project>/data/www/index_0274.html"

    ensure_dir(os.path.dirname(dst_path))

    # 1) 원본을 data/www에 복사(덮어쓰기)
    try:
        shutil.copy2(src_path, dst_path)
        print(f"  [COPY] {rel} -> data/www/{rel}")
    except Exception as e:
        print(f"  [COPY] FAIL: {rel} ({e})")
        return

    lower = rel.lower()

    # 2) 텍스트 자산은 gzip 생성 후 원본 삭제 (gz만 남김)
    if lower.endswith(GZIP_ONLY_EXTS):
        gz_path = dst_path + ".gz"
        try:
            gzip_file(dst_path, gz_path)
        except Exception as e:
            print(f"  [GZIP] FAIL: {rel} ({e})")
            return

        # gzip 성공 후 원본 삭제
        try:
            os.remove(dst_path)
            print(f"  [CLEAN] removed plain: data/www/{rel}")
        except Exception as e:
            print(f"  [CLEAN] FAIL remove plain: {rel} ({e})")

        return

    # 3) 이미지 자산은 gzip 제외, 원본 유지
    if lower.endswith(COPY_ONLY_EXTS):
        # 아무것도 하지 않음(원본 유지)
        return


def sync_all():
    """
    src/v010/data_v010_www 전체를 data/www로 동기화.
    """
    ensure_dir(DST_WWW_DIR)

    if not os.path.isdir(SRC_WWW_DIR):
        print(f"  [SYNC] WARN: SRC_WWW_DIR not found: {SRC_WWW_DIR}")
        return

    for root, _, files in os.walk(SRC_WWW_DIR):
        for fn in files:
            if not is_target_file(fn):
                continue
            sync_one_file(os.path.join(root, fn))


def before_buildfs(source, target, env):
    """
    buildfs 전에 실행:
    - data/www 를 최종 배포 폴더로 만드는 작업
    """
    print("  [SYNC] src/v010/data_v010_www -> data/www (gzip-only for html/css/js)")
    sync_all()


env.AddPreAction("buildfs", before_buildfs)
