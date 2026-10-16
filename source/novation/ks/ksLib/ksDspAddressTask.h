#pragma once
#include <cstdint>
namespace ks {
// Live DSP address request and optional 24-bit reply. This job owns values,
// protocol phases and readiness/retry timing, without a CPU or guest frame.
class DspAddressTask final {
public:
    enum class PortSelection {Keep,A,B};
    enum class Stage {SelectPort,SelectDelay,ReadStatus,MaskStatus,AdmitStatus,
        ResetRetries,DecrementRetries,AdmitRetry,DispatchFault,AwaitFault,
        ResumeFault,AssertHF3,WriteHF3,Command,WriteCommand,WriteAddressHigh,
        WriteAddressLow,ClearHF3,ReleaseHF3,ReadHigh,ReadMiddle,ReadLow,Complete};
    struct Advance {unsigned clocks=0,boundaries=0;};
    constexpr DspAddressTask(uint16_t address,bool receive=false,PortSelection port=PortSelection::Keep)
        :address_(address),receive_(receive),port_(port),stage_(port==PortSelection::Keep?Stage::ReadStatus:Stage::SelectPort){}
    constexpr Stage stage()const{return stage_;}
    constexpr PortSelection portSelection()const{return port_;}
    constexpr bool receives()const{return receive_;}
    constexpr bool requestSent()const{return requestSent_;}
    constexpr bool retry()const{return retry_;}
    constexpr unsigned readyMask()const{return requestSent_ && receive_?1u:4u;}
    constexpr uint8_t status()const{return status_;}
    constexpr uint8_t data()const{return data_;}
    constexpr uint16_t retries()const{return uint16_t(retries_);}
    constexpr uint16_t address()const{return address_;}
    constexpr uint8_t command()const{return receive_?0x80:0;}
    constexpr uint32_t result()const{return result_;}
    constexpr bool complete()const{return stage_==Stage::Complete;}
    constexpr uint64_t elapsedClocks()const{return elapsed_;}
    constexpr unsigned nextBoundaryClocks()const{
        switch(stage_){
        case Stage::SelectPort:return 9;
        case Stage::ReadStatus:case Stage::WriteHF3:case Stage::WriteCommand:
        case Stage::WriteAddressHigh:case Stage::WriteAddressLow:case Stage::ReleaseHF3:
        case Stage::ReadHigh:case Stage::ReadMiddle:case Stage::ReadLow:return 15;
        case Stage::DispatchFault:return 20;
        case Stage::AwaitFault:case Stage::Complete:return 0;
        default:return 3;
        }
    }
    constexpr bool resumeFault(){if(stage_!=Stage::AwaitFault)return false;stage_=Stage::ResumeFault;return true;}
    template<class Read,class Write>
    Advance advanceBefore(unsigned available,Read&& read,Write&& write){
        Advance progress;
        while(const unsigned cost=nextBoundaryClocks()){
            if(cost>available)break;
            switch(stage_){
            case Stage::SelectPort:stage_=port_==PortSelection::B?Stage::SelectDelay:Stage::ReadStatus;break;
            case Stage::SelectDelay:stage_=Stage::ReadStatus;break;
            case Stage::ReadStatus:status_=uint8_t(read(4));stage_=Stage::MaskStatus;break;
            case Stage::MaskStatus:stage_=Stage::AdmitStatus;break;
            case Stage::AdmitStatus:stage_=(status_&readyMask())?(requestSent_?Stage::ClearHF3:Stage::AssertHF3):retry_?Stage::DecrementRetries:Stage::ResetRetries;break;
            case Stage::ResetRetries:retries_=65536;retry_=true;stage_=Stage::ReadStatus;break;
            case Stage::DecrementRetries:--retries_;stage_=Stage::AdmitRetry;break;
            case Stage::AdmitRetry:stage_=retries_?Stage::ReadStatus:Stage::DispatchFault;break;
            case Stage::DispatchFault:stage_=Stage::AwaitFault;break;
            case Stage::ResumeFault:stage_=Stage::ResetRetries;break;
            case Stage::AssertHF3:stage_=Stage::WriteHF3;break;
            case Stage::WriteHF3:write(0,8);stage_=Stage::Command;break;
            case Stage::Command:stage_=Stage::WriteCommand;break;
            case Stage::WriteCommand:write(10,command());stage_=Stage::WriteAddressHigh;break;
            case Stage::WriteAddressHigh:write(12,uint8_t(address_>>8));stage_=Stage::WriteAddressLow;break;
            case Stage::WriteAddressLow:write(14,uint8_t(address_));requestSent_=true;retry_=false;stage_=Stage::ReadStatus;break;
            case Stage::ClearHF3:stage_=Stage::ReleaseHF3;break;
            case Stage::ReleaseHF3:write(0,0);stage_=receive_?Stage::ReadHigh:Stage::Complete;break;
            case Stage::ReadHigh:data_=uint8_t(read(10));result_=uint32_t(data_)<<16;stage_=Stage::ReadMiddle;break;
            case Stage::ReadMiddle:data_=uint8_t(read(12));result_|=uint32_t(data_)<<8;stage_=Stage::ReadLow;break;
            case Stage::ReadLow:data_=uint8_t(read(14));result_|=data_;stage_=Stage::Complete;break;
            default:break;
            }
            elapsed_+=cost;available-=cost;progress.clocks+=cost;++progress.boundaries;
        }
        return progress;
    }
private:
    uint16_t address_;
    bool receive_,requestSent_=false,retry_=false;
    PortSelection port_;
    Stage stage_;
    uint32_t retries_=65536,result_=0;
    uint8_t status_=0,data_=0;
    uint64_t elapsed_=0;
};
}
