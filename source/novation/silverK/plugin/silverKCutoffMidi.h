#pragma once
#include <cstddef>
#include <cstdint>

namespace silverK {
// Host-side removal of consecutive redundant absolute cutoff writes. Each one
// otherwise runs the original controller's expensive DSP parameter service,
// which can starve its foreground arpeggiator. Never coalesce changed values,
// NRPN, performance CCs, or writes separated by another non-clock message.
class KCutoffMidi {
public:
 bool duplicate(const uint8_t* bytes,size_t size) const noexcept {
  return cutoff(bytes,size) && valid && status==bytes[0] && value==bytes[2];
 }
 void accepted(const uint8_t* bytes,size_t size) noexcept {
  if(size==1 && bytes[0]==0xf8)return;
  valid=cutoff(bytes,size);
  if(valid){status=bytes[0];value=bytes[2];}
 }
 void reset() noexcept {valid=false;}
private:
 static bool cutoff(const uint8_t* bytes,size_t size) noexcept {
  return size==3 && (bytes[0]&0xf0)==0xb0 && bytes[1]==105 && bytes[2]<128;
 }
 bool valid=false;
 uint8_t status=0,value=0;
};
}
