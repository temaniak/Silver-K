#include "ksClockSyncForeground.h"
#include "ksRuntimeController.h"
#include "ksLiveMidiController.h"
#include "ksRom.h"
#include "h8s/h8s.hpp"
#include <vector>
#include <cstdlib>
#include <new>
#include <tuple>
#include <memory>
#include <iostream>
#include <cstring>
#include <stdexcept>
namespace {bool tracking=false;uint64_t allocations=0;}
void*operator new(std::size_t n){if(tracking)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void*operator new[](std::size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
namespace {
using Event=std::tuple<bool,uint32_t,uint8_t,uint64_t>;using Raw=std::tuple<bool,uint32_t,uint8_t,uint64_t,uint32_t>;
struct H8:H8SEmulator{using H8SEmulator::lastread;using H8SEmulator::lastwrite;using H8SEmulator::indent;};
template<class B>struct Bus:B{std::vector<Raw>events;uint64_t busy=0,reads=0;bool pairedFlip=false,pairedDone=false;uint8_t read(uint32_t a)override{if(pairedFlip&&!pairedDone&&a==0xffeea4){this->state->memory[a+1]^=0xe7;pairedDone=true;}const uint8_t v=(a==0x400004||a==0x600004)?(reads++<busy?0:255):this->state->memory[a];events.emplace_back(false,a,v,this->state->cycles,uint32_t(this->state->getPC()));return v;}void write(uint32_t a,uint8_t v)override{this->state->memory[a]=v;events.emplace_back(true,a,v,this->state->cycles,uint32_t(this->state->getPC()));}};
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
std::vector<Event>resource(const std::vector<Raw>&rows,H8SDisassembler*decoder){std::vector<Event>r;size_t first=rows.size(),length=0;if(decoder)for(size_t n=0;n<rows.size();++n)if(!std::get<0>(rows[n])&&!(std::get<4>(rows[n])&1)&&std::get<1>(rows[n])==std::get<4>(rows[n])){first=n;decoder->pc=decoder->makepc(std::get<4>(rows[n]));decoder->step();length=uint32_t(decoder->getPC())-std::get<4>(rows[n]);break;}if(decoder)check(length>=2&&length<=10,"Independent instruction fetch bounds");for(size_t n=0;n<rows.size();++n){const auto&e=rows[n];const auto a=std::get<1>(e);if((n>=first&&n<first+length)||(a>=0xfff000&&a<0xfffc00)||(!std::get<0>(e)&&a>=28*4&&a<92*4))continue;r.emplace_back(std::get<0>(e),a,std::get<2>(e),std::get<3>(e));}return r;}
}
int main(int argc,char**argv){try{
 if(argc!=2)return 77;check(!ks::controllerReferenceAvailable(),"Synchronized reference factory linked");ks::Rom rom(argv[1]);check(rom.isValid(),"ROM");g_dasm=false;
 auto h=std::make_unique<H8>();auto b=std::make_unique<ks::ControllerCpu>();auto decoder=std::make_unique<H8SDisassembler>();decoder->loadmem(rom.data().data(),unsigned(rom.data().size()),0);
 for(unsigned at:{0u,0xa00000u})b->loadmem(rom.data().data(),unsigned(rom.data().size()),at);
 auto shared=std::make_unique<ks::LiveMidiController>(*b);auto runtime=std::make_unique<ks::RuntimeController>(*b,*shared);Bus<H8SDevice>oldBus;Bus<ks::ControllerBus>newBus;oldBus.events.reserve(1024);newBus.events.reserve(1024);h->memmap(&oldBus,0,1<<24);b->memmap(&newBus,0,1<<24);
 uint64_t calls=0,phases=0,irqs=0,guards=0,lives=0,faults=0;uint32_t components=0;
 struct Config{uint8_t divider=1,source=0,sourceFlags=0,flags=0,internal=60;uint32_t period=0x1000000,synchronized=0x1000000,previousCoefficient=0,previousPeriod=0;uint16_t signal=300,gain=0x80;uint64_t busy=0;unsigned seed=0,irqAt=~0u,vector=32;int mutateStage=-1,lateBusyComponent=-1;uint32_t mutateAddress=0,mutateValue=0;unsigned mutateBytes=1;bool caller=false,nested=false,pairedFlip=false;};Config cfg;
 auto run=[&](){
  auto foreground=std::make_unique<ks::ClockSyncForeground>();std::fill(h->memory,h->memory+(1<<24),0);for(unsigned at:{0u,0xa00000u})h->loadmem(rom.data().data(),unsigned(rom.data().size()),at);
  const auto put=[&](uint32_t at,uint32_t v,unsigned bytes){for(unsigned n=0;n<bytes;++n)h->memory[at+n]=uint8_t(v>>(8*(bytes-1-n)));};
  put(0xffee98,cfg.divider,1);put(0x169ef,cfg.source,1);put(0xffee97,cfg.sourceFlags,1);put(0x16950,cfg.internal,1);put(0xffee7e,cfg.period,4);put(0xffee8e,cfg.synchronized,4);put(0xffeea0,cfg.signal,2);put(0xffeea4,cfg.gain,2);put(0xffee92,cfg.previousCoefficient,4);put(0xffee8a,cfg.previousPeriod,4);put(0xfff800,0x81004338,4);
  h->memory[0xffff62]=0x50;h->memory[0xffec22]=0x25;
  for(unsigned n=0;n<7;++n)h->regs[n].er=int32_t(0x12345678u+cfg.seed*0x123123u+n*0x11111111u);
  h->setSP(0xfff800);h->pc=h->makepc(0x267a);h->indent=1;h->cycles=1000;h->ccr=cfg.flags;h->exr=0xa0;h->lastread=cfg.seed&1?0x23456:-1;h->lastwrite=cfg.seed&2?0x159f2:-1;h->interruptMode2=true;h->pending_irqs=h->pending_irqs_high=0;
  std::memcpy(b->memory,h->memory,1<<24);for(unsigned n=0;n<8;++n)b->regs[n].er=h->regs[n].er;b->pc=0x267a;b->cycles=h->cycles;b->ccr=h->ccr;b->exr=h->exr;b->indent=h->indent;b->lastread=h->lastread;b->lastwrite=h->lastwrite;b->interruptMode2=true;b->pending_irqs=b->pending_irqs_high=0;b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->nativeLowSize=0x200000;b->interpreterAllowed=false;
  for(unsigned n=0;n<128;++n)h->interruptPriority[n]=b->interruptPriority[n]=0;h->interruptPriority[cfg.vector]=b->interruptPriority[cfg.vector]=6;
  oldBus.pairedFlip=newBus.pairedFlip=cfg.pairedFlip;oldBus.pairedDone=newBus.pairedDone=false;oldBus.reads=newBus.reads=0;oldBus.busy=newBus.busy=cfg.busy;runtime->resetMetrics();std::array<int32_t,8>held{};for(unsigned n=0;n<8;++n)held[n]=b->regs[n].er;std::array<uint8_t,0xc00>scratch{};std::memcpy(scratch.data(),b->memory+0xfff000,scratch.size());const auto entryFlags=b->ccr;unsigned count=0,target=0x4338;bool injected=false,nested=false,mutated=false,fault=false,lateBusy=false;std::array<unsigned,4>landings{};landings.fill(~0u);
  do{
   if(count==cfg.irqAt){if(cfg.vector<64){h->pending_irqs|=1ull<<cfg.vector;b->pending_irqs|=1ull<<cfg.vector;}else{h->pending_irqs_high|=1ull<<(cfg.vector-64);b->pending_irqs_high|=1ull<<(cfg.vector-64);}injected=true;}
   if(cfg.nested&&injected&&!nested&&runtime->activeNativeInterrupts()&&(b->exr&7)==6&&count>cfg.irqAt+1){h->interruptPriority[89]=b->interruptPriority[89]=7;h->pending_irqs_high|=1ull<<25;b->pending_irqs_high|=1ull<<25;nested=true;}
   if(const auto*s=foreground->service()){
    const auto i=unsigned(s->component());if(landings[i]==~0u)landings[i]=count;fault|=s->template task<ks::DspFaultTask>()!=nullptr;
    if(!lateBusy&&int(s->component())==cfg.lateBusyComponent){oldBus.busy=oldBus.reads+65538;newBus.busy=newBus.reads+65538;lateBusy=true;++lives;}
    if(!mutated&&s->component()==ks::ClockSyncService::Component::Sync&&int(s->syncTask().stage())==cfg.mutateStage){for(unsigned n=0;n<cfg.mutateBytes;++n)h->memory[cfg.mutateAddress+n]=b->memory[cfg.mutateAddress+n]=uint8_t(cfg.mutateValue>>(8*(cfg.mutateBytes-1-n)));mutated=true;++lives;}
   }
   if(cfg.caller&&count==3){h->memory[0xfff803]=b->memory[0xfff803]=0x3a;scratch[0x803]=0x3a;target=0x433a;++lives;}
   if(count==5){foreground->resetMetrics();runtime->resetMetrics();}
   oldBus.events.clear();const auto pc=uint32_t(h->getPC());h->step();const auto expected=resource(oldBus.events,decoder.get());newBus.events.clear();bool pending=false;for(unsigned n=0;n<128;++n){const auto bits=n<64?b->pending_irqs:b->pending_irqs_high;if((bits&(1ull<<(n&63)))&&b->interruptPriority[n]>(b->exr&7))pending=true;}
   tracking=true;try{if(pending||runtime->activeNativeInterrupts())runtime->step(*b);else check(foreground->step(*b),"Clock-sync foreground rejected");}catch(...){tracking=false;std::cerr<<"Exception case="<<calls<<" seed="<<cfg.seed<<" phase="<<count<<" PC="<<std::hex<<pc<<std::dec<<'\n';throw;}tracking=false;
   const auto actual=resource(newBus.events,nullptr);if(expected!=actual||h->cycles!=b->cycles){std::cerr<<"Resource/time case="<<calls<<" source="<<unsigned(cfg.source)<<" gain="<<std::hex<<cfg.gain<<" phase="<<std::dec<<count<<" PC="<<std::hex<<pc<<std::dec<<" clocks="<<h->cycles<<'/'<<b->cycles<<'\n';for(const auto*list:{&expected,&actual}){for(const auto&e:*list)std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<std::dec<<' ';std::cerr<<'\n';}throw std::runtime_error("Resource/time");}
   if(foreground->active()||runtime->activeNativeInterrupts()){check(b->getPC()==0x267a&&b->getSP()==0xfff800&&b->ccr==entryFlags,"Internal CPU frame mutated");for(unsigned n=0;n<8;++n)check(b->regs[n].er==held[n],"Internal register frame mutated");}
   check(h->exr==b->exr,"EXR");check(h->pending_irqs==b->pending_irqs&&h->pending_irqs_high==b->pending_irqs_high,"Pending IRQ");++phases;check(++count<2000000,"Clock-sync stalled");
  }while(foreground->active()||runtime->activeNativeInterrupts()||b->getPC()!=int(target));
  for(unsigned n=0;n<8;++n)if(h->regs[n].er!=b->regs[n].er){std::cerr<<"ABI case="<<calls<<" seed="<<cfg.seed<<" source="<<unsigned(cfg.source)<<" gain="<<std::hex<<cfg.gain<<" R"<<n<<" expected="<<uint32_t(h->regs[n].er)<<" actual="<<uint32_t(b->regs[n].er)<<std::dec<<'\n';throw std::runtime_error("Final ABI");}
  if(h->ccr!=b->ccr){std::cerr<<"CCR case="<<calls<<" seed="<<cfg.seed<<" expected="<<std::hex<<unsigned(h->ccr)<<" actual="<<unsigned(b->ccr)<<std::dec<<'\n';throw std::runtime_error("Final CCR");}
  check(h->getPC()==int(target)&&h->getSP()==0xfff804&&b->getSP()==0xfff804&&h->indent==b->indent,"Return edge");check(h->lastread==b->lastread&&h->lastwrite==b->lastwrite,"Final bus history");check(!std::memcmp(h->memory,b->memory,0xfff000)&&!std::memcmp(h->memory+0xfffc00,b->memory+0xfffc00,0x400),"Non-scratch memory");check(!std::memcmp(scratch.data(),b->memory+0xfff000,scratch.size()),"Native graph wrote CPU scratch");check(!b->interpretedSteps,"Interpreter");check(foreground->started+foreground->initial==foreground->completed&&!foreground->active(),"Job lifetime");components|=foreground->components;
  if(cfg.lateBusyComponent>=0)check(lateBusy,"Late busy component not reached");if(cfg.busy>65536||cfg.lateBusyComponent>=0){check(fault,"Actual DSP fault child not reached");++faults;}if(cfg.irqAt!=~0u){check(injected,"IRQ not reached");++irqs;}if(cfg.nested)check(nested,"Nested IRQ not reached");if(cfg.mutateStage>=0)check(mutated,"Live stage not reached");++calls;return landings;
 };
 for(unsigned flags=0;flags<256;++flags)for(unsigned path=0;path<5;++path){cfg=Config{};cfg.flags=uint8_t(flags);cfg.seed=flags;cfg.divider=path?1:0;cfg.source=path>=2?2:0;cfg.gain=path==3?0x800:path==4?0x801:0x80;run();}
 for(unsigned value=0;value<256;++value){cfg=Config{};cfg.internal=uint8_t(value);cfg.seed=value;run();cfg.source=uint8_t(value);run();}
 for(unsigned source:{0u,1u,2u,3u,255u})for(unsigned gain:{0u,1u,0x7fu,0x80u,0x81u,0x7ffu,0x800u,0x801u,0xffffu})for(uint32_t rate:{0u,1u,0x1ffffu,0x7fffffffu,0x80000000u,0xffffffffu}){cfg=Config{};cfg.source=uint8_t(source);cfg.gain=uint16_t(gain);cfg.synchronized=rate;cfg.signal=uint16_t(rate>>16);cfg.seed=rate^gain;run();}

 for(unsigned value=0;value<256;++value){cfg=Config{};cfg.divider=uint8_t(value);cfg.flags=uint8_t(value*37);cfg.sourceFlags=uint8_t(value);cfg.seed=value;run();}
 for(unsigned gain:{0u,1u,0x80u,0x7fffu,0x8000u,0xffffu})for(unsigned signal:{0u,1u,32767u,32768u,65535u})for(uint32_t rate:{0u,0x7fff0000u,0x80000000u,0xffffffffu}){cfg=Config{};cfg.source=2;cfg.gain=uint16_t(gain);cfg.signal=uint16_t(signal);cfg.synchronized=rate;cfg.seed=rate+signal;run();}
 const auto coefficient=[](uint32_t rate){uint32_t n=0x4000000;for(unsigned i=0;i<5&&!(rate&0x80000000u);++i){rate<<=1;n<<=1;}const auto d=uint16_t(rate>>16);const auto high=d?n/d:0;const uint32_t frac=(n-high*d)<<16;return(uint32_t(uint16_t(high))<<16)|uint16_t(d?frac/d:0);};
 for(unsigned internal:{0u,63u,64u,127u,128u,191u,192u,255u})for(uint32_t delta:{0u,1u,4095u,4096u,4097u,0xfffff000u,0xfffff001u,0x80000000u}){cfg=Config{};cfg.source=1;cfg.internal=uint8_t(internal);const auto period=uint32_t(uint8_t(internal+64))*32u*0xd1b7u;cfg.previousCoefficient=coefficient(period<<2)+delta;cfg.previousPeriod=period&0xfffff000u;cfg.seed=delta;run();}
 for(unsigned component:{1u,2u,3u}){cfg=Config{};cfg.lateBusyComponent=int(component);run();}
 cfg=Config{};cfg.busy=65538;cfg.irqAt=50;run();
 cfg=Config{};const auto landing=run();for(unsigned vector:{28u,32u,88u,89u,90u,91u})for(auto pos:landing)if(pos!=~0u){cfg=Config{};cfg.vector=vector;cfg.irqAt=pos;run();}
 cfg=Config{};cfg.irqAt=0;cfg.nested=true;run();cfg=Config{};cfg.caller=true;run();
 for(unsigned busy:{1u,7u,65538u}){cfg=Config{};cfg.busy=busy;run();}
 using T=ks::ClockSyncTask;for(auto stage:{T::Stage::ReadSource,T::Stage::ReadInternalRate,T::Stage::ReadPeriod,T::Stage::ReadActive,T::Stage::ReadActiveAgain,T::Stage::ReadGainAgain,T::Stage::ReadPublishPeriod,T::Stage::ReadPreviousPeriod,T::Stage::ReadSignal,T::Stage::ReadSynchronized,T::Stage::ReadGain,T::Stage::ReadPreviousCoefficient,T::Stage::ReadSourceFlags,T::Stage::WriteSourceFlag}){cfg=Config{};cfg.source=2;cfg.mutateStage=int(stage);cfg.mutateAddress=stage==T::Stage::ReadSignal?0xffeea0:stage==T::Stage::ReadSynchronized?0xffee8e:stage==T::Stage::ReadGain?0xffeea4:stage==T::Stage::ReadPreviousCoefficient?0xffee92:stage==T::Stage::ReadSourceFlags||stage==T::Stage::WriteSourceFlag?0xffee97:stage==T::Stage::ReadSource?0x169ef:stage==T::Stage::ReadInternalRate?0x16950:stage==T::Stage::ReadActive||stage==T::Stage::ReadActiveAgain?0xffee96:stage==T::Stage::ReadGainAgain?0xffeea4:stage==T::Stage::ReadPreviousPeriod?0xffee8a:0xffee7e;cfg.mutateBytes=stage==T::Stage::ReadSignal||stage==T::Stage::ReadGain?2:stage==T::Stage::ReadSynchronized||stage==T::Stage::ReadPreviousCoefficient?4:stage==T::Stage::ReadGainAgain?2:stage==T::Stage::ReadPeriod||stage==T::Stage::ReadPublishPeriod||stage==T::Stage::ReadPreviousPeriod?4:1;cfg.mutateValue=stage==T::Stage::ReadSource?3:stage==T::Stage::ReadActive?0:0x7ff; if(stage==T::Stage::ReadInternalRate||stage==T::Stage::ReadSourceFlags)cfg.source=0;run();}
 cfg=Config{};cfg.source=2;cfg.pairedFlip=true;run();
 for(unsigned which=0;which<9;++which){auto f=std::make_unique<ks::ClockSyncForeground>();b->pc=0x267a;b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->nativeLowSize=0x200000;b->interruptMode2=true;b->indent=1;b->setSP(0xfff800);switch(which){case 0:b->pc=0x267c;break;case 1:b->nativeFetchClocksOnly=false;break;case 2:b->nativeLowBase=nullptr;break;case 3:b->nativeLowSize=0x189ff;break;case 4:b->interruptMode2=false;break;case 5:b->indent=0;break;case 6:b->setSP(0xfff801);break;case 7:b->setSP(0xfff3fe);break;case 8:b->setSP(0xfffc00);break;}std::array<int32_t,8>regs{};for(unsigned n=0;n<8;++n)regs[n]=b->regs[n].er;const auto pc=b->getPC();const auto cycles=b->cycles;const auto flags=b->ccr;const auto exr=b->exr;const auto lr=b->lastread,lw=b->lastwrite,depth=b->indent;newBus.events.clear();tracking=true;check(!f->step(*b),"Guard admitted");tracking=false;check(newBus.events.empty()&&!f->active()&&!f->started&&pc==b->getPC()&&cycles==b->cycles&&flags==b->ccr&&exr==b->exr&&lr==b->lastread&&lw==b->lastwrite&&depth==b->indent,"Rejected entry mutated state");for(unsigned n=0;n<8;++n)check(regs[n]==b->regs[n].er,"Rejected entry mutated register");++guards;}
 check(!allocations,"Native allocations");check(components==15,"Incomplete child coverage");std::cout<<"PASS ClockSync actual Foreground H8 calls="<<calls<<" phases="<<phases<<" IRQ="<<irqs<<" live="<<lives<<" guards="<<guards<<" faults="<<faults<<" components="<<components<<" allocations="<<allocations<<'\n';return 0;
}catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}
