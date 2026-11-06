#pragma once
#include "FlightSupport.h"
enum class FlightState {
  BOOT,
  SENSOR_INITIALIZATION,
  STARTUP_REFERENCE,
  DISARMED,
  ARMING_CHECKS,
  ARMED,
  FAILSAFE,
  FAULT
};
struct SafetyLatch {
  FlightState state = FlightState::BOOT;
  bool rangeRecovery = false;
  const char *block() const {
    if (state == FlightState::FAULT)
      return "FAULT_REBOOT_REQUIRED";
    if (state == FlightState::FAILSAFE)
      return "FAILSAFE_CLEAR_REQUIRED";
    if (state == FlightState::ARMED)
      return "ALREADY_ARMED";
    if (state != FlightState::DISARMED && state != FlightState::ARMING_CHECKS)
      return "INITIALIZATION_PENDING";
    return "NONE";
  }
  void stop() {
    if (state != FlightState::FAULT && state != FlightState::FAILSAFE &&
        state != FlightState::BOOT && state != FlightState::SENSOR_INITIALIZATION &&
        state != FlightState::STARTUP_REFERENCE)
      state = FlightState::DISARMED;
  }
  void fail(bool range) {
    if (state != FlightState::FAULT) {
      state = FlightState::FAILSAFE;
      rangeRecovery = rangeRecovery || range;
    }
  }
  void fault() {
    state = FlightState::FAULT;
  }
  bool clear(bool inputsHealthy, bool rangeHealthy) {
    if (state != FlightState::FAILSAFE || !inputsHealthy || (rangeRecovery && !rangeHealthy))
      return false;
    state = FlightState::DISARMED;
    rangeRecovery = false;
    return true;
  }
  bool arm(bool inputsHealthy) {
    if ((state != FlightState::DISARMED && state != FlightState::ARMING_CHECKS) || !inputsHealthy)
      return false;
    state = FlightState::ARMING_CHECKS;
    state = FlightState::ARMED;
    return true;
  }
};
struct CommandOrder {
  bool set = false, exhausted = false;
  uint32_t last = 0, token = 1;
  bool accept(uint32_t seq) {
    if (set && !newer(seq, last))
      return false;
    set = true;
    last = seq;
    return true;
  }
  void invalidate() {
    if (token == UINT32_MAX)
      exhausted = true;
    else
      ++token;
  }
  // A STOP never moves the owner's high-water mark backwards.
  void observeStop(uint32_t seq, bool owner) {
    if (owner && (!set || newer(seq, last))) {
      set = true;
      last = seq;
    }
  }
  void release() {
    set = false;
    invalidate();
  }
  bool tokenMatches(uint32_t value) const {
    return !exhausted && value == token;
  }
};
inline bool authorizeArm(SafetyLatch &safety, CommandOrder &order, uint32_t token, bool healthy) {
  if (!order.tokenMatches(token) || !safety.arm(healthy))
    return false;
  order.invalidate();
  return true;
}
