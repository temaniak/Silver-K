#pragma once
#include "ksMidiFileEventCoordinator.h"
namespace ks {
class MidiFileEventBindings final {
public:
 MidiFileEventCoordinator::Kind fileMeta(uint8_t index,uint8_t observedType,uint32_t observedHandler)const{
  constexpr std::array<uint8_t,10>types{{47,81,1,2,3,4,5,6,7,127}};
  if(index>=types.size()||types[index]!=observedType)throw std::logic_error("Unsupported evolving MIDI-file meta descriptor");
  using K=MidiFileEventCoordinator::Kind;const auto expected=index==0?0xb38cu:index==1?0xb37cu:0xb376u;
  if(observedHandler!=expected)throw std::logic_error("Unsupported evolving MIDI-file meta binding");
  return index==0?K::End:index==1?K::Tempo:K::Skip;
 }
};
}
