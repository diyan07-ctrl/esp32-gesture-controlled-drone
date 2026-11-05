#pragma once
#include <cstdint>
namespace bench {
enum class State : uint8_t {
  Boot, ImuInitialization, ImuCalibration, WirelessInitialization,
  WaitingForGlove, Ready, CommunicationLost, SensorFault, SystemFault
};
// Deliberately has no armed state or actuator path.
struct Inputs {
  bool imuInitialized = false, calibrated = false, sampleValid = false;
  bool radioInitialized = false, paired = false, channelCompatible = false;
  bool haveCommand = false, commandFresh = false, fatal = false;
};
inline State select(const Inputs &in) {
  if (in.fatal) return State::SystemFault;
  if (!in.imuInitialized || (in.calibrated && !in.sampleValid)) return State::SensorFault;
  if (!in.calibrated) return State::ImuCalibration;
  if (!in.radioInitialized || !in.channelCompatible) return State::WirelessInitialization;
  if (!in.paired || !in.haveCommand) return State::WaitingForGlove;
  return in.commandFresh ? State::Ready : State::CommunicationLost;
}
inline const char *name(State state) {
  switch (state) {
    case State::Boot: return "BOOT";
    case State::ImuInitialization: return "IMU_INITIALIZATION";
    case State::ImuCalibration: return "IMU_CALIBRATION";
    case State::WirelessInitialization: return "WIRELESS_INITIALIZATION";
    case State::WaitingForGlove: return "WAITING_FOR_GLOVE";
    case State::Ready: return "READY_SENSOR_LINK_ONLY";
    case State::CommunicationLost: return "COMMUNICATION_LOST";
    case State::SensorFault: return "SENSOR_FAULT";
    default: return "SYSTEM_FAULT";
  }
}
} // namespace bench
