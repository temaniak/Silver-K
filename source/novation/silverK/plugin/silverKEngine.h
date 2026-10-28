#pragma once
#include "kstationLib/kstationDevice.h"
#include "synthLib/resamplerInOut.h"
#include "silverKCutoffMidi.h"
namespace silverK {
struct KEngine {
 kstation::Device device;
 synthLib::ResamplerInOut resampler{1,2};
 synthLib::ResamplerInOut::TProcessFunc render;
 synthLib::ResamplerInOut::TMidiVec emptyMidi,outputMidi;
 std::array<float,256> silence{};
 double rate;
 KCutoffMidi cutoffMidi;
 KEngine(const std::vector<uint8_t>&,double,int,const std::vector<uint8_t>&,unsigned bufferFloor=0,baseLib::performance::Diagnostics* performance=nullptr);
 void process(float*,float*,int,bool,const float* input=nullptr);
 bool midi(const uint8_t* p,size_t n,uint32_t offset=0){
  if(cutoffMidi.duplicate(p,n))return true;
  const bool ok=device.enqueueMidiBytes(p,n,uint32_t(offset*device.getSamplerate()/rate));
  if(ok)cutoffMidi.accepted(p,n);else cutoffMidi.reset();
  return ok;
 }
 unsigned latency()const;
};
}
