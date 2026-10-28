#pragma once
#include "ksInterruptService.h"
#include <array>
#include <optional>
#include <cstdint>

namespace ks {
// Priority selection and nested peripheral continuations are native controller
// work. The caller supplies pending events and byte resources, never a CPU.
class InterruptScheduler final {
public:
    struct Selection { int vector=-1; uint8_t priority=0; };
    struct Advance {
        unsigned clocks=0;
        bool returned=false;
        uint8_t mask=0;
        uint32_t timer=0,adc=0,receive=0;
    };
    static constexpr unsigned Capacity=7, AdmissionClocks=28;
    static Selection select(uint64_t low,uint64_t high,const uint8_t* priorities,uint8_t mask) {
        Selection selected{-1,uint8_t(mask&7)};
        for(unsigned base=0;base<=64;base+=64){
            auto pending=base?high:low;
            while(pending){
                unsigned bit=0;auto word=pending;
                while(!(word&1)){++bit;word>>=1;}
                if(priorities[base+bit]>selected.priority)
                    selected={int(base+bit),priorities[base+bit]};
                pending&=pending-1;
            }
        }
        return selected;
    }
    unsigned depth()const{return count_;}
    bool active()const{return count_!=0;}
    const InterruptService* current()const{return count_?&jobs_[count_-1]->service:nullptr;}
    uint8_t mask()const{return active()?uint8_t(current()->critical()?7:jobs_[count_-1]->priority):0;}
    bool admit(InterruptService::Kind kind,uint8_t priority,uint8_t previousMask){
        if(!priority||priority>7||priority<=(previousMask&7)||(active()&&priority<=mask())||count_==Capacity)return false;
        jobs_[count_++].emplace(kind,priority,previousMask);return true;
    }
    unsigned nextClocks()const{return active()?current()->nextClocks():0;}
    template<class Reader,class Writer>
    Advance advanceBefore(unsigned available,Reader&& read,Writer&& write){
        const auto cost=nextClocks();if(!cost||cost>available)return {};
        return advancePlannedPhase(cost,read,write);
    }
private:
    friend class NativeControllerExecutor;
    // The owner synchronously passes this phase's admitted nonzero cost.
    template<class Reader,class Writer>
    Advance advancePlannedPhase(unsigned cost,Reader&& read,Writer&& write){
        auto& job=*jobs_[count_-1];
        const auto used=job.service.advancePlannedPhase(cost,read,write);
        if(used!=cost)return {};
        Advance result{used,job.service.complete(),uint8_t(job.service.critical()?7:job.priority),
            job.service.timerComponents(),job.service.adcComponents(),job.service.serialReceiveComponents()};
        if(result.returned){result.mask=job.previousMask;jobs_[--count_].reset();}
        return result;
    }
    struct Job {
        InterruptService service;
        uint8_t priority,previousMask;
        Job(InterruptService::Kind kind,uint8_t level,uint8_t before)
            :service(kind),priority(level),previousMask(before){}
    };
    std::array<std::optional<Job>,Capacity> jobs_{};
    unsigned count_=0;
};
}
