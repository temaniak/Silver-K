#pragma once
#include "ksStateFieldMapTask.h"
#include <array>
#include <cstdint>
namespace ks {
// One engine parameter edit: map the public control, update patch/live mirrors,
// expand packed fields and notify the original engine observers in original order.
class StateParameterEditTask final {
public:
 enum class Entry {Public,Internal};enum class Stage {ComparePublic,AdmitPublic,ReturnPublic,MapControl,AdmitMapping,AdmitMapped,ReturnRejected,BankParameter,ClearParameter,CaptureParameter,ClearField,ReadField,ReadType,CompareType,AdmitType,WriteGlobal,WriteGlobalMirror,SkipToNotification,ReadInhibit,AdmitInhibit,RetainRequest,RetainField,BindCurrent,RestoreField,RestoreRequest,ReadCurrentBase,AddCurrentField,WriteCurrent,ReadLiveBase,AddLiveField,WriteLive,RetainNotifyField,ExpandFields,RebindMode,MarkDirty,RestoreNotifyField,ReadNotificationMode,AdmitNotificationMode,ReadNotificationField,CompareNotificationField,AdmitNotificationField,SkipDisplay,ProbeDisplay,AdmitDisplay,RefreshDisplay,MarkEdited,WriteEdited,ReturnEdited,AwaitChild,Complete};enum class Read {Field,Type,Inhibit,CurrentBase,LiveBase,NotificationMode,NotificationField};enum class Write {Global,GlobalMirror,Current,Live,Edited};enum class Child {MapControl,BindCurrent,ExpandFields,RebindMode,MarkDirty,ProbeDisplay,RefreshDisplay};
 StateParameterEditTask(Entry e,uint8_t parameter,uint8_t value,uint32_t fieldContext):phase_(e==Entry::Public?Stage::ComparePublic:Stage::ClearParameter),parameter_(parameter),value_(value),field_(fieldContext){}
 Stage stage()const{return phase_;}Stage resumeStage()const{return resume_;}bool complete()const{return phase_==Stage::Complete;}bool awaitingChild()const{return phase_==Stage::AwaitChild;}Child child()const{return child_;}
 uint8_t parameter()const{return parameter_;}uint8_t value()const{return value_;}uint32_t field()const{return field_;}
 bool resumeChild(StateFieldMapTask::Outcome result=StateFieldMapTask::Outcome::Unmapped,uint32_t field=0,uint8_t value=0,bool display=false){if(!awaitingChild())return false;if(child_==Child::MapControl){mapping_=result;field_=field;value_=value;}if(child_==Child::ProbeDisplay)display_=display;phase_=resume_;return true;}
 unsigned nextClocks()const{constexpr std::array<unsigned,48> c{{3,3,17,20,3,3,17,3,3,3,3,15,15,3,3,15,15,3,12,3,11,20,20,20,11,18,3,6,18,3,6,11,20,20,20,11,12,3,12,3,3,3,20,3,20,3,12,17}};return unsigned(phase_)<c.size()?c[unsigned(phase_)]:0;}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write){unsigned used=0;while(const auto cost=nextClocks()){if(cost>available-used)break;switch(phase_){
 case Stage::AdmitPublic:phase_=parameter_<=127?Stage::MapControl:Stage::ReturnPublic;break;
 case Stage::MapControl:await(Child::MapControl,Stage::AdmitMapping);break;
 case Stage::AdmitMapping:phase_=mapping_==StateFieldMapTask::Outcome::Unmapped?Stage::BankParameter:Stage::AdmitMapped;break;
 case Stage::AdmitMapped:phase_=mapping_==StateFieldMapTask::Outcome::AboveLimit?Stage::ReturnRejected:Stage::ReadCurrentBase;break;
 case Stage::BankParameter:parameter_=uint8_t(parameter_+128);next();break;
 case Stage::ClearField:field_=0;next();break;case Stage::ReadField:field_=uint8_t(read(Read::Field));next();break;case Stage::ReadType:type_=uint8_t(read(Read::Type));next();break;case Stage::AdmitType:phase_=type_<126?Stage::ReadInhibit:Stage::WriteGlobal;break;
 case Stage::WriteGlobal:write(Write::Global,value_);next();break;case Stage::WriteGlobalMirror:write(Write::GlobalMirror,value_);next();break;case Stage::SkipToNotification:phase_=Stage::ReadNotificationMode;break;
 case Stage::ReadInhibit:inhibit_=uint8_t(read(Read::Inhibit));next();break;case Stage::AdmitInhibit:phase_=inhibit_?Stage::ReturnEdited:Stage::RetainRequest;break;
 case Stage::BindCurrent:await(Child::BindCurrent,Stage::RestoreField);break;
 case Stage::ReadCurrentBase:(void)read(Read::CurrentBase);next();break;case Stage::WriteCurrent:write(Write::Current,value_);next();break;case Stage::ReadLiveBase:(void)read(Read::LiveBase);next();break;case Stage::WriteLive:write(Write::Live,value_);next();break;
 case Stage::ExpandFields:await(Child::ExpandFields,Stage::RebindMode);break;case Stage::RebindMode:await(Child::RebindMode,Stage::MarkDirty);break;case Stage::MarkDirty:await(Child::MarkDirty,Stage::RestoreNotifyField);break;
 case Stage::ReadNotificationMode:notificationMode_=uint8_t(read(Read::NotificationMode));next();break;case Stage::AdmitNotificationMode:phase_=notificationMode_?Stage::ReadNotificationField:Stage::ProbeDisplay;break;
 case Stage::ReadNotificationField:notificationField_=uint8_t(read(Read::NotificationField));next();break;case Stage::AdmitNotificationField:phase_=uint8_t(field_)==notificationField_?Stage::RefreshDisplay:Stage::SkipDisplay;break;case Stage::SkipDisplay:phase_=Stage::MarkEdited;break;
 case Stage::ProbeDisplay:await(Child::ProbeDisplay,Stage::AdmitDisplay);break;case Stage::AdmitDisplay:phase_=display_?Stage::RefreshDisplay:Stage::MarkEdited;break;case Stage::RefreshDisplay:await(Child::RefreshDisplay,Stage::MarkEdited);break;
 case Stage::WriteEdited:write(Write::Edited,1);next();break;case Stage::ReturnPublic:case Stage::ReturnRejected:case Stage::ReturnEdited:phase_=Stage::Complete;break;
 default:next();break;}used+=cost;}return used;}
private:
 void next(){phase_=Stage(unsigned(phase_)+1);}void await(Child c,Stage s){child_=c;resume_=s;phase_=Stage::AwaitChild;}Stage phase_,resume_=Stage::Complete;Child child_=Child::MapControl;StateFieldMapTask::Outcome mapping_=StateFieldMapTask::Outcome::Unmapped;uint8_t parameter_,value_,type_=0,inhibit_=0,notificationMode_=0,notificationField_=0;uint32_t field_;bool display_=false;
};}
