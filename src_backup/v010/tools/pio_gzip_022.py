# =======================================================
# File: tools/pio_gzip_022.py
# - gzip.open mtime 미지원 환경 대응
# =======================================================
Import("env")
import os, gzip

DATA_DIR = os.path.join(env["PROJECT_DIR"], env["PROJECT_SRC_DIR"], "v022", "data_v022")

def gzip_file(path):
    gz_path = path + ".gz"
    with open(path, "rb") as f_in:
        # mtime arg 제거(호환)
        with gzip.open(gz_path, "wb", compresslevel=9) as f_out:
            f_out.write(f_in.read())

def before_build(source, target, env):
    # www only
    www_dir = os.path.join(env["PROJECT_DATA_DIR"])
    # PlatformIO는 data_dir를 env["PROJECT_DATA_DIR"]로 준다
    # 여기서는 실제 파일 경로를 전부 스캔
    for root, _, files in os.walk(www_dir):
        for fn in files:
            if fn.endswith(".gz"): 
                continue
            if fn.endswith(".html") or fn.endswith(".css") or fn.endswith(".js"):
                p = os.path.join(root, fn)
                gzip_file(p)

env.AddPreAction("buildfs", before_build)
