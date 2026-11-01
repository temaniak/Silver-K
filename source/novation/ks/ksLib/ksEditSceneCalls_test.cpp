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
 auto verify=[&](unsigned kind,unsigned bank,unsigned previous,unsigned parameter,unsigned value,unsigned save,unsigned shape,unsigned flags,bool clockOnly,bool odd,unsigned interruptAt,bool mutate=false,bool nested=false){
 shared.editSceneCalls.discardCalls();shared.editAddressCalls.discardCalls();shared.midiFileTransportCalls.discardCalls();shared.monoArpCalls.discardCalls();shared.midiFileEventCalls.discardCalls();shared.midiFileTimeCalls.discardCalls();shared.sysExDumpCalls.discardCalls();shared.legacySysExCalls.discardCalls();shared.sysExActionCalls.discardCalls();shared.patchExpandCalls.discardCalls();shared.sysExFrameCalls.discardCalls();shared.internalNoteCalls.discardCalls();shared.internalMidiCalls.discardCalls();shared.arpSelectCalls.discardCalls();shared.arpEventCalls.discardCalls();shared.arpMainCalls.discardCalls();shared.arpScheduleCalls.discardCalls();shared.arpPrimitiveCalls.discardCalls();shared.midiRouteCalls.discardCalls();shared.midiAuxCalls.discardCalls();shared.midiControlCalls.discardCalls();shared.noteActions.pedalNativeCalls.discardCalls();shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();shared.resetMetrics();shared.editSceneCalls.resetMetrics();for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());unsigned stack=odd?0xfff001u:0xa18000u,entry=std::array<unsigned,5>{0xb786,0xb79e,0xb7de,0xb8ac,0xb8ca}[kind];uint32_t data=std::array<uint32_t,6>{0x33000,0x33001,0xa33000,0xa33001,0xff033000u,0xffa33001u}[shape%6],patch=std::array<uint32_t,6>{0x32000,0x32001,0xa32000,0xa32001,0xff032000u,0xffa32001u}[shape%6];a->setSP(stack);b->setSP(stack);a->regs[5].rl=b->regs[5].rl=int8_t(save);
 for(auto*m:{a->memory,b->memory}){std::fill(m+0x15000,m+0x40000,0);for(unsigned i=0;i<256;++i){m[(data+i)&0xffffff]=uint8_t(i*13+value);m[(patch+i)&0xffffff]=uint8_t(i*17+flags);}auto table=[&](uint32_t base){for(unsigned i=0;i<16;++i){m[(base+16+i*2)&0xffffff]=uint8_t(parameter+i*(shape&1?0:17));m[(base+17+i*2)&0xffffff]=uint8_t(value+(shape&2?i:0));}};table(data);for(unsigned selected:{bank,previous}){uint32_t at=0x85ec00u+uint8_t(selected-1)*64u,page=at&0xffffff00u;for(unsigned i=0;i<256;++i)m[page+i]=uint8_t(i*7+value);table(at);}for(unsigned i=0;i<16;++i)m[0x17495+i]=uint8_t(i*19+flags);for(unsigned i=0;i<64;++i)m[(0x1734e)+i]=uint8_t(i*23+save);m[0x1679e]=uint8_t(bank);m[0x167a0]=uint8_t(previous);for(unsigned i=0;i<4;++i){m[(0x1748e)+i]=uint8_t(data>>(24-i*8));m[0x167bc+i]=uint8_t(patch>>(24-i*8));}m[0x174a6]=1;m[0x17528]=1;m[(patch+239)&0xffffff]=0;m[112]=m[113]=0;m[114]=1;m[115]=0x70;m[0xffed6c]=m[0xffed6d]=m[0xffed6e]=m[0xffed6f]=0;m[0x400004]=m[0x600004]=7;m[stack]=m[stack+1]=0;m[stack+2]=0x43;m[stack+3]=0x38;}
 a->pc=a->makepc(entry);b->pc=b->makepc(entry);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=0;a->cycles=b->cycles=1000;a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;b->nativeFetchClocksOnly=clockOnly;
 unsigned countSteps=0;bool injected=false,done=false;auto step=[&]{unsigned before=unsigned(a->getPC());x.events.clear();y.events.clear();try{runtime.step(*b);done|=shared.editSceneCalls.completed>0;a->step();same(*a,*b,x,y,clockOnly);}catch(const std::exception&){std::cerr<<"before="<<std::hex<<before<<std::dec<<" kind="<<kind<<" bank="<<bank<<" previous="<<previous<<" parameter="<<parameter<<" value="<<value<<" save="<<save<<" shape="<<shape<<'\n';throw;}if(before<first.size()&&!first[before])first[before]=countSteps+1;++steps;};do{if(countSteps==interruptAt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}if(countSteps%17==5){shared.resetMetrics();shared.editSceneCalls.resetMetrics();++resets;}if(a->getPC()==0x1124)for(auto*m:{a->memory,b->memory}){m[0xffed6e]=m[0xffed6c];m[0xffed6f]=m[0xffed6d];}if(mutate&&a->getPC()==0xb7c8)for(auto*m:{a->memory,b->memory})m[(patch+parameter)&0xffffff]^=127;step();check(++countSteps<5000000,"Native edit scene stalled");
 if(nested&&countSteps==5){std::array<int32_t,8>av{},bv{};for(unsigned i=0;i<8;++i){av[i]=a->regs[i].er;bv[i]=b->regs[i].er;}auto ap=a->pc;auto bp=b->pc;auto ac=a->ccr,bc=b->ccr;int ai=a->indent,bi=b->indent;unsigned child=stack-256;a->setSP(child);b->setSP(child);for(auto*m:{a->memory,b->memory}){m[child]=m[child+1]=0;m[child+2]=0x43;m[child+3]=0x38;}a->pc=a->makepc(0xb6fe);b->pc=b->makepc(0xb6fe);unsigned c=0;do{step();check(++c<100,"Nested edit scene stalled");}while(a->getPC()!=0x4338);for(unsigned i=0;i<8;++i){a->regs[i].er=av[i];b->regs[i].er=bv[i];}a->pc=ap;b->pc=bp;a->ccr=ac;b->ccr=bc;a->indent=ai;b->indent=bi;++nestedCases;}
 }while(a->getPC()!=0x4338);check(!b->interpretedSteps&&!shared.editSceneCalls.partialEntries,"Edit scene used recovery");check(done,"Native edit scene did not complete");check(std::memcmp(a->memory+0x15000,b->memory+0x15000,0x40000-0x15000)==0,"Edit scene memory differs");++cases;if(injected)++irqCases;};
 for(bool clockOnly:{false,true}){for(unsigned kind=0;kind<5;++kind)for(unsigned flags=0;flags<256;++flags)verify(kind,flags,flags,flags,flags,flags,flags%6,flags,clockOnly,flags&1,unsigned(-1));for(unsigned parameter=0;parameter<256;++parameter)for(unsigned value=0;value<256;++value)verify(1,1,1,parameter,value,value&1,parameter%6,0xd0,clockOnly,value&1,unsigned(-1));for(unsigned kind=0;kind<5;++kind){first.fill(0);verify(kind,3,2,17,127,1,0,0xd0,clockOnly,false,unsigned(-1));auto selected=first;for(unsigned pc=0;pc<selected.size();++pc)if(selected[pc]&&ks::EditSceneCalls::candidate(pc))verify(kind,3,2,17,127,1,5,0xe7,clockOnly,true,selected[pc]-1);verify(kind,3,2,17,127,1,5,0xd0,clockOnly,true,unsigned(-1),true);verify(kind,3,2,17,127,1,5,0xd0,clockOnly,true,unsigned(-1),false,true);}}
 check(std::memcmp(a->memory,b->memory,1<<24)==0,"Final edit scene memory differs");std::cout<<"PASS: "<<cases<<" actual native edit scene/apply/restore/snapshot-copy calls, "<<steps<<" independent original steps; exact accesses/clocks/state/memory; "<<irqCases<<" IRQ points, "<<resets<<" metric resets, "<<nestedCases<<" nested; zero interpreter/recovery\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
