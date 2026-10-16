#pragma once
#include <cstdint>
namespace ks {
class MidiAuxTask final {
public:
 enum class Kind {Parameter,GlobalControl,Start,Continue,Program,ConditionalProgram,Stop,Noop};
 enum class Stage {CompareLocal,AdmitLocal,CompareLocalValue,AdmitLocalValue,CopyLocal,ZeroLocal,CompareLocalThreshold,AdmitLocalThreshold,HighLocal,SkipLocal,
 CompareSelector,AdmitSelector,CaptureSelector,DispatchSelectorPart,AwaitSelectorPart,PublishSelector,FinishSelector,
 CompareEntry,AdmitEntry,CaptureEntry,DispatchEntryPart,AwaitEntryPart,ReadSelector,AdmitSelectorValue,DispatchEntry,AwaitEntry,FinishEntry,
 CaptureGenericController,CaptureGenericValue,DispatchGeneric,AwaitGeneric,FinishGeneric,
 GlobalReadController,GlobalReadValue,GlobalCompareBank,GlobalAdmitBank,GlobalCaptureBank,GlobalMaskBank,GlobalAdmitMask,GlobalDispatchPart,GlobalAwaitPart,GlobalPublishBank,GlobalFinish,
 DispatchStart,AwaitStart,FinishStart,DispatchContinue,AwaitContinue,FinishContinue,
 DispatchProgramPart,AwaitProgramPart,ReadBank,AdmitBank,SetInvalidBank,PublishInvalidBank,ReadProgram,TailProgram,FinishProgram,TailStop,HandoffProgram,HandoffStop,FinishNoop,Complete};
 enum class Read {Controller,Value,EventFirst,EventSecond,Selector,Bank};
 enum class Write {Selector,Bank,InvalidateBank};
 explicit MidiAuxTask(Kind kind):kind_(kind),stage_(kind==Kind::Parameter?Stage::CompareLocal:kind==Kind::GlobalControl?Stage::GlobalReadController:kind==Kind::Start?Stage::DispatchStart:kind==Kind::Continue?Stage::DispatchContinue:kind==Kind::Stop?Stage::TailStop:kind==Kind::Noop?Stage::FinishNoop:Stage::DispatchProgramPart){}
 Kind kind()const{return kind_;}Stage stage()const{return stage_;}uint8_t part()const{return part_;}uint8_t controller()const{return controller_;}uint8_t value()const{return value_;}uint8_t localValue()const{return localValue_;}
 uint8_t selector()const{return selector_;}uint8_t bank()const{return bank_;}uint8_t program()const{return program_;}uint8_t bankMask()const{return value_&0x60;}
 bool awaitingPart()const{return stage_==Stage::AwaitSelectorPart||stage_==Stage::AwaitEntryPart||stage_==Stage::GlobalAwaitPart||stage_==Stage::AwaitProgramPart;}
 bool resumePart(uint8_t p){if(!awaitingPart())return false;part_=p;stage_=stage_==Stage::AwaitSelectorPart?Stage::PublishSelector:stage_==Stage::AwaitEntryPart?Stage::ReadSelector:stage_==Stage::GlobalAwaitPart?Stage::GlobalPublishBank:Stage::ReadBank;return true;}
 bool awaitingChild()const{return stage_==Stage::AwaitEntry||stage_==Stage::AwaitGeneric||stage_==Stage::AwaitStart||stage_==Stage::AwaitContinue;}
 bool resumeChild(){switch(stage_){case Stage::AwaitEntry:stage_=Stage::FinishEntry;break;case Stage::AwaitGeneric:stage_=Stage::FinishGeneric;break;case Stage::AwaitStart:stage_=Stage::FinishStart;break;case Stage::AwaitContinue:stage_=Stage::FinishContinue;break;default:return false;}return true;}
 bool publication()const{return stage_==Stage::PublishSelector||stage_==Stage::GlobalPublishBank||stage_==Stage::PublishInvalidBank;}
 unsigned publicationOffset()const{return stage_==Stage::PublishSelector?332:331;}
 bool handoff()const{return stage_==Stage::HandoffProgram||stage_==Stage::HandoffStop;}bool complete()const{return stage_==Stage::Complete;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){
 case Stage::DispatchSelectorPart:case Stage::DispatchEntryPart:case Stage::DispatchEntry:case Stage::DispatchGeneric:case Stage::GlobalDispatchPart:case Stage::DispatchStart:case Stage::DispatchContinue:case Stage::DispatchProgramPart:return 20;
 case Stage::ReadSelector:case Stage::ReadBank:return 15;case Stage::PublishSelector:case Stage::GlobalPublishBank:case Stage::PublishInvalidBank:return 12+store;
 case Stage::GlobalReadController:case Stage::ReadProgram:return 6;case Stage::GlobalReadValue:return 9;case Stage::TailProgram:case Stage::TailStop:return 8;
 case Stage::FinishSelector:case Stage::FinishEntry:case Stage::FinishGeneric:case Stage::GlobalFinish:case Stage::FinishStart:case Stage::FinishContinue:case Stage::FinishProgram:case Stage::FinishNoop:return 17;
 case Stage::AwaitSelectorPart:case Stage::AwaitEntryPart:case Stage::AwaitEntry:case Stage::AwaitGeneric:case Stage::GlobalAwaitPart:case Stage::AwaitStart:case Stage::AwaitContinue:case Stage::AwaitProgramPart:case Stage::HandoffProgram:case Stage::HandoffStop:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer,class Timing>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,Timing&& timing){unsigned used=0;while(const auto cost=nextClocks(publication()?timing(publicationOffset()):3)){
  if(cost>available-used)break;
  switch(stage_){
  case Stage::CompareLocal:controller_=uint8_t(read(Read::Controller));stage_=Stage::AdmitLocal;break;case Stage::AdmitLocal:stage_=controller_==122?Stage::CompareLocalValue:Stage::CompareSelector;break;
  case Stage::CompareLocalValue:value_=uint8_t(read(Read::Value));stage_=Stage::AdmitLocalValue;break;case Stage::AdmitLocalValue:stage_=value_==63?Stage::CaptureGenericController:Stage::CopyLocal;break;
  case Stage::CopyLocal:localValue_=uint8_t(read(Read::Value));stage_=Stage::ZeroLocal;break;case Stage::ZeroLocal:value_=0;normalized_=true;stage_=Stage::CompareLocalThreshold;break;
  case Stage::CompareLocalThreshold:stage_=Stage::AdmitLocalThreshold;break;case Stage::AdmitLocalThreshold:stage_=localValue_<63?Stage::CaptureGenericController:Stage::HighLocal;break;
  case Stage::HighLocal:value_=127;stage_=Stage::SkipLocal;break;case Stage::SkipLocal:stage_=Stage::CaptureGenericController;break;
  case Stage::CompareSelector:stage_=Stage::AdmitSelector;break;case Stage::AdmitSelector:stage_=controller_==98?Stage::CaptureSelector:Stage::CompareEntry;break;
  case Stage::CaptureSelector:selector_=uint8_t(read(Read::Value));stage_=Stage::DispatchSelectorPart;break;case Stage::DispatchSelectorPart:stage_=Stage::AwaitSelectorPart;break;
  case Stage::PublishSelector:write(Write::Selector,selector_);stage_=Stage::FinishSelector;break;
  case Stage::CompareEntry:stage_=Stage::AdmitEntry;break;case Stage::AdmitEntry:stage_=controller_==6?Stage::CaptureEntry:Stage::CaptureGenericController;break;
  case Stage::CaptureEntry:value_=uint8_t(read(Read::Value));stage_=Stage::DispatchEntryPart;break;case Stage::DispatchEntryPart:stage_=Stage::AwaitEntryPart;break;
  case Stage::ReadSelector:selector_=uint8_t(read(Read::Selector));stage_=Stage::AdmitSelectorValue;break;case Stage::AdmitSelectorValue:stage_=selector_>=128?Stage::FinishEntry:Stage::DispatchEntry;break;
  case Stage::DispatchEntry:stage_=Stage::AwaitEntry;break;case Stage::CaptureGenericController:controller_=uint8_t(read(Read::Controller));stage_=Stage::CaptureGenericValue;break;
  case Stage::CaptureGenericValue:if(!normalized_)value_=uint8_t(read(Read::Value));stage_=Stage::DispatchGeneric;break;case Stage::DispatchGeneric:stage_=Stage::AwaitGeneric;break;
  case Stage::GlobalReadController:controller_=uint8_t(read(Read::EventFirst));stage_=Stage::GlobalReadValue;break;case Stage::GlobalReadValue:value_=uint8_t(read(Read::EventSecond));stage_=Stage::GlobalCompareBank;break;
  case Stage::GlobalCompareBank:stage_=Stage::GlobalAdmitBank;break;case Stage::GlobalAdmitBank:stage_=controller_==32?Stage::GlobalCaptureBank:Stage::CompareLocal;break;
  case Stage::GlobalCaptureBank:bank_=value_;stage_=Stage::GlobalMaskBank;break;case Stage::GlobalMaskBank:stage_=Stage::GlobalAdmitMask;break;
  case Stage::GlobalAdmitMask:stage_=(value_&0x60)?Stage::GlobalDispatchPart:Stage::GlobalFinish;break;case Stage::GlobalDispatchPart:stage_=Stage::GlobalAwaitPart;break;
  case Stage::GlobalPublishBank:write(Write::Bank,bank_);stage_=Stage::GlobalFinish;break;
  case Stage::DispatchStart:stage_=Stage::AwaitStart;break;case Stage::DispatchContinue:stage_=Stage::AwaitContinue;break;
  case Stage::DispatchProgramPart:stage_=Stage::AwaitProgramPart;break;case Stage::ReadBank:bank_=uint8_t(read(Read::Bank));stage_=kind_==Kind::ConditionalProgram?Stage::AdmitBank:Stage::SetInvalidBank;break;
  case Stage::AdmitBank:stage_=bank_>=128?Stage::FinishProgram:Stage::SetInvalidBank;break;case Stage::SetInvalidBank:stage_=Stage::PublishInvalidBank;break;
  case Stage::PublishInvalidBank:write(Write::InvalidateBank,255);stage_=Stage::ReadProgram;break;case Stage::ReadProgram:program_=uint8_t(read(Read::EventFirst));stage_=Stage::TailProgram;break;
  case Stage::TailProgram:stage_=Stage::HandoffProgram;break;case Stage::TailStop:stage_=Stage::HandoffStop;break;
  case Stage::FinishSelector:case Stage::FinishEntry:case Stage::FinishGeneric:case Stage::GlobalFinish:case Stage::FinishStart:case Stage::FinishContinue:case Stage::FinishProgram:case Stage::FinishNoop:stage_=Stage::Complete;break;default:break;
  }used+=cost;
 }return used;}
private:Kind kind_;Stage stage_;bool normalized_=false;uint8_t part_=0,controller_=0,value_=0,localValue_=0,selector_=0,bank_=0,program_=0;
};
}
