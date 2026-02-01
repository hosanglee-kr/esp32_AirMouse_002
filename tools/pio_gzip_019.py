
# File: tools/pio_gzip_019.py
# [P2] LittleFS 업로드 전에 www 정적 리소스 gzip 자동 생성
# 사용 방법:
#   platformio.ini에 아래 추가
#     extra_scripts = pre:tools/pio_gzip_019.py
#
# 대상:
#   data/www/index_019.html  -> index_019.html.gz
#   data/www/style_019.css   -> style_019.css.gz
#   data/www/app_019.js      -> app_019.js.gz

Import("env")
import os, gzip, shutil

TARGETS = [
    "data/www/index_019.html",
    "data/www/style_019.css",
    "data/www/app_019.js",
]

def gzip_file(path):
    gz_path = path + ".gz"
    if not os.path.exists(path):
        print("[gzip] skip missing:", path)
        return

    with open(path, "rb") as f_in:
        raw = f_in.read()

    with gzip.open(gz_path, "wb", compresslevel=9, mtime=0) as f_out:
        f_out.write(raw)

    print("[gzip] wrote:", gz_path, "(", len(raw), "bytes )")

def before_build(source, target, env):
    for p in TARGETS:
        gzip_file(p)

env.AddPreAction("buildfs", before_build)
