#pragma once
#include "ksMidiFileTransportCoordinator.h"
#include "ksMidiFileGlobalsService.h"
#include "ksStatePlaybackModeService.h"
#include "ksStateDisplayService.h"
#include "ksStateReloadService.h"
#include "ksStateFieldService.h"
#include "ksMidiFileTimeService.h"
#include "ksMidiFileEventService.h"
#include "ksNativeArpOwnedService.h"
#include "ksVoiceFeedbackService.h"
#include "ksPanelMasksService.h"
#include "ksPartFeedbackService.h"
#include "ksStateTickService.h"
#include "ksStateEventsService.h"
#include "ksStateResetService.h"
#include "ksSerialForegroundService.h"
#include "ksStateProgramLoadService.h"
#include "ksStateCentralActionsBindings.h"
#include "ksMidiFileTransportDomainBindings.h"
#include <variant>
namespace ks {
// A reusable fixed work slot owns every real file-player child. Completed
// semantic facts update only the three resource operands consumed downstream.
// Optional void notifications are external observations, never graph inputs.
class MidiFileTransportService final {
public:
 using Coordinator=MidiFileTransportCoordinator;using Input=Coordinator::Input;using Result=Coordinator::Result;using Request=Coordinator::Request;using Stage=Coordinator::Stage;using Effect=Coordinator::Effect;
 enum class Notice{Policy,Dispatch,ChildPhase,ChildComplete,Complete};
 struct Noop{template<class...T>void operator()(T&&...)const{}};
 using Engines=std::variant<std::monostate,MidiFileGlobalsService,StatePlaybackModeService,StateDisplayService,StateReloadService,StateFieldService,MidiFileTimeService,MidiFileEventService,NativeArpOwnedService,VoiceFeedbackService,PanelMasksService,PartFeedbackService,StateTickService,StateEventsService,StateResetService,SerialForegroundService,StatePlaybackModeCoordinator,StateProgramLoadService>;
 MidiFileTransportService():MidiFileTransportService(Input{}){}explicit MidiFileTransportService(Input i):coordinator_(i){}
 const Coordinator&coordinator()const{return coordinator_;}const Input&input()const{return coordinator_.input();}const Result&result()const{return coordinator_.result();}const auto&phaseObservation()const{return coordinator_.phaseObservation();}const Engines&engines()const{return engines_;}Request child()const{return child_;}bool complete()const{return coordinator_.complete()&&!active_;}const Effect&effect()const{return effect_;}uint32_t components()const{return components_;}
 unsigned priority()const{if(!active_)return 0;return std::visit([](const auto&t)->unsigned{using T=std::decay_t<decltype(t)>;if constexpr(std::is_same_v<T,MidiFileEventService>||std::is_same_v<T,NativeArpOwnedService>)return t.priority();else if constexpr(std::is_same_v<T,std::monostate>||std::is_same_v<T,StateResetService>||std::is_same_v<T,StateEventsService>||std::is_same_v<T,MidiFileGlobalsService>||std::is_same_v<T,StateDisplayService>)return 0;else return t.critical()?7:0;},engines_);}bool critical()const{return priority()==7;}
 template<class T>const T*current()const{if constexpr(isEngine<T>())return std::get_if<T>(&engines_);else return nullptr;}
 template<class T>const T*task()const{if(complete())return nullptr;if constexpr(std::is_same_v<T,MidiFileTransportService>)return this;else if constexpr(std::is_same_v<T,Coordinator>)return active_?nullptr:&coordinator_;else if constexpr(std::is_same_v<T,MidiFileTransportTask>)return active_?nullptr:coordinator_.template task<T>();else if constexpr(isEngine<T>())return active_?current<T>():nullptr;else if constexpr(std::is_same_v<T,DspFaultTask>)return currentFault();else return nullptr;}
 unsigned nextClocks(int previous=-1,int previousRead=-1)const{if(complete())return 0;if(!active_)return coordinator_.nextClocks(previous,previousRead);return std::visit([&](const auto&t)->unsigned{using T=std::decay_t<decltype(t)>;if constexpr(std::is_same_v<T,std::monostate>)return 0;else if constexpr(std::is_same_v<T,SerialForegroundService>||std::is_same_v<T,StateResetService>)return t.nextClocks();else if constexpr(std::is_same_v<T,MidiFileEventService>||std::is_same_v<T,NativeArpOwnedService>||std::is_same_v<T,MidiFileTimeService>)return t.nextClocks(previous,previousRead);else return t.nextClocks(previous);},engines_);}
 template<class R,class W,class Resolver>unsigned advanceBefore(unsigned b,R&&r,W&&w,Resolver&&resolve,int previous=-1,int previousRead=-1){return advanceBefore(b,r,w,resolve,Noop{},previous,previousRead);}
 template<class R,class W,class Resolver,class Observer,std::enable_if_t<!std::is_integral_v<std::decay_t<Observer>>,int> =0>unsigned advanceBefore(unsigned budget,R&&read,W&&write,Resolver&&resolve,Observer&&observe,int previous=-1,int previousRead=-1){const auto cost=nextClocks(previous,previousRead);if(!cost||cost>budget)return 0;effect_={};const auto notify=[&](Notice n){static_assert(std::is_void_v<decltype(observe(n,static_cast<const MidiFileTransportService&>(*this)))>);observe(n,static_cast<const MidiFileTransportService&>(*this));};
 if(!active_){const auto stage=coordinator_.policy().stage();coordinator_.advanceBefore(cost,read,write,previous,previousRead);effect_=coordinator_.effect();static_assert(std::is_void_v<decltype(observe(Notice::Policy,stage,static_cast<const MidiFileTransportService&>(*this)))>);observe(Notice::Policy,stage,static_cast<const MidiFileTransportService&>(*this));if(coordinator_.request()!=Request::None){dispatch();notify(Notice::Dispatch);}if(complete())notify(Notice::Complete);return cost;}
 bool done=false;Coordinator::ChildResult outcome;const auto before=context();
 std::visit([&](auto&t){using T=std::decay_t<decltype(t)>;
  if constexpr(std::is_same_v<T,std::monostate>)throw std::logic_error("Missing owned file-player child");
  else{
   if constexpr(std::is_same_v<T,StateDisplayService>){effect_={t.endsReadTransfer(),t.endsWriteTransfer()};t.advanceBefore(cost,read,write,previous);}
   else if constexpr(std::is_same_v<T,StateEventsService>){eventsEffect(t);t.advanceBefore(cost,read,write,previous);}
   else if constexpr(std::is_same_v<T,StateResetService>){resetEffect(t);t.advanceBefore(cost,read,write);}
   else if constexpr(std::is_same_v<T,SerialForegroundService>){serialEffect(t);t.advanceBefore(cost,read,write);}
   else if constexpr(std::is_same_v<T,MidiFileEventService>||std::is_same_v<T,NativeArpOwnedService>)t.advanceBefore(cost,read,write,resolve,observe,previous,previousRead);
   else if constexpr(std::is_same_v<T,StatePlaybackModeService>||std::is_same_v<T,StateFieldService>||std::is_same_v<T,StatePlaybackModeCoordinator>||std::is_same_v<T,StateTickService>)t.advanceBefore(cost,read,write,resolve,previous);
   else if constexpr(std::is_same_v<T,StateReloadService>||std::is_same_v<T,StateProgramLoadService>)t.advanceBefore(cost,read,write,[&](uint8_t category,uint32_t raw){return resolve.schema(category,raw);},previous);
   else if constexpr(std::is_same_v<T,MidiFileTimeService>)t.advanceBefore(cost,read,write,previous,previousRead);
   else t.advanceBefore(cost,read,write,previous);
   if constexpr(!std::is_same_v<T,StateEventsService>&&!std::is_same_v<T,StateResetService>&&!std::is_same_v<T,SerialForegroundService>&&!std::is_same_v<T,StateDisplayService>)effect_={t.effect().read,t.effect().write};
   done=t.complete();if(done)outcome=completed(t,before);
  }
 },engines_);
 notify(Notice::ChildPhase);if(done){notify(Notice::ChildComplete);if(!coordinator_.resume(outcome))throw std::logic_error("File-player child return ownership");active_=false;}return cost;
 }
 template<class C>C domainAfter(C prior)const{if(!complete())throw std::logic_error("Incomplete file-player functional outcome");prior.expressionSource=result().expressionSource;prior.activation=result().activation;prior.presentation=result().presentation;return prior;}
 uint32_t expressionSourceAfter(uint32_t)const{if(!complete())throw std::logic_error("Incomplete file-player source");return result().expressionSource;}uint8_t activationAfter(uint8_t)const{if(!complete())throw std::logic_error("Incomplete file-player activation");return result().activation;}uint16_t presentationAfter(uint16_t)const{if(!complete())throw std::logic_error("Incomplete file-player presentation");return result().presentation;}
private:
 struct Context{uint16_t presentation;uint8_t activation;uint32_t expressionSource;};Context context()const{return{result().presentation,result().activation,result().expressionSource};}
 template<class T>static constexpr bool isEngine(){return std::is_same_v<T,MidiFileGlobalsService>||std::is_same_v<T,StatePlaybackModeService>||std::is_same_v<T,StateDisplayService>||std::is_same_v<T,StateReloadService>||std::is_same_v<T,StateFieldService>||std::is_same_v<T,MidiFileTimeService>||std::is_same_v<T,MidiFileEventService>||std::is_same_v<T,NativeArpOwnedService>||std::is_same_v<T,VoiceFeedbackService>||std::is_same_v<T,PanelMasksService>||std::is_same_v<T,PartFeedbackService>||std::is_same_v<T,StateTickService>||std::is_same_v<T,StateEventsService>||std::is_same_v<T,StateResetService>||std::is_same_v<T,SerialForegroundService>||std::is_same_v<T,StatePlaybackModeCoordinator>||std::is_same_v<T,StateProgramLoadService>;}
 const DspFaultTask*currentFault()const{if(!active_)return nullptr;return std::visit([](const auto&t)->const DspFaultTask*{using T=std::decay_t<decltype(t)>;if constexpr(std::is_same_v<T,StateReloadService>){if(t.component()==StateReloadService::Component::Voices)return t.voices()->faultTask();if(t.component()==StateReloadService::Component::Signal)return t.signal()->faultTask();return nullptr;}else if constexpr(std::is_same_v<T,PartFeedbackService>){const auto*d=t.template task<ControllerDspService>();return d?d->template task<DspFaultTask>():nullptr;}else if constexpr(std::is_same_v<T,VoiceFeedbackService>||std::is_same_v<T,StateTickService>||std::is_same_v<T,StateProgramLoadService>||std::is_same_v<T,MidiFileEventService>||std::is_same_v<T,NativeArpOwnedService>)return t.template task<DspFaultTask>();else return nullptr;},engines_);}
 template<class T,class...A>void start(A&&...a){engines_.template emplace<T>(std::forward<A>(a)...);active_=true;components_|=1u<<unsigned(child_);}
 void dispatch(){child_=coordinator_.request();const auto&i=coordinator_.childInput();switch(child_){
 case Request::SaveMulti:start<MidiFileGlobalsService>(MidiFileGlobalsService::Kind::BackupFactory);break;
 case Request::Silence:start<StatePlaybackModeService>(StatePlaybackModeService::Kind::ResetVoices,StatePlaybackModeService::Input{i.presentation});break;
 case Request::OutputSetup:{StateDisplayService::Input in;in.character=i.presentation;start<StateDisplayService>(StateDisplayService::Kind::Character,in);break;}
 case Request::LoadMulti:start<StateReloadService>();break;
 case Request::Refresh:start<StateFieldService>(StateUiRefreshService::Kind::Refresh,StateUiRefreshService::Input{0,i.presentation,i.presentation});break;
 case Request::Tempo:start<MidiFileTimeService>(MidiFileTimeService::Kind::Tempo,MidiFileTimeService::Input{i.stream,i.tempo,0});break;
 case Request::Delay:start<MidiFileTimeService>(MidiFileTimeService::Kind::Delay,MidiFileTimeService::Input{i.stream,0,0});break;
 case Request::Due:start<MidiFileTimeService>(MidiFileTimeService::Kind::Due,MidiFileTimeService::Input{i.stream,0,0});break;
 case Request::Event:start<MidiFileEventService>(MidiFileEventService::Kind::Event,MidiFileEventService::Input{i.stream,0,i.activation,0});break;
 case Request::Arp:start<NativeArpOwnedService>(NativeArpOwnedService::Kind::Schedule,NativeArpOwnedService::Input{i.expressionSource,i.activation});break;
 case Request::Voices:start<VoiceFeedbackService>();break;case Request::Panel:start<PanelMasksService>();break;case Request::Midi:start<PartFeedbackService>();break;case Request::Clock:start<StateTickService>();break;
 case Request::PollKey:start<StateEventsService>(StateEventsService::Kind::ButtonPop);break;case Request::PollControl:start<StateEventsService>(StateEventsService::Kind::ControlPop);break;
 case Request::ReleaseKey:{StateFieldService::Input in;in.event=i.presentation;start<StateFieldService>(StateFieldService::Kind::Velocity,in);break;}
 case Request::RestoreMultiState:start<MidiFileGlobalsService>(MidiFileGlobalsService::Kind::Restore);break;case Request::RestoreOutputs:start<StateResetService>();break;
 case Request::Flush:start<SerialForegroundService>(SerialForegroundService::Kind::ResetRx);break;
 case Request::RestoreState:start<StatePlaybackModeCoordinator>(StatePlaybackModeCoordinator::Kind::Routing,StatePlaybackModeCoordinator::Input{i.presentation});break;
 case Request::LoadPatch:start<StateProgramLoadService>(StateProgramLoadService::Kind::LoadSingle,StateProgramLoadService::Input{i.presentation});break;
 case Request::None:throw std::logic_error("Missing file-player request");}}
 template<class T>Coordinator::ChildResult completed(const T&t,Context c)const{Coordinator::ChildResult r;const auto setTransfer=[&](const StateUiDomainTransfer&d){c=midiFileTransportDomain::uiTransfer(d,c);};
 if constexpr(std::is_same_v<T,StatePlaybackModeService>){setTransfer(centralActionsDomainBindings::modeNode(t,0));c.presentation=t.coordinator().presentation();if(t.ui())c.presentation=t.ui()->projectionView().lastRefresh()->character;for(unsigned n=0;n<24;++n)if(const auto*p=t.resetOutcome(n)){if(p->kind()==StatePlaybackResetService::Kind::Panic)c.presentation&=0xff00;else if(p->kind()==StatePlaybackResetService::Kind::ReleaseVoices&&p->voices()->releasedAny())c.presentation=uint16_t(p->voices()->lastPatch());}}
 else if constexpr(std::is_same_v<T,StateDisplayService>)c.presentation=t.output();
 else if constexpr(std::is_same_v<T,StateReloadService>){if(!t.panel())throw std::logic_error("Missing file-player reload panel outcome");c.expressionSource=0x60010e;c.activation=uint8_t(t.panel()->bounds()>>8);c.presentation=uint16_t((t.panel()->panelTask().value()<<8)|21u);}
 else if constexpr(std::is_same_v<T,StateFieldService>){if(child_==Request::Refresh)setTransfer(stateUiRefreshDomainTransfer(t));else if(child_==Request::ReleaseKey)setTransfer(stateFieldDomainTransfer(t));}
 else if constexpr(std::is_same_v<T,MidiFileTimeService>){r.stream=t.summary().stream;r.due=t.summary().due;if(t.kind()==MidiFileTimeService::Kind::Delay)c.expressionSource=t.summary().value;}
 else if constexpr(std::is_same_v<T,MidiFileEventService>){const auto&v=t.result();r.stream=v.stream;c.expressionSource=(c.expressionSource&~v.expressionSourceMask)|(v.expressionSource&v.expressionSourceMask);c.activation=v.activation;}
 else if constexpr(std::is_same_v<T,NativeArpOwnedService>){c.expressionSource=t.expressionSourceAfter(c.expressionSource);c.activation=t.activationAfter(c.activation);}
 else if constexpr(std::is_same_v<T,VoiceFeedbackService>)c=midiFileTransportDomain::voiceFeedback(t,c);
 else if constexpr(std::is_same_v<T,PanelMasksService>)c=midiFileTransportDomain::masks(t,c);
 else if constexpr(std::is_same_v<T,PartFeedbackService>)c=midiFileTransportDomain::partFeedback(t,c);
 else if constexpr(std::is_same_v<T,StateTickService>)c=midiFileTransportDomain::tick(t,c);
 else if constexpr(std::is_same_v<T,StateEventsService>){c.presentation=t.event();r.key=uint8_t(t.event()>>8);r.control=uint8_t(t.event());r.available=t.available();}
 else if constexpr(std::is_same_v<T,StateProgramLoadService>)setTransfer(stateUiDomainBindings::program(t));
 r.presentationKnown=!std::is_same_v<T,MidiFileEventService>&&!std::is_same_v<T,NativeArpOwnedService>;r.presentation=c.presentation;r.activationKnown=true;r.activation=c.activation;r.expressionSourceMask=~0u;r.expressionSource=c.expressionSource;return r;}
 void eventsEffect(const StateEventsService&s){using P=PanelInputTask::Stage;using A=PanelAnalogTask::Stage;if(const auto*t=s.panelTask())switch(t->stage()){case P::SavePop:case P::SavePopAux:case P::ConvertControl:effect_.write=true;break;case P::RestoreEmpty:case P::RestorePopAux:case P::RestorePop:case P::ReturnEmpty:case P::ReturnPop:effect_.read=true;break;default:break;}if(const auto*t=s.analogTask())switch(t->stage()){case A::RetainSpan:case A::SaveFirstAddress:case A::SaveFirstBias:case A::SaveSecondAddress:case A::SaveSecondBias:case A::SaveValue:case A::SaveSquare:case A::ScaleFirst:case A::ScaleSecond:effect_.write=true;break;case A::RestoreSpan:case A::RestoreFirstBias:case A::RestoreFirstAddress:case A::RestoreSecondBias:case A::RestoreSecondAddress:case A::RestoreValue:case A::RestoreSquare:case A::ReturnCoefficients:case A::ReturnRange:case A::ReturnUnchanged:case A::ReturnFirst:case A::ReturnSecond:case A::ReturnValue:case A::ReturnSquare:effect_.read=true;break;default:break;}}
 void resetEffect(const StateResetService&s){if(const auto*t=s.stateTask()){using P=StateForegroundTask::Stage;effect_.write=t->stage()==P::ResetButtons||t->stage()==P::ResetControls||t->stage()==P::ResetNotes;effect_.read=t->stage()==P::ReturnReset;}if(const auto*t=s.panelTask()){using P=PanelInputTask::Stage;effect_.write=t->stage()==P::SaveReset;effect_.read=t->stage()==P::RestoreReset||t->stage()==P::ReturnReset;}if(const auto*t=s.keyboardTask()){using P=KeyboardEventsTask::Stage;effect_.write=t->stage()==P::SaveResetEvent;effect_.read=t->stage()==P::RestoreResetEvent||t->stage()==P::ReturnReset;}}
 void serialEffect(const SerialForegroundService&s){using Q=SerialQueueTask::Stage;effect_.write=s.atQueue(Q::RetainCritical)||s.atQueue(Q::RetainWord);effect_.read=s.atQueue(Q::RestoreWord)||s.atQueue(Q::RestoreCritical)||s.atQueue(Q::Finish);}
 Coordinator coordinator_;Engines engines_{};Request child_=Request::None;bool active_=false;Effect effect_{};uint32_t components_=0;
};
}
