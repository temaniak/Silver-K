#pragma once
#include "ksStateStatusDisplayTask.h"
#include "ksStateEditWorkflowTask.h"
#include "ksStatePresetAddressTask.h"
#include "ksStateProgramTask.h"
#include "ksScheduledTimerCoordinator.h"
#include "ksPanelMonitorService.h"
#include "ksControllerDspService.h"
#include <array>
#include <optional>
#include <type_traits>
namespace ks {
// The whole periodic status chain: no CPU ABI is consulted by any descendant.
class StateStatusService final {
public:
 using Kind=StateStatusDisplayTask::Kind;
 enum class Component{Status,Gate,Preset,Program,Monitor,Lcd,Dsp,Timer};
 enum class Route{None,Busy,Special,Clock,Bar,Part};
 struct Input{uint16_t presentation=0;};struct Effect{bool read=false,write=false;};
 struct Frame{StateStatusDisplayTask task;Kind kind;uint16_t initialPresentation=0,outputPresentation=0;std::array<uint8_t,4>busyBytes{};uint8_t special=0,screen=0,gate=0,mode=0,attempts=0;uint16_t raw=0;uint32_t source=0,target=0;unsigned compared=0,child=NoChild;Route route=Route::None;bool meter=false,changed=false,active=false,single=false;explicit Frame(Kind k,uint16_t p):task(k),kind(k),initialPresentation(p){}};
 static constexpr unsigned NoChild=~0u;
 explicit StateStatusService(Kind k):StateStatusService(k,Input{}){}
 StateStatusService(Kind k,Input i):kind_(k),input_(i),presentation_(i.presentation){push(k);}
 Kind kind()const{return kind_;}const Input&input()const{return input_;}Component component()const{return component_;}uint32_t components()const{return components_;}bool complete()const{return !depth_;}bool critical()const{return false;}const Effect&effect()const{return effect_;}uint16_t presentation()const{return presentation_;}const Frame&outcome(unsigned n=0)const{if(n>=used_)throw std::logic_error("Unknown status outcome");return *frames_[n];}
 const PanelMonitorService*monitor()const{return monitor_?&*monitor_:nullptr;}const ControllerDspService*dsp()const{return dsp_?&*dsp_:nullptr;}const ScheduledTimerCoordinator*timer()const{return timer_?&*timer_:nullptr;}const PanelSettingsLcdService*lcd()const{return lcd_?&*lcd_:nullptr;}
 template<class T>const T*task()const{if constexpr(std::is_same_v<T,StateStatusService>)return this;else if constexpr(std::is_same_v<T,StateStatusDisplayTask>)return component_==Component::Status&&depth_?&current().task:nullptr;else if constexpr(std::is_same_v<T,StateEditWorkflowTask>)return component_==Component::Gate?&*gate_:nullptr;else if constexpr(std::is_same_v<T,StatePresetAddressTask>)return component_==Component::Preset?&*preset_:nullptr;else if constexpr(std::is_same_v<T,StateProgramTask>)return component_==Component::Program?&*program_:nullptr;else if constexpr(std::is_same_v<T,PanelMonitorService>)return component_==Component::Monitor?monitor():nullptr;else if constexpr(std::is_same_v<T,PanelSettingsLcdService>)return component_==Component::Lcd?lcd():nullptr;else if constexpr(std::is_same_v<T,ControllerDspService>)return component_==Component::Dsp?dsp():nullptr;else if constexpr(std::is_same_v<T,ScheduledTimerCoordinator>)return component_==Component::Timer?timer():nullptr;else {if(component_==Component::Monitor)return monitor_->template task<T>();if(component_==Component::Dsp)return dsp_->template task<T>();if(component_==Component::Timer)return timer_->template task<T>();return nullptr;}}
 unsigned nextClocks(int previous=-1)const{if(complete())return 0;switch(component_){case Component::Status:return current().task.nextClocks(statusCost(current().task,previous));case Component::Gate:return gate_->nextClocks();case Component::Preset:return preset_->nextClocks();case Component::Program:return program_->nextClocks();case Component::Monitor:return monitor_->nextClocks(previous);case Component::Lcd:return lcd_->nextClocks(previous);case Component::Dsp:return dsp_->nextClocks(previous);case Component::Timer:return timer_->nextClocks(previous);}return 0;}
 template<class R,class W>unsigned advanceBefore(unsigned budget,R&&read,W&&write,int previous=-1){const auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;effect_={};
 const auto byte=[&](uint32_t a,unsigned back=0){return uint8_t(read(a&0xffffff,cost-back));};
 const auto readN=[&](uint32_t a,unsigned n){uint32_t v=0;for(unsigned i=0;i<n;++i)v=(v<<8)|byte(a+i,3*((n-1-i)/2));return v;};
 const auto put=[&](uint32_t a,uint32_t v,unsigned n){for(unsigned i=0;i<n;++i)write((a+i)&0xffffff,uint8_t(v>>(8*(n-1-i))),cost-3*((n-1-i)/2));};
 auto&f=current();
 switch(component_){
 case Component::Gate:gate_->advanceBefore(cost,[&](auto,uint32_t){return f.gate=byte(0x16784);},[](auto,auto){});if(gate_->complete()){effect_.read=true;f.task.resumeChild(bool(f.gate),0,0);select(Component::Status);}break;
 case Component::Preset:{using T=StatePresetAddressTask;const auto stage=preset_->stage();preset_->advanceBefore(cost,[&](T::Read q){return byte(q==T::Read::Bank?0x167ac:0x167ad);},[](auto,auto){throw std::logic_error("Unexpected current-preset write");});if(preset_->awaitingIndex()){effect_.write=true;program_.emplace(StateProgramTask::Kind::ReadIndex);select(Component::Program);}else if(preset_->complete()){effect_.read=true;f.source=preset_->address();f.task.resumeChild(false,0,f.source);select(Component::Status);}(void)stage;break;}
 case Component::Program:{using T=StateProgramTask;using S=T::Stage;const auto stage=program_->stage();program_->advanceBefore(cost,[&](T::Read q){return byte(q==T::Read::Bank?0x167ac:0x167ad,stage==S::ReadBank?2:0);},[](auto,auto){throw std::logic_error("Unexpected program-index write");});effect_.write=stage==S::RetainBank||stage==S::RetainExtra;effect_.read=stage==S::RestoreExtra||stage==S::RestoreBank||stage==S::ReturnIndex;if(program_->complete()){preset_->resumeIndex(program_->index());select(Component::Preset);}break;}
 case Component::Monitor:monitor_->advanceBefore(cost,read,write,previous);effect_={monitor_->effect().read,monitor_->effect().write};if(monitor_->complete()){presentation_=monitor_->presentation();f.task.resumeChild(false,0,0);select(Component::Status);}break;
 case Component::Lcd:lcd_->advanceBefore(cost,read,write,previous);effect_={lcd_->effect().read,lcd_->effect().write};if(lcd_->complete()){presentation_=lcd_->output();f.task.resumeChild(false,0,0);select(Component::Status);}break;
 case Component::Dsp:dsp_->advanceBefore(cost,read,write,previous);effect_={dsp_->effect().read,dsp_->effect().write};if(dsp_->complete()){f.raw=uint16_t(dsp_->summary().reply>>8);f.task.resumeChild(false,f.raw,0);select(Component::Status);}break;
 case Component::Timer:timer_->advanceBefore(cost,read,write,previous);effect_={timer_->effect().read,timer_->effect().write};if(timer_->awaitingAction())throw std::logic_error("Status arm cannot dispatch a delayed action");if(timer_->complete()){presentation_=uint16_t(timer_->summary().counter);f.task.resumeChild(false,0,0);select(Component::Status);}break;
 case Component::Status:{using T=StateStatusDisplayTask;using S=T::Stage;const auto stage=f.task.stage();
 f.task.advanceBefore(cost,[&](T::Read q,uint32_t a)->uint32_t{
  using Q=T::Read;switch(q){case Q::Patch:{const auto v=readN(0x167bc,4);if(!dataSpan(v,209))throw std::logic_error("Unsupported evolving status patch span");if(stage==S::ReadStandardPatch||stage==S::ReadSplitPatch||stage==S::ReadFxPatch)f.target=v;return v;}
  case Q::Program:return readN(0x167ac,2);case Q::PartProgram:return readN(a,2);case Q::Byte:if(!readSpan(a,1))throw std::logic_error("Unsupported status comparison source");return byte(a,stage==S::ReadHeaderSource||stage==S::ReadHeaderTarget||stage==S::ReadTailSource||stage==S::ReadTailTarget?0:2);
  case Q::Notice:return f.busyBytes[0]=byte(0x16791);case Q::Page:return f.busyBytes[1]=byte(0x16790);case Q::Latch:return f.busyBytes[2]=byte(0x16792);case Q::Ui:return f.busyBytes[3]=byte(0x1679f);case Q::Special:return f.special=byte(0x1679e);case Q::SpecialCharacter:return byte(0x17494);case Q::Screen:return f.screen=byte(0x16785);case Q::EditPage:return byte(0x16787);case Q::Mode:return f.mode=byte(0x167ba);case Q::ScanPart:return byte(0x167ee);case Q::Mask:return byte(0x167d3);case Q::Marker:return byte(0x167d6);}
  throw std::logic_error("Unknown status observation");},[&](T::Write q,uint32_t v){put(q==T::Write::ScanPart?0x167ee:q==T::Write::Program?0x167ac:0x167bc,v,q==T::Write::ScanPart?1:q==T::Write::Program?2:4);},statusCost(f.task,previous));
 switch(stage){case S::SelectBusy:if(f.task.busy())f.route=Route::Busy;break;case S::BindSinglePosition:f.single=true;setByte(0x90);break;case S::BindSingleMarker:case S::BindInactiveMarker:setByte(32);break;case S::BindScanAttempts:f.attempts=3;break;case S::TickScanAttempts:--f.attempts;break;case S::BiasScanPosition:presentation_=uint16_t((5*f.task.index()&0xff00)|uint8_t(0xc0+5*f.task.index()));break;case S::ReadSpecialCharacter:case S::ReadInactivePartMarker:case S::ReadCompareMarker:setByte(uint8_t(f.task.value()));break;case S::BindChangedMarker:f.changed=true;setByte('*');break;case S::ReadSavedProgram:f.active=true;break;case S::ReadStandardSource:case S::ReadPrefixSource:case S::ReadHeaderSource:case S::ReadSuffixSource:case S::ReadFxSource:case S::ReadTailSource:++f.compared;break;case S::BindMeterValue:setByte(uint8_t(f.task.value()));break;case S::BindFastPeriod:presentation_=30;break;case S::BindNormalPeriod:presentation_=100;break;default:break;}
 switch(stage){case S::SaveSavedProgram:case S::SaveSavedPatch:effect_.write=true;break;case S::RestoreSavedPatch:case S::RestoreSavedProgram:case S::ReturnStatus:case S::ReturnInactive:case S::ReturnActive:case S::ReturnCompare:effect_.read=true;break;default:break;}
 if(f.task.awaitingChild()){effect_.write=true;switch(stage){
 case S::ReadStatusGate:gate_.emplace(StateEditWorkflowTask::Kind::EditActive);select(Component::Gate);break;
 case S::ShowPartStatus:f.route=Route::Part;push(Kind::Part);break;case S::CompareScanPatch:push(Kind::Compare);break;
 case S::ShowMeter:f.route=Route::Clock;startMonitor(PanelMonitorService::Kind::Clock);break;
 case S::ShowMeterValue:f.route=Route::Bar;startMonitor(PanelMonitorService::Kind::Bar);break;
 case S::ReadMeterInput:{f.meter=true;ControllerDspService::Input i;i.port=DspPort::A;i.address=0x8059;dsp_.emplace(ControllerDspService::Kind::Read,i);select(Component::Dsp);break;}
 case S::ScheduleStatus:timer_.emplace(ScheduledTimerCoordinator::Kind::Arm,2,ScheduledTimerCoordinator::Input{presentation_,ScheduledTimerCoordinator::Action::StatusTick});select(Component::Timer);break;
 case S::ResolveStandardPatch:case S::ResolveSplitPatch:case S::ResolveFxPatch:preset_.emplace(StatePresetAddressTask::Kind::Current);select(Component::Preset);break;
 case S::ShowSpecialCharacter:f.route=Route::Special;startLcd();break;
 case S::ShowSinglePosition:case S::ShowScanPosition:case S::ShowInactiveMarker:case S::ShowInactivePartMarker:case S::ShowChangedMarker:case S::ShowCompareMarker:startLcd();break;
 default:throw std::logic_error("Unclosed status child");}}
 else if(f.task.complete()){f.outputPresentation=presentation_;--depth_;if(depth_){current().task.resumeChild(false,0,0);select(Component::Status);}}break;}
 }
 return cost;}
 static bool dataSpan(uint32_t a,unsigned n){return StateDisplayService::dataSpan(a,n);}static bool readSpan(uint32_t raw,unsigned n){const auto a=raw&0xffffff;return dataSpan(raw,n)||(n&&a>=0x800000&&a<0x900000&&n<=0x900000-a);}
private:
 Frame&current(){return *frames_[active_[depth_-1]];}const Frame&current()const{return *frames_[active_[depth_-1]];}
 void select(Component c){component_=c;components_|=1u<<unsigned(c);}void setByte(uint8_t v){presentation_=uint16_t((presentation_&0xff00)|v);}
 void push(Kind k){if(used_==frames_.size()||depth_==active_.size())throw std::logic_error("Status graph depth exceeded");const auto n=used_++;if(depth_)current().child=n;frames_[n].emplace(k,presentation_);active_[depth_++]=n;select(Component::Status);}
 void startLcd(){lcd_.emplace(PanelSettingsLcdService::Kind::Character,PanelSettingsLcdService::Input{0,presentation_,0});select(Component::Lcd);}
 void startMonitor(PanelMonitorService::Kind k){PanelMonitorService::Input i;i.presentation=presentation_;i.level=uint8_t(presentation_);monitor_.emplace(k,i);select(Component::Monitor);}
 static unsigned byteClock(uint32_t a,int p){a&=0xffffff;return !(p&1)&&a==(uint32_t(p)|1u)?0:3;}
 static unsigned statusCost(const StateStatusDisplayTask&t,int previous){using S=StateStatusDisplayTask::Stage;switch(t.stage()){
 case S::ReadSavedPatch:case S::ReadStandardPatch:case S::ReadSplitPatch:case S::ReadFxPatch:return 6;
 case S::WriteScanPatch:case S::WriteSavedPatch:return 6;
 case S::WriteScanPart:return byteClock(0x167ee,previous);case S::WriteScanProgram:case S::WriteSavedProgram:return byteClock(0x167ac,previous);default:return 3;}}
 Kind kind_;Input input_;Component component_=Component::Status;uint32_t components_=0;Effect effect_{};uint16_t presentation_=0;unsigned depth_=0,used_=0;std::array<unsigned,3>active_{};std::array<std::optional<Frame>,3>frames_{};
 std::optional<StateEditWorkflowTask>gate_;std::optional<StatePresetAddressTask>preset_;std::optional<StateProgramTask>program_;std::optional<PanelMonitorService>monitor_;std::optional<PanelSettingsLcdService>lcd_;std::optional<ControllerDspService>dsp_;std::optional<ScheduledTimerCoordinator>timer_;
};
}
