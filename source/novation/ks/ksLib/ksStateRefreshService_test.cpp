#include "ksStateRefreshForeground.h"
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
template<class Base>struct Bus:Base{std::vector<Raw>events;uint64_t statusReads=0,statusLimit=0,uartReads=0,uartWait=0;uint8_t read(uint32_t a)override{auto v=a==0x400004||a==0x600004?uint8_t(statusReads++<statusLimit?0:255):(a==0xffff8c?uint8_t((this->state->memory[a]&127)|(uartReads++<uartWait?0:128)):this->state->memory[a]);events.emplace_back(false,a,v,this->state->cycles,uint32_t(this->state->getPC()));return v;}void write(uint32_t a,uint8_t v)override{this->state->memory[a]=v;events.emplace_back(true,a,v,this->state->cycles,uint32_t(this->state->getPC()));}};
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}int history(uint32_t a){a&=0xffffff;return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
std::vector<Access>resources(const std::vector<Raw>&events,H8SDisassembler*decoder){std::vector<Access>out;size_t first=events.size(),count=0;if(decoder){for(size_t i=0;i<events.size();++i){const auto&e=events[i];if(!std::get<0>(e)&&!(std::get<4>(e)&1)&&std::get<1>(e)==std::get<4>(e)){first=i;const auto pc=std::get<1>(e);decoder->pc=decoder->makepc(pc);decoder->step();count=uint32_t(decoder->getPC())-pc;check(count>=2&&count<=10,"Instruction length");break;}}check(count,"Missing H8 fetch");}for(size_t i=0;i<events.size();++i){const auto&e=events[i];const auto at=std::get<1>(e);if((i>=first&&i<first+count)||(at>=0xfff000&&at<0xfff800)||(!std::get<0>(e)&&at>=28*4&&at<92*4))continue;out.emplace_back(std::get<0>(e),at,std::get<2>(e),std::get<3>(e));}return out;}

int main(int argc,char**argv){try{
 if(argc!=2)return 77;check(!ks::controllerReferenceAvailable(),"Synchronized H8 adapter linked");g_dasm=false;ks::Rom rom(argv[1]);check(rom.isValid(),"ROM");
 auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();auto decoder=std::make_unique<H8SDisassembler>();decoder->loadmem(rom.data().data(),rom.data().size(),0);
 Bus<H8SDevice>bus;Bus<ks::ControllerBus>nativeBus;a->memmap(&bus,0,1<<24);b->memmap(&nativeBus,0,1<<24);bus.events.reserve(512);nativeBus.events.reserve(512);
 for(unsigned base:{0u,0xa00000u})b->loadmem(rom.data().data(),rom.data().size(),base);
 ks::LiveMidiController midi(*b);auto runtimeOwner=std::make_unique<ks::RuntimeController>(*b,midi);auto&runtime=*runtimeOwner;auto owner=std::make_unique<ks::StateRefreshForeground>();auto&foreground=*owner;
 using T=ks::StateRefreshService;struct Config{unsigned seed=0,flags=0;uint8_t dirty=0,delay=1,pending=0,divider=0,source=0,mode=0,voices=0;uint32_t base=0x30000,voiceSource=0;uint64_t busy=0;bool live=false,caller=false,nested=false,liveVoice=false;};Config cfg;
 uint64_t calls=0,phases=0,preemptions=0,liveChanges=0,guards=0,faults=0,flashCases=0,flashReads=0,voiceGuards=0;uint32_t components=0;
 auto run=[&](unsigned inject=~0u,unsigned vector=32){
  std::array<unsigned,6>landings{};landings.fill(~0u);bool injected=false,nested=false,fault=false,voiceMutated=false;uint64_t caseFlashReads=0;uint32_t mutations=0,target=0x4338;
  runtime.resetMetrics();std::fill(a->memory,a->memory+(1<<24),uint8_t(0));for(unsigned at:{0u,0xa00000u})a->loadmem(rom.data().data(),rom.data().size(),at);
  const auto byte=[&](uint32_t at,uint8_t v){a->memory[at&0xffffff]=v;};const auto word=[&](uint32_t at,uint16_t v){byte(at,v>>8);byte(at+1,v);};const auto wide=[&](uint32_t at,uint32_t v){word(at,v>>16);word(at+2,v);};
  wide(0x167bc,cfg.base);wide(0x167c0,0x34000);wide(0x167a6,0);byte(0x1679c,cfg.dirty);byte(0x1679b,cfg.delay);byte(0x1679a,cfg.pending);byte(0x167d2,15);byte(0x167d3,uint8_t(cfg.seed));byte(0x167ba,cfg.mode);byte(0x16798,0);byte(0x17526,0);byte(0x174a6,cfg.voices);byte(0x1755e,255);byte(0x16784,0);byte(0x1679e,0);byte(0x16790,uint8_t(cfg.seed&3));
  for(unsigned part=0;part<5;++part){wide(0x174ae +20*part,cfg.base+256*part);for(unsigned n=4;n<20;++n)byte(0x174ae +20*part+n,uint8_t(cfg.seed+n));for(unsigned n=0;n<256;++n){byte(cfg.base+256*part+n,uint8_t((cfg.seed*19+n*3)%128));byte(0x34000+256*part+n,uint8_t(n));}for(unsigned n:{0u,139u,222u,223u,224u,225u,226u})byte(cfg.base+256*part+n,0);byte(cfg.base+256*part+73,uint8_t(cfg.seed%3));byte(cfg.base+256*part+76,uint8_t(cfg.seed%4));}
  for(unsigned slot=0;slot<256;++slot){const auto at=0x17560+20*slot;wide(at,cfg.voiceSource?cfg.voiceSource:cfg.base+256*(slot%5));byte(at+4,uint8_t(slot%5));word(at+8,uint16_t(slot<cfg.voices?1:0));byte(at+6,uint8_t(cfg.seed));byte(at+12,uint8_t(cfg.seed&7));}
  if(cfg.voiceSource||cfg.liveVoice){const auto target=cfg.voiceSource?cfg.voiceSource:0x81fd01u;for(unsigned n=0;n<256;++n)byte(target+n,a->memory[(cfg.base+n)&0xffffff]);}
  word(0x15138,0x4470);word(0x1512e,1);word(0x159f2,0xffff);wide(0x1748e,0x36000);byte(0x17492,1);byte(0x17493,0);for(unsigned n=0;n<64;++n)byte(0x36000+n,uint8_t(cfg.seed+n));for(unsigned n=0;n<16;++n){byte(0x36000+16+2*n,uint8_t(16+n));byte(0x36000+17+2*n,uint8_t(n&1));byte(0x17495+n,uint8_t(cfg.seed+n));}
  byte(0xffee98,cfg.divider);byte(0x169ef,cfg.source);byte(0x16950,uint8_t(cfg.seed));byte(0xffee96,uint8_t(cfg.seed&1));byte(0xffee97,uint8_t(cfg.seed));word(0xffeea0,0x400);word(0xffeea4,0x100);wide(0xffee7e,0x100000);wide(0xffee8e,0x100000);wide(0xffee8a,cfg.seed&2?0x100000:0);wide(0xffee92,cfg.seed&4?0x12345:0);wide(0xfff800,target);
  for(unsigned n=0;n<8;++n)a->regs[n].er=int32_t(0x92345678u+cfg.seed*19+n*0x1111u);a->setSP(0xfff800);a->pc=a->makepc(0x437a);a->cycles=1000;a->ccr=uint8_t(cfg.flags);a->exr=0xa0;a->indent=1;a->lastread=0x1234;a->lastwrite=0x2345;a->interruptMode2=true;a->pending_irqs=a->pending_irqs_high=0;for(auto&p:a->interruptPriority)p=0;
  std::copy(a->memory,a->memory+(1<<24),b->memory);for(unsigned n=0;n<8;++n)b->regs[n].er=a->regs[n].er;b->pc=0x437a;b->cycles=1000;b->ccr=a->ccr;b->exr=a->exr;b->indent=1;b->lastread=a->lastread;b->lastwrite=a->lastwrite;b->interruptMode2=true;b->pending_irqs=b->pending_irqs_high=0;for(auto&p:b->interruptPriority)p=0;b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->nativeLowSize=0x200000;b->interpreterAllowed=false;foreground.resetMetrics();
  a->interruptPriority[vector]=b->interruptPriority[vector]=6;bus.statusReads=nativeBus.statusReads=0;bus.statusLimit=nativeBus.statusLimit=cfg.busy;bus.uartReads=nativeBus.uartReads=0;
  std::array<int32_t,8>held{};for(unsigned n=0;n<8;++n)held[n]=b->regs[n].er;const auto heldFlags=b->ccr;
  const auto bothByte=[&](uint32_t at,uint8_t v){a->memory[at&0xffffff]=b->memory[at&0xffffff]=v;};const auto bothWord=[&](uint32_t at,uint16_t v){bothByte(at,v>>8);bothByte(at+1,v);};
  for(auto*memory:{a->memory,b->memory}){memory[0xffec14]=1;memory[0xffffd5]=0xc0;memory[0xffff8a]=0xf0;memory[0xffff8c]=0xb7;memory[0xffff8d]=0xf8;memory[0xffffe6]=0x12;memory[0xffffe7]=0x34;for(unsigned n=0;n<36;++n){memory[0xb918+n]=uint8_t(n);memory[0xb93c+n]=uint8_t(n&1);}memory[0xffff55]=255;memory[0xffff54]=12;memory[0xffff59]=0xe0;memory[0xffff50]=31;memory[0xffed6c]=memory[0xffed6e]=0;memory[0xffed6d]=45;memory[0xffed6f]=17;memory[0xffff62]=255;memory[0xfffeb1]=255;}
  for(auto at:{0x1537cu,0x15406u,0x156e8u}){bothWord(at,2);bothWord(at+2,0);}for(unsigned n=0;n<4;++n){bothWord(0xffff90 + 2*n,0x8000);bothWord(0x15652 + 2*n,0x8000);bothWord(0x1560a + 2*n,0);bothByte(0x1569a + 2*n,0);}bothByte(0x156e2,0);bothByte(0x156e3,0);bothByte(0x156e4,255);bothByte(0xffff98,0x73);
  uint64_t clocks=1000;unsigned count=0;do{
   const auto pc=uint32_t(a->getPC());if(count==inject){if(vector<64){a->pending_irqs|=1ull<<vector;b->pending_irqs|=1ull<<vector;}else{a->pending_irqs_high|=1ull<<(vector-64);b->pending_irqs_high|=1ull<<(vector-64);}injected=true;}
   if(cfg.nested&&injected&&!nested&&runtime.activeNativeInterrupts()&&(b->exr&7)==6&&count>inject+1){a->interruptPriority[89]=b->interruptPriority[89]=7;a->pending_irqs_high|=1ull<<25;b->pending_irqs_high|=1ull<<25;nested=true;}
   if(cfg.caller&&count==5){bothByte(0xfff803,0x3a);target=0x433a;}
   if(cfg.live&&!runtime.activeNativeInterrupts())if(const auto*t=foreground.task<ks::StateForegroundTask>()){using S=ks::StateForegroundTask::Stage;
    if(t->stage()==S::ReadRefreshFlags&&!(mutations&1)){bothByte(0x167d3,0xa5);mutations|=1;}
    if(t->stage()==S::ReadDirty&&!(mutations&2)){bothByte(0x1679c,1);mutations|=2;}
    if(t->stage()==S::RefreshParts&&!(mutations&4)){bothByte(0x1679b,0);bothByte(0x1679a,3);mutations|=4;}
    if(t->stage()==S::RefreshClock&&!(mutations&8)){bothByte(0xffee98,1);bothByte(0x169ef,1);mutations|=8;}}
   if(cfg.liveVoice&&!voiceMutated&&!runtime.activeNativeInterrupts())if(const auto*t=foreground.task<ks::VoiceRefreshTask>())if(t->stage()==ks::VoiceRefreshTask::Stage::ReadPatch){for(unsigned n=0;n<4;++n)bothByte(0x17560+n,uint8_t(0x81fd01u>>(24-8*n)));voiceMutated=true;}
   if(foreground.task<ks::DspFaultTask>())fault=true;if(count==3){foreground.resetMetrics();runtime.resetMetrics();}if(const auto*s=foreground.service()){const auto n=unsigned(s->component());if(landings[n]==~0u)landings[n]=count;if(s->task<ks::ParameterUploadService>()&&landings[5]==~0u)landings[5]=count;}
   bus.events.clear();a->step();const auto expected=resources(bus.events,decoder.get());nativeBus.events.clear();const auto begin=b->cycles;bool admit=false;for(unsigned irq=0;irq<128;++irq){const auto bits=irq<64?b->pending_irqs:b->pending_irqs_high;if((bits&(1ull<<(irq&63)))&&b->interruptPriority[irq]>(b->exr&7))admit=true;}
   tracking=true;try{if(runtime.activeNativeInterrupts()||admit)runtime.step(*b);else check(foreground.step(*b),"Refresh foreground declined");}catch(...){tracking=false;std::cerr<<"Exception seed="<<cfg.seed<<" PC="<<std::hex<<pc<<std::dec<<" phase="<<count<<'\n';for(const auto&e:expected)std::cerr<<" expected="<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<std::dec;std::cerr<<'\n';throw;}tracking=false;
   clocks+=b->cycles-begin;for(const auto&e:nativeBus.events){const auto at=std::get<1>(e);if(std::get<0>(e)){check(at<0xfff000||at>=0xfffc00,"Native refresh wrote CPU scratch");check(at<0x800000||at>=0x900000,"Native refresh wrote voice Flash");}else if(at>=0x800000&&at<0x900000)++caseFlashReads;}const auto actual=resources(nativeBus.events,nullptr);if(foreground.active()||runtime.activeNativeInterrupts()){check(b->pc==0x437a&&b->getSP()==0xfff800&&b->ccr==heldFlags,"Refresh used CPU frame");for(unsigned n=0;n<8;++n)check(b->regs[n].er==held[n],"Refresh used registers");}
   check(a->exr==b->exr,"Refresh priority");check(a->pending_irqs==b->pending_irqs&&a->pending_irqs_high==b->pending_irqs_high,"Refresh pending");
   if(a->cycles!=clocks||expected!=actual){std::cerr<<"Mismatch seed="<<cfg.seed<<" PC="<<std::hex<<pc<<std::dec<<" phase="<<count<<" component="<<(foreground.service()?unsigned(foreground.service()->component()):99)<<" clocks="<<a->cycles<<'/'<<clocks<<'\n';for(const auto&rows:{expected,actual}){for(const auto&e:rows)std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<std::dec<<' ';std::cerr<<'\n';}throw std::runtime_error("Refresh resources/time differ");}check(++count<12000000,"Refresh stalled");++phases;
  }while(uint32_t(a->getPC())!=target||a->indent);
  check(!foreground.active()&&!runtime.activeNativeInterrupts()&&b->pc==target&&!b->indent,"Native refresh return");for(unsigned n=0;n<8;++n)if(a->regs[n].er!=b->regs[n].er){std::cerr<<"ABI seed="<<cfg.seed<<" reg="<<n<<" expected/native="<<std::hex<<a->regs[n].er<<'/'<<b->regs[n].er<<std::dec<<'\n';throw std::runtime_error("Refresh final ABI");}
  if(a->ccr!=b->ccr){std::cerr<<"CCR seed="<<cfg.seed<<" dirty="<<unsigned(cfg.dirty)<<" delay="<<unsigned(cfg.delay)<<" pending="<<unsigned(cfg.pending)<<" divider="<<unsigned(cfg.divider)<<" source="<<unsigned(cfg.source)<<" expected/native="<<std::hex<<unsigned(a->ccr)<<'/'<<unsigned(b->ccr)<<std::dec<<'\n';throw std::runtime_error("Refresh final flags");}
  check(a->lastread==b->lastread&&a->lastwrite==b->lastwrite,"Refresh final history");check(!std::memcmp(a->memory,b->memory,0xfff000)&&!std::memcmp(a->memory+0xfffc00,b->memory+0xfffc00,0x400),"Refresh final memory");check(!b->interpretedSteps,"Refresh interpreter");check(foreground.initial+foreground.started==foreground.completed,"Refresh metrics");check(foreground.returnedPacket==1&&foreground.returnedDeferred==1&&foreground.returnedClock==1,"Refresh return-owned children");check(foreground.returnedPanel==unsigned(cfg.dirty||cfg.live),"Refresh returned panel witness");components|=foreground.components;++calls;if(inject!=~0u)check(injected,"Refresh IRQ landing absent");if(injected)++preemptions;if(cfg.nested)check(nested,"Refresh nested IRQ absent");if(cfg.live){check(mutations==15,"Refresh evolving reads missed");liveChanges+=4;}if(cfg.busy)check(fault,"Refresh DSP fault path absent");if(fault)++faults;if(cfg.voiceSource||cfg.liveVoice){check(caseFlashReads>0,"Refresh Flash voice not read");if(cfg.liveVoice){check(voiceMutated,"Refresh live Flash pointer missed");++liveChanges;}++flashCases;flashReads+=caseFlashReads;}return landings;
 };
 for(unsigned flags=0;flags<256;++flags)for(unsigned route=0;route<4;++route){cfg=Config{};cfg.flags=flags;cfg.seed=flags;cfg.dirty=uint8_t(route&1);cfg.delay=uint8_t(route>>1);cfg.divider=uint8_t(route&1);cfg.source=uint8_t(flags%4);run();}
 for(unsigned pending=0;pending<256;++pending){cfg=Config{};cfg.delay=0;cfg.pending=uint8_t(pending);cfg.seed=pending;cfg.mode=uint8_t(pending&1);cfg.divider=1;run();}
 for(unsigned dirty:{128u,255u}){cfg=Config{};cfg.dirty=uint8_t(dirty);run();}
 for(unsigned raw:{0x80030000u,0xa30000u,0xffdc00u,0x42030001u}){cfg=Config{};cfg.base=raw;cfg.dirty=1;run();}
 for(unsigned route=0;route<3;++route){cfg=Config{};cfg.dirty=uint8_t(route==1);cfg.delay=0;cfg.pending=uint8_t(route==2?3:0);cfg.divider=1;const auto points=run();for(unsigned vector:{28u,32u,88u,89u,90u,91u})for(auto point:points)if(point!=~0u)run(point,vector);}
 cfg=Config{};cfg.live=true;run();cfg.live=false;cfg.nested=true;run(0,32);cfg=Config{};cfg.caller=true;run();cfg=Config{};cfg.busy=65538;run();cfg=Config{};cfg.dirty=1;cfg.delay=0;cfg.pending=3;cfg.voices=1;cfg.divider=1;run();
 // Controls/current remain RAM; only voice-table patch sources use Flash.
 for(unsigned raw:{0x800000u,0x81fd01u,0x8fff00u,0xc181fd01u})for(unsigned dirty:{0u,1u})for(unsigned flags:{0u,255u}){cfg=Config{};cfg.delay=0;cfg.pending=1;cfg.voices=1;cfg.voiceSource=raw;cfg.dirty=uint8_t(dirty);cfg.flags=flags;run();}
 cfg=Config{};cfg.delay=0;cfg.pending=1;cfg.voices=1;cfg.voiceSource=0x81fd01;cfg.dirty=1;{const auto points=run();check(points[5]!=~0u,"Flash upload IRQ boundary absent");run(points[5],32);}
 cfg=Config{};cfg.delay=0;cfg.pending=1;cfg.voices=1;cfg.liveVoice=true;run();
 cfg=Config{};cfg.delay=0;cfg.pending=1;cfg.voices=1;cfg.voiceSource=0x81fd01;cfg.busy=65538;run();
 // Actual public edge rejection must precede job creation or resource effects.
 const auto reject=[&](auto mutate,auto restore){
  b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->nativeLowSize=0x200000;b->interruptMode2=true;b->indent=1;b->pc=0x437a;b->setSP(0xfff800);mutate();
  std::array<int32_t,8>before{};for(unsigned n=0;n<8;++n)before[n]=b->regs[n].er;const auto pc=b->pc;const auto ccr=b->ccr;const auto exr=b->exr;const auto depth=b->indent;const auto cycles=b->cycles;const auto started=foreground.started,completed=foreground.completed;const auto lr=b->lastread,lw=b->lastwrite;nativeBus.events.clear();
  check(!foreground.step(*b),"Refresh accepted invalid admission");check(!foreground.active()&&foreground.started==started&&foreground.completed==completed,"Rejected refresh changed ownership");check(nativeBus.events.empty()&&b->cycles==cycles&&b->pc==pc&&b->ccr==ccr&&b->exr==exr&&b->indent==depth&&b->lastread==lr&&b->lastwrite==lw,"Rejected refresh changed CPU/resources");for(unsigned n=0;n<8;++n)check(b->regs[n].er==before[n],"Rejected refresh changed register");restore();++guards;
 };
 const auto noop=[]{};
 reject([&]{b->nativeFetchClocksOnly=false;},noop);reject([&]{b->nativeLowBase=nullptr;},noop);reject([&]{b->nativeLowSize=0x175ff;},noop);reject([&]{b->interruptMode2=false;},noop);reject([&]{b->indent=0;},noop);reject([&]{b->pc=0x437c;},noop);
 for(unsigned sp:{0xfff801u,0xfff300u,0xfffc00u})reject([&]{b->setSP(sp);},noop);
 for(const auto&schema:ks::StateReloadPanelService::schema)for(unsigned n=0;n<8;++n){const auto at=0xfd66+8*schema.key+n;const auto old=b->memory[at];reject([&]{b->memory[at]=uint8_t(old^128);},[&]{b->memory[at]=old;});}
 for(unsigned c:{0u,2u,3u,4u,6u})for(unsigned n=0;n<4;++n){const auto at=0x1013e +4*c+n;const auto old=b->memory[at];reject([&]{b->memory[at]=uint8_t(old^128);},[&]{b->memory[at]=old;});}
 const auto nativeWide=[&](uint32_t at,uint32_t value){for(unsigned n=0;n<4;++n)b->memory[at+n]=uint8_t(value>>(24-8*n));};
 for(uint32_t bad:{0u,0x9662u,0xffefffu,0xfff000u,0xfff700u,0xffff51u,0x800000u,0x81fd01u,0x8fff00u,0xc181fd01u}){
  reject([&]{b->memory[0x1679c]=1;nativeWide(0x167bc,bad);},[&]{b->memory[0x1679c]=0;nativeWide(0x167bc,0x30000);});
  reject([&]{b->memory[0x1679b]=0;b->memory[0x1679a]=1;nativeWide(0x174ae,bad);},[&]{b->memory[0x1679b]=1;b->memory[0x1679a]=0;nativeWide(0x174ae,0x30000);});
 }
 // Invalid voice patch is observed after ownership and rejected at the actual
 // ReadPatch before any derived patch read; legitimate earlier effects remain.
 for(uint32_t bad:{0u,0x437au,0x8fff01u,0x900000u,0xffefffu,0xfff700u,0xc18fff01u}){
  b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->nativeLowSize=0x200000;b->interruptMode2=true;b->indent=1;b->pc=0x437a;b->setSP(0xfff800);b->memory[0x1679c]=0;b->memory[0x1679b]=0;b->memory[0x1679a]=1;b->memory[0x174a6]=1;b->memory[0x17526]=0;b->memory[0x17564]=0;b->memory[0x17568]=0;b->memory[0x17569]=1;nativeWide(0x174ae,0x30000);nativeWide(0x17560,0x30000);
  auto f=std::make_unique<ks::StateRefreshForeground>();unsigned count=0;for(;;){const auto*t=f->task<ks::VoiceRefreshTask>();if(t&&t->stage()==ks::VoiceRefreshTask::Stage::ReadPatch)break;check(f->step(*b),"Invalid voice prefix declined");check(++count<4096,"Invalid voice landing absent");}
  nativeWide(0x17560,bad);nativeBus.events.clear();bool rejected=false;try{f->step(*b);}catch(const std::logic_error&){rejected=true;}check(rejected&&f->active()&&!f->completed,"Invalid evolving voice patch accepted/fell back");check(nativeBus.events.size()==4,"Invalid voice pointer phase shape");for(const auto&e:nativeBus.events)check(!std::get<0>(e)&&std::get<1>(e)>=0x17560&&std::get<1>(e)<0x17564,"Invalid voice patch used derived resource");++voiceGuards;
 }
 check(allocations==0,"Refresh allocation");check(components==31,"Refresh child coverage");std::cout<<"PASS Refresh H8 calls="<<calls<<" phases="<<phases<<" IRQ="<<preemptions<<" live="<<liveChanges<<" guards="<<guards<<" faults="<<faults<<" flashCases="<<flashCases<<" flashReads="<<flashReads<<" voiceGuards="<<voiceGuards<<" components="<<components<<" allocations="<<allocations<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
