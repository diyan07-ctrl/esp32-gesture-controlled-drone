#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
namespace protocol {
constexpr uint8_t version=1, enabled=1, arm=2, stop=4;
// Explicit little-endian encoding, no packed structs or native float ABI on wire.
constexpr size_t commandSize=24, statusSize=24;
inline uint16_t get16(const uint8_t*p){return uint16_t(p[0])|(uint16_t(p[1])<<8);}
inline uint32_t get32(const uint8_t*p){return uint32_t(get16(p))|(uint32_t(get16(p+2))<<16);}
inline void put16(uint8_t*p,uint16_t x){p[0]=x;p[1]=x>>8;}
inline void put32(uint8_t*p,uint32_t x){put16(p,x);put16(p+2,x>>16);}
inline uint16_t crc(const uint8_t*p,size_t n){uint16_t c=0xffff;while(n--){c^=uint16_t(*p++)<<8;for(int i=0;i<8;i++)c=(c&0x8000)?uint16_t((c<<1)^0x1021):uint16_t(c<<1);}return c;}
struct Command{uint32_t session=0,seq=0; uint16_t throttle=0; int16_t roll=0,pitch=0,yaw=0; uint8_t flags=0;};
inline void encode(const Command&c,uint8_t*p){std::memset(p,0,commandSize);p[0]=0x47;p[1]=version;p[2]=1;p[3]=c.flags;put32(p+4,c.session);put32(p+8,c.seq);put16(p+12,c.throttle);put16(p+14,uint16_t(c.roll));put16(p+16,uint16_t(c.pitch));put16(p+18,uint16_t(c.yaw));put16(p+22,crc(p,22));}
inline bool decode(const uint8_t*p,size_t n,Command&out){
 if(n!=commandSize||p[0]!=0x47||p[1]!=version||p[2]!=1||(p[3]&~7)||get16(p+20)||get16(p+22)!=crc(p,22))return false;
 Command c; c.flags=p[3];c.session=get32(p+4);c.seq=get32(p+8);c.throttle=get16(p+12);c.roll=int16_t(get16(p+14));c.pitch=int16_t(get16(p+16));c.yaw=int16_t(get16(p+18));
 if(!c.session||c.throttle>1000||c.roll < -1000||c.roll>1000||c.pitch < -1000||c.pitch>1000||c.yaw < -1000||c.yaw>1000)return false;
 out=c;return true;
}
inline bool newer(uint32_t a,uint32_t b){const uint32_t d=a-b;return d!=0&&d<0x80000000u;}
struct Receiver {
 Command command{};bool have=false;uint32_t received=0,rejected=0,lost=0;
 bool accept(const uint8_t*p,size_t n,uint32_t now,bool armed){Command c;if(!decode(p,n,c)){++rejected;return false;}
  if(have&&c.session!=command.session){if(armed||c.throttle||c.flags){++rejected;return false;}have=false;}
  if(!have && (c.flags||c.throttle)){++rejected;return false;}
  if(have&&!newer(c.seq,command.seq)){++rejected;return false;}
  if(have){uint32_t gap=c.seq-command.seq;lost+=gap-1;}
  command=c;received=now;have=true;return true;
 }
 bool fresh(uint32_t now,uint32_t limit=200)const{return have&&uint32_t(now-received)<=limit;}
};
struct Status{uint32_t session=0,ack=0;uint8_t state=0;uint16_t faults=0,millivolts=0,age=0;uint32_t rejected=0;};
inline void encodeStatus(const Status&s,uint8_t*p){std::memset(p,0,statusSize);p[0]=0x47;p[1]=version;p[2]=2;p[3]=s.state;put32(p+4,s.session);put32(p+8,s.ack);put16(p+12,s.faults);put16(p+14,s.millivolts);put16(p+16,s.age);put32(p+18,s.rejected);put16(p+22,crc(p,22));}
inline bool decodeStatus(const uint8_t*p,size_t n,Status&s){if(n!=statusSize||p[0]!=0x47||p[1]!=version||p[2]!=2||get16(p+22)!=crc(p,22))return false;s.state=p[3];s.session=get32(p+4);s.ack=get32(p+8);s.faults=get16(p+12);s.millivolts=get16(p+14);s.age=get16(p+16);s.rejected=get32(p+18);return true;}
}
