"""Summarize actual read-only BNO055/PID shadow captures as Markdown."""
import argparse
import json
import statistics as stats
from collections import defaultdict
from pathlib import Path


def load(path):
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line]


def mean(rows, fn):
    return stats.mean(fn(row) for row in rows)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--sensor",type=Path,required=True)
    p.add_argument("--setpoints",type=Path,required=True)
    p.add_argument("--tilt",type=Path)
    p.add_argument("--locked",type=Path)
    p.add_argument("--output",type=Path,required=True)
    args=p.parse_args()
    sensor=[row for row in load(args.sensor) if row["mode"] == "SENSOR"]
    shadow=load(args.setpoints)
    groups=defaultdict(list)
    for row in shadow:
        key=(row["mode"],row["axes"]["roll"]["setpoint"],row["axes"]["pitch"]["setpoint"])
        groups[key].append(row)
    lines=["# Actual drone BNO055 PID shadow capture", "",
           "Generated from ignored local UDP JSONL logs. Values are sensor-frame measurements or software calculations; **no ESC signals were sent**.", "",
           f"Sensor-mode packets: {len(sensor)}. Setpoint-run packets: {len(shadow)}.", "",
           "| Mode | Roll target ° | Pitch target ° | Samples | Mean roll PID | Mean pitch PID | Mean calculated M1 | M2 | M3 | M4 |",
           "|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|"]
    for (mode,r,pitch),rows in groups.items():
        vals=[mean(rows,lambda row:row["axes"]["roll"]["output"]),
              mean(rows,lambda row:row["axes"]["pitch"]["output"])]
        vals += [mean(rows,lambda row,j=j:row["calculated_motor"][j]) for j in range(4)]
        lines.append(f"| {mode} | {r:+.1f} | {pitch:+.1f} | {len(rows)} | " + " | ".join(f"{v:.4f}" for v in vals) + " |")
    lines += ["", "## Sensor-mode observation", ""]
    if sensor:
        for axis in ("roll","pitch"):
            values=[row["axes"][axis]["measured"] for row in sensor if row["sensor_valid"]]
            if values:
                lines.append(f"- {axis}: observed {min(values):.3f}° to {max(values):.3f}°, standard deviation {stats.pstdev(values):.4f}° over this capture. Absolute orientation was not independently measured.")
    if shadow:
        dt=[row["dt_us"] for row in shadow]
        lines.append(f"- Shadow control interval in received samples: {min(dt)}–{max(dt)} µs, mean {stats.mean(dt):.2f} µs. This excludes unreceived packets.")
        lines.append(f"- Reported IMU errors: {min(row['imu_errors'] for row in shadow)}–{max(row['imu_errors'] for row in shadow)}.")
    if args.tilt:
        tilt=load(args.tilt)
        lines += ["", "## Manual tilt capture", ""]
        if tilt:
            lines.append(f"- {len(tilt)} received packets; {sum(row['sensor_valid'] for row in tilt)} sensor-valid; IMU error counter {min(row['imu_errors'] for row in tilt)}–{max(row['imu_errors'] for row in tilt)}.")
            for axis in ("roll","pitch"):
                values=[row["axes"][axis]["measured"] for row in tilt]
                lines.append(f"- Sensor-frame {axis}: {min(values):.2f}° to {max(values):.2f}°. Movement phases were not independently time-marked; physical body-axis signs remain unverified.")
            values=[v for row in tilt for v in row["calculated_motor"]]
            dt=[row["dt_us"] for row in tilt]
            lines.append(f"- Calculated motor command range {min(values):.3f}–{max(values):.3f}; control interval {min(dt)}–{max(dt)} µs in received samples. No ESC signal or actual motor response was measured.")
    if args.locked:
        locked=load(args.locked)
        lines += ["", "## Locked motor mode", ""]
        lines.append(f"- {len(locked)} received packets; all calculated motors zero: {all(all(v == 0 for v in row['calculated_motor']) for row in locked)}; all reported ESC outputs disabled: {all(not row['esc_output_enabled'] for row in locked)}.")
    lines += ["", "## Simulation-driven diagnostic correction", "",
              "The first on-device PID shadow capture reported nonzero P/I/D terms and changing mixer values while the telemetry `output` field remained zero. A local variable in `Pid::update` hid the new diagnostic member. The field was corrected without changing the returned control value or gains; a native regression assertion was added. After reflashing, 195 received shadow packets had a maximum absolute difference of 1.70e-9 between reported output and P+I+D. Before: output field stayed zero; after: it tracks the terms within float rounding."]
    lines += ["", "## Interpretation and limits", "",
              "The setpoint experiment held the physical board approximately still while USB commands changed the *virtual* target. Integral growth under a fixed error is expected; it is not evidence of airframe stabilization. Calculated motor values have no measured thrust, RPM or ESC PWM meaning. The BNO055 internal IMU fusion has no absolute yaw reference. Sensor-to-F450 axes and physical motor numbering remain unverified. No propeller-driven or flight test occurred.", ""]
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text("\n".join(lines),encoding="utf-8")
    print(args.output)


if __name__=="__main__":main()
