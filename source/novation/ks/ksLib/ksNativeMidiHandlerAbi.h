#pragma once
#include "ksNativeMidiHandlerService.h"
#include "ksNativePartNoteAbi.h"
#include "ksNativeMidiControlAbi.h"
#include "ksNativeMidiAuxAbi.h"
#include "ksNativeInternalNoteAbi.h"
namespace ks {
template<class A>void projectNativeMidiHandler(A&a,const NativeMidiHandlerService&s){if(!s.complete())throw std::logic_error("Incomplete MIDI handler projection");std::visit([&](const auto&t){using T=std::decay_t<decltype(t)>;if constexpr(std::is_same_v<T,NativePartNoteService>)projectNativePartNote(a,t);else if constexpr(std::is_same_v<T,NativeMidiControlService>)projectNativeMidiControl(a,t);else if constexpr(std::is_same_v<T,NativeMidiAuxService>)projectNativeMidiAux(a,t);else if constexpr(std::is_same_v<T,NativeInternalNoteService>)projectNativeInternalNote(a,t);else throw std::logic_error("Unbound MIDI handler projection");},s.frames());}
}
