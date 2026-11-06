#include "F450_ESP32_FlightController/Protocol.h"
#include "F450_ESP32_FlightController/Safety.h"
#include <assert.h>
#include <stdio.h>
#include <initializer_list>
#include <string.h>
void closeTo(float a, float b, float eps = .001f) {
  assert(fabsf(a - b) <= eps);
}
int main() {
  CommandOrder order;
  SafetyLatch safety;
  safety.state = FlightState::DISARMED;
  assert(order.accept(10));
  uint32_t oldToken = order.token;
  order.observeStop(12, true);
  order.invalidate();
  safety.stop();
  assert(order.last == 12);
  assert(!order.accept(11));
  assert(!order.accept(12));
  assert(order.accept(13));
  assert(!authorizeArm(safety, order, oldToken, true)); // unseen higher ARM generated before STOP
  assert(safety.state == FlightState::DISARMED);
  uint32_t current = order.token;
  assert(order.accept(14));
  assert(authorizeArm(safety, order, current, true));
  assert(!order.accept(14));
  assert(!authorizeArm(safety, order, current, true));
  order.observeStop(9, true);
  order.invalidate();
  safety.stop();
  assert(order.last == 14);
  assert(!order.accept(13));
  assert(!authorizeArm(safety, order, current, true));
  current = order.token;
  order.release();
  assert(!order.tokenMatches(current));
  assert(order.accept(1));
  order.set = true;
  order.last = 0xfffffffe;
  assert(order.accept(0xffffffff));
  order.observeStop(0, true);
  order.invalidate();
  assert(order.last == 0);
  assert(!order.accept(0xffffffff));
  assert(order.accept(1));
  assert(!order.accept(0x80000001));
  current = order.token;
  order.observeStop(200, false);
  order.invalidate();
  assert(order.last == 1);
  assert(!order.tokenMatches(current));
  order.token = UINT32_MAX;
  order.invalidate();
  assert(order.exhausted);
  assert(!order.tokenMatches(UINT32_MAX));
  puts("Stage3 UDP: stop barrier, delayed/duplicate ARM, stale/foreign STOP, release, wrap, "
       "half-range, token exhaustion PASS");
  for (const char *bad : {"{}", "{\"type\":\"ARM\",\"seq\":-1}",
                          "{\"type\":\"ARM\",\"seq\":4294967296}", "{\"type\":\"ARM\",\"seq\":1.5}",
                          "{\"type\":\"ARM\",\"seq\":true}", "{\"type\":\"ARM\",\"seq\":\"1\"}"}) {
    StaticJsonDocument<256> d;
    assert(!deserializeJson(d, bad));
    assert(!envelope(d));
  }
  StaticJsonDocument<256> d;
  for (const char *bad : {"{\"type\":\"ARM\",\"seq\":1}junk", "{\"type\":\"ARM\",\"seq\":1}{}"})
    assert(!parseEnvelope(d, bad, strlen(bad)));
  const char embeddedNull[] = "{\"type\":\"ARM\",\"seq\":1}\0junk";
  assert(!parseEnvelope(d, embeddedNull, sizeof(embeddedNull) - 1));
  const char *valid = "{\"type\":\"GET\",\"seq\":0} \r\n";
  assert(parseEnvelope(d, valid, strlen(valid)));
  assert(deserializeJson(d, "{bad"));
  assert(!deserializeJson(d, "{\"type\":\"STOP\",\"seq\":4294967295}"));
  assert(envelope(d));
  puts("Stage3 envelope: missing, negative, overflow, fractional, bool, string sequence and "
       "malformed JSON rejected PASS");
  SafetyLatch latch;
  latch.state = FlightState::DISARMED;
  CommandOrder commands;
  assert(authorizeArm(latch, commands, commands.token, true));
  latch.fail(true);
  commands.invalidate();
  for (int i = 0; i < 3; i++) {
    assert(!authorizeArm(latch, commands, commands.token, true));
    assert(!strcmp(latch.block(), "FAILSAFE_CLEAR_REQUIRED"));
  }
  assert(!latch.clear(false, true));
  assert(!latch.clear(true, false));
  latch.stop();
  assert(latch.state == FlightState::FAILSAFE);
  assert(!latch.arm(true)); // sensor recovery and STOP do not clear latch
  assert(latch.clear(true, true));
  commands.invalidate();
  assert(latch.state == FlightState::DISARMED); // explicit clearance does not arm
  assert(!latch.arm(false));
  assert(authorizeArm(latch, commands, commands.token, true));
  latch.fault();
  commands.invalidate();
  for (int i = 0; i < 3; i++) {
    assert(!latch.arm(true));
    assert(!latch.clear(true, true));
    latch.stop();
    assert(latch.state == FlightState::FAULT);
  }
  puts("Stage3 safety: latched FAULT/FAILSAFE, repeated ARM, sensor recovery, STOP bypass "
       "prevention, explicit clearance and fresh ARM PASS");
  HeightEstimator h;
  uint32_t time = 60000;
  for (int i = 0; i < 3; i++) {
    assert(h.update(100, 0, 0, time));
    assert(h.healthy(time) == (i == 2));
    time += 60000;
  }
  assert(!h.update(NAN, 0, 0, time));
  assert(h.count == 0 && !h.healthy(time));
  time += 60000;
  assert(h.update(50, 0, 0, time));
  closeTo(h.height, 54.03);
  assert(!h.healthy(time));
  time += 60000;
  assert(h.update(50, 0, 0, time));
  assert(!h.healthy(time));
  time += 60000;
  assert(!h.update(201, 0, 0, time));
  assert(h.count == 0);
  time += 60000;
  for (int i = 0; i < 3; i++) {
    assert(h.update(60, 0, 0, time));
    assert(h.healthy(time) == (i == 2));
    time += 60000;
  }
  assert(!h.healthy(time + 250001));
  time += 360000;
  for (int i = 0; i < 3; i++) {
    assert(h.update(20, 0, 0, time));
    closeTo(h.height, 24.03);
    assert(h.healthy(time) == (i == 2));
    time += 60000;
  }
  assert(!h.update(20, 31, 0, time));
  assert(h.count == 0);
  h.invalidate();
  assert(!h.latestValid && h.count == 0);
  puts("Stage3 ultrasonic: three consecutive valid, invalid gap, two-valid rejection, implausible, "
       "stale restart, filter reset, tilt guard PASS");
  const float pulses[] = {1000, 1180, 1500, 2000};
  const uint32_t expected[] = {3277, 3867, 4915, 6554};
  for (int i = 0; i < 4; i++) {
    uint32_t duty;
    assert(pulseToDuty(pulses[i], duty));
    assert(duty == expected[i]);
    closeTo(duty * 20000.f / 65536.f, pulses[i], .153);
  }
  float motors[4];
  assert(mix(1500, 0, 0, 0, motors));
  for (float value : motors)
    closeTo(value, 1500);
  puts("Stage3 PWM: 1000/1180/1500/2000 duty counts and zero-correction unscaled mixer PASS");
}
