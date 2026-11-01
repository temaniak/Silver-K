#pragma once
#include "ksNativeVoiceAllocationService.h"
#include "ksVoiceStartTask.h"
#include "ksUnisonTask.h"
#include "ksVoiceTrackingTask.h"
#include "ksParameterUploadService.h"
namespace ks {
// One complete musical start, including live extra-voice count and the
// velocity/glide tail used when a slot is resumed. No processor model is owned.
class NativeVoiceStartService final {
public:
 enum class Kind{Unison,Voice,ResumeGate,FullGate};
 enum class Component{Unison,Start,Allocation,Tracking,Prime,Parameters,Gate,Velocity,Glide,Lookup,Coefficient,Dsp};
 struct Input{uint16_t noteVelocity=0;uint32_t patch=0;DspPort port=DspPort::B;uint16_t address=0;};
 struct Effect{bool read=false,write=false;};
 struct Summary{Input input{};uint32_t patch=0;DspPort port=DspPort::B;uint16_t address=0;uint64_t phases=0,words=0,starts=0;bool tail=false,allocationObserved=false,distanceObserved=false,voiceObserved=false;uint8_t voice=0;uint64_t coefficientOrder=0,productDiscardOrder=0,retryOrder=0;uint32_t coefficient=0;uint16_t remaining=0;};
 NativeVoiceStartService(Kind kind,Input in):kind_(kind),input_(in){summary_.input=in;summary_.patch=in.patch;summary_.port=in.port;summary_.address=in.address;if(kind==Kind::Unison){unison_.emplace();choose(Component::Unison);}else{if(!dataSpan(in.patch,256))throw std::logic_error("Unsupported voice start patch");beginStart(kind==Kind::Voice?VoiceStartTask::Kind::Voice:kind==Kind::ResumeGate?VoiceStartTask::Kind::ResumeGate:VoiceStartTask::Kind::FullGate);}}
 Kind kind()const{return kind_;}bool complete()const{return complete_;}bool critical()const{return false;}Component component()const{return component_;}uint32_t components()const{return components_;}const Effect&effect()const{return effect_;}const Summary&summary()const{return summary_;}
 const VoiceStartTask*start()const{return start_?&*start_:nullptr;}const UnisonTask*unison()const{return unison_?&*unison_:nullptr;}const NativeVoiceAllocationService*allocation()const{return allocation_?&*allocation_:nullptr;}const VoiceTrackingTask*tracking()const{return tracking_?&*tracking_:nullptr;}const ParameterUploadService*parameters()const{return parameters_?&*parameters_:nullptr;}const GlideTask*glide()const{return glide_?&*glide_:nullptr;}const ControllerDspService*gate()const{return gate_?&*gate_:nullptr;}const ControllerDspService*trackingOutput()const{return trackingOutput_?&*trackingOutput_:nullptr;}
 template<class T>const T*task()const{
  if constexpr(std::is_same_v<T,NativeVoiceStartService>)return this;
  else if constexpr(std::is_same_v<T,UnisonTask>)return component_==Component::Unison?unison():nullptr;
  else if constexpr(std::is_same_v<T,VoiceStartTask>)return component_==Component::Start?start():nullptr;
  else if constexpr(std::is_same_v<T,NativeVoiceAllocationService>)return component_==Component::Allocation?allocation():nullptr;
  else if constexpr(std::is_same_v<T,VoiceTrackingTask>)return component_==Component::Tracking?tracking():nullptr;
  else if constexpr(std::is_same_v<T,ParameterUploadService>)return component_==Component::Parameters?parameters():nullptr;
  else if constexpr(std::is_same_v<T,GlideTask>)return component_==Component::Glide?glide():nullptr;
  else if constexpr(std::is_same_v<T,VelocityModulationTask>)return component_==Component::Velocity?&*velocity_:nullptr;
  else if constexpr(std::is_same_v<T,ControllerCoefficientService>)return component_==Component::Coefficient?&*coefficient_:nullptr;
  else if constexpr(std::is_same_v<T,PartLookupTask>)return component_==Component::Lookup?&*lookup_:component_==Component::Allocation?allocation_->template task<T>():component_==Component::Parameters?parameters_->template task<T>():nullptr;
  else if constexpr(std::is_same_v<T,ControllerDspService>)return component_==Component::Dsp||component_==Component::Gate||component_==Component::Prime?&*dsp_:component_==Component::Allocation?allocation_->template task<T>():component_==Component::Parameters?parameters_->template task<T>():nullptr;
  else if constexpr(std::is_same_v<T,DspRoutingTask>||std::is_same_v<T,DspAddressTask>||std::is_same_v<T,DspWordTask>||std::is_same_v<T,DspSelectTask>||std::is_same_v<T,DspFaultTask>)return component_==Component::Dsp||component_==Component::Gate||component_==Component::Prime?dsp_->template task<T>():component_==Component::Allocation?allocation_->template task<T>():component_==Component::Parameters&&parameters_->template task<ControllerDspService>()?parameters_->template task<ControllerDspService>()->template task<T>():nullptr;
  else return nullptr;
 }
 unsigned nextClocks(int previous=-1)const{if(complete_)return 0;switch(component_){
  case Component::Allocation:return allocation_->nextClocks(previous);case Component::Parameters:return parameters_->nextClocks(previous);case Component::Dsp:case Component::Gate:case Component::Prime:return dsp_->nextClocks(previous);
  case Component::Unison:return unison_->nextClocks(unison_->publication()?byteClock(unisonTarget(unison_->publicationKind()),previous):3);
  case Component::Start:return start_->nextClocks();case Component::Tracking:return tracking_->complete()?17:tracking_->nextClocks(tracking_->publication()?byteClock(0x1755e,previous):3);
  case Component::Velocity:return velocity_->nextClocks();case Component::Glide:return glide_->nextClocks(glide_->publication()?byteClock(glideTarget(glide_->publicationKind()),previous):3);
  case Component::Lookup:return lookup_->stage()==PartLookupTask::Stage::Complete?17:lookup_->nextClocks();case Component::Coefficient:return coefficient_->nextClocks();}return 0;}
 template<class R,class W>unsigned advanceBefore(unsigned available,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||available<cost)return 0;effect_={};++summary_.phases;
  const auto byte=[&](uint32_t raw,unsigned back=0){if(!dataSpan(raw,1))throw std::logic_error("Unsupported evolving voice start byte");return uint8_t(read(raw&0xffffff,cost-back));};
  const auto wide=[&](uint32_t raw,unsigned n){if(!dataSpan(raw,n))throw std::logic_error("Unsupported evolving voice start span");uint32_t v=0;for(unsigned i=0;i<n;++i)v=(v<<8)|byte(raw+i,3*((n-1-i)/2));return v;};
  const auto put=[&](uint32_t raw,uint8_t v){if(!dataSpan(raw,1))throw std::logic_error("Unsupported start publication");write(raw&0xffffff,v,cost);};
  if(component_==Component::Allocation){allocation_->advanceBefore(cost,read,write,previous);effect_={allocation_->effect().read,allocation_->effect().write};if(allocation_->complete()){summary_.port=allocation_->summary().port;summary_.address=allocation_->summary().address;start_->resumeChild(allocation_->summary().slot);choose(Component::Start);}return cost;}
  if(component_==Component::Parameters){parameters_->advanceBefore(cost,read,write,previous);effect_={parameters_->effect().read,parameters_->effect().write};if(parameters_->complete()){summary_.port=parameters_->summary().port;summary_.address=parameters_->summary().cachedAddress;start_->resumeChild();choose(Component::Start);}return cost;}
  if(component_==Component::Lookup){if(lookup_->stage()==PartLookupTask::Stage::Complete){const auto index=lookup_->index();lookup_.reset();effect_.read=true;if(lookupOwner_==Component::Unison){unison_->resumeChild(index);choose(Component::Unison);}else if(lookupOwner_==Component::Velocity){velocity_->resumeChild(index);choose(Component::Velocity);}else{summary_.voiceObserved=true;summary_.voice=index;glide_->resumeChild(index);choose(Component::Glide);}}else lookup_->advanceBefore(cost,[&](PartLookupTask::Layout k){return byte(k==PartLookupTask::Layout::Voice?0x174a7:0x17526);});return cost;}
  if(component_==Component::Coefficient){coefficient_->advanceBefore(cost);effect_={coefficient_->effect().read,coefficient_->effect().write};if(coefficient_->complete()){summary_.coefficient=uint32_t(coefficient_->value());summary_.coefficientOrder=summary_.phases;glide_->resumeChild(summary_.coefficient);coefficient_.reset();choose(Component::Glide);}return cost;}
  if(component_==Component::Dsp||component_==Component::Gate||component_==Component::Prime){const auto owner=dspOwner_;dsp_->advanceBefore(cost,read,write,previous);effect_={dsp_->effect().read,dsp_->effect().write};if(dsp_->complete()){summary_.port=dsp_->port();summary_.address=dsp_->address();if(dsp_->summary().retried){summary_.retryOrder=summary_.phases;summary_.remaining=dsp_->summary().remaining;}
   if(owner==Component::Tracking){trackingOutput_=dsp_;tracking_->resumeChild();choose(Component::Tracking);}
   else if(owner==Component::Velocity){velocity_->resumeChild();finishVelocity();}
   else if(owner==Component::Glide){glide_->resumeChild();finishGlide();}
   else{if(owner==Component::Gate)gate_=dsp_;start_->resumeChild();choose(Component::Start);}dsp_.reset();}return cost;}
  if(component_==Component::Unison){using T=UnisonTask;const auto stage=unison_->stage();unison_->advanceBefore(cost,[&](T::Read q)->uint32_t{switch(q){case T::Read::Note:return uint8_t(input_.noteVelocity);case T::Read::Velocity:return uint8_t(input_.noteVelocity>>8);case T::Read::PatchReference:summary_.patch=wide(0x174aeu+20*unison_->part(),4);if(!dataSpan(summary_.patch,256))throw std::logic_error("Unsupported unison patch binding");return 0;case T::Read::Mode:return byte(summary_.patch);case T::Read::Unison:return byte(0x174aeu+20*unison_->part()+18);}return 0;},[&](T::Write q,uint32_t v){put(unisonTarget(q),uint8_t(v));},[&](T::Write q){return byteClock(unisonTarget(q),previous);});
   if(stage==T::Stage::RetainPart)effect_.write=true;if(stage==T::Stage::ReleasePart)effect_.read=true;
   if(unison_->child()==T::Child::Part||unison_->child()==T::Child::PartAgain){effect_.write=true;startLookup(Component::Unison,PartLookupTask::Layout::Controls);}else if(unison_->child()==T::Child::Single){effect_.write=true;beginStart(VoiceStartTask::Kind::Voice);}else if(unison_->complete()){summary_.tail=true;beginStart(VoiceStartTask::Kind::Voice);}return cost;}
  if(component_==Component::Tracking){using T=VoiceTrackingTask;if(tracking_->complete()){effect_.read=true;start_->resumeChild();choose(Component::Start);return cost;}
   tracking_->advanceBefore(cost,[&](T::Read q)->uint32_t{switch(q){case T::Read::Part:return byte(0x17526);case T::Read::ActivePart:return byte(0x16798);case T::Read::Voice:return byte(0x174a7);case T::Read::Tracked:return byte(0x1755e);default:throw std::logic_error("Unsupported start tracking read");}},[&](T::Write,uint32_t v){put(0x1755e,uint8_t(v));},[&](T::Write){return byteClock(0x1755e,previous);});
   if(tracking_->child()!=T::Child::None){effect_.write=true;startDsp(Component::Tracking,DspRoutingTask::Kind::WriteB,tracking_->absoluteOffset(),uint32_t(tracking_->absoluteValue()));}return cost;}
  if(component_==Component::Velocity){using T=VelocityModulationTask;velocity_->advanceBefore(cost,[&](T::Read q){return q==T::Read::Velocity?byte(0x17560u+20*velocity_->slot()+6):byte(summary_.patch+velocityFields[velocityIndex_]);});if(velocity_->child()==T::Child::Voice){effect_.write=true;startLookup(Component::Velocity,PartLookupTask::Layout::Voice);}else if(velocity_->child()==T::Child::Word){effect_.write=true;startDsp(Component::Velocity,DspRoutingTask::Kind::WriteRelative,velocity_->offset(),velocity_->dspWord());}return cost;}
  if(component_==Component::Glide){using T=GlideTask;const auto stage=glide_->stage();const auto slot=0x17560u+20*glide_->slot();glide_->advanceBefore(cost,[&](T::Read q)->uint32_t{switch(q){case T::Read::PatchPitch:return byte(summary_.patch+168);case T::Read::SlotPitch:case T::Read::Transposed:return byte(slot+13);case T::Read::Detune:return byte(summary_.patch+4);case T::Read::Time:return byte(summary_.patch+3);case T::Read::Mode:return byte(summary_.patch);case T::Read::Allocation:if(stage==T::Stage::ReadAllocation)summary_.allocationObserved=true;return byte(0x1752a);case T::Read::Base:return wide(0x1513c,2);case T::Read::DistanceMode:return byte(summary_.patch+5);case T::Read::Distance:summary_.distanceObserved=true;return byte(slot+7);case T::Read::Previous:return byte(0x1752d);}return 0;},[&](T::Write q,uint32_t v){put(glideTarget(q),uint8_t(v));},[&](T::Write q){return byteClock(glideTarget(q),previous);});
   if(stage==T::Stage::RetainCoefficient)effect_.write=true;if(stage==T::Stage::ReleaseCoefficient)effect_.read=true;if(stage==T::Stage::ClearLowHigh)summary_.productDiscardOrder=summary_.phases;
   const auto child=glide_->child();if(child==T::Child::PitchVoice||child==T::Child::DetuneVoice||child==T::Child::DistanceVoice){effect_.write=true;startLookup(Component::Glide,PartLookupTask::Layout::Voice);}else if(child==T::Child::Coefficient){effect_.write=true;coefficient_.emplace(int16_t(glide_->rate()));choose(Component::Coefficient);}else if(child!=T::Child::None){effect_.write=true;startDsp(Component::Glide,DspRoutingTask::Kind::WriteRelative,glide_->offset(),glide_->dspWord());}else finishGlide();return cost;}
  using T=VoiceStartTask;start_->advanceBefore(cost,[&](T::Read q)->uint32_t{switch(q){case T::Read::Velocity:return byte(0x17560u+20*start_->slot()+6);case T::Read::Allocation:return byte(0x1752a);case T::Read::Flags:return byte(summary_.patch+79);case T::Read::Middle:return byte(summary_.patch+(start_->fullGate()?78:147));case T::Read::Low:return byte(summary_.patch+80);case T::Read::Detune:return byte(summary_.patch+4);}return 0;});
  if(start_->child()!=T::Child::None){effect_.write=true;switch(start_->child()){
   case T::Child::Allocation:allocation_.emplace(NativeVoiceAllocationService::Kind::Allocate,NativeVoiceAllocationService::Input{summary_.patch});choose(Component::Allocation);break;
   case T::Child::Tracking:tracking_.emplace(VoiceTrackingTask::Kind::Observe,summary_.patch<0x839000u);choose(Component::Tracking);break;
   case T::Child::Prime:startDsp(Component::Prime,DspRoutingTask::Kind::PrimeRelative,VoiceStartTask::primeOffset(),0);break;
   case T::Child::Parameters:parameters_.emplace(ParameterUploadService::Input{summary_.patch,summary_.port,summary_.address});choose(Component::Parameters);break;
   case T::Child::Gate:startDsp(Component::Gate,DspRoutingTask::Kind::WriteRelative,VoiceStartTask::gateOffset(),start_->dspWord());break;
   default:break;}
  }else if(start_->complete()){if(start_->handoff()){velocityIndex_=0;velocity_.emplace(true,velocityOffsets[0]);choose(Component::Velocity);}else{effect_.read=true;finishStart();}}return cost;
 }
 static bool dataSpan(uint32_t raw,unsigned n){return NativeVoiceAllocationService::dataSpan(raw,n);}
private:
 static constexpr unsigned velocityFields[3]{58,61,66};static constexpr uint16_t velocityOffsets[3]{0x6b,0x6f,0x77};
 static unsigned byteClock(uint32_t raw,int previous){const auto a=raw&0xffffff;return a<0x400000&&!(previous&1)&&a==(uint32_t(previous)|1u)?0:3;}
 uint32_t unisonTarget(UnisonTask::Write q)const{return q==UnisonTask::Write::Note?0x17528:q==UnisonTask::Write::Velocity?0x17529:0x174aeu+20*unison_->part()+18;}
 uint32_t glideTarget(GlideTask::Write q)const{return q==GlideTask::Write::Previous?0x1752d:0x17560u+20*glide_->slot()+7;}
 void choose(Component c){component_=c;components_|=1u<<unsigned(c);}
 void startLookup(Component owner,PartLookupTask::Layout layout){lookupOwner_=owner;lookup_.emplace(layout);choose(Component::Lookup);}
 void startDsp(Component owner,DspRoutingTask::Kind routing,uint16_t address,uint32_t word){dspOwner_=owner;ControllerDspService::Input in{routing,summary_.port,address,word};dsp_.emplace(ControllerDspService::Kind::Routing,in);choose(owner==Component::Gate||owner==Component::Prime?owner:Component::Dsp);++summary_.words;}
 void beginStart(VoiceStartTask::Kind kind){++summary_.starts;start_.emplace(kind);allocation_.reset();tracking_.reset();trackingOutput_.reset();parameters_.reset();gate_.reset();glide_.reset();summary_.allocationObserved=summary_.distanceObserved=summary_.voiceObserved=false;summary_.coefficientOrder=summary_.productDiscardOrder=summary_.retryOrder=0;choose(Component::Start);}
 void finishStart(){if(kind_==Kind::Unison&&!summary_.tail){unison_->resumeChild();choose(Component::Unison);}else complete_=true;}
 void finishVelocity(){if(!velocity_->complete()){choose(Component::Velocity);return;}if(++velocityIndex_==3){glide_.emplace(true);choose(Component::Glide);}else{velocity_.emplace(true,velocityOffsets[velocityIndex_]);choose(Component::Velocity);}}
 void finishGlide(){if(!glide_->complete()){choose(Component::Glide);return;}const auto kind=glide_->resumeGate()?VoiceStartTask::Kind::ResumeGate:VoiceStartTask::Kind::FullGate;start_.emplace(kind);choose(Component::Start);}
 Kind kind_;Input input_;Summary summary_{};Effect effect_{};bool complete_=false;Component component_=Component::Start,lookupOwner_=Component::Unison,dspOwner_=Component::Start;uint32_t components_=0;unsigned velocityIndex_=0;
 std::optional<UnisonTask>unison_;std::optional<VoiceStartTask>start_;std::optional<NativeVoiceAllocationService>allocation_;std::optional<VoiceTrackingTask>tracking_;std::optional<ControllerDspService>trackingOutput_,dsp_,gate_;std::optional<ParameterUploadService>parameters_;std::optional<VelocityModulationTask>velocity_;std::optional<GlideTask>glide_;std::optional<PartLookupTask>lookup_;std::optional<ControllerCoefficientService>coefficient_;
};
}
