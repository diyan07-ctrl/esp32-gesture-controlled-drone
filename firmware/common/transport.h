#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "protocol/packet.h"
#if __has_include("local_secrets.h")
#include "local_secrets.h"
#else
namespace pairing {constexpr bool configured=false;constexpr uint8_t peer[6]={};constexpr uint8_t pmk[16]={};constexpr uint8_t lmk[16]={};}
#endif
namespace radio {
struct Message {uint8_t bytes[24];uint32_t received;};
inline QueueHandle_t inbox=nullptr;
inline volatile bool busy=false;
inline uint32_t dropped=0,sendErrors=0;
inline bool ready=false;
inline void received(const uint8_t*mac,const uint8_t*data,int len){
 if(len!=24||memcmp(mac,pairing::peer,6)){++dropped;return;}
 Message m{};memcpy(m.bytes,data,24);m.received=millis();
 // A one-slot latest-packet mailbox bounds work and memory. Validation is outside callback.
 xQueueOverwrite(inbox,&m);
}
inline void sent(const uint8_t*,esp_now_send_status_t status){if(status!=ESP_NOW_SEND_SUCCESS)++sendErrors;busy=false;}
inline bool begin(){if(!pairing::configured)return false;
 inbox=xQueueCreate(1,sizeof(Message));if(!inbox)return false;
 WiFi.mode(WIFI_STA);WiFi.setSleep(false);
 if(esp_wifi_set_channel(1,WIFI_SECOND_CHAN_NONE)!=ESP_OK||esp_now_init()!=ESP_OK||esp_now_set_pmk(pairing::pmk)!=ESP_OK)return false;
 esp_now_peer_info_t p{};memcpy(p.peer_addr,pairing::peer,6);memcpy(p.lmk,pairing::lmk,16);p.channel=1;p.ifidx=WIFI_IF_STA;p.encrypt=true;
 ready=esp_now_add_peer(&p)==ESP_OK&&esp_now_register_recv_cb(received)==ESP_OK&&esp_now_register_send_cb(sent)==ESP_OK;return ready;
}
inline bool receive(Message&m){return ready&&xQueueReceive(inbox,&m,0)==pdTRUE;}
inline bool send(const uint8_t*p){if(!ready||busy)return false;busy=true;if(esp_now_send(pairing::peer,p,24)!=ESP_OK){busy=false;++sendErrors;return false;}return true;}
}
