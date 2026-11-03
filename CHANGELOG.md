# Changelog

## 2026-11-03

- Retained the existing PlatformIO firmware architecture and pinned toolchain.
- Corrected hardware to MPU6500, two calibrated flex sensors, confirmed motor GPIO/numbering/spins and HC-SR04 supplementary sensing.
- Added protocol v2 sensor/gesture fields, emergency-priority transport, configurable desaturation/PWM and supervised arming/failsafe integration.
- Added a separate propeller-off motor test image, private pairing provisioning, reproducible native unit/integration/simulation runner and expanded CI.
- Updated current wiring/control/calibration/safety/testing documentation; preserved old procurement/publication reports as explicitly historical.
- Hardware validation and flight acceptance remain pending; output gates stay disabled.

## 2026-10-30

Initial experimental firmware/docs and public GitHub publication; historical tests described the earlier MPU6050/potentiometer baseline.
