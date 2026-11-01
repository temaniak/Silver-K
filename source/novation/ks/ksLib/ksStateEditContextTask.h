#pragma once
#include <cstdint>
namespace ks {
// Choose global/patch edit context and display live values against the retained reference.
class StateEditContextTask final {
public:
 enum class Kind { Bind, Clear, Display };enum class Stage { SaveContextValue, SaveContextIndex, BindGlobalBase, ReadContextKind, CompareContextKind, SelectContextKind, ReadScreen, SelectGlobalScreen, CompareCalibrationScreen, SelectCalibrationScreen, ClearContextBase, WriteContextBase, RestoreContextIndex, RestoreContextValue, ReturnContext, SaveClearValue, ClearBase, WriteClearedBase, RestoreClearValue, ReturnClear, NormalizeValueIndex, NormalizeValueLong, RetainValueIndex, ReadEditBase, SelectEditBase, ReadPatchBase, LocateValue, ReadValue, ReadValueKind, SelectValueKind, BindComparedValue, SaveComparedValue, SaveComparedIndex, BindValuePosition, DisplayCurrentValue, RestoreComparedIndex, ReadReferenceValue, RestoreComparedValue, CompareReferenceValue, SelectReferenceValue, BindMatchText, PrintMatchText, JoinMatch, BindReferencePosition, DisplayReferenceValue, ReturnValue, AwaitChild, Complete };
 enum class Read { ContextKind, Screen, EditBase, PatchBase, Value, ValueKind };enum class Write { Base };
 uint32_t index()const{return index_;}uint32_t base()const{return base_;}uint8_t kind()const{return kind_;}uint8_t screen()const{return screen_;}uint8_t value()const{return value_;}
 explicit StateEditContextTask(Kind kind,uint32_t index=0){index_=index;switch(kind){case Kind::Bind:stage_=Stage::SaveContextValue;break;case Kind::Clear:stage_=Stage::SaveClearValue;break;case Kind::Display:stage_=Stage::NormalizeValueIndex;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(uint8_t reference){if(!awaitingChild())return false;if(resume_==Stage::RestoreComparedValue)reference_=reference;stage_=resume_;return true;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::SaveContextValue:case Stage::RestoreContextValue:case Stage::SaveClearValue:case Stage::RestoreClearValue:case Stage::SaveComparedIndex:case Stage::RestoreComparedIndex:case Stage::ReadReferenceValue:case Stage::PrintMatchText:return 20; case Stage::SaveContextIndex:case Stage::RestoreContextIndex:case Stage::SaveComparedValue:case Stage::RestoreComparedValue:return 11; case Stage::BindGlobalBase:case Stage::BindMatchText:return 9; case Stage::ReadContextKind:case Stage::ReadValueKind:case Stage::DisplayCurrentValue:case Stage::DisplayReferenceValue:return 15; case Stage::ReadScreen:return 12; case Stage::WriteContextBase:case Stage::WriteClearedBase:case Stage::ReadEditBase:case Stage::ReadPatchBase:return 18; case Stage::ReturnContext:case Stage::ReturnClear:case Stage::ReturnValue:return 17; case Stage::ReadValue:return 6; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::BindGlobalBase:base_=0x168fc;next();break;
case Stage::ReadContextKind:kind_=uint8_t(read(Read::ContextKind,index_));next();break;
case Stage::SelectContextKind:stage_=kind_<126?Stage::ReadScreen:Stage::WriteContextBase;break;
case Stage::ReadScreen:screen_=uint8_t(read(Read::Screen,0));next();break;
case Stage::SelectGlobalScreen:stage_=screen_?Stage::CompareCalibrationScreen:Stage::WriteContextBase;break;
case Stage::SelectCalibrationScreen:stage_=screen_==9?Stage::WriteContextBase:Stage::ClearContextBase;break;
case Stage::ClearContextBase:base_=0;next();break;
case Stage::WriteContextBase:write(Write::Base,base_);next();break;
case Stage::ClearBase:base_=0;next();break;
case Stage::WriteClearedBase:write(Write::Base,0);next();break;
case Stage::NormalizeValueIndex:index_&=255;next();break;
case Stage::ReadEditBase:base_=read(Read::EditBase,0);next();break;
case Stage::SelectEditBase:stage_=base_?Stage::LocateValue:Stage::ReadPatchBase;break;
case Stage::ReadPatchBase:base_=read(Read::PatchBase,0);next();break;
case Stage::ReadValue:value_=uint8_t(read(Read::Value,base_+index_));next();break;
case Stage::ReadValueKind:kind_=uint8_t(read(Read::ValueKind,index_));next();break;
case Stage::SelectValueKind:stage_=kind_?Stage::BindComparedValue:Stage::ReturnValue;break;
case Stage::SelectReferenceValue:stage_=value_==reference_?Stage::BindMatchText:Stage::BindReferencePosition;break;
case Stage::JoinMatch:stage_=Stage::ReturnValue;break;
case Stage::ReturnContext:stage_=Stage::Complete;break;
case Stage::ReturnClear:stage_=Stage::Complete;break;
case Stage::DisplayCurrentValue:await(Stage::RestoreComparedIndex);break;
case Stage::ReadReferenceValue:await(Stage::RestoreComparedValue);break;
case Stage::PrintMatchText:await(Stage::JoinMatch);break;
case Stage::DisplayReferenceValue:await(Stage::ReturnValue);break;
case Stage::ReturnValue:stage_=Stage::Complete;break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;uint32_t index_=0,base_=0;uint8_t kind_=0,screen_=0,value_=0,reference_=0;
};
}
