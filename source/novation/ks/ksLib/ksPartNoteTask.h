#pragma once
#include <cstdint>
namespace ks {
// Native per-part note decisions. Key ranges are inclusive unsigned bytes;
// zero-velocity Note On enters exactly the same release path as Note Off.
class PartNoteTask final {
public:
 enum class Kind {Off,On};
 enum class Stage {OnReadLocal,OnAdmitLocal,OnReadPart,OnReadActive,OnComparePart,OnAdmitPart,OnSkipPreview,OnReadPreviewGate,OnAdmitPreview,
 OnReadPreviewNote,OnRetainEvent,OnDispatchPreview,OnAwaitPreview,OnReleaseEvent,
 ReadNote,DispatchControls,AwaitControls,ReadLow,ReadHigh,CompareLow,AdmitLow,CompareHigh,AdmitHigh,
 OnReadVelocity,OnAdmitVelocity,OnDispatchState,OnAwaitState,OnDispatchPart,OnAwaitPart,
 OffDispatchState,OffAwaitState,OffDispatchPart,OffAwaitPart,
 ReadPolicy,ObserveArp,AdmitArp,MaskArp,CompareArp,AdmitArpMode,
 OffRetainNote,OffDispatchPriorityPart,OffAwaitPriorityPart,OffReadCount,OffCompareCount,OffAdmitCount,OffReadLast,OffCompareLast,OffAdmitLast,
 OffDispatchReplacement,OffAwaitReplacement,OffObserveReplacement,OffAdmitReplacement,OffDispatchStart,OffAwaitStart,OffReleaseReturn,
 OffReleaseOriginal,OffDispatchRelease,OffAwaitRelease,OffFinish,OffFinishReplacement,
 OnDispatchStart,OnAwaitStart,OnFinish,Complete};
 enum class Read {Local,Part,ActivePart,PreviewGate,EventNote,EventVelocity,MessageWord,Low,High,Note,Policy,Count,Last,Replacement};
 explicit PartNoteTask(Kind k):kind_(k),stage_(k==Kind::On?Stage::OnReadLocal:Stage::ReadNote){}
 bool releasing()const{return releasePath_||kind_==Kind::Off;}Kind kind()const{return kind_;}Stage stage()const{return stage_;}uint8_t part()const{return part_;}uint8_t controlPart()const{return controlPart_;}
 uint8_t note()const{return note_;}uint8_t velocity()const{return velocity_;}uint8_t low()const{return low_;}uint8_t high()const{return high_;}
 uint8_t policy()const{return policy_;}uint8_t maskedPolicy()const{return policy_&0x60;}uint8_t count()const{return count_;}uint8_t last()const{return last_;}
 uint8_t local()const{return local_;}uint8_t selectedPart()const{return selectedPart_;}uint8_t activePart()const{return activePart_;}uint8_t previewGate()const{return previewGate_;}uint8_t previewNote()const{return previewNote_;}
 uint16_t retainedNote()const{return retainedNote_;}uint8_t replacement()const{return replacement_;}
 bool awaitingControls()const{return stage_==Stage::AwaitControls;}
 bool resumeControls(uint8_t p){if(!awaitingControls())return false;controlPart_=p;stage_=Stage::ReadLow;return true;}
 bool awaitingPart()const{return stage_==Stage::OnAwaitPart||stage_==Stage::OffAwaitPart||stage_==Stage::OffAwaitPriorityPart;}
 bool resumePart(uint8_t p){if(!awaitingPart())return false;part_=p;stage_=stage_==Stage::OffAwaitPriorityPart?Stage::OffReadCount:Stage::ReadPolicy;return true;}
 bool awaitingChild()const{return stage_==Stage::OnAwaitPreview||stage_==Stage::OnAwaitState||stage_==Stage::OffAwaitState||stage_==Stage::OffAwaitReplacement||stage_==Stage::OffAwaitStart||stage_==Stage::OffAwaitRelease||stage_==Stage::OnAwaitStart;}
 bool resumeChild(){switch(stage_){case Stage::OnAwaitPreview:stage_=Stage::OnReleaseEvent;break;case Stage::OnAwaitState:stage_=Stage::OnDispatchPart;break;
 case Stage::OffAwaitState:stage_=Stage::OffDispatchPart;break;case Stage::OffAwaitReplacement:stage_=Stage::OffObserveReplacement;break;
 case Stage::OffAwaitStart:stage_=Stage::OffReleaseReturn;break;case Stage::OffAwaitRelease:stage_=Stage::OffFinish;break;case Stage::OnAwaitStart:stage_=Stage::OnFinish;break;default:return false;}return true;}
 bool complete()const{return stage_==Stage::Complete;}
 unsigned nextClocks()const{switch(stage_){
 case Stage::OnReadLocal:case Stage::OnReadPart:case Stage::OnReadActive:case Stage::OnReadPreviewGate:return 12;
 case Stage::ReadNote:case Stage::OnReadPreviewNote:return 6;case Stage::OnReadVelocity:return 9;case Stage::OnAdmitVelocity:return 8;
 case Stage::ReadLow:case Stage::ReadHigh:case Stage::ReadPolicy:case Stage::OffReadCount:case Stage::OffReadLast:return 15;
 case Stage::OnRetainEvent:case Stage::OnReleaseEvent:case Stage::OnDispatchPreview:case Stage::DispatchControls:case Stage::OnDispatchState:case Stage::OnDispatchPart:case Stage::OffDispatchState:case Stage::OffDispatchPart:case Stage::OffDispatchPriorityPart:case Stage::OffDispatchReplacement:case Stage::OffDispatchStart:case Stage::OffDispatchRelease:case Stage::OnDispatchStart:return 20;
 case Stage::OffRetainNote:case Stage::OffReleaseReturn:case Stage::OffReleaseOriginal:return 11;
 case Stage::OffFinish:case Stage::OffFinishReplacement:case Stage::OnFinish:return 17;
 case Stage::OnAwaitPreview:case Stage::AwaitControls:case Stage::OnAwaitState:case Stage::OnAwaitPart:case Stage::OffAwaitState:case Stage::OffAwaitPart:case Stage::OffAwaitPriorityPart:case Stage::OffAwaitReplacement:case Stage::OffAwaitStart:case Stage::OffAwaitRelease:case Stage::OnAwaitStart:case Stage::Complete:return 0;
 default:return 3;}}
 template<class Reader>unsigned advanceBefore(unsigned available,Reader&& read){unsigned used=0;while(const auto cost=nextClocks()){
  if(cost>available-used)break;
  switch(stage_){
  case Stage::OnReadLocal:local_=uint8_t(read(Read::Local));stage_=Stage::OnAdmitLocal;break;
  case Stage::OnAdmitLocal:stage_=local_?Stage::OnReadPart:Stage::OnReadPreviewGate;break;
  case Stage::OnReadPart:selectedPart_=uint8_t(read(Read::Part));stage_=Stage::OnReadActive;break;
  case Stage::OnReadActive:activePart_=uint8_t(read(Read::ActivePart));stage_=Stage::OnComparePart;break;
  case Stage::OnComparePart:stage_=Stage::OnAdmitPart;break;case Stage::OnAdmitPart:stage_=activePart_==selectedPart_?Stage::OnReadPreviewNote:Stage::OnSkipPreview;break;
  case Stage::OnSkipPreview:stage_=Stage::ReadNote;break;case Stage::OnReadPreviewGate:previewGate_=uint8_t(read(Read::PreviewGate));stage_=Stage::OnAdmitPreview;break;
  case Stage::OnAdmitPreview:stage_=previewGate_?Stage::OnReadPreviewNote:Stage::ReadNote;break;
  case Stage::OnReadPreviewNote:previewNote_=uint8_t(read(Read::EventNote));stage_=Stage::OnRetainEvent;break;
  case Stage::OnRetainEvent:stage_=Stage::OnDispatchPreview;break;case Stage::OnDispatchPreview:stage_=Stage::OnAwaitPreview;break;case Stage::OnReleaseEvent:stage_=Stage::ReadNote;break;
  case Stage::ReadNote:note_=uint8_t(read(Read::EventNote));stage_=Stage::DispatchControls;break;case Stage::DispatchControls:stage_=Stage::AwaitControls;break;
  case Stage::ReadLow:low_=uint8_t(read(Read::Low));stage_=Stage::ReadHigh;break;case Stage::ReadHigh:high_=uint8_t(read(Read::High));stage_=Stage::CompareLow;break;
  case Stage::CompareLow:note_=uint8_t(read(Read::Note));stage_=Stage::AdmitLow;break;
  case Stage::AdmitLow:stage_=note_<low_?finish():Stage::CompareHigh;break;
  case Stage::CompareHigh:note_=uint8_t(read(Read::Note));stage_=Stage::AdmitHigh;break;
  case Stage::AdmitHigh:stage_=note_>high_?finish():kind_==Kind::On?Stage::OnReadVelocity:Stage::OffDispatchState;break;
  case Stage::OnReadVelocity:velocity_=uint8_t(read(Read::EventVelocity));stage_=Stage::OnAdmitVelocity;break;
  case Stage::OnAdmitVelocity:if(!velocity_){releasePath_=true;stage_=Stage::OffDispatchState;}else stage_=Stage::OnDispatchState;break;
  case Stage::OnDispatchState:stage_=Stage::OnAwaitState;break;case Stage::OnDispatchPart:stage_=Stage::OnAwaitPart;break;
  case Stage::OffDispatchState:releasePath_=true;stage_=Stage::OffAwaitState;break;case Stage::OffDispatchPart:stage_=Stage::OffAwaitPart;break;
  case Stage::ReadPolicy:policy_=uint8_t(read(Read::Policy));stage_=Stage::ObserveArp;break;case Stage::ObserveArp:stage_=Stage::AdmitArp;break;
  case Stage::AdmitArp:stage_=!(policy_&4)?allowed():Stage::MaskArp;break;case Stage::MaskArp:stage_=Stage::CompareArp;break;case Stage::CompareArp:stage_=Stage::AdmitArpMode;break;
  case Stage::AdmitArpMode:stage_=(policy_&0x60)==0x60?allowed():finish();break;
  case Stage::OffRetainNote:retainedNote_=uint16_t(read(Read::MessageWord));stage_=Stage::OffDispatchPriorityPart;break;
  case Stage::OffDispatchPriorityPart:stage_=Stage::OffAwaitPriorityPart;break;
  case Stage::OffReadCount:count_=uint8_t(read(Read::Count));stage_=Stage::OffCompareCount;break;case Stage::OffCompareCount:stage_=Stage::OffAdmitCount;break;
  case Stage::OffAdmitCount:stage_=count_>1?Stage::OffReleaseOriginal:Stage::OffReadLast;break;
  case Stage::OffReadLast:last_=uint8_t(read(Read::Last));stage_=Stage::OffCompareLast;break;
  case Stage::OffCompareLast:note_=uint8_t(read(Read::Note));stage_=Stage::OffAdmitLast;break;
  case Stage::OffAdmitLast:stage_=note_==last_?Stage::OffDispatchReplacement:Stage::OffReleaseOriginal;break;
  case Stage::OffDispatchReplacement:stage_=Stage::OffAwaitReplacement;break;
  case Stage::OffObserveReplacement:replacement_=uint8_t(read(Read::Replacement));stage_=Stage::OffAdmitReplacement;break;
  case Stage::OffAdmitReplacement:stage_=replacement_>=128?Stage::OffReleaseOriginal:Stage::OffDispatchStart;break;
  case Stage::OffDispatchStart:stage_=Stage::OffAwaitStart;break;case Stage::OffReleaseReturn:stage_=Stage::OffFinishReplacement;break;
  case Stage::OffReleaseOriginal:stage_=Stage::OffDispatchRelease;break;case Stage::OffDispatchRelease:stage_=Stage::OffAwaitRelease;break;
  case Stage::OnDispatchStart:stage_=Stage::OnAwaitStart;break;case Stage::OffFinish:case Stage::OffFinishReplacement:case Stage::OnFinish:stage_=Stage::Complete;break;default:break;
  }used+=cost;
 }return used;}
private:
 Stage finish()const{return releasePath_||kind_==Kind::Off?Stage::OffFinish:Stage::OnFinish;}
 Stage allowed()const{return releasePath_||kind_==Kind::Off?Stage::OffRetainNote:Stage::OnDispatchStart;}
 Kind kind_;Stage stage_;bool releasePath_=false;
 uint8_t part_=0,controlPart_=0,note_=0,velocity_=0,low_=0,high_=0,policy_=0,count_=0,last_=0,replacement_=0xff;
 uint8_t local_=0,selectedPart_=0,activePart_=0,previewGate_=0,previewNote_=0;uint16_t retainedNote_=0;
};
}
