#pragma once
#include "ksStateCentralActionsPrefixAbi.h"
#include "ksScheduledTimerAbi.h"
#include "ksStateTickAbi.h"
#include "ksStateChannelAbi.h"
#include "ksStatePanelAbi.h"
#include "ksVoiceFeedbackAbi.h"
#include "ksPanelMasksAbi.h"
#include "ksPartFeedbackAbi.h"
#include "ksDiagnosticTransferAbi.h"
#include "ksNativeMidiPumpAbi.h"
#include <array>
#include <optional>
#include <type_traits>
namespace ks {
// EXTERNAL Stage3 boundary. The native main graph neither owns this observer
// nor reads this image. Only its outer owner applies the image on completion.
// Nested unbounded queues update these fixed external slots without retaining
// per-event history or changing any native resource operation/decision.
template<class Main,class ArpEngine,class ArpCompatibility,class TransportEngine,class TransportCompatibility>
class ServiceCycleCompatibility final {
public:
 struct Image{std::array<uint32_t,7>values{};uint8_t flags=0;};
 template<class A>explicit ServiceCycleCompatibility(const A&a){copy(image_,a);}
 const Image&image()const{return image_;}
 void operator()(typename Main::Notice notice,const Main&s){
  using N=typename Main::Notice;using C=typename Main::Child;
  if(notice==N::Dispatch){child_=s.child();if(child_==C::MidiOutput)pump_.emplace(image_);else if(child_==C::Arpeggiator)arp_.emplace(ArpEngine::Kind::Schedule,image_);else if(child_==C::PanelInput||child_==C::Display||child_==C::GlobalState)uiBefore_=image_;return;}
  if(notice==N::ExternalBegin){const auto*ui=s.template current<StateCentralActionsService>();if(!ui)throw std::logic_error("Missing main external UI prefix");Image prefix=uiBefore_;projectStateCentralActionsPrefix(prefix,*ui,[&](auto handle,auto&out){externalOutcome(handle.value,out);});image_=prefix;external_=s.external();if(external_==Main::External::Transport)transport_.emplace(image_);return;}
  if(notice==N::ExternalComplete){if(external_==Main::External::Transport){if(!transport_)throw std::logic_error("Missing main transport accounting");copy(image_,transport_->image());}else if(external_==Main::External::Diagnostic){const auto*d=s.template current<DiagnosticTransferService>();if(!d)throw std::logic_error("Missing main diagnostic outcome");projectDiagnosticTransfer(image_,*d);}else throw std::logic_error("Missing main external kind");unsigned index=0;for(;index<externalImages_.size();++index)if(!externalImages_[index])break;if(index==externalImages_.size())throw std::logic_error("Main external accounting capacity");externalImages_[index]=image_;external_=Main::External::None;return;}
  if(notice!=N::ChildComplete)return;
  std::visit([&](const auto&c){using T=std::decay_t<decltype(c)>;
   if constexpr(std::is_same_v<T,std::monostate>)throw std::logic_error("Missing main completed child accounting");
   else if constexpr(std::is_same_v<T,ScheduledTimerService>)projectScheduledTimer(image_,c);
   else if constexpr(std::is_same_v<T,StateTickService>)projectStateTick(image_,c);
   else if constexpr(std::is_same_v<T,StateChannelService>)projectStateChannel(image_,c);
   else if constexpr(std::is_same_v<T,StatePanelService>)projectStatePanel(image_,c);
   else if constexpr(std::is_same_v<T,StateCentralActionsService>){image_=uiBefore_;projectStateCentralActions(image_,c,[&](auto handle,auto&out){externalOutcome(handle.value,out);});}
   else if constexpr(std::is_same_v<T,VoiceFeedbackService>)projectVoiceFeedback(image_,c);
   else if constexpr(std::is_same_v<T,PanelMasksService>)projectPanelMasks(image_,c);
   else if constexpr(std::is_same_v<T,PartFeedbackService>)projectPartFeedback(image_,c);
   else if constexpr(std::is_same_v<T,ArpEngine>){if(!arp_)throw std::logic_error("Missing main arp accounting");copy(image_,arp_->image());}
   else if constexpr(std::is_same_v<T,NativeMidiPumpService>){if(!pump_)throw std::logic_error("Missing main MIDI accounting");}
   else static_assert(!sizeof(T),"Unsupported main child accounting");
  },s.engines());
 }
 template<class...A>void operator()(A&&...args){if(external_==Main::External::Transport){if constexpr(std::is_invocable_v<TransportCompatibility&,A...>){if(!transport_)throw std::logic_error("Missing active transport observer");(*transport_)(std::forward<A>(args)...);return;}}else if(child_==Main::Child::MidiOutput){if constexpr(std::is_invocable_v<NativeMidiPumpAbiObserver<Image>&,A...>){if(!pump_)throw std::logic_error("Missing active MIDI observer");(*pump_)(std::forward<A>(args)...);return;}}else if(child_==Main::Child::Arpeggiator){if constexpr(std::is_invocable_v<ArpCompatibility&,A...>){if(!arp_)throw std::logic_error("Missing active arp observer");(*arp_)(std::forward<A>(args)...);return;}}throw std::logic_error("Unowned main nested completion notice");}
private:
 template<class A,class B>static void copy(A&a,const B&b){a.values=b.values;a.flags=b.flags;}
 template<class A>void externalOutcome(unsigned handle,A&a)const{if(!handle||handle>externalImages_.size()||!externalImages_[handle-1])throw std::logic_error("Missing completed main external image");copy(a,*externalImages_[handle-1]);}
 Image image_{},uiBefore_{};typename Main::Child child_=Main::Child::Timer0;typename Main::External external_=Main::External::None;
 std::optional<NativeMidiPumpAbiObserver<Image>>pump_;std::optional<ArpCompatibility>arp_;std::optional<TransportCompatibility>transport_;
 std::array<std::optional<Image>,3>externalImages_{};
};
}
