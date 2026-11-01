#pragma once
#include "ksServiceCycleCoordinator.h"
#include "ksStateTickService.h"
#include "ksStateCentralActionsService.h"
#include "ksStateChannelService.h"
#include "ksStatePanelService.h"
#include "ksVoiceFeedbackService.h"
#include "ksPanelMasksService.h"
#include "ksPartFeedbackService.h"
#include "ksDiagnosticTransferService.h"
#include <array>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <utility>
namespace ks {
class NativeControllerExecutor;
struct NativeCompletionNoop final {template<class...T>void operator()(T&&...)const noexcept{}};
// One fixed-order main iteration. Engine parameters are concrete native child
// owners, not opcode/call dispatchers. Only one child occupies the reusable
// fixed storage. Ordinary queue length never determines storage capacity.
// The optional void observer is an external compatibility sink: it cannot
// supply a result or a decision to the native graph. Stage3 removes that sink.
template<class TimerEngine,class MidiEngine,class ArpEngine,class TransportEngine>
class ServiceCycleService final {
public:
using Coordinator=ServiceCycleCoordinator;using Input=Coordinator::Input;using Result=Coordinator::Result;using Child=Coordinator::Child;
enum class Notice{Dispatch,ChildPhase,ChildComplete,ExternalBegin,ExternalComplete,Complete};enum class External{None,Transport,Diagnostic};
struct Effect{bool read=false,write=false;};
using Engines=std::variant<std::monostate,TimerEngine,StateTickService,StateCentralActionsService,StateChannelService,StatePanelService,MidiEngine,VoiceFeedbackService,ArpEngine,PanelMasksService,PartFeedbackService>;
using ExternalEngines=std::variant<std::monostate,TransportEngine,DiagnosticTransferService>;
struct ExternalFacts{Result before{},after{};External kind=External::None;bool complete=false;};
ServiceCycleService():ServiceCycleService(Input{}){}explicit ServiceCycleService(Input i):coordinator_(i){}
bool complete()const{return coordinator_.complete();}const Coordinator&coordinator()const{return coordinator_;}const Input&input()const{return coordinator_.input();}const Result&result()const{return coordinator_.result();}const Effect&effect()const{return effect_;}Child child()const{return child_;}unsigned childIndex()const{return index_;}External external()const{return external_;}const Engines&engines()const{return engine_;}const ExternalEngines&externalEngines()const{return externalEngine_;}const ExternalFacts&externalFacts(unsigned n)const{if(n>=externals_.size())throw std::logic_error("Main external outcome index");return externals_[n];}
template<class T>const T*current()const{if constexpr((std::is_same_v<T,TimerEngine>||std::is_same_v<T,StateTickService>||std::is_same_v<T,StateCentralActionsService>||std::is_same_v<T,StateChannelService>||std::is_same_v<T,StatePanelService>||std::is_same_v<T,MidiEngine>||std::is_same_v<T,VoiceFeedbackService>||std::is_same_v<T,ArpEngine>||std::is_same_v<T,PanelMasksService>||std::is_same_v<T,PartFeedbackService>))return std::get_if<T>(&engine_);else if constexpr(std::is_same_v<T,TransportEngine>||std::is_same_v<T,DiagnosticTransferService>)return std::get_if<T>(&externalEngine_);else return nullptr;}
// Read-only selectors expose the active semantic task for diagnostics only.
template<class T>const T*task()const{if(complete())return nullptr;if constexpr(std::is_same_v<T,ServiceCycleService>)return this;else if constexpr(std::is_same_v<T,Coordinator>)return !active_?&coordinator_:nullptr;else if constexpr(std::is_same_v<T,ServiceCycleTask>)return !active_?coordinator_.template task<T>():nullptr;else{const auto selected=[](const auto&s)->const T*{using S=std::decay_t<decltype(s)>;if constexpr(std::is_same_v<S,std::monostate>)return nullptr;else{if(s.complete())return nullptr;if constexpr(std::is_same_v<T,S>)return &s;else if constexpr(std::is_same_v<T,DspFaultTask>){if constexpr(std::is_same_v<S,PartFeedbackService>){const auto*d=s.template task<ControllerDspService>();const auto*f=d&&!d->complete()?d->template task<T>():nullptr;return f&&!f->complete()?f:nullptr;}else{const auto*f=query<T>(s,0);return f&&!f->complete()?f:nullptr;}}else return nullptr;}};return external_!=External::None?std::visit(selected,externalEngine_):std::visit(selected,engine_);}}
unsigned priority()const{if(external_!=External::None)return std::visit([](const auto&s){return priorityOf(s);},externalEngine_);return std::visit([](const auto&s){return priorityOf(s);},engine_);}bool critical()const{return priority()==7;}
unsigned nextClocks(int previous=-1,int previousRead=-1)const{if(complete())return 0;if(external_!=External::None)return std::visit([&](const auto&s){return clocks(s,previous,previousRead);},externalEngine_);if(!active_)return coordinator_.nextClocks(previous);return std::visit([&](const auto&s){return clocks(s,previous,previousRead);},engine_);}
template<class R,class W,class Resolver,class Bindings>unsigned advanceBefore(unsigned budget,R&&read,W&&write,Resolver&&resolve,Bindings&&bindings,int previous=-1,int previousRead=-1){NativeCompletionNoop observer;return advanceBefore(budget,read,write,resolve,bindings,observer,previous,previousRead);}
template<class R,class W,class Resolver,class Bindings,class Observer>unsigned advanceBefore(unsigned budget,R&&read,W&&write,Resolver&&resolve,Bindings&&bindings,Observer&&observer,int previous=-1,int previousRead=-1){const auto cost=nextClocks(previous,previousRead);if(!cost||cost>budget)return 0;return advancePlannedPhase(cost,read,write,resolve,bindings,observer,previous,previousRead);}
private:
friend class NativeControllerExecutor;
friend struct ArpArithmeticRouteTestAccess;
// Valid only within the caller's synchronous admission of this unchanged phase.
template<class R,class W,class Resolver,class Bindings,class Observer>unsigned advancePlannedPhase(unsigned cost,R&&read,W&&write,Resolver&&resolve,Bindings&&bindings,Observer&&observer,int previous,int previousRead){beginPhase();
if(!active_){const bool dispatch=coordinator_.policyStage()==ServiceCycleTask::Stage::Dispatch;coordinator_.advanceBefore(cost);effect_={coordinator_.effect().read,coordinator_.effect().write};if(dispatch){child_=coordinator_.child();index_=coordinator_.index();startChild();notify(observer,Notice::Dispatch);}else notify(observer,Notice::Complete);return cost;}
if(external_!=External::None){bool done=false;std::visit([&](auto&s){using T=std::decay_t<decltype(s)>;if constexpr(!std::is_same_v<T,std::monostate>){if constexpr(std::is_same_v<T,TransportEngine>)s.advanceBefore(cost,read,write,resolve,observer,previous,previousRead);else s.advanceBefore(cost,read,write,previous);effect_={s.effect().read,s.effect().write};done=s.complete();}},externalEngine_);
if(done){auto&f=externals_[externalSlot_];std::visit([&](const auto&s){using T=std::decay_t<decltype(s)>;if constexpr(!std::is_same_v<T,std::monostate>)f.after=bindings.after(s,f.before);},externalEngine_);f.complete=true;notify(observer,Notice::ExternalComplete);auto&ui=std::get<StateCentralActionsService>(engine_);typename StateCentralActionsService::Result r;r.presentation=f.after.presentation;r.event=r.presentation;r.outcome.value=externalSlot_+1;if(!ui.resume(r))throw std::logic_error("Main UI external child return ownership");external_=External::None;externalEngine_.template emplace<std::monostate>();if(ui.complete())finishChild(bindings,observer);}return cost;}
bool done=false;std::visit([&](auto&s){using T=std::decay_t<decltype(s)>;
if constexpr(std::is_same_v<T,std::monostate>)throw std::logic_error("Missing main native child");
else if constexpr(std::is_same_v<T,StatePanelService>){effect_.write=s.clearsWriteHistory();s.advanceBefore(cost,read,write,previous);effect_.read=s.complete();done=s.complete();}
else{if constexpr(std::is_same_v<T,TimerEngine>||std::is_same_v<T,StateTickService>||std::is_same_v<T,StateCentralActionsService>)s.advanceBefore(cost,read,write,resolve,previous);
else if constexpr(std::is_same_v<T,MidiEngine>)s.advanceBefore(cost,read,write,resolve,observer,previous);
else if constexpr(std::is_same_v<T,ArpEngine>)s.advancePlannedPhase(cost,read,write,resolve,observer,previous,previousRead);
else s.advanceBefore(cost,read,write,previous);effect_={s.effect().read,s.effect().write};done=s.complete();}},engine_);finishActivePhase(done,bindings,observer);return cost;}
private:
// These pointers live only from preparation to consumption of ONE phase.
// Neither a query nor tick/IRQ can retain or publish the path.
struct ArithmeticRoute {
 ArpEngine*engine=nullptr;typename ArpEngine::ArithmeticRoute child{};
 explicit operator bool()const{return engine&&bool(child);}
 unsigned cost()const{return engine->arithmeticClocks(child);}
};
ArithmeticRoute prepareArithmeticRoute(){
 if(!active_||external_!=External::None||child_!=Child::Arpeggiator||index_!=unsigned(Child::Arpeggiator)||complete()
  ||!coordinator_.awaitingChild()||coordinator_.index()!=index_||coordinator_.child()!=child_)return{};
 auto*engine=std::get_if<ArpEngine>(&engine_);if(!engine)return{};
 const auto child=engine->prepareArithmeticRoute();return child?ArithmeticRoute{engine,child}:ArithmeticRoute{};
}
void beginPhase(){effect_={};}
template<class Bindings,class Observer>void finishActivePhase(bool done,Bindings&bindings,Observer&observer){
 notify(observer,Notice::ChildPhase);
 if(const auto*ui=std::get_if<StateCentralActionsService>(&engine_)){if(ui->request()!=StateCentralActionsService::Request::None){startExternal(*ui,bindings);notify(observer,Notice::ExternalBegin);}}
 if(done)finishChild(bindings,observer);
}
template<class R,class W,class Resolver,class Bindings,class Observer>unsigned advanceArithmeticPhase(const ArithmeticRoute&r,unsigned cost,R&&read,W&&write,Resolver&&resolve,Bindings&&bindings,Observer&&observer,int previous,int previousRead){
 beginPhase();r.engine->advanceArithmeticPhase(r.child,cost,read,write,resolve,observer,previous,previousRead);
 effect_={r.engine->effect().read,r.engine->effect().write};finishActivePhase(r.engine->complete(),bindings,observer);return cost;
}
// A completed external UI tail returns on its already-paid phase.
// Share the ordinary child completion ordering without another guest phase.
template<class Bindings,class Observer>void finishChild(Bindings&bindings,Observer&observer){Result after;std::visit([&](const auto&s){using T=std::decay_t<decltype(s)>;if constexpr(!std::is_same_v<T,std::monostate>){if constexpr(std::is_same_v<T,StateCentralActionsService>)after=bindings.after(s,coordinator_.result(),externals_);else after=bindings.after(s,coordinator_.result());}},engine_);typename Coordinator::ChildResult r;r.outcome=index_;r.presentationKnown=r.activationKnown=true;r.presentation=after.presentation;r.activation=after.activation;r.expressionSource=after.expressionSource;r.expressionSourceMask=~0u;
notify(observer,Notice::ChildComplete);if(!coordinator_.resume(r))throw std::logic_error("Main child return ownership");active_=false;engine_.template emplace<std::monostate>();}
template<class T,class S>static auto query(const S&s,int)->decltype(s.template task<T>()){return s.template task<T>();}
template<class T,class S>static const T*query(const S&,long){return nullptr;}
template<class Observer>void notify(Observer&observer,Notice n)const{static_assert(std::is_void_v<decltype(observer(n,*this))>,"Native observer must not return execution state");observer(n,*this);}
template<class T>static unsigned priorityOf(const T&s){if constexpr(std::is_same_v<T,std::monostate>)return 0;else if constexpr(std::is_same_v<T,TimerEngine>||std::is_same_v<T,MidiEngine>||std::is_same_v<T,ArpEngine>||std::is_same_v<T,TransportEngine>||std::is_same_v<T,StateCentralActionsService>)return s.priority();else return s.critical()?7:0;}
template<class T>static unsigned clocks(const T&s,int previous,int previousRead){if constexpr(std::is_same_v<T,std::monostate>)return 0;else if constexpr(std::is_same_v<T,ArpEngine>||std::is_same_v<T,TransportEngine>)return s.nextClocks(previous,previousRead);else return s.nextClocks(previous);}
void startChild(){const auto&in=coordinator_.result();switch(child_){case Child::Timer0:case Child::Timer1:case Child::Timer2:case Child::Timer3:case Child::Timer4:engine_.template emplace<TimerEngine>(TimerEngine::Kind::Service,index_);break;case Child::SlowControls:engine_.template emplace<StateTickService>();break;case Child::PanelInput:case Child::Display:case Child::GlobalState:{typename StateCentralActionsService::Input i;i.presentation=in.presentation;const auto k=child_==Child::PanelInput?StateCentralActionsService::Kind::Buttons:child_==Child::Display?StateCentralActionsService::Kind::PanelEvent:StateCentralActionsService::Kind::Audition;engine_.template emplace<StateCentralActionsService>(k,i);break;}case Child::ModeStatus:case Child::ChannelMirror:engine_.template emplace<StateChannelService>(child_==Child::ModeStatus?StateChannelService::Kind::ModeStatus:StateChannelService::Kind::GlobalMirror);break;case Child::KeyEvents:engine_.template emplace<StatePanelService>();break;case Child::MidiOutput:{typename MidiEngine::Input i;i.activation=in.activation;engine_.template emplace<MidiEngine>(i);break;}case Child::VoiceFeedback:engine_.template emplace<VoiceFeedbackService>();break;case Child::Arpeggiator:{typename ArpEngine::Input i;i.expressionSource=in.expressionSource;i.activation=in.activation;engine_.template emplace<ArpEngine>(ArpEngine::Kind::Schedule,i);break;}case Child::HardwareInput:engine_.template emplace<PanelMasksService>();break;case Child::PartFeedback:engine_.template emplace<PartFeedbackService>();break;}active_=true;}
template<class Bindings>void startExternal(const StateCentralActionsService&ui,Bindings&bindings){if(externalCount_==externals_.size())throw std::logic_error("Main external native child capacity");externalSlot_=externalCount_++;auto&f=externals_[externalSlot_];f.before=bindings.beforeExternal(ui,coordinator_.result(),externals_);using Q=StateCentralActionsService::Request;if(ui.request()==Q::Transport){f.kind=external_=External::Transport;typename TransportEngine::Input in;in.presentation=ui.childInput().presentation;in.activation=f.before.activation;in.expressionSource=f.before.expressionSource;externalEngine_.template emplace<TransportEngine>(in);}else if(ui.request()==Q::Diagnostic){f.kind=external_=External::Diagnostic;externalEngine_.template emplace<DiagnosticTransferService>(DiagnosticTransferService::Kind::Dump);}else throw std::logic_error("Unclosed main UI native request");}
Coordinator coordinator_;Engines engine_;ExternalEngines externalEngine_;Effect effect_{};Child child_=Child::Timer0;unsigned index_=0,externalSlot_=0,externalCount_=0;bool active_=false;External external_=External::None;std::array<ExternalFacts,3>externals_{};
};}
