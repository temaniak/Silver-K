#pragma once
#include <cstdint>
namespace ks {
// Choose generated/sample-wave patch for a note, temporarily publishing its
// reference while playback is requested, then restore the original reference.
class NoteProgramTask final {
public:
 enum class Kind {Select,Play};
 enum class Stage {ClearIndex,ReadPart,DoubleIndex,ReadProgram,CompareBank,AdmitBank,CompareLow,AdmitLow,CompareHigh,AdmitHigh,
 CaptureNote,SampleIndex,CompareSample,AdmitSample,ClearSampleWord,CopySample,SetSampleStride,SampleProduct,CopyBank,BankIndex,NormalizeBank,SetBankStride,BankProduct,AddProducts,AddStorage,
 DispatchControls,AwaitControls,ReadOriginal,PublishTemporary,RetainOriginal,RetainControls,DispatchPlay,AwaitPlay,RestoreControls,RestoreOriginal,PublishOriginal,FinishTemporary,
 DispatchPart,AwaitPart,PublishLast,DispatchUnison,AwaitUnison,DispatchFeedback,AwaitFeedback,Finish,Complete};
 enum class Read {Part,Program,Note,OriginalReference};
 enum class Write {TemporaryReference,OriginalReference,LastNote};
 explicit NoteProgramTask(Kind k):kind_(k),stage_(k==Kind::Select?Stage::ClearIndex:Stage::DispatchPart){}
 Kind kind()const{return kind_;}Stage stage()const{return stage_;}uint8_t selectedPart()const{return selectedPart_;}uint8_t tableOffset()const{return tableOffset_;}
 uint16_t program()const{return program_;}uint8_t bank()const{return uint8_t(program_>>8);}uint8_t sample()const{return uint8_t(program_);}
 uint8_t note()const{return note_;}uint8_t sampleIndex()const{return sampleIndex_;}uint8_t bankIndex()const{return bankIndex_;}
 uint32_t sampleOffset()const{return sampleOffset_;}uint32_t bankOffset()const{return bankOffset_;}uint32_t patchReference()const{return patchReference_;}uint32_t originalReference()const{return originalReference_;}
 uint8_t controlPart()const{return controlPart_;}uint8_t part()const{return part_;}uint8_t lastNote()const{return lastNote_;}
 bool awaitingControls()const{return stage_==Stage::AwaitControls;}bool awaitingPart()const{return stage_==Stage::AwaitPart;}
 bool resumeControls(uint8_t p){if(!awaitingControls())return false;controlPart_=p;stage_=Stage::ReadOriginal;return true;}
 bool resumePart(uint8_t p){if(!awaitingPart())return false;part_=p;stage_=Stage::PublishLast;return true;}
 bool awaitingChild()const{return stage_==Stage::AwaitPlay||stage_==Stage::AwaitUnison||stage_==Stage::AwaitFeedback;}
 bool resumeChild(){switch(stage_){case Stage::AwaitPlay:stage_=Stage::RestoreControls;break;case Stage::AwaitUnison:stage_=Stage::DispatchFeedback;break;case Stage::AwaitFeedback:stage_=Stage::Finish;break;default:return false;}return true;}
 bool publication()const{return stage_==Stage::PublishLast;}bool complete()const{return stage_==Stage::Complete;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){
 case Stage::ReadPart:return 12;case Stage::ReadProgram:return 15;case Stage::SetSampleStride:case Stage::SetBankStride:return 6;
 case Stage::SampleProduct:case Stage::BankProduct:return 23;case Stage::AddStorage:return 9;
 case Stage::DispatchControls:case Stage::RetainOriginal:case Stage::RetainControls:case Stage::RestoreControls:case Stage::RestoreOriginal:case Stage::DispatchPart:case Stage::DispatchUnison:case Stage::DispatchFeedback:return 20;
 case Stage::ReadOriginal:case Stage::PublishTemporary:case Stage::PublishOriginal:return 21;case Stage::DispatchPlay:return 15;case Stage::PublishLast:return 12+store;
 case Stage::Finish:case Stage::FinishTemporary:return 17;
 case Stage::AwaitControls:case Stage::AwaitPlay:case Stage::AwaitPart:case Stage::AwaitUnison:case Stage::AwaitFeedback:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer,class Timing>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,Timing&& timing){unsigned used=0;while(const auto cost=nextClocks(publication()?timing(314):3)){
  if(cost>available-used)break;
  switch(stage_){
  case Stage::ClearIndex:stage_=Stage::ReadPart;break;case Stage::ReadPart:selectedPart_=uint8_t(read(Read::Part));tableOffset_=selectedPart_;stage_=Stage::DoubleIndex;break;
  case Stage::DoubleIndex:tableOffset_=uint8_t(tableOffset_*2);stage_=Stage::ReadProgram;break;case Stage::ReadProgram:program_=uint16_t(read(Read::Program));stage_=Stage::CompareBank;break;
  case Stage::CompareBank:stage_=Stage::AdmitBank;break;case Stage::AdmitBank:stage_=bank()<=4?Stage::DispatchPart:Stage::CompareLow;break;
  case Stage::CompareLow:note_=uint8_t(read(Read::Note));stage_=Stage::AdmitLow;break;case Stage::AdmitLow:stage_=note_<36?Stage::Finish:Stage::CompareHigh;break;
  case Stage::CompareHigh:note_=uint8_t(read(Read::Note));stage_=Stage::AdmitHigh;break;case Stage::AdmitHigh:stage_=note_>84?Stage::Finish:Stage::CaptureNote;break;
  case Stage::CaptureNote:note_=uint8_t(read(Read::Note));sampleIndex_=note_;stage_=Stage::SampleIndex;break;case Stage::SampleIndex:sampleIndex_=uint8_t(sampleIndex_-36);stage_=Stage::CompareSample;break;
  case Stage::CompareSample:stage_=Stage::AdmitSample;break;case Stage::AdmitSample:stage_=sampleIndex_==sample()?Stage::DispatchPart:Stage::ClearSampleWord;break;
  case Stage::ClearSampleWord:stage_=Stage::CopySample;break;case Stage::CopySample:stage_=Stage::SetSampleStride;break;case Stage::SetSampleStride:stage_=Stage::SampleProduct;break;
  case Stage::SampleProduct:sampleOffset_=unsigned(sampleIndex_)*0x100;stage_=Stage::CopyBank;break;
  case Stage::CopyBank:bankIndex_=bank();stage_=Stage::BankIndex;break;case Stage::BankIndex:bankIndex_=uint8_t(bankIndex_-5);stage_=Stage::NormalizeBank;break;
  case Stage::NormalizeBank:stage_=Stage::SetBankStride;break;case Stage::SetBankStride:stage_=Stage::BankProduct;break;case Stage::BankProduct:bankOffset_=unsigned(bankIndex_)*0x3200;stage_=Stage::AddProducts;break;
  case Stage::AddProducts:patchReference_=sampleOffset_+bankOffset_;stage_=Stage::AddStorage;break;case Stage::AddStorage:patchReference_+=0x839000;stage_=Stage::DispatchControls;break;
  case Stage::DispatchControls:stage_=Stage::AwaitControls;break;case Stage::ReadOriginal:originalReference_=uint32_t(read(Read::OriginalReference));stage_=Stage::PublishTemporary;break;
  case Stage::PublishTemporary:write(Write::TemporaryReference,patchReference_);stage_=Stage::RetainOriginal;break;case Stage::RetainOriginal:stage_=Stage::RetainControls;break;
  case Stage::RetainControls:stage_=Stage::DispatchPlay;break;case Stage::DispatchPlay:stage_=Stage::AwaitPlay;break;
  case Stage::RestoreControls:stage_=Stage::RestoreOriginal;break;case Stage::RestoreOriginal:stage_=Stage::PublishOriginal;break;
  case Stage::PublishOriginal:write(Write::OriginalReference,originalReference_);stage_=Stage::FinishTemporary;break;
  case Stage::DispatchPart:stage_=Stage::AwaitPart;break;case Stage::PublishLast:lastNote_=uint8_t(read(Read::Note));write(Write::LastNote,lastNote_);stage_=Stage::DispatchUnison;break;
  case Stage::DispatchUnison:stage_=Stage::AwaitUnison;break;case Stage::DispatchFeedback:stage_=Stage::AwaitFeedback;break;case Stage::Finish:case Stage::FinishTemporary:stage_=Stage::Complete;break;default:break;
  }used+=cost;
 }return used;}
private:Kind kind_;Stage stage_;uint8_t selectedPart_=0,tableOffset_=0,note_=0,sampleIndex_=0,bankIndex_=0,controlPart_=0,part_=0,lastNote_=0;uint16_t program_=0;uint32_t sampleOffset_=0,bankOffset_=0,patchReference_=0,originalReference_=0;
};
}
