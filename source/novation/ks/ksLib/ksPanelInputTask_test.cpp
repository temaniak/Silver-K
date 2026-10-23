#include "ksPanelInputTask.h"
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
}
int main(){try{
    using T=ks::PanelInputTask;using S=T::Stage;using K=T::Kind;
    uint64_t cases=0,intervals=0;
    for(unsigned kind=0;kind<7;++kind)for(unsigned index=0;index<65536;++index){
        const auto k=K(kind);const uint16_t event=uint16_t(index*149+0x1234);
        T task(k,event);const uint16_t consumer=uint16_t(index),producer=uint16_t(index+(index&1?0:2));
        uint16_t readIndex=consumer,writeIndex=producer,published=0;
        unsigned eventWrites=0,indexWrites=0,eventReads=0,children=0;
        const auto mapping=uint8_t(index>>8);const uint16_t queued=uint16_t(event^0xa55a);
        auto read=[&](T::Read field)->uint16_t{
            switch(field){case T::Read::Consumer:return readIndex;case T::Read::Producer:return writeIndex;
            case T::Read::Mapping:check(task.key()==uint8_t(event),"Input key was not retained");return mapping;
            case T::Read::Event:check(task.eventIndex()==consumer,"Event location changed after consumer publication");check(readIndex==uint16_t((consumer+2)&task.mask()),"Event read before consumer publication");++eventReads;return queued;}return 0;
        };
        auto write=[&](T::Write field,uint16_t value){
            switch(field){case T::Write::Consumer:readIndex=value;++indexWrites;break;
            case T::Write::Producer:writeIndex=value;++indexWrites;break;
            case T::Write::Event:check(task.eventIndex()==producer,"Producer location was not retained");published=value;++eventWrites;break;}
        };
        unsigned limit=0;
        while(!task.complete()){
            if(task.awaitingControl()){check(!task.nextClocks(),"Awaiting child consumed time");check(task.resumeControl(),"Cannot resume control child");++children;continue;}
            check(!task.resumeControl(),"Resumed absent child");
            const auto stage=task.stage();const unsigned eventClocks=index&1?6:3;const auto cost=task.nextClocks(eventClocks);
            check(cost&&task.advanceBefore(cost-1,read,write,eventClocks)==0&&task.stage()==stage,"Short deadline changed task");
            check(task.advanceBefore(cost,read,write,eventClocks)==cost,"Exact deadline refused task");
            check(++limit<80,"Input task stalled");++intervals;
        }
        check(!task.advanceBefore(100,read,write),"Completed task ran again");
        if(task.reset())check(!readIndex&&!writeIndex&&indexWrites==2&&!eventWrites&&!eventReads,"Reset policy differs");
        else if(task.pop()){
            const bool empty=consumer==producer;
            check(readIndex==(empty?consumer:uint16_t((consumer+2)&task.mask()))&&writeIndex==producer,"Pop index policy differs");
            check(indexWrites==unsigned(!empty)&&eventReads==unsigned(!empty)&&!eventWrites,"Pop accesses differ");
            check(children==unsigned(!empty&&k==K::ControlPop),"Control conversion policy differs");
            if(!empty)check(task.event()==queued,"Popped event differs");
        }else{
            const bool ignored=task.controls()&&(mapping&128);
            check(readIndex==consumer&&writeIndex==(ignored?producer:uint16_t((producer+2)&task.mask())),"Push index policy differs");
            check(eventWrites==unsigned(!ignored)&&indexWrites==unsigned(!ignored)&&!eventReads,"Push accesses differ");
            if(!ignored)check(published==uint16_t((event&0xff00)|mapping),"Mapped event differs");
        }
        ++cases;
    }
    std::cout<<"PASS: "<<cases<<" panel ring policies / "<<intervals<<" intervals; all 16-bit indices, both masks, empty/retained/full mappings, publication order, children and short deadlines\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
