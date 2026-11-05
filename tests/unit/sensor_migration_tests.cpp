#include <iostream>
#include <limits>
#include "potentiometer.h"
#include "flex.h"
#include "gesture.h"
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
  using G = protocol::Gesture;
  float value = -1;
  PotCalibration pot;
  CHECK(!pot.normalize(1500, value));
  pot = {1000, 3000, true}; // Synthetic fixture values, not installed calibration.
  CHECK(pot.valid());
  CHECK(pot.normalize(1000, value) && value == 0);
  CHECK(pot.normalize(3000, value) && value == 1);
  CHECK(pot.normalize(1020, value) && value == 0);
  CHECK(pot.normalize(2000, value) && value > .48f && value < .50f);
  CHECK(!pot.normalize(0, value) && !pot.normalize(4095, value));
  CHECK(!pot.normalize(900, value) && !pot.normalize(3100, value));
  CHECK(!pot.normalize(std::numeric_limits<float>::quiet_NaN(), value));
  PotCalibration reverse{3000, 1000, true};
  CHECK(reverse.normalize(3000, value) && value == 0);
  CHECK(reverse.normalize(1000, value) && value == 1);
  CHECK((!PotCalibration{1000, 1200, true}.valid()));
  CHECK((!PotCalibration{0, 4095, true}.valid()));
  PotFilter filter;
  for (int i = 0; i < 600; i++) {
    float before = filter.value;
    CHECK(filter.update(3000, pot, .004f));
    CHECK(filter.value >= before && filter.value - before <= cfg::potRisePerSecond * .004f + 1e-6f);
    CHECK(filter.value >= 0 && filter.value <= 1 && filter.target == 1);
  }
  CHECK(filter.value > .99f);
  CHECK(filter.update(1000, pot, .004f) && filter.value == 0);
  CHECK(!filter.update(0, pot, .004f) && filter.value == 0 && !filter.initialized);
  CHECK(!filter.update(3000, pot, 1) && filter.value == 0);
  CHECK(!filter.update(3000, pot, 0));
  EndpointCapture capture;
  for (unsigned i = 0; i < cfg::endpointSamples; i++)
    CHECK(capture.add(uint16_t(1000 + i % 2)));
  CHECK(capture.valid() && capture.average() == 1000.5f);
  CHECK(!capture.add(1000));
  capture = {};
  for (unsigned i = 0; i < cfg::endpointSamples; i++)
    capture.add(uint16_t(1000 + (i % 2) * 100));
  CHECK(!capture.valid());
  // Hysteresis holds each finger independently between thresholds.
  GesturePipeline fingers;
  fingers.config.potentiometerThrottle = true;
  fingers.config.bendOn = .8f;
  fingers.config.bendOff = .4f;
  fingers.classify(0, 0, 0, .81f, .2f);
  CHECK(fingers.bent[0] && !fingers.bent[1]);
  fingers.classify(0, 0, 0, .6f, .81f);
  CHECK(fingers.bent[0] && fingers.bent[1]);
  fingers.classify(0, 0, 0, .39f, .6f);
  CHECK(!fingers.bent[0] && fingers.bent[1]);
  // Startup arm hold is insufficient without observing released fingers at low pot.
  GesturePipeline g;
  g.config.potentiometerThrottle = true;
  g.update(0, 0, 0, 1, 1, .004f, 0, 0);
  g.update(0, 0, 0, 1, 1, .004f, 2000, 0);
  CHECK(!g.armed);
  g.update(0, 0, 0, 0, 0, .004f, 2004, 0);
  g.update(0, 0, 0, 1, 1, .004f, 2008, .5f);
  g.update(0, 0, 0, 1, 1, .004f, 4008, .5f);
  CHECK(!g.armed);
  g.update(0, 0, 0, 1, 1, .004f, 4012, 0);
  CHECK(!g.armed);
  g.update(0, 0, 0, 1, 1, .004f, 5012, 0);
  CHECK(g.armed);
  g.update(0, 0, 0, 0, 0, .004f, 5016, .6f);
  CHECK(g.throttle == .6f);
  g.update(0, 0, 0, 1, 0, .004f, 5020, .6f);
  g.update(0, 0, 0, 1, 0, .004f, 5220, .6f);
  CHECK(g.throttle == .6f && g.stable != G::ThrottleUp);
  CHECK(g.update(0, -20, 0, 1, 1, .004f, 5224, .6f) == G::Emergency);
  CHECK(!g.armed && g.throttle == 0 && g.stopped);
  g.reset();
  CHECK(g.update(0, 0, 0, 0, 0, .004f, 5228, 0) == G::Emergency);
  GesturePipeline bad;
  bad.config.bendOn = bad.config.bendOff;
  CHECK(bad.update(0, 0, 0, 0, 0, .004f, 0) == G::Disarm);
  // Known gravity vectors establish signs on each independent estimator.
  for (float roll : {-45.0f, 0.0f, 45.0f}) {
    for (float pitch : {-30.0f, 0.0f, 30.0f}) {
      float r = roll / 57.2957795f, p = pitch / 57.2957795f;
      Sample s{-std::sin(p), std::sin(r) * std::cos(p), std::cos(r) * std::cos(p), 0, 0, 0};
      Estimator glove, drone;
      CHECK(glove.update(s, .004f) && drone.update(s, .004f));
      CHECK(std::fabs(glove.roll - roll) < .001f && std::fabs(drone.pitch - pitch) < .001f);
    }
  }
  Estimator e;
  Sample level;
  CHECK(e.update(level, .004f));
  Sample accelerated{0, 0, 2, 10, 0, 0};
  for (int i = 0; i < 250; i++)
    CHECK(e.update(accelerated, .004f));
  CHECK(std::fabs(e.roll - 10) < .001f); // Out-of-gravity acceleration is rejected for fusion.
  CHECK(!e.update(level, 0));
  e.tau = 0;
  CHECK(!e.update(level, .004f));
  std::cout << "PASS " << checks
            << " pot/flex/gesture/dual-estimator migration assertions (no hardware)\n";
}
