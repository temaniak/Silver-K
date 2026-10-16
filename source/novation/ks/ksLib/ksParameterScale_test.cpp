#include "ksParameterScale.h"
#include "ksRom.h"
#include "h8s/h8s.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <limits>

namespace {struct Original final:H8SEmulator {using H8SEmulator::lastread;using H8SEmulator::lastwrite;using H8SEmulator::indent;};}
int main(int argc,char** argv){try{
    // Fixed synthetic inputs admitted by the direct original-H8 oracle. These
    // exercise actual values/durations on public CI without distributing ROM.
    struct Vector {int16_t input;int32_t coefficient;unsigned clocks,boundaries;};
    constexpr Vector vectors[]{
        {0,0,138,46},{1,0,138,46},{1023,0,138,46},{1024,1,129,43},
        {11264,1024,36,12},{12288,2048,51,17},{32767,0x7ff00000,222,74},
        {-32768,-0x7ff00000,240,79},{-32767,-0x7ff00000,237,78},
        {-11264,-1024,51,16},{-1024,-1,144,47},{-1,0,153,50}
    };
    for(const auto& v:vectors){const auto actual=ks::decodeParameterScale(v.input);
        if(actual.coefficient!=v.coefficient || actual.clocks!=v.clocks || actual.boundaries!=v.boundaries)
            throw std::runtime_error("Admitted native scale vector differs");}
    // Input-free arithmetic properties also run in public CI.
    for(unsigned bits=0;bits<65536;++bits){
        const int16_t input=int16_t(bits);const auto v=ks::decodeParameterScale(input);
        if(v.clocks<36 || v.clocks>240 || v.boundaries<12
            || (input<0?v.coefficient>0:v.coefficient<0))throw std::runtime_error("Native scale bounds/sign");
        if(input!=-32768){const auto other=ks::decodeParameterScale(int16_t(-int(input)));
            if(other.coefficient!=-v.coefficient)throw std::runtime_error("Native scale sign symmetry");}
    }
    // Exercise every clock budget, not only a whole-function fast path. The
    // task owns no CPU context and must survive arbitrary scheduler pauses.
    uint64_t partitions=0;
    for(unsigned bits=0;bits<65536;++bits){
        const int16_t input=int16_t(bits);const auto expected=ks::decodeParameterScale(input);
        for(unsigned budget=0;budget<=expected.clocks+1;++budget){
            ks::ParameterScaleTask task(input);const auto first=task.advanceBefore(budget);
            if(first.clocks>budget || task.elapsedClocks()!=first.clocks
                || task.completedBoundaries()!=first.boundaries
                || (task.nextBoundaryClocks() && budget-first.clocks>=task.nextBoundaryClocks()))
                throw std::runtime_error("Native scale task crossed/missed a deadline");
            const auto pausedClocks=task.elapsedClocks(),pausedSteps=task.completedBoundaries();
            const auto pause=task.advanceBefore(0);
            if(pause.clocks || pause.boundaries || task.elapsedClocks()!=pausedClocks
                || task.completedBoundaries()!=pausedSteps)throw std::runtime_error("Paused scale task changed");
            const auto rest=task.advanceBefore(std::numeric_limits<unsigned>::max());
            if(!task.complete() || task.nextBoundaryClocks() || task.remainingClocks()
                || first.clocks+rest.clocks!=expected.clocks
                || first.boundaries+rest.boundaries!=expected.boundaries
                || task.result().coefficient!=expected.coefficient)
                throw std::runtime_error("Native scale task resume differs");
            const auto done=task.advanceBefore(std::numeric_limits<unsigned>::max());
            if(done.clocks || done.boundaries)throw std::runtime_error("Completed scale task changed");
            ++partitions;
        }
    }
    if(argc==1){std::cout<<"PASS: all 65536 native scale inputs bounded and sign-symmetric; "<<partitions<<" task deadline/resume cases; independent firmware execution requires private KS OS\n";return 0;}
    if(argc!=2)throw std::runtime_error("Expected optional private KS OS path");
    ks::Rom os(argv[1]);if(!os.isValid())throw std::runtime_error("Invalid KS OS");
    // Direct original H8, no synchronized adapter or native prediction in its
    // execution. Returning to an admitted sentinel ends the measured routine.
    auto h=std::make_unique<Original>();h->loadmem(os.data().data(),unsigned(os.data().size()),0);
    uint64_t cases=0,steps=0;
    for(unsigned bits=0;bits<65536;++bits)for(unsigned upper:{0u,0x10u,0x40u,0x50u,0x80u,0x90u,0xc0u,0xd0u}){
        const int16_t input=int16_t(bits);const auto native=ks::decodeParameterScale(input);
        const uint32_t retained=(bits*0x12345u)^0x93ae4c78u;
        h->regs[0].er=int32_t(0x89ab0000u|bits);h->regs[1].er=int32_t(retained);
        for(unsigned i=2;i<7;++i)h->regs[i].er=int32_t(retained+i);
        h->setSP(0xfff000);h->memory[0xfff000]=0;h->memory[0xfff001]=0;
        h->memory[0xfff002]=0x43;h->memory[0xfff003]=0x38;
        h->pc=h->makepc(0xa69e);h->ccr=uint8_t(upper|(bits&0x2f));h->cycles=1000;
        h->pending_irqs=h->pending_irqs_high=0;h->interruptMode2=true;
        h->lastread=int(bits);h->lastwrite=int(retained);
        h->indent=0;
        const auto prepared=ks::prepareParameterScale(input);
        ks::ParameterScaleTask task(input);
        unsigned count=0;
        do{const auto before=h->cycles;h->step();++count;
            if(h->getPC()!=0x4338){
                const auto elapsed=unsigned(h->cycles-before);
                if(task.nextBoundaryClocks()!=elapsed)throw std::runtime_error("Native task next boundary differs from original clocks");
                const auto advanced=task.advanceBefore(elapsed);
                if(advanced.clocks!=elapsed || advanced.boundaries!=1 || task.elapsedClocks()!=h->cycles-1000)
                    throw std::runtime_error("Native task timeline differs from independent original");
            }
            if(count>128)throw std::runtime_error("Original scale did not return");
            if(count==prepared.boundaries){
                if(h->cycles!=1000+prepared.clocks || uint32_t(h->regs[0].er)!=prepared.mantissa
                    || int8_t(h->regs[1].rl)!=prepared.shift || h->getPC()!=(prepared.shift==0?0xa6cc:0xa6bc))
                    throw std::runtime_error("Native scale preparation/duration differs from original");
            }
        }while(h->getPC()!=0x4338);
        if(!task.complete() || task.result().coefficient!=h->regs[0].er || task.elapsedClocks()!=native.clocks)
            throw std::runtime_error("Native task completion/value differs from independent original");
        if(h->regs[0].er!=native.coefficient || h->cycles!=1000+native.clocks+17 || count!=native.boundaries+1
            || uint32_t(h->regs[1].er)!=((uint32_t(bits)<<16)|(retained&0xff00)) || h->getSP()!=0xfff004
            || h->lastread!=-1 || h->lastwrite!=int(retained)){
            std::cerr<<"input="<<input<<" flags="<<upper<<" result="<<h->regs[0].er<<"/"<<native.coefficient
                     <<" clocks="<<(h->cycles-1000)<<"/"<<(native.clocks+17)<<" steps="<<count<<"/"<<(native.boundaries+1)<<'\n';
            throw std::runtime_error("Native scale differs from independent original execution");
        }
        for(unsigned i=2;i<7;++i)if(uint32_t(h->regs[i].er)!=retained+i)throw std::runtime_error("Original scale changed retained input");
        steps+=count;++cases;
    }
    std::cout<<"PASS: "<<cases<<" whole native parameter-scale cases, all 65536 inputs and eight retained masks; "<<steps
             <<" independent original H8 boundaries; coefficient, clocks, preparation/full coefficient and semantic durations and every task preemption boundary exact; "<<partitions<<" deadline/resume cases; native task is not connected to production\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
