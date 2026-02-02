# File: tools/pio_gzip_019.py
# LittleFS 업로드 전에 www 정적 리소스 gzip 자동 생성 (Python 호환)

Import("env")
import os
import gzip

# buildfs 대상 디렉터리: project의 data_dir 사용 (예: src/v010/data_v010)
DATA_DIR = env.subst("$PROJECT_DIR")  # 프로젝트 루트
# platformio.ini에서 data_dir를 쓰는 경우 env["PROJECT_DATA_DIR"]가 잡힙니다.
# (환경마다 다를 수 있어 안전하게 fallback)
DATA_DIR = env.get("PROJECT_DATA_DIR", DATA_DIR)

TARGETS = [
    "www/index_019.html",
    "www/style_019.css",
    "www/app_019.js",
]

def gzip_file(abs_path):
    gz_path = abs_path + ".gz"

    if not os.path.exists(abs_path):
        print("[gzip] skip missing:", abs_path)
        return

    # 이미 .gz가 있고 원본보다 최신이면 스킵
    if os.path.exists(gz_path):
        if os.path.getmtime(gz_path) >= os.path.getmtime(abs_path):
            print("[gzip] up-to-date:", gz_path)
            return

    with open(abs_path, "rb") as f_in:
        raw = f_in.read()

    # ✅ gzip.open(mtime=...) 대신 gzip.GzipFile(fileobj=...) 사용 (구형 Python 호환)
    # mtime을 0으로 고정하고 싶으면 GzipFile에 mtime 인자가 있는 환경에서만 적용 가능.
    # 여기서는 호환성을 최우선으로 해서 mtime을 지정하지 않습니다.
    with open(gz_path, "wb") as f_raw_out:
        with gzip.GzipFile(filename=os.path.basename(abs_path), mode="wb", fileobj=f_raw_out, compresslevel=9) as f_out:
            f_out.write(raw)

    print("[gzip] wrote:", gz_path, "(", len(raw), "bytes )")

def before_build(source, target, env):
    # buildfs는 data_dir 기준으로 패킹됨
    # PlatformIO의 data_dir 경로 얻기
    data_dir = env.get("PROJECT_DATA_DIR", None)
    if data_dir is None:
        # data_dir를 못 찾으면 buildfs 소스 추론(현재 프로젝트 구조에 맞게)
        # 사용 로그에 나온 경로: src/v010/data_v010
        data_dir = os.path.join(env.subst("$PROJECT_DIR"), "src", "v010", "data_v010")

    for rel in TARGETS:
        abs_path = os.path.join(data_dir, rel.replace("/", os.sep))
        gzip_file(abs_path)

# buildfs 전에 실행
env.AddPreAction("buildfs", before_build)
