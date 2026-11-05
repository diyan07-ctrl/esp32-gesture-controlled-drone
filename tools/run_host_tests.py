"""Reproducible native C++ tests. Use --zig path/to/zig.exe on Windows."""
import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCES = ["tests/unit/core_tests.cpp", "tests/unit/sensor_gesture_tests.cpp", "tests/unit/imu_driver_tests.cpp",
           "tests/unit/calibration_storage_tests.cpp", "tests/unit/sensor_migration_tests.cpp",
           "tests/unit/orchestration_tests.cpp",
           "tests/unit/bench_telemetry_tests.cpp",
           "tests/unit/bno055_tests.cpp",
           "tests/unit/pid_shadow_tests.cpp",
           "tests/integration/pipeline_tests.cpp", "simulation/tests/roll_model.cpp",
           "simulation/tests/closed_loop.cpp"]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--zig")
    args = parser.parse_args()
    os.chdir(ROOT)
    build = ROOT / "build"
    build.mkdir(exist_ok=True)
    env = os.environ.copy()
    env["ZIG_GLOBAL_CACHE_DIR"] = str(ROOT / ".cache/zig-global")
    env["ZIG_LOCAL_CACHE_DIR"] = str(ROOT / ".cache/zig-local")
    compiler = [args.zig, "c++"] if args.zig else ["g++"]
    for source in SOURCES:
        target = build / (Path(source).stem + (".exe" if os.name == "nt" else ""))
        subprocess.run(compiler + ["-std=c++17", "-Wall", "-Wextra", "-Werror",
                       "-Wno-misleading-indentation", "-pedantic", "-I", "firmware/common", "-I", "tests/mocks",
                       source, "-o", str(target)], check=True, env=env)
        subprocess.run([str(target)], check=True)
    subprocess.run([sys.executable, "-m", "unittest", "discover", "-s", "tests/unit",
                    "-p", "test_tools.py", "-v"], check=True)
    subprocess.run([sys.executable, "-m", "unittest", "ground_station.test_udp", "-v"], check=True)
    subprocess.run([sys.executable, "-m", "unittest", "ground_station.test_shadow", "-v"], check=True)

if __name__ == "__main__":
    main()
