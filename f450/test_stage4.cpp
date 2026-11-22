#include "F450_ESP32_FlightController/Dispatcher.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <initializer_list>
// Native hardware adapter. Authorization/ownership/RELEASE routing is production code.
struct Fixture {
  CommandSession session;
  CommandOrder order;
  SafetyLatch safety;
  StaticJsonDocument<2048> reply;
  unsigned calls = 0, stops = 0, changes = 0;
  float pulse = 1000;
  bool inputsHealthy = true;
  uint32_t commandTime = 0;
  Fixture() {
    safety.state = FlightState::DISARMED;
  }
  bool isArmed() const {
    return safety.state == FlightState::ARMED;
  }
  void stop() {
    safety.stop();
    order.invalidate();
    pulse = 1000;
    stops++;
  }
  void sessionChanged() {
    commandTime = 0;
    changes++;
  }
  void parameters(JsonDocument &d) {
    d["base"] = 1180;
  }
  DispatchResult command(const char *type, JsonDocument &d, JsonDocument &) {
    calls++;
    if (!strcmp(type, "CMD")) {
      float pilot[4], t = 1180;
      bool ok = parsePilot(d, pilot, t);
      if (ok)
        commandTime = 123;
      return {ok, ok ? "OK" : "INVALID"};
    }
    if (!strcmp(type, "ARM")) {
      bool ok = d["arm_token"].is<uint32_t>() && session.commandSeen &&
                authorizeArm(safety, order, d["arm_token"], inputsHealthy);
      if (ok)
        pulse = 1180;
      return {ok, ok ? "OK" : "ARM_BLOCKED"};
    }
    if (!strcmp(type, "CLEAR_FAULT")) {
      bool ok = safety.clear(inputsHealthy, true);
      if (ok)
        order.invalidate();
      return {ok, ok ? "OK" : "BLOCKED"};
    }
    return {true, "OK"}; // other action callbacks are counted, not hardware simulated
  }
  bool raw(const std::string &bytes, uint32_t sender = 1) {
    return dispatchDatagram(bytes.data(), bytes.size(), sender, session, order, *this, reply);
  }
  bool send(const char *type, uint32_t seq, uint32_t sender = 1, int64_t generation = -1) {
    StaticJsonDocument<512> d;
    d["type"] = type;
    d["seq"] = seq;
    if (strcmp(type, "GET"))
      d["session"] = generation < 0 ? session.generation : uint32_t(generation);
    d["arm_token"] = order.token;
    if (!strcmp(type, "CMD")) {
      d["roll"] = 0;
      d["pitch"] = 0;
      d["yaw"] = 0;
      d["climb"] = 0;
    }
    std::string bytes;
    serializeJson(d, bytes);
    assert(raw(bytes, sender));
    return reply["ok"];
  }
  std::string snapshot() {
    char b[400];
    snprintf(b, sizeof(b), "%d,%d,%u,%u,%d,%d,%u,%u,%d,%d,%u,%u,%u,%u,%.2f", int(safety.state),
             session.active, session.owner, session.generation, session.commandSeen, order.set,
             order.last, order.token, session.exhausted, order.exhausted, calls, stops, changes,
             commandTime, pulse);
    return b;
  }
  void reject(const char *type, uint32_t seq, uint32_t sender = 1, int64_t gen = -1) {
    auto before = snapshot();
    assert(!send(type, seq, sender, gen));
    assert(snapshot() == before);
  }
};
int main() {
  Fixture f;
  assert(f.send("GET", 55));
  assert(!f.session.active && !f.order.set);
  assert(f.reply["owned"] == false);
  assert(f.send("HELLO", 100));
  assert(f.session.active && f.session.owner == 1 && f.order.last == 100);
  f.reject("HELLO", 100);
  f.reject("HELLO", 101, 2); // duplicate acquisition and foreign IP
  assert(f.send("CMD", 101));
  uint32_t old = f.session.generation;
  f.reject("RELEASE", 102, 2);
  f.reject("RELEASE", 100);
  f.reject("RELEASE", 101);
  for (const char *seq : {"-1", "1.5", "4294967296", "true", "\"102\""}) {
    auto before = f.snapshot();
    std::string raw = "{\"type\":\"RELEASE\",\"session\":1,\"seq\":" + std::string(seq) + "}";
    assert(!f.raw(raw));
    assert(f.snapshot() == before);
  }
  assert(f.send("RELEASE", 102));
  assert(!f.session.active && !f.order.set && !f.session.commandSeen);
  assert(f.session.generation != old);
  assert(f.commandTime == 0 && f.pulse == 1000);
  f.reject("RELEASE", 102, 1, old);
  f.reject("RELEASE", 103, 1, old);
  f.reject("ARM", 104, 1, old);
  f.reject("CMD", 105, 1, old);
  f.reject("HELLO", 106, 1, old);
  {
    auto saved = f.snapshot();
    assert(!f.raw("{\"type\":\"RELEASE\",\"session\":1}"));
    assert(f.snapshot() == saved);
  }
  puts("Stage4 RELEASE: current owner, foreign, malformed, duplicate, delayed and repeated "
       "post-release PASS");
  assert(f.send("GET", 700, 2));
  assert(!f.session.active);
  assert(f.send("HELLO", 7, 2));
  assert(f.order.last == 7); // unrelated fresh window
  f.reject("CMD", 106, 1, old);
  f.reject("ARM", 107, 1, old);
  f.reject("HELLO", 108, 1, old);
  auto before = f.snapshot();
  assert(!f.send("ARM", 8, 2));
  assert(f.safety.state == FlightState::DISARMED);
  assert(f.order.last == 8);
  assert(f.send("CMD", 9, 2));
  assert(f.send("ARM", 10, 2));
  assert(f.isArmed());
  f.reject("RELEASE", 11, 2);
  assert(f.order.last == 10);
  assert(f.send("STOP", 11, 2));
  assert(!f.isArmed() && f.pulse == 1000);
  assert(f.send("RELEASE", 12, 2));
  assert(f.send("HELLO", 1, 2));
  f.reject("RELEASE", 12, 2, old + 1); // same IP, retired generation
  puts("Stage4 sessions: read-only GET, fresh HELLO window, old owner traffic/ARM/HELLO, armed "
       "rejection, release after STOP, same-IP generation replay PASS");
  Fixture wrap;
  assert(wrap.send("HELLO", 0xfffffffe));
  assert(wrap.send("CMD", 0xffffffff));
  assert(wrap.send("RELEASE", 0));
  assert(wrap.send("HELLO", 400));
  wrap.reject("HELLO", 400);
  wrap.reject("RELEASE", 0x80000190); // ambiguous half-range
  puts("Stage4 sequence: wraparound, duplicate HELLO, half-range rejection, unrelated new window "
       "PASS");
  for (bool hard : {false, true}) {
    Fixture x;
    assert(x.send("HELLO", 10));
    if (hard)
      x.safety.fault();
    else
      x.safety.fail(true);
    auto state = x.safety.state;
    assert(x.send("RELEASE", 11));
    assert(x.safety.state == state && x.pulse == 1000);
    assert(x.send("HELLO", 0));
    assert(x.send("CMD", 1));
    assert(!x.send("ARM", 2));
    assert(x.safety.state == state);
  }
  Fixture barrier;
  assert(barrier.send("HELLO", 1));
  assert(barrier.send("CMD", 2));
  uint32_t token = barrier.order.token, gen = barrier.session.generation;
  assert(barrier.send("STOP", 3));
  assert(barrier.send("RELEASE", 4));
  assert(barrier.send("HELLO", 1));
  std::string oldArm = "{\"type\":\"ARM\",\"seq\":100,\"session\":" + std::to_string(gen) +
                       ",\"arm_token\":" + std::to_string(token) + "}";
  before = barrier.snapshot();
  assert(barrier.raw(oldArm));
  assert(barrier.reply["ok"] == false);
  assert(barrier.snapshot() == before);
  assert(barrier.send("CMD", 2));
  assert(barrier.send("ARM", 3));
  assert(barrier.isArmed());
  puts("Stage4 safety: RELEASE preserves FAULT/FAILSAFE, STOP barrier survives reacquisition, "
       "fresh CMD and explicit ARM still required PASS");
  Fixture audit;
  for (const char *type : {"CMD", "ARM", "MANUAL", "ALT", "PID", "RESET", "BASE", "ALT_VALIDATE",
                           "SAVE", "CLEAR_FAULT", "RELEASE"})
    audit.reject(type, 1);
  assert(audit.send("HELLO", 10));
  assert(audit.send("CMD", 11));
  uint32_t resumeToken = audit.order.token;
  assert(audit.send("HELLO", 12));
  assert(!audit.session.commandSeen && audit.commandTime == 0 && audit.order.last == 12 &&
         audit.order.token != resumeToken);
  audit.reject("HELLO", 12);
  for (const char *type : {"CMD", "ARM", "MANUAL", "ALT", "PID", "RESET", "BASE", "ALT_VALIDATE",
                           "SAVE", "CLEAR_FAULT", "RELEASE"})
    audit.reject(type, 13, 1, 99);
  for (const char *type : {"CMD", "ARM", "MANUAL", "ALT", "PID", "RESET", "BASE", "ALT_VALIDATE",
                           "SAVE", "CLEAR_FAULT"})
    audit.reject(type, 11, 2);
  assert(audit.send("STOP", 0, 2));
  assert(!audit.isArmed());
  assert(audit.send("DISARM", 0, 2));
  assert(!audit.isArmed());
  puts("Stage4 authorization: all control/configuration branches reject foreign IP before "
       "callbacks; foreign STOP/DISARM remain available PASS");
}
