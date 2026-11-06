#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <Adafruit_BNO055.h>
#include <esp_task_wdt.h>
#include <esp_arduino_version.h>
#if ESP_ARDUINO_VERSION_MAJOR != 2
#error "This firmware requires Arduino-ESP32 2.x; verified with 2.0.17"
#endif
#include "Protocol.h"
#include "Dispatcher.h"
// Supported toolchain: Arduino ESP32 2.0.17. Sensor axes: X forward, Y right, Z down.
const int pins[4] = {25, 26, 32, 33};
Adafruit_BNO055 bno(55, 0x29);
WiFiUDP udp;
Preferences nv;
SemaphoreHandle_t guard;
Gain gains[6];
PID pid[6];
StartupReference startup;
HeightEstimator height;
AltitudeController altitude;
SafetyLatch safety;
FlightState &state = safety.state;
CommandOrder order;
CommandSession session;
const char *stateName() {
  switch (state) {
  case FlightState::BOOT:
    return "BOOT";
  case FlightState::SENSOR_INITIALIZATION:
    return "SENSOR_INITIALIZATION";
  case FlightState::STARTUP_REFERENCE:
    return "STARTUP_REFERENCE";
  case FlightState::DISARMED:
    return "DISARMED";
  case FlightState::ARMING_CHECKS:
    return "ARMING_CHECKS";
  case FlightState::ARMED:
    return "ARMED";
  case FlightState::FAILSAFE:
    return "FAILSAFE";
  default:
    return "FAULT";
  }
}
float manualThrottleUs = 1180;
bool pwmOK = true;
bool watchdogOK = false;
volatile bool echoPending = false, echoHigh = false;
float attitude[3] = {}, absoluteAngles[3] = {}, rates[3] = {}, bias[3] = {}, pilot[4] = {};
float outputs[4] = {}, motors[4] = {1000, 1000, 1000, 1000};
float distanceCm = 0, heightCm = 0, targetCm = 0, baseThrottle = 1180;
bool ready = false, sensorOK = false, armed = false, altHold = false;
uint32_t sensorTime = 0, rangeTime = 0, commandTime = 0, controlTime = 0, overruns = 0;
const char *fault = "STARTUP";

