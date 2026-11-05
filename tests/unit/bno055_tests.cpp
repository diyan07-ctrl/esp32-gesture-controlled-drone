#include <cassert>
#include <cmath>
#include <cstdio>
#include "bno055.h"
static void putLe(unsigned reg, int16_t value) {
  Wire.registers[reg] = uint16_t(value) & 255;
  Wire.registers[reg + 1] = uint16_t(value) >> 8;
}
int main() {
  unsigned checks = 0;
  flight::Bno055 absent;
  Wire.registers[0] = 0x68;
  assert(!absent.begin() && absent.failedRegister == 0); ++checks;
  Wire.registers[0] = 0xa0;
  flight::Bno055 bno;
  assert(bno.begin() && bno.detectedAddress == 0x28 && bno.identity == 0xa0); ++checks;
  assert(Wire.registers[0x3b] == 1 && Wire.registers[0x3d] == 8); ++checks;
  putLe(0x08,100); putLe(0x0a,-200); putLe(0x0c,1000);
  putLe(0x14,160); putLe(0x16,-80); putLe(0x18,16);
  putLe(0x20,16384); putLe(0x22,0); putLe(0x24,0); putLe(0x26,0);
  Wire.registers[0x35] = 0x3c; Wire.registers[0x39] = 5; Wire.registers[0x3a] = 0;
  flight::BnoSample sample;
  assert(bno.sample(sample) && sample.calibrated()); ++checks;
  assert(std::fabs(sample.accel[0]-.1f)<1e-5f && sample.accel[1]==-.2f); ++checks;
  assert(sample.gyro[0]==10 && sample.gyro[1]==-5 && sample.gyro[2]==1); ++checks;
  assert(sample.quaternion[0]==1 && sample.roll==0 && sample.pitch==0); ++checks;
  Wire.registers[0x35] = 0x10;
  assert(bno.sample(sample) && !sample.calibrated()); ++checks;
  Wire.shortRead = true;
  assert(!bno.sample(sample)); ++checks;
  Wire.shortRead = false;
  Wire.registers[0x3a] = 2;
  assert(!bno.sample(sample)); ++checks;
  Wire.registers[0x3a] = 0;
  putLe(0x20,0); putLe(0x22,0); putLe(0x24,0); putLe(0x26,0);
  assert(!bno.sample(sample)); ++checks;
  putLe(0x20,11585); putLe(0x22,11585);
  assert(bno.sample(sample) && std::fabs(sample.roll-90)<.1f); ++checks;
  assert(sample.quaternion[0]>.70f && sample.quaternion[1]>.70f); ++checks;
  std::printf("PASS %u BNO055 register/scale/quaternion/calibration assertions\n", checks);
}
