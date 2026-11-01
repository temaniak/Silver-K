#pragma once
#include <cstdint>

namespace ks {
// Live 24-bit DSP transmission. Payload, address, readiness and retry decisions
// are native values. No processor, instruction, flag or stack drives this task.
// Each admission interval is interruptible; fault-service time belongs to the
// scheduler and is excluded from elapsedClocks(). No cached readiness response.
class DspWordTask final {
public:
    enum class Stage {ReadStatus, MaskStatus, AdmitStatus, ResetRetries,
        DecrementRetries, AdmitRetry, DispatchFault, AwaitFault, ResumeFault,
        Prepare, High, Middle, Low, AdvanceAddress, PublishAddress, Complete};
    struct Advance {unsigned clocks=0,boundaries=0;};
    constexpr DspWordTask(uint32_t word,bool advanceAddress=false,uint16_t address=0)
        :word_(word&0xffffffu),address_(address),advanceAddress_(advanceAddress){}
    constexpr Stage stage()const{return stage_;}
    constexpr bool retry()const{return retry_;}
    constexpr uint8_t status()const{return status_;}
    constexpr uint16_t retries()const{return uint16_t(retries_);}
    constexpr uint32_t word()const{return word_;}
    constexpr uint16_t nextAddress()const{return uint16_t(address_+1u);}
    constexpr uint16_t originalAddress()const{return address_;}
    constexpr bool advancesAddress()const{return advanceAddress_;}
    constexpr bool complete()const{return stage_==Stage::Complete;}
    constexpr uint64_t elapsedClocks()const{return elapsed_;}
    constexpr uint64_t completedBoundaries()const{return boundaries_;}
    constexpr unsigned nextBoundaryClocks()const{
        switch(stage_){
        case Stage::ReadStatus:case Stage::High:case Stage::Middle:case Stage::Low:return 15;
        case Stage::DispatchFault:return 20;
        case Stage::PublishAddress:return 12;
        case Stage::AwaitFault:case Stage::Complete:return 0;
        default:return 3;
        }
    }
    constexpr bool resumeFault(){
        if(stage_!=Stage::AwaitFault)return false;
        stage_=Stage::ResumeFault;return true;
    }
    // Callbacks execute only when their entire interval fits. They observe the
    // actual port at that instant. AwaitFault yields until explicitly resumed.
    template<class Read,class Write,class Publish>
    Advance advanceBefore(unsigned available,Read&& read,Write&& write,Publish&& publish){
        Advance result;
        while(const unsigned cost=nextBoundaryClocks()){
            if(cost>available)break;
            const auto stage=stage_;
            switch(stage){
            case Stage::ReadStatus:status_=uint8_t(read());stage_=Stage::MaskStatus;break;
            case Stage::MaskStatus:stage_=Stage::AdmitStatus;break;
            case Stage::AdmitStatus:
                stage_=(status_&2)?Stage::Prepare:retry_?Stage::DecrementRetries:Stage::ResetRetries;break;
            case Stage::ResetRetries:retries_=65536;retry_=true;stage_=Stage::ReadStatus;break;
            case Stage::DecrementRetries:--retries_;stage_=Stage::AdmitRetry;break;
            case Stage::AdmitRetry:stage_=retries_?Stage::ReadStatus:Stage::DispatchFault;break;
            case Stage::DispatchFault:stage_=Stage::AwaitFault;break;
            case Stage::ResumeFault:stage_=Stage::ResetRetries;break;
            case Stage::Prepare:stage_=Stage::High;break;
            case Stage::High:write(10,uint8_t(word_>>16));stage_=Stage::Middle;break;
            case Stage::Middle:write(12,uint8_t(word_>>8));stage_=Stage::Low;break;
            case Stage::Low:write(14,uint8_t(word_));stage_=advanceAddress_?Stage::AdvanceAddress:Stage::Complete;break;
            case Stage::AdvanceAddress:stage_=Stage::PublishAddress;break;
            case Stage::PublishAddress:publish(nextAddress());stage_=Stage::Complete;break;
            default:break;
            }
            available-=cost;result.clocks+=cost;++result.boundaries;elapsed_+=cost;++boundaries_;
        }
        return result;
    }
private:
    uint32_t word_,retries_=65536;
    uint16_t address_;
    bool advanceAddress_,retry_=false;
    uint8_t status_=0;
    Stage stage_=Stage::ReadStatus;
    uint64_t elapsed_=0,boundaries_=0;
};
}
