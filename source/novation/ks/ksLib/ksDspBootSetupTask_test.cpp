#include "ksDspBootSetupTask.h"
#include <iostream>
#include <stdexcept>
namespace {void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}}
int main(){try{
    using Task=ks::DspBootSetupTask;using R=Task::Request;uint64_t headers=0,cases=0;
    for(unsigned header=0;header<65536;++header){
        Task task;unsigned reads=0;
        const auto result=task.advanceBefore(44,[&](unsigned offset){check(offset==++reads,"Header reads reordered");return uint8_t(offset==1?header>>8:header);},[]{return 0;});
        check(result.clocks==44 && reads==2 && task.request()==R::Stream && task.headerSize()==header && task.streamWords()==uint16_t(header+2),"Native setup header/count decode differs");++headers;
    }
    for(unsigned busy:{0u,1u,2u,7u,65536u,65537u,131073u})for(unsigned budget:{0u,1u,2u,3u,5u,6u,8u,9u,10u,11u,14u,15u,19u,20u,23u,31u}){
        Task task;unsigned reads=0,statusReads=0,streams=0,addresses=0,expWords=0,recipWords=0,faults=0;uint32_t exp=0x40000000;
        auto header=[&](unsigned offset){check(offset==++reads,"Setup header observation order differs");return uint8_t(offset==1?0x7f:0xff);};
        auto status=[&]{return uint8_t(statusReads++<busy?0xff:0xef);};
        check(task.advanceBefore(budget,header,status).clocks<=budget,"Native setup crossed admission budget");
        uint64_t iterations=0;
        while(!task.complete()){
            const auto request=task.request();
            if(request!=R::None){
                const auto time=task.elapsedClocks();check(!task.advanceBefore(1000,header,status).boundaries && time==task.elapsedClocks(),"Parent progressed before child completion");
                if(request==R::Stream){check(task.streamWords()==0x8001,"Native setup stream count differs");++streams;}
                if(request==R::Address){check(task.tableAddress()==0x8600,"Native setup table address differs");++addresses;}
                if(request==R::ExponentialWord){check(expWords<512 && task.wordValue()==exp,"Native exponential table differs");exp+=uint32_t((uint64_t(0xb191)*uint16_t(exp>>16))/512);++expWords;}
                if(request==R::ReciprocalWord){check(recipWords<512 && task.wordValue()==uint32_t((0x800000ull<<16)/(512+recipWords)),"Native reciprocal table differs");++recipWords;}
                if(request==R::Fault)++faults;
                check(task.resumeChild() && !task.resumeChild(),"Native setup child acknowledgement differs");
            }else{
                const unsigned cost=task.nextBoundaryClocks();const auto before=task.elapsedClocks();const auto r=reads,s=statusReads;
                check(!task.advanceBefore(cost-1,header,status).boundaries && task.elapsedClocks()==before && r==reads && s==statusReads,"Premature setup observation/progress");
                task.advanceBefore(cost,header,status);
            }
            check(++iterations<2000000,"Native setup task stalled");
        }
        const uint64_t wait=busy?24ull+uint64_t(busy-1)*27+uint64_t((busy-1)/65536)*26:0;
        check(task.elapsedClocks()==279056+wait && streams==1 && addresses==1 && expWords==512 && recipWords==512 && statusReads==busy+1 && faults==(busy?(busy-1)/65536:0),"Native setup duration/request counts differ");
        check(!task.exponentialRemaining() && !task.reciprocalRemaining() && task.divisor()==1024 && !task.shiftRemaining() && !task.outerRemaining() && !task.innerRemaining(),"Native setup final counters differ");++cases;
    }
    std::cout<<"PASS: "<<headers<<" native header values and "<<cases<<" complete setup/table/deadline/readiness/fault cases; all generated512+512 words and279056 own clocks exact; no CPU state\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
