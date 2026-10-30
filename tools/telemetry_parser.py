"""Convert captured D CSV records to named columns; diagnostics only."""
import argparse
import csv
import sys

FIELDS = "ms state faults battery_V age_ms exec_us misses rejected lost ax_g ay_g az_g gx_dps gy_dps gz_dps roll_deg pitch_deg yaw_deg throttle roll_cmd pitch_cmd yaw_cmd pid_r pid_p pid_y m1 m2 m3 m4".split()


def records(lines):
    for row in csv.reader(lines):
        if not row or row[0] != "D":
            continue
        if len(row) != len(FIELDS) + 1:
            raise ValueError(f"expected {len(FIELDS)} D values, got {len(row)-1}")
        # Reject malformed numeric data while retaining decimal precision in output.
        for value in row[1:]:
            float(value)
        yield row[1:]


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("capture")
    args = p.parse_args()
    w = csv.writer(sys.stdout)
    w.writerow(FIELDS)
    with open(args.capture, encoding="utf-8") as f:
        w.writerows(records(f))
