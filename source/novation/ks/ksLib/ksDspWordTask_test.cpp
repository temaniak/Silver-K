#include "ksDspWordTask.h"
#include <array>
#include <iostream>
#include <stdexcept>
namespace {void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}}
int main(){try{
    uint64_t cases=0;
    for(unsigned position=0;position<3;++position)for(unsigned byte=0;byte<256;++byte)
    for(bool post:{false,true})for(unsigned busy:{0u,1u,2u,7u})for(unsigned budget=0;budget<32;++budget){
        const uint32_t word=0xa55ac3u^(byte<<(position*8));ks::DspWordTask task(word,post,0xffff);
        unsigned reads=0,writes=0,publishes=0,faults=0;std::array<uint8_t,3> bytes{};
        auto read=[&]{return uint8_t(reads++<busy?0xfc:0xfe);};
        auto write=[&](unsigned offset,uint8_t v){check(offset==10+2*writes,"Native byte order differs");bytes[writes++]=v;};
        auto publish=[&](uint16_t address){check(!address,"Native address wrap differs");++publishes;};
        const auto first=task.advanceBefore(budget,read,write,publish);
        check(first.clocks<=budget,"Native task crossed deadline");
        for(unsigned iteration=0;!task.complete();++iteration){
            check(iteration<100,"Native word task stalled");const unsigned cost=task.nextBoundaryClocks();
            const auto before=task.elapsedClocks();const auto r=reads,w=writes,p=publishes;
            check(!task.advanceBefore(cost-1,read,write,publish).boundaries,"Incomplete interval advanced");
            check(before==task.elapsedClocks() && r==reads && w==writes && p==publishes,"Premature native port access");
            task.advanceBefore(cost,read,write,publish);
            if(task.stage()==ks::DspWordTask::Stage::AwaitFault){++faults;task.resumeFault();}
        }
        check(bytes==std::array<uint8_t,3>{uint8_t(word>>16),uint8_t(word>>8),uint8_t(word)},"Native payload differs");
        check(writes==3 && reads==busy+1 && publishes==unsigned(post) && !faults,"Native word access count differs");
        const unsigned wait=busy?24u+(busy-1)*27u:0;
        check(task.elapsedClocks()==69u+wait+(post?15u:0u),"Native word duration differs");++cases;
    }
    // Readiness is latched only for this admission; subsequent actual reads
    // remain live. Changing the port between read and branch cannot refit it.
    ks::DspWordTask latched(0x123456);unsigned reads=0;
    auto noWrite=[](unsigned,uint8_t){};auto noPublish=[](uint16_t){};
    latched.advanceBefore(15,[&]{++reads;return 2;},noWrite,noPublish);
    latched.advanceBefore(6,[&]{++reads;return 0;},noWrite,noPublish);
    check(reads==1 && latched.stage()==ks::DspWordTask::Stage::Prepare,"Readiness was re-read or lost");
    ks::DspWordTask timeout(0xabcdef);unsigned busy=65537,observations=0,faults=0;
    while(!timeout.complete()){
        const auto n=timeout.advanceBefore(31,[&]{return uint8_t(observations++<busy?0:2);},noWrite,noPublish);
        if(timeout.stage()==ks::DspWordTask::Stage::AwaitFault){
            ++faults;const auto time=timeout.elapsedClocks();
            check(!timeout.advanceBefore(1000,[]{return 2;},noWrite,noPublish).boundaries && time==timeout.elapsedClocks(),"Fault did not yield");
            check(timeout.resumeFault() && !timeout.resumeFault(),"Fault acknowledgement differs");
        }else check(n.boundaries || timeout.complete(),"Native timeout task stalled");
    }
    check(faults==1 && observations==65538 && timeout.elapsedClocks()==1769591,"Native timeout timing/count differs");
    std::cout<<"PASS: "<<cases<<" native DSP-word payload/deadline/readiness/address cases, latched admission and complete timeout/fault park/resume; no processor state\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
