#include "ksVoiceRefreshCalls.h"
#include "ksRuntimeController.h"
#include "ksLiveMidiController.h"
#include "ksRom.h"
#include "h8s/h8s.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <tuple>
#include <vector>
namespace {
std::unique_ptr<H8SDisassembler> lengthOracle;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
using Access=std::tuple<bool,uint32_t,uint8_t,uint64_t,uint32_t>;
struct Original final:H8SEmulator {using H8SEmulator::lastread;using H8SEmulator::lastwrite;using H8SEmulator::indent;};
template<class Device>struct MockBus final:Device {
 std::vector<Access> events;uint8_t pitch=0,middle=0x55,low=0xaa;
 uint8_t read(uint32_t a)override{uint8_t value=this->state->memory[a];if(a==0x40000a||a==0x60000a)value=pitch;else if(a==0x40000c||a==0x60000c)value=middle;else if(a==0x40000e||a==0x60000e)value=low;events.emplace_back(false,a,value,this->state->cycles,uint32_t(this->state->getPC()));return value;}
 void write(uint32_t a,uint8_t value)override{events.emplace_back(true,a,value,this->state->cycles,uint32_t(this->state->getPC()));this->state->memory[a]=value;}
};
using OriginalBus=MockBus<H8SDevice>;using NativeBus=MockBus<ks::ControllerBus>;
void same(const Original& a,const ks::ControllerCpu& b,OriginalBus& x,const NativeBus& y,bool clockOnly){
    check(a.getPC()==b.getPC(),"voice-refresh call PC projection differs");
    check(a.cycles==b.cycles,"voice-refresh call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"voice-refresh call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("voice-refresh call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"voice-refresh call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"voice-refresh call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==std::get<4>(e);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original voice-refresh fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original voice-refresh fetch not paired");
        const auto entry=std::get<1>(*first);lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
        const unsigned bytes=unsigned(lengthOracle->getPC())-entry;
        check(bytes>=2 && bytes<=10,"Invalid reference fetch length");
        for(unsigned i=0;i<bytes;++i)check(first+i!=x.events.end() && !std::get<0>(first[i]) && std::get<1>(first[i])==std::get<1>(*first)+i,"Original voice-refresh fetch shape differs");
        x.events.erase(first,first+bytes);
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("voice-refresh call data/stack/value/order/clock/in-flight context differs");
    }
}
}


