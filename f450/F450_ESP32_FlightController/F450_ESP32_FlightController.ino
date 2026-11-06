#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <Adafruit_BNO055.h>
#include <esp_task_wdt.h>
#include "Control.h"
// Supported toolchain: Arduino ESP32 2.0.17. Sensor axes: X forward, Y right, Z down.
const int pins[4] = {25, 26, 32, 33};
Adafruit_BNO055 bno(55, 0x29);
WiFiUDP udp;
Preferences nv;
SemaphoreHandle_t guard;
Gain gains[6];
PID pid[6];
Quaternion reference;
float attitude[3] = {}, absoluteAngles[3] = {}, rates[3] = {}, bias[3] = {}, pilot[4] = {};
float outputs[4] = {}, motors[4] = {1000, 1000, 1000, 1000};
float distanceCm = 0, heightCm = 0, targetCm = 0, baseThrottle = 1180;
bool ready = false, sensorOK = false, armed = false, altHold = false, ownerSet = false,
     seqSet = false;
uint32_t sensorTime = 0, rangeTime = 0, commandTime = 0, lastSequence = 0, controlTime = 0,
         overruns = 0;
const char *fault = "STARTUP";
IPAddress owner;
volatile uint32_t echoRise = 0, echoWidth = 0;
portMUX_TYPE echoMux = portMUX_INITIALIZER_UNLOCKED;
void IRAM_ATTR echoISR() {
  portENTER_CRITICAL_ISR(&echoMux);
  if (digitalRead(19))
    echoRise = micros();
  else
    echoWidth = micros() - echoRise;
  portEXIT_CRITICAL_ISR(&echoMux);
}
void pwm() {
  for (int i = 0; i < 4; i++)
    ledcWrite(i, lroundf(motors[i] * 65536.f / 20000.f));
}
void stop(const char *why) {
  armed = false;
  altHold = false;
  fault = why;
  for (auto &p : pid)
    p = PID{};
  for (float &o : outputs)
    o = 0;
  for (float &m : motors)
    m = 1000;
  pwm();
}
bool readBlock(uint8_t reg, uint8_t *data, int count) {
  Wire.beginTransmission(0x29);
  Wire.write(reg);
  if (Wire.endTransmission(false))
    return false;
  if (Wire.requestFrom((uint8_t)0x29, (uint8_t)count) != count)
    return false;
  for (int i = 0; i < count; i++)
    data[i] = Wire.read();
  return true;
}
int16_t word(uint8_t *b) {
  return (int16_t)(b[0] | (b[1] << 8));
}
void sensorTask(void *) {
  // Adafruit initialization and checked register acquisition use its default unit configuration.
  bool initialized = bno.begin(OPERATION_MODE_IMUPLUS);
  uint32_t count = 0;
  float sum[3] = {};
  Quaternion anchor;
  TickType_t tick = xTaskGetTickCount();
  for (;;) {
    uint8_t qb[8], gb[6], ab[6], status[2];
    bool valid = initialized && readBlock(0x20, qb, 8) && readBlock(0x14, gb, 6) &&
                 readBlock(0x08, ab, 6) && readBlock(0x39, status, 2) && status[0] == 5 &&
                 status[1] == 0;
    Quaternion q;
    float g[3] = {}, acc[3] = {};
    if (valid) {
      q = {word(qb) / 16384.f, word(qb + 2) / 16384.f, word(qb + 4) / 16384.f,
           word(qb + 6) / 16384.f};
      float norm = sqrtf(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
      valid = isfinite(norm) && norm > .95 && norm < 1.05;
      if (valid) {
        q.w /= norm;
        q.x /= norm;
        q.y /= norm;
        q.z /= norm;
      }
      for (int i = 0; i < 3; i++) {
        g[i] = word(gb + 2 * i) / 16.f;
        acc[i] = word(ab + 2 * i) / 100.f;
      }
    }
    xSemaphoreTake(guard, portMAX_DELAY);
    sensorOK = valid;
    if (valid) {
      sensorTime = micros();
      q.angles(absoluteAngles);
      if (!ready) {
        float normA = sqrtf(acc[0] * acc[0] + acc[1] * acc[1] + acc[2] * acc[2]);
        bool still =
            fabsf(normA - 9.80665f) < .5 && fabsf(g[0]) < 3 && fabsf(g[1]) < 3 && fabsf(g[2]) < 3;
        if (count) {
          float delta[3];
          (anchor.inverse() * q).angles(delta);
          still = still && fabsf(delta[0]) < 1 && fabsf(delta[1]) < 1 && fabsf(delta[2]) < 1;
        }
        if (!still) {
          count = 0;
          for (float &s : sum)
            s = 0;
        } else {
          if (!count)
            anchor = q;
          for (int i = 0; i < 3; i++)
            sum[i] += g[i];
          if (++count >= 200) {
            reference = q;
            for (int i = 0; i < 3; i++)
              bias[i] = sum[i] / count;
            ready = true;
            fault = "NONE";
          }
        }
      }
      if (ready) {
        (reference.inverse() * q).angles(attitude);
        for (int i = 0; i < 3; i++)
          rates[i] = g[i] - bias[i];
      }
    } else {
      count = 0;
      for (float &s : sum)
        s = 0;
      if (armed)
        stop("IMU_INVALID");
    }
    xSemaphoreGive(guard);
    vTaskDelayUntil(&tick, pdMS_TO_TICKS(10));
  }
}
void controlTask(void *) {
  TickType_t tick = xTaskGetTickCount();
  uint32_t previous = micros();
  esp_task_wdt_add(NULL);
  for (;;) {
    vTaskDelayUntil(&tick, pdMS_TO_TICKS(4));
    uint32_t now = micros();
    float dt = (now - previous) * 1e-6f;
    previous = now;
    xSemaphoreTake(guard, portMAX_DELAY);
    controlTime = now;
    if (dt > .008f) {
      overruns++;
      if (armed)
        stop("CONTROL_TIMING");
    }
    if (armed && (!sensorOK || now - sensorTime > 50000))
      stop("IMU_STALE");
    if (armed && (now - commandTime > 300000 || WiFi.softAPgetStationNum() == 0))
      stop("COMMAND_LOSS");
    if (armed && altHold && now - rangeTime > 250000)
      stop("RANGE_LOSS");
    if (armed && (fabsf(attitude[0]) > 65 || fabsf(attitude[1]) > 65))
      stop("ATTITUDE_LIMIT");
    if (armed) {
      dt = limit(dt, .001, .008);
      float dr = pid[0].step(wrap(pilot[0] * 50 - attitude[0]), gains[0], dt);
      float dp = pid[1].step(wrap(pilot[1] * 50 - attitude[1]), gains[1], dt);
      outputs[0] = pid[2].step(dr - rates[0], gains[2], dt);
      outputs[1] = pid[3].step(dp - rates[1], gains[3], dt);
      outputs[2] = pid[4].step(pilot[2] * 75 - rates[2], gains[4], dt);
      outputs[3] = 0;
      if (altHold) {
        targetCm = limit(targetCm + pilot[3] * 30 * dt, 6.03, 204.03);
        outputs[3] = pid[5].step(targetCm - heightCm, gains[5], dt);
      }
      mix(fminf(1800, baseThrottle + outputs[3]), outputs[0], outputs[1], outputs[2], motors);
      pwm();
    }
    xSemaphoreGive(guard);
    esp_task_wdt_reset();
  }
}
bool number(JsonVariant v, float lo, float hi, float &out) {
  if (!v.is<float>())
    return false;
  out = v.as<float>();
  return isfinite(out) && out >= lo && out <= hi;
}
void sendDoc(JsonDocument &d, IPAddress ip) {
  char buffer[3000];
  size_t n = serializeJson(d, buffer, sizeof(buffer));
  if (n >= sizeof(buffer))
    return;
  udp.beginPacket(ip, 4211);
  udp.write((uint8_t *)buffer, n);
  udp.endPacket();
}
void parameters(JsonDocument &d) {
  JsonArray a = d.createNestedArray("gains");
  for (auto g : gains) {
    JsonArray row = a.createNestedArray();
    row.add(g.p);
    row.add(g.i);
    row.add(g.d);
  }
  d["base"] = baseThrottle;
}
void packet() {
  int n = udp.parsePacket();
  if (!n)
    return;
  char buffer[1200];
  IPAddress ip = udp.remoteIP();
  if (n > 1199) {
    while (udp.available())
      udp.read();
    return;
  }
  n = udp.read(buffer, 1199);
  buffer[n] = 0;
  StaticJsonDocument<2048> d;
  StaticJsonDocument<2048> reply;
  reply["type"] = "ACK";
  if (deserializeJson(d, buffer, n) || !d["seq"].is<uint32_t>() || !d["type"].is<const char *>())
    return;
  uint32_t seq = d["seq"];
  const char *type = d["type"];
  reply["seq"] = seq;
  bool ok = false;
  const char *reason = "INVALID";
  xSemaphoreTake(guard, portMAX_DELAY);
  // STOP is deliberately accepted even if its sequence is old or its sender differs.
  if (!strcmp(type, "STOP") || !strcmp(type, "DISARM")) {
    stop("PILOT_STOP");
    ok = true;
  } else if (ownerSet && ip != owner) {
    reason = "OWNER_BUSY";
  } else if (!strcmp(type, "RELEASE") && !armed) {
    ownerSet = false;
    seqSet = false;
    ok = true;
  } else if (seqSet && (int32_t)(seq - lastSequence) <= 0) {
    reason = "OLD_SEQUENCE";
  } else {
    owner = ip;
    ownerSet = true;
    seqSet = true;
    lastSequence = seq;
    if (!strcmp(type, "CMD")) {
      float v[4];
      ok = number(d["roll"], -1, 1, v[0]) && number(d["pitch"], -1, 1, v[1]) &&
           number(d["yaw"], -1, 1, v[2]) && number(d["climb"], -1, 1, v[3]);
      if (ok) {
        for (int i = 0; i < 4; i++)
          pilot[i] = v[i];
        commandTime = micros();
      }
    } else if (!strcmp(type, "GET")) {
      parameters(reply);
      ok = true;
    } else if (!strcmp(type, "ARM")) {
      ok = ready && sensorOK && micros() - sensorTime < 50000 && micros() - commandTime < 300000;
      for (float v : pilot)
        ok = ok && fabsf(v) < .01;
      ok = ok && fabsf(attitude[0]) < 5 && fabsf(attitude[1]) < 5;
      if (ok) {
        for (auto &p : pid)
          p = PID{};
        armed = true;
        fault = "NONE";
      } else
        reason = "ARM_PRECONDITIONS";
    } else if (!strcmp(type, "PID")) {
      Gain candidate[6];
      JsonArray a = d["gains"].as<JsonArray>();
      ok = a.size() == 6;
      for (int i = 0; i < 6 && ok; i++)
        ok = a[i].size() == 3 && number(a[i][0], 0, 100, candidate[i].p) &&
             number(a[i][1], 0, 100, candidate[i].i) && number(a[i][2], 0, 100, candidate[i].d);
      if (ok) {
        memcpy(gains, candidate, sizeof(gains));
        for (auto &p : pid)
          p = PID{};
      }
      parameters(reply);
    } else if (!strcmp(type, "RESET")) {
      memcpy(gains, defaults, sizeof(gains));
      for (auto &p : pid)
        p = PID{};
      altHold = false;
      parameters(reply);
      ok = true;
    } else if (!strcmp(type, "BASE")) {
      float v;
      ok = !armed && number(d["value"], 1180, 1800, v);
      if (ok)
        baseThrottle = v;
    } else if (!strcmp(type, "ALT")) {
      float v;
      ok = number(d["value"], 6.03, 204.03, v) && micros() - rangeTime < 250000 && gains[5].p > 0;
      if (ok) {
        targetCm = v;
        altHold = true;
        pid[5] = PID{};
      } else
        reason = "ALT_REQUIRES_RANGE_AND_KP";
    } else if (!strcmp(type, "MANUAL")) {
      altHold = false;
      pid[5] = PID{};
      ok = true;
    } else if (!strcmp(type, "SAVE")) {
      ok = !armed;
      if (ok) {
        nv.begin("f450", false);
        ok = nv.putBytes("gains", gains, sizeof(gains)) == sizeof(gains);
        nv.end();
      }
    } else if (!strcmp(type, "RELEASE")) {
      ok = !armed;
      if (ok) {
        ownerSet = false;
        seqSet = false;
      }
    }
  }
  reply["ok"] = ok;
  reply["reason"] = ok ? "OK" : reason;
  xSemaphoreGive(guard);
  sendDoc(reply, ip);
}
void telemetry() {
  StaticJsonDocument<3000> d;
  d["type"] = "TEL";
  xSemaphoreTake(guard, portMAX_DELAY);
  auto arr = [&](const char *key, float *values, int n) {
    JsonArray a = d.createNestedArray(key);
    for (int i = 0; i < n; i++)
      a.add(values[i]);
  };
  arr("relative", attitude, 3);
  arr("absolute", absoluteAngles, 3);
  arr("gyro", rates, 3);
  arr("bias", bias, 3);
  arr("outputs", outputs, 4);
  arr("motors", motors, 4);
  parameters(d);
  d["distance"] = distanceCm;
  d["height"] = heightCm;
  d["target"] = targetCm;
  d["alt_hold"] = altHold;
  d["armed"] = armed;
  d["reference_ready"] = ready;
  d["imu_ok"] = sensorOK;
  d["imu_age_ms"] = (micros() - sensorTime) / 1000;
  d["range_ok"] = micros() - rangeTime < 250000;
  d["command_age_ms"] = (micros() - commandTime) / 1000;
  d["stations"] = WiFi.softAPgetStationNum();
  d["fault"] = fault;
  d["overruns"] = overruns;
  d["watchdog"] = "ACTIVE_1S";
  d["control_age_us"] = micros() - controlTime;
  IPAddress dest = owner;
  xSemaphoreGive(guard);
  if (ownerSet)
    sendDoc(d, dest);
}
void setup() {
  Serial.begin(115200);
  guard = xSemaphoreCreateMutex();
  memcpy(gains, defaults, sizeof(gains));
  for (int i = 0; i < 4; i++) {
    ledcSetup(i, 50, 16);
    ledcAttachPin(pins[i], i);
  }
  pwm();
  nv.begin("f450", true);
  Gain saved[6];
  if (nv.getBytesLength("gains") == sizeof(saved) &&
      nv.getBytes("gains", saved, sizeof(saved)) == sizeof(saved)) {
    bool valid = true;
    for (auto g : saved)
      valid = valid && isfinite(g.p) && isfinite(g.i) && isfinite(g.d) && g.p >= 0 && g.p <= 100 &&
              g.i >= 0 && g.i <= 100 && g.d >= 0 && g.d <= 100;
    if (valid)
      memcpy(gains, saved, sizeof(gains));
  }
  nv.end();
  Wire.begin(21, 22);
  Wire.setClock(400000);
  Wire.setTimeOut(5);
  pinMode(18, OUTPUT);
  pinMode(19, INPUT);
  attachInterrupt(digitalPinToInterrupt(19), echoISR, CHANGE);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
                    IPAddress(255, 255, 255, 0));
  WiFi.softAP("F450_DRONE", "F450Drone2026");
  udp.begin(4210);
  esp_task_wdt_init(1, true);
  xTaskCreatePinnedToCore(sensorTask, "sensors", 4096, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(controlTask, "control", 4096, NULL, 4, NULL, 1);
}
void loop() {
  static uint32_t trigger = 0, tel = 0;
  uint32_t now = micros();
  if (now - trigger >= 60000) {
    trigger = now;
    digitalWrite(18, HIGH);
    delayMicroseconds(10);
    digitalWrite(18, LOW);
  }
  uint32_t width;
  portENTER_CRITICAL(&echoMux);
  width = echoWidth;
  echoWidth = 0;
  portEXIT_CRITICAL(&echoMux);
  if (width) {
    float cm = width * .0343f / 2;
    if (cm >= 2 && cm <= 200) {
      xSemaphoreTake(guard, portMAX_DELAY);
      distanceCm = cm;
      heightCm = cm + 4.03f;
      rangeTime = micros();
      xSemaphoreGive(guard);
    }
  }
  packet();
  if (now - tel >= 100000) {
    tel = now;
    telemetry();
  }
  delay(1);
}
