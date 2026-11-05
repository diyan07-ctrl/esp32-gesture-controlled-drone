# MPU6050 and potentiometer milestone

Target: two MPU6050s, thumb/index flex sensors and potentiometer throttle. No
new PID gains, ESC enablement, flight-control integration or flight acceptance.
The previous MPU6500 target is superseded; it is deliberately rejected by the
new driver's identity check. Existing motor_test source and shared motor signals
remain unchanged. Use [milestone evidence](sensor-migration-results.md) for actual
build/test status, not historical results.

## GPIO contract and electrical verification boundary

Code and automated repository checks enforce both IMUs on SDA21/SCL22,
glove thumb34/index35/pot36 (ADC1), and ESC GPIO25/26/32/33. These are software-
verified assignments; continuity, header exposure, pullups, rails and actual harness
are **not physically verified**. GPIO36 is normally labeled VP/SVP on DevKit V1;
check the actual board. Never share the pot with thumb GPIO34. All three ADC inputs
are input-only with no internal pull resistor and must be externally conditioned.

MPU6050 supply and I2C logic must match the inspected breakout, with pullups only
to 3.3 V. AD0 low selects bus address 0x68, high selects 0x69; WHO_AM_I is 0x68
in both cases. No CS connection is specified for MPU6050. INT is not wired for the
polled baseline. [Full connection table](connection-tables.md).

## Driver and filter profile

The shared Imu class configures and reads back PWR_MGMT_1/2, CONFIG,
SMPLRT_DIV, GYRO_CONFIG, ACCEL_CONFIG, INT_PIN_CFG and INT_ENABLE. Initialization
checks identity, resets the device, waits 100 ms at startup only, selects PLL X
gyro clock and enables data-ready status. It never writes MPU6500-only register
0x1D. Full scales are fixed +/-8 g (4096 LSB/g), +/-500 deg/s (65.5 LSB/deg/s).
Samples are one 14-byte burst after data-ready; short/NACK/not-ready/clipping/
nonfinite/out-of-range data are rejected and counted. Raw counts are retained.

cfg::imuDlpf=3 selects approximately 44 Hz accelerometer and 42 Hz gyro bandwidth,
with about 4.9/4.8 ms sensor filter delay. cfg::imuSampleDivider=0 retains 1 kHz
sensor output for asynchronous 250 Hz application polling. It is not synchronized
sampling. Only documented DLPF modes 1–6 are accepted by config assertions.
Changing divider must still sustain application polling; not-ready resets an
unfinished calibration. No redundant software gyro filter was added.

Mount sensor raw X forward, Y left, Z up. Rates map to body FRD (+GX,-GY,-GZ);
the gravity proxy is (-AX,+AY,+AZ). Startup waits a further 100 ms before collecting
1000 consecutive level/stationary samples, estimating three gyro biases and
one-position accelerometer offsets against (0,0,1). Movement, clipping or read gaps
reset unfinished accumulation. Initialization/read/calibration failure inhibits
commands; no completion within 15 seconds requires inspection/reboot. Calibration
is volatile and repeated at startup, not silently persisted as a universal bias.
It does not establish six-face scale, misalignment or temperature compensation.

Complementary attitude estimation retains measured dt and Euler body-rate
kinematics for bounded tilt. cfg::attitudeTau=.5 s and gravityMin/Max=.85/1.15 g
control accelerometer correction. Roll correction uses shortest angular error;
pitch beyond the supported 60-degree envelope rejects estimation. Yaw is relative
integrated rate and drifts. Gains/thresholds are bench configuration, not measured
performance. Record real noise and latency before changing filters.

