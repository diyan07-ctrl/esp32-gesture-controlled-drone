#pragma once
#include <Arduino.h>
#include <driver/rmt.h>
#include "range.h"
#include "configuration/config.h"
// Supplementary only. No pulseIn and no waiting for an echo in the flight task.
namespace ultrasonic {
inline portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
inline volatile uint32_t rising = 0, pulse = 0;
inline volatile bool waiting = false, complete = false;
inline flight::RangeFilter filter;
inline void IRAM_ATTR edge() {
  uint32_t us = micros();
  portENTER_CRITICAL_ISR(&mux);
  if (waiting) {
    if (digitalRead(cfg::ultrasonicEcho))
      rising = us;
    else if (rising) {
      pulse = us - rising;
      complete = true;
      waiting = false;
    }
  }
  portEXIT_CRITICAL_ISR(&mux);
}
inline bool ready = false;
inline bool begin() {
  rmt_config_t c = RMT_DEFAULT_CONFIG_TX(gpio_num_t(cfg::ultrasonicTrigger), RMT_CHANNEL_4);
  c.clk_div = 80;
  if (rmt_config(&c) != ESP_OK || rmt_driver_install(c.channel, 0, 0) != ESP_OK)
    return false;
  pinMode(cfg::ultrasonicEcho, INPUT);
  attachInterrupt(digitalPinToInterrupt(cfg::ultrasonicEcho), edge, CHANGE);
  ready = true;
  return true;
}
inline void poll() {
  if (!ready)
    return;
  static uint32_t last = 0, started = 0;
  uint32_t us = micros();
  portENTER_CRITICAL(&mux);
  bool done = complete;
  uint32_t width = pulse;
  complete = false;
  bool expired = waiting && uint32_t(us - started) > 30000;
  if (expired)
    waiting = false;
  portEXIT_CRITICAL(&mux);
  if (done)
    filter.accept(width, millis());
  else if (expired)
    filter.accept(0, millis());
  if (uint32_t(us - last) >= 65000) {
    last = started = us;
    portENTER_CRITICAL(&mux);
    rising = 0;
    waiting = true;
    portEXIT_CRITICAL(&mux);
    rmt_item32_t item{};
    item.level0 = 1;
    item.duration0 = 12;
    item.level1 = 0;
    item.duration1 = 1;
    if (rmt_write_items(RMT_CHANNEL_4, &item, 1, false) != ESP_OK) {
      portENTER_CRITICAL(&mux);
      waiting = false;
      portEXIT_CRITICAL(&mux);
      filter.accept(0, millis());
    }
  }
}
} // namespace ultrasonic
