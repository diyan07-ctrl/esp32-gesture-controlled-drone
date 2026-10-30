"""Scenario calculator; inputs are not measurements. No default C-rating."""
import argparse
import json
import math


def calculate(volts, mah, current, usable=0.8, c_rating=None):
    if not all(math.isfinite(v) and v > 0 for v in (volts, mah, current)):
        raise ValueError("voltage, capacity and current must be finite and positive")
    if not 0 < usable <= 1:
        raise ValueError("usable fraction must be in (0,1]")
    if c_rating is not None and (not math.isfinite(c_rating) or c_rating <= 0):
        raise ValueError("C-rating must be positive and finite")
    ah = mah / 1000
    return {"nominal_Wh": volts * ah, "capacity_Ah": ah,
            "scenario_minutes": 60 * ah * usable / current,
            "required_C_without_margin": current / ah,
            "label_continuous_A": None if c_rating is None else ah * c_rating}


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--volts", type=float, default=11.1)
    p.add_argument("--mah", type=float, default=2200)
    p.add_argument("--current", type=float, required=True)
    p.add_argument("--usable", type=float, default=0.8)
    p.add_argument("--c-rating", type=float)
    args = p.parse_args()
    print(json.dumps(calculate(args.volts, args.mah, args.current, args.usable, args.c_rating), indent=2))
