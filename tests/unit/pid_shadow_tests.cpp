#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include "stabilizer.h"
#include "protocol/shadow_telemetry.h"
static bool near(float a, float b, float e = 1e-5f) { return std::fabs(a-b) < e; }
int main() {
  unsigned checks = 0;
  using namespace flight;
  Pid p;
  float out = p.update(10, 0, .004f);
  assert(near(out, p.pTerm + p.iTerm + p.dTerm)); ++checks;
  assert(near(out, p.output) && p.output != 0); ++checks;
  assert(near(p.error, 10) && near(p.pTerm, cfg::rateKp * 10)); ++checks;
  p.update(10, 1, .004f);
  assert(p.dTerm < 0 && p.iTerm >= 0); ++checks;
  float integral = p.integral;
  p.update(10, 1, .004f, true);
  assert(near(integral,p.integral)); ++checks;
  assert(p.update(10,1,.02f) == 0 && p.output == 0); ++checks;
  Estimator attitude; attitude.initialized = true; attitude.roll = 5;
  Sample rate;
  Stabilizer c;
  auto m = c.update(true,.35f,0,0,0,attitude,rate,.004f);
  assert(m.valid && c.rateTarget[0] == -20 && c.correction[0] < 0); ++checks;
  assert(m.motor[0] > m.motor[1] && m.motor[3] > m.motor[2]); ++checks;
  m = c.update(false,.35f,0,0,0,attitude,rate,.004f);
  for (float v:m.motor) { assert(v == 0); ++checks; }
  assert(c.rollPid.output == 0 && c.rollPid.integral == 0); ++checks;
  shadow_wire::Packet src;
  src.mode = 1; src.sequence = 42; src.sensorValid = 1;
  src.axis[0].p = .1f;
  for (float &v:src.motor) v = .35f;
  uint8_t bytes[shadow_wire::size]; shadow_wire::encode(src,bytes);
  shadow_wire::Packet dst;
  assert(shadow_wire::decode(bytes,sizeof(bytes),dst) && dst.sequence == 42); ++checks;
  bytes[40] ^= 1;
  assert(!shadow_wire::decode(bytes,sizeof(bytes),dst)); ++checks;
  bytes[40] ^= 1;
  assert(!shadow_wire::decode(bytes,sizeof(bytes)-1,dst)); ++checks;
  src.motor[0] = std::numeric_limits<float>::quiet_NaN();
  shadow_wire::encode(src,bytes);
  assert(!shadow_wire::decode(bytes,sizeof(bytes),dst)); ++checks;
  src.motor[0] = .35f; src.mode = 2;
  shadow_wire::encode(src,bytes);
  assert(!shadow_wire::decode(bytes,sizeof(bytes),dst)); ++checks;
  for (int axis=0;axis<3;++axis) {
    auto positive = mix(.5f,axis==0?.1f:0,axis==1?.1f:0,axis==2?.1f:0);
    auto negative = mix(.5f,axis==0?-.1f:0,axis==1?-.1f:0,axis==2?-.1f:0);
    for (int motor=0;motor<4;++motor) {
      int sign = axis==0?cfg::rollSign[motor]:axis==1?cfg::pitchSign[motor]:cfg::yawSign[motor];
      assert(near(positive.motor[motor],.5f+.1f*sign)); ++checks;
      assert(near(negative.motor[motor],.5f-.1f*sign)); ++checks;
    }
  }
  auto saturated = mix(.95f,.2f,.2f,.2f);
  assert(saturated.valid && saturated.saturated); ++checks;
  assert(near(saturated.motor[0],1) && near(saturated.motor[1],1) &&
         near(saturated.motor[2],1) && near(saturated.motor[3],.2f)); ++checks;
  assert(!mix(.35f,std::numeric_limits<float>::quiet_NaN(),0,0).valid); ++checks;
  std::printf("PASS %u PID shadow, telemetry and mixer assertions\n",checks);
}
