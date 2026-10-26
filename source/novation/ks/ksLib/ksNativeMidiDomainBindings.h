#pragma once
#include "ksNativeMidiDecodeService.h"
#include "ksNativeQueuedMidiDecodeService.h"
#include "ksNativeInternalMidiService.h"
#include "ksNativeMidiHandlerDomainBindings.h"
namespace ks {namespace nativeMidiDomainBindings {
inline ExpressionSourceTransfer handler(const NativeMidiHandlerService&s){return std::visit([](const auto&child)->ExpressionSourceTransfer{using T=std::decay_t<decltype(child)>;if constexpr(std::is_same_v<T,NativeMidiAuxService>)return auxiliary(child);else if constexpr(std::is_same_v<T,NativeMidiControlService>)return control(child);else if constexpr(std::is_same_v<T,NativeInternalNoteService>)return internalNote(child);else throw std::logic_error("Unsupported direct MIDI domain child");},s.frames());}
inline ExpressionSourceTransfer route(const NativeMidiRouteService&s){const auto&f=s.summary();if(f.system)return handler(s.handlerOutcome(0));ExpressionSourceTransfer out=fieldExpressionBindings::full(s.routeTask().patch());if(f.count){const auto&d=f.deliveries[f.count-1];if(d.kind==NativeMidiRouteService::DeliveryKind::FallbackProgram||d.kind==NativeMidiRouteService::DeliveryKind::FallbackControl)out.then(handler(s.handlerOutcome(f.count-1)));}return out;}
}
inline ExpressionSourceTransfer midiDecodeExpressionTransfer(const NativeMidiDecodeService&s){if(!s.complete())throw std::logic_error("Incomplete decoder domain transfer");if(s.routeOutcome())return nativeMidiDomainBindings::route(*s.routeOutcome());if(s.immediateOutcome())return nativeMidiDomainBindings::handler(*s.immediateOutcome());return{};}
inline ExpressionSourceTransfer queuedMidiDecodeExpressionTransfer(const NativeQueuedMidiDecodeService&s){if(!s.complete())throw std::logic_error("Incomplete queued decoder domain transfer");if(s.routeOutcome())return nativeMidiDomainBindings::route(*s.routeOutcome());if(s.immediateOutcome())return nativeMidiDomainBindings::handler(*s.immediateOutcome());return{};}
inline ExpressionSourceTransfer internalMidiExpressionTransfer(const NativeInternalMidiService&s){if(!s.complete())throw std::logic_error("Incomplete internal decoder domain transfer");return s.handlerOutcome()?nativeMidiDomainBindings::handler(*s.handlerOutcome()):ExpressionSourceTransfer{};}
}
