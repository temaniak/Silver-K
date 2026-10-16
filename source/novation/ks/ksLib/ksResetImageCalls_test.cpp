#include "ksRuntimeController.h"
#include "ksRuntimeBoundary.h"
#include "ksResetImageCalls.h"
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
    check(a.getPC()==b.getPC(),"reset-image call PC projection differs");
    check(a.cycles==b.cycles,"reset-image call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"reset-image call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("reset-image call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"reset-image call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"reset-image call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==(std::get<4>(e)&0xffffff);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original reset-image fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original reset-image fetch not paired");
        const auto entry=std::get<1>(*first);lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
        const unsigned bytes=entry>=0xffec00?((entry==0xffec04)?2u:4u):unsigned(lengthOracle->getPC())-entry;
        check(bytes>=2 && bytes<=10,"Invalid reference fetch length");
        for(unsigned i=0;i<bytes;++i)check(first+i!=x.events.end() && !std::get<0>(first[i]) && std::get<1>(first[i])==std::get<1>(*first)+i,"Original reset-image fetch shape differs");
        x.events.erase(first,first+bytes);
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("reset-image call data/stack/value/order/clock/in-flight context differs");
    }
}
}
int main(int argc,char**argv){try{
 if(argc!=2)return 77;check(!ks::controllerReferenceAvailable(),"Reset fixture linked recovery");ks::Rom os(argv[1]);check(os.isValid(),"Invalid OS");auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();lengthOracle=std::make_unique<H8SDisassembler>();lengthOracle->loadmem(os.data().data(),unsigned(os.data().size()),0);g_dasm=false;
 for(unsigned base:{0u,0xa00000u}){a->loadmem(os.data().data(),unsigned(os.data().size()),base);b->loadmem(os.data().data(),unsigned(os.data().size()),base);}ks::LiveMidiController shared(*b);ks::RuntimeController runtime(*b,shared,ks::RuntimeController::Entry::Reset);OriginalBus x;NativeBus y;x.events.reserve(64);y.events.reserve(64);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->interpreterAllowed=false;std::mt19937 rng(0x52535449);uint64_t cases=0,steps=0,seams=0,resets=0,irqs=0;std::array<unsigned,0x500> first{};
 auto verify=[&](unsigned entry,unsigned scenario,unsigned seed,unsigned flags,bool clockOnly,unsigned irqAt){
 shared.resetImageCalls.discardCalls();shared.noteActions.discardTransientCalls();shared.voiceActions.discardTransientCalls();shared.resetMetrics();for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());
 const auto word=[&](unsigned i){return uint16_t(seed*193+i*149);};uint32_t sum=0;for(unsigned i=0;i<32;++i)sum+=word(i);
 for(auto*m:{a->memory,b->memory}){m[112]=m[113]=0;m[114]=1;m[115]=0x70;for(unsigned i=0;i<40;++i)m[0x804190+i]=m[0x190+i];if(scenario==1)m[0x804190+17]^=1;auto store=[&](unsigned at,uint32_t v){for(unsigned j=0;j<4;++j)m[at+j]=uint8_t(v>>(24-j*8));};for(unsigned i=0;i<32;++i){auto v=word(i);m[0x804000+i*2]=uint8_t(v>>8);m[0x804001+i*2]=uint8_t(v);}store(0x804184,scenario==2?0:64);store(0x804040,sum+(scenario==3));m[0xffec33]=(scenario==0||scenario==5)?0x55:0;m[0xffff6f]=uint8_t(seed);std::copy_n(m+0x422,14,m+0xffec00);for(unsigned i=0;i<16384;++i)m[0x800000+i]=uint8_t(seed+i*7);}
 a->setSP(0xa18000);b->setSP(0xa18000);a->pc=a->makepc(entry);b->pc=b->makepc(entry);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=0;a->cycles=b->cycles=1000;a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;b->nativeFetchClocksOnly=clockOnly;
 unsigned count=0;bool injected=false;while(unsigned(a->getPC())!=0x1306&&unsigned(a->getPC())!=0x3fe&&unsigned(a->getPC())!=0x454){
  const unsigned before=unsigned(a->getPC());if(count==irqAt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}
  if(count%37==5){shared.resetMetrics();++resets;}
  x.events.clear();y.events.clear();try{runtime.stepStartup(*b);a->step();same(*a,*b,x,y,clockOnly);}catch(const std::exception&){std::cerr<<"before="<<std::hex<<before<<" entry="<<entry<<std::dec<<" scenario="<<scenario<<" seed="<<seed<<" flags="<<flags<<" count="<<count<<'\n';throw;}
  if(before<first.size()&&!first[before])first[before]=count+1;++steps;++count;check(count<400000,"Reset stalled");
  // Explicit empty service seams isolate reset/image ownership. Full real
  // children and DSP/audio remain covered by ksControllerBoot_test.
  bool child=false;switch(before){case 0x306:case 0x30a:case 0x30e:case 0x32a:case 0x32e:case 0x332:case 0x336:case 0x342:case 0x3f0:case 0x3fa:case 0x436:case 0x446:case 0x450:child=true;break;default:break;}
  if(child&&unsigned(a->getPC())!=before){if(before==0x342)for(auto*m:{a->memory,b->memory})m[0xffec33]=(scenario==0||scenario==5)?0x55:0;
   a->pc=a->makepc(0x370c);b->pc=b->makepc(0x370c);x.events.clear();y.events.clear();ks::controllerDetail::RuntimeBoundary f(*b,0x370c,2);f.ret();b->pc=b->makepc(f.next);a->step();same(*a,*b,x,y,clockOnly);++seams;}
 }
 check(!b->interpretedSteps&&!shared.resetImageCalls.partialEntries,"Reset used recovery");check(std::memcmp(a->memory,b->memory,1<<24)==0,"Reset full memory differs");++cases;if(injected)++irqs;
 };
 for(bool clock:{false,true}){for(unsigned entry:{0x2ecu,0x180u,0xffffec00u})for(unsigned scenario=0;scenario<6;++scenario)for(unsigned seed=0;seed<256;seed+=17)verify(entry,scenario,seed,(seed*149)&255,clock,unsigned(-1));
 for(unsigned flags=0;flags<256;++flags)for(unsigned scenario:{0u,4u})verify(0x180,scenario,flags,flags,clock,unsigned(-1));
 for(unsigned scenario:{0u,1u,2u,3u,4u}){first.fill(0);verify(0x2ec,scenario,93,0xd0,clock,unsigned(-1));const auto selected=first;for(unsigned p=0;p<selected.size();++p)if(selected[p])verify(0x2ec,scenario,93,0xe7,clock,selected[p]-1);}}
 std::cout<<"PASS: "<<cases<<" native reset/image lifecycles / "<<steps<<" independent H8 boundaries / "<<seams<<" explicit empty child seams / "<<irqs<<" IRQ sites / "<<resets<<" metric resets; exact state/access/clocks/full memory, zero interpreter/recovery; full children covered separately by boot test\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
