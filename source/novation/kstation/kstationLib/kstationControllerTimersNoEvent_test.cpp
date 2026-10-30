#include "kstationControllerTimers.h"
#include "h8s/h8sdevices.hpp"
#include <array>
#include <cstdio>
#include <memory>
#include <stdexcept>
namespace {
constexpr unsigned B=0xffff60;
class Original final:public Timers {public:void tick(){auto start=Timers::read(B);bool phase=Timers::read(B+2)&0x40;if(phase)Timers::write(B,start&~4);Timers::tick();if(phase)Timers::write(B,start);}};
struct Case {unsigned start,a,b;};
void check(bool ok){if(!ok)throw std::runtime_error("original timer mismatch");}
}
void dynamicTest(){
 auto a=std::make_unique<h8state>();auto b=std::make_unique<kstation::ControllerDeviceState>();
 a->cycles=b->cycles=0;a->pending_irqs=b->pendingIrqs=0;a->pending_irqs_high=b->pendingIrqsHigh=0;a->interruptMode2=b->interruptMode2=false;
 Original old;kstation::ControllerTimers native;old.setState(a.get());
 auto put=[&](unsigned off,unsigned val){old.write(B+off,val);native.write(B+off,val);};
 auto tick=[&](unsigned delta){a->cycles+=delta;b->cycles+=delta;old.tick();native.tick(*b);for(unsigned i=0;i<64;++i)check(old.read(B+i)==native.read(B+i));check(a->pending_irqs==b->pendingIrqs&&a->pending_irqs_high==b->pendingIrqsHigh);};
 constexpr unsigned offsets[]={4,14,24,34,50};
 // Default disabled channels, then simultaneous events on all five.
 tick(31);put(2,0);for(auto off:offsets){put(off,0);put(off+2,7);put(off+3,0xf8);put(off+4,0);put(off+5,0);put(off+6,0);put(off+7,0);put(off+8,0);put(off+9,0);}put(0,31);tick(1);
 // Switch each channel independently through TSTR, PWM and external clock;
 // multiple writes before a tick must use the final state.
 for(unsigned i=0;i<5;++i){put(0,31&~(1u<<i));tick(7);put(0,31);put(2,1u<<i);tick(9);put(2,0);put(offsets[i],4);tick(11);put(offsets[i],0);put(0,0);put(0,31);put(2,1u<<i);put(2,0);tick(3);}
 // Channel 2 MDF suppression, including an intervening disabled tick.
 put(2,0x40);tick(13);put(2,0);tick(2);
 // Implicit copies and assignments preserve cached eligibility; reset uses default.
 auto oldCopy=old;auto nativeCopy=native;auto aCopy=std::make_unique<h8state>();auto bCopy=std::make_unique<kstation::ControllerDeviceState>();aCopy->cycles=bCopy->cycles=a->cycles;aCopy->pending_irqs=bCopy->pendingIrqs=0;aCopy->pending_irqs_high=bCopy->pendingIrqsHigh=0;aCopy->interruptMode2=bCopy->interruptMode2=false;oldCopy.setState(aCopy.get());aCopy->cycles+=3;bCopy->cycles+=3;oldCopy.tick();nativeCopy.tick(*bCopy);for(unsigned i=0;i<64;++i)check(oldCopy.read(B+i)==nativeCopy.read(B+i));check(aCopy->pending_irqs==bCopy->pendingIrqs&&aCopy->pending_irqs_high==bCopy->pendingIrqsHigh);
 Original assigned;kstation::ControllerTimers assignedNative;assigned=old;assignedNative=native;assigned.setState(aCopy.get());aCopy->cycles=bCopy->cycles=a->cycles;aCopy->pending_irqs=bCopy->pendingIrqs=0;aCopy->pending_irqs_high=bCopy->pendingIrqsHigh=0;aCopy->cycles+=4;bCopy->cycles+=4;assigned.tick();assignedNative.tick(*bCopy);for(unsigned i=0;i<64;++i)check(assigned.read(B+i)==assignedNative.read(B+i));check(aCopy->pending_irqs==bCopy->pendingIrqs&&aCopy->pending_irqs_high==bCopy->pendingIrqsHigh);
 native=kstation::ControllerTimers{};Original fresh;fresh.setState(a.get());a->cycles=b->cycles=0;a->pending_irqs=b->pendingIrqs=0;a->pending_irqs_high=b->pendingIrqsHigh=0;a->cycles=b->cycles=20;fresh.tick();native.tick(*b);for(unsigned i=0;i<64;++i)check(fresh.read(B+i)==native.read(B+i));check(a->pending_irqs==b->pendingIrqs&&a->pending_irqs_high==b->pendingIrqsHigh);
 std::puts("PASS dynamic control toggles, simultaneous IRQ, copy, assignment, default reset");
}
int main(){try{
 dynamicTest();
 constexpr std::array<Case,8> cases{{{100,102,105},{65534,5,9},{65535,0,1},{0,0,0},{100,100,101},{100,101,100},{100,103,103},{65530,65535,0}}};
 constexpr std::array<unsigned,8> counts{{0,1,2,3,5,6,65535,65536}};
 auto a=std::make_unique<h8state>();auto b=std::make_unique<kstation::ControllerDeviceState>();unsigned scenarios=0,reads=0;
 for(unsigned mode:{0u,0x20u,0x40u,0x60u})for(auto c:cases)for(unsigned count:counts)for(unsigned flags:{0u,7u}){
  Original old;kstation::ControllerTimers native;old.setState(a.get());a->cycles=b->cycles=0;a->pending_irqs=b->pendingIrqs=0;a->pending_irqs_high=b->pendingIrqsHigh=0;a->interruptMode2=b->interruptMode2=false;
  auto put=[&](unsigned offset,unsigned value){old.write(B+offset,value);native.write(B+offset,value);};
  auto word=[&](unsigned offset,unsigned value){put(offset,value>>8);put(offset+1,value&255);};
  put(0,1);put(2,0);put(4,mode);put(6,flags);put(7,0xf8|flags);word(8,c.start);word(10,c.a);word(12,c.b);
  a->cycles=b->cycles=count;old.tick();native.tick(*b);
  for(unsigned i=0;i<64;++i){check(old.read(B+i)==native.read(B+i));++reads;}
  check(a->pending_irqs==b->pendingIrqs&&a->pending_irqs_high==b->pendingIrqsHigh);++scenarios;
 }
 for(unsigned shift=0;shift<4;++shift){Original old;kstation::ControllerTimers native;old.setState(a.get());a->cycles=b->cycles=0;a->pending_irqs=b->pendingIrqs=0;a->pending_irqs_high=b->pendingIrqsHigh=0;a->interruptMode2=b->interruptMode2=false;for(unsigned at:{B,B+4,B+6}){auto v=at==B?1u:at==B+4?shift:7u;old.write(at,v);native.write(at,v);}a->cycles=b->cycles=(uint64_t(1)<<(32+shift))+16;old.tick();native.tick(*b);for(unsigned i=0;i<64;++i){check(old.read(B+i)==native.read(B+i));++reads;}check(a->pending_irqs==b->pendingIrqs&&a->pending_irqs_high==b->pendingIrqsHigh);++scenarios;}
 std::printf("PASS scenarios=%u register_comparisons=%u\n",scenarios,reads);return 0;
}catch(const std::exception&e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
