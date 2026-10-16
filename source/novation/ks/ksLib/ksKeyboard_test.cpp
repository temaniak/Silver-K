#include "ksKeyboard.h"
#include "ksRom.h"
#include "h8s/h8s.hpp"
#include <algorithm>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <limits>

namespace {
void check(bool v,const char* text){if(!v)throw std::runtime_error(text);}
// Deliberately simple public specification, separate from byte traversal and
// the timing calculation. This is not the firmware timing/value oracle.
int adjacent(const ks::nativeControl::KeyBitmap& keys,unsigned key,bool next){
    if(key>=128)return -1;
    for(int n=int(key)+(next?1:-1);n>=0 && n<128;n+=next?1:-1)
        if(keys[unsigned(n)/8]&(1u<<(unsigned(n)%8)))return n;
    return -1;
}
struct ReadLog final:H8SDevice {
    struct Observation {unsigned index;uint64_t clock;};
    std::vector<Observation> observations;
    unsigned base=0;const ks::nativeControl::KeyBitmap* bitmap=nullptr;
    uint8_t read(uint32_t a)override{observations.push_back({a-base,state->cycles});return (*bitmap)[a-base];}
    void write(uint32_t,uint8_t)override{throw std::runtime_error("Original key search wrote keyboard");}
};
struct Original final:H8SEmulator {using H8SEmulator::lastread;using H8SEmulator::lastwrite;using H8SEmulator::indent;};
}
int main(int argc,char** argv){try{
    using namespace ks::nativeControl;
    std::vector<KeyBitmap> bitmaps(1);
    KeyBitmap full{};full.fill(255);bitmaps.push_back(full);
    for(unsigned note=0;note<128;++note){KeyBitmap single{};single[note/8]=uint8_t(1u<<(note%8));bitmaps.push_back(single);}
    // Exhaust each possible byte at each bitmap position, with other bytes
    // both empty and set. This covers byte skips, crossing and endpoint ties.
    for(unsigned byte=0;byte<16;++byte)for(unsigned value=0;value<256;++value){
        KeyBitmap sparse{},dense=full;sparse[byte]=dense[byte]=uint8_t(value);
        bitmaps.push_back(sparse);bitmaps.push_back(dense);
    }
    std::mt19937 rng(0x4b534b59);
    for(unsigned sample=0;sample<512;++sample){KeyBitmap b{};for(auto& v:b)v=uint8_t(rng());bitmaps.push_back(b);}
    uint64_t values=0;
    for(const auto& b:bitmaps)for(unsigned key=0;key<128;++key)for(bool next:{false,true}){
        const auto v=next?nextKey(b,key):previousKey(b,key);
        check(v.note==adjacent(b,key,next),"Native adjacent-key value differs");
        check(v.clocks>=33 && v.boundaries>=11,"Native adjacent-key duration bounds");
        KeySearchTask task(next?KeySearchTask::Direction::Next:KeySearchTask::Direction::Previous,key);
        // Varied small budgets cross byte reads and arithmetic admission. A
        // zero-sized slice is also a pause, not an implicit data observation.
        unsigned polls=0,reads=0;
        auto read=[&](unsigned i){check(i<16,"Native task read outside keyboard");++reads;return b[i];};
        while(!task.complete()){
            const auto paused=task.result();check(task.advanceBefore(0,read)==0,"Pause consumed clocks");
            check(task.result().clocks==paused.clocks && task.result().boundaries==paused.boundaries,"Pause changed task");
            task.advanceBefore((polls++%17)+1,read);check(polls<4096,"Native key task stalled");
        }
        const auto timed=task.result();
        check(timed.note==v.note && timed.clocks==v.clocks && timed.boundaries==v.boundaries,"Native timed key search differs");
        check(reads>=1 && reads<=16 && !task.advanceBefore(std::numeric_limits<unsigned>::max(),read),"Native completed task observed data");++values;
    }
    check(nextKey(full,128).note==-1 && !nextKey(full,128).clocks
        && previousKey(full,128).note==-1 && !previousKey(full,128).clocks,"Invalid key touched bitmap");
    if(argc==1){std::cout<<"PASS: "<<values<<" native key-search value cases; independent firmware timing requires private KS OS\n";return 0;}
    check(argc==2,"Expected optional private KS OS path");ks::Rom os(argv[1]);check(os.isValid(),"Invalid KS OS");
    auto h=std::make_unique<Original>();h->loadmem(os.data().data(),unsigned(os.data().size()),0);
    uint64_t cases=0,steps=0;
    constexpr unsigned base=0xa16192,stack=0xfff000,returned=0x4338;
    ReadLog observed;observed.base=base;observed.observations.reserve(16);h->memmap(&observed,base,16);
    auto verify=[&](const KeyBitmap& initial,unsigned key,bool next,unsigned mutateAfter){
        KeyBitmap active=initial;observed.bitmap=&active;
        std::copy(active.begin(),active.end(),h->memory+base);
        const auto staticResult=next?nextKey(active,key):previousKey(active,key);
        for(unsigned i=0;i<8;++i)h->regs[i].er=int32_t(rng());
        h->regs[3].er=base;h->regs[5].rl=int8_t(key);
        const auto input=h->regs[5].er,retained=h->regs[3].er;
        h->setSP(stack);h->memory[stack]=0;h->memory[stack+1]=0;
        h->memory[stack+2]=uint8_t(returned>>8);h->memory[stack+3]=uint8_t(returned);
        h->pc=h->makepc(next?0x317a:0x31ac);h->ccr=uint8_t(rng());h->exr=0;
        h->cycles=1000;h->lastread=int(rng());h->lastwrite=int(rng());h->indent=0;
        const int lastwrite=h->lastwrite;h->pending_irqs=h->pending_irqs_high=0;h->interruptMode2=true;
        observed.observations.clear();
        KeySearchTask task(next?KeySearchTask::Direction::Next:KeySearchTask::Direction::Previous,key);
        unsigned nativeReads=0,count=0;
        do{
            const auto before=h->cycles;h->step();check(++count<2048,"Original key search did not return");
            if(h->getPC()!=returned){
                const auto cost=unsigned(h->cycles-before);
                check(task.nextBoundaryClocks()==cost,"Native key task boundary clock differs from original");
                const auto consumed=task.advanceBefore(cost,[&](unsigned index){
                    check(nativeReads<observed.observations.size(),"Native task read before original");
                    const auto& o=observed.observations[nativeReads++];
                    check(o.index==index && o.clock==h->cycles,"Native key read order/clock differs from original");
                    return active[index];
                });
                check(consumed==cost && task.result().clocks==h->cycles-1000,"Native key task clock differs");
                if(count==mutateAfter){
                    // Model another handler changing live keys at a legal
                    // preemption point. An already observed byte must stay
                    // latched; bytes not read yet must use the changed data.
                    for(unsigned i=0;i<16;++i)active[i]^=uint8_t(0x81u+(i*13u+key)*7u);
                    std::copy(active.begin(),active.end(),h->memory+base);
                }
            }
        }while(h->getPC()!=returned);
        const auto timed=task.result();const int keyResult=h->regs[4].rl<0?-1:int(h->regs[4].rl);
        check(task.complete() && nativeReads==observed.observations.size(),"Native task completion/read count differs");
        if(keyResult!=timed.note || h->cycles!=1000+timed.clocks+17 || count!=timed.boundaries+1){
            std::cerr<<"key="<<key<<" next="<<next<<" mutation="<<mutateAfter<<" note="<<keyResult<<'/'<<timed.note
                <<" clocks="<<(h->cycles-1017)<<'/'<<timed.clocks<<" boundaries="<<(count-1)<<'/'<<timed.boundaries<<'\n';
            throw std::runtime_error("Native search differs from independent original execution");
        }
        if(!mutateAfter)check(timed.note==staticResult.note && timed.clocks==staticResult.clocks
            && timed.boundaries==staticResult.boundaries,"Atomic/timed key search differs from original");
        check(h->regs[5].er==input && h->regs[3].er==retained && h->getSP()==stack+4
            && h->lastwrite==lastwrite,"Original search unexpectedly changed retained input or memory");
        check(std::equal(active.begin(),active.end(),h->memory+base),"Original search changed held keys");
        ++cases;steps+=count;return count-1;
    };
    for(const auto& b:bitmaps)for(unsigned key=0;key<128;++key)for(bool next:{false,true})verify(b,key,next,0);
    uint64_t mutationCases=0;
    for(unsigned pattern:{0u,1u,2u,9u,65u,129u,130u,unsigned(bitmaps.size()-1)}){
        for(unsigned key:{0u,1u,7u,8u,31u,63u,64u,120u,127u})for(bool next:{false,true}){
            const auto count=verify(bitmaps[pattern],key,next,0);
            for(unsigned point=1;point<=count;++point){verify(bitmaps[pattern],key,next,point);++mutationCases;}
        }
    }
    std::cout<<"PASS: "<<cases<<" native key-search values/durations/live read timelines, "<<steps
        <<" independently executed H8 boundaries; all keys, sparse/dense exhaustive bytes, random retained flags; "<<mutationCases<<" live-state mutation points; native core not yet connected to playback\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
