#pragma once
#include <cstdint>
namespace kstation {
struct DspTriggerCredits{unsigned retainWord=6,retainLong=10,restoreWord=6,restoreLong=10,read=15,table=12,shortCall=5,call=10,ret=7;};
class DspTriggerValueTask final {
public:
 enum class Stage{Test,Select,Decrement,Clear,ShiftFirst,RotateFirst,ShiftSecond,RotateSecond,ClearLow,Send,Return,Complete};
 constexpr DspTriggerValueTask(uint8_t value=0,uint16_t address=0):input_(value),value_(value),address_(address){}
 Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}bool awaitingDsp()const{return awaiting_;}
 uint16_t address()const{return address_;}uint32_t word()const{return uint32_t(rotated_)<<8;}
 unsigned nextClocks(const DspTriggerCredits&c)const{if(awaiting_)return 0;return stage_==Stage::Complete?0:stage_==Stage::Send?c.call:stage_==Stage::Return?c.ret:3;}
 bool resumeDsp(){if(!awaiting_)return false;awaiting_=false;return true;}
 unsigned advanceBefore(unsigned budget,const DspTriggerCredits&c){auto n=nextClocks(c);if(!n||budget<n)return 0;switch(stage_){case Stage::Select:stage_=input_?Stage::Decrement:Stage::Return;break;case Stage::Decrement:--value_;next();break;case Stage::Clear:rotated_=0;next();break;case Stage::ShiftFirst:case Stage::ShiftSecond:carry_=(value_&1)!=0;value_>>=1;next();break;case Stage::RotateFirst:case Stage::RotateSecond:rotated_=uint16_t((rotated_>>1)|(carry_?0x8000:0));next();break;case Stage::Send:awaiting_=true;next();break;default:next();break;}return n;}
private:void next(){stage_=Stage(unsigned(stage_)+1);}Stage stage_=Stage::Test;uint8_t input_,value_;uint16_t address_,rotated_=0;bool carry_=false,awaiting_=false;
};
class DspTriggerTableTask final {
public:
 enum class Stage{Copy,Mask,Compare,Select,ShiftFirst,ShiftSecond,ShiftThird,ShiftFourth,MaskIndex,MaskOther,Merge,ClearPointer,CopyIndex,BindTable,ReadTable,Send,Return,Complete};
 constexpr DspTriggerTableTask(uint8_t value=0,uint8_t other=0,uint16_t address=0):input_(value),value_(value),other_(other),address_(address){}
 Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}bool awaitingDsp()const{return awaiting_;}
 uint16_t address()const{return address_;}uint32_t word()const{return valueWord_>>8;}uint32_t tableAddress()const{return 0xc124u+value_;}
 unsigned nextClocks(const DspTriggerCredits&c)const{if(awaiting_)return 0;switch(stage_){case Stage::BindTable:return 9;case Stage::ReadTable:return c.table;case Stage::Send:return c.call;case Stage::Return:return c.ret;case Stage::Complete:return 0;default:return 3;}}
 bool resumeDsp(){if(!awaiting_)return false;awaiting_=false;return true;}
 template<class R>unsigned advanceBefore(unsigned budget,const DspTriggerCredits&c,R&&read){auto n=nextClocks(c);if(!n||budget<n)return 0;switch(stage_){case Stage::Select:stage_=(input_&6)==6?Stage::ShiftFirst:Stage::Return;break;case Stage::ShiftFirst:case Stage::ShiftSecond:case Stage::ShiftThird:case Stage::ShiftFourth:value_=uint8_t(value_<<1);next();break;case Stage::MaskIndex:value_&=0x10;next();break;case Stage::MaskOther:other_&=0x0c;next();break;case Stage::Merge:value_|=other_;next();break;case Stage::ReadTable:valueWord_=read(tableAddress());next();break;case Stage::Send:awaiting_=true;next();break;default:next();break;}return n;}
