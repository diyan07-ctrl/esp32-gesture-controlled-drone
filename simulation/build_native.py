"""Build only hardware-independent firmware with mocked hardware into a native library."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build" / "sil"
LIB = BUILD / ("firmware.dll" if os.name == "nt" else "libfirmware.so")


def sources_hash():
    paths = sorted((ROOT / "firmware/common").rglob("*.h"))
    paths += sorted((ROOT / "tests/mocks").glob("*.h"))
    paths += [ROOT / "simulation/native/bridge.cpp"]
    h = hashlib.sha256()
    for path in paths:
        h.update(path.relative_to(ROOT).as_posix().encode())
        h.update(path.read_bytes())
    return h.hexdigest()


def build(zig=None):
    BUILD.mkdir(parents=True, exist_ok=True)
    zig = zig or os.environ.get("SIL_ZIG")
    bundled = ROOT / ".venv/Lib/site-packages/ziglang/zig.exe"
    if not zig and os.name == "nt" and bundled.exists():
        zig = str(bundled)
    compiler = [str(zig), "c++"] if zig else [shutil.which("g++") or "g++"]
    env = os.environ.copy()
    env.update(ZIG_GLOBAL_CACHE_DIR=str(ROOT / ".cache/zig-global"),
               ZIG_LOCAL_CACHE_DIR=str(ROOT / ".cache/zig-local"))
    cmd = compiler + ["-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic",
                      "-shared", "-I", str(ROOT / "firmware/common"),
                      "-I", str(ROOT / "tests/mocks"),
                      str(ROOT / "simulation/native/bridge.cpp"), "-o", str(LIB)]
    if os.name != "nt":
        cmd.insert(len(compiler), "-fPIC")
    subprocess.run(cmd, env=env, check=True, cwd=ROOT)
    (BUILD / "build.json").write_text(json.dumps({"sources_sha256": sources_hash(),
        "command": cmd, "python": sys.version}, indent=2), encoding="utf-8")
    print(f"Built hardware-isolated native firmware: {LIB}")
    return LIB


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--zig")
    build(parser.parse_args().zig)
