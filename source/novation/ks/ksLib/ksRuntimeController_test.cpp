#include "ksRuntimeController.h"
#include "ksLiveMidiController.h"
#include "ksDspTransport.h"
#include "ksRom.h"
#include <algorithm>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <tuple>

namespace {
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
std::unique_ptr<H8SDisassembler> lengthOracle;
struct Bus final:H8SDevice {
    explicit Bus(const std::vector<uint8_t>& image):rom(image),ram(1<<24){}
    const std::vector<uint8_t>& rom;std::vector<uint8_t> ram;
    std::vector<std::tuple<bool,uint32_t,uint8_t,uint64_t,uint32_t>> accesses;
    uint8_t read(uint32_t a)override{auto v=a<rom.size()?rom[a]:ram[a];accesses.emplace_back(false,a,v,state->cycles,state->getPC());return v;}
    void write(uint32_t a,uint8_t v)override{ram[a]=v;accesses.emplace_back(true,a,v,state->cycles,state->getPC());}
};
void same(const ks::ControllerCpu& a,const ks::ControllerCpu& b,const Bus& x,const Bus& y){
    check(a.getPC()==b.getPC(),"Native runtime PC differs");check(a.cycles==b.cycles,"Native runtime clocks differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"Native runtime flags differ");
    for(unsigned i=0;i<8;++i)check(a.regs[i].er==b.regs[i].er,"Native runtime register differs");
    check(a.indent==b.indent && a.lastread==b.lastread && a.lastwrite==b.lastwrite,"Native runtime bus/call bookkeeping differs");
    check(a.pending_irqs==b.pending_irqs && a.pending_irqs_high==b.pending_irqs_high,"Native runtime pending interrupt differs");
    auto expected=x.accesses;
    if(b.nativeFetchClocksOnly){
        // Only instruction fetch callbacks disappear: the verified SRAM is
        // immutable in native mode. Discover the reference fetch sequence and
        // its length independently with the original disassembler; all data
        // and IO accesses must still match, including their clock and PC.
        const auto first=std::find_if(expected.begin(),expected.end(),[](const auto& v){return !std::get<0>(v) && std::get<1>(v)==std::get<4>(v) && !(std::get<4>(v)&1);});
        check(first!=expected.end(),"Reference instruction fetch not found");
        const auto entry=std::get<1>(*first);lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
        const unsigned bytes=unsigned(lengthOracle->getPC())-entry;
        check(bytes>=2 && bytes<=10 && size_t(expected.end()-first)>=bytes,"Reference fetch length invalid");
        for(unsigned n=0;n<bytes;++n)check(!std::get<0>(first[n]) && std::get<1>(first[n])==entry+n,"Reference fetch is not a contiguous code read");
        expected.erase(first,first+bytes);
    }
    if(expected!=y.accesses){
        for(size_t i=0;i<std::min(expected.size(),y.accesses.size());++i)if(expected[i]!=y.accesses[i]){
            auto print=[](const auto& v){std::cerr<<std::get<0>(v)<<':'<<std::hex<<std::get<1>(v)<<':'<<unsigned(std::get<2>(v))<<':'<<std::get<3>(v)<<':'<<std::get<4>(v)<<std::dec;};
            std::cerr<<"bus index "<<i<<" original ";print(expected[i]);std::cerr<<" native ";print(y.accesses[i]);std::cerr<<'\n';break;
        }
        throw std::runtime_error("Native runtime bus access/clock/in-flight PC differs");
    }
}
}
int main(int argc,char** argv){try{
    if(argc!=2){std::cout<<"SKIP: private verified KS OS required\n";return 77;}
    ks::Rom os(argv[1]);check(os.isValid(),"Invalid KS OS");
    lengthOracle=std::make_unique<H8SDisassembler>();lengthOracle->loadmem(os.data().data(),unsigned(os.data().size()),0);g_dasm=false;
    auto original=std::make_unique<ks::ControllerCpu>(),native=std::make_unique<ks::ControllerCpu>();
    Bus x(os.data()),y(os.data());
    for(auto* h:{original.get(),native.get()}){
        h->loadmem(os.data().data(),unsigned(os.data().size()),0);
        h->loadmem(os.data().data(),unsigned(os.data().size()),0xa00000);h->interruptMode2=true;
    }
    original->memmap(&x,0,1<<24);native->memmap(&y,0,1<<24);
    ks::LiveMidiController shared(*native);ks::RuntimeController controller(*native,shared);
    native->interpreterAllowed=false;std::mt19937 random(0x4b534e52);uint64_t boundaries=0;
    for(unsigned pc=0x170;pc<0xb8e8;pc+=2){
        for(unsigned sample=0;sample<32;++sample){
            native->nativeFetchClocksOnly=bool(sample&1);
            for(unsigned i=0;i<8;++i)original->regs[i].er=native->regs[i].er=int32_t(random());
            original->pc=original->makepc(pc);native->pc=native->makepc(pc);
            original->setSP(0xfff000);native->setSP(0xfff000);
            original->ccr=native->ccr=uint8_t(random());original->exr=native->exr=uint8_t(random()&7);
            original->pending_irqs=native->pending_irqs=0;original->pending_irqs_high=native->pending_irqs_high=0;
            x.accesses.clear();y.accesses.clear();
            if(!controller.stepCode(*native))break;
            original->step();++boundaries;
            try{same(*original,*native,x,y);}catch(const std::exception&){std::cerr<<"Runtime PC="<<std::hex<<pc<<std::dec<<" sample="<<sample<<'\n';throw;}
        }
    }
    check(boundaries==13766*32,"Native runtime boundary coverage differs");
    check(x.ram==y.ram,"Native runtime final RAM differs");
    // Execute interrupt arbitration plus the first actual handler boundary, as
    // the reference core does in one step. Cover masks, priorities and ties.
    for(bool clockOnly:{false,true})for(unsigned vector:{28,32,42,89,90})for(unsigned mask=0;mask<8;++mask){
        native->nativeFetchClocksOnly=clockOnly;
        for(auto* h:{original.get(),native.get()}){
            h->pc=h->makepc(0x4338);h->setSP(0xfff000);h->ccr=0;h->exr=uint8_t(mask);
            h->pending_irqs=vector<64?1ull<<vector:0;h->pending_irqs_high=vector>=64?1ull<<(vector-64):0;
            for(auto& priority:h->interruptPriority)priority=0;h->interruptPriority[vector]=6;
            if(vector==32){h->pending_irqs|=1ull<<28;h->interruptPriority[28]=6;}
        }
        x.accesses.clear();y.accesses.clear();controller.step(*native);original->step();same(*original,*native,x,y);
    }
    check(x.ram==y.ram && native->interpretedSteps==0,"Native runtime executed interpreter or changed final RAM");
    // Independent original-H8 oracle for complete timed HDI jobs. Randomize
    // live port replies/words/flags, both DSPs and entry bus history. Discover
    // and remove each reference code fetch with the original disassembler;
    // every data access, in-flight PC and access cycle must remain identical.
    uint64_t jobs=0;
    native->nativeFetchClocksOnly=true;native->nativeLowBase=os.data().data();
    native->nativeLowSize=unsigned(os.data().size());native->nativeLowWritable=true;
    for(unsigned pc:{0x2416,0x2424,0x245e,0x246c,0x24d8,0x24e6,0x2520,0x252e,
        0x256e,0x257c,0x25b6,0x25c4,0x243a,0x2482,0x2592,0x25da,0x24fc,0x2544,0x25e4})
        for(unsigned port:{0x400000,0x600000})for(unsigned trial=0;trial<32;++trial){
            for(unsigned i=0;i<8;++i)original->regs[i].er=native->regs[i].er=int32_t(random());
            for(unsigned off=0;off<16;++off)x.ram[port+off]=y.ram[port+off]=uint8_t(random());
            if(trial<8){
                constexpr int16_t countdowns[]{0,1,2,32767,-32768,-1,-2,3};
                original->regs[0].e=native->regs[0].e=countdowns[trial];
                x.ram[port+4]=y.ram[port+4]=0; // Busy, including timeout/wrap edges.
            }
            for(auto* h:{original.get(),native.get()}){
                h->pc=h->makepc(pc);h->cycles=1000;h->regs[5].er=port;
                h->pending_irqs=h->pending_irqs_high=0;h->interruptMode2=true;
                h->setSP(0xfff000);h->indent=0;
            }
            original->ccr=native->ccr=uint8_t(random());original->exr=native->exr=uint8_t(random()&7);
            for(auto* h:{original.get(),native.get()}){
                h->interruptPriority[0]=h->interruptPriority[90]=1;
                if(trial&1){h->exr=6;h->pending_irqs=1;h->pending_irqs_high=1ull<<(90-64);}
            }
            original->lastread=native->lastread=int(random());original->lastwrite=native->lastwrite=int(random());
            x.accesses.clear();y.accesses.clear();
            check(ks::DspTransport::candidate(pc),"HDI entry not recognized");
            // A task without sufficient time (or with pending arbitration)
            // must leave even the bus and register frame untouched.
            check(!ks::DspTransport::service(*native,1001),"HDI job crossed its deadline");
            const auto pending=native->pending_irqs;const auto mask=native->exr;
            native->pending_irqs=1;native->interruptPriority[0]=7;native->exr=0;
            check(!ks::DspTransport::service(*native,1100),"HDI job skipped unmasked IRQ");
            native->pending_irqs=pending;native->interruptPriority[0]=1;native->exr=mask;
            const auto high=native->pending_irqs_high;
            native->pending_irqs_high=1ull<<(90-64);native->interruptPriority[90]=7;native->exr=6;
            check(!ks::DspTransport::service(*native,1100),"HDI job skipped unmasked high IRQ");
            native->pending_irqs_high=high;native->interruptPriority[90]=1;native->exr=mask;
            check(native->getPC()==pc && native->cycles==1000 && y.accesses.empty(),"Rejected HDI job mutated state");
            const unsigned count=ks::DspTransport::service(*native,1100);check(count>0 && native->cycles<=1100,"HDI job did not execute within deadline");
            std::vector<std::tuple<bool,uint32_t,uint8_t,uint64_t,uint32_t>> data;
            for(unsigned n=0;n<count;++n){
                const auto entry=original->getPC();lengthOracle->pc=lengthOracle->makepc(entry);lengthOracle->step();
                const auto bytes=unsigned(lengthOracle->getPC())-entry;
                x.accesses.clear();original->step();check(x.accesses.size()>=bytes,"HDI reference fetch truncated");
                for(unsigned i=0;i<bytes;++i)check(!std::get<0>(x.accesses[i]) && std::get<1>(x.accesses[i])==entry+i,"HDI reference fetch differs");
                data.insert(data.end(),x.accesses.begin()+bytes,x.accesses.end());
            }
            x.accesses=std::move(data);
            // same() removes code only when comparing one native boundary.
            native->nativeFetchClocksOnly=false;same(*original,*native,x,y);native->nativeFetchClocksOnly=true;
            ++jobs;
        }
    check(jobs==1216 && x.ram==y.ram && native->interpretedSteps==0,"HDI job coverage/RAM/interpreter differs");
    native->nativeLowBase=nullptr;native->nativeLowWritable=false;
    bool rejected=false;try{native->step();}catch(const std::logic_error&){rejected=true;}
    check(rejected && native->interpretedSteps==0,"Native interpreter lock did not reject execution");
    native->pc=native->makepc(0xf00000);native->pending_irqs=native->pending_irqs_high=0;rejected=false;
    try{controller.step(*native);}catch(const std::runtime_error&){rejected=true;}
    check(rejected && native->interpretedSteps==0,"Unknown native state fell back to H8");
    native->memory[0xb000]^=1;rejected=false;
    try{ks::RuntimeController bad(*native,shared);}catch(const std::runtime_error&){rejected=true;}
    check(rejected,"Native runtime accepted different ROM");native->memory[0xb000]^=1;
    native->memory[0xa0b000]^=1;rejected=false;
    try{ks::RuntimeController bad(*native,shared);}catch(const std::runtime_error&){rejected=true;}
    check(rejected,"Native runtime accepted different active code image");
    std::cout<<"PASS: "<<boundaries<<" native boundaries, 80 interrupt/mask/tie entries and "<<jobs<<" timed HDI jobs; registers/flags/clocks/PC/data/IO bus and RAM match H8; zero interpreter calls; deadline/IRQ, unknown-state, ROM/image and interpreter-lock guards\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
