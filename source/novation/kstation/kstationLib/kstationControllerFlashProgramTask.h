#pragma once
#include "kstationFlashStateTask.h"
#include <optional>
namespace kstation {
// Exact cooperative page programming policy; all memory and Flash operations use the board bus.
class ControllerFlashProgramTask final {
 using B=FlashBackupTask;using BS=B::Stage;using P=FlashPageTask;using PS=P::Stage;using C=FlashCompareTask;using CS=C::Stage;
public:
 enum class MaskAction:uint8_t{None,Save,Disable,Restore};
 ControllerFlashProgramTask(uint32_t source,uint32_t target):source_(source&0xffffff),target_(target&0xffffff){}
 bool complete()const{return backup_.complete()&&!page_&&!compare_;}
 bool awaitingNotice()const{return backup_.awaiting()&&!page_&&!compare_&&backup_.stage()==BS::RestoreTarget;}
 bool resumeNotice(){return awaitingNotice()&&backup_.resume();}
 MaskAction maskAction()const{if(!page_||compare_)return MaskAction::None;auto s=page_->stage();return s==PS::SaveFlags?MaskAction::Save:s==PS::Mask?MaskAction::Disable:s==PS::RestoreFlags?MaskAction::Restore:MaskAction::None;}
 bool stackWrites()const{if(compare_){auto s=compare_->stage();return s==CS::RetainScratch||s==CS::RetainTarget||s==CS::RetainSource;}if(page_){auto s=page_->stage();return s==PS::RetainScratch||s==PS::RetainPointer||s==PS::RetainByte||s==PS::RetainTick||s==PS::CompareBefore||s==PS::CompareAfter||s==PS::SaveFlags;}auto s=backup_.stage();return s==BS::Program||s==BS::RetainSource||s==BS::RetainTarget||s==BS::Notify;}
 unsigned nextClocks(int previous=-1)const{if(awaitingNotice()||complete())return 0;auto cr=credits(previous);if(compare_)return compare_->nextClocks(cr);if(page_)return page_->nextClocks(cr);return backup_.nextClocks(cr);}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;auto cr=credits(previous);
  if(compare_){auto n=compare_->advanceBefore(cost,cr,[&](C::Read f,unsigned i){return read((f==C::Read::Source?source_:target_)+i,1,cost-2);});if(compare_->complete()){bool matched=compare_->matches();compare_.reset();page_->resumeCompare(matched);}return n;}
  if(page_){auto n=page_->advanceBefore(cost,cr,[&](P::Read f,unsigned i){uint32_t a=f==P::Read::Dirty?0xfffe32:f==P::Read::Source?source_+i:f==P::Read::Last?target_+i:0xfffd20;unsigned width=f==P::Read::Dirty?2:f==P::Read::Tick||f==P::Read::TickStart?4:1;return read(a,width,cost-(f==P::Read::Source?2:0));},[&](P::Write f,unsigned i,uint32_t v){uint32_t a=f==P::Write::Dirty?0xfffe32:f==P::Write::Target?target_+i:(target_&0xf80000)+(f==P::Write::Unlock2?0x2aaa:0x5555);write(a,v,f==P::Write::Dirty?2:1,cost);});if(page_->awaitingCompare()&&!compare_)compare_.emplace(source_,target_);if(page_->complete()){page_.reset();backup_.resume();}return n;}
  auto n=backup_.advanceBefore(cost,cr,[&](){return read(0xfffe32,2,cost);},[&](uint16_t v){write(0xfffe32,v,2,cost);});if(backup_.awaiting()&&backup_.stage()==BS::RetainSource)page_.emplace(source_,target_);return n;
 }
private:
 static unsigned bus(uint32_t a,unsigned width,int p){unsigned sum=0;for(unsigned i=0;i<width;++i){auto b=(a+i)&0xffffff;if(b<0x400000||(b>=0xfffd10&&b<0xffffa0)){if((p&1)||b!=uint32_t(p|1))sum+=b<0x400000?3:1;p=int(b);}else{sum+=b>=0xffffa0?1:3;p=-1;}}return sum;}
 FlashCredits credits(int previous)const{FlashCredits c;c.saveFlags=9;c.restoreFlags=9;if(compare_){auto s=compare_->stage();if(s==CS::ReadSource)c.read=bus(source_+compare_->index(),1,-1);else if(s==CS::ReadTarget)c.read=bus(target_+compare_->index(),1,-1);}else if(page_){auto s=page_->stage();if(s==PS::ReadDirty)c.read=bus(0xfffe32,2,-1);else if(s==PS::ReadSource)c.read=bus(source_+page_->index(),1,-1);else if(s==PS::ReadStartTick||s==PS::ReadTick)c.read=bus(0xfffd20,4,-1);else if(s==PS::ReadLast)c.read=bus(target_+255,1,-1);else if(s==PS::WriteDirty)c.write=bus(0xfffe32,2,previous);else if(s==PS::Unlock1||s==PS::Program)c.write=bus((target_&0xf80000)+0x5555,1,previous);else if(s==PS::Unlock2)c.write=bus((target_&0xf80000)+0x2aaa,1,previous);else if(s==PS::WriteTarget)c.write=bus(target_+page_->index(),1,previous);}else{if(backup_.stage()==BS::ReadDirty)c.read=bus(0xfffe32,2,-1);else if(backup_.stage()==BS::WriteDirty)c.write=bus(0xfffe32,2,previous);}return c;}
 uint32_t source_,target_;B backup_{};std::optional<P>page_{};std::optional<C>compare_{};
};
}
