#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "control.h"
#include "configuration/config.h"
namespace flight {
// MPU6050 only: WHO_AM_I stays 0x68 even with AD0 high/address 0x69.
// Register reference: docs/sensor-migration.md. No MPU6500 ACCEL_CONFIG2 writes.
class Imu {
  float bx = 0, by = 0, bz = 0;
  unsigned samples = 0;
  float sumx = 0, sumy = 0, sumz = 0;
  float bax = 0, bay = 0, baz = 0, sax = 0, say = 0, saz = 0;
  bool calibrated = false;
  bool reject() {
    ++errors;
    if (!calibrated)
      resetCalibration();
    return false;
  }
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
  uint8_t identity = 0;
  uint32_t errors = 0;
  struct Raw {
    int16_t ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
  } raw;
  void resetCalibration() {
    calibrated = false;
    bx = by = bz = bax = bay = baz = 0;
    samples = 0;
    sumx = sumy = sumz = sax = say = saz = 0;
  }
  unsigned calibrationProgress() const {
    return samples;
  }
  bool begin() {
    resetCalibration();
    errors = 0;
    Wire.begin(cfg::sda, cfg::scl, 400000);
    Wire.setTimeOut(2);
    identity = 0;
    if (!read(0x75, &identity, 1) || identity != 0x68)
      return reject();
    if (!write(0x6b, 0x80))
      return reject();
    delay(100); // Startup only; never called by a running control loop.
    const uint8_t regs[] = {0x6b, 0x6c, 0x1a, 0x19, 0x1b, 0x1c, 0x37, 0x38},
                  vals[] = {1, 0, cfg::imuDlpf, cfg::imuSampleDivider, 8, 16, 0, 1};
    for (unsigned i = 0; i < sizeof(regs); i++)
      if (!write(regs[i], vals[i]))
        return reject();
    for (unsigned i = 0; i < sizeof(regs); i++) {
      uint8_t v;
      if (!read(regs[i], &v, 1) || v != vals[i])
        return reject();
    }
    return true;
  }
  bool sample(Sample &s) {
    uint8_t ready = 0, b[14];
    if (!read(0x3a, &ready, 1) || !(ready & 1) || !read(0x3b, b, 14))
      return reject();
    auto signedWord = [&](int i) { return int16_t((uint16_t(b[i]) << 8) | b[i + 1]); };
    raw = {signedWord(0), signedWord(2),  signedWord(4),
           signedWord(8), signedWord(10), signedWord(12)};
    // Detect raw clipping before a calibrated bias could hide it.
    if (std::abs(int(raw.ax)) >= 32560 || std::abs(int(raw.ay)) >= 32560 ||
        std::abs(int(raw.az)) >= 32560 || std::abs(int(raw.gx)) >= 32420 ||
        std::abs(int(raw.gy)) >= 32420 || std::abs(int(raw.gz)) >= 32420)
      return reject();
    // Mount raw +X forward, +Y left, +Z up. s.a is gravity proxy = -specific force in FRD.
    s.ax = -signedWord(0) / 4096.0f - bax;
    s.ay = signedWord(2) / 4096.0f - bay;
    s.az = signedWord(4) / 4096.0f - baz;
    s.gx = signedWord(8) / 65.5f - bx;
    s.gy = -signedWord(10) / 65.5f - by;
    s.gz = -signedWord(12) / 65.5f - bz;
    return valid(s) || reject();
  }
  bool calibrate(const Sample &s) {
    if (calibrated)
      return true;
    float n = std::sqrt(s.ax * s.ax + s.ay * s.ay + s.az * s.az);
    if (!valid(s) || std::fabs(n - 1) > 0.05f || std::fabs(s.ax) > .087f ||
        std::fabs(s.ay) > .087f || s.az < .95f || std::fabs(s.gx) > 3 || std::fabs(s.gy) > 3 ||
        std::fabs(s.gz) > 3) {
      resetCalibration();
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
      calibrated = true;
      return true;
    }
    return false;
  }
};
} // namespace flight
