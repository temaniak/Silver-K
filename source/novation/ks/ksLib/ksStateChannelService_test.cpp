#include "ksStateChannelForeground.h"
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
 if(argc!=2)return 77;g_dasm=false;ks::Rom rom(argv[1]);check(rom.isValid(),"ROM");check(!ks::controllerReferenceAvailable(),"Reference factory linked");
 auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();auto decoder=std::make_unique<H8SDisassembler>();for(unsigned at:{0u,0xa00000u}){a->loadmem(rom.data().data(),rom.data().size(),at);b->loadmem(rom.data().data(),rom.data().size(),at);}decoder->loadmem(rom.data().data(),rom.data().size(),0);
 Bus<H8SDevice>x;Bus<ks::ControllerBus>y;x.events.reserve(256);y.events.reserve(256);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);ks::LiveMidiController midi(*b);auto rt=std::make_unique<ks::RuntimeController>(*b,midi);ks::StateChannelForeground f;
 using K=ks::StateChannelService::Kind;using S=ks::StateChannelTask::Stage;
 struct Config{K kind=K::ModeStatus;unsigned activation=53,pending=1,previous=0,decay=8,flags=0xa5,priority=0;bool caller=false,nested=false,live=false,reset=false,reentry=false;int previousWrite=-1;unsigned part=2,channel=7,mode=64,producer=254,consumer=128,queueProducer=254,queueConsumer=128;bool full=false;};Config cfg;
 uint64_t cases=0,phases=0,irqCases=0,nestedCases=0,liveCases=0,resets=0,reentries=0;unsigned roots=0;
 auto run=[&](unsigned inject=~0u,unsigned vector=32){
  rt->resetMetrics();f.resetMetrics();check(!f.active(),"Previous channel active");const unsigned root=cfg.kind==K::ModeStatus?0x43a8:0x43c2;std::array<int32_t,8>saved{};
  for(unsigned n=0;n<8;++n)saved[n]=a->regs[n].er=b->regs[n].er=int32_t((std::array<uint32_t,5>{0u,0x80000000u,0xffffffffu,0x7fffffffu,0x12345678u})[(n+cfg.flags)%5]);a->regs[2].r=b->regs[2].r=int16_t((cfg.activation<<8)|cfg.activation);saved[2]=b->regs[2].er;saved[7]=0xfff800;
  a->pc=a->makepc(root);a->setSP(0xfff800);a->cycles=1000;a->indent=1;a->ccr=uint8_t(cfg.flags);a->exr=uint8_t(cfg.priority);a->interruptMode2=true;a->lastread=-1;a->lastwrite=cfg.previousWrite;a->pending_irqs=a->pending_irqs_high=0;for(auto&p:a->interruptPriority)p=0;a->interruptPriority[vector]=6;
  b->pc=root;b->setSP(0xfff800);b->cycles=1000;b->indent=1;b->ccr=uint8_t(cfg.flags);b->exr=uint8_t(cfg.priority);b->interruptMode2=true;b->lastread=-1;b->lastwrite=cfg.previousWrite;b->pending_irqs=b->pending_irqs_high=0;for(auto&p:b->interruptPriority)p=0;b->interruptPriority[vector]=6;b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->nativeLowSize=0x200000;b->interpreterAllowed=false;
  for(auto*m:{a->memory,b->memory}){
   std::fill(m+0x15000,m+0x30000,uint8_t(0));std::fill(m+0xffec00,m+(1<<24),uint8_t(0));const auto byte=[&](unsigned at,unsigned v){m[at]=uint8_t(v);};const auto word=[&](unsigned at,unsigned v){byte(at,v>>8);byte(at+1,v);};const auto wide=[&](unsigned at,uint32_t v){word(at,v>>16);word(at+2,v);};
   wide(0xfff800,0x4338);std::fill(m+0xfff000,m+0xfff800,uint8_t(0x5a));byte(0x16798,cfg.part);byte(0x174ae +20*cfg.part+11,cfg.channel);byte(0x169ed,cfg.mode);word(0x15b04,cfg.queueProducer);word(0x15b06,cfg.queueConsumer);word(0xffed6c,cfg.producer);word(0xffed6e,cfg.full?(cfg.producer+1)&255:cfg.consumer);
   byte(0xffec14,1);byte(0xffee70,128);wide(0xffec10,7);byte(0xffec23,1);wide(0xffee7a,0xffffffff);wide(0xffee7e,1);word(0xffee78,0xffff);wide(0xffee82,0xffffffff);wide(0xffee86,0x34);byte(0xffee97,0);byte(0xffee9a,255);byte(0xffff8c,0xf7);byte(0xffff8a,0xf0);word(0xffec64,0);word(0xffec66,0);byte(0xffed68,0);byte(0xffff8d,0x42);byte(0xffffd5,0xc0);byte(0xffff55,255);byte(0xffff54,12);byte(0xffff59,0xe0);byte(0xffff50,31);byte(0xffff98,0x73);
   for(unsigned n=0;n<36;++n){byte(0xb918+n,n);byte(0xb93c+n,n&1);}for(unsigned n=0;n<4;++n){byte(0xffec16+n,1u<<n);byte(0xffec24+n,128u>>n);word(0xffff90+2*n,0x8000);word(0x15652+2*n,0x8000);word(0x1560a+2*n,0);word(0x1569a+2*n,0);}byte(0x156e4,255);
  }

  auto byte=[&](unsigned at,unsigned v){a->memory[at]=b->memory[at]=uint8_t(v);};
  byte(0xffec1b,cfg.pending&1);byte(0xffec29,cfg.pending&2);byte(0xffec1a,cfg.previous);byte(0xffec28,cfg.previous);byte(0xffec20,cfg.flags);
  for(unsigned n=0;n<4;++n){byte(0xffec16+n,cfg.decay^(n*17));byte(0xffec24+n,cfg.decay^(n*17));}
  byte(0x167ba,cfg.activation);byte(0x167d5,cfg.previous);byte(0x169ee,cfg.decay);
  unsigned count=0,target=0x4338;bool injected=false,nested=false,live=false,reset=false,reentry=false;
  do{const auto pc=uint32_t(a->getPC());
   if(count==inject){if(vector<64){a->pending_irqs|=1ull<<vector;b->pending_irqs|=1ull<<vector;}else{a->pending_irqs_high|=1ull<<(vector-64);b->pending_irqs_high|=1ull<<(vector-64);}injected=true;}
   if(cfg.nested&&injected&&!nested&&rt->activeNativeInterrupts()&&(b->exr&7)==6){a->interruptPriority[89]=b->interruptPriority[89]=7;a->pending_irqs_high|=1ull<<25;b->pending_irqs_high|=1ull<<25;nested=true;}
   if(cfg.live&&!live&&(pc==0x43b0||pc==0x43c8)){byte(pc==0x43b0?0x167d5:0x169ee,255);live=true;}
   if(cfg.caller&&count==5){byte(0xfff800,0xa5);byte(0xfff803,0x3a);target=0x433a;}
   if(f.active()&&count==4){if(cfg.reset){f.resetMetrics();reset=true;}if(cfg.reentry){const auto clock=b->cycles;const auto depth=b->indent;++b->indent;y.events.clear();tracking=true;check(!f.step(*b),"Wrong depth admitted");tracking=false;b->indent=depth;check(clock==b->cycles&&y.events.empty(),"Wrong depth effects");reentry=true;}}
   x.events.clear();y.events.clear();a->step();bool admit=false;for(unsigned irq=0;irq<128;++irq){const auto bits=irq<64?b->pending_irqs:b->pending_irqs_high;if((bits&(1ull<<(irq&63)))&&b->interruptPriority[irq]>(b->exr&7))admit=true;}
   tracking=true;if(rt->activeNativeInterrupts()||admit)rt->step(*b);else check(f.step(*b),"Channel Fore declined");tracking=false;
   const auto expected=resources(x.events,decoder.get()),actual=resources(y.events,nullptr);
   if(a->cycles!=b->cycles||expected!=actual){std::cerr<<"case="<<cases<<" phase="<<count<<" pc="<<std::hex<<pc<<std::dec<<" clocks="<<a->cycles<<'/'<<b->cycles<<'\n';for(const auto&rows:{expected,actual}){for(const auto&e:rows)std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<std::dec<<' ';std::cerr<<'\n';}throw std::runtime_error("Channel phase resource/time differs");}
   check(a->exr==b->exr&&a->pending_irqs==b->pending_irqs&&a->pending_irqs_high==b->pending_irqs_high,"Channel IRQ channel/pending differ");
   if(f.active()||rt->activeNativeInterrupts()){check(b->pc==root&&b->getSP()==0xfff800&&b->ccr==uint8_t(cfg.flags),"Channel CPU continuation");for(unsigned n=0;n<8;++n)check(b->regs[n].er==saved[n],"Channel CPU register shadow");for(unsigned at=0xfff000;at<0xfff800;++at)check(b->memory[at]==0x5a,"Channel inner stack write");}
   ++phases;check(++count<8192,"Channel stalled");
  }while(uint32_t(a->getPC())!=target||a->indent);
  check(!f.active()&&!rt->activeNativeInterrupts()&&b->pc==target&&!b->indent,"Channel did not return");
  for(unsigned n=0;n<8;++n)if(a->regs[n].er!=b->regs[n].er){std::cerr<<"ABI case="<<cases<<" kind="<<unsigned(cfg.kind)<<" r="<<n<<" old/new="<<std::hex<<a->regs[n].er<<'/'<<b->regs[n].er<<std::dec<<'\n';throw std::runtime_error("Channel final register differs");}
  if(a->ccr!=b->ccr)std::cerr<<"CCR="<<unsigned(a->ccr)<<'/'<<unsigned(b->ccr)<<" kind="<<unsigned(cfg.kind)<<'\n';check(a->ccr==b->ccr,"Channel final flags differ");check(a->lastread==b->lastread&&a->lastwrite==b->lastwrite,"Channel histories differ");check(!std::memcmp(a->memory,b->memory,0xfff000)&&!std::memcmp(a->memory+0xfffc00,b->memory+0xfffc00,0x400),"Channel nonscratch memory differs");check(f.started+f.initial==f.completed&&!b->interpretedSteps,"Channel lifetime/interpreter");if(injected)++irqCases;if(cfg.nested){check(nested,"Nested IRQ missed");++nestedCases;}if(cfg.live){check(live,"Live missed");++liveCases;}if(cfg.reset){check(reset&&f.initial==1,"Reset missed");++resets;}if(cfg.reentry){check(reentry,"Reentry missed");++reentries;}roots|=1u<<unsigned(cfg.kind);++cases;return count;
 };
 for(auto k:{K::ModeStatus,K::GlobalMirror})for(unsigned flags=0;flags<256;++flags)for(unsigned mode:{0u,1u,255u}){cfg=Config{};cfg.kind=k;cfg.flags=flags;cfg.priority=flags;cfg.activation=mode;cfg.previous=flags;cfg.decay=flags;run();}
 for(auto k:{K::ModeStatus,K::GlobalMirror}){cfg=Config{};cfg.kind=k;cfg.previous=1;const auto n=run();for(auto vector:{28u,32u,88u,89u,90u,91u})for(unsigned phase=0;phase<n;++phase)run(phase,vector);cfg.nested=true;run(1);cfg=Config{};cfg.kind=k;cfg.live=true;run();cfg=Config{};cfg.kind=k;cfg.caller=true;run();cfg=Config{};cfg.kind=k;cfg.reset=true;run();cfg=Config{};cfg.kind=k;cfg.reentry=true;run();}
 for(auto previous:{-1,0x167d5,0x169b0,0x174b8})for(auto k:{K::ModeStatus,K::GlobalMirror}){cfg=Config{};cfg.kind=k;cfg.previousWrite=previous;run();}
 unsigned guards=0;for(unsigned v=0;v<10;++v){ks::StateChannelForeground reject;b->pc=0x43a8;b->setSP(0xfff800);b->indent=1;b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->nativeLowSize=0x200000;b->interruptMode2=true;switch(v){case 0:b->nativeFetchClocksOnly=false;break;case 1:b->nativeLowBase=nullptr;break;case 2:b->nativeLowSize=0x2ffff;break;case 3:b->interruptMode2=false;break;case 4:b->indent=0;break;case 5:b->pc=0x43aa;break;case 6:b->setSP(0xfff3fe);break;case 7:b->setSP(0xfffc00);break;case 8:b->setSP(0xfff801);break;case 9:b->setSP(0xffffffff);break;}const auto clock=b->cycles;const auto pc=b->pc,sp=b->getSP();const auto lr=b->lastread,lw=b->lastwrite;const auto flags=b->ccr,mask=b->exr;std::array<int32_t,8>before{};for(unsigned n=0;n<8;++n)before[n]=b->regs[n].er;y.events.clear();tracking=true;check(!reject.step(*b),"Mask guard admitted");tracking=false;check(!reject.started&&!reject.active()&&y.events.empty()&&clock==b->cycles&&pc==b->pc&&sp==b->getSP()&&lr==b->lastread&&lw==b->lastwrite&&flags==b->ccr&&mask==b->exr,"Mask guard effects");for(unsigned n=0;n<8;++n)check(before[n]==b->regs[n].er,"Guard registers");++guards;}
 check(roots==3&&!allocations,"Channel allocation/root coverage");std::cout<<"PASS StateChannel calls="<<cases<<" phases="<<phases<<" IRQ="<<irqCases<<" nested="<<nestedCases<<" live="<<liveCases<<" reset="<<resets<<" reentry="<<reentries<<" guards="<<guards<<" allocations="<<allocations<<" interpreter="<<b->interpretedSteps<<'\n';return 0;
}catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}
