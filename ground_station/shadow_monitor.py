"""Console/JSONL receiver for BNO055 PID shadow telemetry; never sends commands."""
import argparse
import json
import socket
import time
from pathlib import Path
from .shadow_codec import decode


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--port", type=int, default=14550)
    p.add_argument("--duration", type=float, default=30)
    p.add_argument("--jsonl", type=Path)
    args = p.parse_args()
    if args.jsonl:
        args.jsonl.parent.mkdir(parents=True, exist_ok=True)
    valid = invalid = missing = 0
    previous = None
    last = None
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(("0.0.0.0", args.port))
        sock.settimeout(.2)
        output = args.jsonl.open("w", encoding="utf-8") if args.jsonl else None
        try:
            started = time.monotonic()
            while time.monotonic() - started < args.duration:
                try:
                    data, _ = sock.recvfrom(512)
                except socket.timeout:
                    if last is not None and time.monotonic() - last > 1:
                        print("STALE shadow telemetry", flush=True)
                        last = None
                    continue
                try:
                    row = decode(data)
                except ValueError as error:
                    invalid += 1
                    print(f"INVALID {error}", flush=True)
                    continue
                if previous is not None and row["sequence"] > previous:
                    missing += row["sequence"] - previous - 1
                previous = row["sequence"]
                last = time.monotonic()
                valid += 1
                row["receive_monotonic_s"] = last
                if output:
                    output.write(json.dumps(row) + "\n")
                if valid % 10 == 1:
                    r, q = row["axes"]["roll"], row["axes"]["pitch"]
                    print(f"{row['mode']} valid={row['sensor_valid']} "
                          f"roll={r['measured']:.2f}/{r['setpoint']:.2f} "
                          f"pitch={q['measured']:.2f}/{q['setpoint']:.2f} "
                          f"PID={r['p']:.5f},{r['i']:.5f},{r['d']:.5f} "
                          f"Mcalc={tuple(round(x,3) for x in row['calculated_motor'])} "
                          f"ESC=DISABLED", flush=True)
        finally:
            if output:
                output.close()
    print(f"SUMMARY valid={valid} invalid={invalid} missing={missing}")
    return 0 if valid else 2


if __name__ == "__main__":
    raise SystemExit(main())
