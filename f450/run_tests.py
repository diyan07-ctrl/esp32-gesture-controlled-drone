"""Run shared firmware mathematics/JSON and laptop tests without hardware."""
import argparse
import os
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument('--zig', help='Path to Zig executable; otherwise use c++ on PATH')
args = parser.parse_args()
include = root / '.pio/libdeps/f450/ArduinoJson/src'
if not include.is_dir():
    parser.error('Install pinned dependencies first: python -m platformio pkg install -d f450')
output = root.parent / 'build/f450-tests'
output.mkdir(parents=True, exist_ok=True)
cxx = [args.zig, 'c++'] if args.zig else ['c++']
for source in ['test_control.cpp', 'test_stage2.cpp']:
    exe = output / (Path(source).stem + ('.exe' if os.name == 'nt' else ''))
    subprocess.run(cxx + ['-std=c++17', '-I'+str(include), str(root/source), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
subprocess.run([sys.executable, '-m', 'unittest', 'discover', '-s', str(root), '-p', 'test_ground_station.py', '-v'], check=True)
subprocess.run([sys.executable, '-m', 'py_compile', str(root/'f450_ground_station.py')], check=True)
