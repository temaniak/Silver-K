#include "ksPanelAdcSamplesTask.h"
#include <array>
#include <iostream>
#include <stdexcept>
namespace {void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}}
int main(){try{
    using T=ks::PanelAdcSamplesTask;uint64_t cases=0,intervals=0;
    for(unsigned raw=0;raw<65536;++raw)for(unsigned previous:{0u,1u,2u,255u,256u,32767u,32768u,65534u,65535u}){
        T task;std::array<uint16_t,4> written{};unsigned reads=0,writes=0;
        auto sample=[&](unsigned channel){return uint16_t(raw+channel*149);};
        auto old=[&](unsigned channel){return uint16_t(previous+channel*257);};
        auto read=[&](T::Read field)->uint16_t{
            if(field==T::Read::Group)return uint8_t(raw);
            check(task.offset()==uint8_t(uint8_t(raw)<<1),"ADC group index does not wrap as a byte");
            check(task.channel()<4,"ADC channel out of range");
            if(field==T::Read::High){check(reads%3==0,"ADC high byte order differs");++reads;return sample(task.channel())>>8;}
            if(field==T::Read::Low){check(reads%3==1,"ADC low byte order differs");++reads;return sample(task.channel())&255;}
            check(reads%3==2,"ADC retained sample order differs");++reads;return old(task.channel());
        };
        auto write=[&](uint16_t value){check(task.channel()==writes&&writes<4,"ADC write order differs");written[writes++]=value;};
        while(!task.complete()){
            const auto stage=task.stage();const auto cost=task.nextClocks();
            check(cost&&!task.advanceBefore(cost-1,read,write)&&task.stage()==stage,"Short ADC deadline changed state");
            check(task.advanceBefore(cost,read,write)==cost,"Exact ADC deadline refused");++intervals;
        }
        for(unsigned channel=0;channel<4;++channel)check(written[channel]==(sample(channel)/2+old(channel)/2),"ADC half-products/truncation differ");
        check(reads==12&&writes==4&&!task.advanceBefore(100,read,write),"ADC completion/count differs");++cases;
    }
    std::cout<<"PASS: "<<cases<<" ADC smoothing policies / "<<intervals<<" intervals; all raw words, retained edges, byte-wrapped group, ordered four channels, separate half-products and deadlines\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
