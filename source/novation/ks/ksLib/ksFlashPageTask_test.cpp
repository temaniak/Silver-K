#include "ksFlashPageTask.h"
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool value, const char* why) { if (!value) throw std::runtime_error(why); }
using Compare = ks::FlashPageCompareTask;
using Program = ks::FlashPageProgramTask;
uint64_t comparisons = 0, programs = 0, boundaries = 0;

template<class Reader>
void compare(Compare& task, Reader&& read) {
    while (!task.complete()) {
        const auto cost = task.nextClocks();
        const auto stage = task.stage();
        unsigned observations = 0;
        auto observed = [&](bool target, unsigned index) { ++observations; return read(target,index); };
        check(!task.advanceBefore(cost-1,observed) && !observations && task.stage()==stage,
              "Comparison crossed a deadline");
        check(task.advanceBefore(cost,observed)==cost,"Comparison refused its interval");
        ++boundaries;
    }
    ++comparisons;
}
}

int main() { try {
    // Every first differing byte, every possible byte pair, and equality.
    for (unsigned first = 0; first <= 256; ++first) {
        Compare task(0xffa30001,0xff820055);
        unsigned reads = 0;
        compare(task,[&](bool target,unsigned index) {
            check(index==reads/2 && target==bool(reads&1),"Live byte order changed");
            ++reads;
            return uint8_t(index + (target && index==first));
        });
        check(reads==2*(first<256?first+1:256),"Comparison did not stop at first difference");
        check(task.source()==(first<256?0xffa30001u:0xffa30101u),"Comparison source result changed");
        check(task.target()==(first<256?0xff820055u:0x820100u),"Comparison target result changed");
    }
    for (unsigned a=0; a<256; ++a) for (unsigned b=0; b<256; ++b) {
        Compare task(0x30000,0x820000);
        compare(task,[&](bool target,unsigned index) { return uint8_t(index?0:target?b:a); });
        check(task.target()==(a==b?0x820100u:0x80820000u),"Byte equality policy changed");
    }

    // Real source/target data, command order, 16-bit pending wrap, protected
    // skips, invalid/alignment errors, live mutation and byte-clock wrap.
    for (unsigned mode=0; mode<9; ++mode) for (unsigned seed=0; seed<256; ++seed) {
        const uint32_t target=mode==1?0x800001:mode==2?0x7fff00:mode==3?0x820001:0x820000;
        Program task(0x30000,target);
        std::array<uint8_t,256> source{},destination{};
        for (unsigned i=0;i<256;++i) source[i]=uint8_t(seed+i*17);
        destination=source;
        if (mode!=0) destination[seed]^=0x80;
        unsigned commands=0,writes=0,reads=0,polls=0,verifies=0;
        uint16_t pending=seed&1?65535:seed;
        unsigned count=0;
        while (!task.complete()) {
            check(++count<12000,"Program stalled");
            if (task.awaitingCompare()) {
                Compare child(task.source(),task.target());
                if (mode==8 && verifies++==1) source[seed]^=1;
                compare(child,[&](bool isTarget,unsigned i) { return isTarget?destination[i]:source[i]; });
                check(task.resumeCompare(child.source(),child.target()),"Comparison completion rejected");
                continue;
            }
            const auto stage=task.stage();
            const auto cost=task.nextClocks();
            unsigned observations=0;
            auto read=[&](Program::Read what,unsigned i)->uint32_t {
                ++observations;
                switch(what) {
                case Program::Read::Pending:return pending;
                case Program::Read::Source:++reads;if(mode==7)source[i]^=1;return source[i];
                case Program::Read::StartTick:return 0xfffffffeu;
                case Program::Read::CurrentTick:return 0xfffffffeu+(mode==5?15:mode==6?14:polls);
                case Program::Read::Ready:++polls;return destination[i]^(mode==4&&polls<4?1:0);
                }
                return 0;
            };
            auto write=[&](Program::Write what,uint16_t value,unsigned i) {
                ++observations;
                if(what==Program::Write::Pending) { pending=value;return; }
                if(what==Program::Write::Byte) {
                    check(commands==3&&i==writes,"Page byte publication order changed");
                    destination[i]=uint8_t(value);++writes;return;
                }
                constexpr unsigned values[]{0xaa,0x55,0xa0};
                check(commands<3&&value==values[commands],"Flash command order changed");++commands;
            };
            check(!task.advanceBefore(cost-1,read,write)&&!observations&&task.stage()==stage,
                  "Program observed IO before its deadline");
            check(task.advanceBefore(cost,read,write)==cost,"Program interval rejected");
            ++boundaries;
        }
        const bool writing=mode>=4;
        check(commands==(writing?3u:0u)&&writes==(writing?256u:0u)&&reads==writes,"Page write admission changed");
        if(writing)check(pending==uint16_t((seed&1?65535:seed)+1),"Pending count did not wrap");
        check(task.target()==(mode==1?target+256:mode==2||mode==3||mode==5||mode==8?target|0x80000000u:target+256),"Program result changed");
        check(!task.resumeCompare(0,0),"Completed program accepted a child");
        ++programs;
    }
    std::cout<<"PASS: "<<comparisons<<" page comparisons, "<<programs<<" program policies, "<<boundaries
             <<" admitted intervals; live bytes/first difference/commands/protection/timeout/wrap/deadlines\n";
    return 0;
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; } }
