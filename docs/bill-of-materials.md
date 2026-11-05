# Current hardware inventory and interface checklist

Current revision: BNO055 on the drone, MPU6050 on the glove, pot throttle.
This is an inventory, not electrical compatibility approval. No current prices
were researched; historical supplier snapshots do not override this target.

|ID|Category|Component|Quantity|Required/Optional|Specification and validation dependency|
|---|---|---|---|---|---|
|E01|Existing|F450 frame with PDB|1|Required|Owner specified; current rating and orientation validation pending|
|E02|Existing|ESP32 DevKit V1 drone; ESP32 glove|2|Required|Drone ESP32-D0WDQ6; 4MB historical detection; verify exact board circuits|
|E03|Existing|MPU6050 breakout (glove)|1|Required for glove|3.3V logic; identity 0x68; physical acceptance pending|
|E11|Existing|BNO055 breakout (drone)|1|Required for drone bench|I2C address 0x29 and CHIP_ID 0xA0 observed; rail and pullups unmeasured|
|E04|Existing|A2212 1000 KV BLDC|4|Required|Owner specified; exact maker and thrust/current curves unknown|
|E05|Existing|30 A ESC|4|Required|Exact model; BEC; PWM startup/endpoints/no-signal behavior unknown|
|E06|Existing|3S LiPo 11.1V 2200mAh 50C|1|Required|Owner marking; not measured continuous capability|
|E07|Existing|Resistive flex sensors|2|Required|Actual resistance; divider values and measured endpoints required|
|E08|Existing|HC-SR04|1|Optional|Historical accessory; disabled; Echo protection needed if fitted|
|E10|Existing|Throttle potentiometer|1|Required|ADC1 GPIO36; actual resistance and non-rail full-travel conditioning unverified|
|E09|Existing|1045 propellers|4|Required for eventual flight|Owner specified; remove during all initial tests; matched spin/thrust verification pending|
|M01|Interface|Regulated drone electronics supply|1|Required independently of USB|Verify installed regulator; board input and transients; do not parallel BEC outputs|
|M02|Interface|Glove regulated supply|1|Required|USB or verified regulated source; no raw LiPo to ESP32|
|M03|Interface|Flex divider/ADC conditioning|2 circuits|Required|Choose fixed resistance from measured flex range; 3.3V-safe input; filtering|
|M04|Interface|HC-SR04 Echo level protection|1 circuit|Required|Divider or level shifter; candidate 10k upper/15k lower must be electrically verified|
|O01|Optional|Battery divider and protection|1 circuit|Optional monitoring|Candidate 47k upper/10k lower; disabled until fitted/verified/DMM calibrated|
|O02|Optional|Glove arm and enable switches|2|Optional|Legacy additional interlocks; disabled by default; active-low dry contacts|
|O03|Optional|Drone local run-permit switch|1|Optional|Disabled until fitted; opening inhibits when configured|
|O04|Optional|Status LEDs and series resistors|2 sets|Optional|GPIO23; select safe LED current|
|T01|Test equipment|DMM and suitable controlled supply|1 each|Required test access|Rails; divider; polarity; power sequence|
|T02|Test equipment|Oscilloscope or logic analyzer|1|Required test access|I2C; PWM; RMT trigger/echo; shutdown and jitter|
|T03|Test equipment|Guarded thrust/current/thermal fixture|1 set|Required before flight|Actual 3S/1045 load validation; safe physical isolation|

See [wiring](connection-tables.md), [validation](validation-results.md), and [motor layout](../hardware/motor-mapping.csv).
