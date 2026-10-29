#pragma once
#include "kstationFlashStateTask.h"
namespace kstation {
class ProgramIndexTask final {
public:
 enum class Stage {Retain,ReadBank,DecrementBank,BindScale,Multiply,ReadProgram,ExtendProgram,AddBank,Restore,Return,Complete};
 explicit ProgramIndexTask(uint32_t source=0):source_(source){}Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}uint16_t result()const{return result_;}uint32_t source()const{return source_;}
 unsigned nextClocks(const FlashCredits&c)const{switch(stage_){case Stage::Retain:return c.pushWord;case Stage::ReadBank:return 5+c.read;case Stage::ReadProgram:return 3+c.read;case Stage::Multiply:return 15;case Stage::Restore:return c.popWord;case Stage::Return:return c.ret;case Stage::Complete:return 0;default:return 3;}}
 template<class R>unsigned advanceBefore(unsigned budget,const FlashCredits&c,R&&read){auto cost=nextClocks(c);if(!cost||cost>budget)return 0;switch(stage_){case Stage::ReadBank:bank_=uint8_t(read(false));break;case Stage::DecrementBank:--bank_;break;case Stage::Multiply:result_=uint16_t(bank_)*100;break;case Stage::ReadProgram:program_=uint8_t(read(true));break;case Stage::AddBank:result_=uint16_t(result_+program_);break;default:break;}stage_=Stage(unsigned(stage_)+1);return cost;}
private:Stage stage_=Stage::Retain;uint32_t source_;uint16_t result_=0;uint8_t bank_=0,program_=0;
};
class ProgramAddressTask final {
public:
 enum class Stage {BindSelection,ReadIndex,BindScale,Multiply,Retain,ReadMode,SelectMode,AddNormal,FinishNormal,AddAlternate,Restore,Return,Complete};
 Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}bool awaiting()const{return waiting_;}bool resumeChild(uint16_t index){if(!waiting_)return false;waiting_=false;index_=index;return true;}uint32_t result()const{return result_;}
 unsigned nextClocks(const FlashCredits&c)const{if(waiting_)return 0;switch(stage_){case Stage::BindSelection:case Stage::AddNormal:case Stage::AddAlternate:return 9;case Stage::ReadIndex:return c.call;case Stage::BindScale:return 6;case Stage::Multiply:return 23;case Stage::Retain:return c.pushWord;case Stage::ReadMode:return 9+c.read;case Stage::Restore:return c.popWord;case Stage::Return:return c.ret;case Stage::Complete:return 0;default:return 3;}}
 template<class R>unsigned advanceBefore(unsigned budget,const FlashCredits&c,R&&read){auto cost=nextClocks(c);if(!cost||cost>budget)return 0;auto next=Stage(unsigned(stage_)+1);switch(stage_){case Stage::ReadIndex:waiting_=true;break;case Stage::Multiply:result_=uint32_t(index_)*128;break;case Stage::ReadMode:mode_=uint8_t(read());break;case Stage::SelectMode:if(mode_)next=Stage::AddAlternate;break;case Stage::AddNormal:result_+=0x226400;break;case Stage::FinishNormal:next=Stage::Restore;break;case Stage::AddAlternate:result_+=0x220000;break;default:break;}stage_=next;return cost;}
private:Stage stage_=Stage::BindSelection;bool waiting_=false;uint16_t index_=0;uint8_t mode_=0;uint32_t result_=0;
};
class ProgramCopyTask final {
public:
 enum class Stage {BindTarget,Count,Read,Write,Advance,Decrement,Repeat,Invalidate,Unpack,ResetDsp,RefreshPanel,ClearVoice,InitVoice,Return,Complete};
 explicit ProgramCopyTask(uint32_t source=0):source_(source){}Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}bool awaiting()const{return waiting_;}bool resumeChild(){if(!waiting_)return false;waiting_=false;return true;}uint32_t source()const{return source_;}unsigned index()const{return index_;}
 unsigned nextClocks(const FlashCredits&c)const{if(waiting_)return 0;switch(stage_){case Stage::BindTarget:return 9;case Stage::Count:return 6;case Stage::Read:return 5+c.read;case Stage::Write:return 3+c.write;case Stage::Invalidate:case Stage::Unpack:case Stage::ResetDsp:case Stage::RefreshPanel:case Stage::InitVoice:return c.call;case Stage::Return:return c.ret;case Stage::Complete:return 0;default:return 3;}}
 template<class R,class W>unsigned advanceBefore(unsigned budget,const FlashCredits&c,R&&read,W&&write){auto cost=nextClocks(c);if(!cost||cost>budget)return 0;auto next=Stage(unsigned(stage_)+1);switch(stage_){case Stage::Count:remaining_=128;index_=0;break;case Stage::Read:byte_=uint8_t(read(index_));break;case Stage::Write:write(index_,byte_);break;case Stage::Advance:++index_;break;case Stage::Decrement:--remaining_;break;case Stage::Repeat:if(remaining_)next=Stage::Read;break;case Stage::Invalidate:case Stage::Unpack:case Stage::ResetDsp:case Stage::RefreshPanel:case Stage::InitVoice:waiting_=true;break;default:break;}stage_=next;return cost;}
