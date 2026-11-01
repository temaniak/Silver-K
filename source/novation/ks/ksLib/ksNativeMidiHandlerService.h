#pragma once
#include "ksNativeMidiHandler.h"
#include "ksNativePartNoteService.h"
#include "ksNativeMidiControlService.h"
#include "ksNativeMidiAuxService.h"
#include "ksNativeInternalNoteService.h"
#include "ksNativeMidiHandlerDomainBindings.h"
#include <variant>
namespace ks {
class NativeMidiHandlerService final {
public:
 struct Input{NativeMidiHandler handler=NativeMidiHandler::Noop;uint32_t payload=0,expressionSource=0,field=0;uint16_t presentation=0,noteVelocity=0;uint8_t channel=0;};struct Effect{bool read=false,write=false;};
 struct Result{bool activationKnown=false;uint8_t activation=0;};
 using Frames=std::variant<std::monostate,NativePartNoteService,NativeMidiControlService,NativeMidiAuxService,NativeInternalNoteService>;
 explicit NativeMidiHandlerService(Input i):input_(i){using H=NativeMidiHandler;using A=NativeMidiAuxService;using C=NativeMidiControlService;switch(i.handler){
 case H::PartOff:case H::PartOn:frames_.emplace<NativePartNoteService>(i.handler==H::PartOn?NativePartNoteService::Kind::On:NativePartNoteService::Kind::Off,NativePartNoteService::Input{i.payload,i.noteVelocity,i.presentation});break;
 case H::InternalOff:case H::InternalOn:frames_.emplace<NativeInternalNoteService>(i.handler==H::InternalOn?NativeInternalNoteService::Kind::On:NativeInternalNoteService::Kind::Off,NativeInternalNoteService::Input{i.payload});break;
 case H::Control:case H::Pressure:case H::Pitch:frames_.emplace<C>(i.handler==H::Control?C::Kind::Control:i.handler==H::Pressure?C::Kind::Pressure:C::Kind::Pitch,C::Input{i.payload,i.expressionSource,i.field,i.channel});break;
 case H::Program:case H::ConditionalProgram:case H::GlobalControl:case H::BeginSysEx:case H::EndSysEx:case H::ManualPeriodic:case H::Noop:{const auto k=i.handler==H::Program?A::Kind::Program:i.handler==H::ConditionalProgram?A::Kind::ConditionalProgram:i.handler==H::GlobalControl?A::Kind::GlobalControl:i.handler==H::BeginSysEx?A::Kind::Start:i.handler==H::EndSysEx?A::Kind::Continue:i.handler==H::ManualPeriodic?A::Kind::Stop:A::Kind::Noop;frames_.emplace<A>(k,A::Input{i.payload,i.field,i.presentation,0,0,i.channel});break;}}
 }
 bool complete()const{return std::visit([](const auto&s)->bool{using T=std::decay_t<decltype(s)>;if constexpr(std::is_same_v<T,std::monostate>)return false;else return s.complete();},frames_);}unsigned priority()const{return std::visit([](const auto&s)->unsigned{using T=std::decay_t<decltype(s)>;if constexpr(std::is_same_v<T,NativePartNoteService>||std::is_same_v<T,NativeMidiAuxService>)return s.priority();else if constexpr(std::is_same_v<T,std::monostate>)return 0;else return s.critical()?7:0;},frames_);}bool critical()const{return priority()==7;}const Effect&effect()const{return effect_;}const Input&input()const{return input_;}const Frames&frames()const{return frames_;}
 Result result()const{if(!complete())throw std::logic_error("Incomplete MIDI handler result");return std::visit([](const auto&s)->Result{using T=std::decay_t<decltype(s)>;PanelActivationTransfer r;if constexpr(std::is_same_v<T,NativeMidiAuxService>)r=nativeMidiDomainBindings::auxiliaryActivation(s);else if constexpr(std::is_same_v<T,NativeMidiControlService>)r=nativeMidiDomainBindings::controlActivation(s);else if constexpr(std::is_same_v<T,NativeInternalNoteService>)r=nativeMidiDomainBindings::internalNoteActivation(s);return{r.known,r.value};},frames_);}
 template<class T>const T*task()const{if(complete())return nullptr;return std::visit([](const auto&s)->const T*{using S=std::decay_t<decltype(s)>;if constexpr(std::is_same_v<S,std::monostate>)return nullptr;else if constexpr(std::is_same_v<T,S>)return &s;else return s.template task<T>();},frames_);}
 unsigned nextClocks(int previous=-1)const{return std::visit([&](const auto&s)->unsigned{using T=std::decay_t<decltype(s)>;if constexpr(std::is_same_v<T,std::monostate>)return 0;else return s.nextClocks(previous);},frames_);}
 template<class R,class W,class Resolver>unsigned advanceBefore(unsigned budget,R&&read,W&&write,Resolver&&resolve,int previous=-1){const auto cost=nextClocks(previous);if(!cost||cost>budget)return 0;return std::visit([&](auto&s)->unsigned{using T=std::decay_t<decltype(s)>;if constexpr(std::is_same_v<T,std::monostate>)return 0;else{const unsigned used=[&]{if constexpr(std::is_same_v<T,NativeInternalNoteService>)return s.advanceBefore(cost,read,write,previous);else return s.advanceBefore(cost,read,write,resolve,previous);}();effect_={s.effect().read,s.effect().write};return used;}},frames_);}
private:Input input_{};Frames frames_{};Effect effect_{};
};
}
