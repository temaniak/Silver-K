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
std::array<uint32_t,1024> expectedTables(){
    std::array<uint32_t,1024> values{};uint32_t value=0x40000000;
    for(unsigned n=0;n<512;++n){values[n]=value>>8;value+=uint32_t((uint64_t(0xb191)*uint16_t(value>>16))/512);}
    for(unsigned n=0;n<512;++n)values[512+n]=uint32_t((0x800000ull<<16)/(512+n))>>8;
    return values;
}
const auto tables=expectedTables();
struct Script {
    uint32_t source=0x20000,base=0x400000,words=2,seed=0;
    uint16_t header=0;
    unsigned headerReads=0,sourceReads=0,programWrites=0,tableWrites=0,addressBytes=0;
    unsigned programReadyReads=0,tableReadyReads=0,bootReadyReads=0,beforeAddressReads=0,afterAddressReads=0;
    unsigned programBusy=0,tableBusy=0,bootBusy=0,addressBusy=0;
    bool bootCleared=false,addressMode=false,addressReady=false,addressComplete=false;
    uint8_t latched=0;
};
template<class Device>struct MockBus final:Device {
    Script script;
    std::vector<Access> events;
    uint8_t read(uint32_t a)override{
        uint8_t value=this->state->memory[a];
        if(a==0x400004 || a==0x600004){
            check(a==script.base+4,"Wrong setup status port");
            if(script.programWrites<script.words*3)value=script.programReadyReads++<script.programBusy?0xfd:0xff;
            else if(!script.bootCleared){value=script.bootReadyReads++<script.bootBusy?0xff:0xef;if(!(value&0x10))script.bootCleared=true;}
            else if(!script.addressMode && !script.addressComplete)value=script.beforeAddressReads++<script.addressBusy?0xfb:0xff;
            else if(script.addressMode){check(script.addressBytes==3,"Address status observed before request complete");value=script.afterAddressReads++<script.addressBusy?0xfb:0xff;if(value&4)script.addressReady=true;}
            else value=script.tableReadyReads++<script.tableBusy?0xfd:0xff;
        }else if(a>=script.source && a<script.source+script.words*3){
            if(script.headerReads<2){const unsigned offset=++script.headerReads;check(a==script.source+offset,"Setup header reads reordered");value=uint8_t(offset==1?script.header>>8:script.header);}
            else{
                check(a==script.source+script.sourceReads && script.sourceReads==script.programWrites,"Setup firmware source reads reordered");
                const auto offset=script.sourceReads++;value=offset==1?uint8_t(script.header>>8):offset==2?uint8_t(script.header):uint8_t(script.seed+offset*37+(offset>>8));script.latched=value;
            }
        }
        events.emplace_back(false,a,value,this->state->cycles,uint32_t(this->state->getPC()));return value;
    }
    void write(uint32_t a,uint8_t value)override{
        if((a>=0x400000 && a<0x400010)||(a>=0x600000 && a<0x600010)){
            if(script.programWrites<script.words*3){
                check(a==script.base+10+2*(script.programWrites%3) && script.sourceReads==script.programWrites+1 && value==script.latched,"Setup firmware stream output differs");if(!(++script.programWrites%3))script.programReadyReads=0;
            }else if(a==script.base){
                if(!script.addressMode && !script.addressComplete){check(script.bootCleared && value==8,"Setup HF3 assertion differs");script.addressMode=true;}
                else{check(script.addressMode && script.addressBytes==3 && script.addressReady && value==0,"Setup HF3 release differs");script.addressMode=false;script.addressComplete=true;}
            }else if(script.addressMode){
                constexpr uint8_t bytes[]{0,0x86,0};check(script.addressBytes<3 && a==script.base+10+2*script.addressBytes && value==bytes[script.addressBytes],"Setup table address bytes differ");++script.addressBytes;
            }else{
                check(script.addressComplete && script.tableWrites<3072,"Setup tuning write before address/after end");const unsigned index=script.tableWrites/3,byte=script.tableWrites%3;
                check(a==script.base+10+2*byte && value==uint8_t(tables[index]>>(16-8*byte)),"Setup generated tuning value/order differs");if(!(++script.tableWrites%3))script.tableReadyReads=0;
            }
        }
        events.emplace_back(true,a,value,this->state->cycles,uint32_t(this->state->getPC()));this->state->memory[a]=value;
    }
};
using OriginalBus=MockBus<H8SDevice>;
using NativeBus=MockBus<ks::ControllerBus>;
void same(const Original& a,const ks::ControllerCpu& b,OriginalBus& x,const NativeBus& y,bool clockOnly){
    check(a.getPC()==b.getPC(),"DSP-setup call PC projection differs");
    check(a.cycles==b.cycles,"DSP-setup call clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"DSP-setup call flags projection differs");
    for(unsigned i=0;i<8;++i)if(a.regs[i].er!=b.regs[i].er){std::cerr<<"register="<<i<<" original="<<std::hex<<a.regs[i].er<<" native="<<b.regs[i].er<<std::dec<<'\n';throw std::runtime_error("DSP-setup call register projection differs");}
    check(a.lastread==b.lastread && a.lastwrite==b.lastwrite && a.indent==b.indent,"DSP-setup call bus/return bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"DSP-setup call IRQ pending differs");
    if(clockOnly){
        auto first=std::find_if(x.events.begin(),x.events.end(),[](const Access& e){return !std::get<0>(e) && !(std::get<4>(e)&1) && std::get<1>(e)==std::get<4>(e);});
        check(first!=x.events.end() && first+1!=x.events.end(),"Original DSP-setup fetch absent");
        check(!std::get<0>(first[1]) && std::get<1>(first[1])==std::get<1>(*first)+1,"Original DSP-setup fetch not paired");
        const auto entry=std::get<1>(*first);lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
        const unsigned bytes=unsigned(lengthOracle->getPC())-entry;
        check(bytes>=2 && bytes<=10,"Invalid reference fetch length");
        for(unsigned i=0;i<bytes;++i)check(first+i!=x.events.end() && !std::get<0>(first[i]) && std::get<1>(first[i])==std::get<1>(*first)+i,"Original DSP-setup fetch shape differs");
        x.events.erase(first,first+bytes);
    }
    if(x.events!=y.events){
        for(size_t i=0;i<std::min(x.events.size(),y.events.size());++i)if(x.events[i]!=y.events[i]){
            auto show=[](const Access& e){std::cerr<<std::get<0>(e)<<':'<<std::hex<<std::get<1>(e)<<':'<<unsigned(std::get<2>(e))<<':'<<std::get<3>(e)<<':'<<std::get<4>(e)<<std::dec;};
            std::cerr<<"access="<<i<<" original=";show(x.events[i]);std::cerr<<" native=";show(y.events[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("DSP-setup call data/stack/value/order/clock/in-flight context differs");
    }
}
}
int main(int argc,char** argv){try{
    if(argc!=2){std::cout<<"SKIP: private verified KS OS required\n";return 77;}
    check(!ks::controllerReferenceAvailable(),"Setup fixture linked synchronized H8/recovery adapter");
    ks::Rom os(argv[1]);check(os.isValid(),"Invalid KS OS");
    auto a=std::make_unique<Original>();auto b=std::make_unique<ks::ControllerCpu>();
    lengthOracle=std::make_unique<H8SDisassembler>();lengthOracle->loadmem(os.data().data(),unsigned(os.data().size()),0);g_dasm=false;
    for(unsigned address:{0u,0xa00000u}){a->loadmem(os.data().data(),unsigned(os.data().size()),address);b->loadmem(os.data().data(),unsigned(os.data().size()),address);}
    ks::LiveMidiController shared(*b);ks::RuntimeController controller(*b,shared);
    for(auto* memory:{a->memory,b->memory}){memory[28*4]=memory[28*4+1]=0;memory[28*4+2]=1;memory[28*4+3]=0x70;}
    OriginalBus x;NativeBus y;x.events.reserve(64);y.events.reserve(64);a->memmap(&x,0,1<<24);b->memmap(&y,0,1<<24);b->interpreterAllowed=false;
    std::mt19937 rng(0x4b534249);uint64_t cases=0,steps=0,irqCases=0,resets=0,faults=0,nestedCalls=0,nestedSteps=0;unsigned maximumLive=1;
    constexpr unsigned returned=0x4338;std::vector<unsigned> interruptPoints;
    struct Context {std::array<int32_t,8> values{};uint32_t pc;uint8_t ccr,exr;int indent;uint64_t pending,high;};
    auto capture=[](auto& cpu){Context v;for(unsigned i=0;i<8;++i)v.values[i]=cpu.regs[i].er;v.pc=uint32_t(cpu.getPC());v.ccr=cpu.ccr;v.exr=cpu.exr;v.indent=cpu.indent;v.pending=cpu.pending_irqs;v.high=cpu.pending_irqs_high;return v;};
    auto restore=[](auto& cpu,const Context& v){for(unsigned i=0;i<8;++i)cpu.regs[i].er=v.values[i];cpu.pc=cpu.makepc(v.pc);cpu.ccr=v.ccr;cpu.exr=v.exr;cpu.indent=v.indent;cpu.pending_irqs=v.pending;cpu.pending_irqs_high=v.high;};
    auto setReturn=[&](unsigned stack){for(auto* memory:{a->memory,b->memory}){memory[stack]=memory[stack+1]=0;memory[stack+2]=uint8_t(returned>>8);memory[stack+3]=uint8_t(returned);}};
    auto verify=[&](uint16_t header,unsigned flags,bool clockOnly,bool odd,unsigned port,uint32_t source,unsigned bootBusy,unsigned programBusy,unsigned addressBusy,unsigned tableBusy,unsigned preempt,bool interleave=false,bool collect=false){
        shared.bootSetupCalls.discardCalls();shared.bootStreamCalls.discardCalls();shared.faultCalls.discardCalls();shared.voiceActions.discardTransientCalls();shared.resetMetrics();
        for(unsigned i=0;i<8;++i)a->regs[i].er=b->regs[i].er=int32_t(rng());
        a->regs[4].er=b->regs[4].er=int32_t(source);a->regs[5].er=b->regs[5].er=int32_t(port);
        const unsigned stack=odd?0xfff001:0xa18000;a->setSP(stack);b->setSP(stack);setReturn(stack);
        a->pc=a->makepc(0x2332);b->pc=b->makepc(0x2332);a->ccr=b->ccr=uint8_t(flags);a->exr=b->exr=0;a->cycles=b->cycles=1000;
        a->lastread=b->lastread=int(rng());a->lastwrite=b->lastwrite=int(rng());a->indent=b->indent=0;
        a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;a->interruptMode2=b->interruptMode2=true;
        for(unsigned i=0;i<128;++i)a->interruptPriority[i]=b->interruptPriority[i]=0;a->interruptPriority[28]=b->interruptPriority[28]=6;
        b->nativeFetchClocksOnly=clockOnly;
        for(auto* memory:{a->memory,b->memory}){memory[0xffec22]=0xa5;memory[0xffff61]=0xa5;memory[0xffff62]=0x5a;}
        Script script;script.source=source&0xffffff;script.header=header;const unsigned encoded=uint16_t(header+2u);script.words=encoded?encoded:65536;script.base=port;script.seed=flags;script.bootBusy=bootBusy;script.programBusy=programBusy;script.addressBusy=addressBusy;script.tableBusy=tableBusy;x.script=y.script=script;
        auto step=[&]{x.events.clear();y.events.clear();controller.step(*b);a->step();same(*a,*b,x,y,clockOnly);};
        auto completed=[&]{check(x.script.sourceReads==script.words*3 && y.script.sourceReads==x.script.sourceReads && x.script.programWrites==script.words*3 && y.script.programWrites==x.script.programWrites && x.script.tableWrites==3072 && y.script.tableWrites==3072 && x.script.addressComplete && y.script.addressComplete,"Setup source/table completion differs");};
        auto nested=[&](auto&& self,unsigned depth)->void{
            const auto original=capture(*a),native=capture(*b);const auto sx=x.script,sy=y.script;
            const unsigned childStack=a->getSP()-64;a->setSP(childStack);b->setSP(childStack);setReturn(childStack);
            a->regs[4].er=b->regs[4].er=int32_t(depth&1?0xffa40001u:0x20000u);a->regs[5].er=b->regs[5].er=int32_t(depth&1?0x600000:0x400000);
            a->pc=a->makepc(0x2332);b->pc=b->makepc(0x2332);a->exr=b->exr=uint8_t(depth);a->indent=b->indent=int(depth);a->pending_irqs=b->pending_irqs=a->pending_irqs_high=b->pending_irqs_high=0;
            Script child;child.source=depth&1?0xa40001:0x20000;child.base=depth&1?0x600000:0x400000;child.header=0;child.words=2;child.seed=depth*37;child.bootBusy=depth%3;x.script=y.script=child;
            maximumLive=std::max(maximumLive,depth+1);unsigned count=0;
            do{step();++nestedSteps;check(++count<200000,"Nested setup call stalled");if(count==6){shared.resetMetrics();++resets;if(depth<7)self(self,depth+1);}}while(a->getPC()!=returned);
            check(x.script.tableWrites==3072 && y.script.tableWrites==3072 && x.script.sourceReads==6 && y.script.sourceReads==6,"Nested setup completion differs");++nestedCalls;
            // Explicit caller-context interleave, separate from actual IRQ/RTE.
            restore(*a,original);restore(*b,native);x.script=sx;y.script=sy;
        };
        std::array<bool,0x10000> seen{};unsigned count=0;bool injected=false;
        do{
            if(count==preempt){a->pending_irqs=b->pending_irqs=1ull<<28;injected=true;}
            if(count==5){shared.resetMetrics();++resets;}
            const unsigned before=unsigned(a->getPC());if(before==0x2304)++faults;
            if(collect && before<seen.size() && !seen[before]){seen[before]=true;interruptPoints.push_back(count);}
            try{step();}catch(const std::exception&){std::cerr<<"header="<<std::hex<<header<<" source="<<source<<" before="<<before<<std::dec<<" busy="<<bootBusy<<','<<programBusy<<','<<addressBusy<<','<<tableBusy<<" flags="<<flags<<" clock-only="<<clockOnly<<'\n';throw;}
            check(++count<4000000,"Actual native setup stalled");++steps;
            if(interleave && count==6)nested(nested,1);
        }while(a->getPC()!=returned);
        completed();check(!b->interpretedSteps && !shared.bootSetupCalls.partialEntries && !shared.bootStreamCalls.partialEntries && !shared.faultCalls.partialEntries && !shared.voiceActions.wordCalls.partialEntries && !shared.voiceActions.addressCalls.partialEntries,"Native setup used interpreter/test recovery");
        const uint64_t expectedFaults=bootBusy?uint64_t(bootBusy-1)/65536:0;
        check(shared.faultCalls.started==expectedFaults && shared.faultCalls.completed==expectedFaults,"Setup boot-ready timeout did not execute native fault service");
        if(!injected && !interleave){auto delay=[](unsigned n)->uint64_t{return n?24ull+uint64_t(n-1)*27+uint64_t((n-1)/65536)*26:0;};const uint64_t expected=1000+uint64_t(script.words)*96+367297+delay(bootBusy)+uint64_t(script.words)*delay(programBusy)+2*delay(addressBusy)+1024*delay(tableBusy)+176*expectedFaults;check(a->cycles==expected,"Actual native setup duration differs");}
        ++cases;if(injected)++irqCases;
    };
    for(unsigned flags=0;flags<256;++flags)verify(0,flags,false,false,0x400000,0x20000,0,0,0,0,unsigned(-1));
    for(bool clockOnly:{false,true})for(bool odd:{false,true})for(unsigned port:{0x400000u,0x600000u})for(uint32_t source:{0x20000u,0xffa40001u}){
        for(unsigned flags:{0u,0x10u,0x40u,0x50u,0x80u,0x90u,0xc0u,0xd0u})verify(0,flags,clockOnly,odd,port,source,2,1,1,1,unsigned(-1));
        for(unsigned header:{1u,0x7ffeu,0x8000u,0xfffeu,0xffffu})verify(uint16_t(header),0xd0,clockOnly,odd,port,source,0,0,0,0,unsigned(-1));
        for(unsigned busy:{65536u,65537u,131073u})verify(0,0xd0,clockOnly,odd,port,source,busy,0,0,0,unsigned(-1));
    }
    verify(0,0xd0,false,false,0x400000,0x20000,2,1,1,1,unsigned(-1),false,true);
    for(bool clockOnly:{false,true})for(unsigned at:interruptPoints)verify(0,0xd0,clockOnly,true,0x600000,0xffa40001,2,1,1,1,at);
    for(bool clockOnly:{false,true})verify(0,0x90,clockOnly,false,0x400000,0x20000,2,0,0,0,unsigned(-1),true);
    check(std::memcmp(a->memory,b->memory,1<<24)==0,"Native setup final full memory differs");
    std::cout<<"PASS: "<<cases<<" real runtime complete bootstrap setup calls, "<<steps<<" independent original H8 steps; header/count edges, all initial flags, both DSPs/tagged source/stack/fetch modes, every512+512 generated tuning word, live readiness and "<<faults<<" native boot-ready fault entries; "<<irqCases<<" selected IRQ save/RTE/resume points, "<<resets<<" metric resets, "<<nestedCalls<<" interleaved child calls / "<<nestedSteps<<" child steps, "<<maximumLive<<" live parent+stream contexts; no synchronized H8 or recovery factory linked\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
