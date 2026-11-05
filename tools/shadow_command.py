"""Send a bounded diagnostic command over USB; the shadow image has no ESC outputs."""
import argparse
import time
import serial

COMMANDS = {"sensor":"n", "shadow":"s", "roll_right":"r", "roll_left":"l",
            "pitch_forward":"f", "pitch_back":"b", "neutral":"c",
            "motor_locked":"m", "stop":"x"}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("command", choices=COMMANDS)
    p.add_argument("--port", required=True)
    args = p.parse_args()
    port = serial.Serial(port=None, baudrate=115200, timeout=.2)
    port.dtr = False
    port.rts = False
    port.port = args.port
    with port:
        port.write(COMMANDS[args.command].encode("ascii"))
        time.sleep(.25)
        for _ in range(10):
            line = port.readline()
            if line.startswith(b"COMMAND,"):
                print(line.decode("ascii", errors="replace").strip())
                break


if __name__ == "__main__": main()
