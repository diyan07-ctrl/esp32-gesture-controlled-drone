#include <iostream>
#include "imu.h"
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
  Imu wrong;
  Wire.registers[0x75] = 0x68;
  CHECK(!wrong.begin());
  Wire.registers[0x75] = 0x70;
  Imu corrupted;
  Wire.corrupt = 0x1c;
  CHECK(!corrupted.begin());
  Wire.corrupt = -1;
  Imu imu;
  CHECK(imu.begin());
  CHECK(Wire.registers[0x1b] == 8 && Wire.registers[0x1c] == 16 && Wire.registers[0x1d] == 3);
  Sample s;
  CHECK(!imu.sample(s));
  Wire.registers[0x3a] = 1;
  Wire.word(0x3b, 0);
  Wire.word(0x3d, 0);
  Wire.word(0x3f, 4096);
  Wire.word(0x43, 0);
  Wire.word(0x45, 0);
  Wire.word(0x47, 0);
  CHECK(imu.sample(s));
  CHECK(s.az == 1 && s.ax == 0 && s.gx == 0);
  Wire.fail = true;
  CHECK(!imu.sample(s));
  Wire.fail = false;
  Wire.word(0x43, 32767);
  CHECK(!imu.sample(s));
  Wire.word(0x43, 0);
  // Synthetic stationary fixture offsets, with an interrupted calibration run.
  Wire.word(0x3b, -80);
  Wire.word(0x3d, 80);
  Wire.word(0x3f, 4137);
  Wire.word(0x43, 32);
  Wire.word(0x45, 32);
  Wire.word(0x47, -65);
  for (unsigned i = 0; i < 500; i++) {
    CHECK(imu.sample(s));
    CHECK(!imu.calibrate(s));
  }
  Sample moving = s;
  moving.gx = 4;
  CHECK(!imu.calibrate(moving));
  for (unsigned i = 0; i < cfg::calibrationSamples; i++) {
    CHECK(imu.sample(s));
    CHECK(imu.calibrate(s) == (i + 1 == cfg::calibrationSamples));
  }
  CHECK(imu.sample(s));
  CHECK(std::fabs(s.ax) < 1e-5f && std::fabs(s.ay) < 1e-5f && std::fabs(s.az - 1) < 1e-5f);
  CHECK(std::fabs(s.gx) < 1e-5f && std::fabs(s.gy) < 1e-5f && std::fabs(s.gz) < 1e-5f);
  std::cout << "PASS " << checks
            << " MPU6500 driver/calibration assertions against mocked I2C (no sensor hardware)\n";
}
