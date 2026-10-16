#include "ksLiveMidiController.h"
#include "ksRom.h"
#include <array>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace {
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
struct Bus final:ks::ControllerBus {
    explicit Bus(const std::vector<uint8_t>& os):rom(os),ram(1<<24){}
    const std::vector<uint8_t>& rom;
    std::vector<uint8_t> ram;
    std::vector<std::tuple<bool,uint32_t,uint8_t,uint64_t,uint32_t>> accesses;
    uint8_t read(uint32_t a)override{const auto v=a<rom.size()?rom[a]:ram[a];accesses.emplace_back(false,a,v,state->cycles,state->getPC());return v;}
    void write(uint32_t a,uint8_t v)override{ram[a]=v;accesses.emplace_back(true,a,v,state->cycles,state->getPC());}
};
void same(const ks::ControllerCpu& a,const ks::ControllerCpu& b,const Bus& x,const Bus& y){
    check(a.getPC()==b.getPC(),"Native MIDI PC differs");check(a.cycles==b.cycles,"Native MIDI cycles differ");
    check(a.ccr==b.ccr && a.exr==b.exr,"Native MIDI flags differ");
    for(unsigned i=0;i<8;++i)check(a.regs[i].er==b.regs[i].er,"Native MIDI register differs");
    check(a.indent==b.indent && a.lastread==b.lastread && a.lastwrite==b.lastwrite,"Native MIDI bus/call bookkeeping differs");
    check(x.accesses==y.accesses,"Native MIDI bus accesses differ");
}
}
int main(int argc,char**argv){try{
    if(argc!=2){std::cout<<"SKIP: private verified KS OS required\n";return 77;}
    ks::Rom os(argv[1]);check(os.isValid(),"Invalid KS OS");
    auto original=std::make_unique<ks::ControllerCpu>(),native=std::make_unique<ks::ControllerCpu>();
    Bus x(os.data()),y(os.data());
    for(auto* h:{original.get(),native.get()}){h->loadmem(os.data().data(),os.data().size(),0);h->interruptMode2=true;}
    original->memmap(&x,0,1<<24);native->memmap(&y,0,1<<24);
    ks::LiveMidiController controller(*native);
    std::mt19937 random(0x4b534d49);uint64_t steps=0,bytes=0;
    bool toggle=false;
    auto byte=[&](uint8_t v){
        for(auto* h:{original.get(),native.get()}){h->pc=h->makepc(0x2de8);h->setSP(0xfff000);h->pushPC(h->makepc(0x200000));h->regs[6].rl=int8_t(v);}
        // The native entry guard inspects the Board's low-RAM alias without bus
        // effects. Mirror only those guard bytes in this isolated synthetic bus.
        native->memory[0xa15c1b]=y.ram[0x15c1b];native->memory[0xa15c1c]=y.ram[0x15c1c];
        native->memory[0xa15c20]=y.ram[0x15c20];
        for(unsigned n=0;n<100;++n){
            x.accesses.clear();y.accesses.clear();
            // Simulate disabling/re-enabling at boundaries inside a partial
            // message. The C++ side must leave a fully resumable guest ABI.
            if((toggle && (n%3)==1) || !controller.step(*native))native->step();original->step();++steps;
            try{same(*original,*native,x,y);}catch(const std::exception& e){
                std::cerr<<"byte="<<unsigned(v)<<" index="<<bytes<<" PC="<<std::hex<<original->getPC()<<'/'<<native->getPC()<<std::dec<<" cycles="<<original->cycles<<'/'<<native->cycles<<'\n';throw;
            }
            if(original->getPC()==0x328e || original->getPC()==0x200000){++bytes;return;}
        }
        throw std::runtime_error("MIDI parser failed to return/dispatch within bound");
    };
    // All channels, all seven-bit note/velocity/CC data, explicit statuses,
    // arbitrary live values and running status. H8 remains the independent oracle.
    for(unsigned channel=0;channel<16;++channel)for(uint8_t kind:{0x80,0x90,0xb0}){
        byte(uint8_t(kind|channel));
        for(unsigned value=0;value<128;++value){
            original->ccr=native->ccr=uint8_t(random());
            byte(uint8_t(value));byte(uint8_t(random()&127));
        }
    }
    check(controller.messages==16*3*128 && controller.nativeSteps>0,"Native decoder coverage missing");
    toggle=true;byte(0x90);for(unsigned n=0;n<128;++n){byte(uint8_t(n));byte(uint8_t(random()&127));}
    byte(0xb0);for(uint8_t cc:{1,7,10,11,64,105,120,123}){byte(cc);byte(0);byte(cc);byte(127);}
    check(x.ram==y.ram,"Final MIDI RAM differs");
    for(uint8_t unsupported:{0xa0,0xc0,0xd0,0xe0,0xf0,0xf1,0xf8,0xff}){
        native->pc=native->makepc(0x2de8);native->regs[6].rl=int8_t(unsupported);
        check(!controller.step(*native),"Unsupported status did not retain H8 path");
    }
    native->pc=native->makepc(0x2de8);native->regs[6].rl=int8_t(0x90);
    native->pending_irqs=1ull<<32;native->interruptPriority[32]=6;native->exr=0;
    check(!controller.step(*native),"Pending interrupt bypassed H8 arbitration");
    native->pending_irqs=0;check(controller.step(*native),"Native parser did not resume");
    native->memory[0x2de8]^=1;bool rejected=false;try{ks::LiveMidiController bad(*native);}catch(const std::exception&){rejected=true;}
    check(rejected,"Unverified decoder revision accepted");
    // Independently execute each fixed note/pedal transition against original
    // H8 with varied operands/flags. No DSP routines are stubbed in this check:
    // calls stop at their real ABI target after one boundary.
    native->memory[0x2de8]^=1;
    native->memory[0x3426]^=1;rejected=false;
    try{ks::LiveMidiController bad(*native);}catch(const std::exception&){rejected=true;}
    check(rejected,"Unverified note-handler revision accepted");native->memory[0x3426]^=1;
    native->memory[0xa338]^=1;rejected=false;
    try{ks::LiveMidiController bad(*native);}catch(const std::exception&){rejected=true;}
    check(rejected,"Unverified voice-handler revision accepted");native->memory[0xa338]^=1;
    ks::LiveNoteController actions;
    native->cycles=original->cycles;native->lastread=original->lastread;
    native->lastwrite=original->lastwrite;native->indent=original->indent;
    uint64_t noteBoundaries=0;
    for(unsigned pc=0;pc<0xa67e;pc+=2)if(ks::LiveNoteController::candidate(pc)){
        for(unsigned sample=0;sample<32;++sample){
            actions.discardTransientCalls();
            for(unsigned i=0;i<8;++i)original->regs[i].er=native->regs[i].er=int(random()&0xffffff);
            original->setSP(0xfff000);native->setSP(0xfff000);
            original->pc=original->makepc(pc);native->pc=native->makepc(pc);
            original->ccr=native->ccr=uint8_t(random());original->exr=native->exr=0;
            original->pending_irqs=native->pending_irqs=0;
            x.accesses.clear();y.accesses.clear();
            if(!actions.step(*native))break;
            check(ks::LiveMidiController::candidate(pc),"Hot page filter excludes a native note transition");
            original->step();++noteBoundaries;
            try{same(*original,*native,x,y);}catch(const std::exception&){std::cerr<<"Note transition PC="<<std::hex<<pc<<std::dec<<" sample="<<sample<<'\n';throw;}
        }
    }
    check(noteBoundaries==462*32,"Note transition coverage differs");
    check(x.ram==y.ram,"Note transition RAM differs");
    ks::LiveVoiceController voices;uint64_t voiceBoundaries=0;
    for(unsigned pc=0;pc<0xa69e;pc+=2)if(ks::LiveVoiceController::candidate(pc)){
        for(unsigned sample=0;sample<32;++sample){
            for(unsigned i=0;i<8;++i)original->regs[i].er=native->regs[i].er=int(random()&0xffffff);
            original->setSP(0xfff000);native->setSP(0xfff000);
            original->pc=original->makepc(pc);native->pc=native->makepc(pc);
            original->ccr=native->ccr=uint8_t(random());original->exr=native->exr=0;
            original->pending_irqs=native->pending_irqs=0;
            x.accesses.clear();y.accesses.clear();
            if(!voices.step(*native))break;
            check(ks::LiveMidiController::candidate(pc),"Hot filter excludes native voice transition");
            original->step();++voiceBoundaries;
            try{same(*original,*native,x,y);}catch(const std::exception&){std::cerr<<"Voice transition PC="<<std::hex<<pc<<std::dec<<" sample="<<sample<<'\n';throw;}
        }
    }
    check(voiceBoundaries==640*32,"Voice transition coverage differs");
    check(x.ram==y.ram,"Voice transition RAM differs");
    for(unsigned pc:{0x2416,0x77f4,0x8fcc,0xa338}){
        native->pc=native->makepc(pc);native->pending_irqs=1ull<<32;
        native->interruptPriority[32]=6;native->exr=0;
        const auto cycles=native->cycles;
        check(!controller.step(*native) && native->getPC()==pc && native->cycles==cycles,"Native voice bypassed pending interrupt");
        native->pending_irqs=0;
    }
    std::cout<<"PASS: "<<bytes<<" live bytes, "<<controller.messages<<" Note/CC messages, "<<steps
        <<" decoder boundaries + "<<noteBoundaries<<" note/pedal + "<<voiceBoundaries<<" voice/transport boundaries: original H8 vs C++ registers/flags/clocks/PC and every bus access; toggle mid-message, unsupported statuses, interrupts and firmware guard\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
