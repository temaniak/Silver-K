#pragma once
#include <cstdint>
namespace ks {
// StateStoredValue semantic policy: logical data, decisions and deferred effects; compatibility frame stays in Calls.
class StateStoredValueTask final {
public:
 enum class Kind { Read, Classify };enum class Stage { CompareGlobalField, SelectGlobalField, CompareUnusedField, SelectUnusedField, BindGlobalStorage, JoinGlobalStorage, ClearUnusedValue, JoinUnusedValue, ClassifyPatchField, SelectPatchField, ResolveAlternatePatch, SelectAlternatePatch, ResolvePatch, CompareMultiField, SelectMultiField, ReadMultiBase, LocateStoredField, ReadStoredField, ReturnStoredValue, CompareSpecialHigh, SelectSpecialHigh, CompareSpecialSentinel, SelectSpecialSentinel, CompareSpecialLow, SelectSpecialLow, CompareSpecialEnd, SelectSpecialEnd, MarkSpecial, ReturnSpecial, AwaitChild, Complete };
 enum class Read { Multi, Value };uint32_t base()const{return base_;}uint32_t pointer()const{return pointer_;}uint32_t value()const{return value_;}
 explicit StateStoredValueTask(Kind kind,uint32_t field=0){field_=uint8_t(field);offset_=field;pointer_=field;switch(kind){case Kind::Read:stage_=Stage::CompareGlobalField;break;case Kind::Classify:stage_=Stage::CompareSpecialHigh;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(uint32_t base,bool special){if(!awaitingChild())return false;if(resume_==Stage::SelectPatchField)special_=special;else {base_=base;alternate_=!special;}stage_=resume_;return true;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::BindGlobalStorage:return 9; case Stage::ClassifyPatchField:return 15; case Stage::ResolveAlternatePatch:case Stage::ResolvePatch:return 20; case Stage::ReadMultiBase:return 12+store; case Stage::ReadStoredField:return 3+store; case Stage::ReturnStoredValue:case Stage::ReturnSpecial:return 17; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::SelectGlobalField:stage_=field_<222?Stage::ClassifyPatchField:Stage::CompareUnusedField;break;
case Stage::SelectUnusedField:stage_=field_<234?Stage::ClearUnusedValue:Stage::BindGlobalStorage;break;
case Stage::BindGlobalStorage:base_=0x81fe16;next();break;
case Stage::ClearUnusedValue:value_=0;next();break;
case Stage::SelectPatchField:stage_=special_?Stage::ResolveAlternatePatch:Stage::ResolvePatch;break;
case Stage::SelectAlternatePatch:stage_=alternate_?Stage::CompareMultiField:Stage::ResolvePatch;break;
case Stage::SelectMultiField:stage_=field_<209?Stage::LocateStoredField:Stage::ReadMultiBase;break;
case Stage::ReadMultiBase:base_=read(Read::Multi,0);next();break;
case Stage::LocateStoredField:pointer_=base_+offset_;next();break;
case Stage::ReadStoredField:value_=uint8_t(read(Read::Value,pointer_));next();break;
case Stage::SelectSpecialHigh:stage_=field_>=212?Stage::MarkSpecial:Stage::CompareSpecialSentinel;break;
case Stage::SelectSpecialSentinel:stage_=field_==126?Stage::ReturnSpecial:Stage::CompareSpecialLow;break;
case Stage::SelectSpecialLow:stage_=field_<84?Stage::ReturnSpecial:Stage::CompareSpecialEnd;break;
case Stage::SelectSpecialEnd:stage_=field_>124?Stage::ReturnSpecial:Stage::MarkSpecial;break;
case Stage::JoinGlobalStorage:stage_=Stage::LocateStoredField;break;
case Stage::JoinUnusedValue:stage_=Stage::ReturnStoredValue;break;
case Stage::ClassifyPatchField:await(Stage::SelectPatchField);break;
case Stage::ResolveAlternatePatch:await(Stage::SelectAlternatePatch);break;
case Stage::ResolvePatch:await(Stage::CompareMultiField);break;
case Stage::ReturnStoredValue:stage_=Stage::Complete;break;
case Stage::ReturnSpecial:stage_=Stage::Complete;break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;uint32_t base_=0,pointer_=0,offset_=0;uint8_t field_=0,value_=0;bool special_=false,alternate_=false;
};
}
