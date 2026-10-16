#pragma once
#include <cstdint>
namespace ks {
// Native start decisions and resume/full gate packet preparation. Complete
// parameter upload remains a separate child, not an implicit CPU fallback.
class VoiceStartTask final {
public:
 enum class Kind {Voice,ResumeGate,FullGate};
 enum class Stage {DispatchAllocation,AwaitAllocation,ReadVelocity,AdmitVelocity,DispatchTracking,AwaitTracking,ReadAllocation,CompareAllocation,AdmitAllocation,
  PrimeOffset,DispatchPrime,AwaitPrime,DispatchParameters,AwaitParameters,ReadFlags,ShapeFlags,ReadMiddle,ShapeMiddle,RotateMiddle,CombineFlags,ClearMiddle,CaptureHigh,
  ReadLow,MaskLow,FixedLow,ReadDetune,BiasDetune,AdmitDetune,EnableShift,GateOffset,DispatchGate,AwaitGate,Finish,Complete};
 enum class Read {Velocity,Allocation,Flags,Middle,Low,Detune};
 enum class Child {None,Allocation,Tracking,Prime,Parameters,Gate};
 explicit VoiceStartTask(Kind kind):kind_(kind),stage_(kind==Kind::Voice?Stage::DispatchAllocation:Stage::ReadFlags){}
 Kind kind()const{return kind_;}bool fullGate()const{return kind_==Kind::FullGate;}Stage stage()const{return stage_;}
 uint8_t slot()const{return slot_;}uint8_t velocity()const{return velocity_;}uint8_t allocation()const{return allocation_;}
 uint8_t flags()const{return flags_;}uint8_t middle()const{return middle_;}uint8_t low()const{return low_;}uint8_t detune()const{return detune_;}
 bool suppressed()const{return suppressed_;}bool handoff()const{return handoff_;}bool complete()const{return stage_==Stage::Complete;}
 static constexpr uint16_t primeOffset(){return 0x2c;}static constexpr uint16_t gateOffset(){return 0x2b;}
 uint32_t dspWord()const{return (uint32_t(flags_)<<16)|(uint32_t(middle_)<<8)|low_;}
 Child child()const{switch(stage_){case Stage::AwaitAllocation:return Child::Allocation;case Stage::AwaitTracking:return Child::Tracking;
 case Stage::AwaitPrime:return Child::Prime;case Stage::AwaitParameters:return Child::Parameters;case Stage::AwaitGate:return Child::Gate;default:return Child::None;}}
 bool resumeChild(uint8_t slot=0){switch(stage_){case Stage::AwaitAllocation:slot_=slot;stage_=Stage::ReadVelocity;return true;
 case Stage::AwaitTracking:stage_=Stage::ReadAllocation;return true;case Stage::AwaitPrime:stage_=Stage::DispatchParameters;return true;
 case Stage::AwaitParameters:stage_=Stage::ReadFlags;return true;case Stage::AwaitGate:stage_=Stage::Finish;return true;default:return false;}}
 unsigned nextClocks()const{switch(stage_){case Stage::DispatchAllocation:case Stage::DispatchTracking:case Stage::DispatchPrime:case Stage::DispatchParameters:case Stage::DispatchGate:return 20;
 case Stage::ReadVelocity:case Stage::ReadFlags:case Stage::ReadMiddle:case Stage::ReadDetune:return 15;
 case Stage::ReadAllocation:return 12;case Stage::AdmitVelocity:return 8;case Stage::PrimeOffset:case Stage::GateOffset:return 6;
 case Stage::ReadLow:return fullGate()?15:3;case Stage::Finish:return 17;
 case Stage::AwaitAllocation:case Stage::AwaitTracking:case Stage::AwaitPrime:case Stage::AwaitParameters:case Stage::AwaitGate:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader>unsigned advanceBefore(unsigned available,Reader&& read){unsigned used=0;while(const auto cost=nextClocks()){
  if(cost>available-used)break;
  switch(stage_){case Stage::DispatchAllocation:stage_=Stage::AwaitAllocation;break;case Stage::ReadVelocity:velocity_=uint8_t(read(Read::Velocity));stage_=Stage::AdmitVelocity;break;
  case Stage::AdmitVelocity:if(velocity_&0x80){suppressed_=true;stage_=Stage::Finish;}else stage_=Stage::DispatchTracking;break;
  case Stage::DispatchTracking:stage_=Stage::AwaitTracking;break;case Stage::ReadAllocation:allocation_=uint8_t(read(Read::Allocation));stage_=Stage::CompareAllocation;break;
  case Stage::CompareAllocation:stage_=Stage::AdmitAllocation;break;case Stage::AdmitAllocation:if(allocation_>1){handoff_=true;stage_=Stage::Complete;}else stage_=Stage::PrimeOffset;break;
  case Stage::PrimeOffset:stage_=Stage::DispatchPrime;break;case Stage::DispatchPrime:stage_=Stage::AwaitPrime;break;case Stage::DispatchParameters:stage_=Stage::AwaitParameters;break;
  case Stage::ReadFlags:flags_=uint8_t(read(Read::Flags));stage_=Stage::ShapeFlags;break;case Stage::ShapeFlags:flags_=fullGate()?uint8_t(flags_&0x3f):uint8_t(flags_|0xc0);stage_=Stage::ReadMiddle;break;
  case Stage::ReadMiddle:middle_=uint8_t(read(Read::Middle));stage_=Stage::ShapeMiddle;break;
  case Stage::ShapeMiddle:middle_=fullGate()?uint8_t(middle_&3):uint8_t(middle_<<1);stage_=fullGate()?Stage::RotateMiddle:Stage::CaptureHigh;break;
  case Stage::RotateMiddle:middle_=uint8_t((middle_>>2)|(unsigned(middle_)<<6));stage_=Stage::CombineFlags;break;
  case Stage::CombineFlags:flags_|=middle_;stage_=Stage::ClearMiddle;break;case Stage::ClearMiddle:middle_=0;stage_=Stage::CaptureHigh;break;
  case Stage::CaptureHigh:stage_=Stage::ReadLow;break;case Stage::ReadLow:low_=fullGate()?uint8_t(read(Read::Low)):uint8_t(0xbf);stage_=fullGate()?Stage::MaskLow:Stage::ReadDetune;break;
  case Stage::MaskLow:low_&=7;stage_=Stage::FixedLow;break;case Stage::FixedLow:low_|=0xb8;stage_=Stage::ReadDetune;break;
  case Stage::ReadDetune:detune_=uint8_t(read(Read::Detune));stage_=Stage::BiasDetune;break;case Stage::BiasDetune:detune_=uint8_t(detune_-64);stage_=Stage::AdmitDetune;break;
  case Stage::AdmitDetune:stage_=detune_?Stage::EnableShift:Stage::GateOffset;break;case Stage::EnableShift:low_|=0x40;stage_=Stage::GateOffset;break;
  case Stage::GateOffset:stage_=Stage::DispatchGate;break;case Stage::DispatchGate:stage_=Stage::AwaitGate;break;case Stage::Finish:stage_=Stage::Complete;break;default:break;}
  used+=cost;
 }return used;}
private:
 Kind kind_;Stage stage_;uint8_t slot_=0,velocity_=0,allocation_=0,flags_=0,middle_=0,low_=0,detune_=0;bool suppressed_=false,handoff_=false;
};
}