Primary register source: [InvenSense MPU-6000/6050 register map, revision 4.2,
hosted copy](https://hw101.tbs1.de/mpu6050/doc/MPU-6000-6050-Register-Map.pdf).
The original [TDK URL](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map1.pdf)
redirected during this implementation. [Espressif's MPU6050 documentation](https://github.com/espressif/esp-drone/blob/master/docs/en/rst/drivers.rst)
also describes identity/address separation and startup configuration. The driver
is independently implemented; no source code from those projects was copied.

## Flex and potentiometer conditioning

Flex endpoints normalize open to 0 and bent to 1 with either polarity. Require
non-rail counts and at least 100 counts span. cfg::flexTau=.04 s is the first-order
time constant; bend-on/off .75/.55 provide per-finger hysteresis before gesture
debounce. Change these in configuration after measuring actual finger distributions.

The pot's measured LOW/HIGH positions map to 0/1 with either wiring polarity.
Require endpoints strictly inside 32..4063 and at least 500 counts span; reject
samples at rails or more than cfg::potMargin=80 counts outside the calibrated
interval. Endpoint deadband cfg::potDeadband=.02 maps the bottom 2% exactly to zero.
The filtered output starts at zero, rises with .06 s low-pass and at most .5/s,
and follows reductions immediately. Invalid raw data/dt/config clear it to zero.
Arming evaluates the unfiltered target, so filtering cannot hide a high pot.

A plain pot tied to 0/3.3 V may reach ADC rails at physical endpoints. Do not use
software clamping to conceal this: design/verify travel stops or external end
resistance and conditioning so the full usable travel remains inside the valid
span. Exact resistor values depend on the actual pot/ADC and remain unverified.
Use an external fault bias/protection as appropriate; a plausible stuck wiper
cannot be detected reliably by this software. Rails may also indicate broken flex
wires; this does not prove every disconnect will be caught.

## Calibration console

Use the independent glove_sensors image with propulsion isolated. Send one
lowercase command at a time at 115200 baud:

| Command | Action |
|---|---|
| o | Hold both fingers open for 250 samples (~1 s) |
| b | Hold both fingers comfortably fully bent for 250 samples |
| l | Hold pot at chosen LOW endpoint for 250 samples |
| h | Hold pot at chosen HIGH endpoint for 250 samples |
| s | Explicitly save valid complete endpoints; cannot interrupt an active capture |
| x | Erase this revision's endpoint record and inhibit commands |

Each capture requires <=32 counts peak-to-peak spread; noisy/rail captures do not
replace endpoints. Capturing inhibits normal commands until successful save.
The new NVS namespace glove-sens-v1 and schema marker reject legacy flex-only
records. Writes invalidate the commit marker first and set it last; partial-write
failures stay inhibited. Records are sanity checked on load; there is no added
application CRC. The marker protocol is not a hardware flash fault proof.

Normal glove firmware locks calibration while its arm request is active or last
receiver state is ARMING_CHECKS/ARMED, even if the acknowledgement goes stale.
Diagnostics never issue commands, so their calibration console is always available.
IMU calibration remains independent of all four analog captures.

## Sensor-only images and serial records

~~~powershell
pio run -e glove_sensors
pio run -e drone_sensors
# Identify the actual port and board first; COMx below is a placeholder.
pio run -e glove_sensors -t upload --upload-port COMx
pio device monitor -p COMx -b 115200
~~~

Both images leave motor pins high-impedance and initialize neither radio nor ESCs.
Each prints identity/init status and S records at 5 Hz with timestamp, read/cal/
estimate validity, calibration count, errors, actual dt, misses, six raw counts,
six calibrated g/deg/s readings and roll/pitch/relative yaw. Invalid read values
are not valid measurements: always inspect read/estimate flags and sample age.
Glove A records contain thumb/index/pot raw counts, three health flags, flex
normalized values, two hysteresis states, filtered pot/target and valid/edit flags.
They are bounded human diagnostics, not the future framed desktop protocol.

Main glove G records now include pot health/raw/filtered values in addition to
flex/link/gesture/throttle fields; any external G reader must adopt the new schema.
Existing D drone serial records and command-v2 encoding are unchanged.

## Bench acceptance before further integration

1. Verify electrical rails, pullups, harness and GPIO36 with DMM/continuity; props
   removed and propulsion power isolated. Identify board/IMU labels and both ports.
2. Flash each matching sensor-only image. Record WHO_AM_I 0x68, complete startup
   calibration, 1000 samples, errors and dt over several minutes.
3. Repeat stationary and gentle positive/negative roll/pitch/yaw motion on each
   board. Compare known level/tilt and gyro signs, raw range, drift and noise.
4. Prove movement/read interruption resets incomplete calibration, and missing IMU
   or bad address inhibits estimation. Capture actual calibration duration and jitter.
5. Capture o/b/l/h, explicitly save, reboot and verify persistence. Record actual
   endpoints and repeatability; test finger hysteresis/noise and pot full travel.
6. Test disconnected/railed analog controls and calibrated interval excursions;
   prove pot fault clears output, no high-pot arming, and no flex throttle increments.
7. Document pass/fail for glove and drone separately using a dated device-specific
   record. Both physical IMU acceptance records are required before new flight-
   control integration. Builds/mocks alone do not satisfy that gate.
