#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <atomic>
#include "configuration/config.h"
#include "protocol/packet.h"
#if __has_include("local_secrets.h")
#include "local_secrets.h"
#else
namespace pairing {
constexpr bool configured = false;
constexpr uint8_t peer[6] = {};
constexpr uint8_t pmk[16] = {};
constexpr uint8_t lmk[16] = {};
} // namespace pairing
#endif
namespace radio {
struct Message {
  uint8_t bytes[protocol::commandSize];
  size_t length = 0;
  uint32_t received;
};
inline QueueHandle_t inbox = nullptr;
inline QueueHandle_t emergencyInbox = nullptr;
inline std::atomic<bool> busy{false};
inline std::atomic<uint32_t> dropped{0}, sendErrors{0};
inline bool ready = false, initialized = false;
inline void received(const uint8_t *mac, const uint8_t *data, int len) {
  if ((len != int(protocol::commandSize) && len != int(protocol::statusSize)) ||
      memcmp(mac, pairing::peer, 6)) {
    ++dropped;
    return;
  }
  Message m{};
  memcpy(m.bytes, data, len);
  m.length = size_t(len);
  m.received = millis();
  protocol::Command c;
  if (protocol::decode(data, len, c) && (c.flags & protocol::stop)) {
    xQueueOverwrite(emergencyInbox, &m);
    return;
  }
  // A one-slot latest-packet mailbox bounds work and memory. Validation is outside callback.
  xQueueOverwrite(inbox, &m);
}
inline void sent(const uint8_t *, esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS)
    ++sendErrors;
  busy = false;
}
inline bool begin() {
  if (initialized)
    return ready;
  inbox = xQueueCreate(1, sizeof(Message));
  emergencyInbox = xQueueCreate(1, sizeof(Message));
  if (!inbox || !emergencyInbox)
    return false;
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  if (WiFi.status() != WL_CONNECTED &&
      esp_wifi_set_channel(cfg::radioChannel, WIFI_SECOND_CHAN_NONE) != ESP_OK)
    return false;
  uint8_t channel = 0;
  wifi_second_chan_t secondary = WIFI_SECOND_CHAN_NONE;
  if (esp_wifi_get_channel(&channel, &secondary) != ESP_OK ||
      channel != cfg::radioChannel || esp_now_init() != ESP_OK)
    return false;
  initialized = true;
  if (!pairing::configured)
    return false;
  if (esp_now_set_pmk(pairing::pmk) != ESP_OK)
    return false;
  esp_now_peer_info_t p{};
  memcpy(p.peer_addr, pairing::peer, 6);
  memcpy(p.lmk, pairing::lmk, 16);
  p.channel = cfg::radioChannel;
  p.ifidx = WIFI_IF_STA;
  p.encrypt = true;
  ready = esp_now_add_peer(&p) == ESP_OK && esp_now_register_recv_cb(received) == ESP_OK &&
          esp_now_register_send_cb(sent) == ESP_OK;
  return ready;
}
inline bool channelCompatible() {
  uint8_t channel = 0;
  wifi_second_chan_t secondary = WIFI_SECOND_CHAN_NONE;
  return esp_wifi_get_channel(&channel, &secondary) == ESP_OK &&
         channel == cfg::radioChannel;
}
inline bool receive(Message &m) {
  return ready &&
         (xQueueReceive(emergencyInbox, &m, 0) == pdTRUE || xQueueReceive(inbox, &m, 0) == pdTRUE);
}
inline bool send(const uint8_t *p, size_t n) {
  if (!ready || busy.exchange(true))
    return false;
  if (esp_now_send(pairing::peer, p, n) != ESP_OK) {
    busy = false;
    ++sendErrors;
    return false;
  }
  return true;
}
} // namespace radio
