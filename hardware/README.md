# Hardware records

[bom.csv](bom.csv), [motor-mapping.csv](motor-mapping.csv), and the [current handover](../docs/handover.md) describe owner-reported installed hardware. The handover is authoritative for evidence limits and power wiring. CSV identities are not electrical approval.

`bom.historical.csv`, `pin-mapping.csv`, `supplier-availability.csv` and `schematics/` retain earlier glove/BNO055/optional-ultrasonic design records. They must not be treated as the installed MPU6050 harness. Current wiring is MPU6050 SDA21/SCL22; ESC FR25 CCW, FL26 CW, RL32 CCW, RR33 CW; shared ESC signal grounds; 3S/PDB to ESCs and regulated 5V buck to ESP32 USB-C subject to measured power-path compatibility. No glove is currently fitted.
