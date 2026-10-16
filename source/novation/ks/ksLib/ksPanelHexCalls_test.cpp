#include "ksPanelActionCalls.h"
#include "ksPanelHexCalls.h"
#include "ksPanelPollCalls.h"
#include "ksPanelByteCalls.h"
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
 std::vector<Access> events;uint8_t pitch=0,middle=0x55,low=0xaa;unsigned busy=0,index=0;uint8_t ready=0;
 uint8_t read(uint32_t a)override{uint8_t value=this->state->memory[a];if(a==0xffff51)value=index++<busy?uint8_t(ready|128):ready;if(a==0x40000a||a==0x60000a)value=pitch;else if(a==0x40000c||a==0x60000c)value=middle;else if(a==0x40000e||a==0x60000e)value=low;events.emplace_back(false,a,value,this->state->cycles,uint32_t(this->state->getPC()));return value;}
 void write(uint32_t a,uint8_t value)override{events.emplace_back(true,a,value,this->state->cycles,uint32_t(this->state->getPC()));this->state->memory[a]=value;}
};
using OriginalBus=MockBus<H8SDevice>;using NativeBus=MockBus<ks::ControllerBus>;
void same(const Original& a,const ks::ControllerCpu& b,OriginalBus& x,const NativeBus& y,bool clockOnly){
    check(a.getPC()==b.getPC(),"voice-panelHex call PC projection differs");
    check(a.cycles==b.cycles,"voice-panelHex call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"voice-panelHex call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("voice-panelHex call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"voice-panelHex call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"voice-panelHex call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==std::get<4>(e);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original voice-panelHex fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original voice-panelHex fetch not paired");
        const auto entry=std::get<1>(*first);lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
        const unsigned bytes=unsigned(lengthOracle->getPC())-entry;
        check(bytes>=2 && bytes<=10,"Invalid reference fetch length");
        for(unsigned i=0;i<bytes;++i)check(first+i!=x.events.end() && !std::get<0>(first[i]) && std::get<1>(first[i])==std::get<1>(*first)+i,"Original voice-panelHex fetch shape differs");
        x.events.erase(first,first+bytes);
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("voice-panelHex call data/stack/value/order/clock/in-flight context differs");
    }
}
}

