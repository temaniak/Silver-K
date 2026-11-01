#include "ksStateChannelTask.h"
#include <iostream>
#include <stdexcept>

namespace {void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}}
int main(){try{
    using T=ks::StateChannelTask;uint64_t cases=0,boundaries=0;
    for(unsigned kind=0;kind<3;++kind)for(unsigned a=0;a<256;++a)for(unsigned b=0;b<256;++b)for(unsigned transfer:{0u,3u}){
        T task{T::Kind(kind)};unsigned reads=0,writes=0,children=0;uint8_t globalChannel=uint8_t(b),selected=uint8_t(a);
        auto read=[&](T::Read field)->unsigned{
            ++reads;
            switch(field){
            case T::Read::Mode:return a;
            case T::Read::MultiStatus:return b;
            case T::Read::GlobalChannel:return globalChannel;
            case T::Read::SelectedPart:return selected;
            case T::Read::SelectedChannel:check(task.part()==a,"Selected part was reread after lookup");return b;
            }return 0;
        };
        auto write=[&](T::Write field,uint8_t value){
            ++writes;
            if(field==T::Write::Message)check(value==0x91,"Mode message changed");
            else {check(value==b,"Channel mirror did not retain its observed byte");globalChannel^=255;}
        };
        while(!task.complete()){
            if(task.awaitingControls()){++children;selected^=255;check(task.resumeControls(),"Lookup child resume");continue;}
            const auto cost=task.nextClocks(transfer);const auto stage=task.stage();const auto oldReads=reads,oldWrites=writes;
            check(!task.advanceBefore(cost-1,read,write,transfer)&&stage==task.stage()&&reads==oldReads&&writes==oldWrites,"State channel early observation");
            check(task.advanceBefore(cost,read,write,transfer)==cost,"State channel admission");++boundaries;
        }
        if(kind==0)check(writes==unsigned(!a||b)&&reads==1+unsigned(a!=0),"Mode/status truth table");
        if(kind==1)check(reads==1&&writes==2&&task.channel()==b,"Global channel lifecycle");
        if(kind==2)check(reads==2&&!writes&&children==1&&task.channel()==b,"Selected channel lifecycle");
        check(!task.resumeControls(),"Completed state channel resumed");++cases;
    }
    std::cout<<"PASS: "<<cases<<" mode/status/channel/part policies, "<<boundaries<<" intervals; all byte pairs, live reads, retained mirrors, both publication costs, deadlines\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
