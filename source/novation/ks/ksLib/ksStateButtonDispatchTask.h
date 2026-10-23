#pragma once
#include <cstdint>
namespace ks {
// Button groups and descriptor-backed action dispatch. The optional refresh
// flag belongs to the descriptor read before the action child, not its reply.
class StateButtonDispatchTask final {
public:
 enum class Kind { Button, Action };enum class Stage { PopButton, TestButton, SelectButton, ReturnEmpty, SelectUiState, CaptureGroup, CaptureKey, MaskGroup, ComparePartGroup, SelectPartGroup, ComparePageGroup, SelectPageGroup, CompareFunctionGroup, SelectFunctionGroup, SetDescriptorStride, ScaleDescriptor, NormalizeDescriptor, LocateDescriptor, ReadTag, ReadKey, ReadValue, SaveTag, NormalizeTag, DispatchAction, RestoreTag, TestRefresh, SelectRefresh, ReturnButton, CompareDirectAction, SelectDirectAction, CompareToggleAction, SelectToggleAction, CompareValueAction, SelectValueAction, ReturnUnknown, AwaitChild, Complete };enum class Read { Tag, Key, Value };
 StateButtonDispatchTask(Kind kind,uint8_t action=0):stage_(kind==Kind::Button?Stage::PopButton:Stage::CompareDirectAction),action_(action){}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(uint16_t event){if(!awaitingChild())return false;event_=event;stage_=resume_;return true;}
 uint16_t event()const{return event_;}unsigned offset()const{return unsigned(index_)*3;}uint8_t tag()const{return tag_;}uint8_t key()const{return key_;}uint8_t value()const{return value_;}
 unsigned nextClocks()const{switch(stage_){case Stage::ReadValue:return 6; case Stage::SelectPageGroup:case Stage::SelectFunctionGroup:case Stage::ReadTag:case Stage::ReadKey:case Stage::SelectRefresh:case Stage::SelectDirectAction:case Stage::SelectToggleAction:case Stage::SelectValueAction:return 8; case Stage::SelectUiState:case Stage::LocateDescriptor:return 9; case Stage::SaveTag:case Stage::RestoreTag:return 11; case Stage::ScaleDescriptor:case Stage::DispatchAction:return 15; case Stage::ReturnEmpty:case Stage::ReturnButton:case Stage::ReturnUnknown:return 17; case Stage::PopButton:return 20; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader>unsigned advanceBefore(unsigned available,Reader&& read){const auto cost=nextClocks();if(!cost||cost>available)return 0;switch(stage_){case Stage::PopButton:await(Stage::TestButton);break;
case Stage::SelectButton:stage_=event_&0x8000?Stage::ReturnEmpty:Stage::SelectUiState;break;
case Stage::CaptureGroup:group_=uint8_t(event_);next();break;
case Stage::CaptureKey:index_=uint8_t(event_)&31;next();break;
case Stage::MaskGroup:group_&=0xe0;next();break;
case Stage::SelectPartGroup:stage_=group_==0x20?Stage::Complete:Stage::ComparePageGroup;break;
case Stage::SelectPageGroup:stage_=group_==0x40?Stage::Complete:Stage::CompareFunctionGroup;break;
case Stage::SelectFunctionGroup:stage_=group_==0x60?Stage::Complete:Stage::SetDescriptorStride;break;
case Stage::ReadTag:tag_=uint8_t(read(Read::Tag));next();break;
case Stage::ReadKey:key_=uint8_t(read(Read::Key));next();break;
case Stage::ReadValue:value_=uint8_t(read(Read::Value));next();break;
case Stage::NormalizeTag:action_=tag_&127;next();break;
case Stage::DispatchAction:await(Stage::RestoreTag);break;
case Stage::SelectRefresh:stage_=tag_&128?Stage::Complete:Stage::ReturnButton;break;
case Stage::SelectDirectAction:stage_=action_==0?Stage::Complete:Stage::CompareToggleAction;break;
case Stage::SelectToggleAction:stage_=action_==1?Stage::Complete:Stage::CompareValueAction;break;
case Stage::SelectValueAction:stage_=action_==2?Stage::Complete:Stage::ReturnUnknown;break;
case Stage::ReturnEmpty:stage_=Stage::Complete;break;
case Stage::ReturnButton:stage_=Stage::Complete;break;
case Stage::ReturnUnknown:stage_=Stage::Complete;break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_,resume_=Stage::Complete;uint16_t event_=0;uint8_t group_=0,index_=0,tag_=0,key_=0,value_=0,action_;
};
}
