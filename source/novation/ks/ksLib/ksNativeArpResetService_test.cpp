#include "ksNativeArpResetForeground.h"
#include "ksControllerState.h"
#include "ksRuntimeController.h"
#include "ksLiveMidiController.h"
#include <cstring>
#include <cstdlib>
#include <new>
namespace {bool tracking=false;uint64_t allocations=0;}
void*operator new(std::size_t n){if(tracking)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void*operator new[](std::size_t n){return::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
#include "ksRom.h"
#include "h8s/h8s.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <vector>
using Access=std::tuple<bool,uint32_t,uint8_t,uint64_t>;using Raw=std::tuple<bool,uint32_t,uint8_t,uint64_t,uint32_t>;
struct Original:H8SEmulator{using H8SEmulator::indent;using H8SEmulator::lastread;using H8SEmulator::lastwrite;};
template<class Base>struct Bus:Base{std::vector<Raw>events;uint64_t statusReads=0,statusLimit=0;uint8_t read(uint32_t a)override{auto v=a==0x400004||a==0x600004?uint8_t(statusReads++<statusLimit?0:255):this->state->memory[a];events.emplace_back(false,a,v,this->state->cycles,uint32_t(this->state->getPC()));return v;}void write(uint32_t a,uint8_t v)override{this->state->memory[a]=v;events.emplace_back(true,a,v,this->state->cycles,uint32_t(this->state->getPC()));}};
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}int history(uint32_t a){a&=0xffffff;return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
std::vector<Access>resources(const std::vector<Raw>&events,H8SDisassembler*decoder){std::vector<Access>out;size_t first=events.size(),count=0;if(decoder){for(size_t i=0;i<events.size();++i){const auto&e=events[i];if(!std::get<0>(e)&&!(std::get<4>(e)&1)&&std::get<1>(e)==std::get<4>(e)){first=i;const auto pc=std::get<1>(e);decoder->pc=decoder->makepc(pc);decoder->step();count=uint32_t(decoder->getPC())-pc;check(count>=2&&count<=10,"Instruction length");break;}}check(count,"Missing H8 fetch");}for(size_t i=0;i<events.size();++i){const auto&e=events[i];const auto at=std::get<1>(e);if((i>=first&&i<first+count)||(at>=0xfff000&&at<0xfff800)||(!std::get<0>(e)&&at>=28*4&&at<92*4))continue;out.emplace_back(std::get<0>(e),at,std::get<2>(e),std::get<3>(e));}return out;}
int main(int argc,char**argv){try{
 if(argc!=2)return 77;g_dasm=false;ks::Rom rom(argv[1]);check(rom.isValid(),"ROM");check(!ks::controllerReferenceAvailable(),"H8 fallback linked");
 auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();auto decoder=std::make_unique<H8SDisassembler>();for(unsigned at:{0u,0xa00000u}){a->loadmem(rom.data().data(),rom.data().size(),at);b->loadmem(rom.data().data(),rom.data().size(),at);}decoder->loadmem(rom.data().data(),rom.data().size(),0);
 Bus<H8SDevice>x;Bus<ks::ControllerBus>y;x.events.reserve(256);y.events.reserve(256);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);ks::LiveMidiController midi(*b);auto rt=std::make_unique<ks::RuntimeController>(*b,midi);ks::NativeArpResetForeground f;
 using C=ks::NativeArpResetService::Component;
 struct Config{unsigned key=60,pattern=0,part=2,flags=0xa5,count=8,pointer=0,policy=0,mode=64,octave=3,periodic=0,vlq=1;bool live=false,caller=false,nested=false,reentry=false,reset=false;};Config cfg;
 uint64_t cases=0,phases=0,irqCases=0,nestedCases=0,liveCases=0,reentries=0,resets=0;uint32_t components=0;
 auto run=[&](unsigned inject=~0u,unsigned vector=32){
  rt->resetMetrics();f.resetMetrics();check(!f.active(),"Previous key active");const unsigned root=0x3f80;std::array<int32_t,8>saved{};
  for(unsigned n=0;n<8;++n)saved[n]=a->regs[n].er=b->regs[n].er=int32_t((std::array<uint32_t,5>{0u,0x80000000u,0xffffffffu,0x7fffffffu,0x12345678u})[(n+cfg.flags)%5]);a->regs[5].r=b->regs[5].r=int16_t((unsigned(cfg.key^99)<<8)|cfg.key);saved[5]=b->regs[5].er;const unsigned map=0x19000; a->regs[4].er=b->regs[4].er=int32_t(cfg.pointer?0xffa19801:0x19801);saved[4]=b->regs[4].er;saved[7]=0xfff800;
  for(auto*h:{a.get()}){h->pc=h->makepc(root);h->setSP(0xfff800);h->cycles=1000;h->indent=1;h->ccr=uint8_t(cfg.flags);h->exr=0;h->interruptMode2=true;h->lastread=-1;h->lastwrite=-1;h->pending_irqs=h->pending_irqs_high=0;for(auto&p:h->interruptPriority)p=0;h->interruptPriority[vector]=6;}
  b->pc=root;b->setSP(0xfff800);b->cycles=1000;b->indent=1;b->ccr=uint8_t(cfg.flags);b->exr=0;b->interruptMode2=true;b->lastread=-1;b->lastwrite=-1;b->pending_irqs=b->pending_irqs_high=0;for(auto&p:b->interruptPriority)p=0;b->interruptPriority[vector]=6;b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->nativeLowSize=0x200000;b->interpreterAllowed=false;
  for(auto*m:{a->memory,b->memory}){
   std::fill(m+0x15000,m+0x30000,uint8_t(0));std::fill(m+0xffec00,m+(1<<24),uint8_t(0));const auto byte=[&](unsigned at,unsigned v){m[at]=uint8_t(v);};const auto word=[&](unsigned at,unsigned v){byte(at,v>>8);byte(at+1,v);};const auto wide=[&](unsigned at,uint32_t v){word(at,v>>16);word(at+2,v);};
   wide(0xfff800,0x4338);std::fill(m+0xfff000,m+0xfff800,uint8_t(0x5a));byte(0x17526,cfg.part);
   for(unsigned p:{cfg.part,4u}){const auto base=0x160d2+334*p;for(unsigned i=0;i<16;++i)byte(base+208+i,cfg.pattern==0?0:cfg.pattern==1?(i==15?128:0):255);for(unsigned i=0;i<128;++i)byte(base+64+i,i^cfg.key);byte(base+306,8);byte(base+307,8);byte(base+308,4);byte(base+314,77);byte(base+316,1);byte(base+317,2);byte(base+322,93);byte(base+323,17);byte(base+324,cfg.mode);byte(base+325,cfg.octave);byte(base+326,99);byte(base+327,cfg.policy);wide(base+48,map);wide(base+52,0x19010);wide(base+56,0xf0000012);wide(base+60,0x11223344);}
   for(unsigned i=0;i<cfg.vlq;++i)byte(map+i,(cfg.key&127)|(i+1<cfg.vlq?128:0));byte(0x168fc+249,cfg.periodic);byte(0x168fc+219,cfg.key);byte(0x168fc+220,cfg.key^71);byte(0x168fc+221,cfg.key^253);byte(0x168fc+79,cfg.key);byte(0x168fc+78,cfg.policy);for(unsigned i=0;i<32;++i)byte(0x1532e + i,i*23+cfg.key);byte(0x167ba,cfg.key%3);byte(0x167d3,cfg.key);byte(0x159f2,0xff);byte(0x159f3,0xff);
   byte(0xffec14,1);byte(0xffee70,128);wide(0xffec10,7);byte(0xffec23,1);wide(0xffee7a,0xffffffff);wide(0xffee7e,1);word(0xffee78,0xffff);wide(0xffee82,0xffffffff);wide(0xffee86,0x34);byte(0xffee97,0);byte(0xffee9a,255);byte(0xffff8c,0xf7);byte(0xffff8a,0xf0);word(0xffec64,0);word(0xffec66,0);byte(0xffed68,0);byte(0xffff8d,0x42);byte(0xffffd5,0xc0);byte(0xffff55,255);byte(0xffff54,12);byte(0xffff59,0xe0);byte(0xffff50,31);byte(0xffff98,0x73);
   for(unsigned n=0;n<36;++n){byte(0xb918+n,n);byte(0xb93c+n,n&1);}for(unsigned n=0;n<4;++n){byte(0xffec16+n,1u<<n);byte(0xffec24+n,128u>>n);word(0xffff90+2*n,0x8000);word(0x15652+2*n,0x8000);word(0x1560a+2*n,0);word(0x1569a+2*n,0);}byte(0x156e4,255);
  }
  unsigned count=0,target=0x4338;bool injected=false,nested=false,reentered=false,reset=false,live=false;std::array<unsigned,9>landings{};landings.fill(~0u);
  do{const auto pc=uint32_t(a->getPC());if(count==inject){if(vector<64){a->pending_irqs|=1ull<<vector;b->pending_irqs|=1ull<<vector;}else{a->pending_irqs_high|=1ull<<(vector-64);b->pending_irqs_high|=1ull<<(vector-64);}injected=true;}
   if(cfg.nested&&injected&&!nested&&rt->activeNativeInterrupts()&&(b->exr&7)==6){a->interruptPriority[89]=b->interruptPriority[89]=7;a->pending_irqs_high|=1ull<<25;b->pending_irqs_high|=1ull<<25;nested=true;}
   if(cfg.caller&&count==5){a->memory[0xfff803]=b->memory[0xfff803]=0x3a;target=0x433a;}
   if(cfg.live&&!live&&pc==0x3074){a->memory[0x17526]=b->memory[0x17526]=4;live=true;}
   if(const auto*s=f.service()){auto n=unsigned(s->component());if(landings[n]==~0u)landings[n]=count;if(cfg.reset&&!reset&&count==5){f.resetMetrics();reset=true;}if(cfg.reentry&&!reentered&&count==5){const auto clock=b->cycles;const auto depth=b->indent;const auto fs=f.phases;++b->indent;y.events.clear();tracking=true;check(!f.step(*b),"ArpReset wrong-depth admitted");tracking=false;b->indent=depth;check(clock==b->cycles&&fs==f.phases&&y.events.empty(),"ArpReset wrong-depth effects");reentered=true;}}
   x.events.clear();y.events.clear();a->step();bool admit=false;for(unsigned irq=0;irq<128;++irq){const auto bits=irq<64?b->pending_irqs:b->pending_irqs_high;if((bits&(1ull<<(irq&63)))&&b->interruptPriority[irq]>(b->exr&7))admit=true;}tracking=true;if(rt->activeNativeInterrupts()||admit)rt->step(*b);else check(f.step(*b),"ArpReset Fore declined");tracking=false;
   const auto expected=resources(x.events,decoder.get()),actual=resources(y.events,nullptr);if(a->cycles!=b->cycles||expected!=actual){std::cerr<<"case="<<cases<<" phase="<<count<<" pc="<<std::hex<<pc<<std::dec<<" clocks="<<a->cycles<<'/'<<b->cycles<<'\n';for(const auto&rows:{expected,actual}){for(const auto&e:rows)std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<std::dec<<' ';std::cerr<<'\n';}throw std::runtime_error("ArpReset phase resource/time differs");}
   check(a->exr==b->exr&&a->pending_irqs==b->pending_irqs&&a->pending_irqs_high==b->pending_irqs_high,"ArpReset IRQ masks/pending differ");if(f.active()||rt->activeNativeInterrupts()){check(b->pc==root&&b->getSP()==0xfff800&&b->ccr==uint8_t(cfg.flags),"ArpReset CPU continuation");for(unsigned n=0;n<8;++n)check(b->regs[n].er==saved[n],"ArpReset CPU register shadow");for(unsigned at=0xfff000;at<0xfff800;++at)check(b->memory[at]==0x5a,"ArpReset CPU stack write");}
   ++phases;check(++count<32768,"ArpReset stalled");
  }while(uint32_t(a->getPC())!=target||a->indent);
  check(!f.active()&&!rt->activeNativeInterrupts()&&b->pc==target&&!b->indent,"ArpReset did not return");for(unsigned n=0;n<8;++n)if(a->regs[n].er!=b->regs[n].er){std::cerr<<"ABI case="<<cases<<" kind="<<cfg.mode<<" key="<<cfg.key<<" pattern="<<cfg.pattern<<" r="<<n<<" old/new="<<std::hex<<a->regs[n].er<<'/'<<b->regs[n].er<<std::dec<<'\n';throw std::runtime_error("ArpReset final register differs");}if(a->ccr!=b->ccr)std::cerr<<"CCR="<<unsigned(a->ccr)<<'/'<<unsigned(b->ccr)<<" kind="<<cfg.mode<<" key="<<cfg.key<<'\n';check(a->ccr==b->ccr,"ArpReset flags differ");check(a->lastread==b->lastread&&a->lastwrite==b->lastwrite,"ArpReset final histories differ");check(!std::memcmp(a->memory,b->memory,0xfff000)&&!std::memcmp(a->memory+0xfffc00,b->memory+0xfffc00,0x400),"ArpReset full nonscratch memory differs");check(f.started+f.initial==f.completed,"ArpReset lifetime");check(!b->interpretedSteps,"ArpReset interpreter");if(injected)++irqCases;if(cfg.nested){check(nested,"ArpReset nested IRQ missed");++nestedCases;}if(cfg.live){check(live,"ArpReset live observations missed");++liveCases;}if(cfg.reentry){check(reentered,"ArpReset reentry missed");++reentries;}if(cfg.reset){check(reset&&f.initial==1&&f.started==0&&f.completed==1,"ArpReset metric reset missed");++resets;}components|=f.components;++cases;return std::make_pair(landings,count);
 };
 for(unsigned policy=0;policy<256;++policy)for(unsigned mode:{0u,1u,64u,65u})for(unsigned pattern=0;pattern<3;++pattern){cfg=Config{};cfg.policy=policy;cfg.mode=mode;cfg.pattern=pattern;cfg.flags=policy;cfg.key=policy;cfg.part=policy%5;cfg.vlq=1+(policy%4);cfg.octave=policy;cfg.periodic=policy%4;run();}
 for(unsigned key=0;key<256;++key)for(unsigned vlq=1;vlq<=4;++vlq){cfg=Config{};cfg.key=key;cfg.vlq=vlq;cfg.flags=key;run();}
 for(unsigned mode:{0u,64u})for(unsigned held:{0u,1u}){cfg=Config{};cfg.mode=mode;cfg.pattern=held;auto points=run();for(auto phase:points.first)if(phase!=~0u)for(auto v:{28u,32u,88u,89u,90u,91u})run(phase,v);cfg.caller=true;run();cfg.caller=false;cfg.reentry=true;run();cfg.reentry=false;cfg.reset=true;run();}
 cfg=Config{};{const auto all=run();for(unsigned phase=0;phase<all.second;++phase)run(phase,32);}
 cfg=Config{};cfg.live=true;run();cfg.live=false;cfg.nested=true;run(1,32);cfg.nested=false;cfg.pointer=1;run();
 check(components==511&&!allocations,"ArpReset coverage/allocations");std::cout<<"PASS NativeArpReset calls="<<cases<<" phases="<<phases<<" IRQ="<<irqCases<<" nested="<<nestedCases<<" live="<<liveCases<<" reentry="<<reentries<<" reset="<<resets<<" components="<<components<<" allocations="<<allocations<<" interpreter="<<b->interpretedSteps<<'\n';return 0;
}catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}
