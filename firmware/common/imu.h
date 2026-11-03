#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "control.h"
#include "configuration/config.h"
namespace flight {
// MPU6500 only: WHO_AM_I 0x70. Reject other parts rather than assuming compatibility.
class Imu {
  float bx = 0, by = 0, bz = 0;
  unsigned samples = 0;
  float sumx = 0, sumy = 0, sumz = 0;
  float bax = 0, bay = 0, baz = 0, sax = 0, say = 0, saz = 0;
  bool write(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(cfg::imuAddress);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
  }
  bool read(uint8_t reg, uint8_t *p, uint8_t n) {
    Wire.beginTransmission(cfg::imuAddress);
    Wire.write(reg);
    if (Wire.endTransmission(false))
      return false;
    if (Wire.requestFrom(cfg::imuAddress, size_t(n), true) != n)
      return false;
    for (unsigned i = 0; i < n; i++)
      p[i] = Wire.read();
    return true;
  }

public:
  bool begin() {
    Wire.begin(cfg::sda, cfg::scl, 400000);
    Wire.setTimeOut(2);
    uint8_t id = 0;
    if (!read(0x75, &id, 1) || id != 0x70)
      return false;
    const uint8_t regs[] = {0x6b, 0x6c, 0x1a, 0x19, 0x1b, 0x1c, 0x1d},
                  vals[] = {1, 0, 3, 0, 8, 16, 3};
    for (unsigned i = 0; i < 7; i++)
      if (!write(regs[i], vals[i]))
        return false;
    for (unsigned i = 0; i < 7; i++) {
      uint8_t v;
      if (!read(regs[i], &v, 1) || v != vals[i])
        return false;
    }
    return true;
  }
  bool sample(Sample &s) {
    uint8_t ready = 0, b[14];
    if (!read(0x3a, &ready, 1) || !(ready & 1) || !read(0x3b, b, 14))
      return false;
    auto word = [&](int i) { return int16_t((uint16_t(b[i]) << 8) | b[i + 1]); };
    // Mount raw +X forward, +Y left, +Z up. s.a is gravity proxy = -specific force in FRD.
    s.ax = -word(0) / 4096.0f - bax;
    s.ay = word(2) / 4096.0f - bay;
    s.az = word(4) / 4096.0f - baz;
    s.gx = word(8) / 65.5f - bx;
    s.gy = -word(10) / 65.5f - by;
    s.gz = -word(12) / 65.5f - bz;
    return valid(s);
  }
  bool calibrate(const Sample &s) {
    float n = std::sqrt(s.ax * s.ax + s.ay * s.ay + s.az * s.az);
    if (!valid(s) || std::fabs(n - 1) > 0.05f || std::fabs(s.ax) > .087f ||
        std::fabs(s.ay) > .087f || s.az < .95f || std::fabs(s.gx) > 3 || std::fabs(s.gy) > 3 ||
        std::fabs(s.gz) > 3) {
      samples = 0;
      sumx = sumy = sumz = sax = say = saz = 0;
      return false;
    }
    sumx += s.gx;
    sumy += s.gy;
    sumz += s.gz;
    sax += s.ax;
    say += s.ay;
    saz += s.az;
    if (++samples == cfg::calibrationSamples) {
      bx = sumx / samples;
      by = sumy / samples;
      bz = sumz / samples;
      bax = sax / samples;
      bay = say / samples;
      baz = saz / samples - 1;
      return true;
    }
    return false;
  }
};
} // namespace flight
