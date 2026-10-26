#pragma once
#include "ksMidiFileEventCoordinator.h"
#include "ksNativeMidiDecodeService.h"
#include "ksNativeMidiDomainBindings.h"
#include <optional>
#include <stdexcept>
#include <type_traits>
namespace ks {
// Complete file-event receiver. Each byte is delivered to the actual native
// decoder before its fixed slot is reused. Length changes work, not capacity.
class MidiFileEventService final {
public:
using Coordinator=MidiFileEventCoordinator;using Kind=Coordinator::Kind;using Input=Coordinator::Input;using Result=Coordinator::Result;using Effect=Coordinator::Effect;
enum class Component{Event,Decode};enum class Notice{DecodeBegin,DecodeComplete,Finished};
struct Noop{template<class...T>void operator()(T&&...)const noexcept{}};
explicit MidiFileEventService(Kind k):MidiFileEventService(k,Input{}){}MidiFileEventService(Kind k,Input i):coordinator_(k,i){}
Kind kind()const{return coordinator_.kind();}const Input&input()const{return coordinator_.input();}const Result&result()const{return coordinator_.result();}const Effect&effect()const{return effect_;}const Coordinator&coordinator()const{return coordinator_;}const NativeMidiDecodeService*decoderOutcome()const{return decoder_?&*decoder_:nullptr;}Component component()const{return component_;}uint32_t components()const{return components_;}bool complete()const{return coordinator_.complete();}unsigned priority()const{return component_==Component::Decode?decoder_->priority():0;}bool critical()const{return priority()==7;}
template<class T>const T*task()const{if(complete())return nullptr;if constexpr(std::is_same_v<T,MidiFileEventService>)return this;else if constexpr(std::is_same_v<T,Coordinator>)return component_==Component::Event?&coordinator_:nullptr;else if constexpr(std::is_same_v<T,NativeMidiDecodeService>)return component_==Component::Decode?&*decoder_:nullptr;else return component_==Component::Event?coordinator_.template task<T>():decoder_->template task<T>();}
unsigned nextClocks(int previous=-1,int previousRead=-1)const{if(complete())return 0;return component_==Component::Event?coordinator_.nextClocks(previous,previousRead):decoder_->nextClocks(previous);}
template<class R,class W,class Resolver>unsigned advanceBefore(unsigned budget,R&&read,W&&write,Resolver&&resolve,int previous=-1,int previousRead=-1){Noop observer;return advanceBefore(budget,read,write,resolve,observer,previous,previousRead);}
template<class R,class W,class Resolver,class Observer>unsigned advanceBefore(unsigned budget,R&&read,W&&write,Resolver&&resolve,Observer&&observer,int previous=-1,int previousRead=-1){const auto cost=nextClocks(previous,previousRead);if(!cost||cost>budget)return 0;effect_={};
if(component_==Component::Decode){decoder_->advanceBefore(cost,read,write,resolve,previous);effect_={decoder_->effect().read,decoder_->effect().write};if(decoder_->complete()){const auto&r=decoder_->result();const auto transfer=midiDecodeExpressionTransfer(*decoder_);Coordinator::DecodeResult result;result.outcome=0;result.activationKnown=r.activationKnown;result.activation=r.activation;result.expressionSource=transfer.value;result.expressionSourceMask=transfer.mask;result.dataBindingKnown=r.dataBindingKnown;result.dataBinding=r.dataBinding;notify(observer,Notice::DecodeComplete);if(!coordinator_.resumeDecode(result))throw std::logic_error("File decoder return ownership");component_=Component::Event;}return cost;}
coordinator_.advanceObservedBefore(cost,read,write,resolve,observer,previous,previousRead);effect_=coordinator_.effect();if(coordinator_.request()==Coordinator::Request::Decode){const auto&i=coordinator_.childInput();decoder_.emplace(NativeMidiDecodeService::Input{i.byte,i.activation,i.dataBinding});component_=Component::Decode;components_|=2;notify(observer,Notice::DecodeBegin);}if(coordinator_.complete())notify(observer,Notice::Finished);return cost;}
private:
template<class O>void notify(O&observer,Notice notice)const{static_assert(std::is_void_v<decltype(observer(notice,*this))>,"File observer must return void");observer(notice,*this);}
Coordinator coordinator_;std::optional<NativeMidiDecodeService>decoder_;Component component_=Component::Event;Effect effect_{};uint32_t components_=1;
};}
