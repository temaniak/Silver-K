#include "ksPedalSenseTask.h"
#include <iostream>
#include <stdexcept>
namespace {void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}}
int main(){try{
    using T=ks::PedalSenseTask;uint64_t cases=0,intervals=0;
    for(unsigned polarity=0;polarity<256;++polarity)for(unsigned port=0;port<256;++port)for(unsigned history:{0u,1u,63u,64u,127u,128u,191u,255u}){
        T task(T::Kind::Pedal);uint8_t stored=0;unsigned writes=0;
        auto read=[&](T::Read field)->uint8_t{return uint8_t(field==T::Read::Polarity?polarity:field==T::Read::Port?port:history);};
        auto write=[&](uint8_t value){stored=value;++writes;};
        while(!task.complete()){
            const auto stage=task.stage();const auto cost=task.nextClocks();
            check(cost&&!task.advanceBefore(cost-1,read,write)&&task.stage()==stage,"Short pedal deadline changed state");
            check(task.advanceBefore(cost,read,write)==cost,"Exact pedal deadline refused");++intervals;
        }
        const uint8_t normalized=uint8_t(port^((polarity>>2)|(polarity<<6)));
        const uint8_t expected=((history^normalized)&64)?(normalized&64?0:127):128;
        check(writes==1&&stored==normalized&&task.result()==expected,"Pedal edge/polarity/history differs");++cases;
    }
    for(unsigned value=0;value<256;++value)for(auto kind:{T::Kind::Switches,T::Kind::Reset}){
        T task(kind);unsigned writes=0;
        while(!task.complete()){const auto cost=task.nextClocks();task.advanceBefore(cost,[&](T::Read){return value;},[&](uint8_t v){check(v==255,"Pedal reset value differs");++writes;});++intervals;}
        check(kind==T::Kind::Switches?task.switches()==(value&0x30)&&!writes:writes==1,"Switch/reset policy differs");++cases;
    }
    std::cout<<"PASS: "<<cases<<" switch/pedal policies / "<<intervals<<" intervals; all polarity/port byte pairs, retained-history edges and deadlines\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
