#pragma once
#include <utility>
#include "ksNativeMonoArpOwnedGraph.h"
#include "ksNativeMonoArpCompatibility.h"
#include "ksNativeInternalMidiAbi.h"
#include "ksMidiFileEventCompatibility.h"
namespace ks {
using NativeMonoArpOwnedService=NativeMonoArpOwnedGraph<NativeMonoArpCoordinator>;
class NativeMonoArpOwnedCompatibility final {
public:
 using Image=NativeMonoArpCompatibility::Image;template<class A>explicit NativeMonoArpOwnedCompatibility(const A&a):mono_(a){}const Image&image()const{return mono_.image();}
 template<class...A>void operator()(NativeMonoArpCoordinator::Notice n,A&&...a){mono_(n,std::forward<A>(a)...);}
 void operator()(NativeMonoArpOwnedService::Notice n,const NativeMonoArpOwnedService&s){using N=NativeMonoArpOwnedService::Notice;if(n==N::InternalComplete){auto a=mono_.image();projectNativeInternalMidi(a,*s.internalOutcome());mono_.replaceImage(a);}else if(n==N::FileBegin)file_.emplace(mono_.image());else if(n==N::FileComplete){if(!file_)throw std::logic_error("Missing external mono file image");mono_.replaceImage(file_->image());}}
 template<class...A>void operator()(MidiFileEventService::Notice n,A&&...a){if(!file_)throw std::logic_error("Missing external mono file image");(*file_)(n,std::forward<A>(a)...);}template<class...A>void operator()(MidiFileEventCoordinator::Notice n,A&&...a){if(!file_)throw std::logic_error("Missing external mono file image");(*file_)(n,std::forward<A>(a)...);}
private:NativeMonoArpCompatibility mono_;std::optional<MidiFileEventCompatibility>file_;
};
}
