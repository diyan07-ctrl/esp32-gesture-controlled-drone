#pragma once
#include <array>
#include <cmath>
#include <algorithm>
namespace flight {
inline float clamp(float x,float a,float b){return std::max(a,std::min(b,x));}
inline float deadband(float x,float d=0.08f){if(!std::isfinite(x)||!std::isfinite(d)||d<0||d>=1)return 0;return std::fabs(x)<=d?0:std::copysign(clamp((std::fabs(x)-d)/(1-d),0,1),x);}
inline float gesture(float angle,float neutral){return deadband(clamp((angle-neutral)/30.0f,-1,1));}
struct Sample{float ax=0,ay=0,az=1,gx=0,gy=0,gz=0;};
inline bool valid(const Sample&s){return std::isfinite(s.ax)&&std::isfinite(s.ay)&&std::isfinite(s.az)&&std::isfinite(s.gx)&&std::isfinite(s.gy)&&std::isfinite(s.gz)&&std::fabs(s.ax)<7.95f&&std::fabs(s.ay)<7.95f&&std::fabs(s.az)<7.95f&&std::fabs(s.gx)<495&&std::fabs(s.gy)<495&&std::fabs(s.gz)<495;}
inline bool validDt(float dt){return std::isfinite(dt)&&dt>=0.002f&&dt<=0.006f;}
struct Estimator{
 float roll=0,pitch=0,yaw=0;bool initialized=false;
 bool update(const Sample&s,float dt){if(!valid(s)||!validDt(dt))return false;
  float norm=std::sqrt(s.ax*s.ax+s.ay*s.ay+s.az*s.az);
  bool gravity=norm>0.85f&&norm<1.15f;
  float ar=std::atan2(s.ay,s.az)*57.2957795f;
  float ap=std::atan2(-s.ax,std::sqrt(s.ay*s.ay+s.az*s.az))*57.2957795f;
  if(!initialized){if(!gravity)return false;roll=ar;pitch=ap;initialized=true;}
  // Euler kinematics with body rates in FRD. Limit use away from gimbal lock.
  if(std::fabs(pitch)>60)return false;
  float r=roll/57.2957795f,p=pitch/57.2957795f;
  float rd=s.gx+std::sin(r)*std::tan(p)*s.gy+std::cos(r)*std::tan(p)*s.gz;
  float pd=std::cos(r)*s.gy-std::sin(r)*s.gz;
  float yd=(std::sin(r)*s.gy+std::cos(r)*s.gz)/std::cos(p);
  roll+=rd*dt;pitch+=pd*dt;yaw=std::remainder(yaw+yd*dt,360.0f);
  if(gravity){float a=0.5f/(0.5f+dt);roll=a*roll+(1-a)*ar;pitch=a*pitch+(1-a)*ap;}
  return std::isfinite(roll)&&std::isfinite(pitch)&&std::isfinite(yaw);
 }
};
struct Pid{
 // Bench starting values only. Units: normalized output per degree/s and seconds.
 float kp=0.001f,ki=0.0002f,kd=0.00001f,limit=0.2f;
 float integral=0,previous=0,derivative=0;bool initialized=false;
 void reset(){integral=previous=derivative=0;initialized=false;}
 float update(float target,float measured,float dt,bool freeze=false){
  if(!std::isfinite(target)||!std::isfinite(measured)||!validDt(dt)){reset();return 0;}
  float error=target-measured;
  float raw=initialized?-(measured-previous)/dt:0;previous=measured;initialized=true;
  derivative+=dt/(0.02f+dt)*(raw-derivative);
  float candidate=clamp(integral+ki*error*dt,-limit,limit);
  float output=kp*error+candidate+kd*derivative;
  if(!freeze&&(std::fabs(output)<=limit||output*error<0))integral=candidate;
  return clamp(kp*error+integral+kd*derivative,-limit,limit);
 }
};
struct Mix{std::array<float,4> motor{};bool valid=false,saturated=false;};
inline Mix mix(float t,float r,float p,float y){
 Mix m;if(!std::isfinite(t)||!std::isfinite(r)||!std::isfinite(p)||!std::isfinite(y)||t<0||t>1)return m;
 // FRD axes, positive roll right-wing-down, pitch nose-up, yaw clockwise from above.
 // M1 FL CW; M2 FR CCW; M3 RR CW; M4 RL CCW. Reaction yaw opposes spin.
 m.motor={t+r+p-y,t-r+p+y,t-r-p-y,t+r-p+y};
 for(float&v:m.motor){if(!std::isfinite(v))return Mix{};if(v<0||v>1)m.saturated=true;v=clamp(v,0,1);}m.valid=true;return m;
}
inline float batteryVoltage(float millivolts,float ratio=5.7f,float scale=1){return millivolts*0.001f*ratio*scale;}
inline bool batteryValid(float v){return std::isfinite(v)&&v>=6&&v<=13.2f;}
}
