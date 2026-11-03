# Wiring and confirmed hardware

Owner reports on 2026-11-03 establish two MPU6500s, two flex sensors, HC-SR04, 3S 2200 mAh 50C, 1045 props, A2212 1000 KV motors and 30 A ESCs. Exact breakout, ESC and regulator models remain unknown. GPIO numbers below are chip GPIOs, not header positions. No actual electrical measurements were performed here.

## Both boards

| Function | GPIO / connection | Constraint |
|---|---|---|
| MPU6500 SDA / SCL | 21 / 22 | Pullups to 3.3 V only; 400 kHz candidate |
| MPU6500 VCC / GND | Verified sensor-compatible 3.3 V / common GND | Inspect breakout regulator/pullups; no assumed 5 V tolerance |
| MPU6500 AD0 | GND | Provisional address 0x68; identity register must read 0x70 |
| MPU6500 CS | High to sensor logic supply where exposed | I2C mode; inspect actual breakout |
| IMU mounting | Raw X forward, Y left, Z up | Firmware transforms into FRD body rates and gravity proxy |
| Optional LED | 23 via series resistor | Not flight-state evidence |

GPIO6–11 are flash pins; avoid strapping pins for added functions. GPIO34/35 are input-only ADC1 and have no internal pull resistors. No propulsion current may flow through sensor wiring or ESP32 ground leads.

## Glove

| Function | GPIO | Hardware/status |
|---|---:|---|
| Flex sensor 1 ADC | 34 | Resistive divider powered from 3.3 V |
| Flex sensor 2 ADC | 35 | Separate resistive divider powered from 3.3 V |
| Optional arm switch | 18 | Active low to GND; disabled by default |
| Optional hold-to-enable | 19 | Active low to GND; disabled by default |

For each flex sensor, connect 3.3 V → flex resistance → ADC node → fixed resistor → GND, with suitable ADC series resistance and filtering. Select the fixed resistor using the measured sensor resistance span and ADC safe range. No sensor resistance, open/bent count or usable endpoint is invented here. Keep the ADC within its characterized range, never above 3.3 V. Capture both endpoints through serial and confirm polarity; inverted endpoint order is supported. A broken wire often produces a rail reading, but a stuck plausible value cannot always be detected.

Power the glove from a verified regulated source such as its USB input. The two boards have independent supply arrangements; ESP-NOW does not require a wired ground between glove and drone.

## Drone

| Function | GPIO | Owner-reported motor location / status |
|---|---:|---|
| M1 / ESC1 signal | 25 | Front-right, CCW viewed from above |
| M2 / ESC2 signal | 26 | Front-left, CW |
| M3 / ESC3 signal | 32 | Rear-left, CCW |
| M4 / ESC4 signal | 33 | Rear-right, CW |
| HC-SR04 Trigger | 16 | Proposed assignment, confirm actual wire |
| HC-SR04 Echo | 17 | Proposed; **5 V level protection mandatory** |
| Optional pack ADC | 34 | Disabled unless divider hardware is fitted and verified |
| Optional run-permit switch | 19 | Active low; disabled until installed |

HC-SR04: VCC from regulated 5 V; GND common with ESP32; Echo through a divider/level shifter. A candidate divider is 10 kΩ from Echo to ADC-side node and 15 kΩ node to GND, giving 3.0 V at a 5.0 V Echo. Check tolerance, real supply maximum, logic thresholds and edge timing. Verify that the actual module recognizes a 3.3 V Trigger, or level-shift Trigger too. Do not connect Echo directly. Acquisition remains disabled with ultrasonicVerified=false until the interface and pins are checked. No reliable range is implied by the software acceptance interval of 0.02–4 m.

Optional battery divider: candidate 47 kΩ upper / 10 kΩ lower, 100 nF bypass and suitable series/protection circuitry. Ratio 5.7 gives approximately 2.211 V at 12.6 V. Verify back-power protection and calibrate with a DMM before batteryMonitorInstalled=true. Low/critical thresholds are provisional loaded-pack values.

All ESC signal grounds must share the drone electronics reference. Candidate series/pulldown signal resistors need validation against the exact ESC input. Isolate unused BEC positive leads individually; never parallel them. Use a verified regulated board input with USB/external-source interaction checked, never direct 3S. The F450 PDB carries battery-to-ESC power; route motor returns separately from the electronics reference. Inspect connector polarity and ESC startup/no-signal behavior before applying propulsion power.