private:void next(){stage_=Stage(unsigned(stage_)+1);}Stage stage_=Stage::Copy;uint8_t input_,value_,other_;uint16_t address_;uint32_t valueWord_=0;bool awaiting_=false;
};
class DspTriggerTask final {
public:
 enum class Child:uint8_t{None,Value,Table};
 enum class Stage{RetainProgram,BindProgram,ReadMode,CompareMode,SelectMode,RetainScratch,RetainAddress,RetainValue,RetainPort,
  ReadSingle,BindSingleAddress,SendSingle,ReadFirst,ReadOther,BindFirstAddress,SendFirst,ReadSecond,ShiftFirst,ShiftSecond,ShiftThird,ReadOtherSecond,ShiftOtherFirst,ShiftOtherSecond,BindSecondAddress,SendSecond,
  RestorePort,RestoreValue,RestoreAddress,RestoreScratch,RestoreProgram,RestoreInput,Return,Complete};
 enum class Read:uint8_t{Mode,Single,First,Other,Second,OtherSecond};
 explicit constexpr DspTriggerTask(uint8_t mode=0):mode_(mode){}
 Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}bool awaitingChild()const{return child_!=Child::None;}Child child()const{return child_;}
 uint8_t value()const{return value_;}uint8_t other()const{return other_;}uint16_t address()const{return stage_==Stage::ReadSingle||stage_==Stage::BindSingleAddress||stage_==Stage::SendSingle?singleAddress():second_?0x8059:0x8053;}
 unsigned lane()const{return lane_;}unsigned singleOffset()const{return 0x93+lane_;}
 bool resumeChild(){if(!awaitingChild())return false;auto c=child_;child_=Child::None;if(c==Child::Value){if(++lane_<3)stage_=Stage::ReadSingle;else stage_=Stage::ReadFirst;}else stage_=second_?Stage::RestorePort:Stage::ReadSecond;return true;}
 unsigned nextClocks(const DspTriggerCredits&c)const{if(awaitingChild())return 0;switch(stage_){case Stage::RetainProgram:case Stage::RetainValue:case Stage::RetainPort:return c.retainLong;case Stage::RetainScratch:case Stage::RetainAddress:return c.retainWord;case Stage::RestorePort:case Stage::RestoreValue:case Stage::RestoreProgram:return c.restoreLong;case Stage::RestoreAddress:case Stage::RestoreScratch:case Stage::RestoreInput:return c.restoreWord;
 case Stage::BindProgram:return 9;case Stage::ReadMode:case Stage::ReadSingle:case Stage::ReadFirst:case Stage::ReadOther:case Stage::ReadSecond:case Stage::ReadOtherSecond:return c.read;case Stage::SelectMode:return 8;case Stage::BindSingleAddress:case Stage::BindFirstAddress:case Stage::BindSecondAddress:return 6;case Stage::SendSingle:case Stage::SendFirst:case Stage::SendSecond:return c.shortCall;case Stage::Return:return c.ret;case Stage::Complete:return 0;default:return 3;}}
 template<class R>unsigned advanceBefore(unsigned budget,const DspTriggerCredits&c,R&&read){auto n=nextClocks(c);if(!n||budget<n)return 0;switch(stage_){case Stage::ReadMode:liveMode_=read(Read::Mode);next();break;case Stage::SelectMode:stage_=liveMode_==mode_?Stage::RetainScratch:Stage::RestoreProgram;break;case Stage::ReadSingle:value_=read(Read::Single);next();break;case Stage::SendSingle:child_=Child::Value;break;case Stage::ReadFirst:value_=read(Read::First);second_=false;next();break;case Stage::ReadOther:other_=read(Read::Other);next();break;case Stage::SendFirst:child_=Child::Table;break;case Stage::ReadSecond:value_=read(Read::Second);second_=true;next();break;case Stage::ShiftFirst:case Stage::ShiftSecond:case Stage::ShiftThird:value_>>=1;next();break;case Stage::ReadOtherSecond:other_=read(Read::OtherSecond);next();break;case Stage::ShiftOtherFirst:case Stage::ShiftOtherSecond:other_>>=1;next();break;case Stage::SendSecond:child_=Child::Table;break;default:next();break;}return n;}
private:void next(){stage_=Stage(unsigned(stage_)+1);}uint16_t singleAddress()const{constexpr uint16_t a[]{0x8181,0x8184,0x8180};return a[lane_];}Stage stage_=Stage::RetainProgram;uint8_t mode_,liveMode_=0,value_=0,other_=0;unsigned lane_=0;Child child_=Child::None;bool second_=false;
};
}
