#include "ksRuntimeController.h"
#include "ksKeyPriorityCalls.h"
#include "ksNoteOffStateCalls.h"
#include "ksPartNoteCalls.h"
#include "ksPedalCalls.h"
#include "ksMidiControlCalls.h"
#include "ksMidiAuxCalls.h"
#include "ksMidiRouteCalls.h"
#include "ksArpPrimitiveCalls.h"
#include "ksArpScheduleCalls.h"
#include "ksArpMainCalls.h"
#include "ksArpEventCalls.h"
#include "ksArpSelectCalls.h"
#include "ksInternalMidiCalls.h"
#include "ksInternalNoteCalls.h"
#include "ksSysExFrameCalls.h"
#include "ksPatchExpandCalls.h"
#include "ksSysExActionCalls.h"
#include "ksLegacySysExCalls.h"
#include "ksSysExDumpCalls.h"
#include "ksMidiFileTimeCalls.h"
#include "ksMidiFileEventCalls.h"
#include "ksMonoArpCalls.h"
#include "ksEditAddressCalls.h"
#include "ksEditSceneCalls.h"
#include "ksEditDecisionCalls.h"
#include "ksMidiFileTransportCalls.h"
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
 uint8_t read(uint32_t a)override{uint8_t value=this->state->memory[a];if(a==0x400004||a==0x600004)value=7;else if(a==0xffff8c)value|=128;else if(a==0x40000a||a==0x60000a)value=pitch;else if(a==0x40000c||a==0x60000c)value=middle;else if(a==0x40000e||a==0x60000e)value=low;events.emplace_back(false,a,value,this->state->cycles,uint32_t(this->state->getPC()));return value;}
 void write(uint32_t a,uint8_t value)override{events.emplace_back(true,a,value,this->state->cycles,uint32_t(this->state->getPC()));this->state->memory[a]=value;}
};
using OriginalBus=MockBus<H8SDevice>;using NativeBus=MockBus<ks::ControllerBus>;
void same(const Original& a,const ks::ControllerCpu& b,OriginalBus& x,const NativeBus& y,bool clockOnly){
    check(a.getPC()==b.getPC(),"key-priority call PC projection differs");
    check(a.cycles==b.cycles,"key-priority call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"key-priority call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("key-priority call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"key-priority call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"key-priority call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==std::get<4>(e);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original key-priority fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original key-priority fetch not paired");
        const auto entry=std::get<1>(*first);lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
        const unsigned bytes=unsigned(lengthOracle->getPC())-entry;
        check(bytes>=2 && bytes<=10,"Invalid reference fetch length");
        for(unsigned i=0;i<bytes;++i)check(first+i!=x.events.end() && !std::get<0>(first[i]) && std::get<1>(first[i])==std::get<1>(*first)+i,"Original key-priority fetch shape differs");
        x.events.erase(first,first+bytes);
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("key-priority call data/stack/value/order/clock/in-flight context differs");
    }
}
}
int main(int argc,char** argv){try{
 if(argc!=2){std::cout<<"SKIP: private verified KS OS required\n";return 77;}
 check(!ks::controllerReferenceAvailable(),"Key priority fixture linked recovery factory");ks::Rom os(argv[1]);check(os.isValid(),"Invalid OS");
 auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();lengthOracle=std::make_unique<H8SDisassembler>();lengthOracle->loadmem(os.data().data(),unsigned(os.data().size()),0);g_dasm=false;
 for(unsigned base:{0u,0xa00000u}){a->loadmem(os.data().data(),unsigned(os.data().size()),base);b->loadmem(os.data().data(),unsigned(os.data().size()),base);}
 ks::LiveMidiController shared(*b);ks::RuntimeController runtime(*b,shared);OriginalBus x;NativeBus y;x.events.reserve(64);y.events.reserve(64);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->interpreterAllowed=false;
 for(auto* m:{a->memory,b->memory}){m[112]=m[113]=0;m[114]=1;m[115]=0x70;}
 std::mt19937 rng(0x4b53524b);uint64_t cases=0,steps=0,irqCases=0,resets=0,nestedCases=0;std::array<unsigned,0xb900> first{};
 uint64_t seams=0;auto verify=[&](unsigned kind,unsigned mode,unsigned bank,unsigned row,unsigned sub,unsigned knob,unsigned oldKnob,unsigned delta,unsigned value,unsigned maximum,unsigned flags,bool clockOnly,bool odd,unsigned interruptAt,bool mutate=false,bool nested=false){
 shared.editDecisionCalls.discardCalls();shared.editSceneCalls.discardCalls();shared.editAddressCalls.discardCalls();shared.midiFileTransportCalls.discardCalls();shared.monoArpCalls.discardCalls();shared.midiFileEventCalls.discardCalls();shared.midiFileTimeCalls.discardCalls();shared.sysExDumpCalls.discardCalls();shared.legacySysExCalls.discardCalls();shared.sysExActionCalls.discardCalls();shared.patchExpandCalls.discardCalls();shared.sysExFrameCalls.discardCalls();shared.internalNoteCalls.discardCalls();shared.internalMidiCalls.discardCalls();shared.arpSelectCalls.discardCalls();shared.arpEventCalls.discardCalls();shared.arpMainCalls.discardCalls();shared.arpScheduleCalls.discardCalls();shared.arpPrimitiveCalls.discardCalls();shared.midiRouteCalls.discardCalls();shared.midiAuxCalls.discardCalls();shared.midiControlCalls.discardCalls();shared.noteActions.pedalNativeCalls.discardCalls();shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();shared.resetMetrics();shared.editDecisionCalls.resetMetrics();for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());unsigned stack=odd?0xfff001u:0xa18000u,entry=std::array<unsigned,7>{0xb554,0xb594,0xb5e4,0xb60c,0xb632,0xb716,0xb83c}[kind];a->setSP(stack);b->setSP(stack);a->regs[0].rh=b->regs[0].rh=int8_t(knob);a->regs[6].rh=b->regs[6].rh=int8_t(knob);a->regs[6].rl=b->regs[6].rl=int8_t(delta);
 for(auto*m:{a->memory,b->memory}){std::fill(m+0x15000,m+0x40000,0);for(unsigned i=0;i<256;++i)m[0x33000+i]=0;for(unsigned i=0;i<0x4000;++i)m[0x85ec00+i]=0;m[0x1679d]=uint8_t(oldKnob);m[0x1679e]=uint8_t(bank);m[0x1679f]=uint8_t(mode);m[0x167a0]=0;m[0x17492]=uint8_t(row);m[0x17493]=uint8_t(sub);m[0x1748e]=0;m[0x1748f]=3;m[0x17490]=0x30;m[0x17491]=0;const auto offset=uint8_t(row-1)*8u+uint8_t(sub*2u)+1u;m[0x33000+16+offset]=uint8_t(value);m[0x1510d+offset]=uint8_t(maximum);m[0x167bc]=0;m[0x167bd]=1;m[0x167be]=0x68;m[0x167bf]=0xfc;m[0x168fc+239]=uint8_t(flags&1);m[0x174a6]=1;m[0x17528]=1;m[112]=m[113]=0;m[114]=1;m[115]=0x70;m[0xffed6c]=m[0xffed6d]=m[0xffed6e]=m[0xffed6f]=0;m[0x400004]=m[0x600004]=7;m[stack]=m[stack+1]=0;m[stack+2]=0x43;m[stack+3]=0x38;}
 a->pc=a->makepc(entry);b->pc=b->makepc(entry);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=0;a->cycles=b->cycles=1000;a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;b->nativeFetchClocksOnly=clockOnly;
 unsigned countSteps=0;bool injected=false,done=false,seam=false;auto step=[&]{unsigned before=unsigned(a->getPC());x.events.clear();y.events.clear();try{runtime.step(*b);done|=shared.editDecisionCalls.completed>0||shared.editDecisionCalls.handedOff>0;a->step();same(*a,*b,x,y,clockOnly);}catch(const std::exception&){std::cerr<<"before="<<std::hex<<before<<std::dec<<" kind="<<kind<<" mode="<<mode<<" bank="<<bank<<" row="<<row<<" sub="<<sub<<" knob="<<knob<<" old="<<oldKnob<<" delta="<<delta<<" value="<<value<<'\n';throw;}if(before<first.size()&&!first[before])first[before]=countSteps+1;++steps;};do{if(a->getPC()==0x668c){seam=true;break;}if(countSteps==interruptAt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}if(countSteps%17==5){shared.resetMetrics();shared.editDecisionCalls.resetMetrics();++resets;}if(a->getPC()==0x1124)for(auto*m:{a->memory,b->memory}){m[0xffed6e]=m[0xffed6c];m[0xffed6f]=m[0xffed6d];}if(mutate&&a->getPC()==0xb5cc)for(auto*m:{a->memory,b->memory})m[0x1679f]^=1;step();check(++countSteps<5000000,"Native edit decision stalled");
 if(nested&&countSteps==5){std::array<int32_t,8>av{},bv{};for(unsigned i=0;i<8;++i){av[i]=a->regs[i].er;bv[i]=b->regs[i].er;}auto ap=a->pc;auto bp=b->pc;auto ac=a->ccr,bc=b->ccr;int ai=a->indent,bi=b->indent;unsigned child=stack-256;a->setSP(child);b->setSP(child);for(auto*m:{a->memory,b->memory}){m[child]=m[child+1]=0;m[child+2]=0x43;m[child+3]=0x38;}a->pc=a->makepc(0xb5e4);b->pc=b->makepc(0xb5e4);unsigned c=0;do{step();check(++c<10000,"Nested edit decision stalled");}while(a->getPC()!=0x4338);for(unsigned i=0;i<8;++i){a->regs[i].er=av[i];b->regs[i].er=bv[i];}a->pc=ap;b->pc=bp;a->ccr=ac;b->ccr=bc;a->indent=ai;b->indent=bi;++nestedCases;}
 }while(a->getPC()!=0x4338);check(!b->interpretedSteps&&!shared.editDecisionCalls.partialEntries,"Edit decision used recovery");check(done||seam,"Native edit decision did not terminate");check(std::memcmp(a->memory+0x15000,b->memory+0x15000,0x40000-0x15000)==0,"Edit decision memory differs");++cases;if(injected)++irqCases;if(seam)++seams;};
 for(bool clockOnly:{false,true}){for(unsigned kind=0;kind<7;++kind)for(unsigned flags=0;flags<256;++flags)verify(kind,flags,flags,flags,flags,flags,uint8_t(flags+7),flags,flags,127,flags,clockOnly,flags&1,unsigned(-1));for(unsigned knob=0;knob<256;++knob)for(unsigned oldKnob:{0u,3u,127u,255u})verify(0,0,1,0,0,knob,oldKnob,0,0,127,0xd0,clockOnly,knob&1,unsigned(-1));for(unsigned value=0;value<256;++value)for(unsigned delta=0;delta<256;++delta)verify(5,0,1,1,0,0,0,delta,value,127,0xd0,clockOnly,delta&1,unsigned(-1));for(unsigned kind=0;kind<7;++kind){for(unsigned mode:{0u,1u,2u,3u}){first.fill(0);verify(kind,mode,1,1,0,31,0,1,63,127,0xd1,clockOnly,false,unsigned(-1));auto selected=first;for(unsigned pc=0;pc<selected.size();++pc)if(selected[pc]&&ks::EditDecisionCalls::candidate(pc))verify(kind,mode,1,1,0,31,0,1,63,127,0xe7,clockOnly,true,selected[pc]-1);}verify(kind,0,1,1,0,31,0,1,63,127,0xd1,clockOnly,true,unsigned(-1),true);verify(kind,0,1,1,0,31,0,1,63,127,0xd1,clockOnly,true,unsigned(-1),false,true);}}
 check(std::memcmp(a->memory,b->memory,1<<24)==0,"Final edit decision memory differs");std::cout<<"PASS: "<<cases<<" actual native knob/edit decision calls, "<<steps<<" independent original steps; exact accesses/clocks/state/memory; "<<irqCases<<" IRQ points, "<<resets<<" metric resets, "<<nestedCases<<" nested, "<<seams<<" exact flash-commit seams; zero interpreter/recovery\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
