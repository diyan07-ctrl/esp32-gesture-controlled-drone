#pragma once
#include <Arduino.h>
#include "control.h"
#include "configuration/config.h"
namespace flight {
class Esc {
  bool attached = false;

public:
  bool ready() const {
    return attached;
  }
  void safeBoot() {
    for (int pin : cfg::motors)
      pinMode(pin, INPUT);
  }
  bool attach() {
    if (!(cfg::outputsVerified && cfg::powerVerified && cfg::propulsionVerified &&
          cfg::controllerValidated))
      return false;
    for (int i = 0; i < 4; i++) {
      if (ledcSetup(i, cfg::pwmHz, cfg::pwmBits) == 0) {
        for (int j = 0; j < i; j++) {
          ledcWrite(j, 0);
          ledcDetachPin(cfg::motors[j]);
          pinMode(cfg::motors[j], INPUT);
        }
        return false;
      }
      ledcWrite(i, 0);
      ledcAttachPin(cfg::motors[i], i);
    }
    attached = true;
    return true;
  }
  void write(const Mix &m, bool armed) {
    if (!attached)
      return;
    // Disarmed = no pulses. Exact ESC must be verified to stop on absent signal.
    bool validAll = m.valid;
    for (float value : m.motor)
      validAll = validAll && std::isfinite(value);
    for (int i = 0; i < 4; i++) {
      ledcWrite(i, pwmDuty(m.motor[i], armed, validAll));
    }
  }
};
} // namespace flight
