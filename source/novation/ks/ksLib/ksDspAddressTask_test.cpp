#include "ksDspAddressTask.h"
#include <array>
#include <iostream>
#include <stdexcept>
namespace {void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}}
int main(){try{
    using Task=ks::DspAddressTask;uint64_t cases=0;
    for(unsigned position=0;position<2;++position)for(unsigned byte=0;byte<256;++byte)
    for(bool receive:{false,true})for(auto port:{Task::PortSelection::Keep,Task::PortSelection::A,Task::PortSelection::B})
    for(unsigned busy:{0u,1u,2u,7u})for(unsigned budget=0;budget<32;++budget){
        const uint16_t address=uint16_t(0x5aa5u^(byte<<(position*8)));const uint32_t word=0x8f00ff;
        Task task(address,receive,port);std::array<unsigned,2> reads{},busyReads{busy,7-busy};unsigned phase=0,writes=0,dataReads=0;
        auto read=[&](unsigned offset){
            if(offset==4){const unsigned mask=phase && receive?1:4;return uint8_t(reads[phase]++<busyReads[phase]?255&~mask:255);}
            check(receive && writes==5 && dataReads<3 && offset==10+2*dataReads,"Native reply read order differs");return uint8_t(word>>(16-8*dataReads++));
        };
        auto write=[&](unsigned offset,uint8_t value){
            constexpr unsigned offsets[]{0,10,12,14,0};const uint8_t values[]{8,uint8_t(receive?0x80:0),uint8_t(address>>8),uint8_t(address),0};
            check(writes<5 && offset==offsets[writes] && value==values[writes],"Native request order/value differs");if(++writes==4)phase=1;
        };
        check(task.advanceBefore(budget,read,write).clocks<=budget,"Native address task crossed deadline");
        for(unsigned iteration=0;!task.complete();++iteration){
            check(iteration<200,"Native address task stalled");const unsigned cost=task.nextBoundaryClocks();const auto time=task.elapsedClocks();const auto observed=reads;const auto w=writes,d=dataReads;
            check(!task.advanceBefore(cost-1,read,write).boundaries && time==task.elapsedClocks() && reads==observed && writes==w && dataReads==d,"Premature native request/reply access");
            task.advanceBefore(cost,read,write);
        }
        auto delay=[](unsigned n){return n?24+(n-1)*27:0;};
        const unsigned selection=port==Task::PortSelection::Keep?0:port==Task::PortSelection::B?12:9;
        check(task.elapsedClocks()==126+(receive?45:0)+selection+delay(busyReads[0])+delay(busyReads[1]),"Native address/reply duration differs");
        check(reads==std::array<unsigned,2>{busyReads[0]+1,busyReads[1]+1} && writes==5 && dataReads==(receive?3:0),"Native request/reply counts differ");
        if(receive)check(task.result()==word,"Native reply assembly differs");++cases;
    }
    for(bool second:{false,true}){
        Task task(0xffff,true);unsigned phase=0,reads[2]{},faults=0;const unsigned busy[2]{second?0u:65537u,second?65537u:0u};
        while(!task.complete()){
            const auto progress=task.advanceBefore(31,[&](unsigned offset){if(offset!=4)return uint8_t(0xab);const unsigned mask=phase?1:4;return uint8_t(reads[phase]++<busy[phase]?255&~mask:255);},[&](unsigned offset,uint8_t){if(offset==14)phase=1;});
            if(task.stage()==Task::Stage::AwaitFault){++faults;const auto time=task.elapsedClocks();check(!task.advanceBefore(1000,[](unsigned){return 255;},[](unsigned,uint8_t){}).boundaries && task.elapsedClocks()==time,"Native fault did not park");check(task.resumeFault() && !task.resumeFault(),"Native fault resume differs");}
            else check(progress.boundaries || task.complete(),"Native dual-phase timeout stalled");
        }
        check(faults==1 && task.elapsedClocks()==1769693 && task.result()==0xababab,"Native dual-phase timeout/reply differs");
    }
    std::cout<<"PASS: "<<cases<<" native address/read payload/deadline/dual-readiness/port-selection cases and complete timeout/fault park/resume in each phase; no CPU state\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
