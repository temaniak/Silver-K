#pragma once
#include "kstationDspTransferTask.h"
namespace kstation {
// K DSP ROM bootstrap stream (155E). Source bytes are read live after each
// ready handshake and individually published in high/middle/low byte order.
// A zero initial word count retains the firmware's 65536-word wrap behavior.
class DspBootStreamTask final {
public:
 enum class Stage {PollReady,ReadHigh,WriteHigh,ReadMiddle,WriteMiddle,ReadLow,WriteLow,Decrement,Repeat,Return,Complete};
 struct Credits {DspTransferCredits protocol;unsigned readByte=8;};
 explicit DspBootStreamTask(uint16_t words):remaining_(words){}
 Stage stage()const{return stage_;}const DspPollTask&poll()const{return poll_;}bool complete()const{return stage_==Stage::Complete;}
 bool awaitingFault()const{return stage_==Stage::PollReady&&poll_.awaitingFault();}bool resumeFault(){return stage_==Stage::PollReady&&poll_.resumeFault();}
 uint16_t remaining()const{return remaining_;}unsigned sourceIndex()const{return index_;}uint8_t byte()const{return byte_;}
 unsigned nextClocks(const Credits&c)const{switch(stage_){case Stage::PollReady:return poll_.nextClocks(c.protocol);case Stage::ReadHigh:case Stage::ReadMiddle:case Stage::ReadLow:return c.readByte;case Stage::WriteHigh:case Stage::WriteMiddle:case Stage::WriteLow:return c.protocol.writePort;case Stage::Return:return c.protocol.ret;case Stage::Complete:return 0;default:return 3;}}
 template<class R,class W>unsigned advanceBefore(unsigned budget,const Credits&c,R&&read,W&&write){auto cost=nextClocks(c);if(!cost||cost>budget)return 0;if(stage_==Stage::PollReady){auto used=poll_.advanceBefore(budget,c.protocol,[&]{return read(true,index_);});if(poll_.complete())stage_=Stage::ReadHigh;return used;}switch(stage_){case Stage::ReadHigh:case Stage::ReadMiddle:case Stage::ReadLow:byte_=uint8_t(read(false,index_++));next();break;case Stage::WriteHigh:case Stage::WriteMiddle:case Stage::WriteLow:write(stage_==Stage::WriteHigh?10:stage_==Stage::WriteMiddle?12:14,byte_);next();break;case Stage::Decrement:--remaining_;next();break;case Stage::Repeat:if(remaining_){poll_=DspPollTask(2,true);stage_=Stage::PollReady;}else stage_=Stage::Return;break;case Stage::Return:stage_=Stage::Complete;break;default:break;}return cost;}
private:void next(){stage_=Stage(unsigned(stage_)+1);}Stage stage_=Stage::PollReady;DspPollTask poll_{2,true};uint16_t remaining_;unsigned index_=0;uint8_t byte_=0;
};
}

