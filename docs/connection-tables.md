# Provisional connection tables

**Do not assemble solely from these tables.** GPIO numbers below are chip GPIOs, not header positions. They are checked against the Espressif reference DevKitC pin constraints, but owned PCB variants have not been identified. Confirm each board's schematic/header before wiring. Neither ESP32 nor IMU I/O is assumed 5 V tolerant. Do not use GPIO6–11 (flash), GPIO0/2/5/12/15 (straps), UART0 during programming, or GPIO34–39 as outputs. ADC1 is used because Wi-Fi conflicts with ADC2 acquisition.

## Glove

| Component | Component pin | ESP32 GPIO | Power source | Voltage | Notes |
|---|---|---|---|---|---|
| DevKit | USB or verified 5V input | — | Protected USB power bank | 5 V | Exactly one board supply; no unregulated cell |
| MPU6050 | VCC | — | Verified sensor-compatible 3V3 rail | 3.3 V | Check breakout regulator dropout and pullups |
| MPU6050 | GND | GND | Glove return | 0 V | Common signal reference |
| MPU6050 | SDA | 21 | Pullup to 3V3 | 3.3 V logic | Short wires; inspect module pullups |
| MPU6050 | SCL | 22 | Pullup to 3V3 | 3.3 V logic | 400 kHz candidate; measure rise time |
| MPU6050 | AD0 | GND | — | 0 V | Address 0x68; verify breakout |
| MPU6050 | INT, XDA, XCL | — | — | — | Not connected; data-ready register polled |
| Arm switch | Contact | 18 | Internal pullup | 3.3 V logic | Active low; other terminal GND |
| Hold-to-enable switch | Contact | 19 | Internal pullup | 3.3 V logic | Active low; open means inhibited |
| Throttle potentiometer | Wiper | 34 ADC1 | External 2.5 V reference | 0–2.5 V | 10 kΩ pot across reference/GND, 1 kΩ wiper series, 100 nF ADC-to-GND; 100 kΩ pulldown; input-only pin |
| Yaw potentiometer | Wiper | 35 ADC1 | External 2.5 V reference | 0–2.5 V | Same conditioning; center 1.25 V; 100 kΩ pulldown |
| Status LED | Anode | 23 | GPIO | 3.3 V | 1 kΩ series, cathode GND; indicates command permission |

Single IMU yaw integration is unsuitable as a reliable absolute yaw input. These separate physical controls are mandatory for this implementation. Potentiometer faults can appear as valid inputs; dual-channel plausibility is an unresolved improvement.

## Drone

| Component | Component pin | ESP32 GPIO | Power source | Voltage | Notes |
|---|---|---|---|---|---|
| Battery | Positive connector | — | 3S LiPo | 9–12.6 V operating example | Exact low limit is pack/load dependent; main isolation by connector |
| Battery | Negative connector | — | Return | 0 V | Distribution star return |
| PDB/harness | IN+, IN− | — | Battery | Up to 12.6 V | Rated for measured total continuous/peak current |
| Buck | VIN+, VIN− | — | Distribution | Up to 12.6 V + transients | Select documented input margin and cooling |
| Buck | OUT+, OUT− | — | Regulated | 5 V, proposed 2 A | Verify output before attaching board; USB disconnected |
| DevKit | Verified 5V and GND | — | Buck | 5 V | Exact board must support this input; never 3S into board |
| MPU6050 | VCC, GND | — | Board 3V3/GND | 3.3 V | Breakout rail compatibility must be measured |
| MPU6050 | SDA, SCL | 21,22 | 3V3 pullups | 3.3 V logic | Same constraints as glove |
| MPU6050 | AD0 | GND | — | 0 V | 0x68 |
| MPU6050 | INT/XDA/XCL | — | — | — | Unconnected |
| ESC1/2/3/4 | Signal | 25/26/27/32 | PWM peripheral | 3.3 V logic | Each via 220 Ω series; 10 kΩ signal-to-GND pulldown; confirm ESC threshold |
| ESC1/2/3/4 | Signal GND | GND reference | Low-current reference | 0 V | Verify connector pinout; route beside each signal |
| Battery divider | Sense midpoint | 34 ADC1 | Battery via 47 kΩ upper, 10 kΩ lower | 2.211 V at 12.6 V | 1% resistors, 100 nF lower resistor bypass; 1 kΩ ADC series |
| Local run-permit / emergency switch | Contact | 19 | Internal pullup | 3.3 V logic | Closed to GND permits; opening or broken wire inhibits/latches when armed |
| Status LED | Anode | 23 | GPIO | 3.3 V | 1 kΩ series then LED to GND |
| Optional buzzer | Driver control | — | Regulated rail | Match selected buzzer | Not implemented; transistor required if load exceeds GPIO rating |
| Optional current sensor | Analog/digital output | — | Model dependent | Must be 3.3 V compatible | No pin reserved or firmware driver; select after current range verified |
| Optional local arm switch | — | — | — | — | Not fitted; physical run permit plus glove arm used |

## ESC power and phases

| ESC | Positive battery input | Negative input | Signal GPIO | Signal ground | BEC | Motor phases |
|---|---|---|---:|---|---|---|
| ESC1 | PDB branch + | PDB branch − | 25 | Electronics reference GND | Unknown; output isolated | M1 front-left |
| ESC2 | PDB branch + | PDB branch − | 26 | Electronics reference GND | Unknown; output isolated | M2 front-right |
| ESC3 | PDB branch + | PDB branch − | 27 | Electronics reference GND | Unknown; output isolated | M3 rear-right |
| ESC4 | PDB branch + | PDB branch − | 32 | Electronics reference GND | Unknown; output isolated | M4 rear-left |

Each ESC's three phase wires connect only to its assigned motor. Swap two phases, with power removed, to reverse a conventional sensorless motor. Verify exact ESC instructions. Wire color and three-pin connector order are not standardized. Identify power, signal, GND and BEC before insertion. If a BEC exists, individually insulate its unused positive lead; retain the verified signal-ground connection. Never parallel unknown BEC outputs. Do not interrupt a presumed BEC lead until verified.

## Power distribution

| Path | Voltage | Current requirement | Physical routing |
|---|---|---|---|
| Battery → PDB | Up to 12.6 V + transients | Sum of measured motor currents + regulator input | High-current paired leads, short star branches |
| PDB → each ESC | Battery rail | Actual maximum motor/prop current + derating | Separate from sensor wiring |
| PDB → buck | Battery rail | Output power/(input voltage × efficiency) | Separate fused/protected electronics branch where suitable |
| Buck → DevKit | 5 V | Measured load + transient margin; proposed 2 A regulator | Low-current paired supply |
| DevKit → IMU | Verified 3.3 V | Sensor plus breakout load | Short quiet branch |
| Glove bank → glove | 5 V | Measured glove peak; provisional 1 A source | Electrically independent from drone |

No propulsion current through ESP32 grounds, breadboards or jumper wires. Buck return, divider bottom, IMU GND and signal GND share an electronics reference connected to distribution ground without carrying motor branch current.
