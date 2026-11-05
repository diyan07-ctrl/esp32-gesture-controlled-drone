# Hardware connection audit — 2026-11-05

Status: documentary audit; no electrical measurement or board upload.
Subsequent [sensor migration](sensor-migration.md) now implements the dedicated
ADC36 and MPU6050 profile and updates current BOM/connection CSVs. The observations
below describe the original inspection; physical wiring verification remains pending.
Current owner requirement: two MPU6050s and a glove throttle potentiometer.
The older BOM/connection CSVs describe MPU6500/HC-SR04 hardware; retain them as
historical implementation inputs until firmware migration, then reconcile together.

## Connections for the requested target

| Board | Function | GPIO / connection | Audit result |
|---|---|---|---|
| Both | MPU6050 SDA | 21 | Retained; pullups must terminate at 3.3 V |
| Both | MPU6050 SCL | 22 | Retained; verify 400 kHz operation on actual wires |
| Both | IMU supply / ground | Verified compatible 3.3 V / local GND | Identify breakout regulator and pullups before power |
| Both | AD0 | GND for candidate address 0x68 | Address scan and WHO_AM_I are separate checks |
| Both | INT | Unconnected initially | Poll data-ready; scope latency before interrupt redesign |
| Glove | Thumb flex ADC | 34, ADC1 | Existing divider; measure resistance/endpoints |
| Glove | Index flex ADC | 35, ADC1 | Separate divider; measure resistance/endpoints |
| Glove | Pot wiper | 36 / VP, ADC1, proposed | Physically confirm exposed header; unused in current glove source |
| Glove | Pot end terminals | 3.3 V / GND | Verify orientation, resistance, wiper continuity and usable ADC span |
| Glove | Supply | USB or verified regulated board input | No raw LiPo |
| Drone | ESC1 / M1 front-right | 25 | Owner-reported mapping; verify physical output |
| Drone | ESC2 / M2 front-left | 26 | Owner-reported mapping; verify physical output |
| Drone | ESC3 / M3 rear-left | 32 | Owner-reported mapping; verify physical output |
| Drone | ESC4 / M4 rear-right | 33 | Owner-reported mapping; verify physical output |
| Drone | ESC signal return | Common electronics GND | Inspect connector; do not infer from wire color |
| Drone | Optional battery ADC | 34 | Disabled; divider/protection not established |
| Drone | Optional HC-SR04 | 16/17 proposed | Outside required target; leave disabled and unwired unless verified |

GPIO34/35/36 are input-only, without internal pull resistors. Use ADC1 with Wi-Fi;
the existing cfg::throttle=34 must not become a second analogRead of the thumb
node. Proposed GPIO36 needs a dedicated config constant and collision assertion.
No final harness may be declared verified from this table.

~~~text
GLOVE (all analog rails 3.3 V)
3V3 -- thumb flex --+-- selected Rfixed -- GND
                   +-- ADC conditioning -- GPIO34
3V3 -- index flex --+-- selected Rfixed -- GND
                   +-- ADC conditioning -- GPIO35
3V3 -- pot end A     pot wiper -- conditioning -- GPIO36 (proposed)
GND -- pot end B
3V3/GND -- MPU6050 VCC/GND; SDA/SCL -- GPIO21/22

DRONE POWER (connector identities and regulator remain unverified)
3S LiPo -- rated connector/protection -- PDB -- four ESC power inputs
                                             each ESC -- three motor phase wires
                            verified regulator -- ESP32 supported supply input
ESP32 GPIO25/26/32/33 ------------------------- corresponding ESC signal inputs
ESP32 GND ------------------------------------ ESC signal grounds / regulator GND
ESP32 3V3/GND -- MPU6050; GPIO21/22 -- SDA/SCL
USB power interaction with regulator must be checked before simultaneous use.
~~~

For a flex divider in the orientation above,
Vadc = Vrail * Rfixed / (Rflex + Rfixed). Select Rfixed from measured flat/bent
resistance, power rating, ADC source-impedance constraints and desired span;
roughly sqrt(Rflat*Rbent) is an idealized sensitivity starting point, not a
specified part value. Validate filtering and attenuation on the real ADC.
Select pot resistance and conditioning after checking the actual component and
ADC loading. Calibrate travel using measured endpoints; ensure full travel remains
electrically safe even when software clamps. Add suitable external bias/fault
detection so an open wiper cannot command flight; plausible stuck faults remain
a limitation and need a deliberate pilot enable/stop mechanism.

## Power and propulsion blockers

3S is 12.6 V fully charged, not always 11.1 V. Never feed it into ESP32 or MPU6050.
The exact ESC model/manual, BEC topology, logic-high threshold, endpoints,
startup behavior and no-signal behavior are unknown. Retain 50 Hz / 1000 us
minimum as a bench candidate; do not auto-calibrate ESCs or send maximum throttle
at boot. 2000 us maximum is also provisional. Source currently uses LEDC rather
than ESP32Servo; an adapter choice cannot establish electrical equivalence.

Do not parallel BEC positives. Identify whether ESCs have BECs at all, then use
one verified power architecture with isolated unused positive leads, rated
regulation and checked USB backfeed. Battery/PDB/ESC currents must not pass through
the ESP32. Check connector polarity, conductors, strain relief and ground noise.

Optional battery ADC remains disabled. The historical 47k/10k candidate divider
is not a fitted circuit: check maximum pack voltage/tolerance, ADC attenuation,
source impedance, capacitor, series/input protection and unpowered backfeed,
then DMM calibration under load. No battery percentage is currently supported.

Owner previously reported M1/M3 CCW and M2/M4 CW viewed from above. Independently
check position, GPIO, phase order, prop handedness and thrust direction. A2212
1000KV/30A labels do not establish safe prop size, full-load current, thermal
margin or lift. Obtain exact motor/ESC/prop data and guarded measurements.

## Required closure record

Photograph board/breakout and ESC labels, mark harness pinouts, measure supply and
ADC rails, record both WHO_AM_I values and MACs, pot/flex endpoints, regulator
load/transient behavior and PWM/no-signal scope captures. Then update BOM,
connection tables and a component-specific CAD schematic together. Existing
[diagrams](../diagrams/README.md) are system/wiring diagrams, not a verified
manufacturing schematic. A complete electrical schematic cannot be honestly
finalized until those components are identified.
