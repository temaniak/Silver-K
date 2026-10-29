#pragma once
#include <cstdint>

namespace kstation {
// K's live, byte-oriented host protocol. KS supplies the task/child ownership
// pattern; K supplies the handshakes, retry policy and timing. These tasks do
// not own a processor frame or decode any instruction/data stream.
struct DspTransferCredits {
    unsigned readStatus=15,writePort=15,readCache=12,writeCache=12;
    unsigned call=10,ret=7,retainWord=6,restoreWord=6;
};
class DspPollTask final {
public:
    enum class Stage {FirstRead,FirstMask,FirstTest,ResetRetries,Read,Mask,
        Test,DecrementRetries,TestRetries,DispatchFault,AwaitFault,Retry,Complete};
    constexpr DspPollTask(uint8_t mask=2,bool readySet=true):mask_(mask),readySet_(readySet){}
    Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}
    bool awaitingFault()const{return stage_==Stage::AwaitFault;}
    bool resumeFault(){if(!awaitingFault())return false;stage_=Stage::Retry;return true;}
    uint8_t status()const{return status_;}uint8_t mask()const{return mask_;}uint16_t retries()const{return retries_;}
    unsigned nextClocks(const DspTransferCredits&c)const{
        switch(stage_){case Stage::FirstRead:case Stage::Read:return c.readStatus;
        case Stage::DispatchFault:return c.call;case Stage::AwaitFault:case Stage::Complete:return 0;default:return 3;}
    }
    template<class R>unsigned advanceBefore(unsigned budget,const DspTransferCredits&c,R&&read){
        const auto cost=nextClocks(c);if(!cost||cost>budget)return 0;
        switch(stage_){
        case Stage::FirstRead:case Stage::Read:status_=uint8_t(read());next();break;
        case Stage::FirstMask:case Stage::Mask:status_&=mask_;next();break;
        case Stage::FirstTest:stage_=ready()?Stage::Complete:Stage::ResetRetries;break;
        case Stage::ResetRetries:retries_=0;stage_=Stage::Read;break;
        case Stage::Test:stage_=ready()?Stage::Complete:Stage::DecrementRetries;break;
        case Stage::DecrementRetries:--retries_;next();break;
        case Stage::TestRetries:stage_=retries_?Stage::Read:Stage::DispatchFault;break;
        case Stage::DispatchFault:stage_=Stage::AwaitFault;break;
        case Stage::Retry:stage_=Stage::Read;break;
        default:break;
        }return cost;
    }
private:
    bool ready()const{return bool(status_)==readySet_;}
    void next(){stage_=Stage(unsigned(stage_)+1);}
    Stage stage_=Stage::FirstRead;uint8_t mask_,status_=0;bool readySet_;uint16_t retries_=0;
};
class DspAddressTask final {
public:
    enum class Stage {SelectPort,PublishCache,PollReady,PrepareAssert,Assert,
        PollAsserted,PrepareRelease,Release,PrepareCommand,WriteCommand,
        WriteAddressHigh,WriteAddressLow,PollResponse,ReadHigh,ReadMiddle,
        ReadLow,Return,Complete};
    enum class Write {Cache,Control,High,Middle,Low};
    explicit constexpr DspAddressTask(uint16_t address,bool receive=false)
        :address_(address),receive_(receive),stage_(receive?Stage::SelectPort:Stage::PollReady),poll_(4,true){}
    Stage stage()const{return stage_;}const DspPollTask&poll()const{return poll_;}
    bool complete()const{return stage_==Stage::Complete;}bool receives()const{return receive_;}
    bool awaitingFault()const{return polling()&&poll_.awaitingFault();}
    bool resumeFault(){return polling()&&poll_.resumeFault();}
    uint16_t address()const{return address_;}uint32_t result()const{return result_;}
    uint8_t byte()const{return byte_;}
    bool polling()const{return stage_==Stage::PollReady||stage_==Stage::PollAsserted||stage_==Stage::PollResponse;}
    unsigned nextClocks(const DspTransferCredits&c)const{
        if(polling())return poll_.nextClocks(c);
        switch(stage_){case Stage::SelectPort:return 9;case Stage::PublishCache:return c.writeCache;
        case Stage::Assert:case Stage::Release:case Stage::WriteCommand:case Stage::WriteAddressHigh:
        case Stage::WriteAddressLow:case Stage::ReadHigh:case Stage::ReadMiddle:case Stage::ReadLow:return c.writePort;
        case Stage::Return:return c.ret;case Stage::Complete:return 0;default:return 3;}
    }
    template<class R,class W>unsigned advanceBefore(unsigned budget,const DspTransferCredits&c,R&&read,W&&write){
        const auto cost=nextClocks(c);if(!cost||cost>budget)return 0;
        if(polling()){
            const auto used=poll_.advanceBefore(budget,c,[&]{return read(4);});
            if(poll_.complete()){
                if(stage_==Stage::PollReady)stage_=Stage::PrepareAssert;
                else if(stage_==Stage::PollAsserted)stage_=Stage::PrepareRelease;
                else stage_=receive_?Stage::ReadHigh:Stage::Return;
            }return used;
        }
        switch(stage_){
        case Stage::SelectPort:stage_=Stage::PublishCache;break;
        case Stage::PublishCache:write(Write::Cache,address_);stage_=Stage::PollReady;break;
        case Stage::PrepareAssert:stage_=Stage::Assert;break;
        case Stage::Assert:write(Write::Control,8);poll_=DspPollTask(16,true);stage_=Stage::PollAsserted;break;
        case Stage::PrepareRelease:stage_=Stage::Release;break;
        case Stage::Release:write(Write::Control,0);stage_=receive_?Stage::PrepareCommand:Stage::WriteCommand;break;
        case Stage::PrepareCommand:stage_=Stage::WriteCommand;break;
        case Stage::WriteCommand:write(Write::High,receive_?0x80:0);stage_=Stage::WriteAddressHigh;break;
        case Stage::WriteAddressHigh:write(Write::Middle,uint8_t(address_>>8));stage_=Stage::WriteAddressLow;break;
        case Stage::WriteAddressLow:write(Write::Low,uint8_t(address_));poll_=DspPollTask(receive_?1:16,receive_);stage_=Stage::PollResponse;break;
        case Stage::ReadHigh:byte_=uint8_t(read(10));result_=uint32_t(byte_)<<16;stage_=Stage::ReadMiddle;break;
        case Stage::ReadMiddle:byte_=uint8_t(read(12));result_|=uint32_t(byte_)<<8;stage_=Stage::ReadLow;break;
        case Stage::ReadLow:byte_=uint8_t(read(14));result_|=byte_;stage_=Stage::Return;break;
        case Stage::Return:stage_=Stage::Complete;break;
        default:break;
        }return cost;
    }
private:
    uint16_t address_;bool receive_;Stage stage_;DspPollTask poll_;uint32_t result_=0;uint8_t byte_=0;
};
class DspWordTask final {
public:
    enum class Stage {SelectPort,ReadCache,CompareCache,SelectAddress,DispatchAddress,
        AwaitAddress,PollReady,PrepareWord,High,Middle,Low,AdvanceAddress,
        PublishAddress,Return,Complete};
    enum class Write {High,Middle,Low,Cache};
    explicit constexpr DspWordTask(uint32_t word,uint16_t address=0,bool addressed=false)
        :word_(word&0xffffff),address_(address),addressed_(addressed),stage_(addressed?Stage::SelectPort:Stage::PollReady){}
    Stage stage()const{return stage_;}const DspPollTask&poll()const{return poll_;}
    bool complete()const{return stage_==Stage::Complete;}bool addressed()const{return addressed_;}
    bool awaitingAddress()const{return stage_==Stage::AwaitAddress;}
    bool resumeAddress(){if(!awaitingAddress())return false;stage_=Stage::PollReady;return true;}
    bool awaitingFault()const{return stage_==Stage::PollReady&&poll_.awaitingFault();}
    bool resumeFault(){return stage_==Stage::PollReady&&poll_.resumeFault();}
    uint32_t word()const{return word_;}uint16_t address()const{return address_;}
    uint16_t cachedAddress()const{return cache_;}uint16_t nextAddress()const{return uint16_t(address_+1);}
    unsigned nextClocks(const DspTransferCredits&c)const{
        switch(stage_){case Stage::SelectPort:return 9;case Stage::ReadCache:return c.readCache;
        case Stage::DispatchAddress:return c.call;case Stage::PollReady:return poll_.nextClocks(c);
        case Stage::High:case Stage::Middle:case Stage::Low:return c.writePort;
        case Stage::PublishAddress:return c.writeCache;case Stage::Return:return c.ret;
        case Stage::AwaitAddress:case Stage::Complete:return 0;default:return 3;}
    }
    template<class R,class W>unsigned advanceBefore(unsigned budget,const DspTransferCredits&c,R&&read,W&&write){
        const auto cost=nextClocks(c);if(!cost||cost>budget)return 0;
        if(stage_==Stage::PollReady){const auto used=poll_.advanceBefore(budget,c,[&]{return read(false);});if(poll_.complete())stage_=Stage::PrepareWord;return used;}
        switch(stage_){
        case Stage::SelectPort:stage_=Stage::ReadCache;break;
        case Stage::ReadCache:cache_=uint16_t(read(true));stage_=Stage::CompareCache;break;
        case Stage::CompareCache:stage_=Stage::SelectAddress;break;
        case Stage::SelectAddress:stage_=cache_==address_?Stage::PollReady:Stage::DispatchAddress;break;
        case Stage::DispatchAddress:stage_=Stage::AwaitAddress;break;
        case Stage::PrepareWord:stage_=Stage::High;break;
        case Stage::High:write(Write::High,uint8_t(word_>>16));stage_=Stage::Middle;break;
        case Stage::Middle:write(Write::Middle,uint8_t(word_>>8));stage_=Stage::Low;break;
        case Stage::Low:write(Write::Low,uint8_t(word_));stage_=addressed_?Stage::AdvanceAddress:Stage::Return;break;
        case Stage::AdvanceAddress:stage_=Stage::PublishAddress;break;
        case Stage::PublishAddress:write(Write::Cache,nextAddress());stage_=Stage::Return;break;
        case Stage::Return:stage_=Stage::Complete;break;
        default:break;
        }return cost;
    }
private:
    uint32_t word_;uint16_t address_,cache_=0;bool addressed_;Stage stage_;DspPollTask poll_;
};
class DspFaultTask final {
public:
    enum class Stage {DispatchLatch,AwaitLatch,ReturnWrapper,RetainValue,ReadPins,
        ToggleFirst,ToggleSecond,WritePins,RestoreValue,ReturnLatch,Complete};
    explicit constexpr DspFaultTask(bool wrapper=true):stage_(wrapper?Stage::DispatchLatch:Stage::RetainValue){}
    Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}
    bool awaitingLatch()const{return stage_==Stage::AwaitLatch;}
    bool resumeLatch(){if(!awaitingLatch())return false;stage_=Stage::ReturnWrapper;return true;}
    unsigned nextClocks(const DspTransferCredits&c)const{
        switch(stage_){case Stage::DispatchLatch:return c.call;case Stage::ReturnWrapper:case Stage::ReturnLatch:return c.ret;
        case Stage::RetainValue:return c.retainWord;case Stage::RestoreValue:return c.restoreWord;
        case Stage::ReadPins:case Stage::WritePins:return 4;case Stage::AwaitLatch:case Stage::Complete:return 0;default:return 3;}
    }
    template<class R,class W>unsigned advanceBefore(unsigned budget,const DspTransferCredits&c,R&&read,W&&write){
        const auto cost=nextClocks(c);if(!cost||cost>budget)return 0;
        switch(stage_){case Stage::DispatchLatch:stage_=Stage::AwaitLatch;break;
        case Stage::ReturnWrapper:case Stage::ReturnLatch:stage_=Stage::Complete;break;
        case Stage::ReadPins:pins_=uint8_t(read());next();break;
        case Stage::ToggleFirst:pins_^=0x10;next();break;
        case Stage::ToggleSecond:pins_^=0x20;next();break;
        case Stage::WritePins:write(pins_);next();break;
        default:next();break;}return cost;
    }
private:void next(){stage_=Stage(unsigned(stage_)+1);}Stage stage_;uint8_t pins_=0;
};
}
