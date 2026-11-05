#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <cmath>
#include <cstdint>
#include <initializer_list>
namespace flight {
// BNO055 sensor-frame diagnostics only. No vehicle-axis transform or control authority.
// Register/scale source: Bosch BST-BNO055-DS000-18, sections 3.3/3.6/4.3.
struct BnoSample {
  float accel[3] = {}, gyro[3] = {}, quaternion[4] = {};
  float roll = 0, pitch = 0, yaw = 0;
  uint8_t calibration = 0, systemStatus = 0, systemError = 0;
  bool calibrated() const {
    return ((calibration >> 4) & 3) == 3 && ((calibration >> 2) & 3) == 3;
  }
};
inline int16_t bnoSigned(const uint8_t *p) {
  return int16_t(uint16_t(p[0]) | (uint16_t(p[1]) << 8));
}
inline bool decodeBnoData(const uint8_t accel[6], const uint8_t gyro[6],
                          const uint8_t quat[8], BnoSample &out) {
  BnoSample candidate = out;
  for (int i = 0; i < 3; ++i) {
    candidate.accel[i] = bnoSigned(accel + i * 2) / 1000.0f; // UNIT_SEL=mg
    candidate.gyro[i] = bnoSigned(gyro + i * 2) / 16.0f;     // deg/s
  }
  float normSquared = 0;
  for (int i = 0; i < 4; ++i) {
    candidate.quaternion[i] = bnoSigned(quat + i * 2) / 16384.0f;
    normSquared += candidate.quaternion[i] * candidate.quaternion[i];
  }
  if (!std::isfinite(normSquared) || normSquared < .64f || normSquared > 1.44f)
    return false;
  float inv = 1.0f / std::sqrt(normSquared);
  for (float &q : candidate.quaternion) q *= inv;
  float w = candidate.quaternion[0], x = candidate.quaternion[1];
  float y = candidate.quaternion[2], z = candidate.quaternion[3];
  candidate.roll = std::atan2(2 * (w * x + y * z), 1 - 2 * (x * x + y * y)) * 57.2957795f;
  float sine = 2 * (w * y - z * x);
  candidate.pitch = std::asin(std::fmax(-1.0f, std::fmin(1.0f, sine))) * 57.2957795f;
  candidate.yaw = std::atan2(2 * (w * z + x * y), 1 - 2 * (y * y + z * z)) * 57.2957795f;
  if (!std::isfinite(candidate.roll) || !std::isfinite(candidate.pitch) ||
      !std::isfinite(candidate.yaw)) return false;
  out = candidate;
  return true;
}
class Bno055 {
  uint8_t address = 0;
  bool read(uint8_t reg, uint8_t *data, uint8_t count) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0 ||
        Wire.requestFrom(address, size_t(count), true) != count) return false;
    for (uint8_t i = 0; i < count; ++i) data[i] = Wire.read();
    return true;
  }
  bool write(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(address);
    Wire.write(reg); Wire.write(value);
    return Wire.endTransmission() == 0;
  }
public:
  uint8_t identity = 0, detectedAddress = 0, failedRegister = 0;
  uint32_t errors = 0;
  bool begin() {
    errors = 0; identity = detectedAddress = failedRegister = 0;
    Wire.begin(21, 22, 100000); // Conservative until breakout clock stretching is measured.
    Wire.setTimeOut(20);
    for (uint8_t candidate : {uint8_t(0x28), uint8_t(0x29)}) {
      address = candidate;
      if (read(0x00, &identity, 1) && identity == 0xA0) {
        detectedAddress = candidate;
        break;
      }
    }
    if (!detectedAddress) { failedRegister = 0x00; ++errors; return false; }
    // Configuration-mode writes and timing are startup-only, never in the sample loop.
    const uint8_t registers[] = {0x3D, 0x07, 0x3B, 0x3E, 0x3D};
    const uint8_t values[] = {0x00, 0x00, 0x01, 0x00, 0x08};
    for (unsigned i = 0; i < sizeof(registers); ++i) {
      if (!write(registers[i], values[i])) {
        failedRegister = registers[i]; ++errors; return false;
      }
      if (registers[i] == 0x3D) delay(values[i] == 0 ? 20 : 10);
    }
    uint8_t mode = 0;
    if (!read(0x3D, &mode, 1) || (mode & 0x0f) != 0x08) {
      failedRegister = 0x3D; ++errors; return false;
    }
    return true;
  }
  bool sample(BnoSample &out) {
    uint8_t accel[6], gyro[6], quaternion[8], calibration, system[2];
    if (!detectedAddress || !read(0x08, accel, 6) || !read(0x14, gyro, 6) ||
        !read(0x20, quaternion, 8) || !read(0x35, &calibration, 1) ||
        !read(0x39, system, 2)) { ++errors; return false; }
    BnoSample next = out;
    next.calibration = calibration;
    next.systemStatus = system[0]; next.systemError = system[1];
    if (next.systemError != 0 || !decodeBnoData(accel, gyro, quaternion, next)) {
      ++errors; return false;
    }
    out = next;
    return true;
  }
};
} // namespace flight
