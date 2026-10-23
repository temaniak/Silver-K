#pragma once
#include <cstdint>
namespace ks {
// Menu parameter edit: wrapped delta, signed lower clamp, bit actions and original three-state transitions.
class StateScreenEditTask final {
public:
 enum class Kind { Edit };enum class Stage { ReadDescriptor, ReadParameter, NormalizeParameterWord, NormalizeParameterLong, BindEditContext, ReadEditBase, SelectEditBase, ReadPatchBase, LocateParameter, ReadMinimum, ReadMaximum, ReadParameterValue, CompareBitMode, SelectBitMode, CompareTriMode, SelectTriMode, AddDelta, TestDelta, SelectDeltaSign, CompareMaximum, SelectMaximum, ClampMaximum, JoinMaximum, CompareMinimum, SelectMinimum, ClampMinimum, JoinMinimum, TestBitDelta, SelectBitDelta, SetBit, JoinSetBit, ClearBit, JoinClearBit, TestTriDelta, SelectTriZero, SelectTriSign, CompareTriMiddle, SelectTriMiddle, SelectTriHigh, SetTriHigh, JoinTriHigh, SetTriMiddle, JoinTriMiddle, CompareTriZero, SelectTriZeroValue, CompareTriHigh, SelectTriHighValue, SetTriZero, JoinTriZero, SetTriLow, PublishValue, RefreshLabel, ClearContext, BindPanelSettings, ReadPanelLevel, ApplyPanelLevel, ReturnEdit, AwaitChild, Complete };
 enum class Read { Descriptor, EditBase, PatchBase, Byte };
 uint32_t descriptor()const{return descriptor_;}uint32_t base()const{return base_;}uint8_t parameter()const{return parameter_;}uint8_t minimum()const{return minimum_;}uint8_t maximum()const{return maximum_;}uint8_t value()const{return value_;}uint8_t level()const{return level_;}
 explicit StateScreenEditTask(Kind kind,uint8_t delta=0){delta_=delta;switch(kind){case Kind::Edit:stage_=Stage::ReadDescriptor;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(){if(!awaitingChild())return false;stage_=resume_;return true;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::ReadDescriptor:case Stage::ReadEditBase:case Stage::ReadPatchBase:return 18; case Stage::ReadParameter:case Stage::ClearContext:case Stage::ReadPanelLevel:return 15; case Stage::BindEditContext:case Stage::PublishValue:case Stage::RefreshLabel:case Stage::ApplyPanelLevel:return 20; case Stage::ReadMinimum:case Stage::ReadMaximum:case Stage::BindPanelSettings:return 9; case Stage::ReadParameterValue:return 6; case Stage::ReturnEdit:return 17; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::ReadDescriptor:descriptor_=read(Read::Descriptor,0);next();break;
case Stage::ReadParameter:parameter_=uint8_t(read(Read::Byte,descriptor_+16));next();break;
case Stage::ReadEditBase:base_=read(Read::EditBase,0);next();break;
case Stage::SelectEditBase:stage_=base_?Stage::LocateParameter:Stage::ReadPatchBase;break;
case Stage::ReadPatchBase:base_=read(Read::PatchBase,0);next();break;
case Stage::ReadMinimum:minimum_=uint8_t(read(Read::Byte,descriptor_+18));next();break;
case Stage::ReadMaximum:maximum_=uint8_t(read(Read::Byte,descriptor_+19));next();break;
case Stage::ReadParameterValue:value_=uint8_t(read(Read::Byte,base_+parameter_));next();break;
case Stage::SelectBitMode:stage_=minimum_==128?Stage::TestBitDelta:Stage::CompareTriMode;break;
case Stage::SelectTriMode:stage_=minimum_==129?Stage::TestTriDelta:Stage::AddDelta;break;
case Stage::AddDelta:value_=uint8_t(value_+delta_);next();break;
case Stage::SelectDeltaSign:stage_=delta_&128?Stage::CompareMinimum:Stage::CompareMaximum;break;
case Stage::SelectMaximum:stage_=value_<=maximum_?Stage::PublishValue:Stage::ClampMaximum;break;
case Stage::ClampMaximum:value_=maximum_;next();break;
case Stage::JoinMaximum:stage_=Stage::PublishValue;break;
case Stage::SelectMinimum:stage_=int8_t(value_)>=int8_t(minimum_)?Stage::PublishValue:Stage::ClampMinimum;break;
case Stage::ClampMinimum:value_=minimum_;next();break;
case Stage::JoinMinimum:stage_=Stage::PublishValue;break;
case Stage::SelectBitDelta:stage_=delta_&128?Stage::ClearBit:Stage::SetBit;break;
case Stage::SetBit:value_|=uint8_t(1u<<(maximum_&7));next();break;
case Stage::JoinSetBit:stage_=Stage::PublishValue;break;
case Stage::ClearBit:value_&=uint8_t(~(1u<<(maximum_&7)));next();break;
case Stage::JoinClearBit:stage_=Stage::PublishValue;break;
case Stage::SelectTriZero:stage_=delta_?Stage::SelectTriSign:Stage::PublishValue;break;
case Stage::SelectTriSign:stage_=delta_&128?Stage::CompareTriZero:Stage::CompareTriMiddle;break;
case Stage::SelectTriMiddle:stage_=value_==63?Stage::PublishValue:Stage::SelectTriHigh;break;
case Stage::SelectTriHigh:stage_=value_>63?Stage::SetTriMiddle:Stage::SetTriHigh;break;
case Stage::SetTriHigh:value_=127;next();break;
case Stage::JoinTriHigh:stage_=Stage::PublishValue;break;
case Stage::SetTriMiddle:value_=63;next();break;
case Stage::JoinTriMiddle:stage_=Stage::PublishValue;break;
case Stage::SelectTriZeroValue:stage_=value_?Stage::CompareTriHigh:Stage::PublishValue;break;
case Stage::SelectTriHighValue:stage_=value_==127?Stage::SetTriZero:Stage::SetTriLow;break;
case Stage::SetTriZero:value_=0;next();break;
case Stage::JoinTriZero:stage_=Stage::PublishValue;break;
case Stage::SetTriLow:value_=127;next();break;
case Stage::ReadPanelLevel:level_=uint8_t(read(Read::Byte,0x168fc+228));next();break;
case Stage::BindEditContext:await(Stage::ReadEditBase);break;
case Stage::PublishValue:await(Stage::RefreshLabel);break;
case Stage::RefreshLabel:await(Stage::ClearContext);break;
case Stage::ClearContext:await(Stage::BindPanelSettings);break;
case Stage::ApplyPanelLevel:await(Stage::ReturnEdit);break;
case Stage::ReturnEdit:stage_=Stage::Complete;break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;uint32_t descriptor_=0,base_=0;uint8_t delta_=0,parameter_=0,minimum_=0,maximum_=0,value_=0,level_=0;
};
}
