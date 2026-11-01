#include "kstationControllerTimers.h"
#include "h8s/h8sdevices.hpp"
#include <cstdio>
#include <memory>
#include <stdexcept>
namespace{void check(bool x,const char*w){if(!x)throw std::runtime_error(w);}uint32_t rng=0x4b54494d;uint32_t randomValue(){rng^=rng<<13;rng^=rng>>17;return rng^=rng<<5;}constexpr unsigned B=0xffff60;class Original final:public Timers{public:void tick(){auto start=Timers::read(B);bool phase=Timers::read(B+2)&0x40;if(phase)Timers::write(B,start&~4);Timers::tick();if(phase)Timers::write(B,start);}};}
int main(){try{auto a=std::make_unique<h8state>();auto b=std::make_unique<kstation::ControllerDeviceState>();uint64_t ticks=0,reads=0;
 for(unsigned n=0;n<2048;++n){Original old;kstation::ControllerTimers native;old.setState(a.get());a->cycles=b->cycles=0;a->pending_irqs=b->pendingIrqs=0;a->pending_irqs_high=b->pendingIrqsHigh=0;a->interruptMode2=b->interruptMode2=false;for(unsigned i=0;i<64;++i){auto v=uint8_t(randomValue());old.write(B+i,v);native.write(B+i,v);}old.write(B,31);native.write(B,31);
  for(unsigned i=0;i<12;++i){auto delta=randomValue()%1024;a->cycles+=delta;b->cycles+=delta;old.tick();native.tick(*b);check(a->pending_irqs==b->pendingIrqs,"CPU-free timer IRQ");for(unsigned r=0;r<64;++r){check(old.read(B+r)==native.read(B+r),"CPU-free timer register");++reads;}++ticks;}
 }
 for(unsigned shift=0;shift<4;++shift){Original old;kstation::ControllerTimers native;old.setState(a.get());a->cycles=b->cycles=0;a->pending_irqs=b->pendingIrqs=0;for(unsigned at:{B,B+4,B+6}){uint8_t v=at==B?1:at==B+4?uint8_t(shift):7;old.write(at,v);native.write(at,v);}a->cycles=b->cycles=(uint64_t(1)<<(32+shift))+16;old.tick();native.tick(*b);for(unsigned r=0;r<64;++r){check(old.read(B+r)==native.read(B+r),"CPU-free timer truncation");++reads;}check(a->pending_irqs==b->pendingIrqs,"CPU-free timer narrowed IRQ");++ticks;}
 std::printf("{\"passed\":true,\"ticks\":%llu,\"register_comparisons\":%llu}\n",ticks,reads);return 0;
}catch(const std::exception&e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
