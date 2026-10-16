#pragma once
#include <cstdint>
namespace ks {
// Release a physical key, removing its held-key entry immediately only when
// sustain permits. An empty mono held map releases part voices before the
// independently rebound physical-down map is published.
class NoteOffStateTask final {
public:
 enum class Stage {RetainPart,RetainMap,RetainPedal,ClearIndex,CaptureNote,IndexQuarter,IndexEighth,RetainIndex,
 DispatchPart,AwaitPart,ReadPedal,ObservePedal,AdmitPedal,BindHeld,LocateHeld,ClearHeld,DispatchCompact,AwaitCompact,ReadMode,ObserveMode,AdmitMode,
 DispatchHeld,AwaitHeld,AdmitHeld,RetainUpper,RetainVoice,RetainData,RetainNote,RetainPatch,DispatchRelease,AwaitRelease,DispatchMonoReset,AwaitMonoReset,
 ReleasePatch,ReleaseNote,ReleaseData,ReleaseVoice,ReleaseUpper,ReleaseIndex,DispatchPhysicalPart,AwaitPhysicalPart,BindPhysical,LocatePhysical,ClearPhysical,
 ReleasePedal,ReleaseMap,ReleasePart,Finish,Complete};
 enum class Read {Note,Pedal,Mode,HeldByte,PhysicalByte};
 enum class Write {HeldByte,PhysicalByte};
 Stage stage()const{return stage_;}uint8_t part()const{return part_;}uint8_t noteIndex()const{return noteIndex_;}
 uint8_t bitmapIndex()const{return bitmapIndex_;}uint8_t pedal()const{return pedal_;}uint8_t mode()const{return mode_;}uint8_t published()const{return published_;}
 bool awaitingPart()const{return stage_==Stage::AwaitPart;}bool awaitingCompact()const{return stage_==Stage::AwaitCompact;}
 bool awaitingHeld()const{return stage_==Stage::AwaitHeld;}bool awaitingRelease()const{return stage_==Stage::AwaitRelease;}
 bool awaitingMonoReset()const{return stage_==Stage::AwaitMonoReset;}bool awaitingPhysicalPart()const{return stage_==Stage::AwaitPhysicalPart;}
 bool resumePart(uint8_t p){if(!awaitingPart())return false;part_=p;stage_=Stage::ReadPedal;return true;}
 bool resumeCompact(uint8_t p){if(!awaitingCompact())return false;part_=p;stage_=Stage::ReadMode;return true;}
 bool resumeHeld(bool any,uint8_t p){if(!awaitingHeld())return false;held_=any;part_=p;stage_=Stage::AdmitHeld;return true;}
 bool resumeRelease(){if(!awaitingRelease())return false;stage_=Stage::DispatchMonoReset;return true;}
 bool resumeMonoReset(){if(!awaitingMonoReset())return false;stage_=Stage::ReleasePatch;return true;}
 bool resumePhysicalPart(uint8_t p){if(!awaitingPhysicalPart())return false;part_=p;stage_=Stage::BindPhysical;return true;}
 bool complete()const{return stage_==Stage::Complete;}
 bool publication()const{return stage_==Stage::ClearHeld||stage_==Stage::ClearPhysical;}
 unsigned publicationOffset()const{return (stage_==Stage::ClearHeld?0xd0u:0xc0u)+bitmapIndex_;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){
 case Stage::RetainPart:case Stage::RetainMap:case Stage::RetainIndex:case Stage::RetainVoice:case Stage::RetainData:case Stage::RetainNote:case Stage::RetainPatch:
 case Stage::ReleasePatch:case Stage::ReleaseNote:case Stage::ReleaseData:case Stage::ReleaseVoice:case Stage::ReleaseIndex:case Stage::ReleaseMap:case Stage::ReleasePart:
 case Stage::DispatchPart:case Stage::DispatchHeld:case Stage::DispatchRelease:case Stage::DispatchMonoReset:case Stage::DispatchPhysicalPart:return 20;
 case Stage::RetainPedal:case Stage::RetainUpper:case Stage::ReleaseUpper:case Stage::ReleasePedal:return 11;
 case Stage::ReadPedal:case Stage::ReadMode:return 15;case Stage::DispatchCompact:return 15;
 case Stage::LocateHeld:case Stage::LocatePhysical:return 9;case Stage::ClearHeld:case Stage::ClearPhysical:return 9+store;
 case Stage::Finish:return 17;
 case Stage::AwaitPart:case Stage::AwaitCompact:case Stage::AwaitHeld:case Stage::AwaitRelease:case Stage::AwaitMonoReset:case Stage::AwaitPhysicalPart:case Stage::Complete:return 0;
 default:return 3;
 }}
 template<class Reader,class Writer,class Timing>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,Timing&& timing){
  unsigned used=0;while(const auto cost=nextClocks(publication()?timing(publicationOffset()):3)){
   if(cost>available-used)break;
   switch(stage_){
   case Stage::RetainPart:stage_=Stage::RetainMap;break;case Stage::RetainMap:stage_=Stage::RetainPedal;break;case Stage::RetainPedal:stage_=Stage::ClearIndex;break;
   case Stage::ClearIndex:stage_=Stage::CaptureNote;break;case Stage::CaptureNote:noteIndex_=uint8_t(read(Read::Note,0));bitmapIndex_=noteIndex_;stage_=Stage::IndexQuarter;break;
   case Stage::IndexQuarter:bitmapIndex_>>=2;stage_=Stage::IndexEighth;break;case Stage::IndexEighth:bitmapIndex_>>=1;stage_=Stage::RetainIndex;break;
   case Stage::RetainIndex:stage_=Stage::DispatchPart;break;case Stage::DispatchPart:stage_=Stage::AwaitPart;break;
   case Stage::ReadPedal:pedal_=uint8_t(read(Read::Pedal,327));stage_=Stage::ObservePedal;break;case Stage::ObservePedal:stage_=Stage::AdmitPedal;break;
   case Stage::AdmitPedal:stage_=(pedal_&16)?Stage::ReleaseIndex:Stage::BindHeld;break;case Stage::BindHeld:stage_=Stage::LocateHeld;break;case Stage::LocateHeld:stage_=Stage::ClearHeld;break;
   case Stage::ClearHeld:published_=uint8_t(read(Read::HeldByte,publicationOffset()))&uint8_t(~(1u<<(uint8_t(read(Read::Note,0))&7)));write(Write::HeldByte,publicationOffset(),published_);stage_=Stage::DispatchCompact;break;
   case Stage::DispatchCompact:stage_=Stage::AwaitCompact;break;case Stage::ReadMode:mode_=uint8_t(read(Read::Mode,324));stage_=Stage::ObserveMode;break;
   case Stage::ObserveMode:stage_=Stage::AdmitMode;break;case Stage::AdmitMode:stage_=(mode_&64)?Stage::DispatchHeld:Stage::ReleaseIndex;break;
   case Stage::DispatchHeld:stage_=Stage::AwaitHeld;break;case Stage::AdmitHeld:stage_=held_?Stage::ReleaseIndex:Stage::RetainUpper;break;
   case Stage::RetainUpper:stage_=Stage::RetainVoice;break;case Stage::RetainVoice:stage_=Stage::RetainData;break;case Stage::RetainData:stage_=Stage::RetainNote;break;case Stage::RetainNote:stage_=Stage::RetainPatch;break;case Stage::RetainPatch:stage_=Stage::DispatchRelease;break;
   case Stage::DispatchRelease:stage_=Stage::AwaitRelease;break;case Stage::DispatchMonoReset:stage_=Stage::AwaitMonoReset;break;
   case Stage::ReleasePatch:stage_=Stage::ReleaseNote;break;case Stage::ReleaseNote:stage_=Stage::ReleaseData;break;case Stage::ReleaseData:stage_=Stage::ReleaseVoice;break;case Stage::ReleaseVoice:stage_=Stage::ReleaseUpper;break;
   case Stage::ReleaseUpper:stage_=Stage::ReleaseIndex;break;case Stage::ReleaseIndex:stage_=Stage::DispatchPhysicalPart;break;case Stage::DispatchPhysicalPart:stage_=Stage::AwaitPhysicalPart;break;
   case Stage::BindPhysical:stage_=Stage::LocatePhysical;break;case Stage::LocatePhysical:stage_=Stage::ClearPhysical;break;
   case Stage::ClearPhysical:published_=uint8_t(read(Read::PhysicalByte,publicationOffset()))&uint8_t(~(1u<<(uint8_t(read(Read::Note,0))&7)));write(Write::PhysicalByte,publicationOffset(),published_);stage_=Stage::ReleasePedal;break;
   case Stage::ReleasePedal:stage_=Stage::ReleaseMap;break;case Stage::ReleaseMap:stage_=Stage::ReleasePart;break;case Stage::ReleasePart:stage_=Stage::Finish;break;case Stage::Finish:stage_=Stage::Complete;break;default:break;
   }used+=cost;
  }return used;
 }
private:Stage stage_=Stage::RetainPart;uint8_t part_=0,noteIndex_=0,bitmapIndex_=0,pedal_=0,mode_=0,published_=0;bool held_=false;
};
}
