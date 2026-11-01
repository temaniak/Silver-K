#pragma once
#include <cstdint>
namespace kstation {
struct VoiceFeedbackCredits{unsigned readGlobal=12,writeGlobal=12,readState=15,writeState=15,writeInactive=21,readCounter=9,writeCounter=9,call=10,ret=7,retain=6,restore=6;};
// One live voice-feedback slot per Main pass, including the original zero-wrap
// countdown policy. Inputs/results are domain values, never processor lanes.
class VoiceFeedbackTask final {
public:
 enum class Child:uint8_t{None,Voice,Bus,Read};
 enum class Stage{ReadCursor,PublishVoice,Increment,ReadCount,CompareCount,SelectCount,ClearCursor,PublishCursor,LookupVoice,ReadState,SelectEmpty,SelectInactive,
  InvalidateInactive,BindInactive,ReadInactive,TestInactive,SelectInactiveFlag,SetInactive,PublishInactive,ExitInactive,
  InvalidateActive,BindActive,ReadActive,ShiftActive,MaskState,MergeState,PublishState,SelectCountDown,ReadCounter,DecrementCounter,SelectCounter,PublishCounter,Return,Complete};
 Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}Child child()const{return child_;}bool awaitingChild()const{return child_!=Child::None;}
 uint32_t voice()const{return voice_;}uint8_t voiceIndex()const{return voiceIndex_;}uint8_t cursor()const{return cursor_;}uint16_t state()const{return state_;}uint16_t counter()const{return counter_;}
 uint16_t dspAddress()const{return inactive_?0x8052:0x8061;}
 bool resumeChild(uint32_t value=0){if(!awaitingChild())return false;if(child_==Child::Voice)voice_=value;else if(child_==Child::Read)feedback_=value&0xffffff;child_=Child::None;return true;}
 unsigned nextClocks(const VoiceFeedbackCredits&c)const{if(awaitingChild())return 0;switch(stage_){case Stage::ReadCursor:case Stage::ReadCount:return c.readGlobal;case Stage::PublishVoice:case Stage::PublishCursor:return c.writeGlobal;case Stage::LookupVoice:case Stage::InvalidateInactive:case Stage::ReadInactive:case Stage::InvalidateActive:case Stage::ReadActive:return c.call;case Stage::ReadState:return c.readState;case Stage::BindInactive:case Stage::BindActive:return 6;case Stage::SetInactive:return 9;case Stage::PublishInactive:return c.writeInactive;case Stage::ExitInactive:return 3;case Stage::MaskState:return 6;case Stage::PublishState:return c.writeState;case Stage::ReadCounter:return c.readCounter;case Stage::PublishCounter:return c.writeCounter;case Stage::Return:return c.ret;case Stage::Complete:return 0;default:return 3;}}
 template<class R,class W>unsigned advanceBefore(unsigned budget,const VoiceFeedbackCredits&c,R&&read,W&&write){auto n=nextClocks(c);if(!n||budget<n)return 0;switch(stage_){case Stage::ReadCursor:voiceIndex_=cursor_=uint8_t(read(0));next();break;case Stage::PublishVoice:write(0,voiceIndex_);next();break;case Stage::Increment:++cursor_;next();break;case Stage::ReadCount:count_=uint8_t(read(1));next();break;case Stage::SelectCount:stage_=cursor_<count_?Stage::PublishCursor:Stage::ClearCursor;break;case Stage::ClearCursor:cursor_=0;next();break;case Stage::PublishCursor:write(1,cursor_);next();break;case Stage::LookupVoice:child_=Child::Voice;next();break;case Stage::ReadState:state_=uint16_t(read(2));next();break;case Stage::SelectEmpty:stage_=state_?Stage::SelectInactive:Stage::Return;break;case Stage::SelectInactive:inactive_=(state_&0x8000)!=0;stage_=inactive_?Stage::InvalidateInactive:Stage::InvalidateActive;break;case Stage::InvalidateInactive:case Stage::InvalidateActive:child_=Child::Bus;next();break;case Stage::ReadInactive:case Stage::ReadActive:child_=Child::Read;next();break;case Stage::SelectInactiveFlag:stage_=(feedback_&1)?Stage::ReadCounter:Stage::SetInactive;break;case Stage::SetInactive:state_=0x7fff;next();break;case Stage::PublishInactive:write(2,0x7fffffff);next();break;case Stage::ExitInactive:stage_=Stage::Return;break;case Stage::ShiftActive:feedback_=uint16_t(feedback_>>8)>>1;next();break;case Stage::MaskState:state_&=0x4000;next();break;case Stage::MergeState:state_|=uint16_t(feedback_);next();break;case Stage::PublishState:write(3,state_);next();break;case Stage::ReadCounter:counter_=uint16_t(read(3));next();break;case Stage::DecrementCounter:--counter_;next();break;case Stage::SelectCounter:stage_=counter_?Stage::PublishCounter:Stage::Return;break;case Stage::PublishCounter:write(4,counter_);next();break;default:next();break;}return n;}
private:void next(){stage_=Stage(unsigned(stage_)+1);}Stage stage_=Stage::ReadCursor;Child child_=Child::None;uint32_t voice_=0,feedback_=0;uint16_t state_=0,counter_=0;uint8_t cursor_=0,voiceIndex_=0,count_=0;bool inactive_=false;
};
class DspRefreshTask final {
public:
 enum class Child:uint8_t{None,Read,Write,Global};enum class Stage{BindAddress,RetainAddress,Read,RestoreAddress,Compare,Select,Clear,Write,Global,Return,Complete};
 Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}Child child()const{return child_;}bool awaitingChild()const{return child_!=Child::None;}
 bool resumeChild(uint32_t value=0){if(!awaitingChild())return false;if(child_==Child::Read)response_=uint8_t(value);child_=Child::None;return true;}
 unsigned nextClocks(const VoiceFeedbackCredits&c)const{if(awaitingChild())return 0;switch(stage_){case Stage::BindAddress:return 6;case Stage::RetainAddress:return c.retain;case Stage::RestoreAddress:return c.restore;case Stage::Read:case Stage::Write:case Stage::Global:return c.call;case Stage::Return:return c.ret;case Stage::Complete:return 0;default:return 3;}}
 unsigned advanceBefore(unsigned budget,const VoiceFeedbackCredits&c){auto n=nextClocks(c);if(!n||budget<n)return 0;switch(stage_){case Stage::Read:child_=Child::Read;next();break;case Stage::Select:stage_=response_==3?Stage::Clear:Stage::Return;break;case Stage::Write:child_=Child::Write;next();break;case Stage::Global:child_=Child::Global;next();break;default:next();break;}return n;}
private:void next(){stage_=Stage(unsigned(stage_)+1);}Stage stage_=Stage::BindAddress;Child child_=Child::None;uint8_t response_=0;
};
}
