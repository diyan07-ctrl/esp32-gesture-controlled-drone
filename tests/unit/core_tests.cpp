#include <iostream>
#include <limits>
#include <cmath>
#include "control.h"
#include "gesture.h"
#include "safety.h"
#include "protocol/packet.h"
int checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<"\n";return 1;}}while(0)
bool near(float a,float b,float tol=0.0001f){return std::fabs(a-b)<tol;}
int main(){using namespace flight;
 const float nan=std::numeric_limits<float>::quiet_NaN();
 CHECK(deadband(0)==0);CHECK(deadband(0.08f)==0);CHECK(deadband(-0.04f)==0);CHECK(near(deadband(1),1));CHECK(near(deadband(-1),-1));CHECK(deadband(nan)==0);CHECK(near(gesture(35,5),1));CHECK(gesture(5,5)==0);
 for(int axis=0;axis<3;axis++){auto m=mix(.5f,axis==0?.1f:0,axis==1?.1f:0,axis==2?.1f:0);CHECK(m.valid);CHECK(!m.saturated);
  const int signs[3][4]={{1,-1,-1,1},{1,1,-1,-1},{-1,1,-1,1}};
  for(int i=0;i<4;i++)CHECK(near(m.motor[i],.5f+.1f*signs[axis][i]));}
 CHECK(!mix(nan,0,0,0).valid);CHECK(!mix(.5f,nan,0,0).valid);CHECK(!mix(2,0,0,0).valid);
 auto clipped=mix(.95f,.2f,.2f,.2f);CHECK(clipped.valid&&clipped.saturated);for(float x:clipped.motor)CHECK(x>=0&&x<=1);
 for(int t=0;t<=100;t++){auto m=mix(t/100.0f,0,0,0);for(float x:m.motor)CHECK(near(x,t/100.0f));}
 protocol::Command c;c.session=123;c.seq=0xfffffffe;uint8_t b[24];protocol::encode(c,b);protocol::Command decoded;
 CHECK(protocol::decode(b,24,decoded));CHECK(decoded.session==123);CHECK(!protocol::decode(b,23,decoded));
 for(int i=0;i<24;i++){b[i]^=1;CHECK(!protocol::decode(b,24,decoded));b[i]^=1;}
 c.roll=1001;protocol::encode(c,b);CHECK(!protocol::decode(b,24,decoded));c.roll=0;
 c.flags=128;protocol::encode(c,b);CHECK(!protocol::decode(b,24,decoded));c.flags=0;
 protocol::Receiver rx;protocol::encode(c,b);CHECK(rx.accept(b,24,0xfffffff0u,false));CHECK(rx.fresh(0x10));CHECK(!rx.fresh(0x200));CHECK(!rx.accept(b,24,10,false));
 c.seq=0xffffffff;protocol::encode(c,b);CHECK(rx.accept(b,24,10,false));c.seq=0;protocol::encode(c,b);CHECK(rx.accept(b,24,20,false));CHECK(rx.lost==0);
 c.session=124;c.seq=1;protocol::encode(c,b);CHECK(!rx.accept(b,24,30,true));CHECK(rx.received==20);CHECK(rx.accept(b,24,30,false));
 c.session=125;c.throttle=1;protocol::encode(c,b);CHECK(!rx.accept(b,24,40,false));
 CHECK(protocol::newer(0,0xffffffff));CHECK(!protocol::newer(0x80000000u,0));
 protocol::Status status;status.session=123;status.ack=12;status.faults=8;protocol::encodeStatus(status,b);protocol::Status back;CHECK(protocol::decodeStatus(b,24,back));CHECK(back.ack==12&&back.faults==8);CHECK(!protocol::decode(b,24,decoded));
 Sample s;Estimator e;CHECK(e.update(s,.004f));CHECK(near(e.roll,0));CHECK(near(e.pitch,0));s.ay=.5f;s.az=.8660254f;Estimator tilted;CHECK(tilted.update(s,.004f));CHECK(near(tilted.roll,30,.01f));s.ax=nan;CHECK(!e.update(s,.004f));s=Sample{};CHECK(!e.update(s,0));CHECK(!e.update(s,.02f));s.gx=500;CHECK(!valid(s));
 Pid pid;pid.kp=1;pid.ki=1;for(int i=0;i<1000;i++)CHECK(near(pid.update(100,0,.004f),pid.limit));CHECK(near(pid.integral,0));pid.reset();pid.kp=0;pid.ki=1;pid.kd=0;pid.update(1,0,.004f);CHECK(pid.integral>0);float before=pid.integral;pid.update(1,0,.004f,true);CHECK(near(before,pid.integral));CHECK(pid.update(nan,0,.004f)==0);CHECK(pid.integral==0);
 CHECK(near(batteryVoltage(2210.5263f),12.6f));CHECK(!batteryValid(0));CHECK(!batteryValid(nan));CHECK(batteryValid(11.1f));
 Safety safe;CHECK(safe.state==State::BOOT);safe.begin();CHECK(safe.state==State::SENSOR_INITIALIZATION);safe.initialized(true);CHECK(safe.state==State::CALIBRATION);safe.calibrated(true);CHECK(safe.state==State::DISARMED);
 Checks ok;ok.sensor=ok.calibrated=ok.fresh=ok.enabled=ok.battery=ok.lowThrottle=ok.hardware=true;ok.arm=true;
 safe.step(ok,0);CHECK(!safe.armed());ok.arm=false;safe.step(ok,1);ok.arm=true;safe.step(ok,10);CHECK(safe.state==State::ARMING_CHECKS);safe.step(ok,1009);CHECK(!safe.armed());safe.step(ok,1010);CHECK(safe.armed());ok.fresh=false;safe.step(ok,1011);CHECK(safe.state==State::FAILSAFE);ok.fresh=true;safe.step(ok,2000);CHECK(safe.state==State::FAILSAFE);
 for(int f=0;f<7;f++){Safety a;a.begin();a.initialized(true);a.calibrated(true);Checks z=ok;z.arm=false;a.step(z,0);z.arm=true;
  if(f==0)z.hardware=false;if(f==1)z.lowThrottle=false;if(f==2)z.sensor=false;if(f==3)z.battery=false;if(f==4)z.enabled=false;if(f==5)z.calibrated=false;if(f==6)z.kill=true;
  a.step(z,1);a.step(z,2000);CHECK(!a.armed());}
 Safety wrap;wrap.begin();wrap.initialized(true);wrap.calibrated(true);ok.arm=false;wrap.step(ok,0xfffff000u);ok.arm=true;wrap.step(ok,0xffffff00u);wrap.step(ok,0x300u);CHECK(wrap.armed());ok.kill=true;wrap.step(ok,0x301u);CHECK(wrap.state==State::FAILSAFE);
 Safety fault;fault.begin();fault.initialized(false);CHECK(fault.state==State::FAULT);fault.step(ok,3);CHECK(fault.state==State::FAULT);
 Safety hard;hard.begin();hard.initialized(true);hard.calibrated(true);Checks bad;bad.hardFaults=Numeric;hard.step(bad,0);CHECK(hard.state==State::FAULT);CHECK(hard.faults==Numeric);
 Safety explicitDisarm;explicitDisarm.begin();explicitDisarm.initialized(true);explicitDisarm.calibrated(true);Checks normal=ok;normal.kill=false;normal.arm=false;explicitDisarm.step(normal,0);normal.arm=true;explicitDisarm.step(normal,1);explicitDisarm.step(normal,1001);CHECK(explicitDisarm.armed());normal.arm=false;explicitDisarm.step(normal,1002);CHECK(explicitDisarm.state==State::DISARMED);normal.arm=true;explicitDisarm.step(normal,1003);CHECK(!explicitDisarm.armed());
 Safety interrupted;interrupted.begin();interrupted.initialized(true);interrupted.calibrated(true);normal.arm=false;interrupted.step(normal,0);normal.arm=true;interrupted.step(normal,1);normal.fresh=false;interrupted.step(normal,500);CHECK(interrupted.state==State::DISARMED);normal.fresh=true;interrupted.step(normal,501);interrupted.step(normal,3000);CHECK(!interrupted.armed());normal.arm=false;interrupted.step(normal,3001);normal.arm=true;interrupted.step(normal,3002);interrupted.step(normal,4002);CHECK(interrupted.armed());
 Gesture g;CHECK(!g.permit(false,false,false,0));CHECK(!g.permit(true,false,false,.5f));CHECK(!g.permit(true,true,true,.5f));CHECK(!g.permit(true,false,false,0));CHECK(g.permit(true,true,true,.2f));CHECK(!g.permit(false,true,true,.2f));CHECK(!g.permit(true,true,true,.2f));
 std::cout<<"PASS "<<checks<<" assertions (shared C++ logic only)\n";return 0;
}
