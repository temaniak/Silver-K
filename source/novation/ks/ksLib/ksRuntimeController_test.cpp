#include "h8s/h8s.hpp"
#include "ksRuntimeController.h"
#include "ksLiveMidiController.h"
#include "ksDspTransport.h"
#include "ksControllerJobs.h"
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
struct Bus final:ks::ControllerBus {
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
        const auto first=std::find_if(expected.begin(),expected.end(),[](const auto& v){return !std::get<0>(v) && std::get<1>(v)==(std::get<4>(v)&0xffffff) && !(std::get<4>(v)&1);});
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
            shared.midiDecodeCalls.discardCalls();shared.midiControlCalls.discardCalls();shared.midiAuxCalls.discardCalls();shared.resetImageCalls.discardCalls();shared.hardwareSetupCalls.discardCalls();shared.midiPartInitCalls.discardCalls();shared.midiRouteCalls.discardCalls();shared.stateFieldMapCalls.discardCalls();shared.voiceTableInitCalls.discardCalls();shared.stateParameterEditCalls.discardCalls();shared.statePatchNameCalls.discardCalls();shared.stateStorageCalls.discardCalls();shared.panelDecayCalls.discardCalls();shared.stateModeCalls.discardCalls();shared.stateParameterExpandCalls.discardCalls();shared.statePackFieldsCalls.discardCalls();shared.statePackRefreshCalls.discardCalls();shared.stateGlobalsCalls.discardCalls();shared.panelByteCalls.discardCalls();shared.panelPollCalls.discardCalls();shared.panelActionCalls.discardCalls();shared.arpPrimitiveCalls.discardCalls();shared.arpScheduleCalls.discardCalls();shared.arpMainCalls.discardCalls();shared.stateMultiSerializeCalls.discardCalls();shared.statePatchLoadCalls.discardCalls();shared.stateProgramCalls.discardCalls();shared.stateMultiLoadCalls.discardCalls();shared.statePresetAddressCalls.discardCalls();shared.stateSingleLoadCalls.discardCalls();shared.serialQueueCalls.discardCalls();shared.serialReceiveCalls.discardCalls();shared.serialSetupCalls.discardCalls();shared.serialTransmitCalls.discardCalls();shared.serialIrqCalls.discardCalls();shared.serialDirectCalls.discardCalls();shared.arpEventCalls.discardCalls();shared.arpSelectCalls.discardCalls();shared.stateManualEditCalls.discardCalls();shared.statePostEditCalls.discardCalls();shared.panelControlCalls.discardCalls();shared.panelToggleCalls.discardCalls();shared.panelDecimalCalls.discardCalls();shared.panelTextCalls.discardCalls();shared.internalMidiCalls.discardCalls();shared.internalNoteCalls.discardCalls();shared.sysExFrameCalls.discardCalls();shared.stateFlashCommitCalls.discardCalls();shared.flashPageCalls.discardCalls();shared.stateStoreServicesCalls.discardCalls();shared.statePresetFormatCalls.discardCalls();shared.statePresetSaveCalls.discardCalls();shared.stateGlobalSaveCalls.discardCalls();shared.stateFactoryServicesCalls.discardCalls();shared.panelFormatCalls.discardCalls();shared.panelHexCalls.discardCalls();shared.panelPercentCalls.discardCalls();shared.panelVersionCalls.discardCalls();shared.patchExpandCalls.discardCalls();shared.sysExActionCalls.discardCalls();shared.legacySysExCalls.discardCalls();shared.stateMultiSaveCalls.discardCalls();shared.stateNameCodecCalls.discardCalls();shared.stateDeferredPartsCalls.discardCalls();shared.statePanelRefreshCalls.discardCalls();shared.stateAuditionCalls.discardCalls();shared.panelPublishCalls.discardCalls();shared.panelMasksCalls.discardCalls();shared.panelInitializeCalls.discardCalls();shared.panelDetectCalls.discardCalls();shared.firmwareLayoutCheckCalls.discardCalls();shared.sysExDumpCalls.discardCalls();shared.midiFileTimeCalls.discardCalls();shared.stateProgramChangeCalls.discardCalls();shared.stateNameEditorCalls.discardCalls();shared.stateBankDisplayCalls.discardCalls();shared.stateDspFactoryTransferCalls.discardCalls();shared.stateDspFactoryResetCalls.discardCalls();shared.stateDspFactoryUploadCalls.discardCalls();shared.timingTablesCalls.discardCalls();shared.midiFileEventCalls.discardCalls();shared.monoArpCalls.discardCalls();shared.midiFileTransportCalls.discardCalls();shared.editAddressCalls.discardCalls();shared.editSceneCalls.discardCalls();shared.editDecisionCalls.discardCalls();shared.noteActions.discardTransientCalls();shared.parameterScaleCalls.discardCalls();shared.voiceActions.discardTransientCalls();shared.faultCalls.discardCalls();shared.bootStreamCalls.discardCalls();shared.bootSetupCalls.discardCalls();shared.startupCalls.discardCalls();shared.scheduledTimerCalls.discardCalls();shared.serviceCycleCalls.discardCalls();shared.stateInitCalls.discardCalls();
            shared.resetImageCalls.discardCalls();shared.hardwareSetupCalls.discardCalls();
        native->nativeFetchClocksOnly=bool(sample&1);
            for(unsigned i=0;i<8;++i)original->regs[i].er=native->regs[i].er=int32_t(random());
            original->pc=original->makepc(pc);native->pc=native->makepc(pc);
            original->setSP(0xfff000);native->setSP(0xfff000);
            original->ccr=native->ccr=uint8_t(random());original->exr=native->exr=uint8_t(random()&7);
            original->pending_irqs=native->pending_irqs=0;original->pending_irqs_high=native->pending_irqs_high=0;
            x.accesses.clear();y.accesses.clear();
            try{if(!controller.stepCode(*native))break;}catch(const std::exception&){std::cerr<<"Runtime admission PC="<<std::hex<<pc<<std::dec<<" sample="<<sample<<'\n';throw;}
            original->step();++boundaries;
            try{same(*original,*native,x,y);}catch(const std::exception&){std::cerr<<"Runtime PC="<<std::hex<<pc<<std::dec<<" sample="<<sample<<'\n';throw;}
        }
    }
    // The 23 shared pin-setup boundaries now also admit through normal IO.
    check(boundaries==13789*32,"Native runtime boundary coverage differs");
    // Reset boundaries are independently checked even for firmware-update,
    // checksum-failure, all branch outcomes and arbitrary partial frames.
    // These cases supplement full production-reset timelines.
    ks::RuntimeController resetController(*native,shared,ks::RuntimeController::Entry::Reset);
    for(unsigned n=0;n<14;++n){
        const auto v=os.data()[0x422+n];
        original->memory[0xffec00+n]=native->memory[0xffec00+n]=lengthOracle->memory[0xffec00+n]=v;
        x.ram[0xffec00+n]=y.ram[0xffec00+n]=v;
    }
    uint64_t resetCases=0;
    for(unsigned pc:{0x170u,0x180u,0x2ecu,0x2f2u,0x2f8u,0x2feu,0x300u,0x302u,0x304u,0x306u,0x30au,0x30eu,0x312u,0x314u,0x318u,0x31au,0x31eu,0x320u,0x322u,0x324u,0x326u,0x328u,0x32au,0x32eu,0x332u,0x336u,0x33au,0x33cu,0x342u,0x346u,0x34cu,0x34eu,0x350u,0x356u,0x35cu,0x360u,0x362u,0x364u,0x366u,0x368u,0x36au,0x36eu,0x374u,0x376u,0x37au,0x37eu,0x380u,0x382u,0x388u,0x38eu,0x392u,0x394u,0x396u,0x398u,0x39au,0x39cu,0x39eu,0x3a0u,0x3a6u,0x3b0u,0x3b2u,0x3b4u,0x3b6u,0x3b8u,0x3bau,0x3bcu,0x3beu,0x3c6u,0x3c8u,0x3cau,0x3ccu,0x3d0u,0x3dau,0x3dcu,0x3deu,0x3e6u,0x3e8u,0x3eau,0x3ecu,0x3eeu,0x3f0u,0x3f4u,0x3fau,0x3feu,0x400u,0x404u,0x40au,0x410u,0x414u,0x416u,0x418u,0x41au,0x41cu,0x41eu,0xffffec00u,0xffffec04u,0xffffec06u,0xffffec0au,0x430u,0x436u,0x43au,0x440u,0x446u,0x44au,0x450u,0x454u,0x456u,0x458u,0x45au,0x45cu,0x45eu,0x460u,0x464u,0x466u,0x468u,0x46au,0x46eu,0x470u,0x474u,0x476u,0x47au,0x47cu,0x47eu,0x480u,0x484u,0x486u,0x488u,0x48au,0x48eu})for(unsigned sample=0;sample<32;++sample){
        shared.resetImageCalls.discardCalls();shared.hardwareSetupCalls.discardCalls();
        native->nativeFetchClocksOnly=bool(sample&1);
        for(unsigned i=0;i<8;++i)original->regs[i].er=native->regs[i].er=int32_t(random());
        for(auto* h:{original.get(),native.get()}){
            h->pc=h->makepc(pc);h->setSP(0xfff000);h->pending_irqs=h->pending_irqs_high=0;
        }
        original->ccr=native->ccr=uint8_t(random());original->exr=native->exr=uint8_t(random()&7);
        original->lastread=native->lastread=int(random());original->lastwrite=native->lastwrite=int(random());
        x.accesses.clear();y.accesses.clear();resetController.stepStartup(*native);original->step();
        try{same(*original,*native,x,y);}catch(const std::exception&){std::cerr<<"Reset PC="<<std::hex<<pc<<std::dec<<" sample="<<sample<<'\n';throw;}
        ++resetCases;
    }
    check(resetCases==4128 && x.ram==y.ram,"Reset boundary coverage/RAM differs");
    std::cout<<"PASS: "<<resetCases<<" reset/copy/checksum/overlay boundaries, mapped and clock-only fetch, arbitrary flags/values exact\n";

    check(x.ram==y.ram,"Native runtime final RAM differs");
    // Execute interrupt arbitration plus the first actual handler boundary, as
    // the reference core does in one step. Cover masks, priorities and ties.
    for(bool clockOnly:{false,true})for(unsigned vector:{28,32,42,89,90})for(unsigned mask=0;mask<8;++mask){
        // Each trial supplies a fresh synthetic IRQ frame and executes only its first boundary.
        shared.serialIrqCalls.discardCalls();
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
            check(!ks::DspTransport::candidate(pc),"Retired HDI batch remained a production candidate");
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
    // Independent original core validates every control-job entry, deadline,
    // selector byte (including out-of-range bytes), arbitrary register/flag
    // state, signed counter wrap and aligned/unaligned return stack.
    uint64_t controlCases=0,controlSteps=0;
    constexpr unsigned windows[]{1,2,3,5,6,7,8,9,11,12,14,15,16,17,20,23,24,32,63,64,127,128,512,3000};
    for(unsigned entry:{0x4000,0x4006,0x4008,0x400c,0x400e,0x4014,
                       0xa66a,0xa670,0xa672,0xa674,0xa676,0xa67c,0xa6be,0xa6c6})
        for(unsigned available:windows)for(unsigned trial=0;trial<256;++trial){
            for(unsigned i=0;i<8;++i)original->regs[i].er=native->regs[i].er=int32_t(random());
            constexpr uint32_t edges[]{0,1,0xffffffffu,0x80000000u,0x7fffffffu,0x40000000u,0x10000u,0x80000001u};
            if(trial<8)original->regs[0].er=native->regs[0].er=int32_t(edges[trial]);
            original->regs[1].rl=native->regs[1].rl=int8_t(trial);
            const unsigned sp=(trial&1)?0xfff001:0xa18000;
            for(auto* h:{original.get(),native.get()}){
                h->pc=h->makepc(entry);h->cycles=1000;h->setSP(sp);h->indent=1;
                h->pending_irqs=h->pending_irqs_high=0;h->interruptMode2=true;
            }
            original->ccr=native->ccr=uint8_t(random());original->exr=native->exr=uint8_t(random()&7);
            original->lastread=native->lastread=int(random());original->lastwrite=native->lastwrite=int(random());
            for(unsigned n=0;n<4;++n)x.ram[sp+n]=y.ram[sp+n]=native->memory[sp+n]=uint8_t(0x4338u>>(8*(3-n)));
            const uint8_t part=uint8_t(trial);x.ram[0x17526]=y.ram[0x17526]=part;native->memory[0xa17526]=part;
            native->nativeLowBase=native->memory+0xa00000;native->nativeLowSize=0x200000;native->nativeLowWritable=true;
            x.accesses.clear();y.accesses.clear();
            // Rejection must not fetch, read RAM, mutate masks or advance time.
            native->pending_irqs=1;native->interruptPriority[0]=7;native->exr=0;
            check(!ks::ControllerJobs::service(*native,1128).steps,"Control job skipped unmasked IRQ");
            native->pending_irqs=0;native->exr=original->exr;
            native->pending_irqs_high=1ull<<26;native->interruptPriority[90]=7;native->exr=0;
            check(!ks::ControllerJobs::service(*native,1128).steps,"Control job skipped high IRQ");
            native->pending_irqs_high=0;native->exr=original->exr;
            native->interruptMode2=false;
            check(!ks::ControllerJobs::service(*native,1128).steps,"Control job admitted mode zero");
            native->interruptMode2=true;
            if(trial&2)for(auto* h:{original.get(),native.get()}){
                h->exr=6;h->pending_irqs=1;h->pending_irqs_high=1ull<<26;
                h->interruptPriority[0]=h->interruptPriority[90]=1;
            }
            check(native->getPC()==entry && native->cycles==1000 && y.accesses.empty(),"Rejected control job changed state");
            const auto result=ks::ControllerJobs::service(*native,1000+available);
            if(native->cycles>1000+available)std::cerr<<"Control deadline entry="<<std::hex<<entry<<std::dec<<" available="<<available<<" trial="<<trial<<" end="<<native->cycles<<" steps="<<result.steps<<" sp="<<std::hex<<sp<<std::dec<<"\n";
            check(native->cycles<=1000+available,"Control job crossed deadline");
            for(unsigned n=0;n<result.steps;++n){
                const unsigned pc=original->getPC();lengthOracle->pc=lengthOracle->makepc(pc);lengthOracle->step();
                const unsigned bytes=unsigned(lengthOracle->getPC())-pc;
                x.accesses.clear();original->step();
                check(x.accesses.size()>=bytes,"Control-job reference fetch truncated");
                for(unsigned i=0;i<bytes;++i)check(!std::get<0>(x.accesses[i]) && std::get<1>(x.accesses[i])==pc+i,"Control-job reference fetch differs");
                // Direct native SRAM reads have no Board callback. Admit only
                // the actual selector and return stack reads; any IO fails.
                for(auto it=x.accesses.begin()+bytes;it!=x.accesses.end();++it){
                    const unsigned a=std::get<1>(*it);
                    check(!std::get<0>(*it) && (a==0x17526 || (a>=sp && a<sp+4)),"Control job accessed unexpected data/IO");
                }
            }
            x.accesses.clear();native->nativeFetchClocksOnly=false;
            try{same(*original,*native,x,y);}catch(const std::exception&){std::cerr<<"Control job PC="<<std::hex<<entry<<std::dec<<" window="<<available<<" trial="<<trial<<'\n';throw;}
            native->nativeFetchClocksOnly=true;
            check(result.noteSteps==(entry<0xa6be?result.steps:0),"Control-job accounting differs");
            controlSteps+=result.steps;++controlCases;
        }
    check(controlCases==86016 && controlSteps>0 && native->interpretedSteps==0,"Control-job coverage/interpreter differs");
    std::cout<<"PASS: "<<controlCases<<" control-job deadline/IRQ/state cases; "<<controlSteps<<" reference boundaries exact\n";
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
