#include <iostream>
#include "pilot.h"
#include "stabilizer.h"
unsigned checks = 0;
#define CHECK(x)                                                                                   \
  do {                                                                                             \
    ++checks;                                                                                      \
    if (!(x)) {                                                                                    \
      std::cerr << "FAIL " << __LINE__ << " " << #x << '\n';                                       \
      return 1;                                                                                    \
    }                                                                                              \
  } while (0)
int main() {
  using namespace flight;
  // Regression for the SIL-discovered one-cycle pitch validity defect.
  Estimator boundary;
  float p = 59.9f / 57.2957795f;
  CHECK(!boundary.update({-std::sin(p), 0, std::cos(p), 0, 494, 0}, .004f));
  CHECK(std::fabs(boundary.pitch) <= 60);
  CHECK(boundary.update(Sample{}, .004f));
  // Prove shared stabilization extraction retains the original controller equations,
  // including PID memory, disarmed resets and prior-cycle saturation feedback.
  Stabilizer shared;
  Pid roll, pitch, yaw;
  bool saturated = false;
  Estimator attitude;
  for (int i = 0; i < 2000; i++) {
    float t = .5f + .4f * std::sin(i * .01f), rc = 20 * std::sin(i * .03f),
          pc = 15 * std::cos(i * .02f);
    bool armed = i % 101 != 0;
    Sample sample{0, 0, 1, 3 * std::sin(i * .04f), 2 * std::cos(i * .04f), 1};
    attitude.roll = 5 * std::sin(i * .02f);
    attitude.pitch = 4 * std::cos(i * .03f);
    Mix original;
    if (armed && t > .02f) {
      float r = roll.update(clamp(cfg::angleKp * (rc - attitude.roll), -cfg::maxRate, cfg::maxRate),
                            sample.gx, .004f, saturated);
      float q =
          pitch.update(clamp(cfg::angleKp * (pc - attitude.pitch), -cfg::maxRate, cfg::maxRate),
                       sample.gy, .004f, saturated);
      float y = yaw.update(10, sample.gz, .004f, saturated);
      original = mix(t, r, q, y);
    } else {
      roll.reset();
      pitch.reset();
      yaw.reset();
      original = mix(0, 0, 0, 0);
    }
    saturated = original.saturated;
    auto result = shared.update(armed, t, rc, pc, 10, attitude, sample, .004f);
    CHECK(result.valid == original.valid && result.saturated == original.saturated);
    for (int j = 0; j < 4; j++)
      CHECK(std::fabs(result.motor[j] - original.motor[j]) < 1e-7f);
  }
  Pilot pilot;
  FlexCalibration flex[2] = {{1000, 3000, true}, {1200, 2800, true}};
  PotCalibration pot{1000, 3000, true};
  uint16_t raw[2] = {1000, 1200};
  Estimator level;
  Sample sensor;
  auto command = pilot.update(level, sensor, true, raw, 1000, flex, pot, false, .004f, 0, true, 3);
  CHECK(command.flags == protocol::enabled && command.throttle == 0 && !pilot.gestures.armed);
  raw[0] = 3000;
  raw[1] = 2800;
  for (unsigned i = 1; i < 400; i++)
    command = pilot.update(level, sensor, true, raw, 1000, flex, pot, false, .004f, 4 * i, true, 3);
  CHECK(pilot.gestures.armed && command.throttle == 0);
  for (unsigned i = 400; i < 800; i++)
    command = pilot.update(level, sensor, true, raw, 2500, flex, pot, false, .004f, 4 * i, true, 5);
  CHECK(command.throttle > 700 && command.throttle < 800);
  command = pilot.update(level, sensor, true, raw, 0, flex, pot, false, .004f, 3200, true, 5);
  CHECK(command.throttle == 0 && command.flags == 0 && !pilot.gestures.armed);
  std::cout << "PASS " << checks << " shared orchestration parity and SIL defect assertions\n";
}