int main(int argc,char**argv){try{
 if(argc!=2)return 77;check(!ks::controllerReferenceAvailable(),"Fixture linked synchronized H8 factory");ks::Rom os(argv[1]);check(os.isValid(),"Invalid ROM");
 auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();lengthOracle=std::make_unique<H8SDisassembler>();lengthOracle->loadmem(os.data().data(),unsigned(os.data().size()),0);g_dasm=false;
 for(unsigned base:{0u,0xa00000u}){a->loadmem(os.data().data(),unsigned(os.data().size()),base);b->loadmem(os.data().data(),unsigned(os.data().size()),base);}
 ks::LiveMidiController shared(*b);ks::RuntimeController runtime(*b,shared);auto& panelAction=shared.panelActionCalls;auto& panelHex=shared.panelHexCalls;auto& panelPoll=shared.panelPollCalls;auto& panelByte=shared.panelByteCalls;OriginalBus x;NativeBus y;x.events.reserve(256);y.events.reserve(256);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->interpreterAllowed=false;
 for(auto*m:{a->memory,b->memory}){m[112]=m[113]=0;m[114]=1;m[115]=0x70;}
 std::mt19937 rng(0x4b535357);uint64_t cases=0,steps=0,irqs=0,resets=0,nestedCases=0;std::array<unsigned,0xa200> first{};
 auto verify=[&](unsigned kind,unsigned count,unsigned pattern,unsigned note,unsigned transpose,unsigned pitch,unsigned flags,bool clockOnly,bool odd,unsigned interruptAt,bool mutate=false,bool nested=false,unsigned part=2){
 panelAction.discardCalls();panelAction.resetMetrics();panelHex.discardCalls();panelHex.resetMetrics();panelPoll.discardCalls();panelByte.discardCalls();shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();shared.resetMetrics();for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());
 const unsigned stack=odd?0xfff001u:0xa18000u;a->setSP(stack);b->setSP(stack);
 for(auto*m:{a->memory,b->memory}){
 m[0x400004]=m[0x600004]=7;m[0x17526]=uint8_t(part);
 for(unsigned v=0;v<256;++v){const unsigned at=0x174ae + v*20,patch=0x30000+v*256;m[at]=0;m[at+1]=uint8_t(patch>>16);m[at+2]=uint8_t(patch>>8);m[at+3]=uint8_t(patch);for(unsigned field=0;field<256;++field)m[patch+field]=uint8_t(note+field*7+v);}
 m[0x17526]=uint8_t(part);m[0x30000+123]=uint8_t(note);m[0x30000+97]=uint8_t(transpose);m[0x30000+98]=uint8_t(pitch);m[0x30000+99]=uint8_t(pattern);m[0x30000+81]=uint8_t(count);m[0x30000+82]=uint8_t(part);m[0x30000+83]=uint8_t(flags);for(unsigned v=0;v<256;++v){unsigned at=0x174ae + v*20;for(unsigned f=0;f<20;++f)m[at+f]=0;m[at+1]=3;m[at+9]=uint8_t(transpose);m[at+10]=uint8_t(count);}m[0x17526]=uint8_t(part);m[0x167ba]=0;m[0xffff51]=uint8_t(transpose);m[0xfffeb1]=uint8_t(count);m[0xffff61]=uint8_t(pitch);m[0xffff62]=uint8_t(pattern);m[stack]=m[stack+1]=0;m[stack+2]=0x43;m[stack+3]=0x38;}
 x.busy=y.busy=pattern;x.index=y.index=0;x.ready=y.ready=uint8_t(transpose&127);a->regs[6].er=b->regs[6].er=0x30000;a->regs[5].er=b->regs[5].er=0x600000;a->regs[6].rl=b->regs[6].rl=int8_t(note);a->regs[6].rh=b->regs[6].rh=int8_t(count);a->regs[0].rl=b->regs[0].rl=int8_t(note);a->regs[0].rh=b->regs[0].rh=int8_t(count);a->pc=a->makepc((kind==0?0xab8:kind==1?0xad8:kind==2?0xb00:0xb18));b->pc=b->makepc((kind==0?0xab8:kind==1?0xad8:kind==2?0xb00:0xb18));a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=uint8_t(pitch);a->cycles=b->cycles=1000;
 a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;
 for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;b->nativeFetchClocksOnly=clockOnly;
 unsigned boundary=0;bool injected=false;
 auto step=[&]{unsigned before=unsigned(a->getPC());x.events.clear();y.events.clear();try{runtime.step(*b);a->step();same(*a,*b,x,y,clockOnly);}catch(const std::exception&){std::cerr<<"before="<<std::hex<<before<<std::dec<<" retune="<<kind<<" count="<<count<<" pattern="<<pattern<<" note="<<note<<" transpose="<<transpose<<" pitch="<<pitch<<" flags="<<flags<<" clock="<<clockOnly<<" odd="<<odd<<'\n';throw;}if(before<first.size()&&!first[before])first[before]=boundary+1;++steps;};
 do{
 if(boundary==interruptAt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}
 if(boundary%37==5){shared.resetMetrics();panelAction.resetMetrics();panelHex.resetMetrics();++resets;}
 if(mutate){const auto here=a->getPC();if(ks::PanelHexCalls::candidate(here)){if(here==0xac0||here==0xae0||here==0xb08||here==0xb18)a->regs[0].rl=b->regs[0].rl=int8_t(note+boundary*17);}}
 step();check(++boundary<100000,"Byte stalled");
 if(nested&&boundary==5){std::array<int32_t,8> av{},bv{};for(unsigned i=0;i<8;++i){av[i]=a->regs[i].er;bv[i]=b->regs[i].er;}const auto ap=a->pc;const auto bp=b->pc;const auto ac=a->ccr,bc=b->ccr;const int ai=a->indent,bi=b->indent;const unsigned child=stack-256;a->setSP(child);b->setSP(child);for(auto*m:{a->memory,b->memory}){m[child]=m[child+1]=0;m[child+2]=0x43;m[child+3]=0x38;}a->regs[6].er=b->regs[6].er=0x30000;a->regs[5].er=b->regs[5].er=0x600000;a->regs[6].rl=b->regs[6].rl=int8_t(note);a->regs[6].rh=b->regs[6].rh=int8_t(count);a->regs[0].rl=b->regs[0].rl=int8_t(note);a->regs[0].rh=b->regs[0].rh=int8_t(count);a->pc=a->makepc((kind==0?0xab8:kind==1?0xad8:kind==2?0xb00:0xb18));b->pc=b->makepc((kind==0?0xab8:kind==1?0xad8:kind==2?0xb00:0xb18));unsigned childCount=0;do{step();check(++childCount<100000,"Nested panelHex stalled");}while(a->getPC()!=0x4338);check(!panelHex.step(*b),"Byte retirement consumed a boundary");for(unsigned i=0;i<8;++i){a->regs[i].er=av[i];b->regs[i].er=bv[i];}a->pc=ap;b->pc=bp;a->ccr=ac;b->ccr=bc;a->indent=ai;b->indent=bi;++nestedCases;}
 }while(a->getPC()!=0x4338);
 check(!panelHex.step(*b),"Byte retirement consumed a boundary");
 check(!b->interpretedSteps,"Native panelHex interpreted");++cases;if(injected)++irqs;return boundary;
 };
 for(bool clock:{false,true}){
 for(unsigned kind=0;kind<4;++kind)for(unsigned high=0;high<256;++high)for(unsigned input=0;input<256;++input)verify(kind,high,0,input,0,0,(high+input*149)&255,clock,bool(input&1),unsigned(-1));
 for(unsigned kind=0;kind<4;++kind)for(unsigned input=0;input<256;++input)verify(kind,255,0,input,20,0,input,clock,bool(input&1),unsigned(-1));
 for(unsigned kind=0;kind<4;++kind)for(unsigned input:{0u,1u,9u,10u,15u,16u,127u,128u,255u}){first.fill(0);verify(kind,255,0,input,0,0,0xd0,clock,false,unsigned(-1));auto selected=first;for(unsigned pc=0;pc<selected.size();++pc)if(selected[pc]&&ks::PanelHexCalls::candidate(pc))verify(kind,255,0,input,0,0,0xe7,clock,true,selected[pc]-1);verify(kind,255,0,input,0,0,0xd0,clock,true,unsigned(-1),true);verify(kind,255,0,input,0,0,0xd0,clock,true,unsigned(-1),false,true);}
 }
 for(bool clock:{false,true})for(unsigned kind=0;kind<4;++kind)for(unsigned mask=0;mask<256;++mask)for(unsigned flags:{0u,0xffu,0xa5u})verify(kind,255,0,mask,0,mask,flags,clock,bool(mask&1),unsigned(-1));
 check(std::memcmp(a->memory,b->memory,1<<24)==0,"Final panelHex full memory differs");std::cout<<"PASS: "<<cases<<" actual native panelHex calls; "<<steps<<" independent original steps; "<<irqs<<" selected IRQs; "<<resets<<" reset boundaries; "<<nestedCases<<" nested calls; exact clocks/accesses/state/full memory, zero interpreter/factory\n";
 return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
