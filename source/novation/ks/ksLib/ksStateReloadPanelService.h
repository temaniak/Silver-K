#pragma once
#include "ksStatePanelRefreshTask.h"
#include "ksStateEditWorkflowTask.h"
#include "ksStateFieldActionsTask.h"
#include "ksStateEditAddressTask.h"
#include "ksStateValueBoundsTask.h"
#include "ksStateStoreService.h"
#include "ksPanelControlTask.h"
#include "ksVoiceTrackingTask.h"
#include "ksDspRoutingTask.h"
#include "ksDspAddressTask.h"
#include "ksDspWordTask.h"
#include "ksDspFaultTask.h"
#include <array>
#include <optional>
#include <stdexcept>
namespace ks {
// Complete authenticated normal-firmware panel refresh. The eleven descriptor
// schemas and zero requests bound this graph to parameter observation, indicator
// updates and the auxiliary DSP-B packet. Numeric handler data is observed but
// never used as a continuation or a code dispatcher. Call admittedSchema at the
// external boundary before ownership; descriptor tables remain immutable.
class StateReloadPanelService final {
public:
 enum class Component {Panel,Workflow,Field,Address,Bounds,Indicator,Auxiliary,DspRouting,DspAddress,DspWord,DspFault};
 struct Effect{bool read=false,write=false;};
 struct Schema{uint8_t key;std::array<uint8_t,8> bytes;};
 inline static constexpr std::array<Schema,11> schema{{
 {1,{{0,0,222,128,129,130,22,1}}},{14,{{0,0,225,131,132,0,22,14}}},
 {38,{{2,2,255,222,0,0,0,0}}},{39,{{4,3,222,6,0,2,4,4}}},
 {40,{{2,2,255,224,0,0,0,5}}},{41,{{2,1,255,139,0,0,0,7}}},
 {42,{{2,2,255,223,0,0,0,8}}},{43,{{3,1,255,211,0,0,0,9}}},
 {44,{{2,2,255,140,0,0,0,15}}},{45,{{2,1,255,225,0,0,0,19}}},
 {46,{{6,1,225,79,0,3,0,18}}}}};
 template<class Peek,class HandlerValidator>static bool admittedSchema(Peek&&peek,HandlerValidator&&valid){
  for(const auto&s:schema){for(unsigned n=0;n<8;++n)if(uint8_t(peek(0xfd66+8*s.key+n))!=s.bytes[n])return false;
   uint32_t raw=0;for(unsigned n=0;n<4;++n)raw=(raw<<8)|uint8_t(peek(0x1013e + 4*s.bytes[0]+n));if(!valid(s.bytes[0],raw))return false;}
  return true;
 }
 Component component()const{return component_;}uint32_t components()const{return components_;}bool complete()const{return panel_.complete();}const Effect& effect()const{return effect_;}
 uint32_t current()const{return current_;}uint32_t pointer()const{return pointer_;}uint32_t descriptor()const{return descriptor_;}uint16_t bounds()const{return boundsValue_;}
 uint32_t lastResolvedBase()const{return resolvedBase_;}uint32_t lastBitSelectorReference()const{return bitSelectorReference_;}
 uint8_t fieldKey()const{return key_;}bool automatic()const{return automatic_;}uint8_t lastValue()const{return childValue_;}
 bool auxiliarySent()const{return auxiliarySent_;}uint8_t auxiliaryValue()const{return auxiliaryValue_;}
 const StatePanelRefreshTask& panelTask()const{return panel_;}const StateEditWorkflowTask* workflowTask()const{return workflow_?&*workflow_:nullptr;}
 const StateFieldActionsTask* fieldTask()const{return fieldCount_?&*fields_[fieldCount_-1]:nullptr;}
 const StateEditAddressTask* addressTask()const{return address_?&*address_:nullptr;}const StateValueBoundsTask* boundsTask()const{return bounds_?&*bounds_:nullptr;}
 const PanelControlTask* indicatorTask()const{return indicator_?&*indicator_:nullptr;}
 unsigned nextClocks(int previous=-1)const{
  if(complete())return 0;
  switch(component_){case Component::Panel:return panel_.nextClocks();case Component::Workflow:return workflow_->nextClocks();
  case Component::Field:return fieldTask()->nextClocks(fieldStore(previous));case Component::Address:return address_->nextClocks();
  case Component::Bounds:return bounds_->nextClocks(boundsStore());case Component::Indicator:return indicator_->nextClocks([&](PanelControlTask::Write w){return byteClock(indicatorWrite(w),previous);});
  case Component::Auxiliary:return auxiliary_->complete()?17:auxiliary_->nextClocks();case Component::DspRouting:return routing_->nextClocks();
  case Component::DspAddress:return dspAddress_->complete()?17:dspAddress_->nextBoundaryClocks();case Component::DspWord:return dspWord_->complete()?17:dspWord_->nextBoundaryClocks();
  case Component::DspFault:return fault_->complete()?17:fault_->nextBoundaryClocks();}return 0;
 }
 template<class Reader,class Writer,class RecipeResolver>unsigned advanceBefore(unsigned available,Reader&&read,Writer&&write,RecipeResolver&&validateHandler,int previous=-1){
  const auto cost=nextClocks(previous);if(!cost||cost>available)return 0;effect_={};
  const auto longValue=[&](uint32_t at){uint32_t v=0;for(unsigned n=0;n<4;++n)v=(v<<8)|uint8_t(read((at+n)&0xffffff,cost-3*((3-n)/2)));return v;};
  const auto dataBase=[&](uint32_t at){const auto value=longValue(at);if(!StateStoreService::dataSpan(value,256))throw std::logic_error("Unsupported evolving panel data base");return value;};
  const auto dataValue=[&](uint32_t at){if(!StateStoreService::dataSpan(at,1))throw std::logic_error("Unsupported evolving panel field reference");return uint8_t(read(at&0xffffff,cost));};
  switch(component_){
  case Component::Panel:{using T=StatePanelRefreshTask;using S=T::Stage;const auto stage=panel_.stage();panel_.advanceBefore(cost,[&](T::Read f)->uint32_t{
   if(f==T::Read::Current)return current_=dataBase(0x167bc);
   const auto at=f==T::Read::OscillatorMode?current_+226:f==T::Read::LfoMode||f==T::Read::FilterMode?current_+88:f==T::Read::MaskFirst?0x167d2:f==T::Read::MaskSecond?0x167d3:f==T::Read::PartMode?0x167ba:f==T::Read::PageMode?0x16784:f==T::Read::EditMode?0x1679e:f==T::Read::GlobalMode?0x167a1:f==T::Read::AlternateMode?0x167a3:0x16790;return dataValue(at);
   });effect_.write=stage==S::RetainCount;effect_.read=stage==S::RestoreCount||stage==S::Return;
   if(panel_.awaitingChild()){effect_.write=true;if(panel_.child()==T::Child::Indicator)startIndicator(panel_.key(),panel_.value(),Component::Panel);
    else {key_=panel_.key();automatic_=panel_.child()==T::Child::Special;workflow_.emplace(automatic_?StateEditWorkflowTask::Kind::AutomaticEvent:StateEditWorkflowTask::Kind::ManualEdit,key_);select(Component::Workflow);}}
   break;}
  case Component::Workflow:{using T=StateEditWorkflowTask;using S=T::Stage;const auto stage=workflow_->stage();workflow_->advanceBefore(cost,[&](T::Read f,uint32_t at)->uint32_t{
    if(f==T::Read::Byte){const auto c=descriptorRead(at,uint8_t(read(at&0xffffff,cost)));if(stage==S::ReadDescriptorCategory)category_=c;return c;}if(f==T::Read::Handler){const auto raw=longValue(at);if(!validateHandler(category_,raw))throw std::logic_error("Unsupported mutated panel handler binding");return 0;}throw std::logic_error("Bounded panel workflow requested dynamic variant");
   },[&](T::Write,uint32_t value){for(unsigned n=0;n<4;++n)write(0x167a6+n,uint8_t(value>>(24-8*n)),cost-3*((3-n)/2));});
   effect_.write=stage==S::SaveAutomaticEvent||stage==S::SaveManualEvent||stage==S::DispatchEditHandler;effect_.read=stage==S::RestoreEditEvent||stage==S::ReturnEdit;
   if(workflow_->awaitingChild()){descriptor_=0xfd66+workflow_->offset();const auto expected=expectedSchema(key_);if(!expected||category_!=expected->bytes[0])throw std::logic_error("Unsupported mutated panel descriptor category");
    using K=StateFieldActionsTask::Kind;pushField(category_==0?K::Indexed:category_==2?K::DirectStep:category_==3?K::IndexedStep:category_==4?K::Pair:K::Bit,0,0);}
   else if(workflow_->complete()){workflow_.reset();panel_.resumeChild();select(Component::Panel);}break;}
  case Component::Field:{using T=StateFieldActionsTask;using S=T::Stage;auto&t=*fields_[fieldCount_-1];const auto stage=t.stage();t.advanceBefore(cost,[&](T::Read f,uint32_t at)->uint32_t{
    if(f==T::Read::Patch)return dataBase(0x167bc);if(f==T::Read::Context)return dataBase(0x167a6);using A=StateFieldActionsTask::Stage;if(stage==A::ReadCurrentValue||stage==A::ReadBitIndex||stage==A::ReadBitValue||stage==A::ReadPairIndex||stage==A::ReadPairValue||stage==A::ReadCycleValue)return dataValue(at);const auto value=descriptorRead(at,uint8_t(read(at&0xffffff,cost)));
    if(stage==S::ReadValueType&&value!=22)throw std::logic_error("Unsupported mutated panel value type");return value;
   },[&](T::Write f,uint32_t at,uint32_t value){if(f==T::Write::Byte)write(at&0xffffff,uint8_t(value),cost);else throw std::logic_error("Unsupported panel edit publication");},fieldStore(previous));
   if(stage==S::LocateBitSelector){bitSelectorReference_=t.patch()+t.selector();if(!StateStoreService::dataSpan(bitSelectorReference_,1))throw std::logic_error("Unsupported evolving panel selector reference");}
   switch(stage){case S::SaveIndexedEvent:case S::SavePairRequest:case S::SavePairValue:effect_.write=true;break;case S::RestoreZeroEvent:case S::RestorePairValue:case S::RestorePairRequest:case S::ReturnZeroValue:case S::ReturnDirect:case S::ReturnIndexed:case S::ReturnPair:case S::ReturnCycleValue:case S::ReturnCycle:effect_.read=true;break;default:break;}
   if(t.awaitingChild()){effect_.write=true;switch(stage){
    case S::DirectAddress:case S::ResolveBitAddress:case S::ResolvePairAddress:address_.emplace(StateEditAddressTask::Kind::Direct,descriptor_);select(Component::Address);break;
    case S::IndexedAddress:case S::ResolveIndexedField:address_.emplace(StateEditAddressTask::Kind::Indexed,descriptor_);select(Component::Address);break;
    case S::StepDirect:case S::StepIndexed:pushField(T::Kind::Current,pointer_,0);break;
    case S::CycleCurrentValue:case S::CyclePair:pushField(T::Kind::Cycle,pointer_,uint16_t(t.value()<<8));break;
    case S::DisplayZeroValue:pushField(T::Kind::Control,pointer_,uint16_t(t.value()<<8));break;
    case S::ResolveValueBounds:bounds_.emplace(StateValueBoundsTask::Kind::Resolve,uint8_t(t.type()),uint8_t(t.value()),automatic_,pointer_);select(Component::Bounds);break;
    case S::ApplyDirectSideEffect:auxiliaryValue_=childValue_;auxiliary_.emplace(VoiceTrackingTask::Kind::Auxiliary);select(Component::Auxiliary);break;
    default:throw std::logic_error("Unsupported nonzero panel edit request");}}
   else if(t.complete()){if(t.exit()==T::Exit::Display){startIndicator(uint8_t(t.control()),uint8_t(t.value()),Component::Field);}
    else if(t.exit()!=T::Exit::None)throw std::logic_error("Unsupported panel action tail");else finishField(uint8_t(t.value()));}
   break;}
  case Component::Address:{using T=StateEditAddressTask;address_->advanceBefore(cost,[&](T::Read f)->uint32_t{switch(f){case T::Read::IndexBase:case T::Read::TargetBase:case T::Read::DirectBase:return resolvedBase_=dataBase(0x167bc);case T::Read::Selector:return descriptorRead(descriptor_+2,uint8_t(read((descriptor_+2)&0xffffff,cost)));case T::Read::Index:return dataValue(address_->selectorAddress());case T::Read::MappedOffset:return descriptorRead(address_->mappingAddress(),uint8_t(read(address_->mappingAddress()&0xffffff,cost)));case T::Read::DirectOffset:case T::Read::GlobalOffset:return descriptorRead(descriptor_+3,uint8_t(read((descriptor_+3)&0xffffff,cost)));}return 0;});
   if(address_->complete()){effect_.read=true;pointer_=address_->target();if(!StateStoreService::dataSpan(pointer_,1))throw std::logic_error("Unsupported evolving panel resolved field");address_.reset();fields_[fieldCount_-1]->resumeChild(pointer_,0,0,false);select(Component::Field);}break;}
  case Component::Bounds:{using T=StateValueBoundsTask;bounds_->advanceBefore(cost,[](T::Read,uint32_t){throw std::logic_error("Unsupported panel bounds read");return uint32_t(0);},[](auto,auto) {},boundsStore());
   if(bounds_->complete()){effect_.read=true;pointer_=bounds_->pointer();boundsValue_=uint16_t(bounds_->bounds());const auto v=uint8_t(bounds_->value());bounds_.reset();fields_[fieldCount_-1]->resumeChild(pointer_,v,boundsValue_,false);select(Component::Field);}break;}
  case Component::Indicator:{using T=PanelControlTask;using S=T::Stage;const auto stage=indicator_->stage();indicator_->advanceBefore(cost,[&](T::Read f){
    if(f==T::Read::Selector)return indicatorKey_;if(f==T::Read::Input)return indicatorValue_;const auto at=f==T::Read::Offset?0x1c0+indicator_->tableOffset():f==T::Read::Limit?0x1c3+indicator_->tableOffset():f==T::Read::Mode?0x1c4+indicator_->tableOffset():f==T::Read::Mirror?0xffec1c+indicator_->offset():f==T::Read::Mask?0x1c2+indicator_->tableOffset():f==T::Read::Set?0x1c1+indicator_->tableOffset():0xffff62;return uint8_t(read(at,cost-(f==T::Read::Port?1:0)));
   },[&](T::Write f,uint8_t v){write(indicatorWrite(f),v,cost);},[&](T::Write f){return byteClock(indicatorWrite(f),previous);});
   switch(stage){case S::RetainFirst:case S::RetainSecond:case S::RetainThird:case S::RetainFourth:case S::RetainInput:case S::HoldDelay:effect_.write=true;break;case S::RestoreDelay:case S::RestoreInput:case S::RestoreFourth:case S::RestoreThird:case S::RestoreSecond:case S::RestoreFirst:case S::Finish:effect_.read=true;break;default:break;}
   if(indicator_->stage()==S::Complete){indicator_.reset();if(indicatorOwner_==Component::Panel){panel_.resumeChild();select(Component::Panel);}else finishField(indicatorValue_);}break;}
  case Component::Auxiliary:if(auxiliary_->complete()){effect_.read=true;auxiliary_.reset();fields_[fieldCount_-1]->resumeChild(pointer_,childValue_,0,false);select(Component::Field);}else {using T=VoiceTrackingTask;auxiliary_->advanceBefore(cost,[&](T::Read){return auxiliaryValue_;},[](T::Write,uint8_t){},[](T::Write){return 3;});if(auxiliary_->child()==T::Child::Absolute){effect_.write=true;auxiliarySent_=true;routing_.emplace(DspRoutingTask::Kind::WriteB,0x8045);select(Component::DspRouting);}}break;
  case Component::DspRouting:{using T=DspRoutingTask;const auto stage=routing_->stage();routing_->advanceBefore(cost,[&](T::Read){return uint16_t((unsigned(read(0x159f2,cost))<<8)|read(0x159f3,cost));},[](T::Write,uint16_t){});if(stage==T::Stage::DispatchWrite)effect_.write=true;if(routing_->complete()){dspAddress_.emplace(0x8045);select(Component::DspAddress);}break;}
  case Component::DspAddress:if(dspAddress_->complete()){effect_.read=true;dspAddress_.reset();dspWord_.emplace(auxiliaryValue_,true,0x8045);select(Component::DspWord);}else {using T=DspAddressTask;if(dspAddress_->stage()==T::Stage::DispatchFault)effect_.write=true;dspAddress_->advanceBefore(cost,[&](unsigned offset){return read(0x600000+offset,cost);},[&](unsigned offset,uint8_t value){write(0x600000+offset,value,cost);});if(dspAddress_->stage()==T::Stage::AwaitFault){faultFromWord_=false;fault_.emplace();select(Component::DspFault);}}break;
  case Component::DspWord:if(dspWord_->complete()){effect_.read=true;dspWord_.reset();auxiliary_->resumeChild();select(Component::Auxiliary);}else {using T=DspWordTask;if(dspWord_->stage()==T::Stage::DispatchFault)effect_.write=true;dspWord_->advanceBefore(cost,[&]{return read(0x600004,cost);},[&](unsigned offset,uint8_t value){write(0x600000+offset,value,cost);},[&](uint16_t v){write(0x159f2,uint8_t(v>>8),cost);write(0x159f3,uint8_t(v),cost);});if(dspWord_->stage()==T::Stage::AwaitFault){faultFromWord_=true;fault_.emplace();select(Component::DspFault);}}break;
  case Component::DspFault:if(fault_->complete()){effect_.read=true;fault_.reset();if(faultFromWord_){dspWord_->resumeFault();select(Component::DspWord);}else {dspAddress_->resumeFault();select(Component::DspAddress);}}else {using T=DspFaultTask;const auto stage=fault_->stage();effect_.write=stage==T::Stage::Admission||stage==T::Stage::LatchAdmission||stage==T::Stage::DelayAdmission;effect_.read=stage==T::Stage::DelayRelease||stage==T::Stage::LatchRelease||stage==T::Stage::CompletionDelay;fault_->advanceBefore(cost,[&](T::Read f){return read(f==T::Read::Latch?0xffec22:0xffff62,cost-(f==T::Read::Control?1:0));},[&](T::Write f,uint8_t v){write(f==T::Write::Latch?0xffec22:f==T::Write::Data?0xffff61:0xffff62,v,cost);});}break;
  }
  return cost;
 }
private:
 static uint8_t descriptorRead(uint32_t at,uint8_t value){for(const auto&s:schema){const uint32_t begin=0xfd66+8*s.key;if(at>=begin&&at<begin+8&&value!=s.bytes[at-begin])throw std::logic_error("Unsupported mutated panel descriptor byte");}return value;}
 static const Schema*expectedSchema(uint8_t key){for(const auto&s:schema)if(s.key==key)return &s;return nullptr;}
 static unsigned byteClock(uint32_t a,int previous){a&=0xffffff;if(a<0x400000)return !(previous&1)&&a==(uint32_t(previous)|1)?0:3;if(a>=0xfffd10&&a<0xffffa0)return !(previous&1)&&a==(uint32_t(previous)|1)?0:1;return a>=0xffffa0?1:3;}
 unsigned fieldStore(int previous)const{using S=StateFieldActionsTask::Stage;switch(fieldTask()->stage()){case S::ReadBitPatch:case S::ReadPairPatch:case S::ReadDirectPatch:return 6;default:return 3;}(void)previous;}
 unsigned boundsStore()const{return 3;}
 uint32_t indicatorWrite(PanelControlTask::Write f)const{return f==PanelControlTask::Write::Mirror?0xffec1c+indicator_->offset():f==PanelControlTask::Write::Data?0xffff61:0xffff62;}
 void select(Component c){component_=c;components_|=1u<<unsigned(c);}
 void pushField(StateFieldActionsTask::Kind kind,uint32_t ptr,uint16_t event){if(fieldCount_==fields_.size())throw std::logic_error("Panel field ownership capacity");fields_[fieldCount_++].emplace(kind,descriptor_,ptr,event,uint16_t(key_|(automatic_?0x100:0)),boundsValue_,0);select(Component::Field);}
 void finishField(uint8_t value){childValue_=value;fields_[--fieldCount_].reset();if(fieldCount_){fields_[fieldCount_-1]->resumeChild(pointer_,value,boundsValue_,false);select(Component::Field);}else {workflow_->resumeChild(false,0);select(Component::Workflow);}}
 void startIndicator(uint8_t key,uint8_t value,Component owner){indicatorKey_=key;indicatorValue_=value;indicatorOwner_=owner;indicator_.emplace();select(Component::Indicator);}
 StatePanelRefreshTask panel_;std::optional<StateEditWorkflowTask>workflow_;std::array<std::optional<StateFieldActionsTask>,3>fields_{};unsigned fieldCount_=0;
 std::optional<StateEditAddressTask>address_;std::optional<StateValueBoundsTask>bounds_;std::optional<PanelControlTask>indicator_;std::optional<VoiceTrackingTask>auxiliary_;std::optional<DspRoutingTask>routing_;std::optional<DspAddressTask>dspAddress_;std::optional<DspWordTask>dspWord_;std::optional<DspFaultTask>fault_;
 Component component_=Component::Panel,indicatorOwner_=Component::Panel;uint32_t components_=1,current_=0,descriptor_=0,pointer_=0,resolvedBase_=0,bitSelectorReference_=0;uint16_t boundsValue_=0;uint8_t key_=0,category_=0,childValue_=0,indicatorKey_=0,indicatorValue_=0,auxiliaryValue_=0;bool automatic_=false,auxiliarySent_=false,faultFromWord_=false;Effect effect_{};
};
}
