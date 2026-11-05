#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include "bench_state.h"
#include "protocol/telemetry.h"
int main() {
  unsigned checks = 0;
  uint8_t packet[telemetry::size];
  telemetry::Snapshot source;
  source.flags = telemetry::ImuInitialized | telemetry::Calibrated |
                 telemetry::EstimateValid | telemetry::WifiConnected |
                 telemetry::Bno055 | telemetry::SensorValid;
  source.state = uint8_t(bench::State::WaitingForGlove);
  source.channel = 1;
  source.sequence = 0xfffffff0u;
  source.uptimeMs = 7000;
  source.dtUs = 4000;
  source.imuErrors = 2;
  source.throttle = 500;
  source.rollCommand = -250;
  source.pitchCommand = 125;
  source.flex[0] = 1234;
  source.flex[1] = 2345;
  source.accel[0] = -.1f;
  source.accel[1] = .2f;
  source.accel[2] = 1.0f;
  source.gyro[0] = 2.5f;
  source.gyro[1] = -3.0f;
  source.gyro[2] = .5f;
  source.roll = -5.5f;
  source.pitch = 7.25f;
  source.yawRate = .5f;
  source.quaternion[0] = 1;
  source.calibration = 0x3f;
  for (unsigned i = 0; i < 1000; ++i) {
    source.sequence += 3;
    source.roll = float(i) / 17;
    telemetry::encode(source, packet);
    telemetry::Snapshot decoded;
    assert(telemetry::decode(packet, sizeof(packet), decoded)); ++checks;
    assert(decoded.sequence == source.sequence && decoded.uptimeMs == source.uptimeMs); ++checks;
    assert(decoded.flags == source.flags && decoded.state == source.state); ++checks;
    assert(decoded.throttle == 500 && decoded.rollCommand == -250 && decoded.flex[1] == 2345); ++checks;
    assert(std::fabs(decoded.roll - source.roll) < 1e-5f && decoded.yawRate == .5f); ++checks;
    assert(decoded.quaternion[0] == 1 && decoded.calibration == 0x3f); ++checks;
  }
  telemetry::Snapshot unchanged;
  unchanged.sequence = 123;
  packet[80] ^= 1;
  assert(!telemetry::decode(packet, sizeof(packet), unchanged) && unchanged.sequence == 123); ++checks;
  packet[80] ^= 1;
  assert(!telemetry::decode(packet, sizeof(packet) - 1, unchanged)); ++checks;
  packet[2] = 99;
  assert(!telemetry::decode(packet, sizeof(packet), unchanged)); ++checks;
  packet[2] = telemetry::version;
  protocol::put16(packet + 68, 1001);
  protocol::put16(packet + 140, protocol::crc(packet, 140));
  assert(!telemetry::decode(packet, sizeof(packet), unchanged)); ++checks;
  bench::Inputs input;
  assert(bench::select(input) == bench::State::SensorFault); ++checks;
  input.imuInitialized = input.sampleValid = true;
  assert(bench::select(input) == bench::State::ImuCalibration); ++checks;
  input.calibrated = true;
  assert(bench::select(input) == bench::State::WirelessInitialization); ++checks;
  input.radioInitialized = input.channelCompatible = true;
  assert(bench::select(input) == bench::State::WaitingForGlove); ++checks;
  input.paired = input.haveCommand = input.commandFresh = true;
  assert(bench::select(input) == bench::State::Ready); ++checks;
  input.commandFresh = false;
  assert(bench::select(input) == bench::State::CommunicationLost); ++checks;
  input.sampleValid = false;
  assert(bench::select(input) == bench::State::SensorFault); ++checks;
  input.fatal = true;
  assert(bench::select(input) == bench::State::SystemFault); ++checks;
  std::printf("PASS %u bench telemetry codec/state assertions\n", checks);
}
