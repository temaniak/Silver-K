#pragma once
#include "ksVoiceSelectionTask.h"
#include "ksVoicePopulationTask.h"
#include "ksPartControlTasks.h"
#include "ksVelocityCurveTask.h"
#include "ksControllerDspService.h"
#include <optional>
#include <type_traits>
namespace ks {
// Complete allocation and physical-slot initialization. A candidate, owner,
// velocity curve and live patch binding are domain facts, not CPU state.
class NativeVoiceAllocationService final {
public:
 enum class Kind{Allocate,Populate};
 enum class Component{Selection,Population,Lookup,Mirror,Velocity,Dsp,Gate};
 struct Input{uint32_t patch=0;};struct Effect{bool read=false,write=false;};
 struct Summary{Input input{};uint8_t slot=0,mirrorByte=0;bool population=false,selected=false,pitchRead=false;DspPort port=DspPort::B;uint16_t address=0;};
 NativeVoiceAllocationService(Kind kind,Input in):kind_(kind),input_(in){if(!dataSpan(in.patch,256))throw std::logic_error("Unsupported allocation patch");summary_.input=in;if(kind==Kind::Allocate){selection_.emplace();choose(Component::Selection);}else beginPopulation();}
 Kind kind()const{return kind_;}bool complete()const{return complete_;}bool critical()const{return false;}
 Component component()const{return component_;}uint32_t components()const{return components_;}const Effect&effect()const{return effect_;}const Summary&summary()const{return summary_;}
 const VoiceSelectionTask*selection()const{return selection_?&*selection_:nullptr;}const VoicePopulationTask*population()const{return population_?&*population_:nullptr;}
 const VelocityCurveTask*velocity()const{return velocity_?&*velocity_:nullptr;}
 const ControllerDspService*populationSelect()const{return lastSelect_?&*lastSelect_:nullptr;}
 const ControllerDspService*populationPitch()const{return lastPitch_?&*lastPitch_:nullptr;}
 template<class T>const T*task()const{
  if constexpr(std::is_same_v<T,NativeVoiceAllocationService>)return this;
  else if constexpr(std::is_same_v<T,VoiceSelectionTask>)return component_==Component::Selection?selection():nullptr;
  else if constexpr(std::is_same_v<T,VoicePopulationTask>)return component_==Component::Population?population():nullptr;
  else if constexpr(std::is_same_v<T,PartLookupTask>)return component_==Component::Lookup?&*lookup_:nullptr;
  else if constexpr(std::is_same_v<T,PartMirrorTask>)return component_==Component::Mirror?&*mirror_:nullptr;
  else if constexpr(std::is_same_v<T,VelocityCurveTask>)return component_==Component::Velocity?velocity():nullptr;
  else if constexpr(std::is_same_v<T,ControllerDspService>)return component_==Component::Dsp?&*dsp_:nullptr;
  else if constexpr(std::is_same_v<T,VoiceGateOffTask>)return component_==Component::Gate?&*gate_:nullptr;
  else return component_==Component::Dsp?dsp_->template task<T>():nullptr;
 }
 unsigned nextClocks(int previous=-1)const{
  if(complete_)return 0;
  switch(component_){
   case Component::Selection:return selection_->nextClocks(byteClock(0x174a7,previous));
   case Component::Population:return population_->complete()?17:population_->nextClocks(population_->bytePublication()?byteClock(populationTarget(population_->publication()),previous):3);
   case Component::Lookup:return lookup_->stage()==PartLookupTask::Stage::Complete?17:lookup_->nextClocks();
   case Component::Mirror:return mirror_->stage()==PartMirrorTask::Stage::Complete?17:mirror_->nextClocks(byteClock(0x17512+mirror_->position(),previous));
   case Component::Velocity:return velocity_->complete()?17:velocity_->nextClocks();
   case Component::Dsp:return dsp_->nextClocks(previous);
   case Component::Gate:return gate_->stage()==VoiceGateOffTask::Stage::Complete?17:gate_->nextClocks();
  }return 0;
 }
 template<class R,class W>unsigned advanceBefore(unsigned available,R&&read,W&&write,int previous=-1){
  const auto cost=nextClocks(previous);if(!cost||available<cost)return 0;effect_={};
  const auto byte=[&](uint32_t a,unsigned back=0){if(!dataSpan(a,1))throw std::logic_error("Unsupported evolving allocation byte");return uint8_t(read(a&0xffffff,cost-back));};
  const auto wide=[&](uint32_t a,unsigned n){if(!dataSpan(a,n))throw std::logic_error("Unsupported evolving allocation span");uint32_t v=0;for(unsigned i=0;i<n;++i)v=(v<<8)|byte(a+i,3*((n-1-i)/2));return v;};
  const auto put=[&](uint32_t a,uint32_t value,unsigned n){if(!dataSpan(a,n))throw std::logic_error("Unsupported allocation publication");for(unsigned i=0;i<n;++i)write((a+i)&0xffffff,uint8_t(value>>(8*(n-1-i))),cost-3*((n-1-i)/2));};
  if(component_==Component::Lookup){
   if(lookup_->stage()==PartLookupTask::Stage::Complete){const auto index=lookup_->index();lookup_.reset();effect_.read=true;
    if(lookupOwner_==LookupOwner::SelectionPart||lookupOwner_==LookupOwner::SelectionVoice){selection_->resumeChild(index);choose(Component::Selection);}
    else {if(lookupOwner_==LookupOwner::PopulationVoice)summary_.slot=index;population_->resumeChild(index);choose(Component::Population);}
   }else lookup_->advanceBefore(cost,[&](PartLookupTask::Layout k){return byte(k==PartLookupTask::Layout::Voice?0x174a7:0x17526);});return cost;
  }
  if(component_==Component::Mirror){
   if(mirror_->stage()==PartMirrorTask::Stage::Complete){mirror_.reset();effect_.read=true;population_->resumeChild();choose(Component::Population);}
   else mirror_->advanceBefore(cost,[&](unsigned part,unsigned at){return summary_.mirrorByte=byte(0x174aeu+20*part+at,2);},[&](unsigned at,uint8_t v){put(0x17512+at,v,1);},[&](unsigned at){return byteClock(0x17512+at,previous);});return cost;
  }
  if(component_==Component::Velocity){
   if(velocity_->complete()){effect_.read=true;population_->resumeChild(velocity_->velocity());choose(Component::Population);}
   else velocity_->advanceBefore(cost,[&](unsigned index){return byte(0x1534eu+index);});return cost;
  }
  if(component_==Component::Dsp){dsp_->advanceBefore(cost,read,write,previous);effect_={dsp_->effect().read,dsp_->effect().write};
   if(dsp_->complete()){
    summary_.port=dsp_->port();summary_.address=dsp_->address();
    if(dspOwner_==DspOwner::PopulationSelect){lastSelect_=dsp_;summary_.selected=true;population_->resumeChild();choose(Component::Population);}
    else if(dspOwner_==DspOwner::PopulationPitch){lastPitch_=dsp_;summary_.pitchRead=true;population_->resumeChild(uint8_t(dsp_->summary().reply>>16));choose(Component::Population);}
    else if(dspOwner_==DspOwner::Gate){gate_->resumeChild();choose(Component::Gate);}
    else {selection_->resumeChild();choose(Component::Selection);}
    dsp_.reset();
   }return cost;
  }
  if(component_==Component::Gate){
   if(gate_->stage()==VoiceGateOffTask::Stage::Complete){gate_.reset();effect_.read=true;selection_->resumeChild();finishSelection();}
   else {const auto stage=gate_->stage();gate_->advanceBefore(cost);if(stage==VoiceGateOffTask::Stage::Dispatch){effect_.write=true;startDsp(DspOwner::Gate,DspRoutingTask::Kind::WriteRelative,VoiceGateOffTask::offset(),0);}}return cost;
  }
  if(component_==Component::Selection){using T=VoiceSelectionTask;using S=T::Stage;const auto stage=selection_->stage();const auto slot=0x17560u+20*selection_->slot(),part=0x174aeu+20*selection_->controlPart();
   selection_->advanceBefore(cost,[&](T::Read q)->uint32_t{
    switch(q){case T::Read::PatchMode:return byte(input_.patch);case T::Read::Held:return byte(part+17);case T::Read::Unison:return byte(part+18);case T::Read::Part:return byte(0x17526);case T::Read::Count:return byte(0x174a6);case T::Read::Age:return wide(slot+8,4);case T::Read::OwnerPart:return byte(slot+4);case T::Read::OwnerUnison:return byte(slot+12);case T::Read::SlotNote:return byte(slot+5);case T::Read::VoiceIndex:return byte(0x174a7);case T::Read::IncomingNote:return byte(0x17528);}throw std::logic_error("Unknown allocation decision observation");
   },[&](T::Write q,uint8_t value){put(q==T::Write::VoiceIndex?0x174a7:0x1752a,value,1);},[&](T::Write){return byteClock(0x174a7,previous);});
   if(selection_->child()!=T::Child::None){effect_.write=true;
    switch(selection_->child()){
     case T::Child::Part:startLookup(LookupOwner::SelectionPart,PartLookupTask::Layout::Controls);break;
     case T::Child::Voice:startLookup(LookupOwner::SelectionVoice,PartLookupTask::Layout::Voice);break;
     case T::Child::SelectVoice:startDsp(DspOwner::Selection,DspRoutingTask::Kind::VoiceB,0,0);break;
     case T::Child::WriteOne:startDsp(DspOwner::Selection,DspRoutingTask::Kind::WriteRelative,T::firstReleaseOffset(),T::releaseCommand());break;
     case T::Child::WriteTwo:startDsp(DspOwner::Selection,DspRoutingTask::Kind::WriteRelative,T::secondReleaseOffset(),T::releaseCommand());break;
     case T::Child::Gate:gate_.emplace();choose(Component::Gate);break;
     default:throw std::logic_error("Unknown allocation child");
    }
   }else if(selection_->complete())finishSelection();(void)stage;return cost;
  }
  using T=VoicePopulationTask;using S=T::Stage;
  if(population_->complete()){complete_=true;effect_.read=true;summary_.population=true;return cost;}
  const auto stage=population_->stage();const auto slot=0x17560u+20*population_->slot(),part=0x174aeu+20*population_->controlPart();
  population_->advanceBefore(cost,[&](T::Read q)->uint32_t{
   switch(q){case T::Read::Note:return byte(0x17528);case T::Read::Transpose:return byte(part+14);case T::Read::Unison:return byte(part+18);case T::Read::Velocity:return byte(0x17529);case T::Read::Curve:return byte(part+16);case T::Read::Power:return byte(0x169f1);case T::Read::Part:return byte(0x17526);case T::Read::Extra:return byte(input_.patch+173);case T::Read::GlideNote:return byte(slot+13);}throw std::logic_error("Unknown slot population observation");
  },[&](T::Write q,uint32_t value){put(populationTarget(q),q==T::Write::PatchReference?input_.patch:value,q==T::Write::PatchReference||q==T::Write::Age?4:1);},[&](T::Write q){return byteClock(populationTarget(q),previous);});
  if(population_->child()!=T::Child::None){effect_.write=true;
   switch(population_->child()){
    case T::Child::Voice:startLookup(LookupOwner::PopulationVoice,PartLookupTask::Layout::Voice);break;
    case T::Child::Part:startLookup(LookupOwner::PopulationPart,PartLookupTask::Layout::Controls);break;
    case T::Child::Velocity:velocity_.emplace(population_->velocity(),population_->curve(),population_->power());choose(Component::Velocity);break;
    case T::Child::Mirror:mirror_.emplace(population_->part());choose(Component::Mirror);break;
    case T::Child::Select:startDsp(DspOwner::PopulationSelect,DspRoutingTask::Kind::VoiceB,0,0);break;
    case T::Child::Pitch:startDsp(DspOwner::PopulationPitch,DspRoutingTask::Kind::ReadAlternate,T::pitchOffset(),0);break;
    default:throw std::logic_error("Unknown slot population child");
   }
  }(void)stage;return cost;
 }
 static bool dataSpan(uint32_t raw,unsigned n){const auto a=raw&0xffffff;return n&&((a>=0xb8e8&&a<0x200000&&n<=0x200000-a)||(a>=0x800000&&a<0x900000&&n<=0x900000-a)||(a>=0xa0b8e8&&a<0xc00000&&n<=0xc00000-a)||(a>=0xffdc00&&a<0xfff000&&n<=0xfff000-a));}
private:
 enum class LookupOwner{SelectionPart,SelectionVoice,PopulationVoice,PopulationPart};enum class DspOwner{Selection,PopulationSelect,PopulationPitch,Gate};
 static unsigned byteClock(uint32_t at,int previous){return at<0x400000&&!(previous&1)&&at==(uint32_t(previous)|1u)?0:3;}
 void choose(Component c){component_=c;components_|=1u<<unsigned(c);}
 void beginPopulation(){population_.emplace();choose(Component::Population);}
 void finishSelection(){if(selection_->complete())beginPopulation();else choose(Component::Selection);}
 void startLookup(LookupOwner owner,PartLookupTask::Layout layout){lookupOwner_=owner;lookup_.emplace(layout);choose(Component::Lookup);}
 void startDsp(DspOwner owner,DspRoutingTask::Kind routing,uint16_t address,uint32_t word){dspOwner_=owner;dsp_.emplace(ControllerDspService::Kind::Routing,ControllerDspService::Input{routing,summary_.port,address,word});choose(Component::Dsp);}
 uint32_t populationTarget(VoicePopulationTask::Write q)const{using W=VoicePopulationTask::Write;const auto slot=0x17560u+20*population_->slot(),part=0x174aeu+20*population_->controlPart();switch(q){case W::PatchReference:return slot;case W::Note:return slot+5;case W::Transposed:return slot+13;case W::Held:return part+17;case W::Unison:return slot+12;case W::Velocity:return slot+6;case W::Age:return slot+8;case W::Part:return slot+4;case W::Extra:return slot+18;case W::Distance:return slot+7;case W::Pending:return slot+17;}throw std::logic_error("Unknown slot population target");}
 Kind kind_;Input input_;Summary summary_{};Effect effect_{};bool complete_=false;Component component_=Component::Selection;uint32_t components_=0;LookupOwner lookupOwner_=LookupOwner::SelectionPart;DspOwner dspOwner_=DspOwner::Selection;
 std::optional<VoiceSelectionTask>selection_;std::optional<VoicePopulationTask>population_;std::optional<PartLookupTask>lookup_;std::optional<PartMirrorTask>mirror_;std::optional<VelocityCurveTask>velocity_;std::optional<ControllerDspService>dsp_,lastSelect_,lastPitch_;std::optional<VoiceGateOffTask>gate_;
};
}
