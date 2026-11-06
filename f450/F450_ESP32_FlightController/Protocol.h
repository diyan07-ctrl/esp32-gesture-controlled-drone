#pragma once
#include <ArduinoJson.h>
#include "FlightSupport.h"
inline bool envelope(JsonDocument &d) {
  return d["seq"].is<uint32_t>() && d["type"].is<const char *>();
}
struct DatagramReader {
  const char *data;
  size_t size, pos = 0;
  int read() {
    return pos < size ? (unsigned char)data[pos++] : -1;
  }
  size_t readBytes(char *out, size_t count) {
    size_t n = 0;
    while (n < count && pos < size)
      out[n++] = data[pos++];
    return n;
  }
};
inline bool parseEnvelope(JsonDocument &d, const char *data, size_t size) {
  DatagramReader reader{data, size};
  if (deserializeJson(d, reader) || !envelope(d))
    return false;
  while (reader.pos < size) {
    char c = data[reader.pos++];
    if (c != ' ' && c != '\r' && c != '\n' && c != '\t')
      return false;
  }
  return true;
}
inline bool number(JsonVariant v, float lo, float hi, float &out) {
  if (!v.is<float>())
    return false;
  out = v.as<float>();
  return isfinite(out) && out >= lo && out <= hi;
}
inline bool parsePilot(JsonDocument &d, float *pilot, float &throttle) {
  float values[4], t = throttle;
  if (!number(d["roll"], -1, 1, values[0]) || !number(d["pitch"], -1, 1, values[1]) ||
      !number(d["yaw"], -1, 1, values[2]) || !number(d["climb"], -1, 1, values[3]))
    return false;
  if (d.containsKey("throttle") && !number(d["throttle"], 1180, 1800, t))
    return false;
  for (int i = 0; i < 4; i++)
    pilot[i] = values[i];
  throttle = t;
  return true;
}
inline bool parseGains(JsonDocument &d, Gain *result) {
  Gain candidate[6];
  JsonArray a = d["gains"].as<JsonArray>();
  if (a.size() != 6)
    return false;
  for (int i = 0; i < 6; i++)
    if (a[i].size() != 3 || !number(a[i][0], 0, 100, candidate[i].p) ||
        !number(a[i][1], 0, 100, candidate[i].i) || !number(a[i][2], 0, 100, candidate[i].d))
      return false;
  for (int i = 0; i < 6; i++)
    result[i] = candidate[i];
  return true;
}
