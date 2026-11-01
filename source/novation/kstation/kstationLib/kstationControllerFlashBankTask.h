#pragma once
#include "kstationFlashCommitTask.h"
#include "kstationProgramStateTask.h"
#include <optional>
namespace kstation {
// Prepares one bank page and yields its source and destination to the flash writer.
class ControllerFlashBankTask final {
 using T=FlashBankStoreTask;using S=T::Stage;using I=ProgramIndexTask;using IS=I::Stage;
public:
 struct Commit {uint32_t source=0,target=0;};
 ControllerFlashBankTask(T::Kind kind,uint32_t source,uint32_t indexSource=0xcd14):task_(kind,source),input_(source),indexSource_(indexSource){}
 bool complete()const{return task_.complete();}
 bool awaitingCommit()const{return task_.awaitingChild()&&task_.child()==T::Child::Commit;}
 Commit commit()const{return awaitingCommit()?Commit{task_.kind()==T::Kind::Patch?0xce1eu:input_,task_.flash()&0xffff00u}:Commit{};}
 bool resumeCommit(){return awaitingCommit()&&task_.resume(T::Child::Commit);}
 bool stackWrites()const{return index_?index_->stage()==IS::Retain:task_.stage()==S::Index;}
 unsigned nextClocks(int previous=-1)const{
  if(complete()||awaitingCommit())return 0;
  if(index_){FlashCredits c;c.read=bus(indexSource_+(index_->stage()==IS::ReadProgram),previous);return index_->nextClocks(c);}
  switch(task_.stage()){
   case S::ReadParity:return 6+bus(0xcd15,previous);
   case S::Index:return 10;
   case S::CompareIndex:case S::Scale:case S::Count:case S::CountOther:return 6;
   case S::SelectParity:return 8;
   case S::SelectIndex:return task_.kind()==T::Kind::Patch?3:8;
   case S::Multiply:return 23;
   case S::AddBase:case S::AddBuffer:case S::AddOtherBuffer:case S::BindBuffer:return 9;
   case S::Read:case S::ReadOther:return 5+bus(task_.source(),previous);
   case S::Write:case S::WriteOther:return 3+bus(task_.destination(),previous);
   case S::Commit:return task_.kind()==T::Kind::Patch?3:8;
   case S::ReturnInvalid:return 7;
   case S::AwaitChild:case S::Complete:return 0;
   default:return 3;
  }
 }
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){
  unsigned cost=nextClocks(previous);if(!cost||cost>budget)return 0;
  if(index_){FlashCredits c;c.read=bus(indexSource_+(index_->stage()==IS::ReadProgram),previous);auto n=index_->advanceBefore(cost,c,[&](bool second){return read(indexSource_+second,1,cost-(second?0:2));});if(index_->complete()){auto result=index_->result();index_.reset();task_.resume(T::Child::Index,result);}return n;}
  auto ok=task_.advance([&](uint32_t address,unsigned data){return uint8_t(read(data?address:0xcd15,1,cost-(data?2:0)));},[&](uint32_t address,uint8_t v){write(address,v,1,cost);});
  if(task_.awaitingChild()&&task_.child()==T::Child::Index)index_.emplace(indexSource_);
  return ok?cost:0;
 }
private:
 static unsigned bus(uint32_t a,int p){a&=0xffffff;if(a<0x400000||(a>=0xfffd10&&a<0xffffa0))return (p&1)||a!=uint32_t(p|1)?(a<0x400000?3:1):0;return a>=0xffffa0?1:3;}
 T task_;uint32_t input_,indexSource_;std::optional<I>index_{};
};
}
