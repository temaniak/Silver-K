#pragma once
#include "ksStateSingleLoadTask.h"
#include "ksStatePresetAddressTask.h"
#include "ksStateProgramTask.h"
#include "ksStateReloadCoordinator.h"
#include "ksStateReloadVoiceService.h"
#include "ksStateReloadPanelService.h"
#include "ksStateStoreService.h"
#include <optional>
#include <stdexcept>
namespace ks {
// Complete single-program byte load and observer refresh. Source/target values
// are logical addresses latched only at their actual resource read phase.
class StateProgramLoadDataService final {
public:
 enum class Component {Single,Preset,Program,Invalidate,AdcReset,Expand,Semitone,Enable,Lookup,Keys,Signal,Panel};
 struct Effect {bool read=false,write=false;};
 struct Summary {uint32_t source=0,target=0,overlaySource=0,overlayTarget=0,lateTarget=0,expanded=0,expandedTarget=0;uint8_t mode=0,category=0,part=0,lastByte=0,menu=0;bool overlay=false;};
 explicit StateProgramLoadDataService(uint32_t source):source_(source){summary_.source=source;if(!readSpan(source,209))throw std::logic_error("Unsupported single-load source");}
 Component component()const{return component_;}bool complete()const{return single_.complete();}bool critical()const{return false;}const Effect&effect()const{return effect_;}const Summary&summary()const{return summary_;}uint32_t components()const{return components_;}
 const StateSingleLoadTask&single()const{return single_;}const StateReloadPanelService*panel()const{return panel_?&*panel_:nullptr;}const StateReloadVoiceService*signal()const{return signal_?&*signal_:nullptr;}
 template<class T>const T*task()const{if constexpr(std::is_same_v<T,StateSingleLoadTask>)return component_==Component::Single?&single_:nullptr;else if constexpr(std::is_same_v<T,StatePresetAddressTask>)return component_==Component::Preset?&*preset_:nullptr;else if constexpr(std::is_same_v<T,StateProgramTask>)return component_==Component::Program?&*program_:nullptr;else if constexpr(std::is_same_v<T,StateGlobalsTask>)return component_==Component::Invalidate?&*invalid_:nullptr;else if constexpr(std::is_same_v<T,StateParameterExpandTask>)return component_==Component::Expand?&*expand_:nullptr;else if constexpr(std::is_same_v<T,PartEnableTask>)return component_==Component::Enable?&*enable_:nullptr;else if constexpr(std::is_same_v<T,PanelAdcScanTask>)return component_==Component::AdcReset?&*adc_:nullptr;else if constexpr(std::is_same_v<T,StatePanelModesTask>)return component_==Component::Semitone?&*semitone_:nullptr;else if constexpr(std::is_same_v<T,PartLookupTask>)return component_==Component::Lookup?&*lookup_:nullptr;else if constexpr(std::is_same_v<T,StateReloadPanelService>)return component_==Component::Panel?&*panel_:nullptr;else if constexpr(std::is_same_v<T,StateReloadVoiceService>)return component_==Component::Signal?&*signal_:nullptr;else return nullptr;}
 static bool dataSpan(uint32_t a,unsigned n){return StateStoreService::dataSpan(a,n);}static bool readSpan(uint32_t raw,unsigned n){const auto a=raw&0xffffff;return dataSpan(raw,n)||(n&&a>=0x800000&&a<0x900000&&n<=0x900000-a);}
 unsigned nextClocks(int previousWrite=-1)const{if(complete())return 0;switch(component_){
 case Component::Single:return single_.nextClocks(byteClock(singleWrite(),previousWrite));case Component::Preset:return preset_->nextClocks(byteClock(0x167ad,previousWrite));case Component::Program:return program_->nextClocks();
 case Component::Invalidate:return invalid_->nextClocks(byteClock(invalidBase_+invalid_->byteOffset(),previousWrite));case Component::AdcReset:return adc_->nextClocks();case Component::Expand:return expand_->nextClocks(byteClock(expandTarget_+expand_->targetOffset(),previousWrite));case Component::Semitone:return semitone_->nextClocks(byteClock(semitone_->stage()==StatePanelModesTask::Stage::WriteOldSemitone?0x1678e:0x1678d,previousWrite));case Component::Enable:return enable_->nextClocks([&](PartEnableTask::Write w){return byteClock(enableWrite(w),previousWrite);});case Component::Lookup:return lookup_->stage()==PartLookupTask::Stage::Complete?17:lookup_->nextClocks();case Component::Keys:return 0;case Component::Signal:return signal_->nextClocks(previousWrite);case Component::Panel:return panel_->nextClocks(previousWrite);}return 0;}
 template<class Reader,class Writer,class Resolver>unsigned advanceBefore(unsigned available,Reader&&read,Writer&&write,Resolver&&resolver,int previousWrite=-1){const auto cost=nextClocks(previousWrite);if(!cost||cost>available)return 0;effect_={};
 const auto read32=[&](uint32_t a){uint32_t v=0;for(unsigned n=0;n<4;++n)v=(v<<8)|uint8_t(read((a+n)&0xffffff,cost-3*((3-n)/2)));return v;};
 switch(component_){
 case Component::Single:{using T=StateSingleLoadTask;using S=T::Stage;const auto stage=single_.stage();single_.advanceBefore(cost,[&](T::Read q)->uint32_t{switch(q){case T::Read::Target:return summary_.target=target_=checkedData(read32(0x167bc),209);case T::Read::Mode:return summary_.mode=uint8_t(read(0x167ba,cost));case T::Read::Byte:return summary_.lastByte=uint8_t(read((source_+single_.byteOffset())&0xffffff,cost-2));case T::Read::OverlayTarget:return summary_.overlayTarget=overlayTarget_=checkedData(read32(0x167bc),127);case T::Read::OverlayByte:return summary_.lastByte=uint8_t(read((overlay_+84+single_.overlayOffset())&0xffffff,cost-2));case T::Read::OverlayExtra:return summary_.lastByte=uint8_t(read((overlay_+126)&0xffffff,cost));case T::Read::LateTarget:return summary_.lateTarget=lateTarget_=checkedData(read32(0x167bc),177);case T::Read::Category:return summary_.category=uint8_t(read((lateTarget_+176)&0xffffff,cost));case T::Read::Part:return summary_.part=uint8_t(read(0x16798,cost));}throw std::logic_error("Unexpected single load observation");},[&](T::Write,uint8_t v){write(singleWrite()&0xffffff,v,cost);},byteClock(singleWrite(),previousWrite));effect_.read=stage==S::ReturnLoad;
 if(single_.awaitingChild()){effect_.write=true;switch(single_.child()){case T::Child::Overlay:preset_.emplace(StatePresetAddressTask::Kind::Overlay);select(Component::Preset);break;case T::Child::InvalidateLive:invalid_.emplace(StateGlobalsTask::Kind::InvalidateLive);select(Component::Invalidate);break;case T::Child::ExpandFields:expand_.emplace(StateParameterExpandTask::Entry::Refresh);select(Component::Expand);break;case T::Child::RefreshPart:enablePart_=single_.part();enable_.emplace();select(Component::Enable);break;case T::Child::RefreshPerformance:signal_.emplace(StateReloadVoiceService::Kind::Signal);select(Component::Signal);break;case T::Child::Panel:panel_.emplace();select(Component::Panel);break;}}break;}
 case Component::Preset:{using T=StatePresetAddressTask;using S=T::Stage;const auto stage=preset_->stage();preset_->advanceBefore(cost,[&](T::Read q){return read(q==T::Read::Bank?0x167ac:0x167ad,cost);},[&](T::Write,uint8_t v){write(0x167ad,v,cost);},byteClock(0x167ad,previousWrite));effect_.write=stage==S::RetainProgram||stage==S::ReadAliasIndex;effect_.read=stage==S::RestoreUnavailableProgram||stage==S::RestoreProgram||stage==S::ReturnUnavailable||stage==S::ReturnAlias;
 if(preset_->awaitingIndex()){program_.emplace(StateProgramTask::Kind::ReadIndex);select(Component::Program);}else if(preset_->complete()){summary_.overlay=preset_->available();summary_.overlaySource=overlay_=preset_->address();single_.resumeChild(summary_.overlay);preset_.reset();select(Component::Single);}break;}
 case Component::Program:{using T=StateProgramTask;using S=T::Stage;const auto stage=program_->stage();program_->advanceBefore(cost,[&](T::Read q){return read(q==T::Read::Bank?0x167ac:0x167ad,stage==S::ReadBank?cost-2:cost);},[](auto,auto){throw std::logic_error("Program read attempted publication");});effect_.write=stage==S::RetainBank||stage==S::RetainExtra;effect_.read=stage==S::RestoreExtra||stage==S::RestoreBank||stage==S::ReturnIndex;if(program_->complete()){preset_->resumeIndex(program_->index());program_.reset();select(Component::Preset);}break;}
  case Component::Invalidate:{using T=StateGlobalsTask;
   invalid_->advanceBefore(cost,[&](T::Read){return invalidBase_=checkedData(read32(0x167c0),255);},[&](T::Write,uint8_t v){write((invalidBase_+invalid_->byteOffset())&0xffffff,v,cost);},byteClock(invalidBase_+invalid_->byteOffset(),previousWrite));
   if(invalid_->awaitingInvalid()){effect_.write=true;adc_.emplace(PanelAdcScanTask::Kind::ResetLast);select(Component::AdcReset);}
   else if(invalid_->complete()){effect_.read=true;invalid_.reset();single_.resumeChild();select(Component::Single);}
   break;}
  case Component::AdcReset:{using T=PanelAdcScanTask;adc_->advanceBefore(cost,[](T::Read){return 0u;},[&](T::Write,uint8_t v){write(0x156e4,v,cost);});
   if(adc_->complete()){effect_.read=true;adc_.reset();invalid_->resumeInvalid();select(Component::Invalidate);}break;}
  case Component::Expand:{using T=StateParameterExpandTask;using S=T::Stage;const auto stage=expand_->stage();
   expand_->advanceBefore(cost,[&](T::Read r)->uint32_t{if(r==T::Read::CurrentBase)return expandSource_=checkedData(read32(0x167bc),228);if(r==T::Read::LiveBase)return expandTarget_=checkedData(read32(0x167c0),228);if(r==T::Read::Mode)return read(0x167ba,cost);return read((expandSource_+expand_->sourceOffset())&0xffffff,cost);},[&](unsigned offset,uint8_t v){write((expandTarget_+offset)&0xffffff,v,cost);},byteClock(expandTarget_+expand_->targetOffset(),previousWrite));
   summary_.expanded=expandSource_;summary_.expandedTarget=expandTarget_;if(stage==S::CaptureCurrent)expandTarget_=expandSource_;
   effect_.write=stage==S::DispatchFirst||stage==S::RetainRequest||stage==S::DispatchSigned;
   effect_.read=stage==S::RestoreRequest||stage==S::ReturnExpansion;
   if(expand_->awaitingSigned()){semitoneOwner_=Component::Expand;semitone_.emplace(StatePanelModesTask::Kind::Semitone,uint16_t(expand_->signedMode())<<8);select(Component::Semitone);}
   else if(expand_->complete()){expand_.reset();single_.resumeChild();select(Component::Single);}
   break;}
  case Component::Semitone:{using T=StatePanelModesTask;semitone_->advanceBefore(cost,[&](T::Read r,uint32_t){return read(r==T::Read::Type?0x158f2:0x1678d,cost);},[&](T::Write r,uint32_t,uint32_t v){write(r==T::Write::PreviousSemitone?0x1678e:0x1678d,uint8_t(v),cost);},byteClock(semitone_->stage()==T::Stage::WriteOldSemitone?0x1678e:0x1678d,previousWrite));
   if(semitone_->complete()){effect_.read=true;semitone_.reset();if(semitoneOwner_==Component::Expand){expand_->resumeSigned();select(Component::Expand);}else throw std::logic_error("Unexpected semitone parent");}break;}
  case Component::Enable:{using T=PartEnableTask;const auto stage=enable_->stage();enable_->advanceBefore(cost,[&](T::Read r){return r==T::Read::Input?enablePart_:uint8_t(read(0x174ae + 20*enable_->part()+18,cost));},[&](T::Write r,uint8_t v){write(enableWrite(r)&0xffffff,v,cost);},[&](T::Write r){return byteClock(enableWrite(r),previousWrite);});
   if(enable_->awaiting()){effect_.write=true;lookupOwner_=Component::Enable;lookup_.emplace(PartLookupTask::Layout::Controls);select(Component::Lookup);}
   else if(stage==T::Stage::Finish){effect_.read=true;enable_.reset();single_.resumeChild();select(Component::Single);}break;}
  case Component::Lookup:{using T=PartLookupTask;
   if(lookup_->stage()==T::Stage::Complete){effect_.read=true;const auto part=lookup_->index();lookup_.reset();
    if(lookupOwner_==Component::Enable){enable_->resumeChild(part);select(Component::Enable);}
    else if(lookupOwner_==Component::Keys)throw std::logic_error("Unexpected key lookup");
    else throw std::logic_error("Unexpected lookup owner");
   }else lookup_->advanceBefore(cost,[&](T::Layout){return read(0x17526,cost);});
   break;}

 case Component::Signal:signal_->advanceBefore(cost,read,write,previousWrite);effect_={signal_->effect().read,signal_->effect().write};if(signal_->complete()){single_.resumeChild();select(Component::Single);}break;
 case Component::Panel:panel_->advanceBefore(cost,[&](uint32_t a,unsigned dt){const auto v=read(a,dt);if(panel_->component()==StateReloadPanelService::Component::Panel&&(a&0xffffff)==0x16790)summary_.menu=uint8_t(v);return v;},write,[&](uint8_t c,uint32_t raw){return resolver(c,raw);},previousWrite);effect_={panel_->effect().read,panel_->effect().write};if(panel_->complete()){single_.resumeChild();select(Component::Single);}break;
 case Component::Keys:throw std::logic_error("Unused single-load key path");
 }return cost;}
private:
 static uint32_t checkedData(uint32_t a,unsigned n){if(!dataSpan(a,n))throw std::logic_error("Unsupported evolving single-load data span");return a;}
 static unsigned byteClock(uint32_t a,int previous){a&=0xffffff;return a<0x400000&&!(previous&1)&&a==(uint32_t(previous)|1u)?0u:3u;}
 void select(Component c){component_=c;components_|=1u<<unsigned(c);}
 uint32_t singleWrite()const{using S=StateSingleLoadTask::Stage;switch(single_.stage()){case S::WriteByte:return target_+single_.byteOffset();case S::WriteOverlayByte:return overlayTarget_+84+single_.overlayOffset();case S::WriteOverlayExtra:return overlayTarget_+126;case S::WriteCategory:return 0x167da;case S::WriteGate:return 0x167a3;case S::WriteBank:return 0x1679e;default:return 0x167a0;}}
 uint32_t enableWrite(PartEnableTask::Write w)const{return w==PartEnableTask::Write::Part?0x17526:0x174ae +20*enable_->part()+(w==PartEnableTask::Write::Enable?17:18);}
 StateSingleLoadTask single_;Summary summary_{};Component component_=Component::Single,semitoneOwner_=Component::Expand,lookupOwner_=Component::Enable;Effect effect_{};uint32_t components_=1;
 uint32_t source_=0,target_=0,overlay_=0,overlayTarget_=0,lateTarget_=0,invalidBase_=0,expandSource_=0,expandTarget_=0;uint8_t enablePart_=0;
 std::optional<StatePresetAddressTask>preset_;std::optional<StateProgramTask>program_;std::optional<StateGlobalsTask>invalid_;std::optional<PanelAdcScanTask>adc_;std::optional<StateParameterExpandTask>expand_;std::optional<StatePanelModesTask>semitone_;std::optional<PartEnableTask>enable_;std::optional<PartLookupTask>lookup_;std::optional<StateReloadVoiceService>signal_;std::optional<StateReloadPanelService>panel_;
};}
