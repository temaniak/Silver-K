#pragma once
#include "kstationNativeMidiEvent.h"
#include "kstationNativeMidiControl.h"
#include <array>
#include <cstdint>
#include <type_traits>
#include <variant>
namespace kstation {
// Temporary H8 caller projection. The owner composes only the explicitly
// lifted tasks. Unlifted children are reported, never executed here.
class NativeMidiEvents final {
 using H=NativeMidiEventHelper;using E=NativeMidiExpressionEvent;using N=NativeMidiNoteEvent;
 using O=NativeMidiHeldOn;using F=NativeMidiHeldOff;using R=NativeMidiRemoveOrder;using C=NativeMidiClearNotes;using P=NativeMidiPhysicalScan;using Q=NativeMidiNearestNote;using S=NativeMidiSeekNote;using I=NativeMidiFirstNote;using T=NativeMidiTrigger;using D=NativeMidiDspTrigger;using A=NativeMidiApplyExpression;using V=NativeMidiVoiceRelease;using B=NativeMidiVoiceStartPrefix;using W=NativeMidiVoiceBuild;using M=NativeMidiVoiceModulation;using G=NativeMidiVoiceGlide;using Y=NativeMidiVoiceTiming;using U=NativeMidiFloatConvert;using Z=NativeMidiVoicePortamento;using J=NativeMidiVoiceFinal;using CC=NativeMidiControl;using GC=NativeMidiGeneralControl;using AC=NativeMidiApplyControl;
 using Job=std::variant<std::monostate,H,E,N,O,F,R,C,P,Q,S,I,T,D,A,V,B,W,M,G,Y,U,Z,J,CC,GC,AC>;
public:
 enum class External:uint8_t { None,VoiceStart,VoiceRelease,FirstNoteDspReset,TriggerTail,ApplyPitch,ApplyPressure,DspControl,PartLookup,PitchRefresh,PressureRefresh,PressureCommit,VoiceAddress,DeviceVoice,FinalizeVoiceRelease,VoiceBuild,VoiceAllocator,VoicePrepare,InitialCommand,VoiceParameters,LongVoiceParameters,SendVoiceCommand,LongVoiceContinuation,VoiceTail,FloatConvert,CcModulation,CcBreath,CcVolume,GeneralControl,AllSoundOff,ReleaseAllVoices,SetNrpn,ReleaseSustain,ControlParameter };
 using Range=NativeMidiEventImageRanges::Range;
 inline static constexpr auto CodeRanges=NativeMidiEventImageRanges::ranges;
 static bool candidate(uint32_t pc){for(auto range:CodeRanges)if(pc>=range.begin&&pc<range.end)return true;return false;}
 void reset(){depth_=0;for(auto&j:jobs_)j.emplace<std::monostate>();}
 bool active()const{return depth_!=0;}
 // Callers that own a direct 33F4 invocation must provide its verified return.
 // No memory or architectural state is read beyond the admitted call frame.
 bool admitTrigger(Stage1Cpu&c,uint32_t callerReturn){if(depth_||midi_projection::blocked(c)||uint32_t(c.getPC())!=0x33f4)return false;jobs_[0].emplace<T>(uint8_t(c.reg8(13)),callerReturn);depth_=1;return true;}
 bool admitControl(Stage1Cpu&c,uint32_t callerReturn){if(depth_||midi_projection::blocked(c)||uint32_t(c.getPC())!=0x2e34)return false;jobs_[0].emplace<CC>(callerReturn);depth_=1;return true;}
 External external()const {if(!depth_)return External::None;const auto&j=jobs_[depth_-1];
  if(auto*p=std::get_if<D>(&j);p&&p->task().awaitingChild())return External::DspControl;
  if(auto*p=std::get_if<A>(&j);p&&p->task().awaitingChild()){switch(p->task().child()){case MidiApplyExpressionTask::Child::LookupPart:return External::PartLookup;case MidiApplyExpressionTask::Child::UpdatePitch:return External::PitchRefresh;case MidiApplyExpressionTask::Child::UpdatePressure:return External::PressureRefresh;case MidiApplyExpressionTask::Child::UpdatePressureTail:return External::PressureCommit;}}
  if(auto*p=std::get_if<V>(&j);p&&p->task().awaitingChild()){switch(p->task().child()){case MidiVoiceReleaseTask::Child::VoiceAddress:return External::VoiceAddress;case MidiVoiceReleaseTask::Child::DeviceVoice:return External::DeviceVoice;case MidiVoiceReleaseTask::Child::ReleaseVoice:return External::FinalizeVoiceRelease;}}
  if(auto*p=std::get_if<B>(&j);p&&p->task().awaitingChild()&&p->task().child()==MidiVoiceStartPrefixTask::Child::LookupPart)return External::PartLookup;
  if(auto*p=std::get_if<W>(&j);p&&p->task().awaitingChild()){switch(p->task().child()){case MidiVoiceBuildTask::Child::Allocate:return External::VoiceAllocator;case MidiVoiceBuildTask::Child::Prepare:return External::VoicePrepare;case MidiVoiceBuildTask::Child::InitialCommand:return External::InitialCommand;case MidiVoiceBuildTask::Child::Parameters:return External::VoiceParameters;case MidiVoiceBuildTask::Child::LongParameters:break;case MidiVoiceBuildTask::Child::SendCommand:return External::SendVoiceCommand;}}
  if(auto*p=std::get_if<M>(&j);p&&p->task().awaitingChild()){switch(p->task().child()){case MidiVoiceModulationTask::Child::VoiceAddress:return External::VoiceAddress;case MidiVoiceModulationTask::Child::SendCommand:return External::SendVoiceCommand;case MidiVoiceModulationTask::Child::ContinueLong:break;}}
  if(auto*p=std::get_if<G>(&j);p&&p->task().awaitingChild()){switch(p->task().child()){case MidiVoiceGlideTask::Child::VoiceAddress:return External::VoiceAddress;case MidiVoiceGlideTask::Child::SendCommand:return External::SendVoiceCommand;case MidiVoiceGlideTask::Child::ContinueLong:break;}}
  if(auto*p=std::get_if<Z>(&j);p&&p->task().awaitingChild()){switch(p->task().child()){case MidiVoicePortamentoTask::Child::VoiceAddress:return External::VoiceAddress;case MidiVoicePortamentoTask::Child::SendFirst:case MidiVoicePortamentoTask::Child::SendSecond:return External::SendVoiceCommand;case MidiVoicePortamentoTask::Child::ContinueLong:break;}}
  if(auto*p=std::get_if<J>(&j);p&&p->task().awaitingChild()&&p->task().child()!=MidiVoiceFinalTask::Child::SharedFinal)return External::SendVoiceCommand;
  if(auto*p=std::get_if<CC>(&j);p&&p->task().awaitingChild()){switch(p->task().child()){case MidiControlTask::Child::Modulation:case MidiControlTask::Child::Breath:break;case MidiControlTask::Child::Volume:return External::CcVolume;case MidiControlTask::Child::General:break;default:break;}}
  if(auto*p=std::get_if<GC>(&j);p&&p->task().awaitingChild()){switch(p->task().child()){case MidiGeneralControlTask::Child::AllSoundOff:return External::AllSoundOff;case MidiGeneralControlTask::Child::ReleaseVoices:return External::ReleaseAllVoices;case MidiGeneralControlTask::Child::SetNrpn:return External::SetNrpn;case MidiGeneralControlTask::Child::ReleaseSustain:return External::ReleaseSustain;case MidiGeneralControlTask::Child::Parameter:return External::ControlParameter;default:break;}}
  if(auto*p=std::get_if<AC>(&j);p&&p->task().awaitingChild()){switch(p->task().child()){case MidiApplyControlTask::Child::LookupPart:return External::PartLookup;case MidiApplyControlTask::Child::Refresh:return External::PressureRefresh;case MidiApplyControlTask::Child::Commit:return External::PressureCommit;}}
  return External::None;
 }
 bool step(Stage1Cpu&c){if(midi_projection::blocked(c))return false;
  for(unsigned turns=0;turns<jobs_.size()+1;++turns){
   if(!depth_){if(!push(c,false))return false;}
   auto&j=jobs_[depth_-1];
   if(awaiting(j)){
    if(resume(j,c)){if(complete(j)){j.emplace<std::monostate>();--depth_;continue;}continue;}
    if(uint32_t(c.getPC())!=childEntry(j)||!push(c,true))return false;
    continue;
   }
   const bool advanced=std::visit([&](auto&v)->bool{using V=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<V,std::monostate>)return false;else return v.step(c);},j);
   if(!advanced)return false;
   if(complete(j)){j.emplace<std::monostate>();--depth_;}
   return true;
  }
  return false;
 }
private:
 static bool awaiting(const Job&j){return std::visit([](const auto&v)->bool{using V=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<V,E>||std::is_same_v<V,N>||std::is_same_v<V,O>||std::is_same_v<V,F>||std::is_same_v<V,Q>||std::is_same_v<V,I>||std::is_same_v<V,T>||std::is_same_v<V,D>||std::is_same_v<V,A>||std::is_same_v<V,NativeMidiVoiceRelease>||std::is_same_v<V,NativeMidiVoiceStartPrefix>||std::is_same_v<V,NativeMidiVoiceBuild>||std::is_same_v<V,NativeMidiVoiceModulation>||std::is_same_v<V,NativeMidiVoiceGlide>||std::is_same_v<V,NativeMidiVoiceTiming>||std::is_same_v<V,NativeMidiVoicePortamento>||std::is_same_v<V,NativeMidiVoiceFinal>||std::is_same_v<V,NativeMidiControl>||std::is_same_v<V,NativeMidiGeneralControl>||std::is_same_v<V,NativeMidiApplyControl>)return v.task().awaitingChild();else return false;},j);}
 static bool complete(const Job&j){return std::visit([](const auto&v)->bool{using V=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<V,std::monostate>)return true;else return v.task().complete();},j);}
 static bool resume(Job&j,Stage1Cpu&c){return std::visit([&](auto&v)->bool{using V=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<V,E>||std::is_same_v<V,N>||std::is_same_v<V,O>||std::is_same_v<V,F>||std::is_same_v<V,Q>||std::is_same_v<V,I>||std::is_same_v<V,T>||std::is_same_v<V,D>||std::is_same_v<V,A>||std::is_same_v<V,NativeMidiVoiceRelease>||std::is_same_v<V,NativeMidiVoiceStartPrefix>||std::is_same_v<V,NativeMidiVoiceBuild>||std::is_same_v<V,NativeMidiVoiceModulation>||std::is_same_v<V,NativeMidiVoiceGlide>||std::is_same_v<V,NativeMidiVoiceTiming>||std::is_same_v<V,NativeMidiVoicePortamento>||std::is_same_v<V,NativeMidiVoiceFinal>||std::is_same_v<V,NativeMidiControl>||std::is_same_v<V,NativeMidiGeneralControl>||std::is_same_v<V,NativeMidiApplyControl>)return v.resumeChild(c);else return false;},j);}
 static uint32_t childEntry(const Job&j){
  if(auto*p=std::get_if<E>(&j)){switch(p->task().child()){case MidiExpressionEventTask::Child::StoreChannel:return 0x2d38;case MidiExpressionEventTask::Child::ChannelMatch:return 0x2d5c;case MidiExpressionEventTask::Child::ApplyPitch:return 0x80ce;case MidiExpressionEventTask::Child::ApplyPressure:return 0x8140;}}
  if(auto*p=std::get_if<N>(&j)){switch(p->task().child()){case MidiNoteEventTask::Child::ChannelMatch:return 0x2d5c;case MidiNoteEventTask::Child::HeldOn:return 0x2a82;case MidiNoteEventTask::Child::HeldOff:return 0x2afe;case MidiNoteEventTask::Child::PreviousNote:return 0x2c36;case MidiNoteEventTask::Child::Trigger:return 0x33f4;case MidiNoteEventTask::Child::Release:return 0x759a;}}
  if(auto*p=std::get_if<O>(&j))return p->task().child()==MidiHeldOnTask::Child::AnyPhysical?0x2bf6:0x3812;
  if(std::holds_alternative<F>(j))return 0x2b38;
  if(auto*p=std::get_if<Q>(&j))return p->task().child()==MidiNearestNoteTask::Child::Upper?0x2c86:0x2cb8;
  if(auto*p=std::get_if<I>(&j)){switch(p->task().child()){case MidiFirstNoteTask::Child::HeldScan:return 0x2c16;case MidiFirstNoteTask::Child::ClearNotes:return 0x2b7a;case MidiFirstNoteTask::Child::ResetDsp:return 0x823c;}}
  if(auto*p=std::get_if<T>(&j))return p->task().child()==MidiTriggerTask::Child::StartVoice?0x7302:0x8242;
  if(std::holds_alternative<D>(j))return 0x8260;
  if(auto*p=std::get_if<A>(&j)){switch(p->task().child()){case MidiApplyExpressionTask::Child::LookupPart:return 0x72ce;case MidiApplyExpressionTask::Child::UpdatePitch:return 0x78c2;case MidiApplyExpressionTask::Child::UpdatePressure:return 0x785c;case MidiApplyExpressionTask::Child::UpdatePressureTail:return 0x5e64;}}
  if(auto*p=std::get_if<V>(&j)){switch(p->task().child()){case MidiVoiceReleaseTask::Child::VoiceAddress:return 0x72ba;case MidiVoiceReleaseTask::Child::DeviceVoice:return 0x5bb8;case MidiVoiceReleaseTask::Child::ReleaseVoice:return 0x6f44;}}
  if(auto*p=std::get_if<B>(&j))return p->task().child()==MidiVoiceStartPrefixTask::Child::LookupPart?0x72ce:0x7360;
  if(auto*p=std::get_if<W>(&j)){switch(p->task().child()){case MidiVoiceBuildTask::Child::Allocate:return 0x6fde;case MidiVoiceBuildTask::Child::Prepare:return 0x837c;case MidiVoiceBuildTask::Child::InitialCommand:return 0x5ba0;case MidiVoiceBuildTask::Child::Parameters:return 0x610e;case MidiVoiceBuildTask::Child::LongParameters:return 0x73ac;case MidiVoiceBuildTask::Child::SendCommand:return 0x5bec;}}
  if(auto*p=std::get_if<M>(&j)){switch(p->task().child()){case MidiVoiceModulationTask::Child::VoiceAddress:return 0x72ba;case MidiVoiceModulationTask::Child::SendCommand:return 0x5bec;case MidiVoiceModulationTask::Child::ContinueLong:return 0x7448;}}
  if(auto*p=std::get_if<G>(&j)){switch(p->task().child()){case MidiVoiceGlideTask::Child::VoiceAddress:return 0x72ba;case MidiVoiceGlideTask::Child::SendCommand:return 0x5bec;case MidiVoiceGlideTask::Child::ContinueLong:return 0x7492;}}
  if(auto*p=std::get_if<Y>(&j))return p->task().child()==MidiVoiceTimingTask::Child::Convert?0x5c04:0x74d4;
  if(auto*p=std::get_if<Z>(&j)){switch(p->task().child()){case MidiVoicePortamentoTask::Child::VoiceAddress:return 0x72ba;case MidiVoicePortamentoTask::Child::SendFirst:case MidiVoicePortamentoTask::Child::SendSecond:return 0x5bec;case MidiVoicePortamentoTask::Child::ContinueLong:return 0x753e;}}
  if(auto*p=std::get_if<J>(&j))return p->task().child()==MidiVoiceFinalTask::Child::SharedFinal?0x737e:0x5bec;
  if(auto*p=std::get_if<CC>(&j)){switch(p->task().child()){case MidiControlTask::Child::StoreChannel:return 0x2d38;case MidiControlTask::Child::Match:return 0x2d5c;case MidiControlTask::Child::Modulation:return 0x80f4;case MidiControlTask::Child::Breath:return 0x811a;case MidiControlTask::Child::Volume:return 0x403c;case MidiControlTask::Child::General:return 0x2ea8;}}
  if(auto*p=std::get_if<GC>(&j)){switch(p->task().child()){case MidiGeneralControlTask::Child::Match:return 0x2d5c;case MidiGeneralControlTask::Child::AllSoundOff:return 0x7760;case MidiGeneralControlTask::Child::ReleaseVoices:return 0x7644;case MidiGeneralControlTask::Child::ClearHeld:return 0x2b7a;case MidiGeneralControlTask::Child::SetNrpn:return 0x575a;case MidiGeneralControlTask::Child::ReleaseSustain:return 0x76d6;case MidiGeneralControlTask::Child::Parameter:return 0x576a;}}
  if(auto*p=std::get_if<AC>(&j)){switch(p->task().child()){case MidiApplyControlTask::Child::LookupPart:return 0x72ce;case MidiApplyControlTask::Child::Refresh:return 0x785c;case MidiApplyControlTask::Child::Commit:return 0x5e64;}}
  return 0;
 }
 bool push(Stage1Cpu&c,bool nested){if(depth_==jobs_.size())return false;const auto at=uint32_t(c.getPC());auto&j=jobs_[depth_];const auto note=uint8_t(c.reg8(13));
  switch(at){
   case 0x2d38:j.emplace<H>(MidiEventHelperTask::Kind::StoreChannel,uint32_t(c.reg32(1)),uint8_t(c.reg8(8)));break;
   case 0x2d5c:j.emplace<H>(MidiEventHelperTask::Kind::ChannelMatch);break;
   case 0x2df0:j.emplace<E>(MidiExpressionEventTask::Kind::PitchBend);break;case 0x2e18:j.emplace<E>(MidiExpressionEventTask::Kind::ChannelPressure);break;
   case 0x2dc8:j.emplace<N>(MidiNoteEventTask::Kind::On);break;case 0x2d7e:j.emplace<N>(MidiNoteEventTask::Kind::Off);break;
   case 0x2a82:j.emplace<O>(note,uint8_t(c.reg8(5)));break;case 0x2afe:j.emplace<F>(note);break;case 0x2b38:j.emplace<R>(note);break;case 0x2b7a:j.emplace<C>();break;
   case 0x2bf6:j.emplace<P>(MidiPhysicalScanTask::Kind::Physical);break;case 0x2c16:j.emplace<P>(MidiPhysicalScanTask::Kind::Held);break;case 0x2c36:j.emplace<Q>(note);break;
   case 0x2c86:j.emplace<S>(MidiSeekNoteTask::Kind::Upper,note,uint32_t(c.reg32(3)));break;case 0x2cb8:j.emplace<S>(MidiSeekNoteTask::Kind::Lower,note,uint32_t(c.reg32(3)));break;
   case 0x3812:j.emplace<I>();break;
   case 0x2ea8:{if(!nested||!depth_)return false;const auto*parent=std::get_if<CC>(&jobs_[depth_-1]);if(!parent||!parent->task().awaitingChild()||parent->task().child()!=MidiControlTask::Child::General)return false;j.emplace<GC>(uint8_t(c.reg8(0)),uint8_t(c.reg8(8)));break;}
   case 0x5c04:j.emplace<U>(uint16_t(c.reg16(0)));break;
   case 0x759a:j.emplace<V>(uint8_t(c.reg8(14)),note);break;
   case 0x80f4:j.emplace<AC>(MidiApplyControlTask::Kind::Modulation,uint8_t(c.reg8(14)),note);break;
   case 0x811a:j.emplace<AC>(MidiApplyControlTask::Kind::Breath,uint8_t(c.reg8(14)),note);break;
   case 0x80ce:j.emplace<A>(MidiApplyExpressionTask::Kind::Pitch,uint8_t(c.reg8(14)),note);break;
   case 0x8140:j.emplace<A>(MidiApplyExpressionTask::Kind::Pressure,uint8_t(c.reg8(14)),note);break;
   case 0x737e:{if(!nested||!depth_)return false;const auto*parent=std::get_if<J>(&jobs_[depth_-1]);if(!parent||!parent->task().awaitingChild()||parent->task().child()!=MidiVoiceFinalTask::Child::SharedFinal)return false;j.emplace<W>(parent->callerReturn(),true);break;}
   case 0x753e:{if(!nested||!depth_)return false;const auto*parent=std::get_if<Z>(&jobs_[depth_-1]);if(!parent||!parent->task().awaitingChild()||parent->task().child()!=MidiVoicePortamentoTask::Child::ContinueLong)return false;j.emplace<J>(parent->callerReturn());break;}
   case 0x74d4:{if(!nested||!depth_)return false;const auto*parent=std::get_if<Y>(&jobs_[depth_-1]);if(!parent||!parent->task().awaitingChild()||parent->task().child()!=MidiVoiceTimingTask::Child::ContinueLong)return false;j.emplace<Z>(uint32_t(c.reg32(0)),parent->callerReturn());break;}
   case 0x7492:{if(!nested||!depth_)return false;const auto*parent=std::get_if<G>(&jobs_[depth_-1]);if(!parent||!parent->task().awaitingChild()||parent->task().child()!=MidiVoiceGlideTask::Child::ContinueLong)return false;j.emplace<Y>(parent->callerReturn());break;}
   case 0x7448:{if(!nested||!depth_)return false;const auto*parent=std::get_if<M>(&jobs_[depth_-1]);if(!parent||!parent->task().awaitingChild()||parent->task().child()!=MidiVoiceModulationTask::Child::ContinueLong)return false;j.emplace<G>(parent->callerReturn());break;}
   case 0x73ac:{if(!nested||!depth_)return false;const auto*parent=std::get_if<W>(&jobs_[depth_-1]);if(!parent||!parent->task().awaitingChild()||parent->task().child()!=MidiVoiceBuildTask::Child::LongParameters)return false;j.emplace<M>(parent->callerReturn());break;}
   case 0x7360:{if(!nested||!depth_)return false;const auto*parent=std::get_if<B>(&jobs_[depth_-1]);if(!parent||!parent->task().awaitingChild()||parent->task().child()==MidiVoiceStartPrefixTask::Child::LookupPart)return false;j.emplace<W>(parent->task().child()==MidiVoiceStartPrefixTask::Child::BuildVoice?0x7346:parent->callerReturn());break;}
   case 0x7302:{if(!nested||!depth_)return false;const auto*parent=std::get_if<T>(&jobs_[depth_-1]);if(!parent||!parent->task().awaitingChild()||parent->task().child()!=MidiTriggerTask::Child::StartVoice)return false;j.emplace<B>(uint8_t(c.reg8(14)),note,uint8_t(c.reg8(5)),0x33fe);break;}
   case 0x823c:{if(!nested||!depth_)return false;const auto*parent=std::get_if<I>(&jobs_[depth_-1]);if(!parent||!parent->task().awaitingChild()||parent->task().child()!=MidiFirstNoteTask::Child::ResetDsp)return false;j.emplace<D>(MidiDspTriggerTask::Kind::Reset,0x388a);break;}
   case 0x8242:{if(!nested||!depth_)return false;const auto*parent=std::get_if<T>(&jobs_[depth_-1]);if(!parent||!parent->task().awaitingChild()||parent->task().child()!=MidiTriggerTask::Child::TailPublish)return false;j.emplace<D>(MidiDspTriggerTask::Kind::Note,parent->callerReturn());break;}
   case 0x33f4:{if(!nested||!depth_)return false;const auto*parent=std::get_if<N>(&jobs_[depth_-1]);if(!parent||!parent->task().awaitingChild()||parent->task().child()!=MidiNoteEventTask::Child::Trigger)return false;j.emplace<T>(note,parent->task().offPath()?0x2dbc:0x2dee);break;}
   default:return false;
  }++depth_;return true;
 }
 std::array<Job,16>jobs_{};unsigned depth_=0;
};
}
