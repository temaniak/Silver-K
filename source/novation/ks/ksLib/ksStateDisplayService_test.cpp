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
 std::vector<Access> events;unsigned busy=0,reply=0,statusReads=0;
 uint8_t read(uint32_t a)override{auto v=this->state->memory[a];if(a==0xffff51){++statusReads;v=uint8_t(busy?(--busy,128):reply);}events.emplace_back(false,a,v,this->state->cycles,uint32_t(this->state->getPC()));return v;}
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
 if(argc!=2){std::cout<<"SKIP: private verified KS OS required\n";return 77;}check(!ks::controllerReferenceAvailable(),"Synchronized H8 factory linked");ks::Rom rom(argv[1]);check(rom.isValid(),"Invalid OS");auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();auto decoder=std::make_unique<H8SDisassembler>();for(unsigned base:{0u,0xa00000u}){a->loadmem(rom.data().data(),unsigned(rom.data().size()),base);b->loadmem(rom.data().data(),unsigned(rom.data().size()),base);}decoder->loadmem(rom.data().data(),unsigned(rom.data().size()),0);g_dasm=false;
 Bus<H8SDevice>x;Bus<ks::ControllerBus>y;x.events.reserve(256);y.events.reserve(256);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->nativeLowSize=0x200000;b->interpreterAllowed=false;ks::LiveMidiController shared(*b);ks::RuntimeController runtime(*b,shared);std::mt19937 rng(0x4453504c);
 using K=ks::StateDisplayService::Kind;constexpr std::array<unsigned,5>roots{{0x5ae2,0x5da8,0x9b8,0x976,0x8fc}};
 struct Config{K kind=K::Format;unsigned type=2,value=123,bit=0,character=0x90,width=3,mode=7,reply=0,busy=0,flags=0xa5,exr=0;uint32_t table=0x18000,source=0x19000;bool live=false,liveCaller=false,empty=false;int read=-1,write=-1;}cfg;
 uint64_t cases=0,phases=0,nested=0,resets=0,guards=0,liveCases=0,callerCases=0;uint32_t components=0,kindMask=0;
 const auto run=[&](unsigned inject=~0u,unsigned vector=32){check(!runtime.stateDisplayForeground.active()&&!runtime.activeNativeInterrupts(),"Previous display graph active");runtime.resetMetrics();const auto root=roots[unsigned(cfg.kind)];std::array<int32_t,8>saved{};for(unsigned n=0;n<8;++n)a->regs[n].er=b->regs[n].er=int32_t(rng());a->regs[0].rl=b->regs[0].rl=int8_t(cfg.value);a->regs[1].rl=b->regs[1].rl=int8_t(cfg.type);a->regs[2].rh=b->regs[2].rh=int8_t(cfg.bit);a->regs[6].r=b->regs[6].r=int16_t(cfg.character);if(cfg.kind==K::Text)a->regs[6].er=b->regs[6].er=int32_t(cfg.source);a->setSP(0xfff800);b->setSP(0xfff800);for(unsigned n=0;n<8;++n)saved[n]=b->regs[n].er;
  a->pc=a->makepc(root);b->pc=root;a->ccr=b->ccr=uint8_t(cfg.flags);a->exr=b->exr=uint8_t(cfg.exr);a->cycles=b->cycles=1000;a->indent=b->indent=1;a->lastread=b->lastread=cfg.read;a->lastwrite=b->lastwrite=cfg.write;a->interruptMode2=b->interruptMode2=true;a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;for(auto&p:a->interruptPriority)p=0;for(auto&p:b->interruptPriority)p=0;a->interruptPriority[vector]=b->interruptPriority[vector]=7;
  const auto byte=[&](uint32_t at,uint8_t v){a->memory[at&0xffffff]=b->memory[at&0xffffff]=v;};const auto word=[&](uint32_t at,uint16_t v){byte(at,uint8_t(v>>8));byte(at+1,uint8_t(v));};const auto wide=[&](uint32_t at,uint32_t v){word(at,uint16_t(v>>16));word(at+2,uint16_t(v));};
  for(unsigned n=0;n<2048;++n)byte(0x18000+n,uint8_t('A'+n%26));byte(cfg.table,uint8_t(cfg.width));for(unsigned n=1;n<=cfg.width*256+256;++n)byte(cfg.table+n,uint8_t('A'+n%26));wide(0x167c4,cfg.table);wide(0x167c8,cfg.table);wide(0x167cc,cfg.table);wide(0x167bc,0x1a000);byte(0x1a057,uint8_t(cfg.mode));byte(cfg.source,'O');byte(cfg.source+1,'f');byte(cfg.source+2,'f');byte(cfg.source+3,0);for(unsigned n=0;n<8;++n)byte(0x11685+n,rom.data()[0x11685+n]);if(cfg.empty){byte(0x11685,0);byte(cfg.source,0);}
  for(auto*m:{a->memory,b->memory}){std::fill(m+0xfff700,m+0xfff800,uint8_t(0x5a));m[0xffec14]=1;m[0xffee70]=128;m[0xffec23]=1;m[0xffec10]=m[0xffec11]=m[0xffec12]=0;m[0xffec13]=7;for(unsigned n=0;n<4;++n){m[0xffee7a+n]=255;m[0xffee7e + n]=uint8_t(n==3);m[0xffee82+n]=255;m[0xffee86+n]=uint8_t(n==3);m[0xffec16+n]=uint8_t(1u<<n);m[0xffec24+n]=uint8_t(128u>>n);}m[0xffee78]=m[0xffee79]=255;m[0xffee97]=0;m[0xffee9a]=255;m[0xffed6c]=m[0xffed6e]=0;m[0xffed6d]=45;m[0xffed6f]=17;m[0xffec64]=m[0xffec65]=m[0xffec66]=m[0xffec67]=0;m[0xffff8c]=0xb7;m[0xffff8a]=0xf0;m[0xffff8d]=0x42;m[0xffed68]=0;m[0xfffeb1]=255;}
  wide(0xfff800,0x4338);x.busy=y.busy=cfg.busy;x.reply=y.reply=cfg.reply;x.statusReads=y.statusReads=0;unsigned count=0,live=0;bool injected=false;uint32_t target=0x4338;
  do{const auto pc=uint32_t(a->getPC());if(count==inject){if(vector<64)a->pending_irqs|=1ull<<vector,b->pending_irqs|=1ull<<vector;else a->pending_irqs_high|=1ull<<(vector-64),b->pending_irqs_high|=1ull<<(vector-64);injected=true;}if(cfg.liveCaller&&count==2){wide(0xfff800,0xa500433a);target=0x433a;}
   if(cfg.live){if(!(live&1)&&pc==0x5b2c){wide(0x167c4,cfg.table+1);byte(cfg.table+1,3);live|=1;}if(!(live&2)&&pc==0x5b34){byte(cfg.table+1,2);live|=2;}if(!(live&4)&&pc==0x5b46){byte(uint32_t(a->regs[0].er),'!');live|=4;}}
   if(count==1){runtime.resetMetrics();++resets;}x.events.clear();y.events.clear();track=true;runtime.step(*b);track=false;a->step();if(a->cycles!=b->cycles){std::cerr<<"kind="<<unsigned(cfg.kind)<<" type="<<cfg.type<<" value="<<cfg.value<<" PC="<<std::hex<<pc<<std::dec<<" phase="<<count<<" clocks="<<a->cycles<<'/'<<b->cycles<<'\n';throw std::runtime_error("Display phase clocks differ");}const auto expected=resources(x.events,decoder.get()),actual=resources(y.events,nullptr);if(expected!=actual){std::cerr<<"kind="<<unsigned(cfg.kind)<<" type="<<cfg.type<<" value="<<cfg.value<<" PC="<<std::hex<<pc<<std::dec<<" phase="<<count<<" transfers="<<expected.size()<<'/'<<actual.size()<<'\n';for(const auto&rows:{expected,actual}){for(const auto&e:rows)std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<std::dec<<' ';std::cerr<<'\n';}throw std::runtime_error("Display byte/value/order/timestamp differs");}
   check(a->pending_irqs==b->pending_irqs&&a->pending_irqs_high==b->pending_irqs_high&&a->exr==b->exr,"Display IRQ admission differs");if(runtime.stateDisplayForeground.active()){check(b->pc==root&&b->getSP()==0xfff800&&b->ccr==uint8_t(cfg.flags),"Display graph used CPU continuation");for(unsigned n=0;n<8;++n)check(b->regs[n].er==saved[n],"Display graph used CPU register");}for(unsigned at=0xfff700;at<0xfff800;++at)check(b->memory[at]==0x5a,"Display graph wrote internal CPU frame");check(++count<300000,"Display graph stalled");++phases;
  }while(uint32_t(a->getPC())!=target||a->indent);
  check(!runtime.stateDisplayForeground.active()&&!runtime.activeNativeInterrupts()&&runtime.stateDisplayForeground.initial+runtime.stateDisplayForeground.started==runtime.stateDisplayForeground.completed,"Display ownership/metrics imbalance");for(unsigned n=0;n<8;++n)if(a->regs[n].er!=b->regs[n].er){std::cerr<<"kind="<<unsigned(cfg.kind)<<" type="<<cfg.type<<" value="<<cfg.value<<" register="<<n<<" old/new="<<std::hex<<a->regs[n].er<<'/'<<b->regs[n].er<<std::dec<<'\n';throw std::runtime_error("Display final caller register differs");}if(a->ccr!=b->ccr||a->lastread!=b->lastread||a->lastwrite!=b->lastwrite){std::cerr<<"kind="<<unsigned(cfg.kind)<<" type="<<cfg.type<<" value="<<cfg.value<<" flags="<<std::hex<<unsigned(a->ccr)<<'/'<<unsigned(b->ccr)<<" history="<<a->lastread<<'/'<<b->lastread<<':'<<a->lastwrite<<'/'<<b->lastwrite<<std::dec<<'\n';throw std::runtime_error("Display final flags/history differs");}check(a->exr==b->exr&&a->getSP()==b->getSP()&&x.statusReads==y.statusReads&&!x.busy&&!y.busy,"Display final priority/poll count differs");if(injected||cfg.live||cfg.liveCaller)check(std::equal(a->memory,a->memory+0xfff700,b->memory)&&std::equal(a->memory+0xfff800,a->memory+(1<<24),b->memory+0xfff800),"Display full evolving resources differ");if(injected)++nested;if(cfg.live){check(live==7,"Display live fixture missed sample");++liveCases;}if(cfg.liveCaller)++callerCases;components|=runtime.stateDisplayForeground.components;kindMask|=1u<<unsigned(cfg.kind);++cases;return count;
 };
 // First isolate final caller projections across each distinct presentation.
 for(unsigned type:{2u,3u,4u,5u,6u,7u,10u,35u,36u,38u,39u,46u,47u,255u})for(unsigned value:{0u,1u,63u,64u,127u,128u,255u}){cfg=Config{};cfg.type=type;cfg.value=value;run();}
 for(unsigned type:{5u,6u,37u,39u,47u}){cfg=Config{};cfg.type=type;cfg.value=0;cfg.mode=0;cfg.empty=true;run();}for(auto kind:{K::Off,K::Text}){cfg=Config{};cfg.kind=kind;cfg.empty=true;run();}
 for(unsigned type=0;type<256;++type)for(unsigned value=0;value<256;++value){cfg=Config{};cfg.type=type;cfg.value=value;cfg.bit=value&7;run();}
 for(unsigned kind=0;kind<5;++kind)for(unsigned flags=0;flags<256;++flags)for(unsigned value:{0u,1u,127u,128u,255u}){cfg=Config{};cfg.kind=K(kind);cfg.flags=cfg.exr=flags;cfg.character=kind<3?0x90:value;cfg.value=value;cfg.reply=flags&1?20:0;cfg.busy=flags%4;run();}
 for(unsigned type:{4u,8u,25u})for(unsigned width:{0u,1u,2u,3u,20u,127u,128u,255u})for(unsigned value:{0u,1u,255u}){cfg=Config{};cfg.type=type;cfg.width=width;cfg.value=value;run();}
 for(uint32_t base:{0x18001u,0xa18000u,0xa18001u,0xffdc00u,0xffdc01u})for(unsigned type:{4u,8u,25u}){cfg=Config{};cfg.table=base;cfg.type=type;cfg.value=1;cfg.width=3;run();}
 for(unsigned reply=0;reply<128;++reply)for(unsigned kind=0;kind<5;++kind){cfg=Config{};cfg.kind=K(kind);cfg.reply=reply;cfg.character=65;cfg.busy=3;run();}
 for(uint32_t source:{0x19001u,0xa19000u,0xffeffcu}){cfg=Config{};cfg.kind=K::Text;cfg.source=source;run();}
 for(unsigned mode=0;mode<256;++mode){cfg=Config{};cfg.type=47;cfg.mode=mode;cfg.value=mode;run();}for(int read:{-1,0xffec33,0xffff50,0x11521})for(int write:{-1,0xffff60,0xfffeb0,0x11524,0xffec33}){cfg=Config{};cfg.read=read;cfg.write=write;run();}
 for(unsigned kind=0;kind<5;++kind){cfg=Config{};cfg.kind=K(kind);cfg.type=4;const auto count=run();for(unsigned vector:{28u,32u,88u,89u,90u,91u})for(unsigned at=0;at<count;at+=3)run(at,vector);cfg.liveCaller=true;run();}cfg=Config{};cfg.type=4;cfg.live=true;run();
 for(unsigned kind=0;kind<5;++kind)for(unsigned variant=0;variant<10;++variant){b->pc=roots[kind];b->setSP(0xfff800);b->indent=1;b->cycles=1000;b->ccr=0xf5;b->exr=0xa0;b->lastread=0x12345;b->lastwrite=0x23456;b->nativeLowSize=0x200000;b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->interruptMode2=true;b->regs[6].er=0x19000;b->memory[0x19000]=0;switch(variant){case 0:b->nativeFetchClocksOnly=false;break;case 1:b->nativeLowBase=nullptr;break;case 2:b->nativeLowSize=0x10000;break;case 3:b->interruptMode2=false;break;case 4:b->indent=0;break;case 5:b->setSP(0xfff801);break;case 6:b->setSP(0xfff3fe);break;case 7:b->setSP(0xfffbfe);break;case 8:b->setSP(0xa18000);break;case 9:b->pc=0x5ae4;break;}const auto pc=b->pc,sp=b->getSP();std::array<int32_t,8>saved{};for(unsigned n=0;n<8;++n)saved[n]=b->regs[n].er;y.events.clear();track=true;const bool handled=runtime.stateDisplayForeground.step(*b);track=false;check(!handled&&!runtime.stateDisplayForeground.active()&&b->pc==pc&&b->getSP()==sp&&b->cycles==1000&&b->ccr==0xf5&&b->exr==0xa0&&b->lastread==0x12345&&b->lastwrite==0x23456&&y.events.empty(),"Display diagnostic admission guard changed caller/resources");for(unsigned n=0;n<8;++n)check(b->regs[n].er==saved[n],"Display rejected admission changed register");++guards;}
 for(uint32_t bad:{0u,0x5ae2u,0xb800u,0x1fffffu,0xbfffffu,0x40000au,0xffff51u,0xffefffu,0xfff000u,0xfff3fcu,0xfffbffu,0xfffff0u})for(bool text:{false,true}){b->pc=text?0x9b8:0x5ae2;b->setSP(0xfff400);b->indent=1;b->cycles=1000;b->nativeLowSize=0x200000;b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->interruptMode2=true;b->regs[1].rl=4;b->regs[0].rl=1;b->regs[6].er=int32_t(bad);for(unsigned n=0;n<4;++n)b->memory[0x167c4+n]=uint8_t(bad>>(24-8*n));b->memory[bad&0xffffff]=3;y.events.clear();const auto before=b->cycles;track=true;const bool handled=runtime.stateDisplayForeground.step(*b);track=false;check(!handled&&!runtime.stateDisplayForeground.active()&&b->cycles==before&&y.events.empty(),"Display reserved/crossing data guard accepted");++guards;}
 check(components==255&&kindMask==31&&liveCases==1&&callerCases==5&&!allocations&&!b->interpretedSteps,"Display coverage/allocation/interpreter contract differs");std::cout<<"PASS: "<<cases<<" CPU-free display calls; "<<phases<<" independent H8 phases; "<<nested<<" IRQ preemptions; "<<resets<<" metric resets; "<<guards<<" guards; "<<callerCases<<" live callers; all five roots/eight components, all type/value bytes, numeric/text/note/bit/tri/disabled/LCD busy paths, exact bytes/time/final ABI; zero allocations/interpreter/internal CPU frames\n";return 0;
 }catch(const std::exception&e){track=false;std::cerr<<e.what()<<'\n';return 1;}}
