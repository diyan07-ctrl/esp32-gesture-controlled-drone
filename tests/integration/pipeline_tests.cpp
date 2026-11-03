#include <iostream>
#include "protocol/packet.h"
#include "safety.h"
#include "control.h"
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
  for (int fault = 0; fault < 7; fault++) {
    Safety s;
    s.begin();
    s.initialized(true);
    s.calibrated(true);
    protocol::Receiver rx;
    protocol::Command c;
    c.session = 100 + fault;
    uint8_t bytes[protocol::commandSize];
    auto send = [&](uint32_t now) {
      protocol::encode(c, bytes);
      return rx.accept(bytes, sizeof(bytes), now, s.armed());
    };
    CHECK(send(0));
    Checks k;
    k.sensor = k.calibrated = k.battery = k.hardware = k.lowThrottle = true;
    auto step = [&](uint32_t now) {
      k.fresh = rx.fresh(now, cfg::timeoutMs);
      k.enabled = rx.command.flags & protocol::enabled;
      k.arm = rx.command.flags & protocol::arm;
      k.kill = rx.command.flags & protocol::stop;
      s.step(k, now);
    };
    step(0);
    c.flags = protocol::enabled | protocol::arm;
    for (uint32_t ms = 20; ms <= 1040; ms += 20) {
      ++c.seq;
      CHECK(send(ms));
      step(ms);
    }
    CHECK(s.armed());
    c.throttle = 500;
    ++c.seq;
    CHECK(send(1060));
    step(1060);
    CHECK(s.armed());
    auto m = mix(.5f, .1f, -.1f, .05f);
    CHECK(m.valid);
    for (float v : m.motor)
      CHECK(pwmDuty(v, s.armed()) >= pwmDuty(0, true));
    if (fault == 0) {
      step(1260);
      CHECK(s.armed());
      step(1261);
      CHECK(s.state == State::FAILSAFE);
    }
    if (fault == 1) {
      protocol::encode(c, bytes);
      bytes[13] ^= 1;
      CHECK(!rx.accept(bytes, sizeof(bytes), 1080, true));
      CHECK(rx.received == 1060);
      step(1261);
    }
    if (fault == 2) {
      CHECK(!send(1080));
      CHECK(rx.received == 1060);
      step(1261);
    }
    if (fault == 3) {
      c.flags = protocol::stop;
      c.gesture = protocol::Gesture::Emergency;
      ++c.seq;
      CHECK(send(1080));
      step(1080);
      CHECK(s.state == State::FAILSAFE && s.faults == Emergency);
    }
    if (fault == 4) {
      k.sensor = false;
      step(1080);
      CHECK(s.state == State::FAULT);
    }
    if (fault == 5) {
      k.hardFaults = Timing;
      step(1080);
      CHECK(s.state == State::FAULT);
    }
    if (fault == 6) {
      c.flags = protocol::enabled;
      ++c.seq;
      CHECK(send(1080));
      step(1080);
      CHECK(s.state == State::DISARMED);
    }
    CHECK(!s.armed());
    for (float v : m.motor)
      CHECK(pwmDuty(v, s.armed()) == 0);
    c.flags = protocol::enabled | protocol::arm;
    c.throttle = 0;
    c.seq += 3;
    CHECK(send(1300));
    step(1300);
    CHECK(!s.armed());
    if (fault != 6)
      CHECK(s.state == State::FAILSAFE || s.state == State::FAULT);
  }
  Safety init;
  init.begin();
  init.initialized(false);
  CHECK(init.state == State::FAULT);
  CHECK(!init.armed());
  // Lost sequence accounting and invalid-range commands never refresh the link.
  protocol::Receiver r;
  protocol::Command c;
  c.session = 1;
  uint8_t b[protocol::commandSize];
  protocol::encode(c, b);
  CHECK(r.accept(b, sizeof(b), 0, false));
  c.seq = 4;
  protocol::encode(c, b);
  CHECK(r.accept(b, sizeof(b), 20, false));
  CHECK(r.lost == 3);
  c.seq = 5;
  c.throttle = 1001;
  protocol::encode(c, b);
  CHECK(!r.accept(b, sizeof(b), 40, false));
  CHECK(r.received == 20);
  std::cout << "PASS " << checks
            << " mocked command-to-safety-to-PWM integration assertions (no RF)\n";
}