const char *inputBlockReason() {
  if (armed)
    return "ALREADY_ARMED";
  if (!pwmOK)
    return "PWM_INITIALIZATION";
  if (!watchdogOK)
    return "WATCHDOG_INITIALIZATION";
  if (!ready)
    return "WAIT_STARTUP_REFERENCE";
  if (!sensorOK || !fresh(micros(), sensorTime, 50000))
    return "IMU_NOT_HEALTHY";
  if (!session.active || !session.commandSeen || !fresh(micros(), commandTime, 300000) ||
      WiFi.softAPgetStationNum() == 0)
    return "COMMAND_NOT_FRESH";
  if (manualThrottleUs > 1180)
    return "THROTTLE_NOT_IDLE";
  for (float v : pilot)
    if (fabsf(v) >= .01)
      return "PILOT_NOT_NEUTRAL";
  if (fabsf(attitude[0]) >= 5 || fabsf(attitude[1]) >= 5)
    return "ATTITUDE_NOT_LEVEL";
  return "NONE";
}
const char *armBlockReason() {
  const char *latch = safety.block();
  if (strcmp(latch, "NONE"))
    return latch;
  if (order.exhausted)
    return "ARM_TOKEN_EXHAUSTED_REBOOT";
  return inputBlockReason();
}
volatile uint32_t echoRise = 0, echoWidth = 0;
portMUX_TYPE echoMux = portMUX_INITIALIZER_UNLOCKED;
void IRAM_ATTR echoISR() {
  portENTER_CRITICAL_ISR(&echoMux);
  if (echoPending) {
    if (digitalRead(19)) {
      echoRise = micros();
      echoHigh = true;
    } else if (echoHigh) {
      echoWidth = micros() - echoRise;
      echoPending = false;
      echoHigh = false;
    }
  }
  portEXIT_CRITICAL_ISR(&echoMux);
}
void pwm() {
  uint32_t duties[4];
  bool valid = true;
  for (int i = 0; i < 4; i++)
    valid = pulseToDuty(motors[i], duties[i]) && valid;
  if (!valid) {
    armed = false;
    safety.fault();
    order.invalidate();
    fault = "INVALID_MOTOR";
    altitude.manual();
    altHold = false;
    for (int i = 0; i < 4; i++) {
      motors[i] = 1000;
      pulseToDuty(1000, duties[i]);
    }
  }
  for (int i = 0; i < 4; i++)
    ledcWrite(i, duties[i]);
}
void stop(const char *why) {
  if (!strcmp(why, "PILOT_STOP"))
    safety.stop();
  else if (!strcmp(why, "COMMAND_LOSS") || !strcmp(why, "RANGE_LOSS"))
    safety.fail(!strcmp(why, "RANGE_LOSS"));
  else
    safety.fault();
  order.invalidate();
  altitude.manual();
  altitude.collective = 1180;
  manualThrottleUs = 1180;
  armed = false;
  altHold = false;
  if (strcmp(why, "PILOT_STOP") || (state != FlightState::FAULT && state != FlightState::FAILSAFE))
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
  xSemaphoreTake(guard, portMAX_DELAY);
  if (!initialized) {
    safety.fault();
    order.invalidate();
  } else if (state != FlightState::FAULT)
    state = FlightState::STARTUP_REFERENCE;
  if (!initialized)
    fault = "IMU_INITIALIZATION";
  xSemaphoreGive(guard);
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
      valid = q.normalize();
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
      bool wasReady = ready;
      valid = startup.update(q, g, acc);
      sensorOK = valid;
      ready = startup.ready;
      if (!wasReady && ready) {
        if (state == FlightState::STARTUP_REFERENCE) {
          fault = "NONE";
          state = FlightState::DISARMED;
        }
      }
      if (ready && valid) {
        (startup.reference.inverse() * q).angles(attitude);
        for (int i = 0; i < 3; i++) {
          bias[i] = startup.bias[i];
          rates[i] = g[i] - bias[i];
        }
      }
    } else {
      if (!ready)
        startup.reset();
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
  xSemaphoreTake(guard, portMAX_DELAY);
  watchdogOK = (esp_task_wdt_add(NULL) == ESP_OK) && watchdogOK;
  if (!watchdogOK)
    stop("WATCHDOG_INITIALIZATION");
  xSemaphoreGive(guard);
  for (;;) {
    vTaskDelayUntil(&tick, pdMS_TO_TICKS(4));
    uint32_t now = micros();
    float dt = (now - previous) * 1e-6f;
    previous = now;
    xSemaphoreTake(guard, portMAX_DELAY);
    now = micros();
    controlTime = now;
    if (dt <= 0 || dt > .008f) {
      overruns++;
      if (armed)
        stop("CONTROL_TIMING");
    }
    if (armed && (!sensorOK || !fresh(now, sensorTime, 50000)))
      stop("IMU_STALE");
    if (armed && (!session.active || !session.commandSeen || !fresh(now, commandTime, 300000) ||
                  WiFi.softAPgetStationNum() == 0))
      stop("COMMAND_LOSS");
    if (armed && altHold && !height.healthy(now))
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
      if (!altitude.step(manualThrottleUs, pilot[3], gains[5], height, now, dt))
        stop("RANGE_LOSS");
      altHold = altitude.active;
      targetCm = altitude.target;
      outputs[3] = altitude.correction;
      if (armed && !mix(altitude.collective, outputs[0], outputs[1], outputs[2], motors))
        stop("INVALID_MOTOR");
      pwm();
    }
    xSemaphoreGive(guard);
    if (esp_task_wdt_reset() != ESP_OK) {
      xSemaphoreTake(guard, portMAX_DELAY);
      watchdogOK = false;
      stop("WATCHDOG_FEED_FAILED");
      xSemaphoreGive(guard);
    }
  }
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
DispatchResult applyCommand(const char *type, JsonDocument &d, JsonDocument &reply) {
  bool ok = false;
  const char *reason = "INVALID";
  if (!strcmp(type, "CMD")) {
    float v[4];
    float collective = manualThrottleUs;
    ok = parsePilot(d, v, collective);
    if (ok) {
      if (!altitude.active)
        manualThrottleUs = collective;
      for (int i = 0; i < 4; i++)
        pilot[i] = v[i];
      commandTime = micros();
    }

  } else if (!strcmp(type, "ARM")) {
    reason = armBlockReason();
    if (!strcmp(reason, "NONE")) {
      if (!d["arm_token"].is<uint32_t>() || !order.tokenMatches(d["arm_token"].as<uint32_t>()))
        reason = "STALE_ARM_TOKEN";
      else if (authorizeArm(safety, order, d["arm_token"].as<uint32_t>(), true)) {
        for (auto &p : pid)
          p = PID{};
        armed = true;
        altitude.collective = 1180;
        fault = "NONE";
        ok = true;
      }
    }
  } else if (!strcmp(type, "CLEAR_FAULT")) {
    if (!d["arm_token"].is<uint32_t>() || !order.tokenMatches(d["arm_token"].as<uint32_t>()))
      reason = "STALE_ARM_TOKEN";
    else {
      ok = safety.clear(!strcmp(inputBlockReason(), "NONE"), height.healthy(micros()));
      if (ok) {
        fault = "NONE";
        order.invalidate();
      } else
        reason = state == FlightState::FAULT ? "FAULT_REBOOT_REQUIRED"
                                             : "CLEAR_REQUIRES_HEALTHY_FAILSAFE";
    }
  } else if (!strcmp(type, "PID")) {
    Gain candidate[6];
    ok = parseGains(d, candidate);
    if (!ok)
      reason = "INVALID_PID";
    if (ok) {
      altitude.manual();
      altitude.validated = false;
      altHold = false;
      manualThrottleUs = altitude.collective;
      memcpy(gains, candidate, sizeof(gains));
      for (auto &p : pid)
        p = PID{};
    }
    parameters(reply);
  } else if (!strcmp(type, "RESET")) {
    memcpy(gains, defaults, sizeof(gains));
    for (auto &p : pid)
      p = PID{};
    altitude.manual();
    altitude.validated = false;
    manualThrottleUs = altitude.collective;
    altHold = false;
    parameters(reply);
    ok = true;
  } else if (!strcmp(type, "BASE")) {
    float v;
    ok = !armed && number(d["value"], 1180, 1800, v);
    if (ok) {
      baseThrottle = v;
      manualThrottleUs = v;
    }
  } else if (!strcmp(type, "ALT")) {
    float v;
    ok = number(d["value"], 6.03, 204.03, v);
    if (ok) {
      if (altitude.active) {
        ok = height.healthy(micros());
        if (ok)
          altitude.requestedTarget = v;
      } else {
        ok = altitude.enable(v, height, gains[5], micros());
        if (ok)
          altitude.requestedTarget = v;
      }
    }
    if (ok) {
      targetCm = v;
      altHold = true;
    } else
      reason = "ALT_REQUIRES_VALIDATED_GAINS_AND_RANGE";
  } else if (!strcmp(type, "ALT_VALIDATE")) {
    ok = !armed && gains[5].p > 0 && height.healthy(micros());
    if (ok)
      altitude.validated = true;
    else
      reason = "VALIDATE_DISARMED_WITH_GAINS_AND_RANGE";
  } else if (!strcmp(type, "MANUAL")) {
    manualThrottleUs = altitude.collective;
    altitude.manual();
    altHold = false;
    ok = true;
  } else if (!strcmp(type, "SAVE")) {
    ok = !armed;
    if (ok) {
      nv.begin("f450", false);
      ok = nv.putBytes("gains", gains, sizeof(gains)) == sizeof(gains);
      nv.end();
    }
  }
  return {ok, ok ? "OK" : reason};
}
struct FirmwareHost {
  bool isArmed() const {
    return armed;
  }
  void stop() {
    ::stop("PILOT_STOP");
  }
  void sessionChanged() {
    commandTime = 0;
  }
  void parameters(JsonDocument &reply) {
    ::parameters(reply);
  }
  DispatchResult command(const char *type, JsonDocument &d, JsonDocument &reply) {
    return applyCommand(type, d, reply);
  }
};
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
  if (n <= 0)
    return;
  StaticJsonDocument<2048> reply;
  FirmwareHost host;
  xSemaphoreTake(guard, portMAX_DELAY);
  bool respond = dispatchDatagram(buffer, n, uint32_t(ip), session, order, host, reply);
  xSemaphoreGive(guard);
  if (respond)
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
  d["requested_target"] = altitude.requestedTarget;
  d["alt_hold"] = altHold;
  d["armed"] = armed;
  d["reference_ready"] = ready;
  d["imu_ok"] = sensorOK;
  d["imu_age_ms"] = (micros() - sensorTime) / 1000;
  d["range_ok"] = height.healthy(micros());
  d["range_age_ms"] = (micros() - height.time) / 1000;
  d["range_rejected"] = height.rejected;
  d["alt_error"] = altitude.active ? altitude.error : 0;
  d["alt_configured"] = gains[5].p > 0;
  d["alt_validated"] = altitude.validated;
  d["mode"] = altitude.active ? "ALTITUDE_HOLD" : "MANUAL_THROTTLE";
  d["collective_us"] = altitude.collective;
  d["manual_throttle_us"] = manualThrottleUs;
  d["state"] = stateName();
  d["arm_token"] = order.token;
  d["range_recovery_samples"] = height.count;
  d["reference_samples"] = startup.count;
  d["battery_status"] = "NOT_INSTALLED";
  d["pwm_ok"] = pwmOK;
  d["arming_block_reason"] = armBlockReason();
  d["arming_available"] = !strcmp(armBlockReason(), "NONE");

  d["command_age_ms"] = (micros() - commandTime) / 1000;
  d["stations"] = WiFi.softAPgetStationNum();
  d["fault"] = fault;
  d["overruns"] = overruns;
  d["watchdog"] = watchdogOK ? "REGISTERED_1S" : "FAILED";
  d["control_age_us"] = micros() - controlTime;
  IPAddress dest(session.owner);
  bool hasOwner = session.active;
  d["session"] = session.generation;
  d["owner_active"] = session.active;
  d["owned"] = session.active;
  xSemaphoreGive(guard);
  if (hasOwner)
    sendDoc(d, dest);
}
void setup() {
  Serial.begin(115200);
  guard = xSemaphoreCreateMutex();
  order.token = esp_random();
  session.generation = esp_random();
  memcpy(gains, defaults, sizeof(gains));
  for (int i = 0; i < 4; i++) {
    pwmOK = (ledcSetup(i, 50, 16) > 0) && pwmOK;
    ledcAttachPin(pins[i], i);
  }
  pwm();
  if (!pwmOK) {
    safety.fault();
    fault = "PWM_INITIALIZATION";
    order.invalidate();
  }
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
  if (state != FlightState::FAULT)
    state = FlightState::SENSOR_INITIALIZATION;
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
  watchdogOK = esp_task_wdt_init(1, true) == ESP_OK;
  xTaskCreatePinnedToCore(sensorTask, "sensors", 4096, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(controlTask, "control", 4096, NULL, 4, NULL, 1);
}
void loop() {
  static uint32_t trigger = 0, tel = 0;
  uint32_t now = micros();
  if (now - trigger >= 60000) {
    trigger = now;
    portENTER_CRITICAL(&echoMux);
    echoPending = true;
    echoHigh = false;
    echoWidth = 0;
    portEXIT_CRITICAL(&echoMux);
    digitalWrite(18, HIGH);
    delayMicroseconds(10);
    digitalWrite(18, LOW);
  }
  uint32_t width;
  portENTER_CRITICAL(&echoMux);
  width = echoWidth;
  echoWidth = 0;
  portEXIT_CRITICAL(&echoMux);
  bool timeout = false;
  portENTER_CRITICAL(&echoMux);
  if (echoPending && uint32_t(now - trigger) > 15000) {
    echoPending = false;
    echoHigh = false;
    timeout = true;
  }
  portEXIT_CRITICAL(&echoMux);
  if (width || timeout) {
    xSemaphoreTake(guard, portMAX_DELAY);
    if (!ready || !sensorOK || !fresh(micros(), sensorTime, 50000))
      height.invalidate();
    else
      height.update(timeout ? NAN : width * .0343f / 2, attitude[0], attitude[1], micros());
    distanceCm = height.slant;
    heightCm = height.height;
    rangeTime = height.time;
    xSemaphoreGive(guard);
  }
  packet();
  if (now - tel >= 100000) {
    tel = now;
    telemetry();
  }
  delay(1);
}
