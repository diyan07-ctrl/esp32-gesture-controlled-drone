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
  Wire.registers[0x75] = 0x70;
  CHECK(!wrong.begin());
  Wire.registers[0x75] = 0x68;
  Wire.registers[0x1d] = 0xa5; // Reserved on MPU6050: must remain untouched.
  Imu corrupted;
  Wire.corrupt = 0x1c;
  CHECK(!corrupted.begin());
  Wire.corrupt = -1;
  Imu imu;
  CHECK(imu.begin());
  CHECK(Wire.registers[0x1b] == 8 && Wire.registers[0x1c] == 16 && Wire.registers[0x1d] == 0xa5);
  CHECK(Wire.registers[0x1a] == cfg::imuDlpf && Wire.registers[0x19] == cfg::imuSampleDivider);
  CHECK(Wire.registers[0x38] == 1 && Wire.registers[0x6b] == 1);
  CHECK(imu.identity == 0x68);
  Sample s;
  CHECK(!imu.sample(s));
  Wire.registers[0x3a] = 1;
  Wire.setWord(0x3b, 0);
  Wire.setWord(0x3d, 0);
  Wire.setWord(0x3f, 4096);
  Wire.setWord(0x43, 0);
  Wire.setWord(0x45, 0);
  Wire.setWord(0x47, 0);
  CHECK(imu.sample(s));
  CHECK(s.az == 1 && s.ax == 0 && s.gx == 0);
  CHECK(imu.raw.az == 4096 && imu.raw.ax == 0);
  Wire.setWord(0x3b, 4096);
  Wire.setWord(0x3d, 2048);
  Wire.setWord(0x43, 655);
  Wire.setWord(0x45, 655);
  Wire.setWord(0x47, -655);
  CHECK(imu.sample(s));
  CHECK(s.ax == -1 && s.ay == .5f && s.gx == 10 && s.gy == -10 && s.gz == 10);
  Wire.setWord(0x3b, 0);
  Wire.setWord(0x3d, 0);
  Wire.fail = true;
  CHECK(!imu.sample(s));
  Wire.fail = false;
  Wire.shortRead = true;
  CHECK(!imu.sample(s));
  Wire.shortRead = false;
  Wire.setWord(0x43, 32767);
  CHECK(!imu.sample(s));
  Wire.setWord(0x43, 0);
  // Synthetic stationary fixture offsets, with an interrupted calibration run.
  Wire.setWord(0x3b, -80);
  Wire.setWord(0x3d, 80);
  Wire.setWord(0x3f, 4137);
  Wire.setWord(0x43, 32);
  Wire.setWord(0x45, 32);
  Wire.setWord(0x47, -65);
  for (unsigned i = 0; i < 500; i++) {
    CHECK(imu.sample(s));
    CHECK(!imu.calibrate(s));
  }
  Sample moving = s;
  moving.gx = 4;
  CHECK(!imu.calibrate(moving));
  CHECK(imu.calibrationProgress() == 0);
  CHECK(!imu.calibrate(s));
  Wire.registers[0x3a] = 0;
  CHECK(!imu.sample(s));
  CHECK(imu.calibrationProgress() == 0);
  Wire.registers[0x3a] = 1;
  for (unsigned i = 0; i < cfg::calibrationSamples; i++) {
    CHECK(imu.sample(s));
    CHECK(imu.calibrate(s) == (i + 1 == cfg::calibrationSamples));
  }
  CHECK(imu.sample(s));
  CHECK(std::fabs(s.ax) < 1e-5f && std::fabs(s.ay) < 1e-5f && std::fabs(s.az - 1) < 1e-5f);
  CHECK(std::fabs(s.gx) < 1e-5f && std::fabs(s.gy) < 1e-5f && std::fabs(s.gz) < 1e-5f);
  CHECK(imu.calibrate(s)); // Successful calibration is idempotent.
  CHECK(imu.calibrationProgress() == cfg::calibrationSamples);
  // A second device instance cannot share bias or progress with the first.
  Imu second;
  CHECK(second.begin());
  CHECK(second.calibrationProgress() == 0);
  CHECK(second.sample(s) && std::fabs(s.gx) > .4f);
  CHECK(imu.sample(s) && std::fabs(s.gx) < 1e-5f);
  // Every configured register readback and identity failure must reject initialization.
  for (int reg : {0x6b, 0x6c, 0x1a, 0x19, 0x1b, 0x1c, 0x37, 0x38}) {
    Wire.corrupt = reg;
    Imu bad;
    CHECK(!bad.begin());
  }
  Wire.corrupt = -1;
  for (unsigned id : {0u, 0x69u, 0x70u, 0xffu}) {
    Wire.registers[0x75] = uint8_t(id);
    Imu bad;
    CHECK(!bad.begin());
  }
  Wire.registers[0x75] = 0x68;
  std::cout << "PASS " << checks
            << " MPU6050 driver/calibration assertions against mocked I2C (no sensor hardware)\n";
}
