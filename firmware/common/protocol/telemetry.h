#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include "packet.h"
namespace telemetry {
constexpr uint8_t version = 2, firmwareVersion = 2;
constexpr size_t size = 142;
enum Flag : uint16_t {
  ImuInitialized = 1, Calibrated = 2, EstimateValid = 4,
  EspNowInitialized = 8, PeerConfigured = 16, WifiConnected = 32,
  LinkFresh = 64, OutputsEnabled = 128, Bno055 = 256, SensorValid = 512
};
struct Snapshot {
  uint16_t flags = 0;
  uint8_t state = 0, channel = 0, gesture = 0, commandFlags = 0;
  uint32_t sequence = 0, uptimeMs = 0, dtUs = 0, executionUs = 0, maximumUs = 0;
  uint32_t missed = 0, imuErrors = 0, accepted = 0, rejected = 0, lost = 0;
  uint32_t mailboxDropped = 0, sendErrors = 0, commandSequence = 0;
  uint32_t commandAgeMs = 0xffffffffu, wifiReconnects = 0;
  uint16_t throttle = 0, flex[2] = {};
  int16_t rollCommand = 0, pitchCommand = 0, yawCommand = 0;
  float accel[3] = {}, gyro[3] = {}, roll = 0, pitch = 0, yawRate = 0;
  float quaternion[4] = {};
  uint8_t calibration = 0, systemStatus = 0, systemError = 0;
};
static_assert(sizeof(float) == 4, "Telemetry requires IEEE-754 binary32");
inline void putFloat(uint8_t *p, float value) {
  uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  protocol::put32(p, bits);
}
inline float getFloat(const uint8_t *p) {
  uint32_t bits = protocol::get32(p);
  float value;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}
inline void encode(const Snapshot &s, uint8_t *p) {
  std::memset(p, 0, size);
  p[0] = 'F'; p[1] = 'T'; p[2] = version; p[3] = firmwareVersion;
  protocol::put16(p + 4, size); protocol::put16(p + 6, s.flags);
  protocol::put32(p + 8, s.sequence); protocol::put32(p + 12, s.uptimeMs);
  p[16] = s.state; p[17] = s.channel; p[18] = s.gesture; p[19] = s.commandFlags;
  const uint32_t counters[] = {s.dtUs, s.executionUs, s.maximumUs, s.missed, s.imuErrors,
    s.accepted, s.rejected, s.lost, s.mailboxDropped, s.sendErrors,
    s.commandSequence, s.commandAgeMs};
  for (unsigned i = 0; i < 12; ++i) protocol::put32(p + 20 + 4 * i, counters[i]);
  protocol::put16(p + 68, s.throttle);
  protocol::put16(p + 70, uint16_t(s.rollCommand));
  protocol::put16(p + 72, uint16_t(s.pitchCommand));
  protocol::put16(p + 74, uint16_t(s.yawCommand));
  protocol::put16(p + 76, s.flex[0]); protocol::put16(p + 78, s.flex[1]);
  for (unsigned i = 0; i < 3; ++i) {
    putFloat(p + 80 + 4 * i, s.accel[i]);
    putFloat(p + 92 + 4 * i, s.gyro[i]);
  }
  putFloat(p + 104, s.roll); putFloat(p + 108, s.pitch);
  putFloat(p + 112, s.yawRate);
  protocol::put32(p + 116, s.wifiReconnects);
  for (unsigned i = 0; i < 4; ++i) putFloat(p + 120 + 4 * i, s.quaternion[i]);
  p[136] = s.calibration; p[137] = s.systemStatus; p[138] = s.systemError;
  protocol::put16(p + 140, protocol::crc(p, 140));
}
inline bool decode(const uint8_t *p, size_t n, Snapshot &out) {
  if (!p || n != size || p[0] != 'F' || p[1] != 'T' || p[2] != version ||
      p[3] != firmwareVersion || protocol::get16(p + 4) != size ||
      protocol::get16(p + 140) != protocol::crc(p, 140) ||
      (protocol::get16(p + 6) & ~uint16_t(895)) ||
      (protocol::get16(p + 6) & OutputsEnabled) || p[16] > 8 || p[17] > 14 ||
      p[18] > uint8_t(protocol::Gesture::Emergency) || (p[19] & ~7) ||
      protocol::get16(p + 68) > 1000 || protocol::get16(p + 76) > 4095 ||
      protocol::get16(p + 78) > 4095)
    return false;
  Snapshot s;
  s.flags = protocol::get16(p + 6); s.sequence = protocol::get32(p + 8);
  s.uptimeMs = protocol::get32(p + 12);
  s.state = p[16]; s.channel = p[17]; s.gesture = p[18]; s.commandFlags = p[19];
  uint32_t *counters[] = {&s.dtUs, &s.executionUs, &s.maximumUs, &s.missed, &s.imuErrors,
    &s.accepted, &s.rejected, &s.lost, &s.mailboxDropped, &s.sendErrors,
    &s.commandSequence, &s.commandAgeMs};
  for (unsigned i = 0; i < 12; ++i) *counters[i] = protocol::get32(p + 20 + 4 * i);
  s.throttle = protocol::get16(p + 68);
  s.rollCommand = int16_t(protocol::get16(p + 70));
  s.pitchCommand = int16_t(protocol::get16(p + 72));
  s.yawCommand = int16_t(protocol::get16(p + 74));
  if (s.rollCommand < -1000 || s.rollCommand > 1000 ||
      s.pitchCommand < -1000 || s.pitchCommand > 1000 ||
      s.yawCommand < -1000 || s.yawCommand > 1000) return false;
  for (unsigned i = 0; i < 2; ++i) s.flex[i] = protocol::get16(p + 76 + 2 * i);
  for (unsigned i = 0; i < 3; ++i) {
    s.accel[i] = getFloat(p + 80 + 4 * i);
    s.gyro[i] = getFloat(p + 92 + 4 * i);
    if (!std::isfinite(s.accel[i]) || !std::isfinite(s.gyro[i])) return false;
  }
  s.roll = getFloat(p + 104); s.pitch = getFloat(p + 108);
  s.yawRate = getFloat(p + 112); s.wifiReconnects = protocol::get32(p + 116);
  for (unsigned i = 0; i < 4; ++i) s.quaternion[i] = getFloat(p + 120 + 4 * i);
  s.calibration = p[136]; s.systemStatus = p[137]; s.systemError = p[138];
  if (!std::isfinite(s.roll) || !std::isfinite(s.pitch) || !std::isfinite(s.yawRate)) return false;
  for (float q : s.quaternion) if (!std::isfinite(q)) return false;
  out = s;
  return true;
}
} // namespace telemetry
