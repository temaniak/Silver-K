#include "ksNativeVoiceReleaseForeground.h"
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
template<class Base>struct Bus:Base{std::vector<Raw>events;uint64_t statusReads=0,statusLimit=0;uint32_t reply=0x123408;uint8_t read(uint32_t a)override{auto v=a==0x400004||a==0x600004?uint8_t(statusReads++<statusLimit?0:255):(a==0x60000a?uint8_t(reply>>16):a==0x60000c?uint8_t(reply>>8):a==0x60000e?uint8_t(reply):this->state->memory[a]);events.emplace_back(false,a,v,this->state->cycles,uint32_t(this->state->getPC()));return v;}void write(uint32_t a,uint8_t v)override{this->state->memory[a]=v;events.emplace_back(true,a,v,this->state->cycles,uint32_t(this->state->getPC()));}};
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}int history(uint32_t a){a&=0xffffff;return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
std::vector<Access>resources(const std::vector<Raw>&events,H8SDisassembler*decoder){std::vector<Access>out;size_t first=events.size(),count=0;if(decoder){for(size_t i=0;i<events.size();++i){const auto&e=events[i];if(!std::get<0>(e)&&!(std::get<4>(e)&1)&&std::get<1>(e)==std::get<4>(e)){first=i;const auto pc=std::get<1>(e);decoder->pc=decoder->makepc(pc);decoder->step();count=uint32_t(decoder->getPC())-pc;check(count>=2&&count<=10,"Instruction length");break;}}check(count,"Missing H8 fetch");}for(size_t i=0;i<events.size();++i){const auto&e=events[i];const auto at=std::get<1>(e);if((i>=first&&i<first+count)||(at>=0xfff000&&at<0xfff800)||(!std::get<0>(e)&&at>=28*4&&at<92*4))continue;out.emplace_back(std::get<0>(e),at,std::get<2>(e),std::get<3>(e));}return out;}
int main(int argc,char**argv){try{
 if(argc!=2)return 77;g_dasm=false;ks::Rom rom(argv[1]);check(rom.isValid(),"ROM");check(!ks::controllerReferenceAvailable(),"Reference adapter unexpectedly linked");
 auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();auto decoder=std::make_unique<H8SDisassembler>();for(unsigned at:{0u,0xa00000u}){a->loadmem(rom.data().data(),rom.data().size(),at);b->loadmem(rom.data().data(),rom.data().size(),at);}decoder->loadmem(rom.data().data(),rom.data().size(),0);
 Bus<H8SDevice>x;Bus<ks::ControllerBus>y;x.events.reserve(256);y.events.reserve(256);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);ks::LiveMidiController midi(*b);auto rt=std::make_unique<ks::RuntimeController>(*b,midi);ks::NativeVoiceReleaseForeground f;
 using K=ks::VoiceReleaseTask::Mode;using C=ks::NativeVoiceReleaseService::Component;
 struct Config{K kind=K::Note;unsigned count=4,age=0x4012,part=2,owner=2,note=60,slotNote=60,extra=0,patch=0x18000,patchFlags=0,pedal=0,pending=1,cut=0,flags=0xa5,priority=0,busy=0;unsigned live=0,mixed=0;bool caller=false,nested=false,reentry=false,reset=false;int previousWrite=-1;};Config cfg;
 uint64_t cases=0,phases=0,irqCases=0,nestedCases=0,liveCases=0,reentries=0,resets=0,faults=0;uint32_t components=0;
 auto run=[&](unsigned inject=~0u,unsigned vector=32){
  rt->resetMetrics();f.resetMetrics();check(!f.active(),"Previous release active");const unsigned roots[]{0x92c0,0x9374,0x936e,0x9400,0x9376};const unsigned root=roots[unsigned(cfg.kind)];std::array<int32_t,8>saved{};
  for(unsigned n=0;n<8;++n)saved[n]=a->regs[n].er=b->regs[n].er=int32_t((std::array<uint32_t,5>{0u,0x80000000u,0xffffffffu,0x7fffffffu,0x12345678u})[(n+cfg.flags)%5]);a->regs[5].rl=b->regs[5].rl=int8_t(cfg.note);a->regs[2].r=b->regs[2].r=int16_t(cfg.cut);saved[5]=b->regs[5].er;saved[2]=b->regs[2].er;saved[7]=0xfff800;
  a->pc=a->makepc(root);a->setSP(0xfff800);a->cycles=1000;a->indent=1;a->ccr=uint8_t(cfg.flags);a->exr=uint8_t(cfg.priority);a->interruptMode2=true;a->lastread=-1;a->lastwrite=cfg.previousWrite;a->pending_irqs=a->pending_irqs_high=0;for(auto&p:a->interruptPriority)p=0;a->interruptPriority[vector]=6;
  b->pc=root;b->setSP(0xfff800);b->cycles=1000;b->indent=1;b->ccr=uint8_t(cfg.flags);b->exr=uint8_t(cfg.priority);b->interruptMode2=true;b->lastread=-1;b->lastwrite=cfg.previousWrite;b->pending_irqs=b->pending_irqs_high=0;for(auto&p:b->interruptPriority)p=0;b->interruptPriority[vector]=6;b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->nativeLowSize=0x200000;b->interpreterAllowed=false;
  x.statusReads=y.statusReads=0;x.statusLimit=y.statusLimit=cfg.busy;
  for(auto*m:{a->memory,b->memory}){
   std::fill(m+0x15000,m+0x30000,uint8_t(0));std::fill(m+0xffec00,m+(1<<24),uint8_t(0));const auto byte=[&](unsigned at,unsigned v){m[at]=uint8_t(v);};const auto word=[&](unsigned at,unsigned v){byte(at,v>>8);byte(at+1,v);};const auto wide=[&](unsigned at,uint32_t v){word(at,v>>16);word(at+2,v);};
   wide(0xfff800,0x4338);std::fill(m+0xfff000,m+0xfff800,uint8_t(0x5a));byte(0x17526,cfg.part);byte(0x174a6,cfg.count);for(unsigned i=0;i<256;++i){wide(0x17560+20*i,cfg.patch);byte(0x17560+20*i+4,cfg.owner);byte(0x17560+20*i+5,cfg.slotNote);word(0x17560+20*i+8,cfg.age);byte(0x17560+20*i+17,cfg.pending);byte(0x17560+20*i+18,cfg.mixed?((i+cfg.mixed)&1):cfg.extra);}byte(cfg.patch+88,cfg.patchFlags);byte(0x160d2+334*cfg.part+309,cfg.pedal);word(0x159f2,0x2468);
   byte(0xffec14,1);byte(0xffee70,128);wide(0xffec10,7);byte(0xffec23,1);wide(0xffee7a,0xffffffff);wide(0xffee7e,1);word(0xffee78,0xffff);wide(0xffee82,0xffffffff);wide(0xffee86,0x34);byte(0xffee97,0);byte(0xffee9a,255);byte(0xffff8c,0xf7);byte(0xffff8a,0xf0);word(0xffec64,0);word(0xffec66,0);byte(0xffed68,0);byte(0xffff8d,0x42);byte(0xffffd5,0xc0);byte(0xffff55,255);byte(0xffff54,12);byte(0xffff59,0xe0);byte(0xffff50,31);byte(0xffff98,0x73);
   for(unsigned n=0;n<36;++n){byte(0xb918+n,n);byte(0xb93c+n,n&1);}for(unsigned n=0;n<4;++n){byte(0xffec16+n,1u<<n);byte(0xffec24+n,128u>>n);word(0xffff90+2*n,0x8000);word(0x15652+2*n,0x8000);word(0x1560a+2*n,0);word(0x1569a+2*n,0);}byte(0x156e4,255);
  }
  auto bothByte=[&](unsigned at,unsigned v){a->memory[at]=b->memory[at]=uint8_t(v);};auto bothWord=[&](unsigned at,unsigned v){bothByte(at,v>>8);bothByte(at+1,v);};
  unsigned count=0,liveMask=0,target=0x4338;bool injected=false,nested=false,reentered=false,reset=false,fault=false;std::array<unsigned,10>landings{};landings.fill(~0u);
  do{const auto pc=uint32_t(a->getPC());if(count==inject){if(vector<64){a->pending_irqs|=1ull<<vector;b->pending_irqs|=1ull<<vector;}else{a->pending_irqs_high|=1ull<<(vector-64);b->pending_irqs_high|=1ull<<(vector-64);}injected=true;}
   if(cfg.nested&&injected&&!nested&&rt->activeNativeInterrupts()&&(b->exr&7)==6){a->interruptPriority[89]=b->interruptPriority[89]=7;a->pending_irqs_high|=1ull<<25;b->pending_irqs_high|=1ull<<25;nested=true;}
   if(cfg.caller&&count==5){bothByte(0xfff800,0xa5);bothByte(0xfff803,0x3a);target=0x433a;}
   if(cfg.live&&!rt->activeNativeInterrupts()){
    auto once=[&](unsigned bit,unsigned p,auto&&change){if(!(liveMask&(1u<<bit))&&pc==p){change();liveMask|=1u<<bit;}};
    if(cfg.live==1){once(0,0x92de,[&]{bothWord(0x17560+8,0x4013);});once(1,0x930a,[&]{bothWord(0x17560,1);bothWord(0x17562,0x8200);bothByte(0x18258,0);});once(2,0x9324,[&]{bothByte(0x160d2+334*cfg.part+309,127);});}
    else if(cfg.live==2){once(0,0x940e,[&]{bothByte(0x174a6,1);});once(1,0x941c,[&]{bothWord(0x17560+8,0x4013);});once(2,0x9432,[&]{bothByte(0x17560+17,0);});}
    else {once(0,0x9376,[&]{bothByte(0x17526,1);});once(1,0x9384,[&]{bothByte(0x174a6,1);});once(2,0x9394,[&]{bothWord(0x17560+8,0x4013);});}
   }

   if(const auto*s=f.service()){auto n=unsigned(s->component());if(landings[n]==~0u)landings[n]=count;if(s->task<ks::DspFaultTask>())fault=true;if(cfg.reset&&!reset&&n==unsigned(C::Route)){f.resetMetrics();reset=true;}if(cfg.reentry&&!reentered&&n==unsigned(C::Lookup)){const auto clock=b->cycles;const auto depth=b->indent;const auto fs=f.phases;++b->indent;y.events.clear();tracking=true;check(!f.step(*b),"Release wrong-depth admitted");tracking=false;b->indent=depth;check(clock==b->cycles&&fs==f.phases&&y.events.empty(),"Release wrong-depth effects");reentered=true;}}
   x.events.clear();y.events.clear();a->step();bool admit=false;for(unsigned irq=0;irq<128;++irq){const auto bits=irq<64?b->pending_irqs:b->pending_irqs_high;if((bits&(1ull<<(irq&63)))&&b->interruptPriority[irq]>(b->exr&7))admit=true;}tracking=true;if(rt->activeNativeInterrupts()||admit)rt->step(*b);else check(f.step(*b),"Release Fore declined");tracking=false;
   const auto expected=resources(x.events,decoder.get()),actual=resources(y.events,nullptr);if(a->cycles!=b->cycles||expected!=actual){std::cerr<<"case="<<cases<<" phase="<<count<<" pc="<<std::hex<<pc<<std::dec<<" clocks="<<a->cycles<<'/'<<b->cycles<<'\n';for(const auto&rows:{expected,actual}){for(const auto&e:rows)std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<std::dec<<' ';std::cerr<<'\n';}throw std::runtime_error("Release phase resource/time differs");}
   check(a->exr==b->exr&&a->pending_irqs==b->pending_irqs&&a->pending_irqs_high==b->pending_irqs_high,"Release IRQ masks/pending differ");if(f.active()||rt->activeNativeInterrupts()){check(b->pc==root&&b->getSP()==0xfff800&&b->ccr==uint8_t(cfg.flags),"Release CPU continuation");for(unsigned n=0;n<8;++n)check(b->regs[n].er==saved[n],"Release CPU register shadow");for(unsigned at=0xfff000;at<0xfff800;++at)check(b->memory[at]==0x5a,"Release CPU stack write");}
   ++phases;check(++count<1000000,"Release stalled");
  }while(uint32_t(a->getPC())!=target||a->indent);
  check(!f.active()&&!rt->activeNativeInterrupts()&&b->pc==target&&!b->indent,"Release did not return");for(unsigned n=0;n<8;++n)if(a->regs[n].er!=b->regs[n].er){std::cerr<<"ABI case="<<cases<<" r="<<n<<" old/new="<<std::hex<<a->regs[n].er<<'/'<<b->regs[n].er<<std::dec<<'\n';throw std::runtime_error("Release final register differs");}if(a->ccr!=b->ccr)std::cerr<<"case="<<cases<<" mode="<<unsigned(cfg.kind)<<" part="<<cfg.part<<" count="<<cfg.count<<" age="<<cfg.age<<" owner="<<cfg.owner<<" note="<<cfg.slotNote<<" input="<<cfg.note<<" pedal="<<cfg.pedal<<" flags="<<cfg.flags<<" CCR="<<unsigned(a->ccr)<<'/'<<unsigned(b->ccr)<<'\n';check(a->ccr==b->ccr,"Release flags differ");check(a->lastread==b->lastread&&a->lastwrite==b->lastwrite,"Release final histories differ");check(!std::memcmp(a->memory,b->memory,0xfff000)&&!std::memcmp(a->memory+0xfffc00,b->memory+0xfffc00,0x400),"Release full nonscratch memory differs");check(f.started+f.initial==f.completed,"Release lifetime");check(!b->interpretedSteps,"Release interpreter");if(injected)++irqCases;if(cfg.nested){check(nested,"Release nested IRQ missed");++nestedCases;}if(cfg.live){check(liveMask==7,"Release live observations missed");++liveCases;}if(cfg.reentry){check(reentered,"Release reentry missed");++reentries;}if(cfg.reset){check(reset&&f.initial==1&&f.started==0&&f.completed==1,"Release metric reset missed");++resets;}if(cfg.busy)check(fault,"Release timeout missed");if(fault)++faults;components|=f.components;++cases;return std::make_pair(landings,count);
 };
 for(unsigned flags=0;flags<256;++flags)for(auto mode:{K::Note,K::Part,K::FastPart,K::Pedal,K::ProvidedPart}){cfg=Config{};cfg.kind=mode;cfg.flags=flags;cfg.cut=uint16_t(flags*257);cfg.pending=flags;cfg.pedal=flags;cfg.patchFlags=flags;cfg.age=(flags&1)?0x8000:0x4012;run();}
 for(auto mode:{K::Note,K::Part,K::FastPart,K::Pedal,K::ProvidedPart})for(unsigned age:{0u,1u,0x4000u,0x7fffu,0x8000u,0xffffu})for(unsigned owner:{1u,2u,255u})for(unsigned note:{59u,60u})for(unsigned pedal:{0u,127u}){cfg=Config{};cfg.kind=mode;cfg.age=age;cfg.owner=owner;cfg.slotNote=note;cfg.pedal=pedal;run();}
 for(auto mode:{K::Note,K::Part,K::FastPart,K::Pedal,K::ProvidedPart})for(unsigned part:{0u,1u,2u,4u,127u,255u})for(unsigned count:{0u,1u,2u,14u}){cfg=Config{};cfg.kind=mode;cfg.part=part;cfg.owner=part;cfg.count=count;run();}
 for(unsigned note=0;note<256;++note){cfg=Config{};cfg.note=note;cfg.slotNote=note;cfg.extra=note;run();}
 for(auto mode:{K::Note,K::Part,K::FastPart,K::Pedal,K::ProvidedPart}){
  cfg=Config{};cfg.kind=mode;auto baseline=run();for(unsigned phase=0;phase<baseline.second;++phase)run(phase,32);
  for(auto vector:{28u,32u,88u,89u,90u,91u}){cfg=Config{};cfg.kind=mode;run(1,vector);}cfg.nested=true;run(1,32);
  cfg=Config{};cfg.kind=mode;cfg.caller=true;run();cfg=Config{};cfg.kind=mode;cfg.reset=true;run();cfg=Config{};cfg.kind=mode;cfg.reentry=true;run();
 }
 cfg=Config{};cfg.busy=65538;run();
 for(unsigned live=1;live<=3;++live){cfg=Config{};cfg.live=live;cfg.kind=live==1?K::Note:live==2?K::Pedal:K::Part;run();}
 for(unsigned mixed:{1u,2u}){cfg=Config{};cfg.mixed=mixed;cfg.pedal=127;run();}
 uint64_t guards=0;for(unsigned v=0;v<10;++v){ks::NativeVoiceReleaseForeground rejected;b->pc=0x92c0;b->setSP(0xfff800);b->indent=1;b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->nativeLowSize=0x200000;b->interruptMode2=true;switch(v){case 0:b->nativeFetchClocksOnly=false;break;case 1:b->nativeLowBase=nullptr;break;case 2:b->nativeLowSize=0x2ffff;break;case 3:b->interruptMode2=false;break;case 4:b->indent=0;break;case 5:b->pc=0x92c2;break;case 6:b->setSP(0xfff3fe);break;case 7:b->setSP(0xfffc00);break;case 8:b->setSP(0xfff801);break;case 9:b->setSP(0xffffffff);break;}const auto clock=b->cycles;const auto pc=b->pc,sp=b->getSP();const auto lr=b->lastread,lw=b->lastwrite;const auto flags=b->ccr,mask=b->exr;std::array<int32_t,8>before{};for(unsigned n=0;n<8;++n)before[n]=b->regs[n].er;y.events.clear();tracking=true;check(!rejected.step(*b),"Release guard admitted");tracking=false;check(!rejected.started&&!rejected.active()&&y.events.empty()&&clock==b->cycles&&pc==b->pc&&sp==b->getSP()&&lr==b->lastread&&lw==b->lastwrite&&flags==b->ccr&&mask==b->exr,"Release guard changed state");for(unsigned n=0;n<8;++n)check(before[n]==b->regs[n].er,"Release guard changed registers");++guards;}
 unsigned liveGuards=0;for(uint32_t invalid:{0x100u,0x1ffff0u,0xfff000u,0xfff7ffu}){
  ks::NativeVoiceReleaseForeground bad;b->pc=0x92c0;b->setSP(0xfff800);b->indent=1;b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->nativeLowSize=0x200000;b->interruptMode2=true;b->exr=0;b->pending_irqs=b->pending_irqs_high=0;b->regs[5].rl=60;b->memory[0x17526]=2;b->memory[0x174a6]=1;const auto at=0x17560u;for(unsigned j=0;j<4;++j)b->memory[at+j]=uint8_t(invalid>>(24-8*j));b->memory[at+4]=2;b->memory[at+5]=60;b->memory[at+8]=0;b->memory[at+9]=1;b->memory[at+18]=0;
  unsigned steps=0;while(true){const auto*t=bad.task<ks::VoiceReleaseTask>();if(t&&(t->stage()==ks::VoiceReleaseTask::Stage::ReadPatchEarly||t->stage()==ks::VoiceReleaseTask::Stage::ReadPatchLate))break;check(bad.step(*b),"Bad-patch prefix declined");check(++steps<100,"Bad-patch phase absent");}
  y.events.clear();bool threw=false;try{bad.step(*b);}catch(const std::logic_error&){threw=true;}check(threw&&bad.active(),"Malformed live patch fell back");for(const auto&e:y.events)check(!std::get<0>(e),"Malformed patch rejecting phase wrote resource");++liveGuards;
 }
 check((components&1)&&!allocations,"Release component/alloc gate");std::cout<<"PASS VoiceRelease calls="<<cases<<" phases="<<phases<<" IRQ="<<irqCases<<" nested="<<nestedCases<<" live="<<liveCases<<" reentry="<<reentries<<" reset="<<resets<<" guards="<<guards<<" live_guards="<<liveGuards<<" faults="<<faults<<" components="<<components<<" allocations="<<allocations<<" interpreter="<<b->interpretedSteps<<'\n';return 0;
}catch(const std::exception&e){tracking=false;std::cerr<<e.what()<<'\n';return 1;}}
