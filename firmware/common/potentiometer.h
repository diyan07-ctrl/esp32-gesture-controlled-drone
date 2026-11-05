#pragma once
#include "control.h"
namespace flight {
struct PotCalibration {
  float low = 0, high = 0;
  bool measured = false;
  bool valid() const {
    return measured && std::isfinite(low) && std::isfinite(high) && low > 32 && low < 4063 &&
           high > 32 && high < 4063 && std::fabs(high - low) >= cfg::potMinSpan;
  }
  bool normalize(float raw, float &out) const {
    if (!valid() || !std::isfinite(raw) || raw <= 32 || raw >= 4063 ||
        raw < std::min(low, high) - cfg::potMargin || raw > std::max(low, high) + cfg::potMargin)
      return false;
    float v = clamp((raw - low) / (high - low), 0, 1);
    out = v <= cfg::potDeadband ? 0 : (v - cfg::potDeadband) / (1 - cfg::potDeadband);
    return true;
  }
};
struct PotFilter {
  float value = 0, target = 0;
  float tau = cfg::potTau, risePerSecond = cfg::potRisePerSecond;
  bool initialized = false;
  bool update(float raw, const PotCalibration &cal, float dt) {
    if (!cal.normalize(raw, target) || !validDt(dt) || !std::isfinite(tau) || tau <= 0 ||
        !std::isfinite(risePerSecond) || risePerSecond <= 0) {
      value = target = 0;
      initialized = false;
      return false;
    }
    // Begin at zero. Reductions are immediate; rises are filtered and slew bounded.
    initialized = true;
    if (target <= value)
      value = target;
    else
      value += std::min(dt / (tau + dt) * (target - value), risePerSecond * dt);
    return true;
  }
};
// Stable endpoint averaging is shared by the calibration console and host tests.
struct EndpointCapture {
  unsigned count = 0;
  uint32_t sum = 0;
  uint16_t minimum = 4095, maximum = 0;
  bool add(uint16_t raw) {
    if (raw > 4095 || count >= cfg::endpointSamples)
      return false;
    sum += raw;
    minimum = std::min(minimum, raw);
    maximum = std::max(maximum, raw);
    ++count;
    return true;
  }
  bool valid() const {
    return count == cfg::endpointSamples && minimum > 10 && maximum < 4085 &&
           maximum - minimum <= cfg::captureMaxSpread;
  }
  float average() const {
    return count ? sum / float(count) : 0;
  }
};
} // namespace flight
