"""Evaluate the provisional divider at full pack voltage."""
import argparse
import json
import math


def calculate(max_voltage=12.6, upper=47000, lower=10000, tolerance=0.01, capacitance=1e-7):
    if not all(math.isfinite(x) and x > 0 for x in (max_voltage, upper, lower, capacitance)):
        raise ValueError("inputs must be finite and positive")
    if not 0 <= tolerance < 1:
        raise ValueError("tolerance must be in [0,1)")
    current = max_voltage / (upper + lower)
    rth = upper * lower / (upper + lower)
    return {"adc_V": max_voltage * lower / (upper + lower),
            "worst_high_adc_V": max_voltage * lower * (1 + tolerance) /
            (upper * (1 - tolerance) + lower * (1 + tolerance)),
            "divider_A": current, "upper_W": current**2 * upper,
            "lower_W": current**2 * lower, "thevenin_ohms": rth,
            "tau_s": rth * capacitance, "cutoff_Hz": 1 / (2 * math.pi * rth * capacitance)}


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--max-voltage", type=float, default=12.6)
    a = p.parse_args()
    print(json.dumps(calculate(a.max_voltage), indent=2))
