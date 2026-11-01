#pragma once
#include "kstationFlashStateTask.h"
namespace kstation {
// Standalone LCD services used by startup and state writes. Timer1 retains its
// own independent owner while handling an interrupt.
class LcdTask final {
public:
 enum class Kind {Command,Ready,Read,Write,Notify};
 enum class Stage {Retain,Prepare1,Publish1,Prepare2,Publish2,Prepare3,Publish3,Pause1,Pause2,Pause3,ReadData,Prepare4,Publish4,Prepare5,Publish5,Restore,Return,DispatchReady,RestoreCommand,DispatchWrite,FirstRead,TestReady,SelectReady,FinalRead,BindNotice,DispatchNotice,BindNoticeCount,PublishNoticeCount,Complete};
 enum class Write {Direction,Port,Data,NoticeCount};
 explicit LcdTask(Kind kind=Kind::Read,uint8_t value=0):kind_(kind),stage_(kind==Kind::Ready?Stage::FirstRead:Stage::Retain),value_(value),command_(value){}
 Kind kind()const{return kind_;}Stage stage()const{return stage_;}bool complete()const{return stage_==Stage::Complete;}bool awaiting()const{return awaiting_;}uint8_t value()const{return kind_==Kind::Read?data_:value_;}
 bool resumeChild(uint8_t value){if(!awaiting_)return false;awaiting_=false;if(kind_==Kind::Ready)value_=value;return true;}
 unsigned nextClocks(const FlashCredits&c)const{if(awaiting_)return 0;switch(stage_){case Stage::Retain:return c.pushWord;case Stage::Restore:case Stage::RestoreCommand:return c.popWord;case Stage::DispatchReady:case Stage::FirstRead:case Stage::FinalRead:return c.shortCall;case Stage::DispatchWrite:case Stage::DispatchNotice:return c.call;case Stage::Publish1:case Stage::Publish2:case Stage::Publish3:case Stage::Publish4:case Stage::Publish5:return 3+c.write;case Stage::ReadData:return 3+c.read;case Stage::PublishNoticeCount:return 9+c.write;case Stage::Return:return c.ret;case Stage::Complete:return 0;default:return 3;}}
 template<class R,class W>unsigned advanceBefore(unsigned budget,const FlashCredits&c,R&&read,W&&write){auto cost=nextClocks(c);if(!cost||cost>budget)return 0;auto next=Stage(unsigned(stage_)+1);switch(stage_){
 case Stage::Retain:next=kind_==Kind::Command?Stage::DispatchReady:kind_==Kind::Notify?Stage::BindNotice:Stage::Prepare1;break;
 case Stage::Prepare1:value_=kind_==Kind::Read?0:0x40;break;
 case Stage::Publish1:write(kind_==Kind::Read?Write::Direction:Write::Port,value_);break;
 case Stage::Prepare2:value_=kind_==Kind::Read?0x50:0xc0;break;
 case Stage::Publish2:write(Write::Port,value_);next=kind_==Kind::Read?Stage::Prepare3:Stage::Publish3;break;
 case Stage::Prepare3:value_=0xd0;break;
 case Stage::Publish3:write(kind_==Kind::Read?Write::Port:Write::Data,kind_==Kind::Read?value_:command_);break;
 case Stage::Pause3:next=kind_==Kind::Read?Stage::ReadData:Stage::Prepare4;break;
 case Stage::ReadData:data_=uint8_t(read());break;
 case Stage::Prepare4:value_=kind_==Kind::Read?0x50:0x40;break;
 case Stage::Publish4:write(Write::Port,value_);next=kind_==Kind::Read?Stage::Prepare5:Stage::Restore;break;
 case Stage::Prepare5:value_=0xff;break;
 case Stage::Publish5:write(Write::Direction,value_);break;
 case Stage::Restore:next=Stage::Return;break;
 case Stage::Return:next=Stage::Complete;break;
 case Stage::DispatchReady:awaiting_=true;next=Stage::RestoreCommand;break;
 case Stage::RestoreCommand:next=Stage::DispatchWrite;break;
 case Stage::DispatchWrite:awaiting_=true;next=Stage::Return;break;
 case Stage::FirstRead:awaiting_=true;next=Stage::TestReady;break;
 case Stage::TestReady:next=Stage::SelectReady;break;
 case Stage::SelectReady:next=(value_&0x80)?Stage::FirstRead:Stage::FinalRead;break;
 case Stage::FinalRead:awaiting_=true;next=Stage::Return;break;
 case Stage::BindNotice:value_=0x0e;next=Stage::DispatchNotice;break;
 case Stage::DispatchNotice:awaiting_=true;next=Stage::BindNoticeCount;break;
 case Stage::BindNoticeCount:value_=0x19;next=Stage::PublishNoticeCount;break;
 case Stage::PublishNoticeCount:write(Write::NoticeCount,value_);next=Stage::Restore;break;
 default:break;}stage_=next;return cost;}
 void command(uint8_t v){command_=v;}
private:Kind kind_;Stage stage_;uint8_t value_=0,command_=0,data_=0;bool awaiting_=false;
};
}
