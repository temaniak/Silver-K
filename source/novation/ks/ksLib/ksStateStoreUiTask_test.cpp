#include "ksStateStoreUiTask.h"
#include <array>
#include <iostream>
#include <stdexcept>

namespace {void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}}
int main(){try{
    using T=ks::StateStoreUiTask;using C=T::Child;using W=T::Write;
    uint64_t cases=0,intervals=0;
    for(unsigned kind=0;kind<5;++kind)for(unsigned allowed=0;allowed<256;++allowed)for(unsigned mode=0;mode<256;++mode){
        T task{T::Kind(kind)};std::array<C,8> children{};unsigned count=0,pageWrites=0,dirtyWrites=0,categoryWrites=0;
        uint8_t page=0xaa,dirty=0x55;const auto category=uint8_t(allowed^mode);const uint32_t current=mode&1?0xfffffff0u:0x30000u;
        auto read=[&](T::Read field)->uint32_t{return field==T::Read::Current?current:field==T::Read::Category?category:mode;};
        auto write=[&](W field,uint8_t value){
            if(field==W::Page){page=value;++pageWrites;}
            else if(field==W::Dirty){dirty=value;++dirtyWrites;}
            else{check(task.current()==current&&value==category,"Store category was not retained");++categoryWrites;}
        };
        unsigned limit=0;
        while(!task.complete()&&!task.awaitingTail()){
            if(task.awaitingChild()){
                check(!task.nextClocks()&&!task.advanceBefore(100,read,write),"Awaiting store child consumed time");
                check(count<children.size(),"Store child count overflow");children[count++]=task.child();
                check(task.resumeChild(uint8_t(allowed)),"Store child did not resume");continue;
            }
            check(!task.resumeChild(),"Absent store child resumed");
            const auto stage=task.stage();const unsigned writeClocks=mode&1?0:3;const auto cost=task.nextClocks(writeClocks);
            check(cost&&!task.advanceBefore(cost-1,read,write,writeClocks)&&task.stage()==stage,"Short store deadline changed state");
            check(task.advanceBefore(cost,read,write,writeClocks)==cost,"Exact store deadline refused");++intervals;check(++limit<80,"Store UI stalled");
        }
        if(kind==0){check(count==3&&children[0]==C::SerializeMulti&&children[1]==C::SaveMulti&&children[2]==C::DisplayMulti,"Multi store order differs");check(pageWrites==1&&!page&&!dirtyWrites&&!categoryWrites&&task.tail()==T::Tail::ResetPanel,"Multi store publications differ");}
        if(kind==1){check(count==(mode?5u:3u)&&children[0]==C::PrepareSingle&&children[1]==C::SaveSingle&&children[2]==C::ResetPanel,"Single store order differs");if(mode)check(children[3]==C::ReloadMulti&&children[4]==C::RefreshName,"Single store reload order differs");check(pageWrites==1&&!page&&dirtyWrites==unsigned(mode!=0)&&(!dirtyWrites||!dirty)&&categoryWrites==1&&task.tail()==T::Tail::Display,"Single store publications differ");}
        if(kind==2){check(count==(allowed?2u:1u)&&children[0]==C::CheckProtection,"Protection call order differs");if(allowed)check(children[1]==C::RefreshName,"Program store name not refreshed");check(pageWrites==unsigned(allowed!=0)&&dirtyWrites==unsigned(allowed!=0)&&(!allowed||(page==1&&!dirty))&&!categoryWrites,"Protected/allowed store publications differ");check(task.complete()==(allowed==0),"Protection completion differs");}
        if(kind==3)check(count==1&&children[0]==C::NameEditor&&pageWrites==1&&page==2&&!dirtyWrites&&!categoryWrites,"Name page policy differs");
        if(kind==4)check(!count&&pageWrites==1&&page==3&&!dirtyWrites&&!categoryWrites,"Store page policy differs");
        if(task.awaitingTail()){check(!task.nextClocks()&&task.retireTail(),"Store tail did not retire");}
        check(task.complete()&&!task.retireTail()&&!task.advanceBefore(100,read,write),"Completed store task ran again");++cases;
    }
    std::cout<<"PASS: "<<cases<<" store UI policies / "<<intervals<<" intervals; all allowed/mode bytes, both store costs, child order, protection/publications, tail retirement and deadlines\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
