#pragma once
#include <cstdint>
#include <cmath>
namespace flight {
struct RangeFilter {
  float meters = 0;
  bool have = false;
  uint32_t lastGood = 0, invalid = 0;
  bool accept(uint32_t pulseUs, uint32_t now) {
    float m = pulseUs * .0001715f;
    if (pulseUs == 0 || m < .02f || m > 4) {
      ++invalid;
      return false;
    }
    meters = have ? meters + .25f * (m - meters) : m;
    have = true;
    lastGood = now;
    return true;
  }
  bool healthy(uint32_t now) const {
    return have && uint32_t(now - lastGood) <= 300;
  }
};
} // namespace flight
