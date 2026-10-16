#pragma once
#include <cstdint>
namespace ks {
// Firmware byte streaming with live transmit readiness and byte observations.
// The native cursor indexes a data source, not a processor address space.
// Zero in the original 16-bit count denotes the full 65536-word block.
// Costs admit ordinary three-clock ROM/SRAM data, not one-clock control IO.
class DspBootStreamTask final {
public:
    enum class Stage {ReadStatus,MaskStatus,AdmitStatus,ResetRetries,
        DecrementRetries,AdmitRetry,DispatchFault,AwaitFault,ResumeFault,
        ReadHigh,WriteHigh,ReadMiddle,WriteMiddle,ReadLow,WriteLow,Count,
        Continue,Complete};
    struct Advance {unsigned clocks=0,boundaries=0;};
    explicit constexpr DspBootStreamTask(uint16_t words):remaining_(words?words:65536u){}
    constexpr Stage stage()const{return stage_;}
    constexpr bool complete()const{return stage_==Stage::Complete;}
    constexpr bool retry()const{return retry_;}
    constexpr uint8_t status()const{return status_;}
    constexpr uint8_t byte()const{return byte_;}
    constexpr uint16_t retries()const{return uint16_t(retries_);}
    constexpr uint32_t remainingWords()const{return remaining_;}
    constexpr uint32_t bytesConsumed()const{return bytes_;}
    constexpr uint64_t elapsedClocks()const{return elapsed_;}
    constexpr unsigned nextBoundaryClocks()const{
        switch(stage_){
        case Stage::ReadStatus:case Stage::WriteHigh:case Stage::WriteMiddle:case Stage::WriteLow:return 15;
        case Stage::ReadHigh:case Stage::ReadMiddle:case Stage::ReadLow:return 8;
        case Stage::DispatchFault:return 20;
        case Stage::AwaitFault:case Stage::Complete:return 0;
        default:return 3;
        }
    }
    constexpr bool resumeFault(){if(stage_!=Stage::AwaitFault)return false;stage_=Stage::ResumeFault;return true;}
    template<class Status,class Source,class Write>
    Advance advanceBefore(unsigned available,Status&& status,Source&& source,Write&& write){
        Advance result;
        while(const unsigned cost=nextBoundaryClocks()){
            if(cost>available)break;
            switch(stage_){
            case Stage::ReadStatus:status_=uint8_t(status());stage_=Stage::MaskStatus;break;
            case Stage::MaskStatus:stage_=Stage::AdmitStatus;break;
            case Stage::AdmitStatus:stage_=(status_&2)?Stage::ReadHigh:retry_?Stage::DecrementRetries:Stage::ResetRetries;break;
            case Stage::ResetRetries:retries_=65536;retry_=true;stage_=Stage::ReadStatus;break;
            case Stage::DecrementRetries:--retries_;stage_=Stage::AdmitRetry;break;
            case Stage::AdmitRetry:stage_=retries_?Stage::ReadStatus:Stage::DispatchFault;break;
            case Stage::DispatchFault:stage_=Stage::AwaitFault;break;
            case Stage::ResumeFault:stage_=Stage::ResetRetries;break;
            case Stage::ReadHigh:byte_=uint8_t(source(bytes_++));stage_=Stage::WriteHigh;break;
            case Stage::WriteHigh:write(10,byte_);stage_=Stage::ReadMiddle;break;
            case Stage::ReadMiddle:byte_=uint8_t(source(bytes_++));stage_=Stage::WriteMiddle;break;
            case Stage::WriteMiddle:write(12,byte_);stage_=Stage::ReadLow;break;
            case Stage::ReadLow:byte_=uint8_t(source(bytes_++));stage_=Stage::WriteLow;break;
            case Stage::WriteLow:write(14,byte_);stage_=Stage::Count;break;
            case Stage::Count:--remaining_;stage_=Stage::Continue;break;
            case Stage::Continue:retry_=false;stage_=remaining_?Stage::ReadStatus:Stage::Complete;break;
            default:break;
            }
            elapsed_+=cost;available-=cost;result.clocks+=cost;++result.boundaries;
        }
        return result;
    }
private:
    uint32_t remaining_,bytes_=0,retries_=65536;
    uint64_t elapsed_=0;
    Stage stage_=Stage::ReadStatus;
    uint8_t status_=0,byte_=0;
    bool retry_=false;
};
}
