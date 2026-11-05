"""Capture physical bench serial diagnostics with an optional explicit ESP32 reset."""
import argparse
import json
import time
from pathlib import Path
import serial


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port",required=True)
    parser.add_argument("--seconds",type=float,default=12)
    parser.add_argument("--reset",action="store_true")
    parser.add_argument("--output",type=Path,default=Path("build/hardware/drone-bench-serial.txt"))
    args=parser.parse_args()
    args.output.parent.mkdir(parents=True,exist_ok=True)
    lines=[]
    # Preconfigure modem-control lines *before* opening: opening with default
    # RTS asserted can pulse EN low on CP210x ESP32 DevKits.
    port=serial.Serial(port=None,baudrate=115200,timeout=.25)
    port.dtr=False
    port.rts=False
    port.port=args.port
    with port:
        port.dtr=False
        port.rts=False
        if args.reset:
            port.rts=True
            time.sleep(.15)
            port.reset_input_buffer()
            port.rts=False
        deadline=time.monotonic()+args.seconds
        while time.monotonic()<deadline:
            raw=port.readline()
            if raw:
                line=raw.decode("utf-8",errors="replace").strip()
                if line:lines.append(line)
    args.output.write_text("\n".join(lines)+"\n",encoding="utf-8")
    noteworthy=[x for x in lines if x.startswith(("DRONE_BENCH","I2C_","IMU_INIT", "BNO055_INIT", "IMU_CALIBRATION_FAILED", "ESPNOW", "WIFI,", "WIFI_CONFIG", "STATE,"))]
    print("\n".join(noteworthy[:35]))
    diag=[x for x in lines if x.startswith("DIAG,") and x.count(",")>=28]
    print(json.dumps({"port":args.port,"total_lines":len(lines),"diagnostic_lines":len(diag),
        "last_diagnostic":diag[-1] if diag else None,"log":str(args.output)},indent=2))
    return 0 if lines else 2


if __name__=="__main__":raise SystemExit(main())
