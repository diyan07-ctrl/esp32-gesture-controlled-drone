#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include "packet.h"
namespace shadow_wire {
constexpr size_t size = 194;
constexpr uint8_t version = 1;
struct Axis {
  float setpoint = 0, measured = 0, error = 0, p = 0, i = 0, d = 0;
  float output = 0, rateTarget = 0, measuredRate = 0;
};
struct Packet {
  uint8_t mode = 0, calibration = 0, sensorValid = 0;
  uint32_t sequence = 0, uptimeMs = 0, dtUs = 0, imuErrors = 0;
  Axis axis[3];
  float motor[4] = {}, accel[3] = {}, gyro[3] = {}, quaternion[4] = {};
};
inline void putFloat(uint8_t *out, float v) {
  uint32_t bits;
  std::memcpy(&bits, &v, 4);
  protocol::put32(out, bits);
}
inline float getFloat(const uint8_t *in) {
  uint32_t bits = protocol::get32(in);
  float v;
  std::memcpy(&v, &bits, 4);
  return v;
}
inline void encode(const Packet &s, uint8_t *out) {
  std::memset(out, 0, size);
  out[0] = 'P'; out[1] = 'S'; out[2] = version;
  out[3] = s.mode;
  protocol::put16(out + 4, size);
  protocol::put32(out + 8, s.sequence);
  protocol::put32(out + 12, s.uptimeMs);
  protocol::put32(out + 16, s.dtUs);
  protocol::put32(out + 20, s.imuErrors);
  for (unsigned j = 0; j < 3; ++j) {
    const float values[] = {s.axis[j].setpoint, s.axis[j].measured,
      s.axis[j].error, s.axis[j].p, s.axis[j].i, s.axis[j].d,
      s.axis[j].output, s.axis[j].rateTarget, s.axis[j].measuredRate};
    for (unsigned k = 0; k < 9; ++k) putFloat(out + 24 + 36 * j + 4 * k, values[k]);
  }
  for (unsigned k = 0; k < 4; ++k) {
    putFloat(out + 132 + 4 * k, s.motor[k]);
    putFloat(out + 172 + 4 * k, s.quaternion[k]);
  }
  for (unsigned k = 0; k < 3; ++k) {
    putFloat(out + 148 + 4 * k, s.accel[k]);
    putFloat(out + 160 + 4 * k, s.gyro[k]);
  }
  out[188] = s.calibration;
  out[189] = s.sensorValid;
  // Bytes 190–191 reserved zero: physical ESC output is impossible in this image.
  protocol::put16(out + 192, protocol::crc(out, 192));
}
inline bool decode(const uint8_t *in, size_t n, Packet &out) {
  if (!in || n != size || in[0] != 'P' || in[1] != 'S' ||
      in[2] != version || in[3] > 2 || protocol::get16(in + 4) != size ||
      in[6] || in[7] || in[189] > 1 || in[190] || in[191] ||
      protocol::get16(in + 192) != protocol::crc(in, 192)) return false;
  Packet s;
  s.mode = in[3]; s.sequence = protocol::get32(in + 8);
  s.uptimeMs = protocol::get32(in + 12); s.dtUs = protocol::get32(in + 16);
  s.imuErrors = protocol::get32(in + 20);
  for (unsigned j = 0; j < 3; ++j) {
    float *values[] = {&s.axis[j].setpoint, &s.axis[j].measured,
      &s.axis[j].error, &s.axis[j].p, &s.axis[j].i, &s.axis[j].d,
      &s.axis[j].output, &s.axis[j].rateTarget, &s.axis[j].measuredRate};
    for (unsigned k = 0; k < 9; ++k) *values[k] = getFloat(in + 24 + 36 * j + 4 * k);
  }
  for (unsigned k = 0; k < 4; ++k) {
    s.motor[k] = getFloat(in + 132 + 4 * k);
    s.quaternion[k] = getFloat(in + 172 + 4 * k);
  }
  for (unsigned k = 0; k < 3; ++k) {
    s.accel[k] = getFloat(in + 148 + 4 * k);
    s.gyro[k] = getFloat(in + 160 + 4 * k);
  }
  s.calibration = in[188]; s.sensorValid = in[189];
  for (const auto &axis : s.axis) {
    for (float value : {axis.setpoint, axis.measured, axis.error, axis.p, axis.i,
                        axis.d, axis.output, axis.rateTarget, axis.measuredRate})
      if (!std::isfinite(value)) return false;
  }
  for (float value : s.motor)
    if (!std::isfinite(value) || value < 0 || value > 1 || (s.mode != 1 && value != 0))
      return false;
  for (float value : s.accel) if (!std::isfinite(value)) return false;
  for (float value : s.gyro) if (!std::isfinite(value)) return false;
  for (float value : s.quaternion) if (!std::isfinite(value)) return false;
  out = s;
  return true;
}
} // namespace shadow_wire