int main(int argc,char**argv){try{
 if(argc!=2)return 77;check(!ks::controllerReferenceAvailable(),"Fixture linked synchronized H8 factory");ks::Rom os(argv[1]);check(os.isValid(),"Invalid ROM");
 auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();lengthOracle=std::make_unique<H8SDisassembler>();lengthOracle->loadmem(os.data().data(),unsigned(os.data().size()),0);g_dasm=false;
 for(unsigned base:{0u,0xa00000u}){a->loadmem(os.data().data(),unsigned(os.data().size()),base);b->loadmem(os.data().data(),unsigned(os.data().size()),base);}
 ks::LiveMidiController shared(*b);ks::RuntimeController runtime(*b,shared);auto& refresh=shared.voiceActions.refreshCalls;OriginalBus x;NativeBus y;x.events.reserve(256);y.events.reserve(256);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->interpreterAllowed=false;
 for(auto*m:{a->memory,b->memory}){m[112]=m[113]=0;m[114]=1;m[115]=0x70;}
 std::mt19937 rng(0x4b535357);uint64_t cases=0,steps=0,irqs=0,resets=0,nestedCases=0;std::array<unsigned,0x9700> first{};
 auto verify=[&](unsigned kind,unsigned count,unsigned pattern,unsigned note,unsigned transpose,unsigned pitch,unsigned flags,bool clockOnly,bool odd,unsigned interruptAt,bool mutate=false,bool nested=false,unsigned part=2){
 refresh.discardCalls();refresh.resetMetrics();shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();shared.resetMetrics();for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());
 const unsigned stack=odd?0xfff001u:0xa18000u;a->setSP(stack);b->setSP(stack);
 for(auto*m:{a->memory,b->memory}){
 m[0x400004]=m[0x600004]=7;m[0x17526]=uint8_t(part);m[0x174a6]=uint8_t(count);m[0x17520]=uint8_t(transpose);m[0x17523]=uint8_t(transpose);
 for(unsigned v=0;v<256;++v){const unsigned at=0x17560+v*20,patch=0x30000+v*256;
 m[at]=0;m[at+1]=uint8_t(patch>>16);m[at+2]=uint8_t(patch>>8);m[at+3]=uint8_t(patch);m[at+4]=uint8_t(pattern&2?v%4:part);m[at+5]=uint8_t(note+v);m[at+6]=100;
 const unsigned age=(pattern&1)?((v%3)?0:0xffff):(pattern&4?0:0x8000|v);m[at+8]=uint8_t(age>>8);m[at+9]=uint8_t(age);m[at+10]=0x23;m[at+11]=0x45;m[at+13]=uint8_t(note+v);m[at+17]=2;m[patch+168]=uint8_t(pitch);}
 for(unsigned at=0;at<80;++at)m[0x174ae + at]=uint8_t(at*7+3);
 for(unsigned part=0;part<4;++part){const unsigned at=0x174ae + part*20;m[at]=0;m[at+1]=3;m[at+2]=0x40;m[at+3]=0;}
 for(unsigned at=0;at<256;++at)m[0x34000+at]=uint8_t(pitch+at*7);
 m[stack]=m[stack+1]=0;m[stack+2]=0x43;m[stack+3]=0x38;}
 a->pc=a->makepc(kind==0?0x958e:kind==1?0x95f4:0x9662);b->pc=b->makepc(kind==0?0x958e:kind==1?0x95f4:0x9662);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=0;a->cycles=b->cycles=1000;
 a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;
 for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;b->nativeFetchClocksOnly=clockOnly;
 unsigned boundary=0;bool injected=false;
 auto step=[&]{unsigned before=unsigned(a->getPC());x.events.clear();y.events.clear();try{runtime.step(*b);a->step();same(*a,*b,x,y,clockOnly);}catch(const std::exception&){std::cerr<<"before="<<std::hex<<before<<std::dec<<" retune="<<kind<<" count="<<count<<" pattern="<<pattern<<" note="<<note<<" transpose="<<transpose<<" pitch="<<pitch<<" flags="<<flags<<" clock="<<clockOnly<<" odd="<<odd<<'\n';throw;}if(before<first.size()&&!first[before])first[before]=boundary+1;++steps;};
 do{
 if(boundary==interruptAt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}
 if(boundary%37==5){shared.resetMetrics();refresh.resetMetrics();++resets;}
 if(mutate){for(auto*m:{a->memory,b->memory}){m[0x17526]=uint8_t((part+(boundary>>3))%4);m[0x17520]=uint8_t(transpose+boundary*37);m[0x17523]=uint8_t(boundary*41);m[0x174a7]=uint8_t(boundary%14);for(unsigned v=0;v<256;++v){m[0x17560+v*20+4]=uint8_t((boundary+v)%4);m[0x17560+v*20+5]=uint8_t(note+boundary+v);m[0x30000+v*256+168]=uint8_t(pitch+boundary*11);}}}
 step();check(++boundary<2000000,"Refresh stalled");
 if(nested&&boundary==5){std::array<int32_t,8> av{},bv{};for(unsigned i=0;i<8;++i){av[i]=a->regs[i].er;bv[i]=b->regs[i].er;}const auto ap=a->pc;const auto bp=b->pc;const auto ac=a->ccr,bc=b->ccr;const int ai=a->indent,bi=b->indent;const unsigned child=stack-256;a->setSP(child);b->setSP(child);for(auto*m:{a->memory,b->memory}){m[child]=m[child+1]=0;m[child+2]=0x43;m[child+3]=0x38;}a->pc=a->makepc(0x958e);b->pc=b->makepc(0x958e);unsigned childCount=0;do{step();check(++childCount<2000000,"Nested refresh stalled");}while(a->getPC()!=0x4338);for(unsigned i=0;i<8;++i){a->regs[i].er=av[i];b->regs[i].er=bv[i];}a->pc=ap;b->pc=bp;a->ccr=ac;b->ccr=bc;a->indent=ai;b->indent=bi;++nestedCases;}
 }while(a->getPC()!=0x4338);
 check(!b->interpretedSteps,"Native refresh interpreted");++cases;if(injected)++irqs;return boundary;
 };
 for(bool clock:{false,true}){
 for(unsigned kind=0;kind<3;++kind)for(unsigned count:{0u,1u,2u,14u,15u,255u})for(unsigned pattern=0;pattern<8;++pattern)for(unsigned mode:{0u,1u,255u})verify(kind,count,pattern,60,mode,64,(count+pattern+mode)&255,clock,bool(count&1),unsigned(-1));
 for(unsigned kind=0;kind<3;++kind)for(unsigned flags=0;flags<256;++flags)verify(kind,1,0,255,0,flags,flags,clock,bool(flags&1),unsigned(-1));
 for(unsigned kind=0;kind<3;++kind)for(unsigned pattern:{0u,1u,4u}){first.fill(0);verify(kind,2,pattern,60,0,64,0xd0,clock,false,unsigned(-1));const auto selected=first;for(unsigned pc=0;pc<selected.size();++pc)if(selected[pc]&&ks::VoiceRefreshCalls::candidate(pc))verify(kind,2,pattern,60,0,64,0xe7,clock,true,selected[pc]-1);verify(kind,2,pattern,60,0,64,0xd0,clock,true,unsigned(-1),true);verify(kind,2,pattern,60,0,64,0xd0,clock,true,unsigned(-1),false,true);}
 }
 check(std::memcmp(a->memory,b->memory,1<<24)==0,"Final refresh full memory differs");std::cout<<"PASS: "<<cases<<" actual native refresh calls; "<<steps<<" independent original steps; "<<irqs<<" selected IRQs; "<<resets<<" reset boundaries; "<<nestedCases<<" nested calls; exact clocks/accesses/state/full memory, zero interpreter/factory\n";
 return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
