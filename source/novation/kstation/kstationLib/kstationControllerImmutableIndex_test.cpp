#include "kstationControllerDeviceState.h"
#include <array>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <vector>

using State = kstation::ControllerDeviceState;
struct Range { uint32_t begin, end; };
struct Bus final : kstation::ControllerResourceBus {
 unsigned writes=0, reads=0;
 bool mapped(uint32_t a)const override{return a>=0x600000&&a<0x600010;}
 uint8_t read(uint32_t a)override{++reads;return state_->memory[a];}
 void write(uint32_t a,uint8_t v)override{++writes;state_->memory[a]=v;}
};
static uint32_t seed=0x428dc71bu;
static uint32_t random32(){seed^=seed<<13;seed^=seed>>17;return seed^=seed<<5;}
static bool linear(const std::vector<Range>& ranges,uint32_t address){auto a=address&0xffffff;for(auto r:ranges)if(a>=r.begin&&a<r.end)return true;return false;}
static void check(bool ok,const char* what){if(!ok){std::fprintf(stderr,"FAIL: %s\n",what);throw std::runtime_error(what);}}
template<class F> static bool throws(F f){try{f();}catch(const std::logic_error&){return true;}return false;}
int main(){
 auto s=std::make_unique<State>();Bus bus;s->attachBus(&bus);std::vector<Range> ranges;
 auto add=[&](uint32_t b,uint32_t e){check(s->protectImmutable(b,e),"bind");ranges.push_back({b,e});};
 // Reverse order, overlap, adjacency, bridging and both firmware mirrors.
 add(0x400140,0x400142);add(0x140,0x142);add(0x102,0x104);add(0x100,0x102);add(0x101,0x103);
 add(0xfffffe,0x1000000);add(0,1);add(0x600004,0x600005);
 check(!s->protectImmutable(4,4)&&!s->protectImmutable(0xffffff,0x1000001),"invalid bind");
 const uint8_t wrong=1,zero=0;
 const auto before=s->immutableBindingCount();
 check(!s->protectImmutableVerified(0x600008,0x600009,&wrong),"failed verified bind");
 check(s->immutableBindingCount()==before&&!s->isImmutable(0x600008)&&bus.reads==1,"failed bind unchanged");
 check(s->protectImmutableVerified(0x600008,0x600009,&zero),"mapped verified bind");ranges.push_back({0x600008,0x600009});
 check(s->isVerifiedImmutable(0x600008)&&!s->isVerifiedImmutable(0x600004),"verified identity");
 for(unsigned i=0;i<4096;++i){auto b=random32()&0xffffff;auto length=1+(random32()%128);auto e=b+length;if(e>State::MemorySize)e=State::MemorySize;add(b,e);}
 for(auto r:ranges){for(uint32_t a:{r.begin-1,r.begin,r.end-1,r.end,r.begin+0x1000000})check(s->isImmutable(a)==linear(ranges,a),"range boundary");}
 for(unsigned i=0;i<50000;++i){auto a=random32();check(s->isImmutable(a)==linear(ranges,a),"random membership");}
 // A late binding must update the existing index, including merging spans.
 add(0x104,0x140);for(uint32_t a=0xff;a<=0x143;++a)check(s->isImmutable(a)==linear(ranges,a),"late merge");
 const auto cycles=s->cycles;const auto history=s->lastWrite;const auto writes=bus.writes;
 const auto a=s->memory[0x600003],b=s->memory[0x600004],c=s->memory[0xfffffd],d=s->memory[0xffffff];
 check(throws([&]{s->write8(0x600004,1);}),"mapped protected byte");
 check(throws([&]{s->write16(0x600003,0x1234);}),"mapped word preflight");
 check(throws([&]{s->write32(0x600002,0x12345678);}),"mapped long preflight");
 check(throws([&]{s->write32(0xfffffd,0x12345678);}),"wrap long preflight");
 check(throws([&]{s->write16(0xffffffff,0x1234);}),"masked wrap preflight");
 check(s->cycles==cycles&&s->lastWrite==history&&bus.writes==writes&&s->memory[0x600003]==a&&s->memory[0x600004]==b&&s->memory[0xfffffd]==c&&s->memory[0xffffff]==d,"atomic rejection");
 check(throws([&]{s->resetForBoot();}),"bound reset rejection");
 // Fill the original registration capacity with duplicate, already covered spans.
 while(s->immutableBindingCount()<State::ImmutableCapacity)add(0x100,0x101);
 const auto covered=s->isImmutable(0x60000e);
 check(!s->protectImmutable(0x60000e,0x60000f)&&!s->protectImmutableVerified(0x60000e,0x60000f,&zero),"capacity rejection");
 check(s->isImmutable(0x60000e)==covered,"capacity rejection leaves index unchanged");
 auto fresh=std::make_unique<State>();fresh->memory[7]=4;fresh->cycles=9;fresh->resetForBoot();check(!fresh->isImmutable(7)&&fresh->memory[7]==0&&fresh->cycles==0,"empty reset");
 Bus freshBus;fresh->attachBus(&freshBus);
 fresh->write16(0x600000,0x1234);
 check(freshBus.writes==2&&fresh->memory[0x600000]==0x12&&fresh->memory[0x600001]==0x34&&fresh->cycles==6&&fresh->lastWrite==-1,"mapped word success");
 fresh->write32(0x600002,0x56789abc);
 check(freshBus.writes==6&&fresh->memory[0x600002]==0x56&&fresh->memory[0x600003]==0x78&&fresh->memory[0x600004]==0x9a&&fresh->memory[0x600005]==0xbc&&fresh->cycles==18&&fresh->lastWrite==-1,"mapped long success");
 auto full=std::make_unique<State>();
 for(unsigned i=State::ImmutableCapacity;i>0;--i){auto b=2*(i-1);check(full->protectImmutable(b,b+1),"distinct descending bind");}
 for(unsigned i=0;i<State::ImmutableCapacity;++i){auto b=2*i;check(full->isImmutable(b)&&!full->isImmutable(b+1)&&full->isImmutable(b+0x1000000),"full index membership");}
 check(!full->protectImmutable(0x20000,0x20001)&&!full->isImmutable(0x20000)&&full->isImmutable(2*(State::ImmutableCapacity-1)),"full distinct index capacity");
 std::puts("PASS immutable index: linear equivalence, boundaries, binding, capacity and atomic writes");
}
