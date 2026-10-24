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
 std::vector<Access> events;bool overlay=false;unsigned dspBusy=0,dspReads=0,txWait=0,txReads=0;
 uint8_t read(uint32_t a)override{auto v=this->state->memory[overlay&&a>=0xb8e8&&a<0x200000?0xa00000+a:a];if(a==0x400004&&dspReads++<dspBusy)v=0;if(a==0xffed6f&&txReads++<txWait)v=uint8_t(this->state->memory[0xffed6d]+1);events.emplace_back(false,a,v,this->state->cycles,uint32_t(this->state->getPC()));return v;}
 void write(uint32_t a,uint8_t v)override{this->state->memory[overlay&&a>=0xb8e8&&a<0x200000?0xa00000+a:a]=v;events.emplace_back(true,a,v,this->state->cycles,uint32_t(this->state->getPC()));}
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
 auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();auto decoder=std::make_unique<H8SDisassembler>();
 for(unsigned base:{0u,0xa00000u}){a->loadmem(rom.data().data(),unsigned(rom.data().size()),base);b->loadmem(rom.data().data(),unsigned(rom.data().size()),base);}decoder->loadmem(rom.data().data(),unsigned(rom.data().size()),0);g_dasm=false;
 Bus<H8SDevice>x;Bus<ks::ControllerBus>y;x.events.reserve(256);y.events.reserve(256);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->nativeLowSize=0x200000;b->interpreterAllowed=false;
 ks::LiveMidiController state(*b);ks::RuntimeController runtime(*b,state);auto& foreground=runtime.stateApplyForeground;std::mt19937 rng(0x4150504c);
 const std::array<unsigned,7>roots{{0x6ae6,0x6bf0,0x57e2,0x5816,0x63a4,0x63bc,0x6198}};
 const std::array<unsigned,7>returns{{0x6c3a,0x6c3a,0x5814,0x5828,0x63ba,0x63fc,0x61c0}};
 struct Config{unsigned field=10,type=127,value=64,current=17,cached=31,pickup=0,policy=63,control=74,part=2,selected=2,mode=0,enabled=1,bank=2,program=17,busy=0,txWait=0,recipeUpper=0;uint32_t base=0x24000,live=0x25000,over=0,source=0x167ac,evolvingBase=0;bool overlay=false;}cfg;
 uint64_t cases=0,boundaries=0,nested=0,resets=0,mutations=0,guardCases=0;uint32_t components=0;
 auto verify=[&](unsigned kind,uint32_t savedValue,unsigned flags,unsigned injectAt=~0u,unsigned vector=32,bool live=false){
  check(!foreground.active()&&!runtime.activeNativeInterrupts(),"Previous Apply graph did not retire");runtime.resetMetrics();foreground.resetMetrics();
  std::array<int32_t,8>saved{};for(unsigned n=0;n<8;++n)saved[n]=a->regs[n].er=b->regs[n].er=int32_t(rng());a->regs[0].er=b->regs[0].er=int32_t(savedValue);
  a->regs[1].er=b->regs[1].er=int32_t(kind==2||kind==3?cfg.field:(cfg.over?cfg.over:cfg.base)+cfg.field);a->regs[6].r=b->regs[6].r=int16_t((cfg.value<<8)|cfg.control);if(kind==6)a->regs[0].er=b->regs[0].er=int32_t(cfg.source);
  for(unsigned n=0;n<8;++n)saved[n]=b->regs[n].er;a->setSP(0xfff800);b->setSP(0xfff800);saved[7]=int32_t(0xfff800);
  a->pc=a->makepc(roots[kind]);a->ccr=uint8_t(flags);a->exr=0xa0;a->cycles=1000;a->indent=1;a->lastread=0x12345;a->lastwrite=0x23456;a->interruptMode2=true;a->pending_irqs=a->pending_irqs_high=0;for(auto&p:a->interruptPriority)p=0;a->interruptPriority[vector]=6;
  b->pc=roots[kind];b->ccr=uint8_t(flags);b->exr=0xa0;b->cycles=1000;b->indent=1;b->lastread=0x12345;b->lastwrite=0x23456;b->interruptMode2=true;b->pending_irqs=b->pending_irqs_high=0;for(auto&p:b->interruptPriority)p=0;b->interruptPriority[vector]=6;
  x.overlay=y.overlay=cfg.overlay;x.dspBusy=y.dspBusy=cfg.busy;x.dspReads=y.dspReads=0;x.txWait=y.txWait=cfg.txWait;x.txReads=y.txReads=0;b->nativeLowBase=cfg.overlay?b->memory+0xa00000:b->memory;
  const auto byte=[&](uint32_t address,uint8_t value){address&=0xffffff;a->memory[address]=b->memory[address]=value;if(cfg.overlay&&address>=0xb8e8&&address<0x200000)a->memory[0xa00000 + address]=b->memory[0xa00000 + address]=value;};
  const auto word=[&](uint32_t address,uint16_t value){byte(address,uint8_t(value>>8));byte(address+1,uint8_t(value));};const auto longValue=[&](uint32_t address,uint32_t value){for(unsigned n=0;n<4;++n)byte(address+n,uint8_t(value>>(24-8*n)));};
  for(auto*m:{a->memory,b->memory}){
   std::fill(m+0xfff700,m+0xfff800,uint8_t(0x5a));
   std::fill(m+0x1537c,m+0x15800,uint8_t(0x7b));
   for(unsigned n=0;n<36;++n){m[0xb918 + n]=uint8_t(n);m[0xb93c + n]=uint8_t(n&1);}
   m[0xffec14]=0;m[0xffee70]=128;m[0xffec10]=m[0xffec11]=m[0xffec12]=0;m[0xffec13]=7;
   for(unsigned n=0;n<4;++n){m[0xffee7a + n]=0xff;m[0xffee7e + n]=uint8_t(n==3);m[0xffee82 + n]=0;m[0xffee86 + n]=uint8_t(n==3);}
   m[0xffec23]=1;m[0xffee97]=0x1c;m[0xffee96]=1;m[0xffee9a]=0;m[0xffff51]=0;m[0xffff55]=0xff;m[0xffff54]=12;m[0xffff59]=0xe0;m[0xffff50]=31;
   m[0xffee76]=m[0xffee77]=m[0xffee78]=m[0xffee79]=255;m[0xffed6c]=m[0xffed6e]=0;m[0xffed6d]=45;m[0xffed6f]=17;
   m[0xffec64]=m[0xffec65]=m[0xffec66]=m[0xffec67]=0;m[0xffff8c]=0xb7;m[0xffff8a]=0xf0;m[0xffff8d]=0xf8;
   m[0xffffe6]=0x12;m[0xffffe7]=0x34;
   for(unsigned n=0;n<8;++n){m[0xb960 + n*2]=0xaa;m[0xb961 + n*2]=0xaa;}
   for(unsigned n=0;n<64;++n){auto at=0xb970 + 4*n;m[at]=3;m[at+1]=uint8_t(n+36);m[at+2]=0xff;m[at+3]=0xe2;}
   for(unsigned n=0;n<4;++n){m[0xffec16 + n]=uint8_t(1u<<n);m[0xffec24 + n]=uint8_t(128u>>n);}
  }


  for(unsigned n=0;n<4;++n){word(0xffff90 + 2*n,0x8000);word(0x15652 + 2*n,0x8000);word(0x1560a + 2*n,0);byte(0x1569a + 2*n,0);}byte(0x156e2,36);byte(0x156e3,0);byte(0x156e4,255);byte(0xffff98,0x73);for(unsigned at:{0x1537cu,0x1537eu,0x15406u,0x15408u,0x156e8u,0x156eau})word(at,0);
  for(unsigned n=0;n<256;++n){byte(cfg.base+n,uint8_t(cfg.current));byte(cfg.live+n,uint8_t(cfg.cached));if(cfg.over)byte(cfg.over+n,uint8_t(cfg.current));}
  longValue(0x167a6,cfg.over);longValue(0x167bc,cfg.base);longValue(0x167c0,cfg.live);byte(71586+cfg.field,uint8_t(cfg.type));byte(71330+cfg.field,uint8_t(cfg.control));
  byte(0x169f9,uint8_t(cfg.pickup));byte(0x169ed,uint8_t(cfg.policy));byte(0x16798,uint8_t(cfg.selected));byte(0x17526,uint8_t(cfg.part));byte(0x167ba,uint8_t(cfg.mode));byte(0x1679a,0x24);byte(0x1679c,0);byte(0x169b1,3);byte(0x174ae + 20*cfg.selected+11,3);byte((cfg.over?cfg.over:cfg.base)+103,uint8_t(cfg.enabled));
  byte(0x167ac,uint8_t(cfg.bank));byte(0x167ad,uint8_t(cfg.program));if(kind==6){byte(cfg.source,uint8_t(cfg.bank));byte(cfg.source+1,uint8_t(cfg.program));}
  constexpr unsigned recipeRoots[]{0x68b0,0x68de,0x6922,0x6944,0x6970,0x69a6};for(unsigned n=0;n<6;++n)longValue(71306+4*n,cfg.recipeUpper|recipeRoots[n]);word(0x159f2,0x1234);byte(0x400004,7);byte(0xffff62,255);byte(0xffec22,7);
  const auto index=[&](unsigned program){const auto previous=uint8_t(cfg.bank-1);return program+(previous>4?400+unsigned(uint8_t(previous-4))*50:unsigned(previous)*100);};byte(0x820000 + 256*index(cfg.program)+cfg.field,uint8_t(cfg.current));byte(0x820000 + 256*index(49)+cfg.field,uint8_t(cfg.current^0x55));byte(0x81fe16 + cfg.field,uint8_t(cfg.current^0xaa));
  longValue(0xfff800,0x4338);const uint32_t target=live?0x433a:0x4338;unsigned count=0,liveMask=0;bool injected=false;
  do{
   if(count==2&&cfg.evolvingBase)longValue(0x167bc,cfg.evolvingBase);
   if(count==injectAt){if(vector<64)a->pending_irqs=b->pending_irqs=1ull<<vector;else a->pending_irqs_high=b->pending_irqs_high=1ull<<(vector-64);injected=true;}
   if(live){const auto once=[&](unsigned bit,uint32_t pc,auto&&change){if(!(liveMask&(1u<<bit))&&a->getPC()==pc){change();liveMask|=1u<<bit;++mutations;}};
    if(kind==0){once(0,0x6b18,[&]{longValue(0x167c0,cfg.live+1);});once(1,0x6b30,[&]{byte(0x169f9,1);});once(2,0x6ba8,[&]{byte(0x169ed,127);});}
    if(kind<=1){once(3,0xa194,[&]{byte(0x17526,4);});once(4,0xa19a,[&]{byte(0x167ba,1);});}
    if(kind==2||kind==4||kind==5||kind==6){once(0,0x619c,[&]{byte(kind==6?cfg.source:0x167ac,6);});once(1,0x61b4,[&]{byte((kind==6?cfg.source:0x167ac)+1,50);});}
    once(7,returns[kind],[&]{byte(0xfff803,0x3a);});if(kind==5)once(7,0x63ce,[&]{byte(0xfff803,0x3a);});if(kind==3)once(7,0x5820,[&]{byte(0xfff803,0x3a);});
   }
   if(count==1){runtime.resetMetrics();foreground.resetMetrics();++resets;}x.events.clear();y.events.clear();track=true;runtime.step(*b);track=false;a->step();
   if(a->cycles!=b->cycles){std::cerr<<"kind="<<kind<<" field="<<cfg.field<<" type="<<cfg.type<<" phase="<<count<<" old/new PC="<<std::hex<<a->getPC()<<'/'<<b->getPC()<<std::dec<<" clocks="<<a->cycles<<'/'<<b->cycles<<'\n';throw std::runtime_error("Apply phase clocks differ");}
   const auto expected=resources(x.events,decoder.get()),actual=resources(y.events,nullptr);if(expected!=actual){std::cerr<<"kind="<<kind<<" field="<<cfg.field<<" type="<<cfg.type<<" phase="<<count<<" old/new PC="<<std::hex<<a->getPC()<<'/'<<b->getPC()<<std::dec<<" transfers="<<expected.size()<<'/'<<actual.size()<<'\n';for(const auto&rows:{expected,actual}){for(const auto&e:rows)std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<std::dec<<' ';std::cerr<<'\n';}throw std::runtime_error("Apply resource byte/value/order/time differs");}
   check(a->pending_irqs==b->pending_irqs&&a->pending_irqs_high==b->pending_irqs_high&&a->exr==b->exr,"Apply IRQ/critical mask differs");
   if(b->pc!=target||b->indent){check(b->pc==roots[kind]&&b->getSP()==0xfff800&&b->ccr==uint8_t(flags),"Native Apply used CPU continuation");for(unsigned n=0;n<8;++n)check(b->regs[n].er==saved[n],"Native Apply used CPU scratch registers");}
   for(unsigned at=0xfff700;at<0xfff800;++at)check(b->memory[at]==0x5a,"Native Apply wrote CPU stack");check(++count<1000000,"Apply did not finish");++boundaries;
  }while(a->getPC()!=target||a->indent);
  check(!foreground.active()&&!runtime.activeNativeInterrupts(),"Apply left active continuation");check(foreground.started+foreground.initialActive==foreground.completed,"Apply metric reset lost owner");check(runtime.nativeInterruptStarts+runtime.nativeInterruptInitial==runtime.nativeInterruptReturns,"Apply IRQ owner balance differs");
  for(unsigned n=0;n<8;++n)if(a->regs[n].er!=b->regs[n].er){std::cerr<<"kind="<<kind<<" field="<<cfg.field<<" type="<<cfg.type<<" value="<<cfg.value<<" register="<<n<<" old/new="<<std::hex<<a->regs[n].er<<'/'<<b->regs[n].er<<std::dec<<'\n';throw std::runtime_error("Apply final register differs");}
  if(a->ccr!=b->ccr||a->exr!=b->exr||a->lastread!=b->lastread||a->lastwrite!=b->lastwrite){std::cerr<<"kind="<<kind<<" field="<<cfg.field<<" type="<<cfg.type<<" flags="<<std::hex<<unsigned(a->ccr)<<'/'<<unsigned(b->ccr)<<" history="<<a->lastread<<'/'<<b->lastread<<':'<<a->lastwrite<<'/'<<b->lastwrite<<std::dec<<'\n';throw std::runtime_error("Apply final flags/history differ");}
  check(b->getSP()==0xfff804,"Apply caller return differs");if(injected||live)check(std::equal(a->memory,a->memory+0xfff700,b->memory)&&std::equal(a->memory+0xfff800,a->memory+(1<<24),b->memory+0xfff800),"Apply evolving full memory differs");if(live)check(liveMask&128,"Apply live caller frame was not observed");components|=foreground.components;++cases;if(injected)++nested;return count;
 };
 for(unsigned kind=0;kind<7;++kind)for(unsigned flags=0;flags<256;++flags)for(uint32_t value:{0u,1u,0x7fffffffu,0x80000000u,0xffffffffu}){cfg=Config{};cfg.bank=flags;cfg.program=flags;verify(kind,value,flags);}
 for(unsigned field=0;field<256;++field)for(unsigned type:{0u,1u,2u,3u,4u,5u,6u,7u,127u,128u,254u,255u})for(unsigned value:{0u,1u,63u,127u,255u}){cfg=Config{};cfg.field=field;cfg.type=type;cfg.value=value;cfg.recipeUpper=(field&7)<<24;verify(0,0x80000000,(field+type)&255);}
 for(unsigned bank=0;bank<256;++bank)for(unsigned program=0;program<256;++program){cfg=Config{};cfg.bank=bank;cfg.program=program;verify(6,0x12345678,(bank+program)&255);if(!(program&31)){verify(4,0x12345678,program);verify(5,0x12345678,program);}}
 for(unsigned field=0;field<256;++field)for(unsigned bank:{0u,1u,4u,5u,6u,255u})for(unsigned program:{0u,49u,50u,255u}){cfg=Config{};cfg.field=field;cfg.bank=bank;cfg.program=program;verify(2,0x87654321,field);verify(3,0x87654321,field);}
 for(unsigned pickup:{0u,1u,255u})for(unsigned cache:{0u,31u,127u,253u,254u,255u})for(unsigned current:{0u,1u,63u,127u,255u})for(unsigned value:{0u,1u,62u,63u,64u,127u,255u}){cfg=Config{};cfg.type=0;cfg.pickup=pickup;cfg.cached=cache;cfg.current=current;cfg.value=value;cfg.policy=127;verify(0,0,0xe3);}
 for(unsigned type=0;type<256;++type)for(unsigned policy:{0u,62u,63u,64u,127u,255u})for(unsigned control:{0u,74u,127u,128u,255u}){cfg=Config{};cfg.type=type;cfg.policy=policy;cfg.control=control;verify(0,0x80000000,0xe3);}
 for(unsigned kind=0;kind<7;++kind){cfg=Config{};if(kind==0){cfg.type=6;cfg.field=226;cfg.policy=127;cfg.control=255;}if(kind==1)cfg.field=217;const auto count=verify(kind,0x80000000,0xe3);for(unsigned vector:{28u,32u,88u,89u,90u,91u})for(unsigned at=0;at<count;++at)verify(kind,0x80000000,0xe3,at,vector);}
 for(bool overlay:{false,true})for(unsigned kind=0;kind<7;++kind){cfg=Config{};cfg.overlay=overlay;if(kind<=1)cfg.field=217;if(kind==5)cfg.bank=6;verify(kind,0x80000000,0xe3,~0u,32,true);}
 for(uint32_t base:{0x24000u,0x24001u,0xa24000u,0xa24001u,0xffdc00u,0xffdc01u,0x80024000u,0xff024001u})for(unsigned type:{0u,6u,127u,255u}){cfg=Config{};cfg.base=base;cfg.type=type;cfg.field=217;cfg.policy=127;verify(0,0x80000000,0xe3);}
 for(uint32_t changed:{0x1024000u,0x80024000u,0xff024000u}){cfg=Config{};cfg.type=127;cfg.evolvingBase=changed;verify(0,0x80000000,0xe3);}
 for(unsigned busy:{1u,3u,65537u}){cfg=Config{};cfg.field=217;cfg.busy=busy;verify(0,0x80000000,0xe3);}
 for(unsigned wait:{1u,3u}){cfg=Config{};cfg.type=0;cfg.policy=127;cfg.txWait=wait;verify(0,0x80000000,0xe3);}
 for(unsigned kind=0;kind<7;++kind)for(unsigned variant=0;variant<10;++variant){foreground.resetMetrics();b->pc=roots[kind];b->setSP(0xfff800);b->indent=1;b->cycles=1000;b->ccr=0xe3;b->exr=0xa0;b->lastread=0x12345;b->lastwrite=0x23456;b->pending_irqs=0x123;b->pending_irqs_high=0x456;b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->nativeLowSize=0x200000;b->interruptMode2=true;
  switch(variant){case 0:b->nativeFetchClocksOnly=false;break;case 1:b->nativeLowBase=nullptr;break;case 2:b->interruptMode2=false;break;case 3:b->indent=0;break;case 4:b->indent=-1;break;case 5:b->setSP(0xfff801);break;case 6:b->setSP(0xfff3fe);break;case 7:b->setSP(0xfffbfe);break;case 8:b->setSP(0xa18000);break;case 9:b->pc=0x4eb0;break;}
  std::array<int32_t,8>saved{};for(unsigned n=0;n<8;++n)saved[n]=b->regs[n].er;const auto pc=b->pc;const auto depth=b->indent;y.events.clear();track=true;const auto handled=foreground.step(*b);track=false;check(!handled&&!foreground.active()&&!foreground.started&&!foreground.phases&&!foreground.components&&y.events.empty(),"Rejected Apply admission claimed resources/owner");check(b->pc==pc&&b->indent==depth&&b->cycles==1000&&b->ccr==0xe3&&b->exr==0xa0&&b->lastread==0x12345&&b->lastwrite==0x23456&&b->pending_irqs==0x123&&b->pending_irqs_high==0x456,"Rejected Apply changed caller/IRQs");for(unsigned n=0;n<8;++n)check(b->regs[n].er==saved[n],"Rejected Apply changed registers");++guardCases;
 }
 for(unsigned kind:{0u,1u,6u})for(uint32_t address:{0xffefffu,0xfff000u,0xfff7ffu,0xfffbffu,0xffffffu}){foreground.resetMetrics();b->pc=roots[kind];b->setSP(0xfff800);b->indent=1;b->cycles=1000;b->nativeFetchClocksOnly=true;b->nativeLowBase=b->memory;b->nativeLowSize=0x200000;b->interruptMode2=true;if(kind==6)b->regs[0].er=int32_t(address);else b->regs[1].er=int32_t(address);y.events.clear();track=true;const auto handled=foreground.step(*b);track=false;check(!handled&&!foreground.active()&&!foreground.started&&!foreground.phases&&b->cycles==1000&&y.events.empty(),"Unsafe Apply pair claimed owner");++guardCases;}
 check(components==0x1ffff&&guardCases==85&&!allocations&&!b->interpretedSteps,"Apply closure/guard/allocation/interpreter contract differs");std::cout<<"PASS: "<<cases<<" CPU-free Apply/Stored calls; "<<boundaries<<" independent H8 phases; "<<nested<<" native IRQ preemptions; "<<resets<<" metric resets; "<<mutations<<" live resource changes; "<<guardCases<<" guards; seven roots and17children, all flags/values, packing/pickup/store/MIDI/UART/DSP readiness+timeout, bank/program lookup/alias and evolving low/overlay resources; exact bytes/clocks/final ABI; zero allocations/interpreter/internal CPU stack writes\n";return 0;
 }catch(const std::exception&e){track=false;std::cerr<<e.what()<<'\n';return 1;}}
