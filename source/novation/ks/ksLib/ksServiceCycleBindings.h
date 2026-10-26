#pragma once
#include "ksServiceCyclePanelBindings.h"
#include "ksStateCentralActionsBindings.h"
#include "ksScheduledTimerBindings.h"
#include "ksNativeMidiPumpService.h"
namespace ks {
// Domain composition is independent of external ABI accounting. Only operands
// actually consumed by later services are transferred. Arp/file MIDI receivers
// have no incoming presentation consumer; their final R6 is external metadata.
template<class Main,class ArpEngine,class TransportEngine>
class ServiceCycleBindings final {
public:
 using Context=typename Main::Result;using Facts=typename Main::ExternalFacts;
 Context after(const ScheduledTimerService&s,Context c)const{return serviceCycleData::uiTransfer(scheduledTimerDomainTransfer(s),c);}
 Context after(const StateTickService&s,Context c)const{return serviceCycleData::tick(s,c);}
 Context after(const StateChannelService&s,Context c)const{return serviceCycleData::channel(s,c);}
 Context after(const StatePanelService&s,Context c)const{return serviceCycleData::panel(s,c);}
 Context after(const VoiceFeedbackService&s,Context c)const{return serviceCycleData::voiceFeedback(s,c);}
 Context after(const PartFeedbackService&s,Context c)const{return serviceCycleData::partFeedback(s,c);}
 Context after(const PanelMasksService&s,Context c)const{return serviceCycleData::masks(s,c);}
 Context after(const DiagnosticTransferService&s,Context c)const{return serviceCycleData::diagnostic(s,c);}
 Context after(const NativeMidiPumpService&s,Context c)const{if(!s.complete())throw std::logic_error("Incomplete main MIDI domain");const auto&f=s.summary();if(f.activationKnown)c.activation=f.activation;c.expressionSource=(c.expressionSource&~f.expressionSourceMask)|(f.expressionSource&f.expressionSourceMask);c.presentation=midiPumpPresentationAfter(s,c.presentation);return c;}
 Context after(const ArpEngine&s,Context c)const{if(!s.complete())throw std::logic_error("Incomplete main arp domain");c.expressionSource=s.expressionSourceAfter(c.expressionSource);c.activation=s.activationAfter(c.activation);return c;}
 Context after(const TransportEngine&s,Context c)const{if(!s.complete())throw std::logic_error("Incomplete main file-player domain");return s.domainAfter(c);}
 template<std::size_t N>Context after(const StateCentralActionsService&s,Context c,const std::array<Facts,N>&facts)const{return serviceCycleData::uiTransfer(stateCentralActionsDomainTransfer(s,[&](auto handle){return external(facts,handle.value);}),c);}
 template<std::size_t N>Context beforeExternal(const StateCentralActionsService&s,Context c,const std::array<Facts,N>&facts)const{return serviceCycleData::uiTransfer(stateCentralActionsPrefixDomainTransfer(s,[&](auto handle){return external(facts,handle.value);}),c);}
private:
 template<std::size_t N>static StateUiDomainTransfer external(const std::array<Facts,N>&facts,unsigned handle){if(!handle||handle>N||!facts[handle-1].complete)throw std::logic_error("Missing main external domain outcome");const auto&f=facts[handle-1];StateUiDomainTransfer out;out.expression=fieldExpressionBindings::full(f.after.expressionSource);out.activation={true,f.after.activation};out.presentation=stateUiDomainBindings::word(f.after.presentation);return out;}
};
}
