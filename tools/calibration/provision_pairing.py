"""Write an ignored pairing header. Keys are generated locally and never printed."""
import argparse
from pathlib import Path
import re
import secrets

def mac(value):
    if not re.fullmatch(r"(?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}", value):
        raise argparse.ArgumentTypeError("Use six colon-separated hexadecimal bytes")
    result = bytes.fromhex(value.replace(":", ""))
    if result[0] & 1 or not any(result):
        raise argparse.ArgumentTypeError("A nonzero unicast station MAC is required")
    return result

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--glove-mac", required=True, type=mac)
    parser.add_argument("--drone-mac", required=True, type=mac)
    args = parser.parse_args()
    if args.glove_mac == args.drone_mac:
        parser.error("The two station MACs must differ")
    dest = Path(__file__).resolve().parents[2] / "firmware/common/local_secrets.h"
    def array(data):
        return ",".join(f"0x{x:02x}" for x in data)
    text = ("#pragma once\n#include <cstdint>\nnamespace pairing {\n"
            "constexpr bool configured=true;\n#ifdef BUILD_GLOVE\n"
            f"constexpr uint8_t peer[6]={{{array(args.drone_mac)}}};\n#else\n"
            f"constexpr uint8_t peer[6]={{{array(args.glove_mac)}}};\n#endif\n"
            f"constexpr uint8_t pmk[16]={{{array(secrets.token_bytes(16))}}};\n"
            f"constexpr uint8_t lmk[16]={{{array(secrets.token_bytes(16))}}};\n}}\n")
    with dest.open("x", encoding="utf-8") as stream:
        stream.write(text)
    print("Wrote ignored pairing header; rebuild and flash BOTH targets. Keep this file private.")

if __name__ == "__main__":
    main()
