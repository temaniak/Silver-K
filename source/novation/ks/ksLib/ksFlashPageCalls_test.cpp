#include "ksRuntimeController.h"
#include "ksFlashPageCalls.h"
#include "ksLiveMidiController.h"
#include "ksRom.h"
#include "h8s/h8s.hpp"
#include <algorithm>
#include <cstdlib>
#include <new>
#include <array>
#include <cstring>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <tuple>
#include <vector>
namespace {bool trackAllocations=false;uint64_t nativeAllocations=0;}
void* operator new(std::size_t count){if(trackAllocations)++nativeAllocations;if(auto* p=std::malloc(count?count:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t count){return ::operator new(count);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
namespace {
std::unique_ptr<H8SDisassembler> lengthOracle;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
using Access=std::tuple<bool,uint32_t,uint8_t,uint64_t,uint32_t>;
struct Original final:H8SEmulator {using H8SEmulator::lastread;using H8SEmulator::lastwrite;using H8SEmulator::indent;};
template<class Device>struct MockBus final:Device {
 std::vector<Access> events;uint8_t pitch=0,middle=0x55,low=0xaa;unsigned polls=0,delay=0;bool timeout=false;
 uint8_t read(uint32_t a)override{uint8_t value=this->state->memory[a];
 if(a==0xffec13&&unsigned(this->state->getPC())==0x1247){value=uint8_t(value+(timeout?15:polls));++polls;}
 if(a>=0x800000&&a<0x880000&&unsigned(this->state->getPC())==0x1255&&polls<=delay)value^=1;if(a==0x40000a||a==0x60000a)value=pitch;else if(a==0x40000c||a==0x60000c)value=middle;else if(a==0x40000e||a==0x60000e)value=low;events.emplace_back(false,a,value,this->state->cycles,uint32_t(this->state->getPC()));return value;}
 void write(uint32_t a,uint8_t value)override{events.emplace_back(true,a,value,this->state->cycles,uint32_t(this->state->getPC()));this->state->memory[a]=value;}
};
using OriginalBus=MockBus<H8SDevice>;using NativeBus=MockBus<ks::ControllerBus>;
void same(const Original& a,const ks::ControllerCpu& b,OriginalBus& x,const NativeBus& y,bool clockOnly){
    check(a.getPC()==b.getPC(),"Flash page call PC projection differs");
    check(a.cycles==b.cycles,"Flash page call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"Flash page call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("Flash page call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"Flash page call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"Flash page call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==std::get<4>(e);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original Flash page fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original Flash page fetch not paired");
        const auto entry=std::get<1>(*first);lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
        const unsigned bytes=unsigned(lengthOracle->getPC())-entry;
        check(bytes>=2 && bytes<=10,"Invalid reference fetch length");
        for(unsigned i=0;i<bytes;++i)check(first+i!=x.events.end() && !std::get<0>(first[i]) && std::get<1>(first[i])==std::get<1>(*first)+i,"Original Flash page fetch shape differs");
        x.events.erase(first,first+bytes);
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("Flash page call data/stack/value/order/clock/in-flight context differs");
    }
}
}
int main(int argc,char** argv){try{if(argc!=2)return 77;check(!ks::controllerReferenceAvailable(),"Reference factory linked");ks::Rom os(argv[1]);check(os.isValid(),"Invalid OS");auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();lengthOracle=std::make_unique<H8SDisassembler>();lengthOracle->loadmem(os.data().data(),unsigned(os.data().size()),0);g_dasm=false;for(unsigned base:{0u,0xa00000u}){a->loadmem(os.data().data(),unsigned(os.data().size()),base);b->loadmem(os.data().data(),unsigned(os.data().size()),base);}ks::LiveMidiController shared(*b);ks::RuntimeController runtime(*b,shared);auto& calls=shared.flashPageCalls;OriginalBus x;NativeBus y;x.events.reserve(64);y.events.reserve(64);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->interpreterAllowed=false;
 for(auto* m:{a->memory,b->memory}){m[112]=m[113]=0;m[114]=1;m[115]=0x70;}std::mt19937 rng(0x4b534e41);uint64_t cases=0,steps=0,irqs=0,resets=0,nests=0;std::array<unsigned,0x9000> first{};constexpr unsigned roots[]{0x11ae,0x1272};
 auto verify=[&](unsigned binding,unsigned seed,unsigned flags,bool clockOnly,bool odd,bool tagged,bool mutate,unsigned irqAt,bool nested=false){shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();calls.discardCalls();calls.resetMetrics();shared.resetMetrics();for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());const unsigned entry=roots[binding],stack=odd?0xfff001u:0xa18000u;const uint32_t patch=tagged?0xffa30000:0x30000,provided=(tagged?0xffa33000:0x33000)+(seed&1),selected=(tagged?0xffa34000:0x34000)+((seed>>2)&1);
  for(auto* m:{a->memory,b->memory}){for(unsigned i=0;i<4;++i){m[0x167bc+i]=uint8_t(patch>>(24-i*8));m[0x1748e +i]=uint8_t(selected>>(24-i*8));}for(unsigned i=0;i<256;++i){m[0x30000+i]=uint8_t(seed*13+i*17);m[0x169fc+i]=uint8_t(seed*23+i*31);m[0x33000+i]=uint8_t(seed*37+i*43);m[0x34000+i]=uint8_t(seed*47+i*53);}for(unsigned i=0;i<17;++i)m[0x1733c+i]=uint8_t(seed*59+i*61);for(unsigned i=0;i<256;++i){m[(provided+i)&0xffffff]=uint8_t(seed*13+i*17);m[0x820000+i]=seed&4?uint8_t(seed*13+i*17):uint8_t(seed*23+i*31);}for(unsigned i=0;i<256;++i){m[0x820000+i]=m[(provided+i)&0xffffff];}
  if(seed%8!=5)m[0x820000+uint8_t(seed)]^=0x80;
  for(unsigned i=0;i<4;++i)m[0xffec10+i]=uint8_t((0xfffffff8u+seed)>>(24-i*8));
  m[0xffed6a]=uint8_t(seed&128?255:seed);m[0xffed6b]=uint8_t(seed&128?255:seed);m[stack]=m[stack+1]=0;m[stack+2]=0x43;m[stack+3]=0x38;}
  a->regs[5].er=b->regs[5].er=provided;a->regs[6].er=b->regs[6].er=binding?0x820055:seed%8==0?0x800001:seed%8==2?0x820001:seed%8==3?0x7fff00:0x820000;a->setSP(stack);b->setSP(stack);a->pc=a->makepc(entry);b->pc=b->makepc(entry);a->ccr=b->ccr=flags;a->exr=b->exr=0;a->cycles=b->cycles=1000;a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;b->nativeFetchClocksOnly=clockOnly;
  x.polls=y.polls=0;x.delay=y.delay=seed%8==4?3:0;x.timeout=y.timeout=seed%8==6;unsigned count=0;bool injected=false;auto step=[&]{const unsigned before=unsigned(a->getPC());x.events.clear();y.events.clear();try{trackAllocations=true;runtime.step(*b);trackAllocations=false;a->step();same(*a,*b,x,y,clockOnly);}catch(const std::exception&){trackAllocations=false;std::cerr<<"before="<<std::hex<<before<<std::dec<<" root="<<binding<<" seed="<<seed<<" flags="<<flags<<" clock="<<clockOnly<<" odd="<<odd<<" tagged="<<tagged<<'\n';throw;}if(before<first.size()&&!first[before])first[before]=count+1;++steps;};
  do{if(count==irqAt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}if(count%61==5){calls.resetMetrics();shared.resetMetrics();++resets;}if(mutate)for(unsigned i=0;i<256;++i)for(auto* m:{a->memory,b->memory}){m[0x30000+i]=uint8_t(seed+count*13+i*17);m[0x169fc+i]=uint8_t(seed+count*23+i*31);m[0x33000+i]=uint8_t(seed+count*37+i*43);m[0x34000+i]=uint8_t(seed+count*47+i*53);if(i<17)m[0x1733c+i]=uint8_t(seed+count*59+i*61);}step();check(++count<65536,"Flash page call stalled");
   if(nested&&count==5){std::array<int32_t,8> av{},bv{};for(unsigned i=0;i<8;++i){av[i]=a->regs[i].er;bv[i]=b->regs[i].er;}const auto ap=a->pc;const auto bp=b->pc;const auto ac=a->ccr,bc=b->ccr;const int ai=a->indent,bi=b->indent;const unsigned child=stack-512;a->setSP(child);b->setSP(child);a->pc=a->makepc(entry);b->pc=b->makepc(entry);a->regs[5].er=b->regs[5].er=provided;a->regs[6].er=b->regs[6].er=binding?0x820055:seed%8==0?0x800001:seed%8==2?0x820001:seed%8==3?0x7fff00:0x820000;for(auto* m:{a->memory,b->memory}){m[child]=m[child+1]=0;m[child+2]=0x43;m[child+3]=0x38;}unsigned n=0;do{step();check(++n<65536,"Nested Flash page stalled");if(n==7){calls.resetMetrics();shared.resetMetrics();++resets;}}while(a->getPC()!=0x4338||a->getSP()!=child+4);for(unsigned i=0;i<8;++i){a->regs[i].er=av[i];b->regs[i].er=bv[i];}a->pc=ap;b->pc=bp;a->ccr=ac;b->ccr=bc;a->indent=ai;b->indent=bi;++nests;}
  }while(a->getPC()!=0x4338||a->getSP()!=stack+4);check(!b->interpretedSteps&&!calls.partialEntries,"Flash page recovery");check(calls.started+calls.initialActive==calls.completed+calls.activeCalls()&&!calls.activeCalls(),"Flash page metadata lifetime");++cases;if(injected)++irqs;};
 for(bool clock:{false,true})for(unsigned binding=0;binding<2;++binding){for(unsigned seed=0;seed<256;++seed)verify(binding,seed,seed,clock,seed&1,seed&2,false,unsigned(-1));for(unsigned flags=0;flags<256;++flags)verify(binding,flags,flags,clock,flags&1,flags&2,true,unsigned(-1));for(unsigned seed:{0u,1u,2u,3u,5u,127u,255u}){first.fill(0);verify(binding,seed,0xd0,clock,false,false,false,unsigned(-1));const auto selected=first;for(unsigned pc=0x11ae;pc<0x12b0;++pc)if(selected[pc])verify(binding,seed,0xe7,clock,true,true,true,selected[pc]-1);}for(unsigned seed:{0u,1u,2u,3u,5u})verify(binding,seed,0x80,clock,true,true,false,unsigned(-1),true);}
 check(std::memcmp(a->memory,b->memory,1<<24)==0,"Full memory differs");
 check(nativeAllocations==0,"Native Flash stepping allocated");
 calls.discardCalls();calls.resetMetrics();
 for(unsigned i=0;i<16;++i){b->setSP(0xa18000-i*128);b->pc=b->makepc(0x11ae);check(calls.step(*b),"Capacity admission");}
 check(calls.activeCalls()==16&&calls.started==16,"Bounded call storage differs");
 b->setSP(0xa17000);b->pc=b->makepc(0x11ae);bool rejected=false;
 try{calls.step(*b);}catch(const std::runtime_error&){rejected=true;}
 check(rejected&&calls.activeCalls()==16,"Capacity overflow did not reject explicitly");
 calls.discardCalls();check(calls.activeCalls()==0,"Call storage did not clear");
 // The deliberate capacity test writes native-only synthetic stack frames.
 check(std::memcmp(a->memory,b->memory,0xa16000)==0,"Full data memory differs");std::cout<<"PASS: "<<cases<<" full Flash-page calls, "<<steps<<" original steps, "<<irqs<<" IRQ, "<<resets<<" resets, "<<nests<<" nested, pagecompare/program/unchanged/protected/invalid/delayed-ready/timeout/pending-wrap,exact bus/state/clocks/full memory,zero recovery/native allocations; fixed16-call capacity overflow rejected\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
