#pragma once
#include "ksNativeArpOwnedService.h"
#include "ksNativeArpCompatibility.h"
#include "ksNativeInternalMidiAbi.h"
#include "ksMidiFileEventCompatibility.h"
namespace ks {
// External observer only. No owned native graph may contain this sink or read
// its return image. Noop and observing executions must have identical effects.
class NativeArpOwnedCompatibility final {
public:
 using Image=NativeArpCompatibility::Image;
 template<class A>NativeArpOwnedCompatibility(NativeArpOwnedService::Kind k,const A&a):arp_(k,a){}
 const Image&image()const{return arp_.image();}
 template<class...A>void operator()(NativeArpService::Notice n,A&&...a){arp_(n,std::forward<A>(a)...);}template<class...A>void operator()(NativeArpMainService::Notice n,A&&...a){arp_(n,std::forward<A>(a)...);}template<class...A>void operator()(NativeMonoArpCoordinator::Notice n,A&&...a){arp_(n,std::forward<A>(a)...);}
 void operator()(NativeArpOwnedService::Notice n,const NativeArpOwnedService&s){using N=NativeArpOwnedService::Notice;if(n==N::InternalComplete){auto a=arp_.image();projectNativeInternalMidi(a,*s.internalOutcome());arp_.replaceImage(a);}else if(n==N::FileBegin)file_.emplace(arp_.image());else if(n==N::FileComplete){if(!file_)throw std::logic_error("Missing external file image");arp_.replaceImage(file_->image());}}
 template<class...A>void operator()(MidiFileEventService::Notice n,A&&...a){if(!file_)throw std::logic_error("Missing external file image");(*file_)(n,std::forward<A>(a)...);}template<class...A>void operator()(MidiFileEventCoordinator::Notice n,A&&...a){if(!file_)throw std::logic_error("Missing external file image");(*file_)(n,std::forward<A>(a)...);}
private:
 NativeArpCompatibility arp_;std::optional<MidiFileEventCompatibility>file_;
};
}
