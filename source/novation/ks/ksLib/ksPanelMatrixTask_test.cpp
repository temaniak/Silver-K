#include "ksPanelMatrixTask.h"
#include <array>
#include <iostream>
#include <stdexcept>
namespace {void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}}
int main(){try{
    using T=ks::PanelMatrixTask;using S=T::Stage;uint64_t cases=0,intervals=0;
    for(unsigned previous=0;previous<256;++previous)for(unsigned sample=0;sample<256;++sample){
        T task(T::Kind::Scan);std::array<uint16_t,48> events{};unsigned count=0,settled=0,rows=0,gateWrites=0;
        auto read=[&](T::Read field)->uint8_t{
            check(settled==12,"Panel row sampled before settling");
            return uint8_t(field==T::Read::Previous?previous:sample);
        };
        auto write=[&](T::Write field,uint8_t value){
            if(field==T::Write::Mux){check(value==(rows<6?0x20+rows:0x30),"Panel mux order differs");settled=0;++rows;}
            if(field==T::Write::Gate){check(value==(gateWrites?255:0),"Panel gate order differs");++gateWrites;}
            if(field==T::Write::History)check(value==sample,"Panel history differs");
        };
        while(!task.complete()){
            if(task.awaitingChild()){
                check(count<events.size(),"Panel event overflow");events[count++]=uint16_t((task.pressed()<<8)|task.key());
                check(task.resumeChild(),"Panel event cannot resume");continue;
            }
            const auto before=task.stage();const unsigned storeClocks=task.stage()==S::WriteHistory?3:1;
            const auto cost=task.nextClocks(storeClocks);
            check(cost&&!task.advanceBefore(cost-1,read,write,storeClocks)&&task.stage()==before,"Short panel deadline changed state");
            check(task.advanceBefore(cost,read,write,storeClocks)==cost,"Exact panel deadline refused");
            if(before==S::DecrementDelay)++settled;++intervals;
        }
        unsigned expected=0;
        for(unsigned row=0;row<6;++row)for(unsigned bit=0;bit<8;++bit)if((previous^sample)&(1u<<bit)){
            const auto event=uint16_t((((sample&(1u<<bit))?0:1)<<8)|(row*8+bit));
            check(events[expected++]==event,"Panel physical event order/value differs");
        }
        check(count==expected&&rows==7&&gateWrites==2,"Panel final counts differ");
        check(!task.advanceBefore(100,read,write),"Completed panel scan ran again");++cases;
    }
    T initialize(T::Kind::Initialize);unsigned children=0;
    while(!initialize.complete()){
        if(initialize.awaitingChild()){++children;initialize.resumeChild();continue;}
        const auto cost=initialize.nextClocks();check(initialize.advanceBefore(cost,[](T::Read){return 0;},[](T::Write,uint8_t){})==cost,"Panel initialization refused");
    }
    check(children==2,"Initialization did not scan then reset its queue");
    std::cout<<"PASS: "<<++cases<<" matrix policies / "<<intervals<<" intervals; all row-byte pairs, six-row active-low order, settling/gates/children/deadlines\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
