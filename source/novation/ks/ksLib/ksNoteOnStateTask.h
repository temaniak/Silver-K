#pragma once
#include <cstdint>
namespace ks {
// Append a key event and publish both keyboard maps, observing MIDI payload
// and selected-part state at the same publication points as the controller.
class NoteOnStateTask final {
public:
 enum class Stage {RetainPart,RetainIndex,RetainScratch,DispatchActivity,AwaitActivity,ObserveActivity,AdmitActivity,
 RetainNote,RetainVelocity,DispatchMonoReset,AwaitMonoReset,ReleaseVelocity,ReleaseNote,DispatchPart,AwaitPart,
 ClearIndex,ReadCount,CompareCount,AdmitAppend,CaptureCount,LocateEntry,BindEntry,StoreEntry,StoreLastEvent,
 IncrementCount,StoreHeldCount,StoreDownCount,DispatchLivePart,AwaitLivePart,ClearVelocityIndex,CaptureVelocityKey,BindVelocity,
 StoreVelocity,ClearMapIndex,CaptureMapKey,ScaleMapQuarter,ScaleMapHalf,BindMap,LocatePhysical,PublishPhysical,
 LocateHeld,PublishHeld,ReleaseScratch,ReleaseIndex,ReleasePart,Finish,Complete};
 enum class Input {MidiWord,Note,Velocity,DownCount,PhysicalByte,HeldByte};
 Stage stage()const{return stage_;}uint8_t part()const{return part_;}unsigned count()const{return count_;}
 unsigned key()const{return key_;}unsigned mapIndex()const{return mapIndex_;}bool active()const{return activity_!=0;}
 bool complete()const{return stage_==Stage::Complete;}
 bool awaitingActivity()const{return stage_==Stage::AwaitActivity;}
 bool awaitingMonoReset()const{return stage_==Stage::AwaitMonoReset;}
 bool awaitingPart()const{return stage_==Stage::AwaitPart||stage_==Stage::AwaitLivePart;}
 bool resumeActivity(uint16_t result){if(!awaitingActivity())return false;activity_=result;stage_=Stage::ObserveActivity;return true;}
 bool resumeMonoReset(){if(!awaitingMonoReset())return false;stage_=Stage::ReleaseVelocity;return true;}
 bool resumePart(uint8_t part){if(!awaitingPart())return false;part_=part;stage_=stage_==Stage::AwaitPart?Stage::ClearIndex:Stage::ClearVelocityIndex;return true;}
 bool bytePublication()const{return stage_==Stage::StoreHeldCount||stage_==Stage::StoreDownCount||stage_==Stage::StoreVelocity||stage_==Stage::PublishPhysical||stage_==Stage::PublishHeld;}
 unsigned publicationOffset()const{switch(stage_){case Stage::StoreHeldCount:return 307;case Stage::StoreDownCount:return 306;case Stage::StoreVelocity:return 64+key_;case Stage::PublishPhysical:return 0xc0+mapIndex_;default:return 0xd0+mapIndex_;}}
 unsigned nextClocks(unsigned write=3)const{switch(stage_){
 case Stage::RetainPart:case Stage::RetainIndex:case Stage::RetainScratch:case Stage::ReleaseScratch:case Stage::ReleaseIndex:case Stage::ReleasePart:
 case Stage::DispatchActivity:case Stage::DispatchMonoReset:case Stage::DispatchPart:case Stage::DispatchLivePart:return 20;
 case Stage::RetainNote:case Stage::RetainVelocity:case Stage::ReleaseVelocity:case Stage::ReleaseNote:return 11;
 case Stage::ReadCount:case Stage::StoreEntry:return 15;case Stage::StoreLastEvent:return 12;
 case Stage::StoreHeldCount:case Stage::StoreDownCount:case Stage::StoreVelocity:return 12+write;
 case Stage::PublishPhysical:case Stage::PublishHeld:return 9+write;
 case Stage::LocatePhysical:case Stage::LocateHeld:return 9;case Stage::Finish:return 17;
 case Stage::AwaitActivity:case Stage::AwaitMonoReset:case Stage::AwaitPart:case Stage::AwaitLivePart:case Stage::Complete:return 0;
 default:return 3;}}
 template<class Reader,class Writer,class Timing>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& publish,Timing&& timing){unsigned used=0;
  while(const auto cost=nextClocks(bytePublication()?timing(publicationOffset()):3u)){if(cost>available-used)break;
   switch(stage_){case Stage::RetainPart:stage_=Stage::RetainIndex;break;case Stage::RetainIndex:stage_=Stage::RetainScratch;break;
   case Stage::RetainScratch:stage_=Stage::DispatchActivity;break;case Stage::DispatchActivity:stage_=Stage::AwaitActivity;break;
   case Stage::ObserveActivity:stage_=Stage::AdmitActivity;break;case Stage::AdmitActivity:stage_=active()?Stage::DispatchPart:Stage::RetainNote;break;
   case Stage::RetainNote:stage_=Stage::RetainVelocity;break;case Stage::RetainVelocity:stage_=Stage::DispatchMonoReset;break;
   case Stage::DispatchMonoReset:stage_=Stage::AwaitMonoReset;break;case Stage::ReleaseVelocity:stage_=Stage::ReleaseNote;break;
   case Stage::ReleaseNote:stage_=Stage::DispatchPart;break;case Stage::DispatchPart:stage_=Stage::AwaitPart;break;
   case Stage::ClearIndex:stage_=Stage::ReadCount;break;case Stage::ReadCount:count_=uint8_t(read(Input::DownCount,306));stage_=Stage::CompareCount;break;
   case Stage::CompareCount:stage_=Stage::AdmitAppend;break;case Stage::AdmitAppend:stage_=count_>=32?Stage::DispatchLivePart:Stage::CaptureCount;break;
   case Stage::CaptureCount:stage_=Stage::LocateEntry;break;case Stage::LocateEntry:stage_=Stage::BindEntry;break;case Stage::BindEntry:stage_=Stage::StoreEntry;break;
   case Stage::StoreEntry:publish(240+2*count_,uint16_t(read(Input::MidiWord,0)),2);stage_=Stage::StoreLastEvent;break;
   case Stage::StoreLastEvent:publish(0,uint16_t(read(Input::MidiWord,0)),0);stage_=Stage::IncrementCount;break;
   case Stage::IncrementCount:++count_;stage_=Stage::StoreHeldCount;break;
   case Stage::StoreHeldCount:publish(307,count_,1);stage_=Stage::StoreDownCount;break;
   case Stage::StoreDownCount:publish(306,count_,1);stage_=Stage::DispatchLivePart;break;
   case Stage::DispatchLivePart:stage_=Stage::AwaitLivePart;break;
   case Stage::ClearVelocityIndex:stage_=Stage::CaptureVelocityKey;break;case Stage::CaptureVelocityKey:key_=uint8_t(read(Input::Note,0));stage_=Stage::BindVelocity;break;
   case Stage::BindVelocity:stage_=Stage::StoreVelocity;break;case Stage::StoreVelocity:publish(64+key_,uint8_t(read(Input::Velocity,0)),1);stage_=Stage::ClearMapIndex;break;
   case Stage::ClearMapIndex:stage_=Stage::CaptureMapKey;break;case Stage::CaptureMapKey:mapIndex_=uint8_t(read(Input::Note,0));stage_=Stage::ScaleMapQuarter;break;
   case Stage::ScaleMapQuarter:mapIndex_>>=2;stage_=Stage::ScaleMapHalf;break;case Stage::ScaleMapHalf:mapIndex_>>=1;stage_=Stage::BindMap;break;
   case Stage::BindMap:stage_=Stage::LocatePhysical;break;case Stage::LocatePhysical:stage_=Stage::PublishPhysical;break;
   case Stage::PublishPhysical:{const unsigned at=0xc0+mapIndex_,value=uint8_t(read(Input::PhysicalByte,at)),bit=uint8_t(read(Input::Note,0))&7;publish(at,value|(1u<<bit),1);stage_=Stage::LocateHeld;break;}
   case Stage::LocateHeld:stage_=Stage::PublishHeld;break;
   case Stage::PublishHeld:{const unsigned at=0xd0+mapIndex_,value=uint8_t(read(Input::HeldByte,at)),bit=uint8_t(read(Input::Note,0))&7;publish(at,value|(1u<<bit),1);stage_=Stage::ReleaseScratch;break;}
   case Stage::ReleaseScratch:stage_=Stage::ReleaseIndex;break;case Stage::ReleaseIndex:stage_=Stage::ReleasePart;break;
   case Stage::ReleasePart:stage_=Stage::Finish;break;case Stage::Finish:stage_=Stage::Complete;break;default:break;}used+=cost;
  }return used;
 }
private:Stage stage_=Stage::RetainPart;uint8_t part_=0;unsigned count_=0,key_=0,mapIndex_=0;uint16_t activity_=0;
};
}
