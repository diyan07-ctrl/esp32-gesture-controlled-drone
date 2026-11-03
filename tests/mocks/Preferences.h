#pragma once
#include <cstddef>
#include <map>
#include <string>
namespace mock_nvs {
inline std::map<std::string, float> values;
inline unsigned writes = 0, failAt = 0;
inline bool openFail = false;
inline bool writeAllowed() {
  return ++writes != failAt;
}
} // namespace mock_nvs
struct Preferences {
  bool begin(const char *, bool) {
    return !mock_nvs::openFail;
  }
  void end() {}
  float getFloat(const char *k, float fallback) {
    auto p = mock_nvs::values.find(k);
    return p == mock_nvs::values.end() ? fallback : p->second;
  }
  bool getBool(const char *k, bool fallback) {
    return getFloat(k, fallback ? 1 : 0) != 0;
  }
  size_t putFloat(const char *k, float v) {
    if (!mock_nvs::writeAllowed())
      return 0;
    mock_nvs::values[k] = v;
    return sizeof(float);
  }
  size_t putBool(const char *k, bool v) {
    if (!mock_nvs::writeAllowed())
      return 0;
    mock_nvs::values[k] = v ? 1 : 0;
    return 1;
  }
  bool clear() {
    if (!mock_nvs::writeAllowed())
      return false;
    mock_nvs::values.clear();
    return true;
  }
};