namespace kstation {
class DspResetTask final {
public:
 enum class Stage {RetainPins,ReadPins,Assert,PublishAssert,RetainDelay,CountSettling,RetainInner,CountDelay,TickDelay,RepeatDelay,ReleaseInner,TickSettling,RepeatSettling,ReleaseDelay,ReleaseReset,PublishRelease,RestorePins,Return,Complete};
 struct Credits {unsigned retainWord=6,restoreWord=6,ret=7;};
 Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}bool secondDelay()const{return second_;}uint8_t pins()const{return pins_;}
 unsigned nextClocks(const Credits&c)const{switch(stage_){case Stage::RetainPins:case Stage::RetainDelay:case Stage::RetainInner:return c.retainWord;case Stage::RestorePins:case Stage::ReleaseInner:case Stage::ReleaseDelay:return c.restoreWord;case Stage::CountSettling:case Stage::CountDelay:return 6;case Stage::ReadPins:case Stage::PublishAssert:case Stage::PublishRelease:return 4;case Stage::Return:return c.ret;case Stage::Complete:return 0;default:return 3;}}
 template<class R,class W>unsigned advanceBefore(unsigned budget,const Credits&c,R&&read,W&&write){auto cost=nextClocks(c);if(!cost||cost>budget)return 0;auto next=Stage(unsigned(stage_)+1);switch(stage_){case Stage::ReadPins:pins_=uint8_t(read());break;case Stage::Assert:pins_&=uint8_t(~2u);break;case Stage::PublishAssert:case Stage::PublishRelease:write(pins_);break;case Stage::CountSettling:settling_=10;break;case Stage::CountDelay:delay_=2663;break;case Stage::TickDelay:--delay_;break;case Stage::RepeatDelay:if(delay_)next=Stage::TickDelay;break;case Stage::TickSettling:--settling_;break;case Stage::RepeatSettling:if(settling_)next=Stage::RetainInner;break;case Stage::ReleaseDelay:next=second_?Stage::RestorePins:Stage::ReleaseReset;break;case Stage::ReleaseReset:pins_|=2;break;case Stage::Return:next=Stage::Complete;break;default:break;}if(stage_==Stage::PublishRelease){second_=true;next=Stage::RetainDelay;}stage_=next;return cost;}
private:Stage stage_=Stage::RetainPins;bool second_=false;uint8_t pins_=0,settling_=0;uint16_t delay_=0;
};
class DspBootTask final {
public:
 enum class Stage {ResetDsp,BindPort,BindProgram,ReadCountHigh,ReadCountLow,BindCount,IncludeHeader,SendProgram,RetainDelay,CountSettling,RetainInner,CountDelay,TickDelay,RepeatDelay,ReleaseInner,TickSettling,RepeatSettling,ReleaseDelay,PollFinished,SelectAddress,SendAddress,BindTables,CountTables,SendTables,InvalidateAddress,Return,Complete};
 enum class Child {None,Reset,ProgramStream,Address,TableStream,Invalidate};
 struct Credits {DspTransferCredits protocol;unsigned shortCall=5,readHeader=9;};
 Stage stage()const{return stage_;}Child child()const{return child_;}bool awaiting()const{return child_!=Child::None;}bool awaitingFault()const{return stage_==Stage::PollFinished&&poll_.awaitingFault();}bool resumeFault(){return stage_==Stage::PollFinished&&poll_.resumeFault();}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(){if(!awaiting())return false;child_=Child::None;return true;}uint16_t programWords()const{return uint16_t(count_+2);}const DspPollTask&poll()const{return poll_;}
 unsigned nextClocks(const Credits&c)const{if(awaiting())return 0;switch(stage_){case Stage::ResetDsp:case Stage::InvalidateAddress:return c.protocol.call;case Stage::SendProgram:case Stage::SendAddress:case Stage::SendTables:return c.shortCall;case Stage::BindPort:case Stage::BindProgram:case Stage::BindTables:return 9;case Stage::ReadCountHigh:case Stage::ReadCountLow:return c.readHeader;case Stage::SelectAddress:case Stage::CountTables:case Stage::CountSettling:case Stage::CountDelay:return 6;case Stage::RetainDelay:case Stage::RetainInner:return c.protocol.retainWord;case Stage::ReleaseInner:case Stage::ReleaseDelay:return c.protocol.restoreWord;case Stage::PollFinished:{auto cr=c.protocol;cr.call=c.shortCall;return poll_.nextClocks(cr);}case Stage::Return:return c.protocol.ret;case Stage::Complete:return 0;default:return 3;}}
 template<class R>unsigned advanceBefore(unsigned budget,const Credits&c,R&&read){auto cost=nextClocks(c);if(!cost||cost>budget)return 0;if(stage_==Stage::PollFinished){auto cr=c.protocol;cr.call=c.shortCall;auto used=poll_.advanceBefore(budget,cr,[&]{return read(4);});if(poll_.complete())stage_=Stage::SelectAddress;return used;}auto next=Stage(unsigned(stage_)+1);switch(stage_){case Stage::ResetDsp:child_=Child::Reset;break;case Stage::ReadCountHigh:count_=uint16_t(uint8_t(read(1)))<<8;break;case Stage::ReadCountLow:count_|=uint8_t(read(2));break;case Stage::SendProgram:child_=Child::ProgramStream;break;case Stage::CountSettling:settling_=10;break;case Stage::CountDelay:delay_=2663;break;case Stage::TickDelay:--delay_;break;case Stage::RepeatDelay:if(delay_)next=Stage::TickDelay;break;case Stage::TickSettling:--settling_;break;case Stage::RepeatSettling:if(settling_)next=Stage::RetainInner;break;case Stage::SendAddress:child_=Child::Address;break;case Stage::SendTables:child_=Child::TableStream;break;case Stage::InvalidateAddress:child_=Child::Invalidate;break;case Stage::Return:next=Stage::Complete;break;default:break;}stage_=next;return cost;}
private:Stage stage_=Stage::ResetDsp;Child child_=Child::None;DspPollTask poll_{16,false};uint16_t count_=0,delay_=0;uint8_t settling_=0;
};
}
