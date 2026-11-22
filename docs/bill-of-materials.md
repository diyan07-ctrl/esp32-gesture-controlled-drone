# Current physical hardware inventory

Owner report supplied for closure on 2026-11-22; inventory does not establish compatibility. No glove, BNO055 or HC-SR04 is currently installed. The removed power bank has no measured replacement mass. See [handover](handover.md).

|ID|Category|Component|Quantity|Required/Optional|Specification and validation dependency|
|---|---|---|---|---|---|
|E01|Owner reported|F450 frame and soldered PDB|1|Installed|PDB current rating; insulation; solder joints and polarity need inspection|
|E02|Owner reported|ESP32 DevKit V1 with USB-C power input|1|Installed|Exact board/USB power-path schematic unverified; 3.3V GPIO|
|E03|Owner reported|MPU6050 breakout|1|Installed|SDA21/SCL22; breakout supply/pullups and mounted axes must be verified|
|E04|Owner reported|A2212/13T 1000KV BLDC motor|4|Installed|Exact maker and 3S/10-inch thrust/current/thermal curves unknown|
|E05|Owner reported|30A ESC|4|Installed|Model; BEC; signal logic; protocol; endpoints and no-signal response unknown|
|E06|Owner reported|3S 2200mAh LiPo|1|Installed|11.1V nominal; 12.6V full; owner says 40C; prior image reportedly 50C; unresolved|
|E09|Owner reported|10-inch CW/CCW propellers|4|Removed for bench tests|Pitch and exact part unverified; historical 1045 claim is not a new measurement|
|M01|Owner reported|Adjustable buck set to 5V feeding ESP32 USB-C|1|Installed|Current rating; ripple; transient response; USB-C compatibility/backfeed unmeasured|
|M05|Owner reported|Power/signal harness and common ESC signal grounds|1 set|Required|Wire/connector ratings; continuity; strain relief and grounding need inspection|
|T01|Validation access|DMM and controlled supply|1 each|Required|No new electrical measurements in closure|
|T02|Validation access|Oscilloscope or logic analyzer|1|Required|No ESC timing/rail transient validation in closure|
|T03|Validation access|Guarded thrust/current/thermal fixture|1|Required before flight|No loaded propulsion characterization established|

[Historical BOM](../hardware/bom.historical.csv) retains earlier glove/BNO055/accessory identities.
