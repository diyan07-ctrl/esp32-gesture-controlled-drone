# Validation record

Executed 2026-11-06. No hardware was uploaded, armed or bench tested.

| Check | Result | Evidence / limits |
|---|---|---|
| Full ESP32 firmware compilation and link | PASS | PlatformIO espressif32 6.9.0, Arduino core 2.0.17, ESP32 Dev Module; 46,372 bytes RAM, 783,837 bytes flash |
| Python syntax | PASS | `python -m py_compile f450/f450_ground_station.py` |
| Ground station unit tests | 3 PASS | `python -m unittest discover -s f450 -p test_ground_station.py -v`; finite numeric validation, JSON command format and sequence wrap, gain ACK handling |
| Native control mathematics | PASS | Zig C++17 executable from `test_control.cpp`: reference first PID step, integral clamp, roll/pitch/yaw mixer geometry, quaternion reference identity, heading crossing and height offset arithmetic |
| Hardware sensor and PWM operation | UNVERIFIED | Requires actual ESP32, BNO055, HC-SR04 and scope |
| Real Wi-Fi / UDP / NVS operation | UNVERIFIED | Unit tests are laptop component tests; firmware parsing and faults were inspected and compiled, not exercised on hardware |
| Timing / watchdog | UNVERIFIED physically | Nominal scheduling and overrun cutoff implemented; measure real jitter and forced watchdog restart |
| GUI keyboard operation | UNVERIFIED interactively | Tkinter imported during tests; no interactive session performed |
| Altitude stability and flight | UNVERIFIED | Altitude defaults zero, intentionally disabled until configured; reference gains are not validated on this hardware |

Windows build note: long absolute include paths caused GCC `CreateProcess` failures. A temporary `subst P:` mapping to the repository shortened include paths and allowed a complete build. Build using a short checkout path (for example `C:\src\f450`) if this occurs. The mapping used during verification was removed afterward. No special drive letter is embedded in source/configuration.

Native test reproduction with a normal C++ compiler:

```text
c++ -std=c++17 f450/test_control.cpp -o f450_test
./f450_test
```

Reference flight-controller snapshot SHA256:
`2E672F6156BA0EE8827CA9D77BFC78405ED8EBE2FB872EFE08EEBCB57C716496`

The snapshot preserves the fetched source bytes. The upstream URL follows main and may change; use this checked-in snapshot when comparing constants.
