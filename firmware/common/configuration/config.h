#pragma once
#include <cstdint>
namespace cfg {
constexpr int sda = 21, scl = 22, battery = 34;
constexpr int motors[4] = {25, 26, 32, 33};
// Owner-confirmed top-view numbering: FR, FL, RL, RR (counterclockwise).
constexpr int rollSign[4] = {-1, 1, 1, -1}, pitchSign[4] = {1, 1, -1, -1};
constexpr int yawSign[4] = {1, -1, 1, -1}; // CCW, CW, CCW, CW viewed from above
constexpr int flex[2] = {34, 35};
constexpr int ultrasonicTrigger = 16, ultrasonicEcho = 17;
constexpr bool ultrasonicVerified = false, batteryMonitorInstalled = false;
constexpr bool localPermitInstalled = false, gloveSwitchesInstalled = false;
constexpr uint8_t imuAddress = 0x68, radioChannel = 1;
constexpr unsigned calibrationSamples = 1000;
constexpr int arm = 18, kill = 19, led = 23, throttle = 36;
// MPU6050 CONFIG controls both accel/gyro DLPF. Full scales stay +/-8g, +/-500dps.
constexpr uint8_t imuDlpf = 3, imuSampleDivider = 0;
constexpr float attitudeTau = .5f, gravityMin = .85f, gravityMax = 1.15f;
constexpr float flexTau = .04f, flexBendOn = .75f, flexBendOff = .55f;
constexpr float potTau = .06f, potRisePerSecond = .5f, potDeadband = .02f;
constexpr float potMinSpan = 500, potMargin = 80, captureMaxSpread = 32;
constexpr unsigned endpointSamples = 250;
constexpr uint32_t periodUs = 4000, timeoutMs = 200, armHoldMs = 1000;
constexpr float maxTilt = 20, maxRate = 90, maxYaw = 60;
constexpr float angleKp = 4.0f;
constexpr float rateKp = 0.001f, rateKi = 0.0002f, rateKd = 0.00001f, rateLimit = 0.2f;
// Never enable by merely changing this flag. See docs/output-enablement.md.
constexpr bool outputsVerified = false;
constexpr bool powerVerified = false;
constexpr bool propulsionVerified = false;
constexpr bool controllerValidated = false;
constexpr int pwmHz = 50, pwmMin = 1000, pwmMax = 2000, pwmBits = 16;
constexpr float dividerRatio = 5.7f, batteryScale = 1.0f;
constexpr float lowBattery = 10.8f, criticalBattery = 9.9f;
// Thresholds above are bench placeholders; loaded pack thresholds need characterization.
constexpr unsigned logHz = 5;
static_assert(pwmHz > 0 && pwmBits > 0 && pwmBits <= 16, "Invalid ESC PWM configuration");
static_assert(pwmMin > 0 && pwmMin < pwmMax && pwmMax < 1000000 / pwmHz,
              "ESC pulse endpoints must fit the PWM frame");
static_assert(periodUs >= 2000 && periodUs <= 6000 && periodUs % 1000 == 0,
              "Control period must fit the tested timing contract and RTOS millisecond ticks");
static_assert(logHz > 0 && logHz <= 1000, "Diagnostic rate must be nonzero and bounded");
static_assert(imuAddress == 0x68 || imuAddress == 0x69, "Invalid MPU6050 I2C address");
static_assert(throttle != flex[0] && throttle != flex[1] && flex[0] != flex[1],
              "Glove ADC inputs must be distinct");
static_assert(throttle == 36, "Throttle uses dedicated ADC1 GPIO36/VP");
static_assert(imuDlpf >= 1 && imuDlpf <= 6, "Use a documented 1kHz MPU6050 DLPF mode");
static_assert(attitudeTau > 0 && gravityMin > 0 && gravityMin < 1 && gravityMax > 1,
              "Invalid complementary filter parameters");
static_assert(flexTau > 0 && flexBendOff > 0 && flexBendOff < flexBendOn && flexBendOn < 1,
              "Invalid flex filtering/hysteresis");
static_assert(potTau > 0 && potRisePerSecond > 0 && potDeadband > 0 && potDeadband < .1f &&
                  endpointSamples > 0 && endpointSamples <= 1000,
              "Invalid pot/capture configuration");
static_assert(motors[0] != motors[1] && motors[0] != motors[2] && motors[0] != motors[3] &&
                  motors[1] != motors[2] && motors[1] != motors[3] && motors[2] != motors[3],
              "ESC channels must use distinct pins");
} // namespace cfg
