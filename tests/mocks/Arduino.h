#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
struct MockSerial {
  std::string input;
  size_t available() const {
    return input.size();
  }
  char read() {
    char c = input.front();
    input.erase(0, 1);
    return c;
  }
  void println(const char *) {}
  template <class... T> void printf(const char *, T...) {}
};
inline MockSerial Serial;