private:Stage stage_=Stage::BindTarget;uint32_t source_;unsigned index_=0,remaining_=0;uint8_t byte_=0;bool waiting_=false;
};
class ProgramLoadTask final {
public:
 enum class Stage {ResolveAddress,Copy,ModeLatch,ClearVoice,ReleaseVoices,ClearPending,PublishPending,Return,Complete};
 Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}bool awaiting()const{return waiting_;}bool resumeChild(){if(!waiting_)return false;waiting_=false;return true;}
 unsigned nextClocks(const FlashCredits&c)const{if(waiting_)return 0;switch(stage_){case Stage::ResolveAddress:case Stage::Copy:return c.shortCall;case Stage::ModeLatch:case Stage::ReleaseVoices:return c.call;case Stage::PublishPending:return 9+c.write;case Stage::Return:return c.ret;case Stage::Complete:return 0;default:return 3;}}
 template<class W>unsigned advanceBefore(unsigned budget,const FlashCredits&c,W&&write){auto cost=nextClocks(c);if(!cost||cost>budget)return 0;switch(stage_){case Stage::ResolveAddress:case Stage::Copy:case Stage::ModeLatch:case Stage::ReleaseVoices:waiting_=true;break;case Stage::PublishPending:write(0);break;default:break;}stage_=Stage(unsigned(stage_)+1);return cost;}
private:Stage stage_=Stage::ResolveAddress;bool waiting_=false;
};
class ProgramMirrorClearTask final {
public:enum class Stage {Bind,Count,Prepare,Write,Advance,Decrement,Repeat,Return,Complete};Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}unsigned index()const{return index_;}unsigned nextClocks(const FlashCredits&c)const{switch(stage_){case Stage::Bind:return 9;case Stage::Count:return 6;case Stage::Write:return 3+c.write;case Stage::Return:return c.ret;case Stage::Complete:return 0;default:return 3;}}
 template<class W>unsigned advanceBefore(unsigned budget,const FlashCredits&c,W&&write){auto cost=nextClocks(c);if(!cost||cost>budget)return 0;auto next=Stage(unsigned(stage_)+1);switch(stage_){case Stage::Count:remaining_=205;break;case Stage::Write:write(index_,255);break;case Stage::Advance:++index_;break;case Stage::Decrement:--remaining_;break;case Stage::Repeat:if(remaining_)next=Stage::Write;break;default:break;}stage_=next;return cost;}
private:Stage stage_=Stage::Bind;unsigned index_=0,remaining_=0;};
class ProgramModeTask final {
public:enum class Stage {Retain,Bind,Read,Compare,Select,Prepare,Publish,Restore,Return,Complete};Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}unsigned nextClocks(const FlashCredits&c)const{switch(stage_){case Stage::Retain:return c.pushLong;case Stage::Bind:return 9;case Stage::Read:return 12+c.read;case Stage::Publish:return 9+c.write;case Stage::Restore:return c.popLong;case Stage::Return:return c.ret;case Stage::Complete:return 0;default:return 3;}}
 template<class R,class W>unsigned advanceBefore(unsigned budget,const FlashCredits&c,R&&read,W&&write){auto cost=nextClocks(c);if(!cost||cost>budget)return 0;auto next=Stage(unsigned(stage_)+1);switch(stage_){case Stage::Read:mode_=uint8_t(read());break;case Stage::Select:if(mode_!=1)next=Stage::Restore;break;case Stage::Publish:write(1);break;default:break;}stage_=next;return cost;}
private:Stage stage_=Stage::Retain;uint8_t mode_=0;};
class ProgramDspResetTask final {
public:enum class Stage {Address,Word,Send,Return,Complete};Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}bool awaiting()const{return waiting_;}bool resumeChild(){if(!waiting_)return false;waiting_=false;return true;}unsigned nextClocks(const FlashCredits&c)const{if(waiting_)return 0;switch(stage_){case Stage::Address:return 6;case Stage::Word:return 9;case Stage::Send:return c.call;case Stage::Return:return c.ret;case Stage::Complete:return 0;}return 0;}
 unsigned advanceBefore(unsigned budget,const FlashCredits&c){auto cost=nextClocks(c);if(!cost||cost>budget)return 0;if(stage_==Stage::Send)waiting_=true;stage_=Stage(unsigned(stage_)+1);return cost;}
private:Stage stage_=Stage::Address;bool waiting_=false;};
}
