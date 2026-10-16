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
    check(a.getPC()==b.getPC(),"voice-release call PC projection differs");
    check(a.cycles==b.cycles,"voice-release call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"voice-release call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("voice-release call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"voice-release call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"voice-release call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==std::get<4>(e);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original voice-release fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original voice-release fetch not paired");
        const auto entry=std::get<1>(*first);lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
        const unsigned bytes=unsigned(lengthOracle->getPC())-entry;
        check(bytes>=2 && bytes<=10,"Invalid reference fetch length");
        for(unsigned i=0;i<bytes;++i)check(first+i!=x.events.end() && !std::get<0>(first[i]) && std::get<1>(first[i])==std::get<1>(*first)+i,"Original voice-release fetch shape differs");
        x.events.erase(first,first+bytes);
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("voice-release call data/stack/value/order/clock/in-flight context differs");
    }
}
}
int main(int argc,char** argv){try{
 if(argc!=2){std::cout<<"SKIP: private verified KS OS required\n";return 77;}
 check(!ks::controllerReferenceAvailable(),"Release fixture linked recovery factory");ks::Rom os(argv[1]);check(os.isValid(),"Invalid OS");
 auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();lengthOracle=std::make_unique<H8SDisassembler>();lengthOracle->loadmem(os.data().data(),unsigned(os.data().size()),0);g_dasm=false;
 for(unsigned base:{0u,0xa00000u}){a->loadmem(os.data().data(),unsigned(os.data().size()),base);b->loadmem(os.data().data(),unsigned(os.data().size()),base);}
 ks::LiveMidiController shared(*b);ks::RuntimeController runtime(*b,shared);OriginalBus x;NativeBus y;x.events.reserve(64);y.events.reserve(64);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->interpreterAllowed=false;
 for(auto* m:{a->memory,b->memory}){m[112]=m[113]=0;m[114]=1;m[115]=0x70;}
 std::mt19937 rng(0x4b535652);uint64_t cases=0,steps=0,irqCases=0,resets=0,nestedCases=0;std::array<unsigned,0x9500> first{};
 auto store32=[](uint8_t* m,unsigned at,uint32_t value){for(unsigned i=0;i<4;++i)m[at+i]=uint8_t(value>>(24-8*i));};
 auto verify=[&](unsigned entry,unsigned voices,unsigned shape,unsigned flags,bool clockOnly,bool odd,unsigned interruptAt,bool mutate=false,bool nested=false,unsigned part=2,unsigned note=60,bool tagged=false,uint16_t cut=0xffff){
  shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();shared.resetMetrics();for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());
  a->regs[5].rl=b->regs[5].rl=int8_t(note);a->regs[2].r=b->regs[2].r=int16_t(cut);
  const unsigned stack=odd?0xfff001u:0xa18000u;
  a->setSP(stack);b->setSP(stack);for(auto* m:{a->memory,b->memory}){
   m[0x400004]=m[0x600004]=7;m[0x17526]=uint8_t(part);m[0x174a6]=uint8_t(voices);m[0x174a7]=0xee;
   for(unsigned p=0;p<4;++p){m[0x160d2 + p*334+309]=uint8_t(shape==5?16:0);for(unsigned at=0;at<16;++at)m[0x160d2 + p*334+0xc0 + at]=0xa5;}
   for(unsigned voice=0;voice<256;++voice){const unsigned base=0x17560 + voice*20,patch=0x30000 + voice*256;
    const bool mixed=shape==8;store32(m,base,(tagged?0xffa00000u:0)+patch);
    store32(m,base+8,shape==2||(mixed&&voice%5==0)?0:0x80000000u+voice*0x10000u+0x1234u);
    m[base+4]=uint8_t(shape==1||(mixed&&voice%5==1)?part+1:part);m[base+5]=uint8_t(note+(shape==3||(mixed&&voice%5==2)?1:0));
    m[base+17]=uint8_t(shape==6||(mixed&&voice%5==3)?0:16);m[base+18]=uint8_t(shape==4||(mixed&&voice%5==4)?1:0);
    m[(tagged?0xa00000:0)+patch+88]=uint8_t(shape==7?4:0);
   }
   m[stack]=m[stack+1]=0;m[stack+2]=0x43;m[stack+3]=0x38;
  }
  a->pc=a->makepc(entry);b->pc=b->makepc(entry);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=0;a->cycles=b->cycles=1000;
  a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;
  for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;b->nativeFetchClocksOnly=clockOnly;
  unsigned count=0;bool injected=false;
  auto step=[&]{const unsigned before=unsigned(a->getPC());x.events.clear();y.events.clear();try{runtime.step(*b);a->step();same(*a,*b,x,y,clockOnly);}catch(const std::exception&){std::cerr<<"before="<<std::hex<<before<<" entry="<<entry<<std::dec<<" voices="<<voices<<" shape="<<shape<<" part="<<part<<" flags="<<flags<<" clock="<<clockOnly<<" odd="<<odd<<'\n';throw;}
   if(before<first.size()&&!first[before])first[before]=count+1;++steps;};
  do{
   if(count==interruptAt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}
   if(count%97==5){shared.resetMetrics();++resets;}
   if(mutate)for(auto* m:{a->memory,b->memory}){
    m[0x17526]=uint8_t(count%4);m[0x174a7]=uint8_t(count%voices);
    for(unsigned p=0;p<4;++p)m[0x160d2 + p*334+309]=uint8_t((count+p)%3?16:0);
    for(unsigned v=0;v<voices;++v){const unsigned at=0x17560 + v*20;
     m[at+4]=uint8_t((v+count/11)%4);m[at+5]=uint8_t(note+(count%3==0));m[at+17]=uint8_t(count%7);m[at+18]=uint8_t(count%5==0);
     store32(m,at+8,uint32_t(count*797u+v*0x314159u));m[(tagged?0xa00000:0)+0x30000 + v*256+88]=uint8_t(count%3?0:4);
    }
   }
   step();check(++count<60000,"Native release stalled");
   if(nested&&count==5){
    std::array<int32_t,8> av{},bv{};for(unsigned i=0;i<8;++i){av[i]=a->regs[i].er;bv[i]=b->regs[i].er;}
    const auto ap=a->pc;const auto bp=b->pc;const auto ac=a->ccr,bc=b->ccr;const int ai=a->indent,bi=b->indent;const unsigned child=stack-256;
    a->setSP(child);b->setSP(child);for(auto* m:{a->memory,b->memory}){m[child]=m[child+1]=0;m[child+2]=0x43;m[child+3]=0x38;}
    a->pc=a->makepc(entry);b->pc=b->makepc(entry);unsigned childCount=0;
    do{step();check(++childCount<60000,"Nested release stalled");if(childCount==5){shared.resetMetrics();++resets;}}while(a->getPC()!=0x4338);
    for(unsigned i=0;i<8;++i){a->regs[i].er=av[i];b->regs[i].er=bv[i];}a->pc=ap;b->pc=bp;a->ccr=ac;b->ccr=bc;a->indent=ai;b->indent=bi;++nestedCases;
   }
  }while(a->getPC()!=0x4338);
  check(!b->interpretedSteps&&!shared.noteActions.releaseCalls.partialEntries&&!shared.noteActions.keyResetCalls.partialEntries&&!shared.noteActions.partControlCalls.partialEntries&&!shared.voiceActions.selectionCalls.partialEntries,"Native release used recovery");
  check(shared.noteActions.releaseCalls.completed>=1,"Native release did not complete");check(std::memcmp(a->memory+0x160d2,b->memory+0x160d2,0x40000-0x160d2)==0,"Release memory differs");++cases;if(injected)++irqCases;
 };
 for(bool clockOnly:{false,true})for(unsigned entry:{0x92c0u,0x9374u,0x936eu,0x9400u,0x9376u}){
  for(unsigned shape=0;shape<9;++shape)for(unsigned voices:{1u,14u,256u})for(unsigned flags:{0u,0x10u,0x40u,0x50u,0x80u,0x90u,0xc0u,0xd0u})verify(entry,voices,shape,flags,clockOnly,bool(flags&16),unsigned(-1),false,false,2,60,bool(shape&1));
  for(unsigned part=0;part<4;++part)for(unsigned flags=0;flags<256;++flags)verify(entry,14,flags/32,flags,clockOnly,bool(flags&1),unsigned(-1),false,false,part,flags,bool(flags&2),uint16_t(flags*257));
  for(unsigned shape:{0u,5u,7u,8u}){
   first.fill(0);verify(entry,14,shape,0xd0,clockOnly,false,unsigned(-1));const auto selected=first;
   for(unsigned pc=0;pc<selected.size();++pc)if(selected[pc]&&ks::VoiceReleaseCalls::candidate(pc))verify(entry,14,shape,0xe7,clockOnly,true,selected[pc]-1);
   verify(entry,14,shape,0xd0,clockOnly,true,unsigned(-1),true);
  }
  verify(entry,14,0,0xd0,clockOnly,true,unsigned(-1),false,true);
 }
 check(std::memcmp(a->memory,b->memory,1<<24)==0,"Release final full memory differs");
 std::cout<<"PASS: "<<cases<<" actual native release calls, "<<steps<<" independent original steps; note/part/fast/provided-policy/pedal, active/owner/note/extra/hold/bypass/mixed cases, voice-count1/14/256, all flags/parts/notes, low/tagged bindings, both fetch modes, odd/even SRAM and live mutations; "<<irqCases<<" selected IRQ points, "<<resets<<" metric resets, "<<nestedCases<<" nested calls; exact accesses/clocks/state/memory; zero interpreter/recovery\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
