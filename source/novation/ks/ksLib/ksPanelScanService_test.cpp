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
 std::vector<Access> events;std::array<uint8_t,6> samples{};
 uint8_t read(uint32_t a)override{auto v=this->state->memory[a];if(a==0xffff51){const auto mux=this->state->memory[0xffff62];if(mux>=0x20&&mux<0x26)v=samples[mux-0x20];}events.emplace_back(false,a,v,this->state->cycles,uint32_t(this->state->getPC()));return v;}
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
 if(argc!=2){std::cout<<"SKIP: private verified KS OS required\n";return 77;}
 check(!ks::controllerReferenceAvailable(),"Test linked synchronized H8 adapter");ks::Rom rom(argv[1]);check(rom.isValid(),"Invalid ROM");
 auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();auto decoder=std::make_unique<H8SDisassembler>();
 for(unsigned base:{0u,0xa00000u}){a->loadmem(rom.data().data(),unsigned(rom.data().size()),base);b->loadmem(rom.data().data(),unsigned(rom.data().size()),base);}
 decoder->loadmem(rom.data().data(),unsigned(rom.data().size()),0);g_dasm=false;
 Bus<H8SDevice>x;Bus<ks::ControllerBus>y;x.events.reserve(256);y.events.reserve(256);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);
 b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->nativeLowSize=0x200000;b->interpreterAllowed=false;
 ks::LiveMidiController state(*b);ks::RuntimeController runtime(*b,state);std::mt19937 rng(0x504e5343);
 using K=ks::PanelScanService::Kind;constexpr std::array<unsigned,3>roots{{0x13f8,0x1456,0x14e4}};
 struct Config{K kind=K::Scan;unsigned producer=126,consumer=0,event=0x142,flags=0xa5,exr=0,mappingBias=0,overrideKey=256,overrideMapping=0;
  int32_t r0=0x12345678;int lastread=-1,lastwrite=-1;bool live=false,liveCaller=false;
  std::array<uint8_t,6>previous{{0xaa,0xaa,0xaa,0xaa,0xaa,0xaa}},samples{{0x55,0x55,0x55,0x55,0x55,0x55}};}cfg;
 uint64_t cases=0,boundaries=0,nested=0,metricResets=0,liveCases=0,callerCases=0,guardCases=0,rows=0,events=0;uint32_t components=0;
 auto run=[&](unsigned injectAt=~0u,unsigned vector=32){
  check(!runtime.panelScanForeground.active()&&!runtime.activeNativeInterrupts(),"Previous panel graph did not retire");runtime.resetMetrics();
  const auto root=roots[unsigned(cfg.kind)];std::array<int32_t,8>saved{};
  for(unsigned n=0;n<8;++n)saved[n]=a->regs[n].er=b->regs[n].er=int32_t(rng());
  saved[0]=a->regs[0].er=b->regs[0].er=cfg.r0;
  a->regs[6].r=b->regs[6].r=int16_t(cfg.event);saved[6]=b->regs[6].er;
  saved[7]=int32_t(0xfff800);a->setSP(0xfff800);b->setSP(0xfff800);
  a->pc=a->makepc(root);a->ccr=uint8_t(cfg.flags);a->exr=uint8_t(cfg.exr);a->cycles=1000;a->indent=1;a->lastread=cfg.lastread;a->lastwrite=cfg.lastwrite;a->interruptMode2=true;a->pending_irqs=a->pending_irqs_high=0;
  b->pc=root;b->ccr=uint8_t(cfg.flags);b->exr=uint8_t(cfg.exr);b->cycles=1000;b->indent=1;b->lastread=cfg.lastread;b->lastwrite=cfg.lastwrite;b->interruptMode2=true;b->pending_irqs=b->pending_irqs_high=0;
  for(auto&p:a->interruptPriority)p=0;for(auto&p:b->interruptPriority)p=0;a->interruptPriority[vector]=b->interruptPriority[vector]=7;
  x.samples=y.samples=cfg.samples;
  for(auto*m:{a->memory,b->memory}){
   const auto byte=[&](uint32_t at,uint8_t value){m[at]=value;};const auto word=[&](uint32_t at,uint16_t value){byte(at,uint8_t(value>>8));byte(at+1,uint8_t(value));};
   const auto wide=[&](uint32_t at,uint32_t value){word(at,uint16_t(value>>16));word(at+2,uint16_t(value));};
   std::fill(m+0xfff700,m+0xfff800,uint8_t(0x5a));wide(0xfff800,0x4338);
   word(0x1537c,uint16_t(cfg.producer));word(0x1537e,uint16_t(cfg.consumer));
   for(unsigned n=0;n<128;++n)byte(0x15380+n,uint8_t(n*149));
   for(unsigned n=0;n<256;++n)byte(0xb8e8+n,uint8_t(n*37+cfg.mappingBias));
   if(cfg.overrideKey<256)byte(0xb8e8+cfg.overrideKey,uint8_t(cfg.overrideMapping));
   for(unsigned n=0;n<6;++n)byte(0x15400+n,cfg.previous[n]);
   byte(0xffff62,0x30);byte(0xfffeb1,0xff);byte(0xffff51,0);
   byte(0xffec14,1);byte(0xffee70,128);wide(0xffec10,7);byte(0xffec23,1);
   wide(0xffee7a,0xffffffff);wide(0xffee7e,1);word(0xffee78,0xffff);wide(0xffee82,0xffffffff);wide(0xffee86,0x34);byte(0xffee97,0);byte(0xffee9a,0xff);
   word(0xffed6c,45);word(0xffed6e,17);byte(0xffff8c,0xf7);byte(0xffff8a,0xf0);
   for(unsigned n=0;n<4;++n){byte(0xffec16+n,uint8_t(1u<<n));byte(0xffec24+n,uint8_t(128u>>n));}
  }
  const auto setByte=[&](uint32_t at,uint8_t value){a->memory[at]=b->memory[at]=value;};
  const auto setWord=[&](uint32_t at,uint16_t value){setByte(at,uint8_t(value>>8));setByte(at+1,uint8_t(value));};
  std::vector<uint16_t>published;std::vector<uint8_t>mux,gate;published.reserve(64);mux.reserve(64);gate.reserve(64);
  unsigned count=0,liveMask=0;uint32_t target=0x4338;bool injected=false;
  do{
   const auto pc=uint32_t(a->getPC());
   if(!injected&&count==injectAt){if(vector<64)a->pending_irqs|=1ull<<vector,b->pending_irqs|=1ull<<vector;else a->pending_irqs_high|=1ull<<(vector-64),b->pending_irqs_high|=1ull<<(vector-64);injected=true;}
   if(cfg.liveCaller&&count==5){setWord(0xfff800,0xa500);setWord(0xfff802,0x433a);target=0x433a;}
   if(cfg.live){
    const auto once=[&](unsigned bit,uint32_t phase,auto&&change){if(!(liveMask&(1u<<bit))&&pc==phase){change();liveMask|=1u<<bit;}};
    once(0,0x148e,[&]{setByte(0x15400,0x11);});
    once(1,0x1490,[&]{x.samples[0]=y.samples[0]=0xe7;});
    once(2,0x1492,[&]{setByte(0x15400,0x88);x.samples[0]=y.samples[0]=0x3c;});
    once(3,0x14e8,[&]{setByte(0xb8e8+uint8_t(a->regs[5].rl),0xd6);});
    once(4,0x14f0,[&]{setWord(0x1537c,0x7e);setByte(0xb8e8+uint8_t(a->regs[5].rl),0x99);});
   }
   if(count==3){runtime.resetMetrics();++metricResets;}
   x.events.clear();y.events.clear();track=true;runtime.step(*b);track=false;a->step();
   if(a->cycles!=b->cycles){std::cerr<<"root="<<std::hex<<root<<" oldPC="<<pc<<std::dec<<" phase="<<count<<" clocks="<<a->cycles<<'/'<<b->cycles<<'\n';throw std::runtime_error("Panel scan phase clocks differ");}
   auto expected=resources(x.events,decoder.get()),actual=resources(y.events,nullptr);
   if(expected!=actual){std::cerr<<"root="<<std::hex<<root<<" oldPC="<<pc<<" newPC="<<b->pc<<std::dec<<" phase="<<count<<" old/new transfers="<<expected.size()<<'/'<<actual.size()<<'\n';for(const auto&list:{expected,actual}){for(const auto&e:list)std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<std::dec<<' ';std::cerr<<'\n';}throw std::runtime_error("Panel scan transfer/value/order/time differs");}
   for(size_t i=0;i<actual.size();++i){const auto&e=actual[i];if(!std::get<0>(e))continue;const auto at=std::get<1>(e);if(at==0xffff62)mux.push_back(std::get<2>(e));if(at==0xfffeb1)gate.push_back(std::get<2>(e));
    if(at>=0x15380&&at<0x15400&&!(at&1)){check(i+1<actual.size()&&std::get<1>(actual[i+1])==at+1,"Panel event word shape differs");published.push_back(uint16_t((unsigned(std::get<2>(e))<<8)|std::get<2>(actual[i+1])));}
   }
   check(std::equal(a->memory+0xffdc00,a->memory+0xfff700,b->memory+0xffdc00)&&std::equal(a->memory+0x1537c,a->memory+0x15406,b->memory+0x1537c),"Panel evolving resource RAM differs");
   check(a->pending_irqs==b->pending_irqs&&a->pending_irqs_high==b->pending_irqs_high&&a->exr==b->exr,"Panel IRQ priority/admission differs");
   if(runtime.panelScanForeground.active()){
    check(b->pc==root&&b->getSP()==0xfff800&&b->ccr==uint8_t(cfg.flags),"Panel graph used CPU continuation");for(unsigned n=0;n<8;++n)check(b->regs[n].er==saved[n],"Panel graph used CPU register");
    for(unsigned at=0xfff700;at<0xfff800;++at)check(b->memory[at]==0x5a,"Panel graph wrote inner CPU stack");
   }
   check(++count<8192,"Panel scan graph did not finish");++boundaries;
  }while(uint32_t(a->getPC())!=target||a->indent!=0);
  check(!runtime.panelScanForeground.active()&&!runtime.activeNativeInterrupts(),"Panel native graph did not retire");
  check(runtime.panelScanForeground.started+runtime.panelScanForeground.initial==runtime.panelScanForeground.completed,"Panel metric reset lost graph");
  for(unsigned n=0;n<8;++n)if(a->regs[n].er!=b->regs[n].er){std::cerr<<"kind="<<unsigned(cfg.kind)<<" reg="<<n<<" old/new="<<std::hex<<a->regs[n].er<<'/'<<b->regs[n].er<<std::dec<<'\n';throw std::runtime_error("Panel returned register differs");}
  if(a->ccr!=b->ccr){std::cerr<<"kind="<<unsigned(cfg.kind)<<" ccr="<<std::hex<<unsigned(a->ccr)<<'/'<<unsigned(b->ccr)<<" entry="<<cfg.flags<<std::dec<<'\n';throw std::runtime_error("Panel returned CCR differs");}
  if(a->lastread!=b->lastread||a->lastwrite!=b->lastwrite)std::cerr<<"Panel bus history old/new read="<<std::hex<<a->lastread<<'/'<<b->lastread<<" write="<<a->lastwrite<<'/'<<b->lastwrite<<std::dec<<'\n';
  check(a->exr==b->exr&&a->indent==b->indent&&a->lastread==b->lastread&&a->lastwrite==b->lastwrite,"Panel returned priority/history differs");
  if(injected){check(std::equal(a->memory,a->memory+0xfff700,b->memory)&&std::equal(a->memory+0xfff800,a->memory+(1<<24),b->memory+0xfff800),"Nested panel full resource memory differs");++nested;}
  if(!injected&&!cfg.live){
   std::vector<uint16_t>wanted;wanted.reserve(48);
   const auto mapping=[&](unsigned key){return uint8_t(key==cfg.overrideKey?cfg.overrideMapping:key*37+cfg.mappingBias);};
   if(cfg.kind==K::ButtonPush)wanted.push_back(uint16_t((cfg.event&0xff00)|mapping(cfg.event&255)));
   else for(unsigned row=0;row<6;++row){for(unsigned bit=0;bit<8;++bit)if((cfg.previous[row]^cfg.samples[row])&(1u<<bit))wanted.push_back(uint16_t((cfg.samples[row]&(1u<<bit)?0:0x100)|mapping(row*8+bit)));}
   check(published==wanted,"Panel event physical order/active-low state/mapping differs");
   if(cfg.kind==K::ButtonPush)check(mux.empty()&&gate.empty(),"Standalone push touched matrix ports");
   else {check(mux==std::vector<uint8_t>({0x20,0x21,0x22,0x23,0x24,0x25,0x30})&&gate==std::vector<uint8_t>({0,255}),"Six-row mux/gate order differs");rows+=6;}
   events+=published.size();
  }
  if(cfg.live){check(liveMask==31,"Live panel resource coverage incomplete");++liveCases;}
  if(cfg.liveCaller){check(b->pc==0x433a,"Panel outer caller edge was not live");++callerCases;}
  components|=runtime.panelScanForeground.components;++cases;return count;
 };
 for(unsigned kind=0;kind<3;++kind)for(unsigned flags=0;flags<256;++flags)for(int32_t value:{0,1,-1,int32_t(0x80000000u),int32_t(0x7fffffffu)}){
  cfg=Config{};cfg.kind=K(kind);cfg.flags=cfg.exr=flags;cfg.r0=value;cfg.event=uint16_t(value);cfg.previous=cfg.samples;cfg.samples[5]^=uint8_t(flags&1);run();
 }
 for(auto kind:{K::Initialize,K::Scan})for(unsigned row=0;row<6;++row)for(unsigned pattern=0;pattern<256;++pattern)for(bool reverse:{false,true}){
  cfg=Config{};cfg.kind=kind;cfg.previous.fill(0x55);cfg.samples=cfg.previous;cfg.previous[row]=reverse?uint8_t(pattern):0;cfg.samples[row]=reverse?0:uint8_t(pattern);run();
 }
 for(unsigned producer=0;producer<128;producer+=2)for(unsigned consumer=0;consumer<128;consumer+=2)for(auto kind:{K::Initialize,K::Scan,K::ButtonPush}){
  cfg=Config{};cfg.kind=kind;cfg.producer=producer;cfg.consumer=consumer;cfg.previous=cfg.samples;run();
 }
 for(unsigned event=0;event<65536;++event){cfg=Config{};cfg.kind=K::ButtonPush;cfg.event=event;cfg.producer=event&126;cfg.mappingBias=event>>8;run();}
 for(unsigned key=0;key<256;++key)for(unsigned mapping=0;mapping<256;++mapping){cfg=Config{};cfg.kind=K::ButtonPush;cfg.event=0xa500|key;cfg.overrideKey=key;cfg.overrideMapping=mapping;run();}
 for(auto kind:{K::Initialize,K::Scan,K::ButtonPush})for(int read:{-1,0x15400,0x15401,0xffff50,0xffff61,0xfffeb0,0x1537b})for(int write:{-1,0x15400,0x15401,0xffff50,0xffff61,0xfffeb0,0x1537b}){cfg=Config{};cfg.kind=kind;cfg.lastread=read;cfg.lastwrite=write;run();}
 for(unsigned kind=0;kind<3;++kind){cfg=Config{};cfg.kind=K(kind);const auto full=run();for(unsigned phase=1;phase<full;++phase)run(phase);}
 cfg=Config{};cfg.live=true;run();cfg=Config{};cfg.liveCaller=true;run();
 for(unsigned kind=0;kind<3;++kind)for(unsigned variant=0;variant<9;++variant){
  b->pc=roots[kind];b->setSP(0xfff800);b->indent=1;b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->nativeLowSize=0x200000;b->interruptMode2=true;
  if(variant==0)b->nativeFetchClocksOnly=false;else if(variant==1)b->nativeLowBase=nullptr;else if(variant==2)b->interruptMode2=false;else if(variant==3)b->indent=0;else if(variant==4)b->nativeLowSize=0;else b->setSP(variant==5?0xfff3fe:variant==6?0xfff801:variant==7?0xfffc00:0xffffffff);
  const auto cycles=b->cycles;track=true;const auto handled=runtime.panelScanForeground.step(*b);track=false;check(!handled&&!runtime.panelScanForeground.active()&&b->cycles==cycles,"Panel diagnostic frame guard failed");++guardCases;
 }
 for(unsigned kind=0;kind<3;++kind)for(unsigned address:{0x1537cu,0x1537eu})for(unsigned value:{1u,0x80u,0x100u}){
  b->pc=roots[kind];b->setSP(0xfff800);b->indent=1;b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->nativeLowSize=0x200000;b->interruptMode2=true;
  b->memory[0x1537c]=b->memory[0x1537d]=b->memory[0x1537e]=b->memory[0x1537f]=0;b->memory[address]=uint8_t(value>>8);b->memory[address+1]=uint8_t(value);
  const auto cycles=b->cycles;track=true;const auto handled=runtime.panelScanForeground.step(*b);track=false;check(!handled&&!runtime.panelScanForeground.active()&&b->cycles==cycles,"Panel malformed ring guard failed");++guardCases;
 }
 check(liveCases==1&&callerCases==1&&components==7,"Panel child/live coverage incomplete");check(!allocations&&!b->interpretedSteps,"Panel graph allocated or used interpreter");
 std::cout<<"PASS: "<<cases<<" complete CPU-frame-free panel graphs; "<<boundaries<<" independent H8 phases; "<<nested<<" nested IRQ cases; "<<rows<<" ordered rows / "<<events<<" mapped events; "<<metricResets<<" metric resets; "<<guardCases<<" guards; all 3 roots, live resources/caller/history/final ABI and byte timestamps exact; no allocations/interpreter/inner CPU stack writes\n";return 0;
 }catch(const std::exception&e){track=false;std::cerr<<e.what()<<'\n';return 1;}}
