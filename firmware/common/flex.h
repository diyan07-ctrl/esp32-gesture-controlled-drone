#pragma once
#include "control.h"
namespace flight {
// Endpoints are measured ADC counts, never invented defaults. Inverted wiring is supported.
struct FlexCalibration {
  float open = 0, bent = 0;
  bool measured = false;
  bool valid() const {
    return measured && std::isfinite(open) && std::isfinite(bent) && open > 10 && open < 4085 &&
           bent > 10 && bent < 4085 && std::fabs(bent - open) >= 100;
  }
  bool normalize(float raw, float &out) const {
    if (!valid() || !std::isfinite(raw) || raw <= 10 || raw >= 4085)
      return false;
    out = clamp((raw - open) / (bent - open), 0, 1);
    return true;
  }
};
struct FlexFilter {
  float tau = cfg::flexTau;
  float value = 0;
  bool initialized = false;
  bool update(float raw, const FlexCalibration &cal, float dt) {
    float v;
    if (!cal.normalize(raw, v) || !validDt(dt) || !std::isfinite(tau) || tau <= 0) {
      initialized = false;
      return false;
    }
    if (!initialized) {
      value = v;
      initialized = true;
    } else
      value += dt / (tau + dt) * (v - value);
    return true;
  }
};
} // namespace flight
