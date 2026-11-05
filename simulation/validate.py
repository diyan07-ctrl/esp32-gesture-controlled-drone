"""One command rebuilds native logic, executes regressions, and generates SIL evidence."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import unittest
from .build_native import ROOT, build


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--output",default="build/sil/report")
    p.add_argument("--config")
    p.add_argument("--scenario",action="append")
    p.add_argument("--baseline",help="Previous results.json for before/after comparison")
    p.add_argument("--zig")
    p.add_argument("--skip-host",action="store_true",help="CI already runs the unchanged host suite separately")
    p.add_argument("--firmware",action="store_true",help="Also build all five existing ESP32 targets")
    p.add_argument("--no-build",action="store_true",help="Use a fingerprint-verified existing library (dashboard subprocess)")
    args=p.parse_args()
    os.chdir(ROOT)
    if not args.no_build: build(args.zig)
    if not args.skip_host:
        cmd=[sys.executable,"tools/run_host_tests.py"]
        zig=args.zig or (str(ROOT/".venv/Lib/site-packages/ziglang/zig.exe") if os.name=="nt" else None)
        if zig:cmd += ["--zig",zig]
        subprocess.run(cmd,check=True)
    subprocess.run(["node","tools/check_repository.mjs"],check=True)
    suite=unittest.defaultTestLoader.loadTestsFromName("simulation.test_sil")
    tests=unittest.TextTestRunner(verbosity=2).run(suite)
    from .report import generate, dump
    summary=generate(args.output,args.config,args.scenario,args.baseline)
    test_summary={"run":tests.testsRun,"failures":len(tests.failures),"errors":len(tests.errors),
                  "skipped":len(tests.skipped)}
    dump(Path(args.output)/"automated-tests.json",test_summary)
    if args.firmware:
        env=os.environ.copy();env["PLATFORMIO_CORE_DIR"]=str(ROOT/".cache/platformio")
        subprocess.run([sys.executable,"-m","platformio","run","-e","glove","-e","drone",
                        "-e","motor_test","-e","glove_sensors","-e","drone_sensors"],env=env,check=True)
    return 0 if tests.wasSuccessful() and not summary["unexpected_failures"] else 1


if __name__=="__main__":sys.exit(main())
