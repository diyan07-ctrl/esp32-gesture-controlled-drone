#include <iostream>
#include <limits>
#include "flex.h"
#include "gesture.h"
#include "range.h"
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
  // Synthetic endpoints exercise the algorithm; they are not device calibration values.
  FlexCalibration c;
  float out = 9;
  CHECK(!c.normalize(1000, out));
  c = {1000, 3000, true};
  CHECK(c.valid());
  CHECK(c.normalize(2000, out) && out == .5f);
  CHECK(!c.normalize(0, out));
  CHECK(!c.normalize(4095, out));
  c = {3000, 1000, true};
  CHECK(c.normalize(2000, out) && out == .5f);
  c = {2000, 2050, true};
  CHECK(!c.valid());
  c = {1000, 3000, true};
  FlexFilter f;
  CHECK(f.update(1000, c, .004f));
  CHECK(f.update(3000, c, .004f) && f.value > 0 && f.value < 1);
  CHECK(!f.update(0, c, .004f));
  GesturePipeline g;
  CHECK(g.update(0, 0, 0, 0, 0, .004f, 0) == G::Neutral);
  CHECK(g.update(0, 0, 0, 1, 1, .004f, 4) == G::Neutral);
  CHECK(!g.armed);
  g.update(0, 0, 0, 1, 1, .004f, 1004);
  CHECK(g.armed);
  g.update(0, 0, 0, 1, 0, .004f, 1008);
  g.update(0, 0, 0, 1, 0, .004f, 1160);
  CHECK(g.stable == G::ThrottleUp);
  for (unsigned i = 0; i < 500; i++)
    g.update(0, 0, 0, 1, 0, .004f, 1164 + 4 * i);
  CHECK(g.throttle > .29f && g.throttle < .32f);
  g.update(0, 0, 0, 0, 0, .004f, 3200);
  g.update(0, 0, 0, 0, 0, .004f, 3400);
  float held = g.throttle;
  g.update(0, 0, 0, 0, 0, .004f, 3404);
  CHECK(g.throttle == held);
  g.update(0, 20, 0, 1, 1, .004f, 3408);
  g.update(0, 20, 0, 1, 1, .004f, 3600);
  CHECK(!g.armed && g.throttle == 0 && g.stable == G::Disarm);
  CHECK(g.update(0, -20, 0, 1, 1, .004f, 3604) == G::Emergency);
  CHECK(g.stopped && !g.armed);
  g.reset();
  CHECK(g.update(0, 0, 0, 0, 0, .004f, 4000) == G::Emergency);
  for (int i = 0; i < 6; i++) {
    GesturePipeline x;
    float r = i == 0   ? -20
              : i == 1 ? 20
                       : 0,
          p = i == 2   ? -20
              : i == 3 ? 20
                       : 0,
          z = i == 4   ? -50
              : i == 5 ? 50
                       : 0;
    const G expected[] = {G::Left, G::Right, G::Forward, G::Backward, G::YawLeft, G::YawRight};
    x.update(r, p, z, 0, 0, .004f, 0);
    CHECK(x.update(r, p, z, 0, 0, .004f, 151) == expected[i]);
  }
  GesturePipeline debounce;
  debounce.update(20, 0, 0, 0, 0, .004f, 0);
  debounce.update(0, 0, 0, 0, 0, .004f, 100);
  CHECK(debounce.stable == G::Neutral);
  CHECK(debounce.update(0, 0, 0, 0, 0, 0, 104) == G::Disarm);
  RangeFilter range;
  CHECK(!range.healthy(0));
  CHECK(!range.accept(0, 0));
  CHECK(!range.accept(10, 0));
  CHECK(!range.accept(30000, 0));
  CHECK(range.accept(5831, 0xfffffff0));
  CHECK(range.healthy(0x10));
  CHECK(!range.healthy(0x200));
  CHECK(std::fabs(range.meters - 1) < .01f);
  CHECK(pwmDuty(0, false) == 0);
  CHECK(pwmDuty(0, true) == 3276);
  CHECK(pwmDuty(1, true) == 6553);
  CHECK(pwmDuty(9, true) == pwmDuty(1, true));
  CHECK(pwmDuty(-1, true) == pwmDuty(0, true));
  CHECK(pwmDuty(std::numeric_limits<float>::quiet_NaN(), true) == 0);
  CHECK(pwmDuty(.5f, true, false) == 0);
  uint8_t bytes[protocol::commandSize];
  protocol::Command cmd;
  cmd.session = 1;
  cmd.gesture = G::Right;
  cmd.flex[0] = 3000;
  cmd.flex[1] = 1500;
  cmd.orientation[0] = -1500;
  protocol::encode(cmd, bytes);
  protocol::Command back;
  CHECK(protocol::decode(bytes, sizeof(bytes), back));
  CHECK(back.gesture == G::Right && back.flex[0] == 3000 && back.orientation[0] == -1500);
  cmd.flex[0] = 4096;
  protocol::encode(cmd, bytes);
  CHECK(!protocol::decode(bytes, sizeof(bytes), back));
  cmd.flex[0] = 0;
  cmd.gesture = G(255);
  protocol::encode(cmd, bytes);
  CHECK(!protocol::decode(bytes, sizeof(bytes), back));
  CHECK(!protocol::decode(nullptr, 40, back));
  std::cout << "PASS " << checks << " sensor/gesture/PWM assertions\n";
}
