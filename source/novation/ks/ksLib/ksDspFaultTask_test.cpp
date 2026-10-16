#include "ksDspFaultTask.h"
#include <array>
#include <iostream>
#include <stdexcept>
namespace {void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}}
int main(){try{
    using Task=ks::DspFaultTask;uint64_t cases=0;
    auto verify=[&](unsigned latch,unsigned first,unsigned last,unsigned budget){
        Task task;unsigned reads=0,writes=0;std::array<uint8_t,5> output{};
        auto read=[&](Task::Read input){
            if(!reads)check(input==Task::Read::Latch,"Native latch observation order differs");else check(input==Task::Read::Control && reads<3,"Native control observation order differs");
            return uint8_t(reads++==0?latch:reads==2?first:last);
        };
        auto write=[&](Task::Write target,uint8_t value){
            constexpr Task::Write targets[]{Task::Write::Latch,Task::Write::Data,Task::Write::Control,Task::Write::Control,Task::Write::Control};
            check(writes<5 && target==targets[writes],"Native fault output order differs");output[writes++]=value;
        };
        check(task.advanceBefore(budget,read,write).clocks<=budget,"Native fault crossed deadline");
        for(unsigned iteration=0;!task.complete();++iteration){
            check(iteration<32,"Native fault task stalled");const unsigned cost=task.nextBoundaryClocks(),before=task.elapsedClocks(),r=reads,w=writes;
            check(!task.advanceBefore(cost-1,read,write).boundaries && task.elapsedClocks()==before && reads==r && writes==w,"Premature fault observation/output");
            task.advanceBefore(cost,read,write);
        }
        check(task.elapsedClocks()==159 && reads==3 && writes==5 && task.latch()==uint8_t(latch^7) && !task.remainingTicks(),"Native fault duration/state differs");
        check(output==std::array<uint8_t,5>{uint8_t(latch^7),uint8_t(latch^7),0x36,uint8_t(first&~0x20),uint8_t(last|0x20)},"Native fault pulse values differ");++cases;
    };
    for(unsigned latch=0;latch<256;++latch)for(unsigned control:{0u,1u,0x1fu,0x20u,0x36u,0x7fu,0x80u,0xffu})for(unsigned budget=0;budget<33;++budget)verify(latch,control,control^0xa5,budget);
    for(unsigned first=0;first<256;++first)for(unsigned last=0;last<256;++last)verify(0xa5,first,last,31);
    std::cout<<"PASS: "<<cases<<" native fault latch/control/deadline cases; both live control reads, pulse order, three-tick delay and exact calibrated duration; no CPU state\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
