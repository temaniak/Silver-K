#include "ksTimerServiceTask.h"
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
uint64_t cases=0,boundaries=0;
}
int main(){try{
    using I=ks::TimerInterruptTask;
    for(bool extended:{false,true})for(unsigned board=0;board<256;++board)for(unsigned divider=0;divider<256;++divider){
        I task;
        uint8_t liveDivider=uint8_t(divider);
        uint32_t liveCounter=board&1?0xffffffffu:board*17u+divider;
        unsigned reads=0,writes=0,children=0,clockChildren=0,decays=0;
        const auto latestDivider=board?liveDivider:uint8_t(divider+1);
        const bool clockDue=latestDivider>=128;
        const auto expectedCounter=liveCounter+0x100u+1;
        auto read=[&](I::Read field)->uint32_t{
            ++reads;
            switch(field){
            case I::Read::Status:return board^divider;
            case I::Read::BoardMode:return board;
            case I::Read::Divider:return liveDivider;
            case I::Read::Counter:return liveCounter;
            }return 0;
        };
        auto write=[&](I::Write field,uint32_t value){
            ++writes;
            if(field==I::Write::Acknowledge)check(value==0xc0&&writes==1,"Timer acknowledgement changed");
            if(field==I::Write::Divider)check(value==uint8_t(latestDivider+128),"Divider wrap changed");
            if(field==I::Write::Counter)check(value==expectedCounter,"Counter was observed before child completion");
        };
        while(!task.complete()){
            if(task.awaitingChild()){
                if(task.child()==I::Child::BoardScan){check(board==0&&children==0,"Board scan order");++liveDivider;}
                if(task.child()==I::Child::Clock){check(clockDue,"Spurious clock tick");++clockChildren;liveCounter+=0x100;}
                if(task.child()==I::Child::PanelDecay){check(clockChildren==1&&!(expectedCounter&7),"Panel cadence");++decays;}
                ++children;check(task.resumeChild(),"Timer child rejected");continue;
            }
            const auto cost=task.nextClocks(extended?23:17);const auto stage=task.stage();const auto oldReads=reads,oldWrites=writes;
            check(!task.advanceBefore(cost-1,read,write,extended?23:17)&&task.stage()==stage&&reads==oldReads&&writes==oldWrites,"Early timer IO");
            check(task.advanceBefore(cost,read,write,extended?23:17)==cost,"Timer interval admission");++boundaries;
        }
        check(clockChildren==unsigned(clockDue)&&decays==unsigned(clockDue&&!(expectedCounter&7)),"Timer service selection");
        check(reads==3+unsigned(clockDue)&&writes==2+unsigned(clockDue)&&!task.resumeChild(),"Timer observation count");++cases;
    }
    using T=ks::ClockAccumulatorTask;
    constexpr std::array<uint32_t,6> phases{{0,1,0x7fffffffu,0x80000000u,0xfffffffeu,0xffffffffu}};
    for(auto phase:phases)for(unsigned flags=0;flags<256;++flags)for(unsigned timeout=0;timeout<256;++timeout){
        T task;
        const uint32_t increment=(flags&1)?1u:0x80000001u;
        const auto expected=uint64_t(phase)+increment;
        const uint16_t pulses=timeout&1?65535:timeout*257;
        const uint8_t lateFlags=uint8_t(flags^0xa5);
        unsigned reads=0,writes=0,children=0,timeoutWrites=0;
        auto read=[&](T::Read field)->uint32_t{
            ++reads;
            switch(field){
            case T::Read::Increment:return increment;
            case T::Read::Phase:return phase;
            case T::Read::PulseCount:return pulses;
            case T::Read::OutputFlags:return flags;
            case T::Read::Secondary:return 0xffffffffu-timeout;
            case T::Read::SecondaryIncrement:return flags+1;
            case T::Read::RunFlags:return lateFlags;
            case T::Read::Timeout:return timeout;
            }return 0;
        };
        auto write=[&](T::Write field,uint32_t value){
            ++writes;
            switch(field){
            case T::Write::Phase:check(value==uint32_t(expected),"Primary phase wrap");break;
            case T::Write::PulseCount:check(value==uint16_t(pulses+unsigned(expected>0xffffffffu)),"Pulse count wrap");break;
            case T::Write::Secondary:check(value==uint32_t(0xffffffffu-timeout+flags+1),"Secondary phase wrap");break;
            case T::Write::Timeout:++timeoutWrites;check(value==uint8_t(timeout+1)&&value!=0,"Timeout publication");break;
            case T::Write::RunFlags:check(value==(timeout==255?lateFlags&0xfeu:lateFlags),"Late run flags");break;
            }
        };
        while(!task.complete()){
            if(task.awaitingOutput()){++children;check(task.resumeOutput(),"Clock output resume");continue;}
            const auto cost=task.nextClocks();const auto stage=task.stage();const auto oldReads=reads,oldWrites=writes;
            check(!task.advanceBefore(cost-1,read,write)&&stage==task.stage()&&reads==oldReads&&writes==oldWrites,"Early accumulator IO");
            check(task.advanceBefore(cost,read,write)==cost,"Accumulator admission");++boundaries;
        }
        check(children==unsigned(expected>0xffffffffu&&bool(flags&2)),"F8 carry selection");
        check(timeoutWrites==unsigned(timeout!=255)&&writes==4+timeoutWrites,"Timeout completion count");
        check(!task.resumeOutput(),"Completed accumulator resumed");++cases;
    }
    for(unsigned value=0;value<65536;++value){
        ks::ControllerClockReadTask task;unsigned reads=0,clocks=0;
        while(!task.complete()){
            const auto cost=task.nextClocks();auto read=[&]{++reads;return uint32_t(value*65537u);};
            check(!task.advanceBefore(cost-1,read),"Early clock read");clocks+=task.advanceBefore(cost,read);++boundaries;
        }
        check(reads==1&&clocks==44&&task.counter()==value*65537u,"Live 32-bit clock value/duration");++cases;
    }
    std::cout<<"PASS: "<<cases<<" timer/dual-phase/read policies, "<<boundaries<<" intervals; all byte divider/flags/timeout pairs, 16/32-bit wrap, late children, deadlines\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
