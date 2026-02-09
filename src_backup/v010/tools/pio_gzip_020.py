# ======================================================
# File: tools/pio_gzip_020.py
# - Python 구버전 호환: gzip.open(mtime=...) 미지원 => 제거
# ======================================================
import os
import gzip

def gzip_file(path):
    gz_path = path + ".gz"

    # 이미 gz가 더 최신이면 스킵
    if os.path.exists(gz_path):
        if os.path.getmtime(gz_path) >= os.path.getmtime(path):
            return

    with open(path, "rb") as f_in:
        data = f_in.read()

    # NOTE: gzip header mtime 고정은 python 3.8+ 필요
    with gzip.open(gz_path, "wb", compresslevel=9) as f_out:
        f_out.write(data)

def before_build(source, target, env):
    data_dir = env.get("PROJECT_DATA_DIR")
    if not data_dir:
        return

    # www + json 대상
    for root, _, files in os.walk(data_dir):
        for fn in files:
            if fn.endswith(".gz"):
                continue
            if fn.endswith(".html") or fn.endswith(".css") or fn.endswith(".js") or fn.endswith(".json"):
                p = os.path.join(root, fn)
                gzip_file(p)

    print("[pio_gzip] gzip done")

# PlatformIO hook
Import("env")
env.AddPreAction("buildfs", before_build)
