#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
struct MockWire {
  std::array<uint8_t, 256> registers{};
  uint8_t cursor = 0;
  bool first = true, fail = false;
  int corrupt = -1;
  void begin(int, int, unsigned) {}
  void setTimeOut(unsigned) {}
  void beginTransmission(uint8_t) {
    first = true;
  }
  void write(uint8_t v) {
    if (first) {
      cursor = v;
      first = false;
    } else
      registers[cursor++] = v;
  }
  int endTransmission(bool = true) {
    return fail ? 1 : 0;
  }
  size_t requestFrom(uint8_t, size_t n, bool) {
    return fail ? 0 : n;
  }
  uint8_t read() {
    uint8_t v = registers[cursor];
    if (cursor == corrupt)
      v ^= 1;
    ++cursor;
    return v;
  }
  void word(unsigned reg, int16_t v) {
    registers[reg] = uint16_t(v) >> 8;
    registers[reg + 1] = uint16_t(v) & 255;
  }
};
inline MockWire Wire;
