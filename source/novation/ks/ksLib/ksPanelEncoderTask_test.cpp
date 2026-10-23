#include "ksPanelEncoderTask.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace {void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}}
int main(){try{
    using T=ks::PanelEncoderTask;uint64_t cases=0,intervals=0;
    for(unsigned timer=0;timer<65536;++timer)for(unsigned seed:{0u,1u,4u,13u,14u,127u,128u,255u}){
        const uint8_t oldHalf=uint8_t((timer>>1)+seed),oldQuarter=uint8_t((timer>>2)+(seed&1?1:seed?255:0)),oldDelta=uint8_t(seed^128);
        uint8_t activity=uint8_t(seed),half=oldHalf,quarter=oldQuarter,delta=oldDelta;T task;
        auto read=[&](T::Read field)->uint16_t{return field==T::Read::Timer?uint16_t(timer):field==T::Read::HalfHistory?oldHalf:field==T::Read::Activity?uint8_t(seed):field==T::Read::QuarterHistory?oldQuarter:oldDelta;};
        auto write=[&](T::Write field,uint8_t value){if(field==T::Write::HalfHistory)half=value;else if(field==T::Write::Activity)activity=value;else if(field==T::Write::QuarterHistory)quarter=value;else delta=value;};
        while(!task.complete()){
            const auto stage=task.stage();const auto cost=task.nextClocks();
            check(cost&&!task.advanceBefore(cost-1,read,write)&&task.stage()==stage,"Short encoder deadline changed state");
            check(task.advanceBefore(cost,read,write)==cost,"Exact encoder deadline refused");++intervals;
        }
        uint8_t distance=uint8_t(oldHalf-uint8_t(timer>>1));if(distance&128)distance=uint8_t(0-distance);
        uint8_t expectedActivity=uint8_t(distance+seed);if(expectedActivity)expectedActivity=std::min<unsigned>(uint8_t(expectedActivity-1),13);
        unsigned gain=expectedActivity<4?1:expectedActivity-3;
        const uint8_t movement=uint8_t(oldQuarter-uint8_t(timer>>2));
        if(movement&&((oldDelta^movement)&128)){expectedActivity=0;gain=1;}
        const uint8_t result=movement?uint8_t(movement&128?gain:0-gain):0;
        check(half==uint8_t(timer>>1)&&quarter==uint8_t(timer>>2)&&activity==expectedActivity,"Encoder history/activity differs");
        check(delta==(movement?movement:oldDelta)&&task.result()==result,"Encoder direction/result differs");
        check(!task.advanceBefore(100,read,write),"Completed encoder ran again");++cases;
    }
    std::cout<<"PASS: "<<cases<<" encoder policies / "<<intervals<<" intervals; all timer words, wrapping/signed differences, decay/clipping/reversal/stationary and deadlines\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
