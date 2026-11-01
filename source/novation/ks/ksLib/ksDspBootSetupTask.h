#pragma once
#include <cstdint>
namespace ks {
// Semantic setup of one DSP: header, firmware stream, startup delay/readiness,
// address selection and the two generated tuning tables. Child requests carry
// native values; their durations belong to the scheduler. No CPU or addresses.
class DspBootSetupTask final {
public:
    enum class Stage {HeaderHigh,HeaderLow,CaptureSize,StreamCount,DispatchStream,AwaitStream,
        DelayAdmission,OuterSetup,OuterAdmission,InnerSetup,InnerTick,InnerGate,
        OuterRelease,OuterTick,OuterGate,DelayRelease,
        BootStatus,BootMask,BootAdmit,BootReset,BootDecrement,BootRetry,
        DispatchFault,AwaitFault,BootResume,
        TableAddress,DispatchAddress,AwaitAddress,
        ExponentialValue,ExponentialFactor,ExponentialCount,DispatchExponential,AwaitExponential,
        ProductSeed,Product,ShiftSetup,Shift,ShiftTick,ShiftGate,AddDelta,ExponentialTick,ExponentialGate,
        ReciprocalDivisor,ReciprocalCount,Numerator,DivideHigh,CopyHigh,ClearDividend,
        DivideLow,ComposeWord,DispatchReciprocal,AwaitReciprocal,DivisorTick,ReciprocalTick,ReciprocalGate,Complete};
    enum class Request {None,Stream,Address,ExponentialWord,ReciprocalWord,Fault};
    struct Division {uint16_t quotient=0,remainder=0;};
    struct Advance {unsigned clocks=0,boundaries=0;};
    constexpr Stage stage()const{return stage_;}
    constexpr bool complete()const{return stage_==Stage::Complete;}
    constexpr uint8_t headerHigh()const{return headerHigh_;}
    constexpr uint16_t headerSize()const{return headerSize_;}
    constexpr uint16_t streamWords()const{return streamWords_;}
    constexpr unsigned outerRemaining()const{return outer_;}
    constexpr unsigned innerRemaining()const{return inner_;}
    constexpr uint8_t status()const{return status_;}
    constexpr bool retry()const{return retry_;}
    constexpr uint16_t retries()const{return uint16_t(retries_);}
    static constexpr uint16_t tableAddress(){return 0x8600;}
    static constexpr uint16_t factor(){return 0xb191;}
    constexpr uint32_t wordValue()const{return value_;}
    constexpr uint32_t product()const{return product_;}
    constexpr unsigned shiftRemaining()const{return shift_;}
    constexpr unsigned exponentialRemaining()const{return exponential_;}
    constexpr uint16_t divisor()const{return divisor_;}
    constexpr unsigned reciprocalRemaining()const{return reciprocal_;}
    constexpr Division division()const{return division_;}
    constexpr uint16_t quotientHigh()const{return quotientHigh_;}
    constexpr uint64_t elapsedClocks()const{return elapsed_;}
    constexpr Request request()const{
        switch(stage_){
        case Stage::AwaitStream:return Request::Stream;
        case Stage::AwaitAddress:return Request::Address;
        case Stage::AwaitExponential:return Request::ExponentialWord;
        case Stage::AwaitReciprocal:return Request::ReciprocalWord;
        case Stage::AwaitFault:return Request::Fault;
        default:return Request::None;
        }
    }
    constexpr bool resumeChild(){
        switch(stage_){
        case Stage::AwaitStream:stage_=Stage::DelayAdmission;return true;
        case Stage::AwaitAddress:stage_=Stage::ExponentialValue;return true;
        case Stage::AwaitExponential:stage_=Stage::ProductSeed;return true;
        case Stage::AwaitReciprocal:stage_=Stage::DivisorTick;return true;
        case Stage::AwaitFault:stage_=Stage::BootResume;return true;
        default:return false;
        }
    }
    constexpr unsigned nextBoundaryClocks()const{
        switch(stage_){
        case Stage::HeaderHigh:case Stage::HeaderLow:case Stage::ExponentialValue:case Stage::Numerator:return 9;
        case Stage::DispatchStream:case Stage::DispatchAddress:case Stage::DispatchExponential:
        case Stage::DispatchReciprocal:case Stage::DispatchFault:return 20;
        case Stage::DelayAdmission:case Stage::OuterAdmission:case Stage::OuterRelease:case Stage::DelayRelease:return 11;
        case Stage::OuterSetup:case Stage::InnerSetup:case Stage::TableAddress:case Stage::ExponentialFactor:
        case Stage::ExponentialCount:case Stage::ReciprocalDivisor:case Stage::ReciprocalCount:return 6;
        case Stage::BootStatus:return 15;
        case Stage::Product:case Stage::DivideHigh:case Stage::DivideLow:return 23;
        case Stage::AwaitStream:case Stage::AwaitAddress:case Stage::AwaitExponential:
        case Stage::AwaitReciprocal:case Stage::AwaitFault:case Stage::Complete:return 0;
        default:return 3;
        }
    }
    template<class Header,class Status>
    Advance advanceBefore(unsigned available,Header&& header,Status&& status){
        Advance progress;
        while(const unsigned cost=nextBoundaryClocks()){
            if(cost>available)break;
            switch(stage_){
            case Stage::HeaderHigh:headerHigh_=uint8_t(header(1));stage_=Stage::HeaderLow;break;
            case Stage::HeaderLow:headerSize_=uint16_t((unsigned(headerHigh_)<<8)|uint8_t(header(2)));stage_=Stage::CaptureSize;break;
            case Stage::CaptureSize:stage_=Stage::StreamCount;break;
            case Stage::StreamCount:streamWords_=uint16_t(headerSize_+2u);stage_=Stage::DispatchStream;break;
            case Stage::DispatchStream:stage_=Stage::AwaitStream;break;
            case Stage::DelayAdmission:stage_=Stage::OuterSetup;break;
            case Stage::OuterSetup:outer_=10;stage_=Stage::OuterAdmission;break;
            case Stage::OuterAdmission:stage_=Stage::InnerSetup;break;
            case Stage::InnerSetup:inner_=0xa67;stage_=Stage::InnerTick;break;
            case Stage::InnerTick:--inner_;stage_=Stage::InnerGate;break;
            case Stage::InnerGate:stage_=inner_?Stage::InnerTick:Stage::OuterRelease;break;
            case Stage::OuterRelease:stage_=Stage::OuterTick;break;
            case Stage::OuterTick:--outer_;stage_=Stage::OuterGate;break;
            case Stage::OuterGate:stage_=outer_?Stage::OuterAdmission:Stage::DelayRelease;break;
            case Stage::DelayRelease:stage_=Stage::BootStatus;break;
            case Stage::BootStatus:status_=uint8_t(status());stage_=Stage::BootMask;break;
            case Stage::BootMask:stage_=Stage::BootAdmit;break;
            case Stage::BootAdmit:stage_=!(status_&0x10)?Stage::TableAddress:retry_?Stage::BootDecrement:Stage::BootReset;break;
            case Stage::BootReset:retries_=65536;retry_=true;stage_=Stage::BootStatus;break;
            case Stage::BootDecrement:--retries_;stage_=Stage::BootRetry;break;
            case Stage::BootRetry:stage_=retries_?Stage::BootStatus:Stage::DispatchFault;break;
            case Stage::DispatchFault:stage_=Stage::AwaitFault;break;
            case Stage::BootResume:stage_=Stage::BootReset;break;
            case Stage::TableAddress:stage_=Stage::DispatchAddress;break;
            case Stage::DispatchAddress:stage_=Stage::AwaitAddress;break;
            case Stage::ExponentialValue:value_=0x40000000;stage_=Stage::ExponentialFactor;break;
            case Stage::ExponentialFactor:stage_=Stage::ExponentialCount;break;
            case Stage::ExponentialCount:exponential_=512;stage_=Stage::DispatchExponential;break;
            case Stage::DispatchExponential:stage_=Stage::AwaitExponential;break;
            case Stage::ProductSeed:stage_=Stage::Product;break;
            case Stage::Product:product_=uint32_t(factor())*uint16_t(value_>>16);stage_=Stage::ShiftSetup;break;
            case Stage::ShiftSetup:shift_=9;stage_=Stage::Shift;break;
            case Stage::Shift:product_>>=1;stage_=Stage::ShiftTick;break;
            case Stage::ShiftTick:--shift_;stage_=Stage::ShiftGate;break;
            case Stage::ShiftGate:stage_=shift_?Stage::Shift:Stage::AddDelta;break;
            case Stage::AddDelta:value_+=product_;stage_=Stage::ExponentialTick;break;
            case Stage::ExponentialTick:--exponential_;stage_=Stage::ExponentialGate;break;
            case Stage::ExponentialGate:stage_=exponential_?Stage::DispatchExponential:Stage::ReciprocalDivisor;break;
            case Stage::ReciprocalDivisor:divisor_=512;stage_=Stage::ReciprocalCount;break;
            case Stage::ReciprocalCount:reciprocal_=512;stage_=Stage::Numerator;break;
            case Stage::Numerator:numerator_=0x800000;stage_=Stage::DivideHigh;break;
            case Stage::DivideHigh:division_=divide(numerator_,divisor_);stage_=Stage::CopyHigh;break;
            case Stage::CopyHigh:quotientHigh_=division_.quotient;stage_=Stage::ClearDividend;break;
            case Stage::ClearDividend:numerator_=uint32_t(division_.remainder)<<16;division_.quotient=0;stage_=Stage::DivideLow;break;
            case Stage::DivideLow:division_=divide(numerator_,divisor_);stage_=Stage::ComposeWord;break;
            case Stage::ComposeWord:value_=(uint32_t(quotientHigh_)<<16)|division_.quotient;stage_=Stage::DispatchReciprocal;break;
            case Stage::DispatchReciprocal:stage_=Stage::AwaitReciprocal;break;
            case Stage::DivisorTick:++divisor_;stage_=Stage::ReciprocalTick;break;
            case Stage::ReciprocalTick:--reciprocal_;stage_=Stage::ReciprocalGate;break;
            case Stage::ReciprocalGate:stage_=reciprocal_?Stage::Numerator:Stage::Complete;break;
            default:break;
            }
            available-=cost;elapsed_+=cost;progress.clocks+=cost;++progress.boundaries;
        }
        return progress;
    }
private:
    static constexpr Division divide(uint32_t numerator,uint16_t denominator){
        return {uint16_t(numerator/denominator),uint16_t(numerator%denominator)};
    }
    Stage stage_=Stage::HeaderHigh;
    uint8_t headerHigh_=0,status_=0;
    uint16_t headerSize_=0,streamWords_=0,divisor_=0,quotientHigh_=0;
    unsigned outer_=0,inner_=0,shift_=0,exponential_=0,reciprocal_=0;
    uint32_t retries_=65536,value_=0,product_=0,numerator_=0;
    Division division_{};
    bool retry_=false;
    uint64_t elapsed_=0;
};
}
