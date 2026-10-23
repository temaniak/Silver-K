#include "ksRuntimeController.h"
#include "ksLiveMidiController.h"
#include "ksRom.h"
#include "h8s/h8s.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <new>
#include <random>
#include <stdexcept>
#include <tuple>
#include <vector>
namespace { bool track=false; uint64_t allocations=0; }
void* operator new(std::size_t n){if(track)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}
void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,std::size_t)noexcept{std::free(p);}
void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
using Access=std::tuple<bool,uint32_t,uint8_t,uint64_t,uint32_t>;
using Transfer=std::tuple<bool,uint32_t,uint8_t,uint64_t>;
struct Original final:H8SEmulator{using H8SEmulator::lastread;using H8SEmulator::lastwrite;using H8SEmulator::indent;};
template<class Base>struct Bus final:Base {
 std::vector<Access> events;
 uint8_t read(uint32_t a)override{auto v=this->state->memory[a];events.emplace_back(false,a,v,this->state->cycles,uint32_t(this->state->getPC()));return v;}
 void write(uint32_t a,uint8_t v)override{this->state->memory[a]=v;events.emplace_back(true,a,v,this->state->cycles,uint32_t(this->state->getPC()));}
};
bool scratch(uint32_t a){return a>=0xfff700&&a<0xfff800;}
std::vector<Transfer> resources(const std::vector<Access>& events,H8SDisassembler* decoder){
 std::vector<Transfer> out;size_t fetchStart=events.size(),fetchBytes=0;
 if(decoder){
  for(size_t i=0;i<events.size();++i){const auto&e=events[i];
   if(!std::get<0>(e)&&!(std::get<4>(e)&1)&&std::get<1>(e)==std::get<4>(e)){
    fetchStart=i;const auto pc=std::get<1>(e);decoder->pc=decoder->makepc(pc);decoder->step();fetchBytes=uint32_t(decoder->getPC())-pc;
    check(fetchBytes>=2&&fetchBytes<=10&&i+fetchBytes<=events.size(),"Original fetch length invalid");
    for(unsigned n=0;n<fetchBytes;++n)check(!std::get<0>(events[i+n])&&std::get<1>(events[i+n])==pc+n,"Original fetch shape invalid");
    break;
   }
  }
  check(fetchBytes,"Original instruction fetch absent");
 }
 for(size_t i=0;i<events.size();++i){const auto&e=events[i];auto a=std::get<1>(e);
  if((i>=fetchStart&&i<fetchStart+fetchBytes)||scratch(a)||(!std::get<0>(e)&&a>=28*4&&a<92*4))continue;
  out.emplace_back(std::get<0>(e),a,std::get<2>(e),std::get<3>(e));
 }
 return out;
}
}
int main(int argc,char**argv){try{
 if(argc!=2){std::cout<<"SKIP: private verified KS OS required\n";return 77;}check(!ks::controllerReferenceAvailable(),"Test linked synchronized H8 adapter");ks::Rom rom(argv[1]);check(rom.isValid(),"Invalid ROM");
 auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();auto decoder=std::make_unique<H8SDisassembler>();for(unsigned base:{0u,0xa00000u}){a->loadmem(rom.data().data(),unsigned(rom.data().size()),base);b->loadmem(rom.data().data(),unsigned(rom.data().size()),base);}decoder->loadmem(rom.data().data(),unsigned(rom.data().size()),0);g_dasm=false;
 Bus<H8SDevice>x;Bus<ks::ControllerBus>y;x.events.reserve(256);y.events.reserve(256);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->nativeLowSize=0x200000;b->interpreterAllowed=false;ks::LiveMidiController state(*b);ks::RuntimeController runtime(*b,state);std::mt19937 rng(0x5354504e);
 struct Config{unsigned consumer=510,queueProducer=254,queueConsumer=128,serialProducer=254,mode=64,bias=12,part=2,channel=7,local=0,flags=0xa5,exr=0,count=2;std::array<uint16_t,8>events{{0x6430,0xc030}};int lastread=-1,lastwrite=-1;bool full=false,live=false,liveCaller=false;}cfg;
 uint64_t cases=0,boundaries=0,nested=0,resets=0,liveCases=0,callerCases=0,guardCases=0,fullCases=0;uint32_t components=0;unsigned trace=0;
 auto run=[&](unsigned injectAt=~0u,unsigned vector=32){
  check(!runtime.statePanelForeground.active()&&!runtime.activeNativeInterrupts(),"Previous Controls graph did not retire");runtime.resetMetrics();std::array<int32_t,8>saved{};for(unsigned n=0;n<8;++n)saved[n]=a->regs[n].er=b->regs[n].er=int32_t(rng());saved[7]=0xfff800;a->setSP(0xfff800);b->setSP(0xfff800);
  a->pc=a->makepc(0x4406);a->ccr=uint8_t(cfg.flags);a->exr=uint8_t(cfg.exr);a->cycles=1000;a->indent=1;a->lastread=cfg.lastread;a->lastwrite=cfg.lastwrite;a->interruptMode2=true;a->pending_irqs=a->pending_irqs_high=0;b->pc=0x4406;b->ccr=uint8_t(cfg.flags);b->exr=uint8_t(cfg.exr);b->cycles=1000;b->indent=1;b->lastread=cfg.lastread;b->lastwrite=cfg.lastwrite;b->interruptMode2=true;b->pending_irqs=b->pending_irqs_high=0;for(auto&p:a->interruptPriority)p=0;for(auto&p:b->interruptPriority)p=0;a->interruptPriority[vector]=b->interruptPriority[vector]=7;
  for(auto*m:{a->memory,b->memory}){const auto byte=[&](unsigned at,unsigned v){m[at]=uint8_t(v);};const auto word=[&](unsigned at,unsigned v){byte(at,v>>8);byte(at+1,v);};const auto wide=[&](unsigned at,uint32_t v){word(at,v>>16);word(at+2,v);};
   std::fill(m+0xfff700,m+0xfff800,uint8_t(0x5a));wide(0xfff800,0x4338);word(0x156ea,cfg.consumer);word(0x156e8,(cfg.consumer+2*cfg.count)&510);for(unsigned i=0;i<cfg.count;++i)word(0x156ec+((cfg.consumer+2*i)&510),cfg.events[i]);word(0x15b04,cfg.queueProducer);word(0x15b06,cfg.queueConsumer);byte(0x15c08,0);word(0xffed6c,cfg.serialProducer);word(0xffed6e,(cfg.serialProducer+(cfg.full?1:128))&255);byte(0x169ed,cfg.mode);byte(0x1678d,cfg.bias);byte(0x16798,cfg.part);byte(0x174aeu+20*cfg.part+11,cfg.channel);byte(0x160d2u+334*cfg.part+327,cfg.local);byte(0x17526,0xaa);for(unsigned i=0;i<256;++i)byte(0xffeeaeu+i,i^0x35);
   byte(0xffec14,1);byte(0xffee70,128);wide(0xffec10,7);byte(0xffec23,1);wide(0xffee7a,0xffffffff);wide(0xffee7e,1);word(0xffee78,0xffff);wide(0xffee82,0xffffffff);wide(0xffee86,0x34);byte(0xffee97,0);byte(0xffee9a,255);byte(0xffff8c,0xf7);byte(0xffff8a,0xf0);word(0xffec64,0);word(0xffec66,0);byte(0xffed68,0);byte(0xffff8d,0x42);
   for(unsigned n=0;n<4;++n){byte(0xffec16+n,1u<<n);byte(0xffec24+n,128u>>n);}
  }
  const auto byte=[&](unsigned at,unsigned v){a->memory[at]=b->memory[at]=uint8_t(v);};const auto word=[&](unsigned at,unsigned v){byte(at,v>>8);byte(at+1,v);};unsigned count=0,live=0,polls=0;uint32_t target=0x4338;bool injected=false;
  do{const auto pc=uint32_t(a->getPC());if(!injected&&count==injectAt){if(vector<64)a->pending_irqs|=1ull<<vector,b->pending_irqs|=1ull<<vector;else a->pending_irqs_high|=1ull<<(vector-64),b->pending_irqs_high|=1ull<<(vector-64);injected=true;}
   if(cfg.full&&pc==0x1124&&++polls==5)word(0xffed6e,(cfg.serialProducer+128)&255);
   if(cfg.liveCaller&&count==5){word(0xfff800,0xa500);word(0xfff802,0x433a);target=0x433a;}
   if(cfg.live){const auto once=[&](unsigned bit,unsigned at,auto&&change){if(!(live&(1u<<bit))&&pc==at){change();live|=1u<<bit;}};once(0,0x441a,[&]{byte(0x1678d,5);});once(1,0x2b42,[&]{byte(0x169ed,255);});once(2,0x43ec,[&]{byte(0x16798,3);byte(0x174aeu+20*3+11,11);byte(0x160d2u+334*3+327,0x24);});once(3,0x2bb6,[&]{byte(0x160d2u+334*3+327,0);});}
   if(count==3){runtime.resetMetrics();++resets;}x.events.clear();y.events.clear();track=true;runtime.step(*b);track=false;a->step();
   if(a->cycles!=b->cycles){std::cerr<<"case="<<cases<<" event="<<std::hex<<cfg.events[0]<<" oldPC="<<pc<<std::dec<<" phase="<<count<<" clocks="<<a->cycles<<'/'<<b->cycles<<'\n';throw std::runtime_error("Controls H8 phase clocks differ");}
   const auto expected=resources(x.events,decoder.get()),actual=resources(y.events,nullptr);if(expected!=actual){std::cerr<<"case="<<cases<<" event="<<std::hex<<cfg.events[0]<<" oldPC="<<pc<<" newPC="<<b->pc<<std::dec<<" phase="<<count<<" old/new transfers="<<expected.size()<<'/'<<actual.size()<<'\n';for(const auto&list:{expected,actual}){for(const auto&e:list)std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<std::dec<<' ';std::cerr<<'\n';}throw std::runtime_error("Controls H8 resource/value/order/timestamp differs");}
   for(const auto&e:actual)if(std::get<0>(e))check(a->memory[std::get<1>(e)]==b->memory[std::get<1>(e)],"Controls evolving resource byte differs");check(a->pending_irqs==b->pending_irqs&&a->pending_irqs_high==b->pending_irqs_high&&a->exr==b->exr,"Controls IRQ priority/admission differs");
   if(runtime.statePanelForeground.active()){check(b->pc==0x4406&&b->getSP()==0xfff800&&b->ccr==uint8_t(cfg.flags),"Controls graph used CPU continuation");for(unsigned n=0;n<8;++n)check(b->regs[n].er==saved[n],"Controls graph used CPU register");for(unsigned at=0xfff700;at<0xfff800;++at)check(b->memory[at]==0x5a,"Controls graph wrote internal CPU stack");}
   check(++count<16384,"Controls graph did not finish");++boundaries;
  }while(uint32_t(a->getPC())!=target||a->indent!=0);
  check(!runtime.statePanelForeground.active()&&!runtime.activeNativeInterrupts(),"Controls graph ownership did not retire");check(runtime.statePanelForeground.initial+runtime.statePanelForeground.started==runtime.statePanelForeground.completed,"Controls metric reset lost graph");for(unsigned n=0;n<8;++n)if(a->regs[n].er!=b->regs[n].er){std::cerr<<"case="<<cases<<" reg="<<n<<" old/new="<<std::hex<<a->regs[n].er<<'/'<<b->regs[n].er<<std::dec<<'\n';throw std::runtime_error("Controls final register ABI differs");}if(a->ccr!=b->ccr){std::cerr<<"case="<<cases<<" CCR="<<std::hex<<unsigned(a->ccr)<<'/'<<unsigned(b->ccr)<<" entry="<<cfg.flags<<std::dec<<'\n';throw std::runtime_error("Controls final CCR differs");}if(a->lastread!=b->lastread||a->lastwrite!=b->lastwrite)std::cerr<<"Controls history "<<std::hex<<a->lastread<<'/'<<b->lastread<<' '<<a->lastwrite<<'/'<<b->lastwrite<<std::dec<<'\n';check(a->exr==b->exr&&a->indent==b->indent&&a->lastread==b->lastread&&a->lastwrite==b->lastwrite,"Controls final priority/history differs");
  check(std::equal(a->memory+0x15000,a->memory+0x2b000,b->memory+0x15000)&&std::equal(a->memory+0xffec00,a->memory+0xfff700,b->memory+0xffec00)&&std::equal(a->memory+0xfff800,a->memory+(1<<24),b->memory+0xfff800),"Controls complete resource RAM differs");if(injected)++nested;if(cfg.live){check(live==15,"Controls live resource fixture missed phase");++liveCases;}if(cfg.liveCaller){check(b->pc==0x433a,"Controls live caller frame not observed");++callerCases;}if(cfg.full){check(polls>=5,"Controls full UART did not wait");++fullCases;}components|=runtime.statePanelForeground.components;++cases;return count;
 };
 for(unsigned flags=0;flags<256;++flags)for(unsigned variant=0;variant<5;++variant){cfg=Config{};cfg.flags=cfg.exr=flags;if(variant==0)cfg.count=0;else if(variant==1)cfg.events={{0x1230,0x9230}};else if(variant==2)cfg.events={{0x0031,0x6430}};else if(variant==3){cfg.bias=255;cfg.events={{0x7f80,0xff80}};}else cfg.local=0x64;run();}
 for(unsigned event=0;event<65536;++event){cfg=Config{};cfg.count=1;cfg.events[0]=uint16_t(event);cfg.bias=event>>8;cfg.local=event&255;cfg.mode=event>>8;cfg.consumer=event&510;cfg.queueProducer=cfg.serialProducer=event&255;run();}
 for(unsigned local=0;local<256;++local)for(unsigned mode:{0u,63u,64u,255u})for(unsigned part:{0u,3u,255u}){cfg=Config{};cfg.local=local;cfg.mode=mode;cfg.part=part;cfg.channel=local;run();}
 for(unsigned producer=0;producer<256;++producer)for(unsigned consumer=0;consumer<256;consumer+=17){cfg=Config{};cfg.queueProducer=cfg.serialProducer=producer;cfg.queueConsumer=consumer;cfg.consumer=consumer*2;run();}
 for(int read:{-1,0x156e9,0x17525,0x15b07,0xffeead,0xffff89})for(int write:{-1,0x156e9,0x17525,0x15b07,0x15b08,0xffed6d,0xffff89}){cfg=Config{};cfg.lastread=read;cfg.lastwrite=write;run();}
 cfg=Config{};trace=run();for(unsigned phase=1;phase<trace;++phase)run(phase);for(unsigned vector:{28u,88u,89u,90u,91u})for(unsigned phase=1;phase<trace;phase+=7)run(phase,vector);cfg=Config{};cfg.full=true;trace=run();for(unsigned phase=1;phase<trace;phase+=3)run(phase,90);cfg=Config{};cfg.live=true;run();cfg=Config{};cfg.liveCaller=true;run();
 for(unsigned variant=0;variant<17;++variant){b->pc=0x4406;b->setSP(0xfff800);b->indent=1;b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->nativeLowSize=0x200000;b->interruptMode2=true;for(unsigned at:{0x156e8u,0x156eau,0x15b04u,0x15b06u,0xffed6cu,0xffed6eu})b->memory[at]=b->memory[at+1]=0;if(variant==0)b->nativeFetchClocksOnly=false;else if(variant==1)b->nativeLowBase=nullptr;else if(variant==2)b->interruptMode2=false;else if(variant==3)b->indent=0;else if(variant==4)b->nativeLowSize=0x2ffff;else if(variant<9)b->setSP(variant==5?0xfff3fe:variant==6?0xfff801:variant==7?0xfffc00:0xffffffff);else {const unsigned at=variant<13?(variant&1?0x156e8:0x156ea):variant<15?(variant&1?0x15b04:0x15b06):variant==15?0xffed6c:0xffed6e;const auto value=variant<11?1u:variant<13?512u:256u;b->memory[at]=uint8_t(value>>8);b->memory[at+1]=uint8_t(value);}const auto clock=b->cycles;track=true;const auto handled=runtime.statePanelForeground.step(*b);track=false;check(!handled&&!runtime.statePanelForeground.active()&&b->cycles==clock,"Controls malformed diagnostic admission guard failed");++guardCases;}
 check(components==0x1ff&&liveCases==1&&callerCases==1&&fullCases>0,"Controls child/live/full coverage incomplete");check(!allocations&&!b->interpretedSteps,"Controls graph allocated or invoked interpreter");std::cout<<"PASS: "<<cases<<" closed Controls graphs; "<<boundaries<<" independent stock H8 phases; "<<nested<<" nested IRQ cases; "<<resets<<" metric resets; "<<guardCases<<" guards; "<<fullCases<<" full UART cases; all 9 components, resource clocks, final ABI/flags/history and live caller exact; no allocations/interpreter/inner CPU frames\n";return 0;
 }catch(const std::exception&e){track=false;std::cerr<<e.what()<<'\n';return 1;}}
