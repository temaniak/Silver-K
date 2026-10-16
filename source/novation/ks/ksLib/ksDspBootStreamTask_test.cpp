#include "ksDspBootStreamTask.h"
#include <iostream>
#include <stdexcept>
namespace {void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}}
int main(){try{
    using Task=ks::DspBootStreamTask;uint64_t cases=0;
    auto verify=[&](uint16_t input,unsigned firstBusy,unsigned laterBusy,unsigned seed,unsigned initialBudget,bool mutate){
        Task task(input);const uint32_t words=input?input:65536u;unsigned readsInWord=0,observations=0,sourceReads=0,writes=0,faults=0,generation=0;uint8_t latched=0;
        auto status=[&]{const unsigned busy=writes<3?firstBusy:laterBusy;++observations;return uint8_t(readsInWord++<busy?0xfd:0xff);};
        auto source=[&](uint32_t offset){check(offset==sourceReads && sourceReads==writes,"Firmware source was prefetched/reordered");++sourceReads;latched=uint8_t(seed+offset*37+generation*13+(offset>>8));return latched;};
        auto write=[&](unsigned offset,uint8_t value){check(offset==10+2*(writes%3) && sourceReads==writes+1 && value==latched,"Native firmware stream byte/order differs");if(!(++writes%3))readsInWord=0;};
        check(task.advanceBefore(initialBudget,status,source,write).clocks<=initialBudget,"Bootstrap stream crossed deadline");
        uint64_t iterations=0;
        while(!task.complete()){
            if(mutate)++generation;
            const unsigned cost=task.nextBoundaryClocks();const auto time=task.elapsedClocks();const auto r=observations,s=sourceReads,w=writes;
            check(!task.advanceBefore(cost-1,status,source,write).boundaries && task.elapsedClocks()==time && r==observations && s==sourceReads && w==writes,"Premature bootstrap stream observation/output");
            task.advanceBefore(cost,status,source,write);
            if(task.stage()==Task::Stage::AwaitFault){
                ++faults;const auto pause=task.elapsedClocks();check(!task.advanceBefore(1000,status,source,write).boundaries && pause==task.elapsedClocks(),"Bootstrap fault did not park");check(task.resumeFault() && !task.resumeFault(),"Bootstrap fault resume differs");
            }
            check(++iterations<uint64_t(words)*100+2000000,"Native bootstrap stream stalled");
        }
        auto delay=[](unsigned busy)->uint64_t{
            if(!busy)return 0;
            const unsigned faults=(busy-1)/65536;
            // First busy observation/reset costs24; each subsequent busy
            // retry costs27, with20 dispatch+3 resume+3 reset per full timeout.
            return 24ull+uint64_t(busy-1)*27+uint64_t(faults)*26;
        };
        const uint64_t expected=uint64_t(words)*96+delay(firstBusy)+uint64_t(words-1)*delay(laterBusy);
        check(task.elapsedClocks()==expected && !task.remainingWords() && task.bytesConsumed()==words*3 && sourceReads==words*3 && writes==words*3,"Native firmware stream duration/count/cursor differs");
        check(faults==(firstBusy?(firstBusy-1)/65536:0)+(words-1)*(laterBusy?(laterBusy-1)/65536:0),"Native firmware timeout count differs");
        check(observations==firstBusy+1+(words-1)*(laterBusy+1),"Native firmware readiness read count differs");++cases;
    };
    for(unsigned input:{1u,2u,3u,7u,17u})for(unsigned seed=0;seed<256;++seed)for(unsigned busy:{0u,1u,2u,7u})for(unsigned budget=0;budget<32;++budget)
        verify(uint16_t(input),busy,7-busy,seed,budget,true);
    for(unsigned input:{0u,255u,256u,257u,32767u,32768u,65535u})for(unsigned busy:{0u,1u,2u})verify(uint16_t(input),busy,busy,0xa5,31,true);
    for(unsigned busy:{65536u,65537u,131073u})verify(2,busy,0,0xa5,31,true);
    std::cout<<"PASS: "<<cases<<" native firmware-stream source/byte/readiness/deadline cases, zero/full 16-bit counts, live mutation, wrapped counts and complete timeout/fault resume; no CPU state\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
